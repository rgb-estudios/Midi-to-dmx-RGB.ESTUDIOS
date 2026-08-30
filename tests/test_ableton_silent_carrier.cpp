#include "capture/dmx_take_file_store.h"
#include "capture/dmx_take_scheduler.h"
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
         ("aeyla-ableton-silent-carrier-" + std::to_string(stamp));
}

aeyla::DmxUniverse fixture_frame(std::uint64_t index) {
  aeyla::DmxUniverse frame{};
  frame[0] = static_cast<std::uint8_t>((index + 3U) & 0xFFU);
  frame[127] = static_cast<std::uint8_t>((index * 7U + 11U) & 0xFFU);
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
  stream.target_path = directory / "AbletonSilentCarrier.aeylatake";
  stream.song_id = "song-ableton-silent-carrier";
  stream.song_name = "Ableton Silent Carrier";
  stream.take_name = "Take 01";
  stream.source_ipv4 = "2.0.0.10";
  stream.port_address = 0U;
  stream.frames_per_second = 44U;

  DmxTakeStreamWriter writer;
  require(writer.start(stream, error), error);
  for(std::uint64_t index = 0U; index < 88U; ++index)
    require(writer.try_push_frame(fixture_frame(index)),
            "writer rejected Ableton carrier fixture frame");
  require(writer.finalize(error), error);

  output::ArtNetOutputWorker output;
  output::ArtNetOutputConfig output_config;
  output_config.source_ipv4 = "127.0.0.1";
  output_config.target_ipv4 = "127.0.0.1";
  output_config.udp_port = 17651U;
  output_config.port_address = 0U;
  output_config.channel_count = 512U;
  output_config.frames_per_second = 44U;
  require(output.start(output_config, error), error);

  runtime::HostTransportMailbox host;
  // One valid stopped callback is enough to prove the host instance existed.
  // Ableton may then suspend DSP for a silent device while stopped.
  host.publish(false, false, 0.0, 0.0, 120.0);

  DmxTakeScheduler scheduler;
  scheduler.attach(&output, &host);
  require(scheduler.load_take_file(stream.target_path, 48000.0, error), error);
  require(scheduler.arm(error), error);
  require(wait_until([&]() {
    const auto status = scheduler.status();
    return status.armed && status.file_backed && status.hold_valid &&
           output.override_enabled();
  }), "scheduler did not establish armed stopped carrier");

  const auto packets_before_silence = output.stats().sent_packets;
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  const auto stopped = scheduler.status();
  require(stopped.armed,
          "Ableton-style stopped DSP suspension incorrectly disarmed carrier");
  require(!stopped.host_heartbeat_ok,
          "fixture did not age the host heartbeat beyond timeout");
  require(output.override_enabled(),
          "stopped carrier disappeared when host callbacks became stale");
  require(output.stats().sent_packets >= packets_before_silence + 20U,
          "Art-Net carrier did not continue while stopped callbacks were stale");

  // A fresh running callback re-establishes the sample clock. Playback owns a
  // relative sample cursor, while ARM owns physical Art-Net authority.
  host.publish(true, false, 0.0, 0.0, 120.0);
  require(wait_until([&]() { return scheduler.status().host_heartbeat_ok; }),
          "fresh host callback did not restore heartbeat");
  require(scheduler.play(error, DmxClipClockSource::host_samples), error);
  scheduler.advance_samples(4800U, false);
  require(wait_until([&]() { return scheduler.status().playing; }),
          "sample-clock playback did not enter PLAYING");

  // This is the live-show regression that R09.1 did not cover: PLAY -> DAW
  // STOP/PAUSE -> suspended callbacks -> DAW PLAY, repeated. RUNNING=false is
  // an intentional clock hold, not a host failure. The cursor must freeze and
  // the Art-Net endpoint must remain leased and continuously transmitting.
  for(int cycle = 0; cycle < 3; ++cycle) {
    const auto before_stop = scheduler.status();
    const auto packets_before_stop = output.stats().sent_packets;
    host.publish(false, false,
                 static_cast<double>((cycle + 1) * 4800),
                 0.0, 120.0);

    std::this_thread::sleep_for(std::chrono::milliseconds(850));
    const auto held = scheduler.status();
    require(held.armed,
            "intentional DAW STOP/PAUSE disarmed the Take authority");
    require(!held.host_heartbeat_ok,
            "stopped-host fixture did not age the literal heartbeat");
    require(output.override_enabled(),
            "Art-Net override disappeared during intentional DAW STOP/PAUSE");
    require(held.current_frame == before_stop.current_frame,
            "DMX cursor moved while the DAW sample clock was stopped");
    require(output.stats().sent_packets >= packets_before_stop + 15U,
            "Art-Net carrier stopped retransmitting the held frame");

    host.publish(true, false,
                 static_cast<double>((cycle + 1) * 4800),
                 0.0, 120.0);
    require(wait_until([&]() { return scheduler.status().host_heartbeat_ok; }),
            "DAW PLAY did not restore the sample-clock heartbeat");
    scheduler.advance_samples(2400U, false);
    require(wait_until([&]() {
      return scheduler.status().current_frame > held.current_frame;
    }), "DMX cursor did not continue after DAW PLAY resumed");
    require(scheduler.status().armed && output.override_enabled(),
            "DAW PLAY required an illegal manual Art-Net re-arm");
  }

  // Distinguish an intentional stopped host from a real failure. Here the last
  // authoritative snapshot still says RUNNING=true and callbacks disappear;
  // the sample clock is genuinely lost, so fail-closed remains mandatory.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  require(wait_until([&]() { return !scheduler.status().armed; }),
          "PLAYING sample-clock take did not fail closed after real heartbeat loss");
  require(!output.override_enabled(),
          "Art-Net override remained authoritative after PLAY clock loss");

  scheduler.disarm();
  output.stop();
  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);

  std::cout << "AEYLA silent-carrier PASS: repeated DAW STOP/PLAY keeps carrier; real PLAY heartbeat loss fails closed\n";
  return EXIT_SUCCESS;
}
