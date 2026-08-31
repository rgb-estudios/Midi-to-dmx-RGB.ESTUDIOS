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
  const auto totalMs = static_cast<std::uint64_t>(
      std::llround(seconds * 1000.0));
  const auto minutes = totalMs / 60000U;
  const auto secondsPart = (totalMs / 1000U) % 60U;
  const auto milliseconds = totalMs % 1000U;
  std::ostringstream stream;
  stream << minutes << ':' << std::setw(2) << std::setfill('0')
         << secondsPart << '.' << std::setw(3) << milliseconds;
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
    if(found != scan.entries.end() &&
       found->frame_count == edited->frame_count &&
       found->frames_per_second == edited->frames_per_second)
      return *found;

    // An external delete/replace cannot leave the editor pointing at a file
    // that the validated library index no longer contains.
    aeyla::take_library_session::clear_edit_state(owner, songId);
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
  if(TakeOutputArmed())
    return {false, {}, "Desarma la salida antes de renombrar una canción"};

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
  GetParam(kParamBlackout)->Set(enabled ? 1.0 : 0.0);
  mParamBlackout.store(enabled, std::memory_order_release);

  const std::scoped_lock lock(mModelMutex);
  if(enabled)
    mModel.release_transients();
  mModel.set_blackout(enabled);
  SyncSnapshotToAtomicsLocked();

  // R10.5: APAGÓN TOTAL is a physical mask, not DISARM. The single Art-Net
  // worker keeps its lease/carrier and transmits zero at 44 Hz above Take and
  // EN VIVO. Releasing APAGÓN reveals the underlying current state without a
  // second ARM action.
  mArtNetOutput.set_blackout_latched(enabled);
  PublishOutputFrameLocked(
      mRenderingOffline.load(std::memory_order_acquire));
}

bool AeylaVisualDmx::SelectSongFromUI(std::size_t songIndex)
{
  if(TakeRecording())
    return false;

  // Selecting PREPARADA is metadata/navigation only. It never owns physical
  // Art-Net authority and therefore may not disarm or latch blackout.
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
  mMidiPreloadSongRequest.store(static_cast<int>(songIndex),
                                std::memory_order_release);
  SetShowMidiMessage("PREPARADA · " + mModel.snapshot().active_song_name +
                     " · PLAY decide cuándo reemplaza la canción al aire");
  mLastProjectedSongId.clear();
  mLastProjectedTick = 0U;
  SyncSnapshotToAtomicsLocked();
  return true;
}

bool AeylaVisualDmx::RefreshNetworkInterfacesFromUI()
{
  if(NetworkConfigurationBusy() || TakeRecording() ||
     TakeOutputArmed() || OutputArmed())
    return false;
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
  if(direction == 0 || NetworkConfigurationBusy() || TakeRecording())
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
  if(direction == 0 || NetworkConfigurationBusy() || TakeRecording())
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

std::optional<aeyla::network::NetworkInterface>
AeylaVisualDmx::SelectedTxInterface() const
{
  const std::scoped_lock lock(mNetworkMutex);
  if(mTxInterfaceIndex >= mNetworkInterfaces.size())
    return std::nullopt;
  return mNetworkInterfaces[mTxInterfaceIndex];
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
  if(NetworkConfigurationBusy())
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
  const auto operation = mNetworkConfiguration.Snapshot();
  return operation.busy() ||
         operation.revision != mLastNetworkConfigurationRevision.load(
             std::memory_order_acquire);
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
    const auto expectedTarget = mActiveCaptureTarget;
    std::string error;
    if(!mArtNetCapture.end_streamed_recording(error))
    {
      mActiveCaptureTarget.clear();
      mCaptureSyncAnchor.reset();
      aeyla::take_library_session::set_storage_message(
          this, "ERROR DE GRABACIÓN · " + error);
      return {false, {}, "No fue posible cerrar la toma DMX · " + error};
    }
    mActiveCaptureTarget.clear();

    const auto library = aeyla::take_library_session::directory(this);
    auto scan = aeyla::capture::scan_take_directory(library, songId);
    const auto captured = aeyla::capture::find_take_entry_by_path(
        scan, expectedTarget);
    if(!scan.ok() || !captured.has_value())
    {
      mCaptureSyncAnchor.reset();
      const std::string detail = scan.ok()
          ? "el archivo exacto no apareció en el índice"
          : scan.error;
      return {false, {},
              "La grabación terminó, pero no se pudo verificar su archivo final · " +
                  detail};
    }

    const auto& newest = *captured;
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
        syncMessage = " · ENTRADA AUTO " + FormatDuration(inSeconds) +
                      " · REPRODUCIR / MTC";
      }
      else
      {
        aeyla::take_library_session::clear_edit_state(this, songId);
        aeyla::take_library_session::set_loaded_path(this, songId, newest.path);
        syncMessage = " · ENTRADA AUTO NO DISPONIBLE · " + error;
      }
    }
    else
    {
      aeyla::take_library_session::clear_edit_state(this, songId);
      aeyla::take_library_session::set_loaded_path(this, songId, newest.path);
      syncMessage = " · SIN ANCLA REPRODUCIR / MTC · AJUSTA ENTRADA MANUALMENTE";
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
    mActiveCaptureTarget.clear();
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
    if(ShowMidiMapping().enabled)
      mMidiPreflightCursor.store(0, std::memory_order_release);
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
    mActiveCaptureTarget.clear();
    mCaptureSyncAnchor.reset();
    return {false, {}, "No fue posible iniciar la grabación directa a disco · " + error};
  }
  mActiveCaptureTarget = target;

  const auto sync = mCaptureSyncAnchor.status();
  const std::string syncMessage =
      sync.state == aeyla::capture::DmxCaptureSyncState::waiting_for_transport
          ? " · ESPERANDO REPRODUCIR / MTC"
          : " · SIN ANCLA AUTOMÁTICA · INICIA GRABACIÓN CON REAPER DETENIDO";

  aeyla::take_library_session::set_storage_message(
      this, "GRABANDO EN DISCO · " + target.filename().string() + syncMessage);
  return {true, takeName,
          "Grabando DMX desde " + stats.source_ipv4 +
              " · 44 Hz · RAM acotada" + syncMessage};
}

bool AeylaVisualDmx::TakeRecording() const noexcept
{
  const auto stats = mArtNetCapture.stats();
  return stats.recording || stats.streaming_to_disk;
}

bool AeylaVisualDmx::TakePlaying() const noexcept
{
  return mTakeScheduler.status().playing;
}

bool AeylaVisualDmx::TakeOutputArmed() const noexcept
{
  return mTakeScheduler.status().armed;
}

aeyla::product::AuthoringResult AeylaVisualDmx::ToggleActiveTakePlaybackFromUI()
{
  if(TakeRecording())
    return {false, {}, "Detén la grabación antes de reproducir una toma"};

  const auto current = mTakeScheduler.status();
  const std::size_t preparedSongIndex = ActiveSongIndex();
  const int activeLiveSongIndex = mActiveTakeSongIndex.load(
      std::memory_order_acquire);
  const bool preparedIsCurrentLiveSong =
      activeLiveSongIndex >= 0 &&
      static_cast<std::size_t>(activeLiveSongIndex) == preparedSongIndex;

  // PLAY/PAUSA only toggles transport when the prepared Song is already the
  // Song AL AIRE. If another Song is PREPARADA, this button is a GO and must
  // atomically replace the live reader without withdrawing Art-Net authority.
  if(current.playing && preparedIsCurrentLiveSong)
  {
    std::string error;
    if(!mTakeScheduler.pause(error))
      return {false, {}, error};
    return {true, {}, "PAUSA · se mantiene el último estado DMX"};
  }
  if(current.paused && preparedIsCurrentLiveSong)
  {
    std::string error;
    if(!mTakeScheduler.resume(error))
      return {false, {}, error};
    return {true, {}, "REANUDAR · continúa desde el mismo punto"};
  }

  std::string projectId;
  std::string songId;
  std::size_t songIndex = 0U;
  std::uint16_t outputUniverse = 0U;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
    songIndex = snapshot.active_song_index;
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
    if(ShowMidiMapping().enabled)
      mMidiPreflightCursor.store(0, std::memory_order_release);
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
  const auto edited = aeyla::take_library_session::edit_state(this, songId);
  const auto scheduler = mTakeScheduler.status();
  const std::uint64_t expectedStart =
      edited.has_value() && edited->path == selected.path
          ? edited->start_frame : 0U;
  const std::uint64_t expectedEnd =
      edited.has_value() && edited->path == selected.path
          ? edited->end_frame_exclusive : selected.frame_count;
  const bool sameValidatedClip =
      mLoadedTakeSongIndex.load(std::memory_order_acquire) ==
          static_cast<int>(songIndex) &&
      loaded == selected.path && scheduler.file_backed &&
      scheduler.range_start_frame == expectedStart &&
      scheduler.range_end_frame_exclusive == expectedEnd;
  bool startedByAtomicReplace = false;
  if(!sameValidatedClip)
  {
    mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
    if(scheduler.armed)
    {
      // Validate the prepared file beside the live reader. The old Song keeps
      // its held frame/carrier until replace_armed_take_file() commits one
      // reader swap. A validation/swap failure leaves the previous Song live.
      aeyla::capture::DmxTakeFileReader candidate;
      if(!candidate.open(selected.path, error))
        return {false, {}, "GO cancelado · la canción anterior sigue AL AIRE · " + error};
      const auto info = candidate.info();
      if(!info.open || info.song_id != songId ||
         info.port_address != outputUniverse ||
         expectedStart >= expectedEnd || expectedEnd > info.frame_count)
        return {false, {},
                "GO cancelado · la toma preparada no coincide con canción, universo o ENTRADA/SALIDA · la anterior sigue AL AIRE"};

      if(!mTakeScheduler.replace_armed_take_file(
             candidate, GetSampleRate(),
             static_cast<std::size_t>(expectedStart),
             static_cast<std::size_t>(expectedEnd),
             aeyla::capture::DmxClipClockSource::monotonic_realtime,
             0U, error))
        return {false, {},
                "GO cancelado · la canción anterior sigue AL AIRE · " + error};
      startedByAtomicReplace = true;
    }
    else
    {
      if(!mTakeScheduler.load_take_file(selected.path, GetSampleRate(), error))
        return {false, {}, "La toma no superó la validación · " + error};
      if(edited.has_value() && edited->path == selected.path &&
         !mTakeScheduler.set_play_range(
             static_cast<std::size_t>(edited->start_frame),
             static_cast<std::size_t>(edited->end_frame_exclusive), error))
        return {false, {}, "El rango ENTRADA / SALIDA no pudo cargarse · " + error};
    }
    aeyla::take_library_session::set_loaded_path(this, songId, selected.path);
    mLoadedTakeSongIndex.store(static_cast<int>(songIndex),
                               std::memory_order_release);
    aeyla::take_library_session::set_storage_message(
        this, startedByAtomicReplace
            ? "GO SIN CORTE · " + selected.path.filename().string()
            : "CARGADA DESDE DISCO · " + selected.path.filename().string());
  }

  // El botón manual debe continuar aunque REAPER cierre su dispositivo de
  // audio al perder foco. Los futuros disparos DAW/MIDI conservarán el reloj
  // por muestras del host mediante DmxClipClockSource::host_samples.
  if(!startedByAtomicReplace &&
     !mTakeScheduler.play(
         error, aeyla::capture::DmxClipClockSource::monotonic_realtime))
    return {false, {}, error};
  mActiveTakeSongIndex.store(static_cast<int>(songIndex),
                             std::memory_order_release);

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
  if(ShowMidiMapping().enabled &&
     mMidiPreflightCursor.load(std::memory_order_acquire) >= 0)
    return {false, {},
            "Espera a que MIDI / SHOW indique PRECARGA COMPLETA antes de armar"};
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
  std::size_t songIndex = 0U;
  std::uint16_t outputUniverse = 0U;
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
    songIndex = snapshot.active_song_index;
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
  const auto edited = aeyla::take_library_session::edit_state(this, songId);
  const auto scheduler = mTakeScheduler.status();
  const std::uint64_t expectedStart =
      edited.has_value() && edited->path == selected.path
          ? edited->start_frame : 0U;
  const std::uint64_t expectedEnd =
      edited.has_value() && edited->path == selected.path
          ? edited->end_frame_exclusive : selected.frame_count;
  const bool sameValidatedClip =
      mLoadedTakeSongIndex.load(std::memory_order_acquire) ==
          static_cast<int>(songIndex) &&
      loaded == selected.path && scheduler.file_backed &&
      scheduler.range_start_frame == expectedStart &&
      scheduler.range_end_frame_exclusive == expectedEnd;
  if(!sameValidatedClip)
  {
    std::string loadError;
    mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
    if(!mTakeScheduler.load_take_file(selected.path, GetSampleRate(), loadError))
      return {false, {}, "No fue posible preparar la toma desde disco · " + loadError};
    if(edited.has_value() && edited->path == selected.path &&
       !mTakeScheduler.set_play_range(
           static_cast<std::size_t>(edited->start_frame),
           static_cast<std::size_t>(edited->end_frame_exclusive), loadError))
      return {false, {}, "No fue posible aplicar ENTRADA / SALIDA · " + loadError};
    aeyla::take_library_session::set_loaded_path(this, songId, selected.path);
    mLoadedTakeSongIndex.store(static_cast<int>(songIndex),
                               std::memory_order_release);
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
  if(captureStats.recording || captureStats.streaming_to_disk)
  {
    std::string result = captureStats.recording
        ? "GRABANDO · "
        : "CAPTURA DETENIDA POR ERROR · ";
    result +=
        std::to_string(captureStats.recorded_frames) + " CUADROS";
    if(captureStats.streaming_to_disk)
      result += " · DISCO";
    if(captureStats.storage_failed)
      result += " · ERROR DE ALMACENAMIENTO";
    const auto sync = mCaptureSyncAnchor.status();
    if(sync.state == aeyla::capture::DmxCaptureSyncState::waiting_for_transport)
      result += " · ESPERANDO REPRODUCIR / MTC";
    else if(sync.state == aeyla::capture::DmxCaptureSyncState::anchored)
      result += " · SINCRONÍA FIJADA · ENTRADA AUTO " +
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

  const auto take = ActiveTakeEditorSnapshot();
  if(!take.available) return "SIN TOMA · GRABA UNA TOMA DMX";
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
           std::to_string(static_cast<unsigned>(stats.port_address) + 1U) + " · " +
           std::to_string(stats.packets_accepted) + " PAQUETES";
  return "RX LISTO · ESPERANDO ART-NET · U" +
         std::to_string(static_cast<unsigned>(stats.port_address) + 1U);
}

double AeylaVisualDmx::ActiveTakePlaybackProgress() const
{
  return mTakeScheduler.status().progress;
}
