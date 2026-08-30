#include "AeylaVisualDmx.h"
#include "AeylaTakeLibrarySession.h"
#include "capture/dmx_take_file_store.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {

const aeyla::capture::TakeFileIndexEntry& SelectMidiTake(
    const void* owner,
    std::string_view song_id,
    const aeyla::capture::TakeLibraryScanResult& scan) {
  const auto edited = aeyla::take_library_session::edit_state(owner, song_id);
  if(edited.has_value()) {
    const auto found = std::find_if(
        scan.entries.begin(), scan.entries.end(),
        [&](const auto& entry) {
          return entry.path == edited->path &&
                 entry.frame_count == edited->frame_count &&
                 entry.frames_per_second == edited->frames_per_second;
        });
    if(found != scan.entries.end())
      return *found;
    aeyla::take_library_session::clear_edit_state(owner, song_id);
  }
  return scan.entries.front();
}

const char* LearnTargetName(aeyla::runtime::ShowMidiLearnTarget target) {
  using Target = aeyla::runtime::ShowMidiLearnTarget;
  switch(target) {
    case Target::previous_song: return "CANCIÓN ANTERIOR";
    case Target::next_song: return "SIGUIENTE CANCIÓN";
    case Target::play_retrigger: return "PLAY / REINICIAR";
    case Target::pause_resume: return "PAUSA / REANUDAR";
    case Target::stop_reset: return "STOP / RESET";
    case Target::capture_start: return "REC START";
    case Target::capture_stop: return "REC STOP";
    case Target::launch_song_base: return "BASE DE 15 CANCIONES";
    case Target::none: return "NINGUNO";
  }
  return "NINGUNO";
}

}  // namespace

aeyla::runtime::ShowMidiMapping AeylaVisualDmx::ShowMidiMapping() const noexcept
{
  auto mapping = aeyla::runtime::unpack_show_midi_mapping(
      mShowMidiMappingPacked.load(std::memory_order_acquire));
  mapping.capture_start_note = mShowMidiCaptureStartNote.load(
      std::memory_order_acquire);
  mapping.capture_stop_note = mShowMidiCaptureStopNote.load(
      std::memory_order_acquire);
  return mapping;
}

void AeylaVisualDmx::SetShowMidiMessage(std::string message)
{
  const std::scoped_lock lock(mShowMidiMutex);
  mShowMidiMessage = std::move(message);
}

std::string AeylaVisualDmx::ShowMidiStatus() const
{
  const auto target = ShowMidiLearnTarget();
  if(target != aeyla::runtime::ShowMidiLearnTarget::none)
    return std::string("APRENDER MIDI · toca una nota para ") +
           LearnTargetName(target);
  const std::scoped_lock lock(mShowMidiMutex);
  return mShowMidiMessage;
}

void AeylaVisualDmx::SyncShowMidiMappingToState(
    const aeyla::runtime::ShowMidiMapping& mapping)
{
  mShowMidiMappingPacked.store(
      aeyla::runtime::pack_show_midi_mapping(mapping),
      std::memory_order_release);
  mShowMidiCaptureStartNote.store(mapping.capture_start_note,
                                  std::memory_order_release);
  mShowMidiCaptureStopNote.store(mapping.capture_stop_note,
                                 std::memory_order_release);
  const std::scoped_lock lock(mHostStateMutex);
  mHostStateCache.show_midi = mapping;
}

std::uint64_t AeylaVisualDmx::BeginShowTransportMutation() noexcept
{
  mShowTransportMutation.store(true, std::memory_order_release);
  for(;;) {
    const auto before = mAudioAdvanceSequence.load(std::memory_order_acquire);
    if((before & 1U) != 0U) {
      std::this_thread::yield();
      continue;
    }
    const auto completed = mProcessedTransportSamples.load(
        std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_acquire);
    const auto after = mAudioAdvanceSequence.load(std::memory_order_relaxed);
    if(before == after && (after & 1U) == 0U)
      return completed;
  }
}

void AeylaVisualDmx::EndShowTransportMutation() noexcept
{
  mShowTransportMutation.store(false, std::memory_order_release);
}

void AeylaVisualDmx::SynchronizeShowTransportCursor(
    std::uint64_t trigger_sample,
    std::uint64_t base_cursor) noexcept
{
  for(;;) {
    const auto before = mAudioAdvanceSequence.load(std::memory_order_acquire);
    if((before & 1U) != 0U) {
      std::this_thread::yield();
      continue;
    }
    const auto completed = mProcessedTransportSamples.load(
        std::memory_order_acquire);
    const auto elapsed = completed > trigger_sample
        ? completed - trigger_sample : 0U;
    const auto cursor = base_cursor >
            std::numeric_limits<std::uint64_t>::max() - elapsed
        ? std::numeric_limits<std::uint64_t>::max()
        : base_cursor + elapsed;
    mTakeScheduler.synchronize_host_cursor(cursor);
    std::atomic_thread_fence(std::memory_order_acquire);
    const auto after = mAudioAdvanceSequence.load(std::memory_order_relaxed);
    if(before == after && (after & 1U) == 0U)
      return;
  }
}

aeyla::product::AuthoringResult AeylaVisualDmx::ToggleShowMidiFromUI()
{
  auto mapping = ShowMidiMapping();
  if(aeyla::runtime::validate_show_midi_mapping(mapping) !=
     aeyla::runtime::ShowMidiMappingError::none)
    return {false, {}, "El mapa MIDI no es válido; vuelve a aprender sus notas"};
  if(!mapping.enabled && (TakeOutputArmed() || OutputArmed()))
    return {false, {},
            "Desarma cualquier autoridad antes de activar y precargar MIDI SHOW"};
  mapping.enabled = !mapping.enabled;
  mShowMidiLearnTarget.store(aeyla::runtime::ShowMidiLearnTarget::none,
                             std::memory_order_release);
  {
    const std::scoped_lock lock(mModelMutex);
    mModel.release_transients();
    SyncSnapshotToAtomicsLocked();
  }
  SyncShowMidiMappingToState(mapping);
  if(mapping.enabled)
  {
    mMidiPreloadSongRequest.store(static_cast<int>(ActiveSongIndex()),
                                  std::memory_order_release);
    mMidiPreflightCursor.store(0, std::memory_order_release);
  }
  else
    mMidiPreflightCursor.store(-1, std::memory_order_release);
  const std::string message = mapping.enabled
      ? "MIDI SHOW ACTIVO · canal " + std::to_string(mapping.channel) +
            " · N41 PANIC · N" + std::to_string(mapping.capture_start_note) +
            " REC START · N" + std::to_string(mapping.capture_stop_note) +
            " REC STOP · reloj por muestras del DAW"
      : "MIDI SHOW DESACTIVADO · las notas vuelven a los Cues normales";
  SetShowMidiMessage(message);
  return {true, {}, message};
}

aeyla::product::AuthoringResult AeylaVisualDmx::CycleShowMidiChannelFromUI(
    int direction)
{
  if(direction == 0)
    return {false, {}, "Selecciona una dirección de canal MIDI"};
  if(TakeOutputArmed() || OutputArmed())
    return {false, {},
            "Desarma la salida antes de cambiar el mapa MIDI del show"};
  auto mapping = ShowMidiMapping();
  const int next = direction > 0
      ? (mapping.channel == 16U ? 1 : mapping.channel + 1)
      : (mapping.channel == 1U ? 16 : mapping.channel - 1);
  mapping.channel = static_cast<std::uint8_t>(next);
  {
    const std::scoped_lock lock(mModelMutex);
    mModel.release_transients();
    SyncSnapshotToAtomicsLocked();
  }
  SyncShowMidiMappingToState(mapping);
  const std::string message =
      "CANAL MIDI SHOW · " + std::to_string(mapping.channel);
  SetShowMidiMessage(message);
  return {true, {}, message};
}

aeyla::product::AuthoringResult AeylaVisualDmx::BeginShowMidiLearnFromUI(
    aeyla::runtime::ShowMidiLearnTarget target)
{
  if(target == aeyla::runtime::ShowMidiLearnTarget::none) {
    mShowMidiLearnTarget.store(target, std::memory_order_release);
    return {true, {}, "APRENDER MIDI CANCELADO"};
  }
  if(TakeOutputArmed() || OutputArmed())
    return {false, {},
            "Desarma la salida antes de aprender notas MIDI del show"};
  mShowMidiLearnTarget.store(target, std::memory_order_release);
  const std::string message =
      std::string("APRENDER MIDI · toca una nota para ") + LearnTargetName(target);
  SetShowMidiMessage(message);
  return {true, {}, message};
}

aeyla::runtime::ShowMidiLearnTarget
AeylaVisualDmx::ShowMidiLearnTarget() const noexcept
{
  return mShowMidiLearnTarget.load(std::memory_order_acquire);
}

bool AeylaVisualDmx::PreloadPreparedTakeForMidiLocked(
    std::size_t song_index,
    std::string& error_message)
{
  error_message.clear();
  const auto& show = mModel.show_program();
  if(song_index >= show.songs.size()) {
    error_message = "La canción preparada no existe";
    return false;
  }
  const auto& song = show.songs[song_index];
  const auto project_id = mModel.snapshot().project_id;
  const auto output_universe = mModel.project_document().output.universe;
  aeyla::take_library_session::ensure_scope(this, project_id);
  const auto library = aeyla::take_library_session::directory(this);
  if(library.empty()) {
    error_message = "Selecciona primero la biblioteca de tomas";
    return false;
  }
  const auto scan = aeyla::capture::scan_take_directory(library, song.song_id);
  if(!scan.ok() || scan.entries.empty()) {
    error_message = scan.ok()
        ? "La canción preparada no tiene una toma DMX"
        : "No se pudo leer la biblioteca · " + scan.error;
    return false;
  }
  const auto& selected = SelectMidiTake(this, song.song_id, scan);
  const auto edited = aeyla::take_library_session::edit_state(this, song.song_id);
  const std::uint64_t expected_start =
      edited.has_value() && edited->path == selected.path
          ? edited->start_frame : 0U;
  const std::uint64_t expected_end =
      edited.has_value() && edited->path == selected.path
          ? edited->end_frame_exclusive : selected.frame_count;
  auto& prepared_reader = mPreparedMidiTakeReaders[song_index];
  auto& prepared_path = mPreparedMidiTakePaths[song_index];
  auto& prepared_start = mPreparedMidiTakeStarts[song_index];
  auto& prepared_end = mPreparedMidiTakeEnds[song_index];
  const auto prepared_info = prepared_reader
      ? prepared_reader->info()
      : aeyla::capture::DmxTakeFileReaderInfo{};
  if(prepared_reader && prepared_path == selected.path &&
     prepared_start == expected_start && prepared_end == expected_end &&
     prepared_info.open && prepared_info.song_id == song.song_id &&
     prepared_info.port_address == output_universe)
    return true;

  auto candidate = std::make_unique<aeyla::capture::DmxTakeFileReader>();
  if(!candidate->open(selected.path, error_message)) {
    error_message = "La toma preparada no superó la validación · " +
                    error_message;
    return false;
  }
  const auto info = candidate->info();
  if(info.song_id != song.song_id ||
     info.port_address != output_universe ||
     expected_start >= expected_end || expected_end > info.frame_count) {
    error_message =
        "La toma preparada no coincide con canción, universo o ENTRADA/SALIDA";
    return false;
  }
  prepared_reader = std::move(candidate);
  prepared_path = selected.path;
  prepared_start = expected_start;
  prepared_end = expected_end;
  return true;
}

bool AeylaVisualDmx::StartPreparedTakeFromMidiLocked(
    std::size_t song_index,
    std::uint64_t trigger_sample,
    std::string& error_message)
{
  error_message.clear();
  if(TakeRecording()) {
    error_message = "MIDI PLAY bloqueado mientras AEYLA está grabando";
    return false;
  }

  const auto& show = mModel.show_program();
  if(song_index >= show.songs.size()) {
    error_message = "La canción MIDI solicitada no existe";
    return false;
  }
  const auto& song = show.songs[song_index];
  const std::string project_id = mModel.snapshot().project_id;
  const std::uint16_t output_universe =
      mModel.project_document().output.universe;
  aeyla::take_library_session::ensure_scope(this, project_id);
  const auto library = aeyla::take_library_session::directory(this);
  if(library.empty()) {
    error_message =
        "Selecciona una biblioteca de tomas antes de usar PLAY por MIDI";
    return false;
  }

  const auto scan = aeyla::capture::scan_take_directory(library, song.song_id);
  if(!scan.ok() || scan.entries.empty()) {
    error_message = scan.ok()
        ? "La canción preparada no tiene una toma DMX"
        : "No se pudo leer la biblioteca de tomas · " + scan.error;
    return false;
  }
  const auto& selected = SelectMidiTake(this, song.song_id, scan);
  if(selected.port_address != output_universe) {
    error_message = "El universo de la toma no coincide con la salida del proyecto";
    return false;
  }

  const auto edited = aeyla::take_library_session::edit_state(this, song.song_id);
  const std::uint64_t expected_start =
      edited.has_value() && edited->path == selected.path
          ? edited->start_frame : 0U;
  const std::uint64_t expected_end =
      edited.has_value() && edited->path == selected.path
          ? edited->end_frame_exclusive : selected.frame_count;
  auto scheduler = mTakeScheduler.status();
  const bool same_clip =
      mLoadedTakeSongIndex.load(std::memory_order_acquire) ==
          static_cast<int>(song_index) &&
      aeyla::take_library_session::loaded_path(this, song.song_id) ==
          selected.path &&
      scheduler.file_backed &&
      scheduler.range_start_frame == expected_start &&
      scheduler.range_end_frame_exclusive == expected_end;

  const bool preserve_arm = scheduler.armed;
  bool cursor_started = false;
  if(!same_clip) {
    if(preserve_arm) {
      std::unique_ptr<aeyla::capture::DmxTakeFileReader> candidate;
      auto& prepared_reader = mPreparedMidiTakeReaders[song_index];
      auto& prepared_path = mPreparedMidiTakePaths[song_index];
      auto& prepared_start = mPreparedMidiTakeStarts[song_index];
      auto& prepared_end = mPreparedMidiTakeEnds[song_index];
      if(prepared_reader && prepared_path == selected.path &&
         prepared_start == expected_start && prepared_end == expected_end) {
        candidate = std::move(prepared_reader);
      }
      else {
        mMidiPreloadSongRequest.store(static_cast<int>(song_index),
                                      std::memory_order_release);
        error_message =
            "La canción aún no está precargada; espera LISTA y repite PLAY";
        return false;
      }
      prepared_path.clear();
      prepared_start = 0U;
      prepared_end = 0U;
      const auto candidate_info = candidate->info();
      if(candidate_info.song_id != song.song_id ||
         candidate_info.port_address != output_universe) {
        error_message =
            "La toma validada no pertenece a la canción/universo preparado";
        return false;
      }
      const auto mutation_completed = BeginShowTransportMutation();
      const auto mutation_elapsed = mutation_completed > trigger_sample
          ? mutation_completed - trigger_sample : 0U;
      const bool replaced = mTakeScheduler.replace_armed_take_file(
             *candidate, GetSampleRate(),
             static_cast<std::size_t>(expected_start),
             static_cast<std::size_t>(expected_end),
             aeyla::capture::DmxClipClockSource::host_samples,
             mutation_elapsed, error_message);
      EndShowTransportMutation();
      if(!replaced) {
        prepared_reader = std::move(candidate);
        prepared_path = selected.path;
        prepared_start = expected_start;
        prepared_end = expected_end;
        error_message = "El cambio de canción conservó la anterior · " +
                        error_message;
        return false;
      }
      cursor_started = true;
      const int previous_index = mLoadedTakeSongIndex.load(
          std::memory_order_acquire);
      if(previous_index >= 0 &&
         static_cast<std::size_t>(previous_index) < show.songs.size() &&
         previous_index != static_cast<int>(song_index)) {
        const auto old_index = static_cast<std::size_t>(previous_index);
        mPreparedMidiTakeReaders[old_index] = std::move(candidate);
        mPreparedMidiTakePaths[old_index] =
            aeyla::take_library_session::loaded_path(
                this, show.songs[old_index].song_id);
        mPreparedMidiTakeStarts[old_index] = scheduler.range_start_frame;
        mPreparedMidiTakeEnds[old_index] =
            scheduler.range_end_frame_exclusive;
      }
    }
    else {
      mTakeScheduler.stop_reset();
      mTakeScheduler.disarm();
      mActiveTakeSongIndex.store(-1, std::memory_order_release);
      mLoadedTakeSongIndex.store(-1, std::memory_order_release);
      mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
      if(!mTakeScheduler.load_take_file(selected.path, GetSampleRate(),
                                        error_message)) {
        error_message = "La toma no superó la validación · " + error_message;
        return false;
      }
      if(edited.has_value() && edited->path == selected.path &&
         !mTakeScheduler.set_play_range(
             static_cast<std::size_t>(edited->start_frame),
             static_cast<std::size_t>(edited->end_frame_exclusive),
             error_message)) {
        error_message =
            "No se pudo aplicar ENTRADA / SALIDA · " + error_message;
        return false;
      }
    }
    aeyla::take_library_session::set_loaded_path(this, song.song_id,
                                                 selected.path);
    mLoadedTakeSongIndex.store(static_cast<int>(song_index),
                               std::memory_order_release);
    scheduler = mTakeScheduler.status();
  }

  if(!preserve_arm || same_clip) {
    const auto mutation_completed = BeginShowTransportMutation();
    const auto mutation_elapsed = mutation_completed > trigger_sample
        ? mutation_completed - trigger_sample : 0U;
    const bool played = mTakeScheduler.play(
        error_message, aeyla::capture::DmxClipClockSource::host_samples,
        mutation_elapsed);
    EndShowTransportMutation();
    if(!played)
      return false;
    cursor_started = true;
  }

  if(cursor_started)
    SynchronizeShowTransportCursor(trigger_sample);

  mActiveTakeSongIndex.store(static_cast<int>(song_index),
                             std::memory_order_release);
  const std::string authority = scheduler.armed
      ? "AL AIRE" : "PREVIA SIN SALIDA";
  SetShowMidiMessage("MIDI PLAY · " + song.name + " · " + authority +
                     " · sincronía por muestras del DAW");
  return true;
}

void AeylaVisualDmx::DrainShowMidiCommandsLocked(
    const aeyla::runtime::HostTransportSnapshot& host)
{
  const auto apply_midi_panic = [&]() {
    mPendingShowMidiEvent.reset();
    aeyla::runtime::ShowMidiEvent ignored{};
    while(mShowMidiIngress.try_consume(ignored))
    {
    }
    mTakeScheduler.stop_reset();
    mTakeScheduler.disarm();
    mActiveTakeSongIndex.store(-1, std::memory_order_release);
    mModel.release_transients();
    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::operator_disarm);
    mModel.set_blackout(true);
    mParamBlackout.store(true, std::memory_order_release);
    SetShowMidiMessage(
        "PANIC MIDI · APAGÓN ACTIVO · salida desarmada · rearme manual");
  };

  // Called with mModelMutex already held by RuntimeTick. This capture path is
  // intentionally implemented here instead of calling the UI toggle: it never
  // opens a folder dialog and therefore remains valid with the plug-in window
  // closed. Disk/network work stays on the non-realtime runtime thread.
  const auto apply_midi_capture_command = [&](aeyla::runtime::ShowMidiCommand command) {
    const bool request_start = command == aeyla::runtime::ShowMidiCommand::capture_start;
    const bool request_stop = command == aeyla::runtime::ShowMidiCommand::capture_stop;
    if(!request_start && !request_stop)
      return;

    if(NetworkConfigurationBusy()) {
      SetShowMidiMessage("MIDI REC BLOQUEADO · espera a que termine el cambio de red");
      return;
    }

    const auto snapshot = mModel.snapshot();
    const std::string project_id = snapshot.project_id;
    const std::string song_id = snapshot.active_song_id;
    const std::string song_name = snapshot.active_song_name;
    const std::uint16_t universe = mModel.project_document().output.universe;
    aeyla::take_library_session::ensure_scope(this, project_id);

    if(request_stop) {
      if(!mArtNetCapture.streamed_recording_active()) {
        SetShowMidiMessage("REC STOP IGNORADO · no existe una captura MIDI activa");
        return;
      }
      const auto expected_target = mActiveCaptureTarget;
      std::string error;
      if(!mArtNetCapture.end_streamed_recording(error)) {
        mActiveCaptureTarget.clear();
        mCaptureSyncAnchor.reset();
        aeyla::take_library_session::set_storage_message(this, "ERROR DE GRABACIÓN MIDI · " + error);
        SetShowMidiMessage("REC STOP · no fue posible cerrar la toma · " + error);
        return;
      }
      mActiveCaptureTarget.clear();
      const auto library = aeyla::take_library_session::directory(this);
      const auto scan = aeyla::capture::scan_take_directory(library, song_id);
      const auto captured = aeyla::capture::find_take_entry_by_path(scan, expected_target);
      if(!scan.ok() || !captured.has_value()) {
        mCaptureSyncAnchor.reset();
        SetShowMidiMessage("REC STOP · archivo final no verificable · " +
            (scan.ok() ? std::string("no apareció en el índice") : scan.error));
        return;
      }
      const auto& newest = *captured;
      aeyla::take_library_session::TakeEditState edit;
      edit.path = newest.path;
      edit.take_name = newest.take_name;
      edit.raw_source = true;
      edit.start_frame = 0U;
      edit.end_frame_exclusive = newest.frame_count;
      edit.frame_count = newest.frame_count;
      edit.frames_per_second = newest.frames_per_second;
      edit.version_count = scan.entries.size();
      edit.version_index = 0U;
      aeyla::take_library_session::set_edit_state(this, song_id, edit);
      aeyla::take_library_session::set_loaded_path(this, song_id, newest.path);
      mCaptureSyncAnchor.reset();
      mMidiPreflightCursor.store(ShowMidiMapping().enabled ? 0 : -1, std::memory_order_release);
      const std::string sync_text = " · CERO = REC START · cuadro 0";
      aeyla::take_library_session::set_storage_message(
          this, "GUARDADA POR MIDI · " + newest.path.filename().string() + sync_text);
      SetShowMidiMessage("REC STOP · " + newest.take_name + " · " +
          std::to_string(newest.frame_count) + " cuadros" + sync_text + " · RAW preservado");
      return;
    }

    if(mArtNetCapture.streamed_recording_active()) {
      SetShowMidiMessage("REC START IGNORADO · la captura ya está activa · usa N43 para detener");
      return;
    }
    if(mArtNetCapture.stats().recording) {
      SetShowMidiMessage("REC START BLOQUEADO · existe otra captura activa; deténla desde su origen");
      return;
    }
    if(OutputArmed() || TakeOutputArmed()) {
      SetShowMidiMessage("REC START BLOQUEADO · desarma la salida física antes de capturar");
      return;
    }
    if(TakePlaying()) {
      SetShowMidiMessage("REC START BLOQUEADO · detén la reproducción de la toma actual");
      return;
    }
    if(song_id.empty()) {
      SetShowMidiMessage("REC START BLOQUEADO · selecciona primero una canción");
      return;
    }
    const auto library = aeyla::take_library_session::directory(this);
    if(library.empty()) {
      SetShowMidiMessage("REC START BLOQUEADO · selecciona una vez la BIBLIOTECA desde la interfaz");
      return;
    }
    std::string directory_error;
    if(!aeyla::capture::prepare_take_directory(library, directory_error)) {
      SetShowMidiMessage("REC START BLOQUEADO · biblioteca sin escritura · " + directory_error);
      return;
    }
    const auto stats = mArtNetCapture.stats();
    if(!stats.running) {
      SetShowMidiMessage("REC START BLOQUEADO · RX Art-Net no está activo · REESCANEA la red");
      return;
    }
    if(!stats.signal_present || stats.source_ipv4.empty()) {
      SetShowMidiMessage("REC START BLOQUEADO · RX sin señal Art-Net válida de Avolites");
      return;
    }
    const auto scan = aeyla::capture::scan_take_directory(library, song_id);
    const std::size_t next_number = scan.ok() ? scan.entries.size() + 1U : 1U;
    const std::string take_name = "Toma " + std::to_string(next_number);
    const auto target = aeyla::capture::make_take_file_path(
        library, song_name.empty() ? song_id : song_name, take_name);
    aeyla::capture::DmxTakeStreamConfig stream;
    stream.target_path = target;
    stream.song_id = song_id;
    stream.song_name = song_name;
    stream.take_name = take_name;
    stream.source_ipv4 = stats.source_ipv4;
    stream.port_address = universe;
    stream.frames_per_second = 44U;

    // REC START itself is the capture origin. No MTC, PLAY edge or N38 marker
    // is allowed to move CERO after recording begins.
    mCaptureSyncAnchor.reset();
    std::string error;
    if(!mArtNetCapture.begin_streamed_recording(stream, error)) {
      mActiveCaptureTarget.clear();
      SetShowMidiMessage("REC START BLOQUEADO · no se pudo iniciar captura · " + error);
      return;
    }
    mActiveCaptureTarget = target;
    aeyla::take_library_session::set_storage_message(
        this, "GRABANDO POR MIDI · " + target.filename().string() + " · CERO = REC START");
    SetShowMidiMessage("REC START · " + take_name + " · " + stats.source_ipv4 +
        " · 44 Hz · CERO fijado por la nota MIDI · N43 detiene");
  };

  if(mShowMidiIngress.consume_panic_request()) {
    apply_midi_panic();
    return;
  }

  if(mShowMidiIngress.consume_safety_stop_request()) {
    mPendingShowMidiEvent.reset();
    mTakeScheduler.stop_reset();
    mTakeScheduler.disarm();
    mActiveTakeSongIndex.store(-1, std::memory_order_release);
    mModel.release_transients();
    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::runtime_fault);
    mModel.set_blackout(true);
    mParamBlackout.store(true, std::memory_order_release);
    SetShowMidiMessage(
        "DESBORDAMIENTO MIDI · APAGÓN Y DESARME · revisa el ruteo del DAW");
    return;
  }

  const int preflight_cursor = mMidiPreflightCursor.load(
      std::memory_order_acquire);
  if(preflight_cursor == 0 && ShowMidiMapping().enabled) {
    for(std::size_t index = 0U;
        index < mPreparedMidiTakeReaders.size(); ++index) {
      mPreparedMidiTakeReaders[index].reset();
      mPreparedMidiTakePaths[index].clear();
      mPreparedMidiTakeStarts[index] = 0U;
      mPreparedMidiTakeEnds[index] = 0U;
    }
  }

  const int preload_song = mMidiPreloadSongRequest.exchange(
      -1, std::memory_order_acq_rel);
  if(preload_song >= 0 && ShowMidiMapping().enabled) {
    std::string preload_error;
    if(PreloadPreparedTakeForMidiLocked(
           static_cast<std::size_t>(preload_song), preload_error))
      SetShowMidiMessage("PREPARADA Y VALIDADA · " +
                         mModel.snapshot().active_song_name +
                         " · PLAY será un cambio sin apagón");
    else
      SetShowMidiMessage("PREPARACIÓN MIDI INCOMPLETA · " + preload_error);
  }

  if(preflight_cursor >= 0 && ShowMidiMapping().enabled && !TakeRecording()) {
    const auto song_count = mModel.snapshot().song_count;
    if(static_cast<std::size_t>(preflight_cursor) < song_count) {
      std::string preflight_error;
      const bool ready = PreloadPreparedTakeForMidiLocked(
          static_cast<std::size_t>(preflight_cursor), preflight_error);
      const int next = preflight_cursor + 1;
      mMidiPreflightCursor.store(next, std::memory_order_release);
      SetShowMidiMessage(
          ready
              ? "PRECARGA MIDI " + std::to_string(next) + "/" +
                    std::to_string(song_count) + " · " +
                    mModel.show_program().songs[
                        static_cast<std::size_t>(preflight_cursor)].name
              : "PRECARGA MIDI " + std::to_string(next) + "/" +
                    std::to_string(song_count) + " · OMITIDA · " +
                    preflight_error);
    }
    else {
      mMidiPreflightCursor.store(-1, std::memory_order_release);
      const auto ready_count = static_cast<std::size_t>(std::count_if(
          mPreparedMidiTakeReaders.begin(), mPreparedMidiTakeReaders.end(),
          [](const auto& reader) { return reader != nullptr; }));
      SetShowMidiMessage(
          "PRECARGA MIDI COMPLETA · " + std::to_string(ready_count) + "/" +
          std::to_string(song_count) +
          " canciones listas para cambio sin apagón");
    }
  }

  const std::uint32_t learned = mPendingMidiLearnPacked.exchange(
      0U, std::memory_order_acq_rel);
  if((learned & (1U << 24U)) != 0U) {
    const auto target = static_cast<aeyla::runtime::ShowMidiLearnTarget>(
        learned & 0xFFU);
    const auto channel = static_cast<std::uint8_t>((learned >> 8U) & 0xFFU);
    const auto note = static_cast<std::uint8_t>((learned >> 16U) & 0xFFU);
    if(ShowMidiMapping().enabled &&
       channel == ShowMidiMapping().channel &&
       note == aeyla::runtime::kShowMidiPanicNote) {
      apply_midi_panic();
      return;
    }
    auto mapping = ShowMidiMapping();
    std::string error;
    if(aeyla::runtime::assign_show_midi_note(
           mapping, target, channel, note, error)) {
      mModel.release_transients();
      SyncShowMidiMappingToState(mapping);
      SetShowMidiMessage(std::string("MIDI APRENDIDO · ") +
                         LearnTargetName(target) + " = nota " +
                         std::to_string(note) + " · canal " +
                         std::to_string(channel));
    }
    else {
      SetShowMidiMessage("MIDI NO ASIGNADO · " + error);
    }
  }

  if(!ShowMidiMapping().enabled) {
    mPendingShowMidiEvent.reset();
    aeyla::runtime::ShowMidiEvent ignored{};
    while(mShowMidiIngress.try_consume(ignored))
    {
    }
    return;
  }

  for(std::size_t handled = 0U; handled < 64U; ++handled) {
    if(!mPendingShowMidiEvent.has_value()) {
      aeyla::runtime::ShowMidiEvent next{};
      if(!mShowMidiIngress.try_consume(next))
        return;
      mPendingShowMidiEvent = next;
    }

    const auto pending_command = mPendingShowMidiEvent->command;
    const std::uint64_t completed = mProcessedAudioSamples.load(
        std::memory_order_acquire);
    // REC START/STOP are operational disk commands, not artistic transport
    // events. Execute them immediately on the runtime worker. Artistic commands
    // retain sample-ready scheduling.
    const bool capture_command =
        pending_command == aeyla::runtime::ShowMidiCommand::capture_start ||
        pending_command == aeyla::runtime::ShowMidiCommand::capture_stop;
    if(!capture_command &&
       !aeyla::runtime::show_midi_event_ready(completed, *mPendingShowMidiEvent))
      return;
    const auto event = *mPendingShowMidiEvent;
    mPendingShowMidiEvent.reset();

    if(event.command == aeyla::runtime::ShowMidiCommand::panic_blackout) {
      apply_midi_panic();
      continue;
    }
    if(event.command == aeyla::runtime::ShowMidiCommand::capture_start ||
       event.command == aeyla::runtime::ShowMidiCommand::capture_stop) {
      apply_midi_capture_command(event.command);
      continue;
    }

    const auto song_count = mModel.snapshot().song_count;
    const auto prepared = mModel.snapshot().active_song_index;
    std::string error;

    if(TakeRecording()) {
      SetShowMidiMessage(
          "MIDI SHOW IGNORADO · durante REC sólo N43 REC STOP controla la captura");
      continue;
    }

    switch(event.command) {
      case aeyla::runtime::ShowMidiCommand::previous_song: {
        if(song_count == 0U || prepared == 0U) {
          SetShowMidiMessage("CANCIÓN ANTERIOR · ya estás al inicio de la lista");
          break;
        }
        (void)mModel.select_song(prepared - 1U);
        if(PreloadPreparedTakeForMidiLocked(prepared - 1U, error))
          SetShowMidiMessage("PREPARADA Y VALIDADA · " +
                             mModel.snapshot().active_song_name +
                             " · la canción al aire continúa");
        else
          SetShowMidiMessage("PREPARADA SIN TOMA VÁLIDA · " + error);
        break;
      }
      case aeyla::runtime::ShowMidiCommand::next_song: {
        if(song_count == 0U || prepared + 1U >= song_count) {
          SetShowMidiMessage("SIGUIENTE CANCIÓN · ya estás al final de la lista");
          break;
        }
        (void)mModel.select_song(prepared + 1U);
        if(PreloadPreparedTakeForMidiLocked(prepared + 1U, error))
          SetShowMidiMessage("PREPARADA Y VALIDADA · " +
                             mModel.snapshot().active_song_name +
                             " · la canción al aire continúa");
        else
          SetShowMidiMessage("PREPARADA SIN TOMA VÁLIDA · " + error);
        break;
      }
      case aeyla::runtime::ShowMidiCommand::play_retrigger:
        if(!host.running) {
          SetShowMidiMessage(
              "PLAY MIDI IGNORADO · el transporte del DAW está detenido");
          break;
        }
        if(!StartPreparedTakeFromMidiLocked(
               prepared, event.trigger_sample, error))
          SetShowMidiMessage("PLAY MIDI BLOQUEADO · " + error);
        break;
      case aeyla::runtime::ShowMidiCommand::pause_resume: {
        const auto status = mTakeScheduler.status();
        if(status.playing) {
          const auto mutation_completed = BeginShowTransportMutation();
          const auto rewind = aeyla::runtime::show_midi_elapsed_samples(
              mutation_completed, event);
          const bool paused = mTakeScheduler.pause(error, rewind);
          EndShowTransportMutation();
          if(paused)
            SetShowMidiMessage("PAUSA MIDI · cuadro DMX mantenido");
          else
            SetShowMidiMessage("PAUSA MIDI BLOQUEADA · " + error);
        }
        else if(status.paused) {
          if(!host.running)
            SetShowMidiMessage(
                "REANUDAR MIDI IGNORADO · el DAW está detenido");
          else {
            const auto mutation_completed = BeginShowTransportMutation();
            const auto resume_elapsed =
                aeyla::runtime::show_midi_elapsed_samples(
                    mutation_completed, event);
            const bool resumed = mTakeScheduler.resume(error, resume_elapsed);
            EndShowTransportMutation();
            if(resumed) {
              SynchronizeShowTransportCursor(event.trigger_sample,
                                             status.cursor_samples);
              SetShowMidiMessage(
                  "REANUDAR MIDI · sincronía por muestras del DAW");
            }
            else
              SetShowMidiMessage("REANUDAR MIDI BLOQUEADO · " + error);
          }
        }
        else
          SetShowMidiMessage("PAUSA MIDI IGNORADA · no hay una toma en transporte");
        break;
      }
      case aeyla::runtime::ShowMidiCommand::stop_reset:
        (void)BeginShowTransportMutation();
        mTakeScheduler.stop_reset();
        EndShowTransportMutation();
        mActiveTakeSongIndex.store(-1, std::memory_order_release);
        SetShowMidiMessage("STOP / RESET MIDI · cursor en cero · armado conservado");
        break;
      case aeyla::runtime::ShowMidiCommand::panic_blackout:
        apply_midi_panic();
        break;
      case aeyla::runtime::ShowMidiCommand::capture_start:
      case aeyla::runtime::ShowMidiCommand::capture_stop:
        apply_midi_capture_command(event.command);
        break;
      case aeyla::runtime::ShowMidiCommand::launch_song:
        if(event.song_index >= song_count) {
          SetShowMidiMessage("LANZAMIENTO MIDI IGNORADO · esa canción no existe");
          break;
        }
        (void)mModel.select_song(event.song_index);
        if(!host.running)
          SetShowMidiMessage(
              "CANCIÓN PREPARADA · inicia el DAW para lanzarla sincronizada");
        else if(!StartPreparedTakeFromMidiLocked(
                    event.song_index, event.trigger_sample, error))
          SetShowMidiMessage("LANZAMIENTO MIDI BLOQUEADO · " + error);
        break;
    }
  }
}

void AeylaVisualDmx::ClearShowMidiCommandsLocked() noexcept
{
  mPendingShowMidiEvent.reset();
  mLoadedTakeSongIndex.store(-1, std::memory_order_release);
  mActiveTakeSongIndex.store(-1, std::memory_order_release);
  mPendingMidiLearnPacked.store(0U, std::memory_order_release);
  mShowMidiLearnTarget.store(aeyla::runtime::ShowMidiLearnTarget::none,
                             std::memory_order_release);
  mMidiPreloadSongRequest.store(-1, std::memory_order_release);
  mMidiPreflightCursor.store(-1, std::memory_order_release);
  for(std::size_t index = 0U; index < mPreparedMidiTakeReaders.size(); ++index) {
    mPreparedMidiTakeReaders[index].reset();
    mPreparedMidiTakePaths[index].clear();
    mPreparedMidiTakeStarts[index] = 0U;
    mPreparedMidiTakeEnds[index] = 0U;
  }
  aeyla::runtime::ShowMidiEvent ignored{};
  while(mShowMidiIngress.try_consume(ignored))
  {
  }
}
