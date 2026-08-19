#include "runtime/host_transport_mailbox.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
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
}  // namespace

int main() {
  using aeyla::runtime::HostTransportMailbox;

  HostTransportMailbox mailbox;
  const auto initial = mailbox.latest();
  check(initial.revision == 0U,
        "transport mailbox must explicitly report no host snapshot at startup");

  mailbox.publish(true, false, 48000.0, 4.0, 120.0);
  const auto first = mailbox.latest();
  check(first.revision == 1U && first.running && !first.rendering_offline,
        "first transport snapshot must preserve running/offline flags");
  check(first.sample_position_valid && first.sample_position == 48000,
        "sample position must survive callback-to-runtime publication");
  check(first.ppq_position_valid && std::fabs(first.ppq_position - 4.0) < 1e-12,
        "PPQ position must survive callback-to-runtime publication");
  check(first.tempo_valid && std::fabs(first.tempo_bpm - 120.0) < 1e-12,
        "tempo must survive callback-to-runtime publication");

  mailbox.publish(false, true,
                  std::numeric_limits<double>::quiet_NaN(),
                  std::numeric_limits<double>::infinity(), -1.0);
  const auto invalid = mailbox.latest();
  check(invalid.revision == 2U && !invalid.running && invalid.rendering_offline,
        "latest host flags must replace the previous transport state");
  check(!invalid.sample_position_valid && invalid.sample_position == -1,
        "non-finite sample position must be rejected rather than propagated");
  check(!invalid.ppq_position_valid && invalid.ppq_position == 0.0,
        "non-finite PPQ position must be replaced by a deterministic safe value");
  check(!invalid.tempo_valid && invalid.tempo_bpm == 120.0,
        "invalid tempo must be replaced by a deterministic safe value");

  // Concurrency campaign: the producer writes a relation between sample and PPQ
  // that the reader can verify. A torn logical snapshot would violate it.
  HostTransportMailbox concurrent;
  std::atomic<bool> producer_done{false};
  std::thread producer([&]() {
    for (std::int64_t sequence = 1; sequence <= 200000; ++sequence) {
      concurrent.publish((sequence % 2) != 0, false,
                         static_cast<double>(sequence),
                         static_cast<double>(sequence) * 0.001,
                         100.0 + static_cast<double>(sequence % 100));
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::uint64_t last_revision = 0U;
  while (!producer_done.load(std::memory_order_acquire)) {
    const auto snapshot = concurrent.latest();
    if (snapshot.revision == 0U) continue;

    check(snapshot.revision >= last_revision,
          "consumer revisions must never move backwards");
    last_revision = snapshot.revision;
    check(snapshot.sample_position_valid && snapshot.ppq_position_valid,
          "concurrency campaign must only publish valid positions");
    check(std::fabs(snapshot.ppq_position -
                    static_cast<double>(snapshot.sample_position) * 0.001) < 1e-9,
          "consumer must never observe fields from two different publications");
  }
  producer.join();

  const auto final = concurrent.latest();
  check(final.revision == 200000U,
        "mailbox must retain only the latest producer revision");
  check(final.sample_position == 200000,
        "final mailbox snapshot must expose the newest sample position");
  check(std::fabs(final.ppq_position - 200.0) < 1e-9,
        "final mailbox snapshot must expose the newest PPQ position");

  if (failures == 0) {
    std::cout << "All AEYLA host-transport mailbox tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
