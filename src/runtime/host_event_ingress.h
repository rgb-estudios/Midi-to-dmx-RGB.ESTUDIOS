#pragma once

#include "runtime/host_event.h"
#include "runtime/spsc_queue.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace aeyla::runtime {

// Realtime-safe ingress owned by one host callback producer and one runtime
// consumer. A full queue is treated as a safety condition because dropping a
// note-off or release event could leave a transient executor, strobe or haze
// active. The callback only updates atomics; the runtime performs the actual
// safe-state transition.
template <std::size_t StorageCapacity>
class HostEventIngress {
 public:
  [[nodiscard]] bool try_submit(const HostEvent& event) noexcept {
    if (queue_.try_push(event)) {
      return true;
    }

    dropped_events_.fetch_add(1, std::memory_order_relaxed);
    transient_release_requested_.store(true, std::memory_order_release);
    return false;
  }

  [[nodiscard]] bool try_consume(HostEvent& event) noexcept {
    return queue_.try_pop(event);
  }

  [[nodiscard]] std::uint64_t dropped_events() const noexcept {
    return dropped_events_.load(std::memory_order_relaxed);
  }

  // Runtime-thread operation. Returns true once for each observed overflow
  // period; repeated overflows after consumption set it again.
  [[nodiscard]] bool consume_transient_release_request() noexcept {
    return transient_release_requested_.exchange(false, std::memory_order_acq_rel);
  }

  [[nodiscard]] bool empty() const noexcept { return queue_.empty(); }

 private:
  SpscQueue<HostEvent, StorageCapacity> queue_{};
  std::atomic<std::uint64_t> dropped_events_{0};
  std::atomic<bool> transient_release_requested_{false};
};

}  // namespace aeyla::runtime
