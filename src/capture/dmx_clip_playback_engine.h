#pragma once

#include "capture/dmx_take_file_reader.h"
#include "output/artnet_output_worker.h"
#include "runtime/host_transport_mailbox.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace aeyla::capture {

struct DmxClipPlaybackStatus {
  bool running{false};
  bool loaded{false};
  bool armed{false};
  bool triggered{false};
  bool playing{false};
  bool hold_valid{false};
  bool host_heartbeat_ok{false};
  bool rendering_offline{false};
  std::uint64_t current_frame{0U};
  std::uint64_t range_start_frame{0U};
  std::uint64_t range_end_frame_exclusive{0U};
  std::int64_t clip_start_sample{-1};
  double progress{0.0};
  std::string error;
};

// File-backed, DAW-sample-locked DMX clip player.
//
// The host sample position is the sole artistic clock. The worker never advances
// the clip from wall time; wall time is used only to detect a dead host callback.
// STOP therefore holds the last valid frame, while seek/loop/restart reconstruct
// from absolute host samples without cumulative drift.
class DmxClipPlaybackEngine final {
 public:
  DmxClipPlaybackEngine();
  ~DmxClipPlaybackEngine();

  DmxClipPlaybackEngine(const DmxClipPlaybackEngine&) = delete;
  DmxClipPlaybackEngine& operator=(const DmxClipPlaybackEngine&) = delete;

  void attach(output::ArtNetOutputWorker* output,
              const runtime::HostTransportMailbox* host) noexcept;

  [[nodiscard]] bool load_clip(const std::filesystem::path& path,
                               double sample_rate,
                               std::string& error_message);
  void unload() noexcept;

  [[nodiscard]] bool set_play_range(std::uint64_t start_frame,
                                    std::uint64_t end_frame_exclusive,
                                    std::string& error_message);
  void reset_play_range() noexcept;

  // Called from a non-realtime bridge after a DAW MIDI/event trigger has been
  // translated to an absolute host sample: blockStart + event.sampleOffset.
  [[nodiscard]] bool trigger_at_sample(std::int64_t absolute_sample,
                                       std::string& error_message);
  void clear_trigger() noexcept;

  [[nodiscard]] bool arm(std::string& error_message);
  void disarm() noexcept;

  [[nodiscard]] DmxClipPlaybackStatus status() const;

 private:
  void ensure_thread();
  void shutdown() noexcept;
  void run() noexcept;
  void set_error(std::string message) noexcept;
  [[nodiscard]] std::string error() const;

  mutable std::mutex mutex_;
  output::ArtNetOutputWorker* output_{nullptr};
  const runtime::HostTransportMailbox* host_{nullptr};
  DmxTakeFileReader reader_{};
  double sample_rate_{0.0};
  std::uint64_t range_start_frame_{0U};
  std::uint64_t range_end_frame_exclusive_{0U};
  std::int64_t clip_start_sample_{-1};
  DmxUniverse hold_frame_{};
  bool hold_valid_{false};
  std::uint64_t generation_{3000000000ULL};

  mutable std::mutex error_mutex_;
  std::string error_;

  std::thread worker_;
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> loaded_{false};
  std::atomic<bool> armed_{false};
  std::atomic<bool> triggered_{false};
  std::atomic<bool> playing_{false};
  std::atomic<bool> heartbeat_ok_{false};
  std::atomic<bool> rendering_offline_{false};
  std::atomic<std::uint64_t> current_frame_{0U};
  std::atomic<double> progress_{0.0};
};

}  // namespace aeyla::capture
