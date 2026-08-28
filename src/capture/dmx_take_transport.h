#pragma once

#include <cstddef>
#include <cstdint>

namespace aeyla::capture {

enum class DmxTakeTransportState : std::uint8_t {
  unavailable = 0,
  before_clip,
  in_clip,
  after_clip,
};

struct DmxTakeTransportRequest {
  std::int64_t host_sample_position{-1};
  std::int64_t clip_start_sample{-1};
  double sample_rate{0.0};
  std::uint16_t frames_per_second{44U};
  std::size_t frame_count{0U};
  std::size_t range_start_frame{0U};
  // Zero means original frame_count.
  std::size_t range_end_frame_exclusive{0U};
  bool host_running{false};
  bool rendering_offline{false};
};

struct DmxTakeTransportProjection {
  DmxTakeTransportState state{DmxTakeTransportState::unavailable};
  bool host_running{false};
  bool rendering_offline{false};
  std::size_t frame_index{0U};
  double progress{0.0};
};

// Projects one absolute DAW sample position onto a captured DMX Take.
//
// This is deliberately stateless. The frame is derived from the current host
// sample position on every call, so pause/seek/loop/restart cannot accumulate
// timer drift. The function never advances from wall clock.
[[nodiscard]] DmxTakeTransportProjection project_host_sample_to_take_frame(
    const DmxTakeTransportRequest& request) noexcept;

}  // namespace aeyla::capture
