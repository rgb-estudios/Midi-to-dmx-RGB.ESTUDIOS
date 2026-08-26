#include "AeylaVisualDmx.h"
#include "AeylaTakeLibrarySession.h"
#include "capture/dmx_take_activity.h"
#include "capture/dmx_take_consolidator.h"
#include "capture/dmx_take_file_store.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>

namespace {

std::string FormatTime(double seconds)
{
  if(!std::isfinite(seconds) || seconds < 0.0)
    seconds = 0.0;
  const auto totalMs = static_cast<std::uint64_t>(std::llround(seconds * 1000.0));
  const auto minutes = totalMs / 60000U;
  const auto secondsPart = (totalMs / 1000U) % 60U;
  const auto millis = totalMs % 1000U;
  std::ostringstream stream;
  stream << minutes << ':' << std::setw(2) << std::setfill('0') << secondsPart
         << '.' << std::setw(3) << millis;
  return stream.str();
}

long long DeltaFrames(double deltaSeconds, std::uint16_t fps)
{
  if(!std::isfinite(deltaSeconds) || fps == 0U)
    return 0LL;
  return static_cast<long long>(std::llround(
      deltaSeconds * static_cast<double>(fps)));
}

bool IsRawTakeName(std::string_view name)
{
  return !name.starts_with("Clip consolidado ");
}

std::optional<aeyla::take_library_session::TakeEditState> MakeEditState(
    const aeyla::capture::TakeFileIndexEntry& entry,
    std::string& error)
{
  const auto activity = aeyla::capture::build_take_activity_envelope(entry.path);
  if(!activity.ok()) {
    error = "No se pudo analizar la actividad DMX · " + activity.error;
    return std::nullopt;
  }
  if(activity.frame_count != entry.frame_count ||
     activity.frames_per_second != entry.frames_per_second) {
    error = "La geometría de la toma cambió durante su lectura";
    return std::nullopt;
  }

  aeyla::take_library_session::TakeEditState state;
  state.path = entry.path;
  state.take_name = entry.take_name;
  state.raw_source = IsRawTakeName(entry.take_name);
  state.start_frame = 0U;
  state.end_frame_exclusive = entry.frame_count;
  state.frame_count = entry.frame_count;
  state.frames_per_second = entry.frames_per_second;
  state.activity_count = std::min(activity.buckets.size(),
                                  state.activity_level.size());
  for(std::size_t index = 0U; index < state.activity_count; ++index) {
    state.activity_level[index] = activity.buckets[index].level;
    state.activity_motion[index] = activity.buckets[index].motion;
  }
  return state;
}

std::optional<aeyla::take_library_session::TakeEditState> LatestEditState(
    const void* owner,
    const std::filesystem::path& library,
    std::string_view songId,
    std::string& error)
{
  error.clear();
  if(library.empty()) {
    error = "Selecciona primero una biblioteca de tomas";
    return std::nullopt;
  }
  const auto scan = aeyla::capture::scan_take_directory(library, songId);
  if(!scan.ok()) {
    error = scan.error;
    return std::nullopt;
  }
  if(scan.entries.empty()) {
    error = "No existe una toma DMX para editar";
    return std::nullopt;
  }

  auto current = aeyla::take_library_session::edit_state(owner, songId);
  if(current.has_value()) {
    const auto match = std::find_if(
        scan.entries.begin(), scan.entries.end(),
        [&](const aeyla::capture::TakeFileIndexEntry& entry) {
          return entry.path == current->path &&
                 entry.frame_count == current->frame_count &&
                 entry.frames_per_second == current->frames_per_second;
        });
    if(match != scan.entries.end() && current->activity_count > 0U)
      return current;
    if(match != scan.entries.end()) {
      auto refreshed = MakeEditState(*match, error);
      if(!refreshed.has_value()) return std::nullopt;
      refreshed->start_frame = std::min(current->start_frame,
                                        refreshed->frame_count - 1U);
      refreshed->end_frame_exclusive = std::clamp(
          current->end_frame_exclusive, refreshed->start_frame + 1U,
          refreshed->frame_count);
      aeyla::take_library_session::set_edit_state(owner, songId, *refreshed);
      return refreshed;
    }
  }

  auto state = MakeEditState(scan.entries.front(), error);
  if(!state.has_value()) return std::nullopt;
  aeyla::take_library_session::set_edit_state(owner, songId, *state);
  return state;
}

}  // namespace

aeyla::product::AuthoringResult AeylaVisualDmx::AdjustActiveTakeInFromUI(
    double deltaSeconds)
{
  if(TakeRecording())
    return {false, {}, "Detén GRABAR antes de editar la ENTRADA"};
  if(TakePlaying())
    return {false, {}, "Pausa la reproducción antes de editar la ENTRADA"};
  if(TakeOutputArmed())
    return {false, {}, "Desarma la salida antes de editar ENTRADA / SALIDA"};

  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  if(songId.empty())
    return {false, {}, "No hay una canción seleccionada"};

  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value())
    return {false, {}, error};
  if(state->frame_count < 2U || state->frames_per_second == 0U)
    return {false, {}, "La toma es demasiado corta para recortarla"};

  const long long current = static_cast<long long>(state->start_frame);
  const long long maximum = static_cast<long long>(state->end_frame_exclusive) - 2LL;
  const long long next = std::clamp(
      current + DeltaFrames(deltaSeconds, state->frames_per_second),
      0LL, std::max(0LL, maximum));
  state->start_frame = static_cast<std::uint64_t>(next);
  aeyla::take_library_session::set_edit_state(this, songId, *state);

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  if(!mTakeScheduler.load_take_file(state->path, GetSampleRate(), error))
    return {false, {}, error};
  if(!mTakeScheduler.set_play_range(
         static_cast<std::size_t>(state->start_frame),
         static_cast<std::size_t>(state->end_frame_exclusive), error))
    return {false, {}, error};
  aeyla::take_library_session::set_loaded_path(this, songId, state->path);

  const double inSeconds = static_cast<double>(state->start_frame) /
                           static_cast<double>(state->frames_per_second);
  const double outSeconds = static_cast<double>(state->end_frame_exclusive) /
                            static_cast<double>(state->frames_per_second);
  return {true, {}, "ENTRADA " + FormatTime(inSeconds) +
                    " · SALIDA " + FormatTime(outSeconds)};
}

aeyla::product::AuthoringResult AeylaVisualDmx::AdjustActiveTakeOutFromUI(
    double deltaSeconds)
{
  if(TakeRecording())
    return {false, {}, "Detén GRABAR antes de editar la SALIDA"};
  if(TakePlaying())
    return {false, {}, "Pausa la reproducción antes de editar la SALIDA"};
  if(TakeOutputArmed())
    return {false, {}, "Desarma la salida antes de editar ENTRADA / SALIDA"};

  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  if(songId.empty())
    return {false, {}, "No hay una canción seleccionada"};

  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value())
    return {false, {}, error};
  if(state->frame_count < 2U || state->frames_per_second == 0U)
    return {false, {}, "La toma es demasiado corta para recortarla"};

  const long long current = static_cast<long long>(state->end_frame_exclusive);
  const long long minimum = static_cast<long long>(state->start_frame) + 2LL;
  const long long maximum = static_cast<long long>(state->frame_count);
  const long long next = std::clamp(
      current + DeltaFrames(deltaSeconds, state->frames_per_second),
      minimum, maximum);
  state->end_frame_exclusive = static_cast<std::uint64_t>(next);
  aeyla::take_library_session::set_edit_state(this, songId, *state);

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  if(!mTakeScheduler.load_take_file(state->path, GetSampleRate(), error))
    return {false, {}, error};
  if(!mTakeScheduler.set_play_range(
         static_cast<std::size_t>(state->start_frame),
         static_cast<std::size_t>(state->end_frame_exclusive), error))
    return {false, {}, error};
  aeyla::take_library_session::set_loaded_path(this, songId, state->path);

  const double inSeconds = static_cast<double>(state->start_frame) /
                           static_cast<double>(state->frames_per_second);
  const double outSeconds = static_cast<double>(state->end_frame_exclusive) /
                            static_cast<double>(state->frames_per_second);
  return {true, {}, "ENTRADA " + FormatTime(inSeconds) +
                    " · SALIDA " + FormatTime(outSeconds)};
}

aeyla::product::AuthoringResult AeylaVisualDmx::ResetActiveTakeTrimFromUI()
{
  if(TakeRecording() || TakePlaying() || TakeOutputArmed())
    return {false, {}, "Pausa y desarma antes de restaurar ENTRADA / SALIDA"};

  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  if(songId.empty())
    return {false, {}, "No hay una canción seleccionada"};

  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value())
    return {false, {}, error};
  state->start_frame = 0U;
  state->end_frame_exclusive = state->frame_count;
  aeyla::take_library_session::set_edit_state(this, songId, *state);

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  if(!mTakeScheduler.load_take_file(state->path, GetSampleRate(), error))
    return {false, {}, error};
  if(!mTakeScheduler.set_play_range(0U,
         static_cast<std::size_t>(state->frame_count), error))
    return {false, {}, error};
  return {true, {}, "ENTRADA / SALIDA restauradas a la grabación completa"};
}

aeyla::product::AuthoringResult AeylaVisualDmx::ConsolidateActiveTakeFromUI()
{
  if(TakeRecording())
    return {false, {}, "Detén GRABAR antes de consolidar el clip"};
  if(TakePlaying())
    return {false, {}, "Pausa o detén la reproducción antes de consolidar"};
  if(TakeOutputArmed())
    return {false, {}, "Desarma la salida física antes de consolidar"};

  std::string projectId;
  std::string songId;
  std::string songName;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
    songName = snapshot.active_song_name;
  }
  if(songId.empty())
    return {false, {}, "No hay una canción seleccionada"};

  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  const auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value())
    return {false, {}, error};
  if(state->start_frame >= state->end_frame_exclusive)
    return {false, {}, "El rango ENTRADA / SALIDA no contiene cuadros DMX"};

  const auto scan = aeyla::capture::scan_take_directory(library, songId);
  if(!scan.ok())
    return {false, {}, "No se pudo revisar la biblioteca de tomas · " + scan.error};

  const std::string clipName =
      "Clip consolidado " + std::to_string(scan.entries.size() + 1U);
  const auto target = aeyla::capture::make_take_file_path(
      library, songName.empty() ? songId : songName, clipName);

  aeyla::capture::DmxTakeConsolidateRequest request;
  request.source_path = state->path;
  request.target_path = target;
  request.start_frame = state->start_frame;
  request.end_frame_exclusive = state->end_frame_exclusive;
  request.consolidated_name = clipName;

  const auto consolidated = aeyla::capture::consolidate_take_range(request);
  if(!consolidated.succeeded)
    return {false, {}, consolidated.error};

  aeyla::capture::TakeFileIndexEntry consolidatedEntry;
  consolidatedEntry.path = consolidated.target_path;
  consolidatedEntry.take_name = clipName;
  consolidatedEntry.frame_count = consolidated.frame_count;
  consolidatedEntry.frames_per_second = consolidated.frames_per_second;
  auto consolidatedState = MakeEditState(consolidatedEntry, error);
  if(!consolidatedState.has_value())
    return {false, clipName,
            "El clip se guardó, pero su actividad no pudo indexarse · " + error};
  aeyla::take_library_session::set_edit_state(this, songId,
                                               *consolidatedState);
  aeyla::take_library_session::set_loaded_path(this, songId,
                                               consolidated.target_path);
  aeyla::take_library_session::set_storage_message(
      this, "CLIP CONSOLIDADO · " + consolidated.target_path.filename().string());

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  if(!mTakeScheduler.load_take_file(consolidated.target_path,
                                    GetSampleRate(), error))
    return {false, clipName,
            "El clip se guardó, pero no pudo prepararse para reproducción · " + error};

  return {true, clipName,
          "CLIP CONSOLIDADO · 00:00 = ENTRADA · " +
              FormatTime(consolidated.duration_seconds) + " · 44 Hz"};
}

aeyla::product::AuthoringResult AeylaVisualDmx::SetActiveTakeInFrameFromUI(
    std::uint64_t frameIndex)
{
  if(TakeRecording() || TakePlaying() || TakeOutputArmed())
    return {false, {}, "Pausa y desarma antes de mover la ENTRADA"};

  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  if(songId.empty()) return {false, {}, "No hay una canción seleccionada"};
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value()) return {false, {}, error};
  if(state->end_frame_exclusive < 2U)
    return {false, {}, "La toma es demasiado corta para recortarla"};
  state->start_frame = std::min(frameIndex, state->end_frame_exclusive - 2U);

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  if(aeyla::take_library_session::loaded_path(this, songId) != state->path &&
     !mTakeScheduler.load_take_file(state->path, GetSampleRate(), error))
    return {false, {}, error};
  if(!mTakeScheduler.set_play_range(
         static_cast<std::size_t>(state->start_frame),
         static_cast<std::size_t>(state->end_frame_exclusive), error))
    return {false, {}, error};
  aeyla::take_library_session::set_edit_state(this, songId, *state);
  aeyla::take_library_session::set_loaded_path(this, songId, state->path);
  return {true, {}, "ENTRADA " + FormatTime(
      static_cast<double>(state->start_frame) / state->frames_per_second)};
}

aeyla::product::AuthoringResult AeylaVisualDmx::SetActiveTakeOutFrameFromUI(
    std::uint64_t frameIndexExclusive)
{
  if(TakeRecording() || TakePlaying() || TakeOutputArmed())
    return {false, {}, "Pausa y desarma antes de mover la SALIDA"};

  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  if(songId.empty()) return {false, {}, "No hay una canción seleccionada"};
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value()) return {false, {}, error};
  state->end_frame_exclusive = std::clamp(
      frameIndexExclusive, state->start_frame + 2U, state->frame_count);

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  if(aeyla::take_library_session::loaded_path(this, songId) != state->path &&
     !mTakeScheduler.load_take_file(state->path, GetSampleRate(), error))
    return {false, {}, error};
  if(!mTakeScheduler.set_play_range(
         static_cast<std::size_t>(state->start_frame),
         static_cast<std::size_t>(state->end_frame_exclusive), error))
    return {false, {}, error};
  aeyla::take_library_session::set_edit_state(this, songId, *state);
  aeyla::take_library_session::set_loaded_path(this, songId, state->path);
  return {true, {}, "SALIDA " + FormatTime(
      static_cast<double>(state->end_frame_exclusive) /
      state->frames_per_second)};
}

aeyla::product::AuthoringResult AeylaVisualDmx::SeekActiveTakeFrameFromUI(
    std::uint64_t frameIndex)
{
  if(TakeRecording() || TakePlaying())
    return {false, {}, "Pausa GRABAR / REPRODUCIR antes de mover el cabezal"};
  if(TakeOutputArmed())
    return {false, {}, "Desarma la salida física antes de previsualizar"};

  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  const auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value()) return {false, {}, error};
  const auto bounded = std::clamp(frameIndex, state->start_frame,
                                  state->end_frame_exclusive - 1U);
  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  if(aeyla::take_library_session::loaded_path(this, songId) != state->path) {
    if(!mTakeScheduler.load_take_file(state->path, GetSampleRate(), error) ||
       !mTakeScheduler.set_play_range(
           static_cast<std::size_t>(state->start_frame),
           static_cast<std::size_t>(state->end_frame_exclusive), error))
      return {false, {}, error};
    aeyla::take_library_session::set_loaded_path(this, songId, state->path);
  }
  if(!mTakeScheduler.seek_frame(static_cast<std::size_t>(bounded), error))
    return {false, {}, error};
  return {true, {}, "CABEZAL " + FormatTime(
      static_cast<double>(bounded) / state->frames_per_second) +
      " · previsualización sin salida física"};
}

aeyla::product::AuthoringResult AeylaVisualDmx::CycleActiveTakeVersionFromUI(
    int direction)
{
  if(direction == 0) return {true, {}, {}};
  if(TakeRecording() || TakePlaying() || TakeOutputArmed())
    return {false, {}, "Pausa y desarma antes de cambiar de versión"};

  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  auto scan = aeyla::capture::scan_take_directory(library, songId);
  if(!scan.ok() || scan.entries.empty())
    return {false, {}, scan.ok() ? "No hay versiones de esta toma" : scan.error};
  std::reverse(scan.entries.begin(), scan.entries.end());
  const auto current = aeyla::take_library_session::edit_state(this, songId);
  std::size_t currentIndex = scan.entries.size() - 1U;
  if(current.has_value()) {
    const auto found = std::find_if(scan.entries.begin(), scan.entries.end(),
        [&](const auto& entry) { return entry.path == current->path; });
    if(found != scan.entries.end())
      currentIndex = static_cast<std::size_t>(found - scan.entries.begin());
  }
  const auto next = std::clamp<long long>(
      static_cast<long long>(currentIndex) + (direction < 0 ? -1LL : 1LL),
      0LL, static_cast<long long>(scan.entries.size() - 1U));
  std::string error;
  auto state = MakeEditState(scan.entries[static_cast<std::size_t>(next)], error);
  if(!state.has_value()) return {false, {}, error};
  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  if(!mTakeScheduler.load_take_file(state->path, GetSampleRate(), error) ||
     !mTakeScheduler.set_play_range(0U,
          static_cast<std::size_t>(state->frame_count), error))
    return {false, {}, error};
  aeyla::take_library_session::set_edit_state(this, songId, *state);
  aeyla::take_library_session::set_loaded_path(this, songId, state->path);
  return {true, state->take_name,
          "VERSIÓN " + std::to_string(next + 1U) + " / " +
          std::to_string(scan.entries.size()) + " · " + state->take_name};
}

aeyla::product::AuthoringResult AeylaVisualDmx::ReturnToRawTakeFromUI()
{
  if(TakeRecording() || TakePlaying() || TakeOutputArmed())
    return {false, {}, "Pausa y desarma antes de volver a la toma original"};
  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  const auto scan = aeyla::capture::scan_take_directory(library, songId);
  if(!scan.ok()) return {false, {}, scan.error};
  const auto raw = std::find_if(scan.entries.begin(), scan.entries.end(),
      [](const auto& entry) { return IsRawTakeName(entry.take_name); });
  if(raw == scan.entries.end())
    return {false, {}, "No se encontró una toma RAW para esta canción"};
  std::string error;
  auto state = MakeEditState(*raw, error);
  if(!state.has_value()) return {false, {}, error};
  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  if(!mTakeScheduler.load_take_file(state->path, GetSampleRate(), error))
    return {false, {}, error};
  aeyla::take_library_session::set_edit_state(this, songId, *state);
  aeyla::take_library_session::set_loaded_path(this, songId, state->path);
  return {true, state->take_name, "TOMA ORIGINAL · " + state->take_name};
}

AeylaTakeEditorSnapshot AeylaVisualDmx::ActiveTakeEditorSnapshot() const
{
  AeylaTakeEditorSnapshot result;
  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  if(songId.empty()) return result;
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  const auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value()) return result;

  result.available = true;
  result.raw_source = state->raw_source;
  result.path = state->path;
  result.take_name = state->take_name;
  result.frame_count = state->frame_count;
  result.start_frame = state->start_frame;
  result.end_frame_exclusive = state->end_frame_exclusive;
  result.frames_per_second = state->frames_per_second;
  result.activity_level = state->activity_level;
  result.activity_motion = state->activity_motion;
  result.activity_count = state->activity_count;

  const auto loaded = aeyla::take_library_session::loaded_path(this, songId);
  const auto scheduler = mTakeScheduler.status();
  result.current_frame = loaded == state->path
      ? std::min<std::uint64_t>(scheduler.current_frame,
                                state->frame_count - 1U)
      : state->start_frame;

  auto scan = aeyla::capture::scan_take_directory(library, songId);
  if(scan.ok()) {
    std::reverse(scan.entries.begin(), scan.entries.end());
    result.version_count = scan.entries.size();
    const auto found = std::find_if(scan.entries.begin(), scan.entries.end(),
        [&](const auto& entry) { return entry.path == state->path; });
    if(found != scan.entries.end())
      result.version_index = static_cast<std::size_t>(found - scan.entries.begin());
  }
  return result;
}

double AeylaVisualDmx::ActiveTakeInSeconds() const
{
  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  const auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value() || state->frames_per_second == 0U) return 0.0;
  return static_cast<double>(state->start_frame) /
         static_cast<double>(state->frames_per_second);
}

double AeylaVisualDmx::ActiveTakeOutSeconds() const
{
  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  const auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value() || state->frames_per_second == 0U) return 0.0;
  return static_cast<double>(state->end_frame_exclusive) /
         static_cast<double>(state->frames_per_second);
}

double AeylaVisualDmx::ActiveTakeOriginalDurationSeconds() const
{
  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  const auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value() || state->frames_per_second == 0U) return 0.0;
  return static_cast<double>(state->frame_count) /
         static_cast<double>(state->frames_per_second);
}

double AeylaVisualDmx::ActiveTakeEffectiveDurationSeconds() const
{
  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string error;
  const auto state = LatestEditState(this, library, songId, error);
  if(!state.has_value() || state->frames_per_second == 0U ||
     state->end_frame_exclusive <= state->start_frame) return 0.0;
  return static_cast<double>(state->end_frame_exclusive - state->start_frame) /
         static_cast<double>(state->frames_per_second);
}
