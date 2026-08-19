#include "capture/dmx_take_scheduler.h"

#include <algorithm>
#include <cmath>

namespace aeyla::capture {

DmxTakeScheduler::DmxTakeScheduler() = default;
DmxTakeScheduler::~DmxTakeScheduler() { shutdown(); }

void DmxTakeScheduler::attach(output::ArtNetOutputWorker* output,
                              const runtime::HostTransportMailbox* host) noexcept {
  const std::scoped_lock lock(mutex_);
  output_ = output;
  host_ = host;
}

bool DmxTakeScheduler::load_take(const DmxTake* take,
                                 std::string& error_message) {
  error_message.clear();
  if(take == nullptr || take->frames.empty() || take->frames_per_second == 0U) {
    error_message = "Selected Take has no playable DMX frames";
    return false;
  }
  if(playing_.load(std::memory_order_acquire)) {
    error_message = "Stop the active Take before loading another Take";
    return false;
  }

  const std::scoped_lock lock(mutex_);
  take_ = take;
  range_start_frame_ = take_->effective_start_frame();
  range_end_frame_exclusive_ = take_->effective_end_frame_exclusive();
  if(range_end_frame_exclusive_ <= range_start_frame_ + 1U) {
    range_start_frame_ = 0U;
    range_end_frame_exclusive_ = take_->frames.size();
  }
  current_frame_ = range_start_frame_;
  hold_frame_ = take_->frames[current_frame_];
  hold_valid_ = true;
  progress_.store(0.0, std::memory_order_release);
  error_.clear();
  if(armed_.load(std::memory_order_acquire))
    publish_hold_locked();
  return true;
}

bool DmxTakeScheduler::set_play_range(std::size_t start_frame,
                                      std::size_t end_frame_exclusive,
                                      std::string& error_message) {
  error_message.clear();
  if(playing_.load(std::memory_order_acquire)) {
    error_message = "Stop Take playback before editing IN / OUT";
    return false;
  }

  const std::scoped_lock lock(mutex_);
  if(take_ == nullptr || take_->frames.empty()) {
    error_message = "Load a Take before editing IN / OUT";
    return false;
  }
  if(start_frame >= take_->frames.size()) {
    error_message = "IN is outside the recorded Take";
    return false;
  }
  if(end_frame_exclusive == 0U || end_frame_exclusive > take_->frames.size()) {
    error_message = "OUT is outside the recorded Take";
    return false;
  }
  if(end_frame_exclusive <= start_frame + 1U) {
    error_message = "IN / OUT must leave at least two DMX frames";
    return false;
  }

  range_start_frame_ = start_frame;
  range_end_frame_exclusive_ = end_frame_exclusive;
  current_frame_ = range_start_frame_;
  hold_frame_ = take_->frames[current_frame_];
  hold_valid_ = true;
  progress_.store(0.0, std::memory_order_release);
  error_.clear();
  if(armed_.load(std::memory_order_acquire))
    publish_hold_locked();
  return true;
}

void DmxTakeScheduler::reset_play_range() noexcept {
  if(playing_.load(std::memory_order_acquire))
    return;
  const std::scoped_lock lock(mutex_);
  if(take_ == nullptr || take_->frames.empty())
    return;
  range_start_frame_ = 0U;
  range_end_frame_exclusive_ = take_->frames.size();
  current_frame_ = 0U;
  hold_frame_ = take_->frames.front();
  hold_valid_ = true;
  progress_.store(0.0, std::memory_order_release);
  if(armed_.load(std::memory_order_acquire))
    publish_hold_locked();
}

bool DmxTakeScheduler::play(std::string& error_message) {
  error_message.clear();
  ensure_thread();
  const std::scoped_lock lock(mutex_);
  if(take_ == nullptr || take_->frames.empty()) {
    error_message = "No Take is loaded";
    return false;
  }
  if(range_end_frame_exclusive_ <= range_start_frame_ + 1U ||
     range_end_frame_exclusive_ > take_->frames.size()) {
    error_message = "Take IN / OUT range is invalid";
    return false;
  }
  current_frame_ = range_start_frame_;
  hold_frame_ = take_->frames[current_frame_];
  hold_valid_ = true;
  progress_.store(0.0, std::memory_order_release);
  play_started_ = std::chrono::steady_clock::now();
  playing_.store(true, std::memory_order_release);
  if(armed_.load(std::memory_order_acquire))
    publish_hold_locked();
  return true;
}

void DmxTakeScheduler::stop_hold() noexcept {
  playing_.store(false, std::memory_order_release);
  const std::scoped_lock lock(mutex_);
  if(armed_.load(std::memory_order_acquire) && hold_valid_)
    publish_hold_locked();
}

bool DmxTakeScheduler::arm(std::string& error_message) {
  error_message.clear();
  ensure_thread();

  const std::scoped_lock lock(mutex_);
  if(output_ == nullptr) {
    error_message = "Art-Net output worker is not attached";
    return false;
  }
  if(!output_->stats().running) {
    error_message = "Art-Net output backend is not running";
    return false;
  }
  if(take_ == nullptr || !hold_valid_) {
    error_message = "Load or record a Take before arming Take output";
    return false;
  }
  if(host_ == nullptr) {
    error_message = "DAW host transport heartbeat is unavailable";
    return false;
  }
  const auto host = host_->latest();
  if(host.revision == 0U) {
    error_message = "DAW host has not published a realtime transport heartbeat yet";
    return false;
  }
  if(host.rendering_offline) {
    error_message = "Take output is inhibited during offline render";
    return false;
  }

  heartbeat_ok_.store(true, std::memory_order_release);
  armed_.store(true, std::memory_order_release);
  output_->publish_override(hold_frame_, generation_++);
  output_->set_override_enabled(true);
  error_.clear();
  return true;
}

void DmxTakeScheduler::disarm() noexcept {
  armed_.store(false, std::memory_order_release);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
}

DmxTakeSchedulerStatus DmxTakeScheduler::status() const {
  DmxTakeSchedulerStatus result;
  result.running = running_.load(std::memory_order_acquire);
  result.armed = armed_.load(std::memory_order_acquire);
  result.playing = playing_.load(std::memory_order_acquire);
  result.progress = std::clamp(progress_.load(std::memory_order_acquire), 0.0, 1.0);
  result.host_heartbeat_ok = heartbeat_ok_.load(std::memory_order_acquire);
  {
    const std::scoped_lock lock(mutex_);
    result.hold_valid = hold_valid_;
    result.range_start_frame = range_start_frame_;
    result.range_end_frame_exclusive = range_end_frame_exclusive_;
    result.current_frame = current_frame_;
    result.error = error_;
  }
  return result;
}

void DmxTakeScheduler::ensure_thread() {
  if(worker_.joinable()) return;
  stop_requested_.store(false, std::memory_order_release);
  worker_ = std::thread([this]() { run(); });
}

void DmxTakeScheduler::shutdown() noexcept {
  playing_.store(false, std::memory_order_release);
  armed_.store(false, std::memory_order_release);
  stop_requested_.store(true, std::memory_order_release);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
  if(worker_.joinable()) worker_.join();
  running_.store(false, std::memory_order_release);
}

void DmxTakeScheduler::publish_hold_locked() {
  if(output_ == nullptr || !hold_valid_) return;
  output_->publish_override(hold_frame_, generation_++);
}

void DmxTakeScheduler::update_position_locked(
    std::chrono::steady_clock::time_point now) {
  if(take_ == nullptr || take_->frames.empty() || take_->frames_per_second == 0U)
    return;
  if(range_end_frame_exclusive_ <= range_start_frame_ + 1U ||
     range_end_frame_exclusive_ > take_->frames.size())
    return;

  const double elapsed = std::chrono::duration<double>(now - play_started_).count();
  const double rawOffset = std::max(0.0, elapsed) *
                           static_cast<double>(take_->frames_per_second);
  std::size_t offset = static_cast<std::size_t>(rawOffset);
  const std::size_t rangeFrames = range_end_frame_exclusive_ - range_start_frame_;
  if(offset >= rangeFrames) {
    offset = rangeFrames - 1U;
    playing_.store(false, std::memory_order_release);
    progress_.store(1.0, std::memory_order_release);
  } else {
    const double progress = rangeFrames <= 1U
                                ? 1.0
                                : static_cast<double>(offset) /
                                      static_cast<double>(rangeFrames - 1U);
    progress_.store(progress, std::memory_order_release);
  }

  current_frame_ = range_start_frame_ + offset;
  hold_frame_ = take_->frames[current_frame_];
  hold_valid_ = true;
}

void DmxTakeScheduler::run() noexcept {
  running_.store(true, std::memory_order_release);
  using Clock = std::chrono::steady_clock;
  constexpr auto kLoopPeriod = std::chrono::milliseconds(2);
  constexpr auto kHeartbeatTimeout = std::chrono::milliseconds(750);

  std::uint64_t lastRevision = 0U;
  auto lastHeartbeat = Clock::now();

  while(!stop_requested_.load(std::memory_order_acquire)) {
    const auto now = Clock::now();

    bool hostSafe = false;
    if(host_ != nullptr) {
      const auto host = host_->latest();
      if(host.revision != 0U && host.revision != lastRevision) {
        lastRevision = host.revision;
        lastHeartbeat = now;
      }
      hostSafe = host.revision != 0U && !host.rendering_offline &&
                 now - lastHeartbeat <= kHeartbeatTimeout;
    }
    heartbeat_ok_.store(hostSafe, std::memory_order_release);

    if(!hostSafe && armed_.load(std::memory_order_acquire)) {
      armed_.store(false, std::memory_order_release);
      if(output_ != nullptr)
        output_->set_override_enabled(false);
      const std::scoped_lock lock(mutex_);
      error_ = "Take output auto-disarmed: host heartbeat/offline safety gate";
    }

    {
      const std::scoped_lock lock(mutex_);
      if(playing_.load(std::memory_order_acquire))
        update_position_locked(now);
      if(armed_.load(std::memory_order_acquire) && hold_valid_)
        publish_hold_locked();
    }

    std::this_thread::sleep_for(kLoopPeriod);
  }

  if(output_ != nullptr)
    output_->set_override_enabled(false);
  running_.store(false, std::memory_order_release);
}

}  // namespace aeyla::capture
