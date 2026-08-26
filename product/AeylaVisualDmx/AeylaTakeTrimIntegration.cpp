#include "AeylaVisualDmx.h"
#include "AeylaTakeLibrarySession.h"
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

  const auto& newest = scan.entries.front();
  auto current = aeyla::take_library_session::edit_state(owner, songId);
  if(current.has_value() && current->path == newest.path &&
     current->frame_count == newest.frame_count &&
     current->frames_per_second == newest.frames_per_second)
    return current;

  aeyla::take_library_session::TakeEditState state;
  state.path = newest.path;
  state.start_frame = 0U;
  state.end_frame_exclusive = newest.frame_count;
  state.frame_count = newest.frame_count;
  state.frames_per_second = newest.frames_per_second;
  aeyla::take_library_session::set_edit_state(owner, songId, state);
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

  aeyla::take_library_session::TakeEditState consolidatedState;
  consolidatedState.path = consolidated.target_path;
  consolidatedState.start_frame = 0U;
  consolidatedState.end_frame_exclusive = consolidated.frame_count;
  consolidatedState.frame_count = consolidated.frame_count;
  consolidatedState.frames_per_second = consolidated.frames_per_second;
  aeyla::take_library_session::set_edit_state(this, songId, consolidatedState);
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
