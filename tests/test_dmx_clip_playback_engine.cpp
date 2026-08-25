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

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::capture;

  const auto directory = unique_test_directory();
  std::string error;
  require(prepare_take_directory(directory, error), error);

  constexpr std::uint64_t kFrames = 44U * 3U;
  DmxTakeStreamConfig stream_config;
  stream_config.target_path = directory / "MidiRelative.aeylatake";
  stream_config.song_id = "song-midi-relative";
  stream_config.song_name = "MIDI Relative";
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

  DmxClipPlaybackEngine engine;
  engine.attach(&output);
  require(engine.load_clip(stream_config.target_path, 48000.0, error), error);
  engine.set_host_heartbeat_ok(true);
  require(engine.arm(error), error);
  require(engine.play_from_start(error), error);

  require(wait_until([&]() {
    return engine.status().current_frame == 0U && output.override_enabled();
  }), "PLAY did not start consolidated clip at frame zero");
  require(wait_until([&]() {
    DmxUniverse received{};
    return receiver.latest_frame(received) && received == expected_frame(0U);
  }), "physical Art-Net path did not receive frame zero");

  // One processed host second advances exactly one relative second regardless
  // of where the DAW Arrangement playhead lives.
  engine.advance_samples(48000U, false);
  require(wait_until([&]() { return engine.status().current_frame == 44U; }),
          "one processed second did not resolve to frame 44");
  require(wait_until([&]() {
    DmxUniverse received{};
    return receiver.latest_frame(received) && received == expected_frame(44U);
  }), "Art-Net output did not follow relative clip cursor");

  // PAUSE freezes the cursor and holds the last DMX frame even while callbacks
  // continue to process more samples.
  require(engine.pause(error), error);
  const auto paused_cursor = engine.status().cursor_samples;
  engine.advance_samples(96000U, false);
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  require(engine.status().cursor_samples == paused_cursor,
          "PAUSE must freeze the relative sample cursor");
  require(engine.status().current_frame == 44U && output.override_enabled(),
          "PAUSE must HOLD the last DMX frame");

  require(engine.resume(error), error);
  engine.advance_samples(24000U, false);
  require(wait_until([&]() { return engine.status().current_frame == 66U; }),
          "RESUME did not continue from the paused cursor");

  // Retrigger is independent from absolute host position: it deterministically
  // returns the consolidated clip to zero.
  require(engine.play_from_start(error), error);
  require(wait_until([&]() { return engine.status().current_frame == 0U; }),
          "RETRIGGER did not return clip to frame zero");

  engine.advance_samples(4U * 48000U, false);
  require(wait_until([&]() {
    const auto status = engine.status();
    return status.transport == DmxClipTransportState::ended &&
           status.current_frame == kFrames - 1U && status.progress == 1.0 &&
           output.override_enabled();
  }), "clip end must HOLD the final source frame");

  // Offline render never emits physical Art-Net and does not consume artistic
  // cursor time.
  require(engine.play_from_start(error), error);
  const auto before_offline = engine.status().cursor_samples;
  engine.advance_samples(48000U, true);
  require(wait_until([&]() { return !output.override_enabled(); }),
          "offline render must inhibit physical DMX output");
  require(engine.status().cursor_samples == before_offline,
          "offline render must not advance the live clip cursor");
  engine.advance_samples(0U, false);
  require(wait_until([&]() { return output.override_enabled(); }),
          "physical output did not recover after offline render ended");

  // Host heartbeat is purely a liveness gate. Losing it disables physical
  // authority without changing the artistic cursor.
  const auto before_dead_host = engine.status().cursor_samples;
  engine.set_host_heartbeat_ok(false);
  require(wait_until([&]() { return !output.override_enabled(); }),
          "dead host heartbeat did not fail closed");
  require(engine.status().cursor_samples == before_dead_host,
          "heartbeat must not alter artistic clip position");

  engine.set_host_heartbeat_ok(true);
  require(wait_until([&]() { return output.override_enabled(); }),
          "clip authority did not recover after host heartbeat returned");

  engine.stop_and_reset();
  require(wait_until([&]() { return !output.override_enabled(); }),
          "STOP/RESET must remove physical clip authority");
  require(engine.status().cursor_samples == 0U,
          "STOP/RESET must return cursor to zero");

  engine.disarm();
  engine.unload();
  output.stop();
  receiver.stop();

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);

  std::cout << "AEYLA DMX clip engine PASS: MIDI-relative play/pause/resume/retrigger\n";
  return EXIT_SUCCESS;
}
