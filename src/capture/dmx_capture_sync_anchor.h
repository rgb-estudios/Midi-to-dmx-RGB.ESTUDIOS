#pragma once

#include "runtime/host_transport_mailbox.h"

#include <cstdint>
#include <mutex>
#include <optional>

namespace aeyla::capture {

enum class DmxCaptureSyncState : std::uint8_t {
  idle = 0,
  waiting_for_transport,
  anchored,
  unavailable,
};

enum class DmxCaptureSyncSource : std::uint8_t {
  none = 0,
  transport_start,
  show_midi_marker,
};

struct DmxCaptureSyncStatus {
  DmxCaptureSyncState state{DmxCaptureSyncState::idle};
  std::uint64_t anchor_frame{0U};
  DmxCaptureSyncSource source{DmxCaptureSyncSource::none};
};

// Non-realtime capture alignment state machine.
//
// The operator may start recording Art-Net before the actual Song boundary.
// A stopped -> running DAW transition remains a backwards-compatible fallback,
// while an explicit MIDI SHOW marker may establish (or refine) the anchor even
// when the DAW was already running in pre-roll. The RAW Take remains untouched;
// the captured frame is only used as a non-destructive editor IN marker after
// recording ends.
class DmxCaptureSyncAnchor final {
 public:
  void begin(const runtime::HostTransportSnapshot& initial_host) noexcept;

  // Legacy non-realtime fallback. Kept for hosts where no callback-edge marker
  // was published; an audio-callback transport snapshot below is preferred.
  [[nodiscard]] bool observe(
      const runtime::HostTransportSnapshot& host,
      std::uint64_t recorded_frames) noexcept;

  // Preferred automatic path for REC -> PLAY. The audio callback snapshots the
  // 44 Hz capture cursor on the exact STOP->PLAY block boundary, then the
  // non-realtime worker commits that frame here. An explicit MIDI marker may
  // still refine this transport anchor later.
  [[nodiscard]] bool anchor_transport_snapshot(
      std::uint64_t recorded_frames) noexcept;

  // Preferred explicit show path: MIDI PLAY/launch identifies the actual Song
  // boundary. The capture frame itself was snapshotted at MIDI ingress. It may
  // replace a prior transport anchor, but the first explicit marker wins so
  // retriggers cannot move the edit point.
  [[nodiscard]] bool anchor_explicit(std::uint64_t recorded_frames) noexcept;

  void reset() noexcept;

  [[nodiscard]] DmxCaptureSyncStatus status() const noexcept;

  // Leaves at least two frames in the editable range. No anchor is returned
  // when synchronization was unavailable or the recording is too short.
  [[nodiscard]] std::optional<std::uint64_t> resolved_anchor(
      std::uint64_t total_frames) const noexcept;

 private:
  mutable std::mutex mutex_;
  DmxCaptureSyncState state_{DmxCaptureSyncState::idle};
  std::uint64_t anchor_frame_{0U};
  DmxCaptureSyncSource source_{DmxCaptureSyncSource::none};
};

}  // namespace aeyla::capture
