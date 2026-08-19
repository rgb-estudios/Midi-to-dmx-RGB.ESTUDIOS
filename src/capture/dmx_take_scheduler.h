#pragma once

#include "capture/artnet_capture_worker.h"
#include "output/artnet_output_worker.h"
#include "runtime/host_transport_mailbox.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace aeyla::capture {

struct DmxTakeSchedulerStatus {
  bool running{false};
  bool armed{false};
  bool playing{false};
  bool hold_valid{false};
  bool host_heartbeat_ok{false};
  double progress{0.0};
  std::string error;
};

// Non-realtime scheduler for captured DMX Takes.
//
// The scheduler is independent from the plug-in editor and never touches the
// audio callback. It advances a selected Take from steady clock for the first
// capture/replay hardware gate, while the DAW host transport mailbox acts as a
// liveness/offline safety heartbeat. A later gate will replace the steady-clock
// position with absolute host sample position without changing the Take format.
class DmxTakeScheduler final {
 public:
  DmxTakeScheduler();
  ~DmxTakeScheduler();

  DmxTakeScheduler(const DmxTakeScheduler&) = delete;
  DmxTakeScheduler& operator=(const DmxTakeScheduler&) = delete;

  void attach(output::ArtNetOutputWorker* output,
              const runtime::HostTransportMailbox* host) noexcept;

  [[nodiscard]] bool load_take(const DmxTake* take,
                               std::string& error_message);
  [[nodiscard]] bool play(std::string& error_message);
  void stop_hold() noexcept;

  [[nodiscard]] bool arm(std::string& error_message);
  void disarm() noexcept;

  [[nodiscard]] DmxTakeSchedulerStatus status() const;

 private:
  void ensure_thread();
  void shutdown() noexcept;
  void run() noexcept;
  void publish_hold_locked();
  void update_position_locked(std::chrono::steady_clock::time_point now);

  mutable std::mutex mutex_;
  output::ArtNetOutputWorker* output_{nullptr};
  const runtime::HostTransportMailbox* host_{nullptr};
  const DmxTake* take_{nullptr};
  DmxUniverse hold_frame_{};
  bool hold_valid_{false};
  std::chrono::steady_clock::time_point play_started_{};
  std::uint64_t generation_{2000000000ULL};
  std::string error_;

  std::thread worker_;
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> armed_{false};
  std::atomic<bool> playing_{false};
  std::atomic<double> progress_{0.0};
  std::atomic<bool> heartbeat_ok_{false};
};

}  // namespace aeyla::capture
