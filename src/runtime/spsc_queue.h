#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace aeyla::runtime {

// Fixed-capacity, single-producer/single-consumer ring buffer.
//
// Real-time contract:
// - no dynamic allocation;
// - no locks or condition variables;
// - try_push/try_pop are noexcept and bounded;
// - exactly one producer thread and one consumer thread are permitted.
//
// One slot is reserved to distinguish full from empty, so usable_capacity is
// StorageCapacity - 1.
template <typename T, std::size_t StorageCapacity>
class SpscQueue {
  static_assert(StorageCapacity >= 2, "SPSC queue requires at least two slots");
  static_assert(std::is_trivially_copyable_v<T>, "SPSC queue items must be trivially copyable");

 public:
  static constexpr std::size_t storage_capacity = StorageCapacity;
  static constexpr std::size_t usable_capacity = StorageCapacity - 1;

  SpscQueue() = default;
  SpscQueue(const SpscQueue&) = delete;
  SpscQueue& operator=(const SpscQueue&) = delete;

  [[nodiscard]] bool try_push(const T& item) noexcept {
    const auto write = write_index_.load(std::memory_order_relaxed);
    const auto next = increment(write);
    if (next == read_index_.load(std::memory_order_acquire)) {
      return false;
    }

    storage_[write] = item;
    write_index_.store(next, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool try_pop(T& item) noexcept {
    const auto read = read_index_.load(std::memory_order_relaxed);
    if (read == write_index_.load(std::memory_order_acquire)) {
      return false;
    }

    item = storage_[read];
    read_index_.store(increment(read), std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool empty() const noexcept {
    return read_index_.load(std::memory_order_acquire) ==
           write_index_.load(std::memory_order_acquire);
  }

  void reset_consumer_side() noexcept {
    // Only call when producer activity has stopped. Intended for teardown and
    // test setup, never for concurrent operation.
    read_index_.store(write_index_.load(std::memory_order_acquire),
                      std::memory_order_release);
  }

 private:
  static constexpr std::size_t increment(std::size_t index) noexcept {
    return (index + 1U) % StorageCapacity;
  }

  alignas(64) std::array<T, StorageCapacity> storage_{};
  alignas(64) std::atomic<std::size_t> write_index_{0};
  alignas(64) std::atomic<std::size_t> read_index_{0};
};

}  // namespace aeyla::runtime
