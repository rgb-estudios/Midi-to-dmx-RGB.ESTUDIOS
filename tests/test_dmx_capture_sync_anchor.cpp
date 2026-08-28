#include "capture/dmx_capture_sync_anchor.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if(!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

aeyla::runtime::HostTransportSnapshot host(
    std::uint64_t revision, bool running, bool offline = false) {
  aeyla::runtime::HostTransportSnapshot result;
  result.revision = revision;
  result.running = running;
  result.rendering_offline = offline;
  return result;
}
}  // namespace

int main() {
  using namespace aeyla::capture;

  DmxCaptureSyncAnchor sync;
  check(sync.status().state == DmxCaptureSyncState::idle,
        "capture sync must start idle");

  sync.begin(host(0U, false));
  check(sync.status().state == DmxCaptureSyncState::unavailable,
        "capture sync must not invent a host snapshot");

  sync.begin(host(1U, true));
  check(sync.status().state == DmxCaptureSyncState::unavailable,
        "recording started after PLAY cannot infer the song boundary");

  sync.begin(host(2U, false));
  check(sync.status().state == DmxCaptureSyncState::waiting_for_transport,
        "recording started while stopped must wait for PLAY/MTC");
  check(!sync.observe(host(3U, false), 80U),
        "stopped transport must not fix an IN anchor");
  check(!sync.observe(host(4U, true, true), 90U),
        "offline transport must not fix a physical capture anchor");
  check(sync.observe(host(5U, true), 132U),
        "first realtime PLAY must fix the capture anchor");
  check(!sync.observe(host(6U, true), 220U),
        "later host blocks must not move the first anchor");

  const auto status = sync.status();
  check(status.state == DmxCaptureSyncState::anchored &&
            status.anchor_frame == 132U,
        "capture anchor must retain the recorded-frame boundary");
  check(sync.resolved_anchor(440U) == 132U,
        "normal recording must preserve the captured anchor");
  check(sync.resolved_anchor(133U) == 131U,
        "late anchor must retain at least two editable frames");

  sync.reset();
  check(sync.status().state == DmxCaptureSyncState::idle &&
            !sync.resolved_anchor(440U).has_value(),
        "reset must clear synchronization between recordings");

  if(failures == 0) {
    std::cout << "All AEYLA capture-sync anchor tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
