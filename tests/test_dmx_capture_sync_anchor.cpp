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

  // A running host no longer means the whole recording is unsynchronizable:
  // REAPER may already be in pre-roll and a MIDI SHOW note can still identify
  // the exact artistic boundary later in the same capture.
  sync.begin(host(1U, true));
  check(sync.status().state == DmxCaptureSyncState::unavailable,
        "running-at-record-start must not invent a transport boundary");
  check(sync.anchor_explicit(64U),
        "explicit MIDI marker must anchor a recording that began during pre-roll");
  auto status = sync.status();
  check(status.state == DmxCaptureSyncState::anchored &&
            status.anchor_frame == 64U &&
            status.source == DmxCaptureSyncSource::show_midi_marker,
        "explicit pre-roll marker must be retained as the preferred anchor");
  check(!sync.anchor_explicit(96U),
        "later retriggers must not move the first explicit marker");
  check(sync.resolved_anchor(440U) == 64U,
        "pre-roll marker must resolve to the captured frame");

  sync.reset();
  sync.begin(host(2U, false));
  check(sync.status().state == DmxCaptureSyncState::waiting_for_transport,
        "recording started while stopped must wait for PLAY fallback");
  check(!sync.observe(host(3U, false), 80U),
        "stopped transport must not fix an IN anchor");
  check(!sync.observe(host(4U, true, true), 90U),
        "offline transport must not fix a physical capture anchor");
  check(sync.observe(host(5U, true), 132U),
        "first realtime PLAY must fix the fallback capture anchor");
  check(!sync.observe(host(6U, true), 220U),
        "later host blocks must not move the transport fallback anchor");

  status = sync.status();
  check(status.state == DmxCaptureSyncState::anchored &&
            status.anchor_frame == 132U &&
            status.source == DmxCaptureSyncSource::transport_start,
        "fallback anchor must retain the recorded-frame boundary and source");

  // If PLAY was only pre-roll, the first explicit MIDI SHOW marker is more
  // authoritative and is allowed to refine that coarse fallback exactly once.
  check(sync.anchor_explicit(176U),
        "explicit marker must refine an earlier transport fallback");
  status = sync.status();
  check(status.anchor_frame == 176U &&
            status.source == DmxCaptureSyncSource::show_midi_marker,
        "explicit marker must replace the transport fallback");
  check(!sync.anchor_explicit(220U),
        "second explicit marker must not shift an established Song boundary");
  check(sync.resolved_anchor(440U) == 176U,
        "normal recording must preserve the refined explicit anchor");
  check(sync.resolved_anchor(177U) == 175U,
        "late anchor must retain at least two editable frames");

  sync.reset();
  check(sync.status().state == DmxCaptureSyncState::idle &&
            sync.status().source == DmxCaptureSyncSource::none &&
            !sync.resolved_anchor(440U).has_value(),
        "reset must clear synchronization source between recordings");
  check(!sync.anchor_explicit(10U),
        "an explicit marker outside a recording session must be ignored");

  if(failures == 0) {
    std::cout << "All AEYLA capture-sync anchor tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
