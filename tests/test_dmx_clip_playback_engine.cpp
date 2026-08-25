#include "capture/artnet_capture_worker.h"
#include "capture/dmx_clip_playback_engine.h"
#include "capture/dmx_take_file_store.h"
#include "capture/dmx_take_stream_writer.h"
#include "output/artnet_output_worker.h"
#include "runtime/host_transport_mailbox.h"

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
    std::exit(1);
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
         ("aeyla-dmx-clip-engine-" + std::to_string(stamp));
}

aeyla::DmxUniverse expected_frame(std::uint64_t index) {
  aeyla::DmxUniverse frame{};
  frame[0] = static_cast<std::uint8_t>((index + 1U) & 0xFFU);
  frame[20] = static_cast<std::uint8_t>((index * 3U + 7U) & 0xFFU);
  frame[511] = static_cast<std::uint8_t>((index * 13U + 5U) & 0xFFU);
  return frame;
}

void publish_host(aeyla::runtime::HostTransportMailbox& host,
                  bool running,
                  bool offline,
                  std::int64_t sample) {
  host.publish(running, offline, static_cast<double>(sample), 0.0, 120.0);
}

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::capture;

  const auto directory = unique_test_directory();
  std::string error;
  require(prepare_take_directory(directory, error), error);

  constexpr std::uint64_t kFrames = 44U * 3U;
  DmxTakeStreamConfig stream_config;
  stream_config.target_path = directory / "SampleLocked.aeylatake";
  stream_config.song_id = "song-sample-lock";
  stream_config.song_name = "Sample Lock";
  stream_config.take_name = "Take 01";
  stream_config.source_ipv4 = "2.0.0.10";
  stream_config.port_address = 0U;
  stream_config.frames_per_second = 44U;

  DmxTakeStreamWriter writer;
  require(writer.start(stream_config, error), error);
  for(std::uint64_t index = 0U; index < kFrames; ++index)
    require(writer.try_push_frame(expected_frame(index)),
            "writer rejected DMX clip fixture frame");
  require(writer.finalize(error), error);

  constexpr std::uint16_t kUdpPort = 17645U;
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

  runtime::HostTransportMailbox host;
  DmxClipPlaybackEngine engine;
  engine.attach(&output, &host);
  require(engine.load_clip(stream_config.target_path, 48000.0, error), error);
  require(engine.trigger_at_sample(1000, error), error);
  require(engine.arm(error), error);

  publish_host(host, true, false, 1000);
  require(wait_until([&]() {
    return engine.status().current_frame == 0U && output.override_enabled();
  }), "clip start did not resolve to frame zero");
  require(wait_until([&]() {
    DmxUniverse received{};
    return receiver.latest_frame(received) && received == expected_frame(0U);
  }), "physical Art-Net path did not receive frame zero");

  publish_host(host, true, false, 1000 + 48000);
  require(wait_until([&]() { return engine.status().current_frame == 44U; }),
          "one host second did not resolve to frame 44");
  require(wait_until([&]() {
    DmxUniverse received{};
    return receiver.latest_frame(received) && received == expected_frame(44U);
  }), "Art-Net output did not follow one-second host seek");

  // Backward seek is the critical no-drift property.
  publish_host(host, true, false, 1000 + 24000);
  require(wait_until([&]() { return engine.status().current_frame == 22U; }),
          "backward host seek did not reconstruct frame 22");
  require(wait_until([&]() {
    DmxUniverse received{};
    return receiver.latest_frame(received) && received == expected_frame(22U);
  }), "physical output did not follow backward DMX seek");

  publish_host(host, false, false, 1000 + 24000);
  require(wait_until([&]() {
    const auto status = engine.status();
    return !status.playing && status.hold_valid && output.override_enabled();
  }), "DAW STOP must HOLD the last DMX clip frame");

  // Move while stopped: HOLD must remain authoritative until transport runs.
  publish_host(host, false, false, 1000 + 96000);
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  require(engine.status().current_frame == 22U,
          "stopped playhead movement must not destroy HOLD state");

  publish_host(host, true, false, 1000 + 96000);
  require(wait_until([&]() { return engine.status().current_frame == 88U; }),
          "transport restart at new absolute sample did not reconstruct frame 88");

  publish_host(host, true, false, 1000 + 4 * 48000);
  require(wait_until([&]() {
    const auto status = engine.status();
    return !status.playing && status.current_frame == kFrames - 1U &&
           status.progress == 1.0;
  }), "position after clip must HOLD the final source frame");

  publish_host(host, true, true, 1000 + 48000);
  require(wait_until([&]() {
    const auto status = engine.status();
    return status.rendering_offline && !output.override_enabled();
  }), "offline render must inhibit physical DMX clip output");

  publish_host(host, true, false, 1000 + 48000);
  require(wait_until([&]() { return output.override_enabled(); }),
          "physical output did not recover after offline render ended");

  // A dead host callback must fail closed. Wall time is used only for liveness,
  // never to advance the artistic frame index.
  require(wait_until([&]() {
    return !engine.status().host_heartbeat_ok && !output.override_enabled();
  }, std::chrono::milliseconds(1200)),
          "stale host heartbeat did not disable physical clip authority");

  engine.disarm();
  require(!output.override_enabled(), "explicit DISARM must disable Take authority");
  engine.unload();
  output.stop();
  receiver.stop();

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);

  std::cout << "AEYLA DMX clip engine PASS: sample-lock, seek, HOLD, offline, heartbeat\n";
  return EXIT_SUCCESS;
}
