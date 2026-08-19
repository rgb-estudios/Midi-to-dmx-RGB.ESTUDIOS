#include "AeylaVisualDmx.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
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

}  // namespace

aeyla::product::AuthoringResult AeylaVisualDmx::AdjustActiveTakeInFromUI(
    double deltaSeconds)
{
  if(TakeRecording())
    return {false, {}, "Stop REC before editing Take IN"};
  if(TakePlaying())
    return {false, {}, "STOP / HOLD before editing Take IN"};
  if(TakeOutputArmed())
    return {false, {}, "DISARM Take output before editing IN / OUT"};

  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    songId = ActiveSongIdLocked();
  }
  if(songId.empty())
    return {false, {}, "No active Song"};

  const aeyla::capture::DmxTake* activeTake = nullptr;
  double inSeconds = 0.0;
  double outSeconds = 0.0;
  {
    const std::scoped_lock takeLock(mTakeMutex);
    const auto found = mTakesBySong.find(songId);
    if(found == mTakesBySong.end() || found->second.empty())
      return {false, {}, "Record or load a Take before editing IN / OUT"};

    auto& take = found->second.back();
    if(take.frames.size() < 2U || take.frames_per_second == 0U)
      return {false, {}, "Take is too short to trim"};

    const auto end = take.effective_end_frame_exclusive();
    const long long current = static_cast<long long>(take.effective_start_frame());
    const long long maximum = static_cast<long long>(end) - 2LL;
    const long long next = std::clamp(
        current + DeltaFrames(deltaSeconds, take.frames_per_second),
        0LL, std::max(0LL, maximum));
    take.trim_start_frame = static_cast<std::size_t>(next);

    activeTake = &take;
    inSeconds = static_cast<double>(take.effective_start_frame()) /
                static_cast<double>(take.frames_per_second);
    outSeconds = static_cast<double>(take.effective_end_frame_exclusive()) /
                 static_cast<double>(take.frames_per_second);
  }

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  std::string error;
  if(!mTakeScheduler.load_take(activeTake, error))
    return {false, {}, error};

  return {true, {}, "TAKE IN " + FormatTime(inSeconds) +
                    " · OUT " + FormatTime(outSeconds)};
}

aeyla::product::AuthoringResult AeylaVisualDmx::AdjustActiveTakeOutFromUI(
    double deltaSeconds)
{
  if(TakeRecording())
    return {false, {}, "Stop REC before editing Take OUT"};
  if(TakePlaying())
    return {false, {}, "STOP / HOLD before editing Take OUT"};
  if(TakeOutputArmed())
    return {false, {}, "DISARM Take output before editing IN / OUT"};

  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    songId = ActiveSongIdLocked();
  }
  if(songId.empty())
    return {false, {}, "No active Song"};

  const aeyla::capture::DmxTake* activeTake = nullptr;
  double inSeconds = 0.0;
  double outSeconds = 0.0;
  {
    const std::scoped_lock takeLock(mTakeMutex);
    const auto found = mTakesBySong.find(songId);
    if(found == mTakesBySong.end() || found->second.empty())
      return {false, {}, "Record or load a Take before editing IN / OUT"};

    auto& take = found->second.back();
    if(take.frames.size() < 2U || take.frames_per_second == 0U)
      return {false, {}, "Take is too short to trim"};

    const auto start = take.effective_start_frame();
    const long long current =
        static_cast<long long>(take.effective_end_frame_exclusive());
    const long long minimum = static_cast<long long>(start) + 2LL;
    const long long maximum = static_cast<long long>(take.frames.size());
    const long long next = std::clamp(
        current + DeltaFrames(deltaSeconds, take.frames_per_second),
        minimum, maximum);

    take.trim_end_frame_exclusive =
        next == maximum ? 0U : static_cast<std::size_t>(next);
    activeTake = &take;
    inSeconds = static_cast<double>(take.effective_start_frame()) /
                static_cast<double>(take.frames_per_second);
    outSeconds = static_cast<double>(take.effective_end_frame_exclusive()) /
                 static_cast<double>(take.frames_per_second);
  }

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  std::string error;
  if(!mTakeScheduler.load_take(activeTake, error))
    return {false, {}, error};

  return {true, {}, "TAKE IN " + FormatTime(inSeconds) +
                    " · OUT " + FormatTime(outSeconds)};
}

aeyla::product::AuthoringResult AeylaVisualDmx::ResetActiveTakeTrimFromUI()
{
  if(TakeRecording() || TakePlaying() || TakeOutputArmed())
    return {false, {}, "STOP and DISARM before resetting Take IN / OUT"};

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
      return {false, {}, "No active Take"};
    auto& take = found->second.back();
    take.trim_start_frame = 0U;
    take.trim_end_frame_exclusive = 0U;
    activeTake = &take;
  }

  mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
  std::string error;
  if(!mTakeScheduler.load_take(activeTake, error))
    return {false, {}, error};
  return {true, {}, "Take IN / OUT reset to original recording"};
}

double AeylaVisualDmx::ActiveTakeInSeconds() const
{
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    songId = ActiveSongIdLocked();
  }
  const std::scoped_lock takeLock(mTakeMutex);
  const auto found = mTakesBySong.find(songId);
  if(found == mTakesBySong.end() || found->second.empty()) return 0.0;
  const auto& take = found->second.back();
  if(take.frames_per_second == 0U) return 0.0;
  return static_cast<double>(take.effective_start_frame()) /
         static_cast<double>(take.frames_per_second);
}

double AeylaVisualDmx::ActiveTakeOutSeconds() const
{
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    songId = ActiveSongIdLocked();
  }
  const std::scoped_lock takeLock(mTakeMutex);
  const auto found = mTakesBySong.find(songId);
  if(found == mTakesBySong.end() || found->second.empty()) return 0.0;
  const auto& take = found->second.back();
  if(take.frames_per_second == 0U) return 0.0;
  return static_cast<double>(take.effective_end_frame_exclusive()) /
         static_cast<double>(take.frames_per_second);
}

double AeylaVisualDmx::ActiveTakeOriginalDurationSeconds() const
{
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    songId = ActiveSongIdLocked();
  }
  const std::scoped_lock takeLock(mTakeMutex);
  const auto found = mTakesBySong.find(songId);
  return found == mTakesBySong.end() || found->second.empty()
             ? 0.0
             : found->second.back().duration_seconds();
}

double AeylaVisualDmx::ActiveTakeEffectiveDurationSeconds() const
{
  std::string songId;
  {
    const std::scoped_lock modelLock(mModelMutex);
    songId = ActiveSongIdLocked();
  }
  const std::scoped_lock takeLock(mTakeMutex);
  const auto found = mTakesBySong.find(songId);
  return found == mTakesBySong.end() || found->second.empty()
             ? 0.0
             : found->second.back().effective_duration_seconds();
}
