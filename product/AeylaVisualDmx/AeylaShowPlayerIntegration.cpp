#include "AeylaVisualDmx.h"
#include "AeylaTakeLibrarySession.h"
#include "capture/dmx_take_file_store.h"
#include "network/ipv4_configuration.h"

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

const aeyla::capture::TakeFileIndexEntry& SelectedTake(
    const void* owner,
    std::string_view songId,
    const aeyla::capture::TakeLibraryScanResult& scan)
{
  const auto edited = aeyla::take_library_session::edit_state(owner, songId);
  if(edited.has_value()) {
    const auto found = std::find_if(
        scan.entries.begin(), scan.entries.end(),
        [&](const auto& entry) { return entry.path == edited->path; });
    if(found != scan.entries.end()) return *found;
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
  std::string previousRxId;
  std::string previousTxId;
  std::string previousRx;
  std::string previousTx;
  {
    const std::scoped_lock lock(mNetworkMutex);
    if(mRxInterfaceIndex < mNetworkInterfaces.size()) {
      previousRxId = mNetworkInterfaces[mRxInterfaceIndex].id;
      previousRx = mNetworkInterfaces[mRxInterfaceIndex].ipv4;
    }
    if(mTxInterfaceIndex < mNetworkInterfaces.size()) {
      previousTxId = mNetworkInterfaces[mTxInterfaceIndex].id;
      previousTx = mNetworkInterfaces[mTxInterfaceIndex].ipv4;
    }

    mNetworkInterfaces = discovered;
    const auto preferred = std::find_if(
        mNetworkInterfaces.begin(), mNetworkInterfaces.end(),
        [](const aeyla::network::NetworkInterface& item) {
          return !item.wireless && !item.ipv4.empty();
        });
    const std::size_t preferredIndex = preferred == mNetworkInterfaces.end()
        ? 0U
        : static_cast<std::size_t>(
              std::distance(mNetworkInterfaces.begin(), preferred));
    mRxInterfaceIndex = preferredIndex;
    mTxInterfaceIndex = preferredIndex;

    const auto restore = [&](const std::string& id,
                             const std::string& address,
                             std::size_t fallback) -> std::size_t {
      if(id.empty()) return fallback;
      const auto found = std::find_if(
          mNetworkInterfaces.begin(), mNetworkInterfaces.end(),
          [&](const aeyla::network::NetworkInterface& item) {
            return item.id == id &&
                   (address.empty() || item.ipv4 == address);
          });
      if(found != mNetworkInterfaces.end())
        return static_cast<std::size_t>(
            std::distance(mNetworkInterfaces.begin(), found));
      const auto sameAdapter = std::find_if(
          mNetworkInterfaces.begin(), mNetworkInterfaces.end(),
          [&](const aeyla::network::NetworkInterface& item) {
            return item.id == id;
          });
      return sameAdapter == mNetworkInterfaces.end()
          ? fallback
          : static_cast<std::size_t>(
                std::distance(mNetworkInterfaces.begin(), sameAdapter));
    };
    mRxInterfaceIndex = restore(previousRxId, previousRx, mRxInterfaceIndex);
    mTxInterfaceIndex = restore(previousTxId, previousTx, mTxInterfaceIndex);
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
  if(direction == 0 || mNetworkConfiguration.Snapshot().busy())
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
  return "RX · " + item.name + " · " +
         (item.ipv4.empty()
              ? std::string("SIN IPv4")
              : item.ipv4 + "/" + std::to_string(item.prefix_length));
}

std::string AeylaVisualDmx::TxInterfaceStatus() const
{
  const std::scoped_lock lock(mNetworkMutex);
  if(mTxInterfaceIndex >= mNetworkInterfaces.size())
    return "TX · SIN ADAPTADOR";
  const auto& item = mNetworkInterfaces[mTxInterfaceIndex];
  return "TX · " + item.name + " · " +
         (item.ipv4.empty()
              ? std::string("SIN IPv4")
              : item.ipv4 + "/" + std::to_string(item.prefix_length));
}

aeyla::product::AuthoringResult AeylaVisualDmx::ApplyTxNetworkFromUI(
    std::string ipv4,
    std::string mask)
{
  std::string error;
  const auto network = aeyla::network::make_ipv4_network(ipv4, mask, error);
  if(!network.has_value())
    return {false, {}, std::move(error)};
  if(TakeRecording())
    return {false, {}, "Detén GRABAR antes de cambiar la red TX"};
  if(mNetworkConfiguration.Snapshot().busy())
    return {false, {}, "Espera a que termine el cambio de red actual"};

  aeyla::network::NetworkInterface adapter;
  {
    const std::scoped_lock lock(mNetworkMutex);
    if(mTxInterfaceIndex >= mNetworkInterfaces.size())
      return {false, {}, "Selecciona primero un adaptador TX físico"};
    adapter = mNetworkInterfaces[mTxInterfaceIndex];
  }
  if(adapter.wireless)
    return {false, {},
            "La red de show debe usar Ethernet; selecciona un adaptador cableado"};

  // A network change can never hot-swap below an authoritative clip.
  mTakeScheduler.stop_hold();
  mTakeScheduler.disarm();
  SetBlackoutFromUI(true);

  if(adapter.ipv4 == network->address &&
     adapter.prefix_length == network->prefix_length)
  {
    mArtNetOutput.set_preferred_source_ipv4(network->address);
    auto result = ConfigureArtNetFromUI(network->directed_broadcast + "@0");
    if(result.succeeded)
    {
      const std::scoped_lock lock(mNetworkMutex);
      mNetworkConfigurationMessage = "RED LISTA · " + network->address +
          "/" + std::to_string(network->prefix_length) + " → " +
          network->directed_broadcast + " · U1 · SALIDA DESARMADA";
      result.message = mNetworkConfigurationMessage;
    }
    return result;
  }

  {
    const std::scoped_lock lock(mNetworkMutex);
    mPendingTxAdapterId = adapter.id;
    mNetworkConfigurationMessage =
        "CAMBIO EN CURSO · confirma la solicitud UAC de Windows";
  }
  if(!mNetworkConfiguration.Start(adapter, network->address,
                                  network->prefix_length, error))
  {
    const std::scoped_lock lock(mNetworkMutex);
    mPendingTxAdapterId.clear();
    mNetworkConfigurationMessage = "CAMBIO NO INICIADO · " + error;
    return {false, {}, std::move(error)};
  }
  return {true, network->address,
          "CAMBIO EN CURSO · AEYLA agregará " + network->address + "/" +
              std::to_string(network->prefix_length) + " a " + adapter.name +
              " sin borrar su red existente"};
}

std::string AeylaVisualDmx::NetworkConfigurationStatus() const
{
  const auto operation = mNetworkConfiguration.Snapshot();
  if(operation.busy()) return operation.message;
  const std::scoped_lock lock(mNetworkMutex);
  return mNetworkConfigurationMessage.empty()
      ? std::string("SIN CAMBIOS DE RED PENDIENTES")
      : mNetworkConfigurationMessage;
}

bool AeylaVisualDmx::NetworkConfigurationBusy() const
{
  return mNetworkConfiguration.Snapshot().busy();
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
  if(NetworkConfigurationBusy())
    return {false, {}, "Espera a que termine el cambio de red antes de grabar"};

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
      mCaptureSyncAnchor.reset();
      aeyla::take_library_session::set_storage_message(
          this, "ERROR DE GRABACIÓN · " + error);
      return {false, {}, "No fue posible cerrar la toma DMX · " + error};
    }

    const auto library = aeyla::take_library_session::directory(this);
    auto scan = aeyla::capture::scan_take_directory(library, songId);
    if(!scan.ok() || scan.entries.empty())
    {
      mCaptureSyncAnchor.reset();
      return {false, {}, "La grabación terminó, pero no se pudo indexar el archivo final"};
    }

    const auto& newest = scan.entries.front();
    const auto automaticIn = mCaptureSyncAnchor.resolved_anchor(
        newest.frame_count);
    std::string syncMessage;
    if(automaticIn.has_value())
    {
      if(ApplyCapturedTakeAutoIn(newest, *automaticIn, error))
      {
        const double inSeconds = newest.frames_per_second == 0U
            ? 0.0
            : static_cast<double>(*automaticIn) /
                  static_cast<double>(newest.frames_per_second);
        syncMessage = " · IN AUTO " + FormatDuration(inSeconds) +
                      " · PLAY/MTC";
      }
      else
      {
        aeyla::take_library_session::clear_edit_state(this, songId);
        aeyla::take_library_session::set_loaded_path(this, songId, newest.path);
        syncMessage = " · IN AUTO NO DISPONIBLE · " + error;
      }
    }
    else
    {
      aeyla::take_library_session::clear_edit_state(this, songId);
      aeyla::take_library_session::set_loaded_path(this, songId, newest.path);
      syncMessage = " · SIN ANCLA PLAY/MTC · AJUSTA IN MANUALMENTE";
    }
    mCaptureSyncAnchor.reset();
    aeyla::take_library_session::set_storage_message(
        this, "GUARDADA EN DISCO · " + newest.path.filename().string() +
                  syncMessage);
    const double duration = newest.frames_per_second == 0U
        ? 0.0
        : static_cast<double>(newest.frame_count) /
              static_cast<double>(newest.frames_per_second);
    return {true, newest.take_name,
            newest.take_name + " guardada · " + FormatDuration(duration) +
                " · 44 Hz · RAM acotada" + syncMessage};
  }

  if(mArtNetCapture.stats().recording)
  {
    mArtNetCapture.discard_recording();
    mCaptureSyncAnchor.reset();
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

  // Armar el detector antes que el escritor evita perder un PLAY que llegue
  // exactamente mientras el operador pulsa GRABAR. RuntimeTick sólo observa
  // el flanco cuando la grabación a disco ya figura activa.
  mCaptureSyncAnchor.begin(mHostTransport.latest());
  std::string error;
  if(!mArtNetCapture.begin_streamed_recording(stream, error))
  {
    mCaptureSyncAnchor.reset();
    return {false, {}, "No fue posible iniciar la grabación directa a disco · " + error};
  }

  const auto sync = mCaptureSyncAnchor.status();
  const std::string syncMessage =
      sync.state == aeyla::capture::DmxCaptureSyncState::waiting_for_transport
          ? " · ESPERANDO PLAY/MTC"
          : " · SIN ANCLA AUTOMÁTICA · INICIA GRABACIÓN CON REAPER DETENIDO";

  aeyla::take_library_session::set_storage_message(
      this, "GRABANDO EN DISCO · " + target.filename().string() + syncMessage);
  return {true, takeName,
          "Grabando DMX desde " + stats.source_ipv4 +
              " · 44 Hz · RAM acotada" + syncMessage};
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

  const auto scan = aeyla::capture::scan_take_directory(library, songId);
  if(!scan.ok() || scan.entries.empty())
    return {false, {}, scan.ok()
        ? "No hay tomas DMX guardadas para esta canción" : scan.error};
  const auto& selected = SelectedTake(this, songId, scan);
  if(selected.port_address != outputUniverse)
    return {false, {}, "El universo de la toma no coincide con la salida del proyecto"};

  std::string error;
  const auto loaded = aeyla::take_library_session::loaded_path(this, songId);
  const bool sameValidatedClip =
      loaded == selected.path && mTakeScheduler.status().file_backed;
  if(!sameValidatedClip)
  {
    // Loading is intentionally destructive to output authority, so it is done
    // only when the selected file really changed. The normal operator order
    // ARM -> PLAY must preserve the arm established by the first action.
    mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
    if(!mTakeScheduler.load_take_file(selected.path, GetSampleRate(), error))
      return {false, {}, "La toma no superó la validación · " + error};
    const auto edited = aeyla::take_library_session::edit_state(this, songId);
    if(edited.has_value() && edited->path == selected.path &&
       !mTakeScheduler.set_play_range(
           static_cast<std::size_t>(edited->start_frame),
           static_cast<std::size_t>(edited->end_frame_exclusive), error))
      return {false, {}, "El rango ENTRADA / SALIDA no pudo cargarse · " + error};
    aeyla::take_library_session::set_loaded_path(this, songId, selected.path);
    aeyla::take_library_session::set_storage_message(
        this, "CARGADA DESDE DISCO · " + selected.path.filename().string());
  }

  // El botón manual debe continuar aunque REAPER cierre su dispositivo de
  // audio al perder foco. Los futuros disparos DAW/MIDI conservarán el reloj
  // por muestras del host mediante DmxClipClockSource::host_samples.
  if(!mTakeScheduler.play(
         error, aeyla::capture::DmxClipClockSource::monotonic_realtime))
    return {false, {}, error};

  const auto edited = aeyla::take_library_session::edit_state(this, songId);
  const std::uint64_t selectedFrames = edited.has_value() &&
          edited->path == selected.path
      ? edited->end_frame_exclusive - edited->start_frame
      : selected.frame_count;
  const double duration = selected.frames_per_second == 0U
      ? 0.0
      : static_cast<double>(selectedFrames) /
            static_cast<double>(selected.frames_per_second);
  const std::string authority = TakeOutputArmed()
      ? "REPRODUCIENDO AL AIRE"
      : "PREVIA SIN SALIDA FÍSICA";
  return {true, selected.take_name,
          authority + " · " + selected.take_name + " · " + FormatDuration(duration) +
              " · reloj operativo independiente"};
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
  if(NetworkConfigurationBusy())
    return {false, {}, "Espera a que termine el cambio de red antes de armar"};
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
  // A recorded Take is an independent output authority. A Song with no
  // resolved Cue may keep the Show renderer effectively black, but only the
  // global operator/safety latch is allowed to block Take arming.
  if(GlobalBlackout())
    return {false, {}, "Desactiva APAGÓN antes de armar la salida"};

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
  const auto scan = aeyla::capture::scan_take_directory(library, songId);
  if(!scan.ok() || scan.entries.empty())
    return {false, {}, scan.ok()
        ? "No hay tomas DMX guardadas para esta canción" : scan.error};
  const auto& selected = SelectedTake(this, songId, scan);
  if(selected.port_address != outputUniverse)
    return {false, {}, "El universo de la toma no coincide con la salida del proyecto"};

  const auto loaded = aeyla::take_library_session::loaded_path(this, songId);
  if(loaded != selected.path || !mTakeScheduler.status().file_backed)
  {
    std::string loadError;
    mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
    if(!mTakeScheduler.load_take_file(selected.path, GetSampleRate(), loadError))
      return {false, {}, "No fue posible preparar la toma desde disco · " + loadError};
    const auto edited = aeyla::take_library_session::edit_state(this, songId);
    if(edited.has_value() && edited->path == selected.path &&
       !mTakeScheduler.set_play_range(
           static_cast<std::size_t>(edited->start_frame),
           static_cast<std::size_t>(edited->end_frame_exclusive), loadError))
      return {false, {}, "No fue posible aplicar ENTRADA / SALIDA · " + loadError};
    aeyla::take_library_session::set_loaded_path(this, songId, selected.path);
  }

  std::string error;
  if(!mTakeScheduler.arm(error))
    return {false, {}, error};
  return {true, selected.take_name,
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
    const auto sync = mCaptureSyncAnchor.status();
    if(sync.state == aeyla::capture::DmxCaptureSyncState::waiting_for_transport)
      result += " · ESPERANDO PLAY/MTC";
    else if(sync.state == aeyla::capture::DmxCaptureSyncState::anchored)
      result += " · SINCRONÍA FIJADA · IN AUTO " +
          FormatDuration(static_cast<double>(sync.anchor_frame) / 44.0);
    else if(sync.state == aeyla::capture::DmxCaptureSyncState::unavailable)
      result += " · SIN ANCLA DAW";
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

  const auto& take = SelectedTake(this, songId, scan);
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
    status += " · FINAL / MANTENER";
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
