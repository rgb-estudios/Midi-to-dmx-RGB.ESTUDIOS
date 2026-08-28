#include "capture/dmx_take_activity.h"

#include "capture/dmx_take_file_reader.h"

#include <algorithm>
#include <cstdlib>

namespace aeyla::capture {

DmxTakeActivityEnvelope build_take_activity_envelope(
    const std::filesystem::path& path,
    std::size_t requested_buckets) {
  DmxTakeActivityEnvelope result;
  requested_buckets = std::clamp<std::size_t>(
      requested_buckets, 1U, kMaximumTakeActivityBuckets);

  DmxTakeFileReader reader;
  std::string error;
  if(!reader.open(path, error)) {
    result.error = "No se pudo analizar la actividad DMX · " + error;
    return result;
  }

  const auto info = reader.info();
  result.frame_count = info.frame_count;
  result.frames_per_second = info.frames_per_second;
  const auto bucket_count = static_cast<std::size_t>(
      std::min<std::uint64_t>(info.frame_count, requested_buckets));
  if(bucket_count == 0U) {
    result.error = "La toma no contiene cuadros para mostrar";
    return result;
  }
  result.buckets.resize(bucket_count);

  DmxUniverse previous{};
  DmxUniverse current{};
  bool have_previous = false;
  for(std::uint64_t frame_index = 0U;
      frame_index < info.frame_count; ++frame_index) {
    if(!reader.read_frame(frame_index, current, error)) {
      result.buckets.clear();
      result.error = "No se pudo leer la actividad DMX · " + error;
      return result;
    }

    std::uint8_t level_peak = 0U;
    std::uint8_t motion_peak = 0U;
    for(std::size_t channel = 0U; channel < current.size(); ++channel) {
      level_peak = std::max(level_peak, current[channel]);
      if(have_previous) {
        const int difference = static_cast<int>(current[channel]) -
                               static_cast<int>(previous[channel]);
        motion_peak = std::max(
            motion_peak, static_cast<std::uint8_t>(std::abs(difference)));
      }
    }

    const auto bucket_index = static_cast<std::size_t>(
        std::min<std::uint64_t>(
            (frame_index * static_cast<std::uint64_t>(bucket_count)) /
                info.frame_count,
            bucket_count - 1U));
    auto& bucket = result.buckets[bucket_index];
    bucket.level = std::max(bucket.level, level_peak);
    bucket.motion = std::max(bucket.motion, motion_peak);
    previous = current;
    have_previous = true;
  }
  return result;
}

}  // namespace aeyla::capture
