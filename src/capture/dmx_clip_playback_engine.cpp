#include "capture/dmx_clip_playback_engine.h"

#include "capture/dmx_take_transport.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace aeyla::capture {
namespace {

constexpr auto kHostHeartbeatTimeout = std::chrono::milliseconds(750);
constexpr auto kWorkerSleep = std::chrono::milliseconds(1);

bool valid_sample_rate(double sample_rate) noexcept {
  return std::isfinite(sample_rate) &&
         sample_rate >= 8000.0 && sample_rate <= 768000.0;
}

}  // namespace

DmxClipPlaybackEngine::DmxClipPlaybackEngine() = default;

DmxClipPlaybackEngine::~DmxClipPlaybackEngine() { shutdown(); }

void DmxClipPlaybackEngine::attach(
    output::ArtNetOutputWorker* output,
    const runtime::HostTransportMailbox* host) noexcept {
  const std::scoped_lock lock(mutex_);
  if(output_ != output && output_ != nullptr)
    output_->set_override_enabled(false);
  output_ = output;
  host_ = host;
}

bool DmxClipPlaybackEngine::load_clip(const std::filesystem::path& path,
                                      double sample_rate,
                                      std::string& error_message) {
  error_message.clear();
  if(!valid_sample_rate(sample_rate)) {
    error_message = "DMX clip sample rate is outside supported host bounds";
    return false;
  }

  disarm();
  {
    const std::scoped_lock lock(mutex_);
    reader_.close();
    std::string reader_error;
    if(!reader_.open(path, reader_error)) {
      error_message = std::move(reader_error);
      loaded_.store(false, std::memory_order_release);
      return false;
    }
    const auto info = reader_.info();
    if(!info.open || info.frame_count == 0U) {
      reader_.close();
      loaded_.store(false, std::memory_order_release);
      error_message = "Validated DMX clip contains no playable frames";
      return false;
    }

    sample_rate_ = sample_rate;
    range_start_frame_ = 0U;
    range_end_frame_exclusive_ = info.frame_count;
    clip_start_sample_ = -1;
    hold_frame_.fill(0U);
    hold_valid_ = false;
    current_frame_.store(0U, std::memory_order_relaxed);
    progress_.store(0.0, std::memory_order_relaxed);
    triggered_.store(false, std::memory_order_release);
    rendering_offline_.store(false, std::memory_order_release);
    set_error({});
    loaded_.store(true, std::memory_order_release);
  }
  ensure_thread();
  return true;
}

void DmxClipPlaybackEngine::unload() noexcept {
  disarm();
  const std::scoped_lock lock(mutex_);
  reader_.close();
  sample_rate_ = 0.0;
  range_start_frame_ = 0U;
  range_end_frame_exclusive_ = 0U;
  clip_start_sample_ = -1;
  hold_frame_.fill(0U);
  hold_valid_ = false;
  loaded_.store(false, std::memory_order_release);
  triggered_.store(false, std::memory_order_release);
  current_frame_.store(0U, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
}

bool DmxClipPlaybackEngine::set_play_range(
    std::uint64_t start_frame,
    std::uint64_t end_frame_exclusive,
    std::string& error_message) {
  error_message.clear();
  if(armed_.load(std::memory_order_acquire)) {
    error_message = "Disarm DMX clip output before editing its play range";
    return false;
  }

  const std::scoped_lock lock(mutex_);
  const auto info = reader_.info();
  if(!loaded_.load(std::memory_order_acquire) || !info.open) {
    error_message = "Load a DMX clip before editing its play range";
    return false;
  }
  if(start_frame >= end_frame_exclusive ||
     end_frame_exclusive > info.frame_count) {
    error_message = "DMX clip play range is outside source Take bounds";
    return false;
  }

  range_start_frame_ = start_frame;
  range_end_frame_exclusive_ = end_frame_exclusive;
  hold_valid_ = false;
  current_frame_.store(start_frame, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
  return true;
}

void DmxClipPlaybackEngine::reset_play_range() noexcept {
  if(armed_.load(std::memory_order_acquire)) return;
  const std::scoped_lock lock(mutex_);
  const auto info = reader_.info();
  if(!loaded_.load(std::memory_order_acquire) || !info.open) return;
  range_start_frame_ = 0U;
  range_end_frame_exclusive_ = info.frame_count;
  hold_valid_ = false;
  current_frame_.store(0U, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
}

bool DmxClipPlaybackEngine::trigger_at_sample(
    std::int64_t absolute_sample,
    std::string& error_message) {
  error_message.clear();
  if(absolute_sample < 0) {
    error_message = "DMX clip trigger requires a valid absolute host sample";
    return false;
  }

  const std::scoped_lock lock(mutex_);
  if(!loaded_.load(std::memory_order_acquire) || !reader_.info().open) {
    error_message = "Load a DMX clip before triggering playback";
    return false;
  }
  clip_start_sample_ = absolute_sample;
  hold_valid_ = false;
  triggered_.store(true, std::memory_order_release);
  playing_.store(false, std::memory_order_release);
  current_frame_.store(range_start_frame_, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
  return true;
}

void DmxClipPlaybackEngine::clear_trigger() noexcept {
  const std::scoped_lock lock(mutex_);
  clip_start_sample_ = -1;
  hold_valid_ = false;
  triggered_.store(false, std::memory_order_release);
  playing_.store(false, std::memory_order_release);
  progress_.store(0.0, std::memory_order_relaxed);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
}

bool DmxClipPlaybackEngine::arm(std::string& error_message) {
  error_message.clear();
  {
    const std::scoped_lock lock(mutex_);
    if(!loaded_.load(std::memory_order_acquire) || !reader_.info().open) {
      error_message = "Load a validated DMX clip before arming output";
      return false;
    }
    if(output_ == nullptr || host_ == nullptr) {
      error_message = "DMX clip player is not attached to output and host transport";
      return false;
    }
    if(rendering_offline_.load(std::memory_order_acquire)) {
      error_message = "Offline rendering blocks physical DMX clip output";
      return false;
    }
  }
  ensure_thread();
  set_error({});
  armed_.store(true, std::memory_order_release);
  return true;
}

void DmxClipPlaybackEngine::disarm() noexcept {
  armed_.store(false, std::memory_order_release);
  playing_.store(false, std::memory_order_release);
  const std::scoped_lock lock(mutex_);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
}

DmxClipPlaybackStatus DmxClipPlaybackEngine::status() const {
  DmxClipPlaybackStatus result;
  result.running = running_.load(std::memory_order_acquire);
  result.loaded = loaded_.load(std::memory_order_acquire);
  result.armed = armed_.load(std::memory_order_acquire);
  result.triggered = triggered_.load(std::memory_order_acquire);
  result.playing = playing_.load(std::memory_order_acquire);
  result.host_heartbeat_ok = heartbeat_ok_.load(std::memory_order_acquire);
  result.rendering_offline =
      rendering_offline_.load(std::memory_order_acquire);
  result.current_frame = current_frame_.load(std::memory_order_relaxed);
  result.progress = progress_.load(std::memory_order_relaxed);
  result.error = error();
  {
    const std::scoped_lock lock(mutex_);
    result.hold_valid = hold_valid_;
    result.range_start_frame = range_start_frame_;
    result.range_end_frame_exclusive = range_end_frame_exclusive_;
    result.clip_start_sample = clip_start_sample_;
  }
  return result;
}

void DmxClipPlaybackEngine::ensure_thread() {
  if(worker_.joinable()) return;
  stop_requested_.store(false, std::memory_order_release);
  worker_ = std::thread([this]() { run(); });
}

void DmxClipPlaybackEngine::shutdown() noexcept {
  armed_.store(false, std::memory_order_release);
  stop_requested_.store(true, std::memory_order_release);
  if(worker_.joinable()) worker_.join();
  running_.store(false, std::memory_order_release);
  playing_.store(false, std::memory_order_release);
  heartbeat_ok_.store(false, std::memory_order_release);
  const std::scoped_lock lock(mutex_);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
  reader_.close();
  loaded_.store(false, std::memory_order_release);
}

void DmxClipPlaybackEngine::run() noexcept {
  running_.store(true, std::memory_order_release);
  std::uint64_t last_revision = 0U;
  auto last_revision_time = std::chrono::steady_clock::now();
  bool have_revision = false;

  while(!stop_requested_.load(std::memory_order_acquire)) {
    output::ArtNetOutputWorker* output = nullptr;
    const runtime::HostTransportMailbox* host = nullptr;
    {
      const std::scoped_lock lock(mutex_);
      output = output_;
      host = host_;
    }

    if(!armed_.load(std::memory_order_acquire) || output == nullptr || host == nullptr) {
      heartbeat_ok_.store(false, std::memory_order_release);
      playing_.store(false, std::memory_order_release);
      std::this_thread::sleep_for(kWorkerSleep);
      continue;
    }

    const auto snapshot = host->latest();
    const auto now = std::chrono::steady_clock::now();
    if(snapshot.revision != 0U && snapshot.revision != last_revision) {
      last_revision = snapshot.revision;
      last_revision_time = now;
      have_revision = true;
    }
    const bool heartbeat = have_revision &&
        now - last_revision_time <= kHostHeartbeatTimeout;
    heartbeat_ok_.store(heartbeat, std::memory_order_release);
    rendering_offline_.store(snapshot.rendering_offline,
                             std::memory_order_release);

    if(!heartbeat || snapshot.rendering_offline ||
       !snapshot.sample_position_valid) {
      output->set_override_enabled(false);
      playing_.store(false, std::memory_order_release);
      std::this_thread::sleep_for(kWorkerSleep);
      continue;
    }

    if(!triggered_.load(std::memory_order_acquire)) {
      output->set_override_enabled(false);
      playing_.store(false, std::memory_order_release);
      std::this_thread::sleep_for(kWorkerSleep);
      continue;
    }

    if(!snapshot.running) {
      bool hold = false;
      {
        const std::scoped_lock lock(mutex_);
        hold = hold_valid_;
      }
      output->set_override_enabled(hold);
      playing_.store(false, std::memory_order_release);
      std::this_thread::sleep_for(kWorkerSleep);
      continue;
    }

    bool failed = false;
    bool enable_output = false;
    bool playing = false;
    {
      const std::scoped_lock lock(mutex_);
      const auto info = reader_.info();
      if(!loaded_.load(std::memory_order_acquire) || !info.open ||
         clip_start_sample_ < 0 || !valid_sample_rate(sample_rate_)) {
        failed = true;
        set_error("DMX clip runtime lost a valid loaded/triggered state");
      } else {
        DmxTakeTransportRequest request;
        request.host_sample_position = snapshot.sample_position;
        request.clip_start_sample = clip_start_sample_;
        request.sample_rate = sample_rate_;
        request.frames_per_second = info.frames_per_second;
        request.frame_count = static_cast<std::size_t>(info.frame_count);
        request.range_start_frame =
            static_cast<std::size_t>(range_start_frame_);
        request.range_end_frame_exclusive =
            static_cast<std::size_t>(range_end_frame_exclusive_);
        request.host_running = snapshot.running;
        request.rendering_offline = snapshot.rendering_offline;
        const auto projection = project_host_sample_to_take_frame(request);

        if(projection.state == DmxTakeTransportState::before_clip) {
          enable_output = false;
        } else if(projection.state == DmxTakeTransportState::in_clip ||
                  projection.state == DmxTakeTransportState::after_clip) {
          DmxUniverse frame{};
          std::string read_error;
          if(!reader_.read_frame(projection.frame_index, frame, read_error)) {
            failed = true;
            set_error("DMX clip frame read failed · " + read_error);
          } else {
            if(!hold_valid_ ||
               current_frame_.load(std::memory_order_relaxed) !=
                   projection.frame_index) {
              hold_frame_ = frame;
              hold_valid_ = true;
              current_frame_.store(projection.frame_index,
                                   std::memory_order_relaxed);
              output->publish_override(hold_frame_, ++generation_);
            }
            progress_.store(projection.progress, std::memory_order_relaxed);
            enable_output = true;
            playing = projection.state == DmxTakeTransportState::in_clip;
          }
        } else {
          failed = true;
          set_error("Host sample position could not be projected onto DMX clip");
        }
      }
    }

    if(failed) {
      armed_.store(false, std::memory_order_release);
      output->set_override_enabled(false);
      playing_.store(false, std::memory_order_release);
    } else {
      output->set_override_enabled(enable_output);
      playing_.store(playing, std::memory_order_release);
    }

    std::this_thread::sleep_for(kWorkerSleep);
  }

  running_.store(false, std::memory_order_release);
}

void DmxClipPlaybackEngine::set_error(std::string message) noexcept {
  try {
    const std::scoped_lock lock(error_mutex_);
    error_ = std::move(message);
  } catch(...) {
  }
}

std::string DmxClipPlaybackEngine::error() const {
  const std::scoped_lock lock(error_mutex_);
  return error_;
}

}  // namespace aeyla::capture
