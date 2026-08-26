#include "AeylaVisualDmx.h"
#include "AeylaTakeLibrarySession.h"
#include "capture/dmx_take_file_store.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>

namespace {

std::string FormatDuration(double seconds)
{
  if(!std::isfinite(seconds) || seconds < 0.0)
    seconds = 0.0;
  const auto total = static_cast<std::uint64_t>(std::llround(seconds));
  const auto minutes = total / 60U;
  const auto remainder = total % 60U;
  std::ostringstream stream;
  stream << minutes << ':' << std::setw(2) << std::setfill('0') << remainder;
  return stream.str();
}

std::size_t WrapIndex(std::size_t current, int direction, std::size_t count)
{
  if(count == 0U || direction == 0)
    return current;
  if(direction > 0)
    return (current + 1U) % count;
  return current == 0U ? count - 1U : current - 1U;
}

std::string TrimOperatorText(std::string_view value)
{
  while(!value.empty() &&
        std::isspace(static_cast<unsigned char>(value.front())) != 0)
    value.remove_prefix(1U);
  while(!value.empty() &&
        std::isspace(static_cast<unsigned char>(value.back())) != 0)
    value.remove_suffix(1U);
  return std::string(value);
}

std::optional<std::filesystem::path> PromptTakeLibraryDirectory(IGraphics* ui)
{
  if(ui == nullptr)
    return std::nullopt;
  WDL_String selected;
  ui->PromptForDirectory(selected);
  const char* text = selected.Get();
  if(text == nullptr || text[0] == '\0')
    return std::nullopt;
  return std::filesystem::u8path(text);
}

std::optional<aeyla::capture::TakeFileIndexEntry> NewestTake(
    const std::filesystem::path& library,
    std::string_view songId,
    std::string& error)
{
  error.clear();
  if(library.empty()) {
    error = "No hay una biblioteca de tomas seleccionada";
    return std::nullopt;
  }
  auto scan = aeyla::capture::scan_take_directory(library, songId);
  if(!scan.ok()) {
    error = scan.error;
    return std::nullopt;
  }
  if(scan.entries.empty()) {
    error = "No hay tomas DMX guardadas para esta canción";
    return std::nullopt;
  }
  return scan.entries.front();
}

}  // namespace

std::size_t AeylaVisualDmx::SongCount() const
{
  const std::scoped_lock lock(mModelMutex);
  return mModel.snapshot().song_count;
}

std::size_t AeylaVisualDmx::ActiveSongIndex() const
{
  const std::scoped_lock lock(mModelMutex);
  return mModel.snapshot().active_song_index;
}

std::string AeylaVisualDmx::SongName(std::size_t songIndex) const
{
  const std::scoped_lock lock(mModelMutex);
  const auto& show = mModel.show_program();
  if(songIndex >= show.songs.size())
    return {};
  return show.songs[songIndex].name;
}

aeyla::product::AuthoringResult AeylaVisualDmx::RenameSongFromUI(
    std::size_t songIndex, std::string_view name)
{
  if(TakeRecording())
    return {false, {}, "Detén la grabación antes de renombrar una canción"};
  if(TakePlaying())
    return {false, {}, "Detén la reproducción antes de renombrar una canción"};

  const std::string normalized = TrimOperatorText(name);
  if(normalized.empty())
    return {false, {}, "El nombre de la canción no puede estar vacío"};
  if(normalized.size() > 64U)
    return {false, {}, "El nombre de la canción admite hasta 64 caracteres"};
  if(std::any_of(normalized.begin(), normalized.end(), [](char value) {
       const unsigned char byte = static_cast<unsigned char>(value);
       return byte < 0x20U && value != '\t';
     }))
    return {false, {}, "El nombre contiene caracteres de control no admitidos"};

  const std::scoped_lock lock(mModelMutex);
  auto program = mModel.show_program();
  if(songIndex >= program.songs.size())
    return {false, {}, "La canción ya no existe"};
  if(program.songs[songIndex].name == normalized)
    return {true, program.songs[songIndex].song_id, normalized};

  const std::string songId = program.songs[songIndex].song_id;
  program.songs[songIndex].name = normalized;
  const auto validation = mModel.replace_show_program(program);
  if(!validation.ok())
    return {false, songId, "El cambio de nombre no superó la validación del show"};

  mParamBlackout.store(true, std::memory_order_release);
  mLastProjectedSongId.clear();
  mLastProjectedTick = 0U;
  SyncSnapshotToAtomicsLocked();
  return {true, songId, "Canción renombrada · " + normalized};
}

void AeylaVisualDmx::SetBlackoutFromUI(bool enabled)
{
  if(enabled)
    mTakeScheduler.disarm();

  GetParam(kParamBlackout)->Set(enabled ? 1.0 : 0.0);
  mParamBlackout.store(enabled, std::memory_order_release);

  const std::scoped_lock lock(mModelMutex);
  mModel.release_transients();
  if(enabled)
    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::operator_disarm);
  mModel.set_blackout(enabled);
  SyncSnapshotToAtomicsLocked();
  PublishOutputFrameLocked(
      mRenderingOffline.load(std::memory_order_acquire));
}

bool AeylaVisualDmx::SelectSongFromUI(std::size_t songIndex)
{
  if(TakeRecording())
    return false;

  // Seleccionar otra canción nunca deja una autoridad antigua al aire.
  mTakeScheduler.stop_reset();
  mTakeScheduler.disarm();

  const std::scoped_lock lock(mModelMutex);
  if(!mModel.select_song(songIndex))
    return false;

  bool bound = false;
  {
    const std::scoped_lock stateLock(mHostStateMutex);
    bound = std::any_of(
        mHostStateCache.song_bindings.begin(),
        mHostStateCache.song_bindings.end(),
        [&](const aeyla::runtime::SessionSongBinding& candidate) {
          return candidate.song_id == mModel.snapshot().active_song_id;
        });
  }
  mActiveSongBound.store(bound, std::memory_order_release);
  mLastProjectedSongId.clear();
  mLastProjectedTick = 0U;
  SyncSnapshotToAtomicsLocked();
  return true;
}

bool AeylaVisualDmx::RefreshNetworkInterfacesFromUI()
{
  const auto discovered = aeyla::network::enumerate_ipv4_interfaces();
  std::string previousRx;
  std::string previousTx;
  {
    const std::scoped_lock lock(mNetworkMutex);
    if(mRxInterfaceIndex < mNetworkInterfaces.size())
      previousRx = mNetworkInterfaces[mRxInterfaceIndex].ipv4;
    if(mTxInterfaceIndex < mNetworkInterfaces.size())
      previousTx = mNetworkInterfaces[mTxInterfaceIndex].ipv4;

    mNetworkInterfaces = discovered;
    mRxInterfaceIndex = 0U;
    mTxInterfaceIndex = mNetworkInterfaces.size() > 1U ? 1U : 0U;

    const auto restore = [&](const std::string& address,
                             std::size_t fallback) -> std::size_t {
      if(address.empty()) return fallback;
      const auto found = std::find_if(
          mNetworkInterfaces.begin(), mNetworkInterfaces.end(),
          [&](const aeyla::network::NetworkInterface& item) {
            return item.ipv4 == address;
          });
      return found == mNetworkInterfaces.end()
                 ? fallback
                 : static_cast<std::size_t>(
                       std::distance(mNetworkInterfaces.begin(), found));
    };
    mRxInterfaceIndex = restore(previousRx, mRxInterfaceIndex);
    mTxInterfaceIndex = restore(previousTx, mTxInterfaceIndex);
    if(mNetworkInterfaces.empty())
      mCaptureInputError = "No se detectaron adaptadores IPv4 activos";
  }

  mArtNetOutput.set_preferred_source_ipv4(SelectedTxIpv4());
  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  RestartCaptureInputFromRouting();
  return !discovered.empty();
}

std::size_t AeylaVisualDmx::NetworkInterfaceCount() const
{
  const std::scoped_lock lock(mNetworkMutex);
  return mNetworkInterfaces.size();
}

bool AeylaVisualDmx::CycleRxInterfaceFromUI(int direction)
{
  if(direction == 0 || TakeRecording())
    return false;
  {
    const std::scoped_lock lock(mNetworkMutex);
    if(mNetworkInterfaces.empty())
      return false;
    mRxInterfaceIndex = WrapIndex(mRxInterfaceIndex, direction,
                                  mNetworkInterfaces.size());
  }
  RestartCaptureInputFromRouting();
  return true;
}

bool AeylaVisualDmx::CycleTxInterfaceFromUI(int direction)
{
  if(direction == 0)
    return false;

  mTakeScheduler.disarm();
  ForceDisarmFromUI();
  {
    const std::scoped_lock lock(mNetworkMutex);
    if(mNetworkInterfaces.empty())
      return false;
    mTxInterfaceIndex = WrapIndex(mTxInterfaceIndex, direction,
                                  mNetworkInterfaces.size());
  }
  mArtNetOutput.set_preferred_source_ipv4(SelectedTxIpv4());

  const std::scoped_lock lock(mModelMutex);
  RefreshOutputBackendFromProjectLocked();
  SyncSnapshotToAtomicsLocked();
  return true;
}

std::string AeylaVisualDmx::SelectedRxIpv4() const
{
  const std::scoped_lock lock(mNetworkMutex);
  if(mRxInterfaceIndex >= mNetworkInterfaces.size())
    return {};
  return mNetworkInterfaces[mRxInterfaceIndex].ipv4;
}

std::string AeylaVisualDmx::SelectedTxIpv4() const
{
  const std::scoped_lock lock(mNetworkMutex);
  if(mTxInterfaceIndex >= mNetworkInterfaces.size())
    return {};
  return mNetworkInterfaces[mTxInterfaceIndex].ipv4;
}

std::string AeylaVisualDmx::RxInterfaceStatus() const
{
  const std::scoped_lock lock(mNetworkMutex);
  if(mRxInterfaceIndex >= mNetworkInterfaces.size())
    return "RX · SIN ADAPTADOR";
  const auto& item = mNetworkInterfaces[mRxInterfaceIndex];
  return "RX · " + item.name + " · " + item.ipv4 + "/" +
         std::to_string(item.prefix_length);
}

std::string AeylaVisualDmx::TxInterfaceStatus() const
{
  const std::scoped_lock lock(mNetworkMutex);
  if(mTxInterfaceIndex >= mNetworkInterfaces.size())
    return "TX · SIN ADAPTADOR";
  const auto& item = mNetworkInterfaces[mTxInterfaceIndex];
  return "TX · " + item.name + " · " + item.ipv4 + "/" +
         std::to_string(item.prefix_length);
}

void AeylaVisualDmx::RestartCaptureInputFromRouting()
{
  mArtNetCapture.stop();
  const std::string listen = SelectedRxIpv4();
  if(listen.empty())
  {
    const std::scoped_lock networkLock(mNetworkMutex);
    mCaptureInputError = "Selecciona un adaptador de red RX";
    return;
  }

  std::uint16_t universe = 0U;
  {
    const std::scoped_lock modelLock(mModelMutex);
    universe = mModel.project_document().output.universe;
  }

  aeyla::capture::ArtNetCaptureConfig config;
  config.listen_ipv4 = listen;
  config.port_address = universe;
  config.frames_per_second = 44U;
  std::string error;
  const bool started = mArtNetCapture.start(config, error);
  const std::scoped_lock networkLock(mNetworkMutex);
  mCaptureInputError = started ? std::string{} : std::move(error);
}

std::string AeylaVisualDmx::ActiveSongIdLocked() const
{
  return mModel.snapshot().active_song_id;
}

aeyla::product::AuthoringResult AeylaVisualDmx::ToggleTakeCaptureFromUI()
{
  std::string projectId;
  std::string songId;
  std::string songName;
  std::uint16_t universe = 0U;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
    songName = snapshot.active_song_name;
    universe = mModel.project_document().output.universe;
  }
  aeyla::take_library_session::ensure_scope(this, projectId);

  // STOP de una captura R07: el escritor finaliza el archivo en disco. Nunca
  // reconstruimos la toma completa en RAM.
  if(mArtNetCapture.streamed_recording_active())
  {
    std::string error;
    if(!mArtNetCapture.end_streamed_recording(error))
    {
      aeyla::take_library_session::set_storage_message(
          this, "ERROR DE GRABACIÓN · " + error);
      return {false, {}, "No fue posible cerrar la toma DMX · " + error};
    }

    const auto library = aeyla::take_library_session::directory(this);
    auto scan = aeyla::capture::scan_take_directory(library, songId);
    if(!scan.ok() || scan.entries.empty())
      return {false, {}, "La grabación terminó, pero no se pudo indexar el archivo final"};

    const auto& newest = scan.entries.front();
    aeyla::take_library_session::set_loaded_path(this, songId, newest.path);
    aeyla::take_library_session::set_storage_message(
        this, "GUARDADA EN DISCO · " + newest.path.filename().string());
    const double duration = newest.frames_per_second == 0U
        ? 0.0
        : static_cast<double>(newest.frame_count) /
              static_cast<double>(newest.frames_per_second);
    return {true, newest.take_name,
            newest.take_name + " guardada · " + FormatDuration(duration) +
                " · 44 Hz · RAM acotada"};
  }

  if(mArtNetCapture.stats().recording)
  {
    mArtNetCapture.discard_recording();
    return {false, {}, "Se descartó una captura heredada no compatible con R07"};
  }

  if(OutputArmed() || TakeOutputArmed())
    return {false, {}, "Desarma la salida física antes de grabar desde Avolites"};
  if(TakePlaying())
    return {false, {}, "Pausa o detén el clip DMX antes de grabar otra toma"};
  if(songId.empty())
    return {false, {}, "Crea o selecciona una canción antes de grabar"};

  auto library = aeyla::take_library_session::directory(this);
  std::string libraryError;
  if(!library.empty() &&
     !aeyla::capture::prepare_take_directory(library, libraryError))
  {
    aeyla::take_library_session::set_storage_message(
        this, "BIBLIOTECA SIN CONEXIÓN · " + libraryError);
    library.clear();
  }
  if(library.empty())
  {
    const auto selected = PromptTakeLibraryDirectory(GetUI());
    if(!selected.has_value())
      return {false, {}, "Grabación cancelada · selecciona una carpeta para la biblioteca de tomas"};
    library = *selected;
    if(!aeyla::capture::prepare_take_directory(library, libraryError))
      return {false, {}, "La biblioteca de tomas no permite escritura · " + libraryError};
    aeyla::take_library_session::set_directory(this, library);
  }

  auto stats = mArtNetCapture.stats();
  if(!stats.running)
  {
    RestartCaptureInputFromRouting();
    stats = mArtNetCapture.stats();
    if(!stats.running)
      return {false, {}, "No fue posible iniciar Art-Net RX · " + CaptureInputStatus()};
  }
  if(!stats.signal_present || stats.source_ipv4.empty())
    return {false, {},
            "RX está escuchando, pero aún no existe una señal Art-Net válida. "
            "Activa la salida continua de Art-Net en Avolites y vuelve a presionar GRABAR."};

  const auto scan = aeyla::capture::scan_take_directory(library, songId);
  const std::size_t nextNumber = scan.ok() ? scan.entries.size() + 1U : 1U;
  const std::string takeName = "Toma " + std::to_string(nextNumber);
  const auto target = aeyla::capture::make_take_file_path(
      library, songName.empty() ? songId : songName, takeName);

  aeyla::capture::DmxTakeStreamConfig stream;
  stream.target_path = target;
  stream.song_id = songId;
  stream.song_name = songName;
  stream.take_name = takeName;
  stream.source_ipv4 = stats.source_ipv4;
  stream.port_address = universe;
  stream.frames_per_second = 44U;

  std::string error;
  if(!mArtNetCapture.begin_streamed_recording(stream, error))
    return {false, {}, "No fue posible iniciar la grabación directa a disco · " + error};

  aeyla::take_library_session::set_storage_message(
      this, "GRABANDO EN DISCO · " + target.filename().string());
  return {true, takeName,
          "Grabando DMX desde " + stats.source_ipv4 + " · 44 Hz · RAM acotada"};
}

bool AeylaVisualDmx::TakeRecording() const noexcept
{
  return mArtNetCapture.stats().recording;
}

bool AeylaVisualDmx::TakePlaying() const noexcept
{
  return mTakeScheduler.status().playing;
}

bool AeylaVisualDmx::TakeOutputArmed() const noexcept
{
  return mTakeScheduler.status().armed;
}

bool AeylaVisualDmx::HasActiveTake() const
{
  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  if(songId.empty()) return false;
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  if(library.empty()) return false;
  const auto scan = aeyla::capture::scan_take_directory(library, songId);
  return scan.ok() && !scan.entries.empty();
}

aeyla::product::AuthoringResult AeylaVisualDmx::ToggleActiveTakePlaybackFromUI()
{
  if(TakeRecording())
    return {false, {}, "Detén la grabación antes de reproducir una toma"};

  const auto current = mTakeScheduler.status();
  if(current.playing)
  {
    std::string error;
    if(!mTakeScheduler.pause(error))
      return {false, {}, error};
    return {true, {}, "PAUSA · se mantiene el último estado DMX"};
  }
  if(current.paused)
  {
    std::string error;
    if(!mTakeScheduler.resume(error))
      return {false, {}, error};
    return {true, {}, "REANUDAR · continúa desde el mismo punto"};
  }

  std::string projectId;
  std::string songId;
  std::string songName;
  std::uint16_t outputUniverse = 0U;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
    songName = snapshot.active_song_name;
    outputUniverse = mModel.project_document().output.universe;
  }
  if(songId.empty())
    return {false, {}, "No hay una canción seleccionada"};

  aeyla::take_library_session::ensure_scope(this, projectId);
  auto library = aeyla::take_library_session::directory(this);
  if(library.empty())
  {
    const auto selected = PromptTakeLibraryDirectory(GetUI());
    if(!selected.has_value())
      return {false, {}, "Selecciona la carpeta que contiene las tomas DMX"};
    library = *selected;
    std::string directoryError;
    if(!aeyla::capture::prepare_take_directory(library, directoryError))
      return {false, {}, "La biblioteca de tomas no está disponible · " + directoryError};
    aeyla::take_library_session::set_directory(this, library);
  }

  std::string findError;
  const auto newest = NewestTake(library, songId, findError);
  if(!newest.has_value())
    return {false, {}, findError};
  if(newest->port_address != outputUniverse)
    return {false, {}, "El universo de la toma no coincide con la salida del proyecto"};

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  std::string error;
  if(!mTakeScheduler.load_take_file(newest->path, GetSampleRate(), error))
    return {false, {}, "La toma no superó la validación · " + error};
  aeyla::take_library_session::set_loaded_path(this, songId, newest->path);
  aeyla::take_library_session::set_storage_message(
      this, "CARGADA DESDE DISCO · " + newest->path.filename().string());

  if(!mTakeScheduler.play(error))
    return {false, {}, error};

  const double duration = newest->frames_per_second == 0U
      ? 0.0
      : static_cast<double>(newest->frame_count) /
            static_cast<double>(newest->frames_per_second);
  return {true, newest->take_name,
          "Reproduciendo " + newest->take_name + " · " + FormatDuration(duration) +
              " · cursor relativo por muestras"};
}

void AeylaVisualDmx::StopActiveTakePlaybackFromUI()
{
  // En el control actual DETENER conserva el estado como PAUSA/HOLD. El RESET
  // explícito se añadirá como control separado para evitar dobles significados.
  mTakeScheduler.stop_hold();
}

aeyla::product::AuthoringResult AeylaVisualDmx::ToggleTakeOutputArmFromUI()
{
  if(TakeOutputArmed())
  {
    mTakeScheduler.disarm();
    return {true, {}, "SALIDA DE TOMA DESARMADA"};
  }
  if(OutputArmed())
    return {false, {}, "Desarma la salida del modelo antes de armar la toma DMX"};
  if(TakeRecording())
    return {false, {}, "Detén la captura desde Avolites antes de armar la salida"};
  if(!ProjectValid())
    return {false, {}, "El proyecto no es válido"};
  if(!BackendReady())
    return {false, {}, "Configura primero un destino Art-Net válido"};
  if(!RuntimeHealthy() || RenderingOffline())
    return {false, {}, "La protección del runtime bloquea la salida física"};
  if(EffectiveBlackout())
    return {false, {}, "Desactiva BLACKOUT antes de armar la salida"};

  std::string projectId;
  std::string songId;
  std::uint16_t outputUniverse = 0U;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
    outputUniverse = mModel.project_document().output.universe;
  }
  if(songId.empty())
    return {false, {}, "No hay una canción seleccionada"};

  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  std::string findError;
  const auto newest = NewestTake(library, songId, findError);
  if(!newest.has_value())
    return {false, {}, findError};
  if(newest->port_address != outputUniverse)
    return {false, {}, "El universo de la toma no coincide con la salida del proyecto"};

  const auto loaded = aeyla::take_library_session::loaded_path(this, songId);
  if(loaded != newest->path || !mTakeScheduler.status().file_backed)
  {
    std::string loadError;
    mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
    if(!mTakeScheduler.load_take_file(newest->path, GetSampleRate(), loadError))
      return {false, {}, "No fue posible preparar la toma desde disco · " + loadError};
    aeyla::take_library_session::set_loaded_path(this, songId, newest->path);
  }

  std::string error;
  if(!mTakeScheduler.arm(error))
    return {false, {}, error};
  return {true, newest->take_name,
          "SALIDA DMX ARMADA · " + TxInterfaceStatus()};
}

std::string AeylaVisualDmx::ActiveTakeStatus() const
{
  const auto captureStats = mArtNetCapture.stats();
  if(captureStats.recording)
  {
    std::string result = "GRABANDO · " +
        std::to_string(captureStats.recorded_frames) + " CUADROS";
    if(captureStats.streaming_to_disk)
      result += " · DISCO";
    if(captureStats.storage_failed)
      result += " · ERROR DE ALMACENAMIENTO";
    return result;
  }

  std::string projectId;
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
  }
  if(songId.empty()) return "SIN CANCIÓN";
  aeyla::take_library_session::ensure_scope(this, projectId);
  const auto library = aeyla::take_library_session::directory(this);
  if(library.empty()) return "SIN BIBLIOTECA DE TOMAS";

  const auto scan = aeyla::capture::scan_take_directory(library, songId);
  if(!scan.ok()) return "ERROR DE BIBLIOTECA · " + scan.error;
  if(scan.entries.empty()) return "SIN TOMA · GRABA UNA TOMA DMX";

  const auto& take = scan.entries.front();
  const double duration = take.frames_per_second == 0U
      ? 0.0
      : static_cast<double>(take.frame_count) /
            static_cast<double>(take.frames_per_second);
  const auto scheduler = mTakeScheduler.status();
  std::string status = take.take_name + " · " + FormatDuration(duration) +
                       " · " + std::to_string(take.frames_per_second) + " Hz · DISCO";
  if(scheduler.playing)
    status += " · REPRODUCIENDO";
  else if(scheduler.paused)
    status += " · PAUSA";
  else if(scheduler.ended)
    status += " · FINAL / HOLD";
  if(scheduler.armed)
    status += " · AL AIRE";
  if(!scheduler.error.empty())
    status += " · ERROR";
  return status;
}

std::string AeylaVisualDmx::CaptureInputStatus() const
{
  const auto stats = mArtNetCapture.stats();
  if(!stats.running)
  {
    const std::scoped_lock networkLock(mNetworkMutex);
    return mCaptureInputError.empty() ? "RX APAGADO" : "ERROR RX · " + mCaptureInputError;
  }
  if(stats.storage_failed)
    return "ERROR DE ALMACENAMIENTO · " + stats.storage_error;
  if(stats.signal_present)
    return "RX ACTIVO · " + stats.source_ipv4 + " · U" +
           std::to_string(stats.port_address) + " · " +
           std::to_string(stats.packets_accepted) + " PAQUETES";
  return "RX LISTO · ESPERANDO ART-NET · U" + std::to_string(stats.port_address);
}

double AeylaVisualDmx::ActiveTakePlaybackProgress() const
{
  return mTakeScheduler.status().progress;
}

std::uint64_t AeylaVisualDmx::CaptureAcceptedPackets() const noexcept
{
  return mArtNetCapture.stats().packets_accepted;
}

std::uint64_t AeylaVisualDmx::CaptureSequenceGaps() const noexcept
{
  return mArtNetCapture.stats().sequence_gaps;
}
