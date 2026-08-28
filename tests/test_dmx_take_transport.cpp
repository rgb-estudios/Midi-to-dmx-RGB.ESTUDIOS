#include "capture/dmx_take_transport.h"

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
}  // namespace

int main() {
  using namespace aeyla::capture;

  DmxTakeTransportRequest request;
  request.host_sample_position = 96000;
  request.clip_start_sample = 96000;
  request.sample_rate = 48000.0;
  request.frames_per_second = 44U;
  request.frame_count = 440U;
  request.host_running = true;

  auto projected = project_host_sample_to_take_frame(request);
  check(projected.state == DmxTakeTransportState::in_clip &&
            projected.frame_index == 0U,
        "clip start sample must resolve exactly to frame zero");
  check(projected.host_running,
        "projection must preserve host running state");

  request.host_sample_position = 95999;
  projected = project_host_sample_to_take_frame(request);
  check(projected.state == DmxTakeTransportState::before_clip,
        "sample before clip anchor must remain before_clip");

  request.host_sample_position = 96000 + 48000;
  projected = project_host_sample_to_take_frame(request);
  check(projected.state == DmxTakeTransportState::in_clip &&
            projected.frame_index == 44U,
        "one exact second at 48 kHz must resolve to DMX frame 44");

  request.host_sample_position = 96000 + 24000;
  projected = project_host_sample_to_take_frame(request);
  check(projected.frame_index == 22U,
        "half a second at 48 kHz must resolve to frame 22");

  // Seeking backward is stateless: the projected frame follows the absolute
  // DAW sample position and cannot inherit timer history.
  request.host_sample_position = 96000 + 3 * 48000;
  const auto forward = project_host_sample_to_take_frame(request);
  request.host_sample_position = 96000 + 48000;
  const auto backward = project_host_sample_to_take_frame(request);
  check(forward.frame_index == 132U && backward.frame_index == 44U,
        "backward seek must reconstruct the earlier DMX frame deterministically");

  // 44.1 kHz is common in DAWs and must not drift because the mapping is
  // recomputed from absolute samples rather than incremented frame timers.
  request.clip_start_sample = 0;
  request.sample_rate = 44100.0;
  request.host_sample_position = 44100 * 5;
  request.frame_count = 440U;
  projected = project_host_sample_to_take_frame(request);
  check(projected.frame_index == 220U,
        "five seconds at 44.1 kHz must resolve exactly to frame 220");

  // Non-destructive trim projects host time onto the edited range without
  // mutating the source Take.
  request.sample_rate = 48000.0;
  request.host_sample_position = 0;
  request.range_start_frame = 100U;
  request.range_end_frame_exclusive = 200U;
  projected = project_host_sample_to_take_frame(request);
  check(projected.state == DmxTakeTransportState::in_clip &&
            projected.frame_index == 100U,
        "trimmed clip start must emit the first frame inside the edit range");

  request.host_sample_position = 48000;
  projected = project_host_sample_to_take_frame(request);
  check(projected.frame_index == 144U,
        "trimmed playback must offset host-derived frames by range start");

  request.host_sample_position = 48000 * 3;
  projected = project_host_sample_to_take_frame(request);
  check(projected.state == DmxTakeTransportState::after_clip &&
            projected.frame_index == 199U && projected.progress == 1.0,
        "host position beyond trimmed duration must resolve to after_clip safely");

  request.range_start_frame = 0U;
  request.range_end_frame_exclusive = 0U;
  request.frame_count = 44U;
  request.host_sample_position = 48000;
  projected = project_host_sample_to_take_frame(request);
  check(projected.state == DmxTakeTransportState::after_clip &&
            projected.frame_index == 43U,
        "exact full-take end boundary must not address a frame past the file");

  request.host_sample_position = 0;
  request.rendering_offline = true;
  projected = project_host_sample_to_take_frame(request);
  check(projected.rendering_offline,
        "offline-render state must propagate for physical-output inhibition");

  DmxTakeTransportRequest invalid;
  invalid.host_sample_position = 0;
  invalid.clip_start_sample = 0;
  invalid.sample_rate = 0.0;
  invalid.frames_per_second = 44U;
  invalid.frame_count = 100U;
  check(project_host_sample_to_take_frame(invalid).state ==
            DmxTakeTransportState::unavailable,
        "invalid sample rate must fail closed");

  invalid.sample_rate = 48000.0;
  invalid.frame_count = 0U;
  check(project_host_sample_to_take_frame(invalid).state ==
            DmxTakeTransportState::unavailable,
        "empty Take must fail closed");

  if(failures == 0) {
    std::cout << "All AEYLA DMX take transport tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
