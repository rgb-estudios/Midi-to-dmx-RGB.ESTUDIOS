#include "capture/dmx_take_file_store.h"
#include "capture/dmx_take_stream_writer.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
         ("aeyla-stream-take-test-" + std::to_string(stamp));
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

std::string read_text(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}
}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::capture;

  static_assert(DmxTakeStreamWriter::kBufferedBytes <= 1024U * 1024U);
  check(DmxTakeStreamWriter::kBufferedBytes == 512U * 1024U,
        "streamed capture frame buffer must remain fixed at 512 KiB");

  const auto directory = unique_test_directory();
  std::string error;
  check(prepare_take_directory(directory, error),
        "stream test directory must be writable: " + error);

  const auto target = directory / "Intro__Take_01.aeylatake";
  DmxTakeStreamConfig config;
  config.target_path = target;
  config.song_id = "song-intro";
  config.song_name = "INTRO";
  config.take_name = "Take 01";
  config.source_ipv4 = "2.0.0.10";
  config.port_address = 0U;
  config.frames_per_second = 44U;

  DmxTakeStreamWriter writer;
  check(writer.start(config, error),
        "stream writer must start: " + error);

  constexpr std::uint64_t kFrameCount = 44U * 120U;  // two simulated minutes
  for(std::uint64_t index = 0U; index < kFrameCount; ++index) {
    DmxUniverse frame{};
    frame[0] = static_cast<std::uint8_t>(index & 0xFFU);
    frame[255] = static_cast<std::uint8_t>((index * 3U) & 0xFFU);
    frame[511] = static_cast<std::uint8_t>((index * 7U) & 0xFFU);
    check(writer.try_push_frame(frame),
          "bounded writer must accept paced test frame " +
              std::to_string(index));
    if(failures != 0) break;

    // Keep the producer far below the fixed queue bound while still writing
    // much faster than real-time capture. This validates duration-independent
    // buffering without turning scheduler speed into a flaky overflow test.
    if((index + 1U) % 256U == 0U) {
      check(wait_until_written(writer, index + 1U),
            "disk worker must drain paced frame batch");
      if(failures != 0) break;
    }
  }

  check(writer.finalize(error),
        "streamed Take must finalize atomically: " + error);
  const auto status = writer.status();
  check(!status.failed,
        "successful streamed Take must not report writer failure");
  check(status.frames_enqueued == kFrameCount &&
            status.frames_written == kFrameCount,
        "streamed Take must persist every accepted frame exactly once");
  check(status.queue_overflows == 0U,
        "paced streamed capture must not overflow its bounded queue");
  check(status.peak_buffered_frames <= DmxTakeStreamWriter::kBufferedFrames,
        "peak buffered frames must never exceed the fixed 512 KiB queue");
  check(std::filesystem::exists(target),
        "finalized streamed Take must exist at final path");
  check(!std::filesystem::exists(target.string() + ".tmp"),
        "successful finalize must remove temporary file name");

  const auto scan = scan_take_directory(directory, config.song_id);
  check(scan.ok(), "streamed Take library scan must succeed: " + scan.error);
  check(scan.entries.size() == 1U,
        "streamed Take must be visible to normal library indexing");
  if(!scan.entries.empty()) {
    check(scan.entries.front().frame_count == kFrameCount,
          "streamed Take header must contain final frame count");
    check(scan.entries.front().frames_per_second == 44U,
          "streamed Take header must preserve 44 Hz capture rate");
    check(scan.entries.front().source_ipv4 == config.source_ipv4,
          "streamed Take header must preserve source metadata");
  }

  // Full decoder compatibility is proven on the same multi-minute file. The
  // production player will later become file-backed, but v1 files must remain
  // byte-compatible with the existing decoder while that migration proceeds.
  auto loaded = load_take_file(target, error);
  check(loaded.has_value(),
        "streamed Take must pass existing v1 checksum decoder: " + error);
  if(loaded.has_value()) {
    check(loaded->take.frames.size() == kFrameCount,
          "existing decoder must observe every streamed frame");
    check(loaded->take.frames.front()[0] == 0U,
          "first streamed frame must round-trip");
    const auto last = kFrameCount - 1U;
    check(loaded->take.frames.back()[0] ==
              static_cast<std::uint8_t>(last & 0xFFU) &&
          loaded->take.frames.back()[511] ==
              static_cast<std::uint8_t>((last * 7U) & 0xFFU),
          "last streamed frame must round-trip exactly");
  }

  // P0 RAW immutability: a second capture may never reuse an already-published
  // .aeylatake path. Refuse before recording rather than backup/replace.
  const auto original_size = std::filesystem::file_size(target);
  DmxTakeStreamWriter collision;
  auto collision_config = config;
  collision_config.take_name = "Collision";
  error.clear();
  check(!collision.start(collision_config, error),
        "stream writer must fail closed when RAW target already exists");
  check(error.find("immutable") != std::string::npos,
        "existing RAW collision must report immutable contract");
  check(std::filesystem::file_size(target) == original_size,
        "existing RAW collision must not alter target bytes");
  check(!std::filesystem::exists(target.string() + ".bak"),
        "RAW collision must never create a replacement backup path");
  auto reloaded = load_take_file(target, error);
  check(reloaded.has_value() && reloaded->take.frames.size() == kFrameCount,
        "existing RAW must remain checksum-valid after refused collision");

  // P0 race gate: if a destination appears after recording starts but before
  // publish, finalize must preserve that destination and discard only our temp.
  const auto race_target = directory / "Race.aeylatake";
  DmxTakeStreamWriter raced;
  auto race_config = config;
  race_config.target_path = race_target;
  race_config.take_name = "Race";
  error.clear();
  check(raced.start(race_config, error),
        "race-path writer must start while destination is absent: " + error);
  DmxUniverse race_frame{};
  race_frame[7] = 77U;
  check(raced.try_push_frame(race_frame),
        "race-path writer must accept a frame");
  check(wait_until_written(raced, 1U),
        "race-path writer must durably write its frame before collision");
  {
    std::ofstream sentinel(race_target, std::ios::binary | std::ios::trunc);
    sentinel << "DO-NOT-REPLACE";
  }
  error.clear();
  check(!raced.finalize(error),
        "finalize must fail closed if RAW destination appears during capture");
  check(error.find("immutable") != std::string::npos,
        "late RAW collision must report immutable contract");
  check(read_text(race_target) == "DO-NOT-REPLACE",
        "late collision must leave pre-existing destination byte-for-byte intact");
  check(!std::filesystem::exists(race_target.string() + ".tmp"),
        "late collision must clean only its unpublished temporary file");
  check(!std::filesystem::exists(race_target.string() + ".bak"),
        "late RAW collision must never create a backup/replacement file");

  // Aborting a new recording must remove the incomplete .tmp and never replace
  // the last known-good final Take.
  DmxTakeStreamWriter aborted;
  const auto abort_target = directory / "Abort.aeylatake";
  config.target_path = abort_target;
  config.take_name = "Abort";
  check(aborted.start(config, error),
        "abort-path writer must start: " + error);
  DmxUniverse abort_frame{};
  abort_frame[1] = 99U;
  check(aborted.try_push_frame(abort_frame),
        "abort-path writer must accept one frame");
  aborted.abort();
  check(!std::filesystem::exists(abort_target),
        "aborted capture must not publish an incomplete final Take");
  check(!std::filesystem::exists(abort_target.string() + ".tmp"),
        "aborted capture must remove temporary file");

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);

  if(failures == 0) {
    std::cout << "All AEYLA streamed Take writer tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
