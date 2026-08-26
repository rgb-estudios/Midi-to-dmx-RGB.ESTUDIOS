#include "capture/dmx_take_activity.h"
#include "capture/dmx_take_file_store.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
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

  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() /
      ("aeyla_activity_" + std::to_string(stamp));
  std::error_code fs_error;
  std::filesystem::create_directories(directory, fs_error);
  check(!fs_error, "temporary activity directory must be created");

  DmxTake take;
  take.name = "TOMA ORIGINAL";
  take.port_address = 0U;
  take.frames_per_second = 44U;
  take.source_ipv4 = "2.0.0.10";
  take.frames.resize(176U);
  for(std::size_t frame = 0U; frame < take.frames.size(); ++frame) {
    take.frames[frame].fill(frame < 44U ? 0U : 80U);
    if(frame >= 88U && frame < 132U)
      take.frames[frame][0] = (frame % 2U) == 0U ? 255U : 0U;
    if(frame >= 132U)
      take.frames[frame].fill(255U);
  }

  const auto path = directory / "activity.aeylatake";
  std::string error;
  check(save_take_file_atomic(path, "song-activity", "ACTIVIDAD", take, error),
        "activity Take must save: " + error);

  const auto envelope = build_take_activity_envelope(path, 4U);
  check(envelope.ok(), "activity envelope must build: " + envelope.error);
  check(envelope.buckets.size() == 4U,
        "requested bounded activity bucket count must be preserved");
  check(envelope.frame_count == 176U && envelope.frames_per_second == 44U,
        "activity envelope must preserve file geometry");
  if(envelope.buckets.size() == 4U) {
    check(envelope.buckets[0].level == 0U,
          "silent DMX region must render as zero activity level");
    check(envelope.buckets[1].level > envelope.buckets[0].level,
          "lit DMX region must render above silent region");
    check(envelope.buckets[2].motion > envelope.buckets[1].motion,
          "changing DMX region must expose visible motion");
    check(envelope.buckets[3].level == 255U,
          "full DMX region must reach full normalized level");
  }

  const auto bounded = build_take_activity_envelope(path, 1000U);
  check(bounded.ok() &&
        bounded.buckets.size() <= kMaximumTakeActivityBuckets,
        "activity envelope memory must remain bounded");

  std::filesystem::remove_all(directory, fs_error);
  if(failures == 0) {
    std::cout << "All AEYLA DMX Take activity tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
