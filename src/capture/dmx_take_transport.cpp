#include "capture/dmx_take_transport.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aeyla::capture {
namespace {

[[nodiscard]] bool valid_request(const DmxTakeTransportRequest& request) noexcept {
  return request.host_sample_position >= 0 &&
         request.clip_start_sample >= 0 &&
         std::isfinite(request.sample_rate) &&
         request.sample_rate >= 8000.0 && request.sample_rate <= 768000.0 &&
         request.frames_per_second >= 1U &&
         request.frames_per_second <= 60U &&
         request.frame_count > 0U;
}

[[nodiscard]] std::size_t normalized_range_start(
    const DmxTakeTransportRequest& request) noexcept {
  return std::min(request.range_start_frame, request.frame_count - 1U);
}

[[nodiscard]] std::size_t normalized_range_end(
    const DmxTakeTransportRequest& request,
    std::size_t start) noexcept {
  if(request.range_end_frame_exclusive == 0U)
    return request.frame_count;
  return std::clamp(request.range_end_frame_exclusive,
                    start + 1U,
                    request.frame_count);
}

}  // namespace

DmxTakeTransportProjection project_host_sample_to_take_frame(
    const DmxTakeTransportRequest& request) noexcept {
  DmxTakeTransportProjection result;
  result.host_running = request.host_running;
  result.rendering_offline = request.rendering_offline;

  if(!valid_request(request))
    return result;

  const std::size_t range_start = normalized_range_start(request);
  const std::size_t range_end = normalized_range_end(request, range_start);
  result.frame_index = range_start;

  if(request.host_sample_position < request.clip_start_sample) {
    result.state = DmxTakeTransportState::before_clip;
    return result;
  }

  const std::uint64_t elapsed_samples = static_cast<std::uint64_t>(
      request.host_sample_position - request.clip_start_sample);
  const long double exact_frame =
      static_cast<long double>(elapsed_samples) *
      static_cast<long double>(request.frames_per_second) /
      static_cast<long double>(request.sample_rate);

  if(!std::isfinite(static_cast<double>(exact_frame)) || exact_frame < 0.0L)
    return result;

  // Preserve floor semantics: a future frame must never be emitted early.
  // A tiny epsilon only repairs floating representation exactly at an integer
  // boundary such as 48,000 samples -> frame 44 at 48 kHz / 44 Hz.
  constexpr long double kBoundaryEpsilon = 1e-12L;
  const long double floored = std::floor(exact_frame + kBoundaryEpsilon);
  if(floored > static_cast<long double>(
                   std::numeric_limits<std::size_t>::max())) {
    result.state = DmxTakeTransportState::after_clip;
    result.frame_index = range_end - 1U;
    result.progress = 1.0;
    return result;
  }

  const std::size_t relative_frame = static_cast<std::size_t>(floored);
  const std::size_t range_length = range_end - range_start;
  if(relative_frame >= range_length) {
    result.state = DmxTakeTransportState::after_clip;
    result.frame_index = range_end - 1U;
    result.progress = 1.0;
    return result;
  }

  result.state = DmxTakeTransportState::in_clip;
  result.frame_index = range_start + relative_frame;
  result.progress = range_length <= 1U
      ? 1.0
      : std::clamp(static_cast<double>(relative_frame) /
                       static_cast<double>(range_length - 1U),
                   0.0, 1.0);
  return result;
}

}  // namespace aeyla::capture
