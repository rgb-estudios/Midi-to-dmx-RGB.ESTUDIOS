#include "capture/dmx_take_consolidator.h"
#include "capture/dmx_take_file_reader.h"
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

std::filesystem::path test_directory() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("aeyla_consolidator_" + std::to_string(stamp));
}

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::capture;

  const auto directory = test_directory();
  std::error_code ec;
  std::filesystem::create_directories(directory, ec);
  check(!ec, "temporary consolidation directory must be created");

  DmxTake source;
  source.name = "TOMA ORIGINAL 01";
  source.port_address = 0U;
  source.frames_per_second = 44U;
  source.source_ipv4 = "2.0.0.10";
  source.frames.resize(440U);
  for(std::size_t frame = 0U; frame < source.frames.size(); ++frame) {
    source.frames[frame].fill(0U);
    source.frames[frame][0] = static_cast<std::uint8_t>(frame & 0xFFU);
    source.frames[frame][1] = static_cast<std::uint8_t>((frame * 3U) & 0xFFU);
    source.frames[frame][511] = static_cast<std::uint8_t>((frame * 7U) & 0xFFU);
  }

  const auto source_path = directory / "original.aeylatake";
  std::string error;
  check(save_take_file_atomic(source_path, "song-01", "STORM", source, error),
        "source Take must save: " + error);

  const auto source_size_before = std::filesystem::file_size(source_path, ec);
  check(!ec, "source Take size must be readable");

  DmxTakeConsolidateRequest request;
  request.source_path = source_path;
  request.target_path = directory / "storm_clip_r01.aeylatake";
  request.start_frame = 44U;
  request.end_frame_exclusive = 352U;
  request.consolidated_name = "CLIP CONSOLIDADO R01";

  const auto result = consolidate_take_range(request);
  check(result.succeeded, "consolidation must succeed: " + result.error);
  check(result.frame_count == 308U,
        "consolidated frame count must equal OUT-IN");
  check(result.frames_per_second == 44U,
        "consolidated clip must preserve 44 Hz");
  check(result.duration_seconds == 7.0,
        "308 frames at 44 Hz must equal exactly 7 seconds");

  DmxTakeFileReader reader;
  error.clear();
  check(reader.open(request.target_path, error),
        "consolidated clip must validate: " + error);
  const auto info = reader.info();
  check(info.song_id == "song-01", "song identity must be preserved");
  check(info.song_name == "STORM", "song name must be preserved");
  check(info.take_name == "CLIP CONSOLIDADO R01",
        "consolidated name must be stored");
  check(info.port_address == 0U, "Art-Net universe must be preserved");
  check(info.frame_count == 308U, "stored frame count must be exact");

  DmxUniverse first{};
  DmxUniverse last{};
  check(reader.read_frame(0U, first, error),
        "first consolidated frame must be readable");
  check(reader.read_frame(307U, last, error),
        "last consolidated frame must be readable");
  check(first == source.frames[44U],
        "consolidated 00:00 must equal original IN frame");
  check(last == source.frames[351U],
        "consolidated final frame must equal original OUT-1 frame");

  const auto source_size_after = std::filesystem::file_size(source_path, ec);
  check(!ec && source_size_after == source_size_before,
        "TOMA ORIGINAL must remain byte-size unchanged after consolidation");

  DmxTakeConsolidateRequest overwrite = request;
  overwrite.target_path = source_path;
  const auto overwrite_result = consolidate_take_range(overwrite);
  check(!overwrite_result.succeeded,
        "consolidation must refuse to overwrite the source Take");

  DmxTakeConsolidateRequest invalid = request;
  invalid.start_frame = 400U;
  invalid.end_frame_exclusive = 300U;
  const auto invalid_result = consolidate_take_range(invalid);
  check(!invalid_result.succeeded,
        "invalid IN/OUT must fail explicitly");

  std::filesystem::remove_all(directory, ec);

  if(failures == 0) {
    std::cout << "All AEYLA DMX Take consolidator tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
