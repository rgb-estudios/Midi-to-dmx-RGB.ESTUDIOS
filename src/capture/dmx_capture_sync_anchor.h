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

struct DmxCaptureSyncStatus {
  DmxCaptureSyncState state{DmxCaptureSyncState::idle};
  std::uint64_t anchor_frame{0U};
};

// Non-realtime capture alignment state machine.
//
// The operator may start recording Art-Net before starting the DAW transport.
// The first stopped -> running boundary then identifies the first DMX frame of
// the song. The RAW Take remains untouched; the captured frame is only used as
// a non-destructive editor IN marker after recording ends.
class DmxCaptureSyncAnchor final {
 public:
  void begin(const runtime::HostTransportSnapshot& initial_host) noexcept;

  // Returns true exactly once when a valid running transport fixes the anchor.
  [[nodiscard]] bool observe(
      const runtime::HostTransportSnapshot& host,
      std::uint64_t recorded_frames) noexcept;

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
};

}  // namespace aeyla::capture
