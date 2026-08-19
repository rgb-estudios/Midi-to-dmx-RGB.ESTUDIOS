#include "AeylaVisualDmx.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
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

bool AeylaVisualDmx::SelectSongFromUI(std::size_t songIndex)
{
  if(TakeRecording())
    return false;
  StopActiveTakePlaybackFromUI();

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
  mTakePlaybackProgress.store(0.0, std::memory_order_release);
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

  // A routing mutation is always a safe boundary. Physical output must be
  // explicitly re-armed after the selected TX adapter changes.
  ForceDisarmFromUI();
  {
    const std::scoped_lock lock(mNetworkMutex);
    if(mNetworkInterfaces.empty())
      return false;
    mTxInterfaceIndex = WrapIndex(mTxInterfaceIndex, direction,
                                  mNetworkInterfaces.size());
  }

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
  if(mArtNetCapture.stats().recording)
  {
    std::string songId;
    std::size_t nextNumber = 1U;
    {
      const std::scoped_lock modelLock(mModelMutex);
      songId = ActiveSongIdLocked();
    }
    if(songId.empty())
    {
      mArtNetCapture.discard_recording();
      return {false, {}, "Capture discarded because no active Song exists"};
    }
    {
      const std::scoped_lock takeLock(mTakeMutex);
      nextNumber = mTakesBySong[songId].size() + 1U;
    }

    auto take = mArtNetCapture.end_recording(
        "Take " + std::to_string(nextNumber));
    if(!take.has_value() || take->frames.empty())
      return {false, {}, "Capture stopped but no DMX frames were recorded"};

    const std::string takeName = take->name;
    const double duration = take->duration_seconds();
    {
      const std::scoped_lock takeLock(mTakeMutex);
      auto& versions = mTakesBySong[songId];
      versions.push_back(std::move(*take));
      // The host-integrated pretest retains a short rollback history in RAM.
      // Persisted/versioned Takes are a separate package-format gate.
      constexpr std::size_t kMaximumVolatileTakesPerSong = 5U;
      if(versions.size() > kMaximumVolatileTakesPerSong)
        versions.erase(versions.begin(),
                       versions.begin() +
                           static_cast<std::ptrdiff_t>(
                               versions.size() - kMaximumVolatileTakesPerSong));
    }
    mTakePlaybackProgress.store(0.0, std::memory_order_release);
    return {true, takeName,
            takeName + " captured · " + FormatDuration(duration) +
                " · 44 Hz · VOLATILE PRETEST"};
  }

  if(OutputArmed())
    return {false, {}, "DISARM physical output before recording from Avolites"};
  if(TakePlaying())
    return {false, {}, "Stop Take playback before recording a new Take"};

  {
    const std::scoped_lock modelLock(mModelMutex);
    if(ActiveSongIdLocked().empty())
      return {false, {}, "Create or select a Song before recording a Take"};
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
  mTakePlaybackProgress.store(0.0, std::memory_order_release);
  return {true, {}, "Recording DMX Take from " + stats.source_ipv4};
}

bool AeylaVisualDmx::TakeRecording() const noexcept
{
  return mArtNetCapture.stats().recording;
}

bool AeylaVisualDmx::TakePlaying() const noexcept
{
  return mTakePlaybackActive.load(std::memory_order_acquire);
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

  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    songId = ActiveSongIdLocked();
  }
  if(songId.empty())
    return {false, {}, "No active Song"};

  std::string name;
  double duration = 0.0;
  {
    const std::scoped_lock takeLock(mTakeMutex);
    const auto found = mTakesBySong.find(songId);
    if(found == mTakesBySong.end() || found->second.empty())
      return {false, {}, "Record a Take for this Song first"};
    const auto& take = found->second.back();
    name = take.name;
    duration = take.duration_seconds();
    mTakePlaybackStarted = std::chrono::steady_clock::now();
  }
  mTakePlaybackProgress.store(0.0, std::memory_order_release);
  mTakePlaybackActive.store(true, std::memory_order_release);
  return {true, name,
          "Playing " + name + " · " + FormatDuration(duration) +
              " · PRETEST clock"};
}

void AeylaVisualDmx::StopActiveTakePlaybackFromUI()
{
  if(!mTakePlaybackActive.load(std::memory_order_acquire))
    return;

  // Resolve one last frame while the playback clock is still active, then keep
  // its normalized progress. Physical output remains on that frame (HOLD) while
  // ARM remains active; DISARM is the explicit safe-output transition.
  {
    const std::scoped_lock modelLock(mModelMutex);
    aeyla::DmxUniverse frame{};
    double progress = 0.0;
    (void)ActiveTakeFrameForNow(frame, progress);
  }
  mTakePlaybackActive.store(false, std::memory_order_release);
}

bool AeylaVisualDmx::ActiveTakeFrameForNow(aeyla::DmxUniverse& frame,
                                            double& progress)
{
  // Caller owns mModelMutex, preserving a single lock order: model -> take.
  const std::string songId = ActiveSongIdLocked();
  if(songId.empty()) return false;

  const std::scoped_lock takeLock(mTakeMutex);
  const auto found = mTakesBySong.find(songId);
  if(found == mTakesBySong.end() || found->second.empty())
    return false;
  const auto& take = found->second.back();
  if(take.frames.empty() || take.frames_per_second == 0U)
    return false;

  std::size_t index = 0U;
  if(mTakePlaybackActive.load(std::memory_order_acquire))
  {
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - mTakePlaybackStarted).count();
    const double framePosition = elapsed *
        static_cast<double>(take.frames_per_second);
    index = framePosition <= 0.0
                ? 0U
                : static_cast<std::size_t>(framePosition);
    if(index >= take.frames.size())
    {
      index = take.frames.size() - 1U;
      mTakePlaybackActive.store(false, std::memory_order_release);
      progress = 1.0;
    }
    else
    {
      progress = take.frames.size() <= 1U
                     ? 1.0
                     : static_cast<double>(index) /
                           static_cast<double>(take.frames.size() - 1U);
    }
    mTakePlaybackProgress.store(progress, std::memory_order_release);
  }
  else
  {
    progress = std::clamp(
        mTakePlaybackProgress.load(std::memory_order_acquire), 0.0, 1.0);
    index = static_cast<std::size_t>(std::llround(
        progress * static_cast<double>(take.frames.size() - 1U)));
  }

  frame = take.frames[index];
  return true;
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
    return "NO TAKE · READY TO CAPTURE";
  const auto& take = found->second.back();
  std::string status = take.name + " · " + FormatDuration(take.duration_seconds()) +
                       " · " + std::to_string(take.frames_per_second) + " Hz";
  if(mTakePlaybackActive.load(std::memory_order_acquire))
    status += " · PLAY";
  else if(mTakePlaybackProgress.load(std::memory_order_acquire) > 0.0)
    status += " · HOLD";
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
  return std::clamp(mTakePlaybackProgress.load(std::memory_order_acquire), 0.0, 1.0);
}

std::uint64_t AeylaVisualDmx::CaptureAcceptedPackets() const noexcept
{
  return mArtNetCapture.stats().packets_accepted;
}

std::uint64_t AeylaVisualDmx::CaptureSequenceGaps() const noexcept
{
  return mArtNetCapture.stats().sequence_gaps;
}
