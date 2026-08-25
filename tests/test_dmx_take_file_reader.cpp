#include "capture/dmx_take_file_reader.h"
#include "capture/dmx_take_file_store.h"
#include "capture/dmx_take_stream_writer.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if(!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

std::filesystem::path unique_test_directory() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("aeyla-take-reader-test-" + std::to_string(stamp));
}

bool wait_until_written(aeyla::capture::DmxTakeStreamWriter& writer,
                        std::uint64_t frame_count) {
  for(int attempt = 0; attempt < 5000; ++attempt) {
    const auto status = writer.status();
    if(status.failed) return false;
    if(status.frames_written >= frame_count) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

aeyla::DmxUniverse expected_frame(std::uint64_t index) {
  aeyla::DmxUniverse frame{};
  frame[0] = static_cast<std::uint8_t>(index & 0xFFU);
  frame[17] = static_cast<std::uint8_t>((index * 5U) & 0xFFU);
  frame[255] = static_cast<std::uint8_t>((index * 11U) & 0xFFU);
  frame[511] = static_cast<std::uint8_t>((index * 17U) & 0xFFU);
  return frame;
}
}  // namespace

int main() {
  using namespace aeyla::capture;

  static_assert(DmxTakeFileReader::kCacheBytes == 64U * 1024U);
  check(DmxTakeFileReader::kCacheBytes == 65536U,
        "file-backed playback cache must remain exactly 64 KiB");

  const auto directory = unique_test_directory();
  std::string error;
  check(prepare_take_directory(directory, error),
        "reader test directory must be writable: " + error);

  DmxTakeStreamConfig config;
  config.target_path = directory / "Reader_Source.aeylatake";
  config.song_id = "song-reader";
  config.song_name = "Reader Song";
  config.take_name = "Reader Take";
  config.source_ipv4 = "2.0.0.10";
  config.port_address = 0U;
  config.frames_per_second = 44U;

  DmxTakeStreamWriter writer;
  check(writer.start(config, error),
        "stream writer must start for reader fixture: " + error);

  constexpr std::uint64_t kFrameCount = 44U * 180U;  // three simulated minutes
  for(std::uint64_t index = 0U; index < kFrameCount; ++index) {
    const auto frame = expected_frame(index);
    check(writer.try_push_frame(frame),
          "reader fixture writer rejected frame " + std::to_string(index));
    if(failures != 0) break;
    if((index + 1U) % 256U == 0U) {
      check(wait_until_written(writer, index + 1U),
            "reader fixture disk worker did not drain batch");
      if(failures != 0) break;
    }
  }
  check(writer.finalize(error),
        "reader fixture Take must finalize: " + error);

  DmxTakeFileReader reader;
  check(reader.open(config.target_path, error),
        "file-backed reader must validate/open streamed Take: " + error);
  const auto info = reader.info();
  check(info.open, "reader info must report open state");
  check(info.song_id == config.song_id && info.song_name == config.song_name,
        "reader must expose persisted Song metadata");
  check(info.take_name == config.take_name &&
            info.source_ipv4 == config.source_ipv4,
        "reader must expose persisted Take/source metadata");
  check(info.frames_per_second == 44U && info.frame_count == kFrameCount,
        "reader must expose 44 Hz frame geometry without payload allocation");

  aeyla::DmxUniverse frame{};
  for(const std::uint64_t index :
      {0ULL, 1ULL, 127ULL, 128ULL, 129ULL, 4000ULL,
       kFrameCount - 1ULL, 200ULL, 7000ULL, 64ULL}) {
    check(reader.read_frame(index, frame, error),
          "random frame read must succeed at " + std::to_string(index) +
              ": " + error);
    check(frame == expected_frame(index),
          "random/backward seek returned wrong DMX frame at " +
              std::to_string(index));
  }

  check(!reader.read_frame(kFrameCount, frame, error),
        "reader must reject one-past-end frame");
  check(error.find("outside") != std::string::npos,
        "one-past-end rejection must explain bounds failure");

  reader.close();
  check(!reader.info().open,
        "reader close must release file-backed playback state");
  check(!reader.read_frame(0U, frame, error),
        "closed reader must reject frame access");

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);

  if(failures == 0) {
    std::cout << "All AEYLA constant-memory Take reader tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
