#include "capture/artnet_capture_worker.h"
#include "capture/dmx_take_file_store.h"
#include "output/artnet_output_worker.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {

void require(bool condition, const char* message) {
  if(!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

template <typename Predicate>
bool wait_until(Predicate predicate,
                std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while(std::chrono::steady_clock::now() < deadline) {
    if(predicate()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return predicate();
}

std::filesystem::path unique_test_directory() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("aeyla-artnet-stream-capture-" + std::to_string(stamp));
}

}  // namespace

int main() {
  constexpr std::uint16_t kPort = 16645U;
  constexpr std::uint16_t kUniverse = 7U;

  aeyla::capture::ArtNetCaptureWorker capture;
  aeyla::capture::ArtNetCaptureConfig capture_config;
  capture_config.listen_ipv4 = "127.0.0.1";
  capture_config.udp_port = kPort;
  capture_config.port_address = kUniverse;
  capture_config.frames_per_second = 44U;

  std::string error;
  require(capture.start(capture_config, error), error.c_str());
  require(wait_until([&]() { return capture.stats().running; }),
          "capture worker did not enter running state");

  aeyla::output::ArtNetOutputWorker output;
  aeyla::output::ArtNetOutputConfig output_config;
  output_config.source_ipv4 = "127.0.0.1";
  output_config.target_ipv4 = "127.0.0.1";
  output_config.udp_port = kPort;
  output_config.port_address = kUniverse;
  output_config.channel_count = 512U;
  output_config.frames_per_second = 44U;
  require(output.start(output_config, error), error.c_str());

  aeyla::DmxUniverse expected{};
  expected[0] = 17U;
  expected[1] = 99U;
  expected[44] = 200U;
  expected[511] = 255U;
  output.publish_latest(expected, 1U);
  output.set_enabled(true);

  require(wait_until([&]() { return capture.stats().packets_accepted >= 2U; }),
          "capture worker did not accept loopback ArtDMX packets");

  aeyla::DmxUniverse received{};
  require(capture.latest_frame(received), "capture did not expose latest frame");
  require(received == expected, "latest captured DMX frame differs from source");

  // Keep the old short in-memory gate until product migration is complete.
  require(capture.begin_recording(error), error.c_str());
  std::this_thread::sleep_for(std::chrono::milliseconds(180));
  auto take = capture.end_recording("Loopback Take");
  require(take.has_value(), "recording did not produce a Take");
  require(take->frames_per_second == 44U, "Take FPS changed from requested 44 Hz");
  require(take->port_address == kUniverse, "Take universe differs from capture config");
  require(take->source_ipv4 == "127.0.0.1", "Take source lock is incorrect");
  require(take->frames.size() >= 5U, "fixed-rate recorder produced too few frames");
  require(take->frames.back() == expected, "recorded Take frame differs from source");

  // Production path: the same sampler must write through the fixed 512 KiB
  // queue directly to .aeylatake without accumulating frames in its vector.
  const auto directory = unique_test_directory();
  require(aeyla::capture::prepare_take_directory(directory, error), error.c_str());
  aeyla::capture::DmxTakeStreamConfig stream_config;
  stream_config.target_path = directory / "Loopback_Stream.aeylatake";
  stream_config.song_id = "song-loopback";
  stream_config.song_name = "Loopback Song";
  stream_config.take_name = "Stream Take";
  stream_config.source_ipv4 = "127.0.0.1";
  stream_config.port_address = kUniverse;
  stream_config.frames_per_second = 44U;
  require(capture.begin_streamed_recording(stream_config, error), error.c_str());
  require(capture.streamed_recording_active(),
          "capture worker did not enter streamed recording mode");
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  require(capture.end_streamed_recording(error), error.c_str());
  require(!capture.streamed_recording_active(),
          "streamed recording mode remained active after STOP");

  const auto streamed_stats = capture.stats();
  require(!streamed_stats.storage_failed,
          "streamed capture reported storage failure");
  require(!streamed_stats.overflowed,
          "streamed capture overflowed its bounded queue");
  require(streamed_stats.recorded_frames >= 6U,
          "streamed capture produced too few fixed-rate frames");
  require(streamed_stats.peak_buffered_frames <=
              aeyla::capture::DmxTakeStreamWriter::kBufferedFrames,
          "streamed capture exceeded bounded queue capacity");

  auto stored = aeyla::capture::load_take_file(stream_config.target_path, error);
  require(stored.has_value(), error.c_str());
  require(stored->song_id == stream_config.song_id,
          "streamed capture lost Song identity");
  require(stored->take.source_ipv4 == stream_config.source_ipv4,
          "streamed capture lost source lock metadata");
  require(stored->take.frames.size() >= 6U,
          "streamed capture file contains too few frames");
  require(stored->take.frames.back() == expected,
          "streamed capture final DMX frame differs from source");

  const auto stats = capture.stats();
  require(stats.invalid_packets == 0U, "valid loopback stream produced invalid packets");
  require(stats.sequence_gaps == 0U, "continuous loopback stream produced sequence gaps");

  output.set_enabled(false);
  output.stop();
  capture.stop();

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);

  std::cout << "Art-Net capture loopback PASS: legacy + streamed 44 Hz paths\n";
  return 0;
}
