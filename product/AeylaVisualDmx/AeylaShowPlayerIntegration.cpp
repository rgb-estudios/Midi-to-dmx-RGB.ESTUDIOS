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
    return {false, {}, "Stop recording before renaming a Song"};
  if(TakePlaying())
    return {false, {}, "Stop Take playback before renaming a Song"};

  const std::string normalized = TrimOperatorText(name);
  if(normalized.empty())
    return {false, {}, "Song name cannot be empty"};
  if(normalized.size() > 64U)
    return {false, {}, "Song name must be 64 characters or fewer"};
  if(std::any_of(normalized.begin(), normalized.end(), [](char value) {
       const unsigned char byte = static_cast<unsigned char>(value);
       return byte < 0x20U && value != '\t';
     }))
    return {false, {}, "Song name contains unsupported control characters"};

  const std::scoped_lock lock(mModelMutex);
  auto program = mModel.show_program();
  if(songIndex >= program.songs.size())
    return {false, {}, "Song no longer exists"};
  if(program.songs[songIndex].name == normalized)
    return {true, program.songs[songIndex].song_id, normalized};

  const std::string songId = program.songs[songIndex].song_id;
  program.songs[songIndex].name = normalized;
  const auto validation = mModel.replace_show_program(program);
  if(!validation.ok())
    return {false, songId, "Renamed Song failed Show validation"};

  mParamBlackout.store(true, std::memory_order_release);
  mLastProjectedSongId.clear();
  mLastProjectedTick = 0U;
  SyncSnapshotToAtomicsLocked();
  return {true, songId, "Song renamed · " + normalized};
}

void AeylaVisualDmx::SetBlackoutFromUI(bool enabled)
{
  // BLACKOUT is one deterministic authority boundary. It never calls the
  // operator-facing ARM toggle, so it cannot recursively re-enter ARM/DISARM.
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
  StopActiveTakePlaybackFromUI();
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
  mParamBlackout.store(true, std::memory_order_release);
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
      mCaptureInputError = "No active IPv4 network adapters detected";
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
    return "RX · NO ADAPTER";
  const auto& item = mNetworkInterfaces[mRxInterfaceIndex];
  return "RX · " + item.name + " · " + item.ipv4 + "/" +
         std::to_string(item.prefix_length);
}

std::string AeylaVisualDmx::TxInterfaceStatus() const
{
  const std::scoped_lock lock(mNetworkMutex);
  if(mTxInterfaceIndex >= mNetworkInterfaces.size())
    return "TX · NO ADAPTER";
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
    mCaptureInputError = "Select an RX network adapter";
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
  {
    const std::scoped_lock modelLock(mModelMutex);
    const auto snapshot = mModel.snapshot();
    projectId = snapshot.project_id;
    songId = snapshot.active_song_id;
    songName = snapshot.active_song_name;
  }
  aeyla::take_library_session::ensure_scope(this, projectId);

  if(mArtNetCapture.stats().recording)
  {
    if(songId.empty())
    {
      mArtNetCapture.discard_recording();
      return {false, {}, "Capture discarded because no active Song exists"};
    }

    std::size_t nextNumber = 1U;
    {
      const std::scoped_lock takeLock(mTakeMutex);
      const auto found = mTakesBySong.find(songId);
      if(found != mTakesBySong.end())
        nextNumber = found->second.size() + 1U;
    }

    const auto library = aeyla::take_library_session::directory(this);
    if(!library.empty())
    {
      const auto scan = aeyla::capture::scan_take_directory(library, songId);
      if(scan.ok())
        nextNumber = std::max(nextNumber, scan.entries.size() + 1U);
    }

    auto take = mArtNetCapture.end_recording(
        "Take " + std::to_string(nextNumber));
    if(!take.has_value() || take->frames.empty())
      return {false, {}, "Capture stopped but no DMX frames were recorded"};

    const std::string takeName = take->name;
    const double duration = take->duration_seconds();
    bool persisted = false;
    std::filesystem::path savedPath;
    std::string saveError;
    if(!library.empty())
    {
      savedPath = aeyla::capture::make_take_file_path(
          library, songName.empty() ? songId : songName, takeName);
      persisted = aeyla::capture::save_take_file_atomic(
          savedPath, songId, songName, *take, saveError);
      if(persisted)
      {
        aeyla::take_library_session::set_loaded_path(this, songId, savedPath);
        aeyla::take_library_session::set_storage_message(this, {});
      }
      else
      {
        aeyla::take_library_session::set_storage_message(
            this, "DISK SAVE FAILED · " + saveError);
      }
    }
    else
    {
      saveError = "Take library folder is no longer available";
      aeyla::take_library_session::set_storage_message(
          this, "RAM ONLY · " + saveError);
    }

    {
      const std::scoped_lock takeLock(mTakeMutex);
      auto& versions = mTakesBySong[songId];
      versions.push_back(std::move(*take));
      constexpr std::size_t kMaximumCachedTakesPerSong = 5U;
      if(versions.size() > kMaximumCachedTakesPerSong)
        versions.erase(versions.begin(),
                       versions.begin() +
                           static_cast<std::ptrdiff_t>(
                               versions.size() - kMaximumCachedTakesPerSong));
    }

    if(persisted)
      return {true, takeName,
              takeName + " captured · " + FormatDuration(duration) +
                  " · 44 Hz · SAVED " + savedPath.filename().string()};
    return {true, takeName,
            takeName + " captured · " + FormatDuration(duration) +
                " · RAM SAFE, DISK SAVE FAILED · " + saveError};
  }

  if(OutputArmed() || TakeOutputArmed())
    return {false, {}, "DISARM physical output before recording from Avolites"};
  if(TakePlaying())
    return {false, {}, "Stop Take playback before recording a new Take"};
  if(songId.empty())
    return {false, {}, "Create or select a Song before recording a Take"};

  auto library = aeyla::take_library_session::directory(this);
  std::string libraryError;
  if(!library.empty() &&
     !aeyla::capture::prepare_take_directory(library, libraryError))
  {
    aeyla::take_library_session::set_storage_message(
        this, "LIBRARY OFFLINE · " + libraryError);
    library.clear();
  }
  if(library.empty())
  {
    const auto selected = PromptTakeLibraryDirectory(GetUI());
    if(!selected.has_value())
      return {false, {},
              "Recording cancelled · choose a local or external TAKE LIBRARY folder"};
    library = *selected;
    if(!aeyla::capture::prepare_take_directory(library, libraryError))
      return {false, {}, "Take library is not writable · " + libraryError};
    aeyla::take_library_session::set_directory(this, library);
  }

  auto stats = mArtNetCapture.stats();
  if(!stats.running)
  {
    RestartCaptureInputFromRouting();
    stats = mArtNetCapture.stats();
    if(!stats.running)
      return {false, {}, "Art-Net RX could not start · " + CaptureInputStatus()};
  }
  if(!stats.signal_present)
    return {false, {},
            "RX is listening but no live Art-Net signal is present yet. "
            "Enable Continuous/Broadcast Art-Net in Avolites, then press REC again."};

  std::string error;
  if(!mArtNetCapture.begin_recording(error))
    return {false, {}, error};
  aeyla::take_library_session::set_storage_message(
      this, "RECORDING TO · " + library.string());
  return {true, {}, "Recording DMX Take from " + stats.source_ipv4 +
                    " · destination " + library.string()};
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
  const std::scoped_lock modelLock(mModelMutex);
  const std::string songId = ActiveSongIdLocked();
  if(songId.empty()) return false;
  const std::scoped_lock takeLock(mTakeMutex);
  const auto found = mTakesBySong.find(songId);
  return found != mTakesBySong.end() && !found->second.empty();
}

aeyla::product::AuthoringResult AeylaVisualDmx::ToggleActiveTakePlaybackFromUI()
{
  if(TakeRecording())
    return {false, {}, "Stop recording before playing a Take"};
  if(TakePlaying())
  {
    StopActiveTakePlaybackFromUI();
    return {true, {}, "Take playback stopped · HOLD"};
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
    return {false, {}, "No active Song"};

  aeyla::take_library_session::ensure_scope(this, projectId);
  bool haveMemoryTake = false;
  {
    const std::scoped_lock takeLock(mTakeMutex);
    const auto found = mTakesBySong.find(songId);
    haveMemoryTake = found != mTakesBySong.end() && !found->second.empty();
  }

  auto library = aeyla::take_library_session::directory(this);
  if(library.empty() && !haveMemoryTake)
  {
    const auto selected = PromptTakeLibraryDirectory(GetUI());
    if(selected.has_value())
    {
      library = *selected;
      std::string directoryError;
      if(!aeyla::capture::prepare_take_directory(library, directoryError))
        return {false, {}, "Take library is unavailable · " + directoryError};
      aeyla::take_library_session::set_directory(this, library);
    }
  }

  if(!library.empty())
  {
    auto scan = aeyla::capture::scan_take_directory(library, songId);
    if(!scan.ok() && !haveMemoryTake)
    {
      aeyla::take_library_session::set_storage_message(
          this, "LIBRARY OFFLINE · " + scan.error);
      const auto replacement = PromptTakeLibraryDirectory(GetUI());
      if(replacement.has_value())
      {
        library = *replacement;
        std::string directoryError;
        if(!aeyla::capture::prepare_take_directory(library, directoryError))
          return {false, {}, "Take library is unavailable · " + directoryError};
        aeyla::take_library_session::set_directory(this, library);
        scan = aeyla::capture::scan_take_directory(library, songId);
      }
    }

    if(scan.ok() && !scan.entries.empty())
    {
      const auto& newest = scan.entries.front();
      const auto loadedPath =
          aeyla::take_library_session::loaded_path(this, songId);
      if(loadedPath != newest.path)
      {
        std::string loadError;
        auto stored = aeyla::capture::load_take_file(newest.path, loadError);
        if(!stored.has_value())
          return {false, {}, "Take file failed validation · " + loadError};
        if(stored->song_id != songId)
          return {false, {}, "Take belongs to another Song and was not loaded"};
        if(stored->take.port_address != outputUniverse)
          return {false, {},
                  "Take universe does not match the active project output universe"};

        {
          const std::scoped_lock takeLock(mTakeMutex);
          auto& versions = mTakesBySong[songId];
          versions.push_back(std::move(stored->take));
          constexpr std::size_t kMaximumCachedTakesPerSong = 5U;
          if(versions.size() > kMaximumCachedTakesPerSong)
            versions.erase(versions.begin(),
                           versions.begin() +
                               static_cast<std::ptrdiff_t>(
                                   versions.size() - kMaximumCachedTakesPerSong));
        }
        aeyla::take_library_session::set_loaded_path(this, songId, newest.path);
        aeyla::take_library_session::set_storage_message(
            this, "LOADED FROM DISK · " + newest.path.filename().string());
      }
    }
  }

  const aeyla::capture::DmxTake* activeTake = nullptr;
  std::string name;
  double duration = 0.0;
  {
    const std::scoped_lock takeLock(mTakeMutex);
    const auto found = mTakesBySong.find(songId);
    if(found == mTakesBySong.end() || found->second.empty())
      return {false, {},
              "No Take found for " + songName +
                  " · choose the folder containing its .aeylatake files"};
    activeTake = &found->second.back();
    name = activeTake->name;
    duration = activeTake->duration_seconds();
  }

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  std::string error;
  if(!mTakeScheduler.load_take(activeTake, error))
    return {false, {}, error};
  if(!mTakeScheduler.play(error))
    return {false, {}, error};

  return {true, name,
          "Playing " + name + " · " + FormatDuration(duration) +
              " · PRETEST clock / host heartbeat safety"};
}

void AeylaVisualDmx::StopActiveTakePlaybackFromUI()
{
  mTakeScheduler.stop_hold();
}

aeyla::product::AuthoringResult AeylaVisualDmx::ToggleTakeOutputArmFromUI()
{
  if(TakeOutputArmed())
  {
    mTakeScheduler.disarm();
    return {true, {}, "TAKE OUTPUT DISARMED"};
  }
  if(OutputArmed())
    return {false, {}, "Disarm semantic/model output before arming Take output"};
  if(TakeRecording())
    return {false, {}, "Stop Avolites capture before arming physical output"};
  if(!ProjectValid())
    return {false, {}, "Project is invalid"};
  if(!BackendReady())
    return {false, {}, "Configure a healthy Art-Net output target first"};
  if(!RuntimeHealthy() || RenderingOffline())
    return {false, {}, "Runtime/offline safety gate blocks physical output"};
  if(EffectiveBlackout())
    return {false, {}, "Disable BLACKOUT before arming Take output"};

  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    songId = ActiveSongIdLocked();
  }
  if(songId.empty())
    return {false, {}, "No active Song"};

  const aeyla::capture::DmxTake* activeTake = nullptr;
  {
    const std::scoped_lock takeLock(mTakeMutex);
    const auto found = mTakesBySong.find(songId);
    if(found == mTakesBySong.end() || found->second.empty())
      return {false, {}, "PLAY/load a Take for this Song before arming output"};
    activeTake = &found->second.back();
  }

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  const auto schedulerStatus = mTakeScheduler.status();
  std::string error;
  // A completed capture or disk load may have reallocated the Song's Take
  // vector. Reload the current latest Take whenever playback is stopped so ARM
  // can never retain a stale pointer or an older Take's HOLD frame.
  if(!schedulerStatus.playing && !mTakeScheduler.load_take(activeTake, error))
    return {false, {}, error};
  if(!mTakeScheduler.arm(error))
    return {false, {}, error};
  return {true, activeTake->name,
          "TAKE OUTPUT ARMED · " + TxInterfaceStatus()};
}

std::string AeylaVisualDmx::ActiveTakeStatus() const
{
  const auto captureStats = mArtNetCapture.stats();
  if(captureStats.recording)
    return "REC · " + std::to_string(captureStats.recorded_frames) + " FRAMES";

  const std::scoped_lock modelLock(mModelMutex);
  const std::string songId = ActiveSongIdLocked();
  if(songId.empty()) return "NO SONG";
  const std::scoped_lock takeLock(mTakeMutex);
  const auto found = mTakesBySong.find(songId);
  if(found == mTakesBySong.end() || found->second.empty())
  {
    const auto storage = aeyla::take_library_session::storage_message(this);
    return storage.empty() ? "NO TAKE · REC OR PLAY TO CHOOSE TAKE FOLDER"
                           : "NO RAM TAKE · " + storage;
  }
  const auto& take = found->second.back();
  const auto scheduler = mTakeScheduler.status();
  std::string status = take.name + " · " + FormatDuration(take.duration_seconds()) +
                       " · " + std::to_string(take.frames_per_second) + " Hz";
  if(!aeyla::take_library_session::loaded_path(this, songId).empty())
    status += " · DISK";
  else
    status += " · RAM";
  if(scheduler.playing)
    status += " · PLAY";
  else if(scheduler.progress > 0.0)
    status += " · HOLD";
  if(scheduler.armed)
    status += " · ON AIR";
  const auto storage = aeyla::take_library_session::storage_message(this);
  if(!storage.empty() && storage.rfind("DISK SAVE FAILED", 0U) == 0U)
    status += " · SAVE ERROR";
  return status;
}

std::string AeylaVisualDmx::CaptureInputStatus() const
{
  const auto stats = mArtNetCapture.stats();
  if(!stats.running)
  {
    const std::scoped_lock networkLock(mNetworkMutex);
    return mCaptureInputError.empty() ? "RX OFF" : "RX ERROR · " + mCaptureInputError;
  }
  if(stats.signal_present)
    return "RX LIVE · " + stats.source_ipv4 + " · U" +
           std::to_string(stats.port_address) + " · " +
           std::to_string(stats.packets_accepted) + " PKT";
  return "RX READY · WAITING ART-NET · U" + std::to_string(stats.port_address);
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
