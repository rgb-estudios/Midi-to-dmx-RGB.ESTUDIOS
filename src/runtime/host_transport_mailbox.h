#pragma once

#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

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
//
// One host callback thread is the producer. The sequence counter is a seqlock:
// odd means a write is in progress, even means a stable snapshot. The public
// revision is sequence / 2, so it counts complete publications from 1 upward.
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
        sample_position <= static_cast<double>(
            std::numeric_limits<std::int64_t>::max());
    const bool ppq_valid = std::isfinite(ppq_position);
    const bool tempo_valid =
        std::isfinite(tempo_bpm) && tempo_bpm > 0.0 && tempo_bpm <= 1000.0;

    // Enter write section (odd sequence). Single-producer by contract.
    sequence_.fetch_add(1U, std::memory_order_acq_rel);

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

    // Leave write section (even sequence). Release publishes every field above.
    sequence_.fetch_add(1U, std::memory_order_release);
  }

  [[nodiscard]] HostTransportSnapshot latest() const noexcept {
    HostTransportSnapshot result;

    for (;;) {
      const std::uint64_t before = sequence_.load(std::memory_order_acquire);
      if (before == 0U) return result;
      if ((before & 1U) != 0U) continue;

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

      // The acquire fence must be sequenced *after* every payload load and
      // before the closing sequence observation. An acquire load used only for
      // `after` does not stop earlier relaxed payload loads from moving past it
      // on weakly ordered CPUs, which can produce a logically torn snapshot.
      // This is the canonical read-side ordering for a single-writer seqlock.
      std::atomic_thread_fence(std::memory_order_acquire);
      const std::uint64_t after = sequence_.load(std::memory_order_relaxed);
      if (before == after && (after & 1U) == 0U) {
        result.revision = after / 2U;
        return result;
      }
    }
  }

 private:
  static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                "AEYLA host transport mailbox requires lock-free 64-bit atomics");
  static_assert(std::atomic<std::int64_t>::is_always_lock_free,
                "AEYLA host transport mailbox requires lock-free 64-bit atomics");

  std::atomic<std::uint64_t> sequence_{0U};
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
