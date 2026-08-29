#include "capture/artnet_capture_worker.h"
#include "capture/dmx_clip_playback_engine.h"
#include "capture/dmx_take_file_store.h"
#include "capture/dmx_take_stream_writer.h"
#include "output/artnet_output_worker.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {

void require(bool condition, const std::string& message) {
  if(!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

template <typename Predicate>
bool wait_until(Predicate predicate,
                std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while(std::chrono::steady_clock::now() < deadline) {
    if(predicate()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return predicate();
}

std::filesystem::path unique_test_directory() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("aeyla-armed-carrier-" + std::to_string(stamp));
}

aeyla::DmxUniverse expected_frame(std::uint64_t index) {
  aeyla::DmxUniverse frame{};
  frame[0] = static_cast<std::uint8_t>((index + 1U) & 0xFFU);
  frame[42] = static_cast<std::uint8_t>((index * 5U + 9U) & 0xFFU);
  frame[511] = static_cast<std::uint8_t>((index * 11U + 3U) & 0xFFU);
  return frame;
}

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::capture;

  const auto directory = unique_test_directory();
  std::string error;
  require(prepare_take_directory(directory, error), error);

  DmxTakeStreamConfig stream;
  stream.target_path = directory / "ArmedCarrier.aeylatake";
  stream.song_id = "song-armed-carrier";
  stream.song_name = "Armed Carrier";
  stream.take_name = "Take 01";
  stream.source_ipv4 = "2.0.0.10";
  stream.port_address = 0U;
  stream.frames_per_second = 44U;

  DmxTakeStreamWriter writer;
  require(writer.start(stream, error), error);
  for(std::uint64_t index = 0U; index < 88U; ++index)
    require(writer.try_push_frame(expected_frame(index)),
            "writer rejected carrier fixture frame");
  require(writer.finalize(error), error);

  constexpr std::uint16_t kUdpPort = 17649U;
  ArtNetCaptureWorker receiver;
  ArtNetCaptureConfig receive_config;
  receive_config.listen_ipv4 = "127.0.0.1";
  receive_config.udp_port = kUdpPort;
  receive_config.port_address = 0U;
  receive_config.frames_per_second = 44U;
  require(receiver.start(receive_config, error), error);
  require(wait_until([&]() { return receiver.stats().running; }),
          "Art-Net receiver did not start");

  output::ArtNetOutputWorker output;
  output::ArtNetOutputConfig output_config;
  output_config.source_ipv4 = "127.0.0.1";
  output_config.target_ipv4 = "127.0.0.1";
  output_config.udp_port = kUdpPort;
  output_config.port_address = 0U;
  output_config.channel_count = 512U;
  output_config.frames_per_second = 44U;
  require(output.start(output_config, error), error);

  DmxClipPlaybackEngine engine;
  engine.attach(&output);
  require(engine.load_clip(stream.target_path, 48000.0, error), error);
  engine.set_host_heartbeat_ok(true);

  // Field regression: before this fix ARM only changed an internal bool. The
  // Art-Net receiver stayed silent until PLAY changed transport out of READY.
  require(engine.arm(error), error);
  require(wait_until([&]() {
    const auto status = engine.status();
    return status.armed && status.transport == DmxClipTransportState::ready &&
           status.current_frame == 0U && status.hold_valid &&
           output.override_enabled();
  }), "ARM did not establish Art-Net authority before PLAY");
  require(wait_until([&]() {
    DmxUniverse received{};
    return receiver.latest_frame(received) && received == expected_frame(0U);
  }), "receiver did not get held frame zero immediately after ARM");

  const auto armed_packets_before = output.stats().sent_packets;
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  const auto armed_packets = output.stats().sent_packets - armed_packets_before;
  require(armed_packets >= 8U,
          "ARM with stopped transport did not maintain a continuous Art-Net carrier");

  // Move away from zero, then RESET while still armed. RESET is artistic
  // transport; it must return to frame zero without making the endpoint vanish.
  require(engine.play_from_start(DmxClipClockSource::host_samples, error), error);
  engine.advance_samples(24000U, false);
  require(wait_until([&]() { return engine.status().current_frame == 22U; }),
          "playback fixture did not advance to frame 22");
  engine.stop_and_reset();
  require(wait_until([&]() {
    const auto status = engine.status();
    return status.armed && status.transport == DmxClipTransportState::ready &&
           status.cursor_samples == 0U && status.current_frame == 0U &&
           status.hold_valid && output.override_enabled();
  }), "RESET dropped armed Art-Net authority or failed to return to frame zero");
  require(wait_until([&]() {
    DmxUniverse received{};
    return receiver.latest_frame(received) && received == expected_frame(0U);
  }), "RESET did not republish held frame zero");

  const auto reset_packets_before = output.stats().sent_packets;
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  require(output.stats().sent_packets >= reset_packets_before + 8U,
          "RESET did not preserve the continuous Art-Net carrier");

  // Physical authority is intentionally removed only by DISARM (or the global
  // BLACKOUT/safety paths above this engine).
  engine.disarm();
  require(wait_until([&]() { return !output.override_enabled(); }),
          "DISARM did not remove physical Art-Net authority");

  engine.unload();
  output.stop();
  receiver.stop();

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);

  std::cout << "AEYLA armed carrier PASS: ARM/RESET hold 44 Hz authority until DISARM\n";
  return EXIT_SUCCESS;
}
