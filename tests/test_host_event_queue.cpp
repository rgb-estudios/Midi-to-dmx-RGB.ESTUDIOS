#include "runtime/host_event.h"
#include "runtime/host_event_ingress.h"
#include "runtime/spsc_queue.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

using Event = aeyla::runtime::HostEvent;
using EventType = aeyla::runtime::HostEventType;
}  // namespace

int main() {
  // FIFO and empty behavior.
  {
    aeyla::runtime::SpscQueue<Event, 8> queue;
    Event out{};
    check(queue.empty(), "new queue should be empty");
    check(!queue.try_pop(out), "pop on an empty queue must fail");

    Event first{EventType::note_on, 2, 60, 0, 0.75F, 7, 1024};
    Event second{EventType::note_off, 2, 60, 0, 0.25F, 31, 1048};
    check(queue.try_push(first), "first push should succeed");
    check(queue.try_push(second), "second push should succeed");
    check(queue.try_pop(out), "first pop should succeed");
    check(out.type == EventType::note_on && out.channel == 2 && out.note == 60,
          "queue must preserve first event fields");
    check(out.sample_offset == 7 && out.project_sample == 1024,
          "queue must preserve timing fields");
    check(queue.try_pop(out), "second pop should succeed");
    check(out.type == EventType::note_off, "queue must preserve FIFO order");
    check(queue.empty(), "queue should be empty after draining");
  }

  // Full detection and wraparound. Storage capacity 4 gives 3 usable entries.
  {
    aeyla::runtime::SpscQueue<Event, 4> queue;
    Event event{};
    event.type = EventType::note_on;
    event.note = 1;
    check(queue.try_push(event), "push 1 should succeed");
    event.note = 2;
    check(queue.try_push(event), "push 2 should succeed");
    event.note = 3;
    check(queue.try_push(event), "push 3 should succeed");
    event.note = 4;
    check(!queue.try_push(event), "push beyond usable capacity must fail");

    Event out{};
    check(queue.try_pop(out) && out.note == 1, "oldest event should pop first");
    check(queue.try_push(event), "push after pop should wrap and succeed");
    check(queue.try_pop(out) && out.note == 2, "wrapped queue order 2");
    check(queue.try_pop(out) && out.note == 3, "wrapped queue order 3");
    check(queue.try_pop(out) && out.note == 4, "wrapped queue order 4");
  }

  // Concurrent single-producer/single-consumer stress test.
  {
    constexpr std::uint32_t total = 200000;
    aeyla::runtime::SpscQueue<Event, 1024> queue;
    std::atomic<bool> producer_done{false};
    std::atomic<bool> ordering_ok{true};

    std::thread producer([&] {
      for (std::uint32_t i = 0; i < total; ++i) {
        Event event{};
        event.type = EventType::note_on;
        event.project_sample = static_cast<std::int64_t>(i);
        while (!queue.try_push(event)) {
          std::this_thread::yield();
        }
      }
      producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&] {
      std::uint32_t expected = 0;
      Event event{};
      while (expected < total) {
        if (queue.try_pop(event)) {
          if (event.project_sample != static_cast<std::int64_t>(expected)) {
            ordering_ok.store(false, std::memory_order_relaxed);
          }
          ++expected;
        } else if (producer_done.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }
      }
    });

    producer.join();
    consumer.join();
    check(ordering_ok.load(std::memory_order_relaxed),
          "concurrent SPSC transfer must preserve every event in order");
    check(queue.empty(), "queue should be empty after concurrent drain");
  }

  // Overflow must request a safe transient release instead of failing silently.
  {
    aeyla::runtime::HostEventIngress<3> ingress;  // 2 usable entries
    Event event{};
    event.type = EventType::note_on;
    check(ingress.try_submit(event), "ingress submit 1 should succeed");
    check(ingress.try_submit(event), "ingress submit 2 should succeed");
    check(!ingress.try_submit(event), "ingress overflow should be reported");
    check(ingress.dropped_events() == 1, "overflow counter should increment");
    check(ingress.consume_transient_release_request(),
          "overflow should request transient release/haze-off");
    check(!ingress.consume_transient_release_request(),
          "release request should clear after runtime consumes it");
    check(!ingress.try_submit(event), "a repeated overflow should still be reported");
    check(ingress.dropped_events() == 2, "repeated overflow should increment counter");
    check(ingress.consume_transient_release_request(),
          "repeated overflow should re-arm the safety request");
  }

  if (failures == 0) {
    std::cout << "All host event queue tests passed.\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
