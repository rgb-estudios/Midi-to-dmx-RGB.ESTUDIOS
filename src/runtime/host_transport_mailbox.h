#pragma once

#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>

namespace aeyla::runtime {

struct HostTransportSnapshot {
  std::uint64_t revision{0U};
  bool running{false};
  bool rendering_offline{false};
  bool sample_position_valid{false};
  bool ppq_position_valid{false};
  bool tempo_valid{false};
  std::int64_t sample_position{-1};
  double ppq_position{0.0};
  double tempo_bpm{120.0};
};

// Lock-free latest-state mailbox for the host callback -> runtime boundary.
//
// Transport is deliberately NOT queued like MIDI. Lighting playback depends on
// the host's current absolute position, not on replaying every intermediate
// audio block. If the consumer runs slower than the audio callback it skips old
// snapshots and consumes the newest coherent state.
class HostTransportMailbox final {
 public:
  HostTransportMailbox() = default;

  HostTransportMailbox(const HostTransportMailbox&) = delete;
  HostTransportMailbox& operator=(const HostTransportMailbox&) = delete;

  void publish(bool running, bool rendering_offline,
               double sample_position, double ppq_position,
               double tempo_bpm) noexcept {
    const bool sample_valid =
        std::isfinite(sample_position) && sample_position >= 0.0 &&
        sample_position <= static_cast<double>(INT64_MAX);
    const bool ppq_valid = std::isfinite(ppq_position);
    const bool tempo_valid =
        std::isfinite(tempo_bpm) && tempo_bpm > 0.0 && tempo_bpm <= 1000.0;

    running_.store(running, std::memory_order_relaxed);
    rendering_offline_.store(rendering_offline, std::memory_order_relaxed);
    sample_valid_.store(sample_valid, std::memory_order_relaxed);
    ppq_valid_.store(ppq_valid, std::memory_order_relaxed);
    tempo_valid_.store(tempo_valid, std::memory_order_relaxed);
    sample_position_.store(
        sample_valid ? static_cast<std::int64_t>(sample_position) : -1,
        std::memory_order_relaxed);
    ppq_bits_.store(
        std::bit_cast<std::uint64_t>(ppq_valid ? ppq_position : 0.0),
        std::memory_order_relaxed);
    tempo_bits_.store(
        std::bit_cast<std::uint64_t>(tempo_valid ? tempo_bpm : 120.0),
        std::memory_order_relaxed);

    // Release publishes the complete field set above. Revision 0 means that no
    // host callback has published a snapshot yet.
    revision_.fetch_add(1U, std::memory_order_release);
  }

  [[nodiscard]] HostTransportSnapshot latest() const noexcept {
    HostTransportSnapshot result;

    // A second revision read prevents a torn logical snapshot if the producer
    // publishes while the consumer is sampling the individual lock-free fields.
    for (;;) {
      const std::uint64_t before = revision_.load(std::memory_order_acquire);
      if (before == 0U) return result;

      result.running = running_.load(std::memory_order_relaxed);
      result.rendering_offline =
          rendering_offline_.load(std::memory_order_relaxed);
      result.sample_position_valid =
          sample_valid_.load(std::memory_order_relaxed);
      result.ppq_position_valid = ppq_valid_.load(std::memory_order_relaxed);
      result.tempo_valid = tempo_valid_.load(std::memory_order_relaxed);
      result.sample_position = sample_position_.load(std::memory_order_relaxed);
      result.ppq_position = std::bit_cast<double>(
          ppq_bits_.load(std::memory_order_relaxed));
      result.tempo_bpm = std::bit_cast<double>(
          tempo_bits_.load(std::memory_order_relaxed));

      const std::uint64_t after = revision_.load(std::memory_order_acquire);
      if (before == after) {
        result.revision = after;
        return result;
      }
    }
  }

 private:
  static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                "AEYLA host transport mailbox requires lock-free 64-bit atomics");
  static_assert(std::atomic<std::int64_t>::is_always_lock_free,
                "AEYLA host transport mailbox requires lock-free 64-bit atomics");

  std::atomic<std::uint64_t> revision_{0U};
  std::atomic<bool> running_{false};
  std::atomic<bool> rendering_offline_{false};
  std::atomic<bool> sample_valid_{false};
  std::atomic<bool> ppq_valid_{false};
  std::atomic<bool> tempo_valid_{false};
  std::atomic<std::int64_t> sample_position_{-1};
  std::atomic<std::uint64_t> ppq_bits_{std::bit_cast<std::uint64_t>(0.0)};
  std::atomic<std::uint64_t> tempo_bits_{std::bit_cast<std::uint64_t>(120.0)};
};

}  // namespace aeyla::runtime
