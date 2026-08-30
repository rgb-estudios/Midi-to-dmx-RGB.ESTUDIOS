#include "AeylaLiveMemorySession.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

void require(bool condition, const std::string& message) {
  if(condition) return;
  std::cerr << "FAILED: " << message << '\n';
  std::exit(EXIT_FAILURE);
}

template <typename Predicate>
bool wait_until(Predicate predicate,
                std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while(std::chrono::steady_clock::now() < deadline) {
    if(predicate()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return predicate();
}

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::capture;
  using namespace aeyla::output;
  using namespace aeyla::live_memory_session;

  constexpr std::uint16_t kAvolitesPort = 17673U;
  constexpr std::uint16_t kAeylaTxPort = 17674U;

  std::string error;

  ArtNetCaptureWorker aeylaRx;
  ArtNetCaptureConfig rxConfig;
  rxConfig.listen_ipv4 = "127.0.0.1";
  rxConfig.udp_port = kAvolitesPort;
  rxConfig.port_address = 0U;
  rxConfig.frames_per_second = 44U;
  require(aeylaRx.start(rxConfig, error), error);

  ArtNetOutputWorker avolites;
  ArtNetOutputConfig avolitesConfig;
  avolitesConfig.source_ipv4 = "127.0.0.1";
  avolitesConfig.target_ipv4 = "127.0.0.1";
  avolitesConfig.udp_port = kAvolitesPort;
  avolitesConfig.port_address = 0U;
  avolitesConfig.channel_count = 512U;
  require(avolites.start(avolitesConfig, error), error);

  ArtNetCaptureWorker sink;
  ArtNetCaptureConfig sinkConfig = rxConfig;
  sinkConfig.udp_port = kAeylaTxPort;
  require(sink.start(sinkConfig, error), error);

  ArtNetOutputWorker aeylaTx;
  ArtNetOutputConfig txConfig = avolitesConfig;
  txConfig.udp_port = kAeylaTxPort;
  require(aeylaTx.start(txConfig, error), error);

  DmxUniverse songBase{};
  songBase[0] = 10U;
  songBase[1] = 30U;
  songBase[2] = 50U;
  songBase[100] = 77U;
  aeylaTx.publish_latest(songBase, 1U);
  aeylaTx.prepare_explicit_rearm();
  aeylaTx.set_enabled(true);

  int owner = 0;
  register_runtime(&owner, &aeylaTx, &aeylaRx);

  // Avolites baseline may contain unrelated values and does NOT need to match
  // the song currently emitted by AEYLA. This is the edge that makes a simple
  // Avolites-vs-song diff unsafe.
  DmxUniverse avolitesOff{};
  avolitesOff[100] = 5U;
  avolites.publish_latest(avolitesOff, 1U);
  avolites.set_enabled(true);
  require(wait_until([&]() { return aeylaRx.stats().packets_accepted >= 2U; }),
          "AEYLA RX did not receive Avolites baseline");

  const auto first = learn_from_avolites(&owner, 0U);
  require(first.succeeded, first.message);
  require(view(&owner, 0U).learning,
          "first learn press must retain the OFF baseline as step 1/2");

  DmxUniverse avolitesOn = avolitesOff;
  avolitesOn[0] = 200U;
  avolitesOn[2] = 100U;
  const auto acceptedBefore = aeylaRx.stats().packets_accepted;
  avolites.publish_latest(avolitesOn, 2U);
  require(wait_until([&]() {
    return aeylaRx.stats().packets_accepted >= acceptedBefore + 2U;
  }), "AEYLA RX did not receive Avolites ON state");

  const auto second = learn_from_avolites(&owner, 0U);
  require(second.succeeded, second.message);
  const auto learned = view(&owner, 0U);
  require(learned.configured && !learned.learning,
          "second learn press must finish the memory");
  require(learned.channel_count == 2U,
          "two-snapshot learn must own only the two channels changed in Avolites");

  const auto toggled = toggle(&owner, 0U);
  require(toggled.succeeded, toggled.message);
  require(wait_until([&]() {
    const auto memory = view(&owner, 0U);
    return memory.level > 0.99F && sink.stats().packets_accepted > 20U;
  }, std::chrono::milliseconds(1800)),
          "FRONTAL memory did not finish its 1 s fade");

  DmxUniverse composed{};
  require(sink.latest_frame(composed), "sink did not expose AEYLA live frame");
  require(composed[0] == 200U && composed[2] == 100U,
          "learned channels did not reach Avolites targets");
  require(composed[1] == songBase[1],
          "unrelated song channel was overwritten by learned memory");
  require(composed[100] == songBase[100],
          "unrelated Avolites baseline value leaked into memory mask");

  clear(&owner);
  aeylaTx.publish_latest(songBase, 2U);
  require(wait_until([&]() {
    DmxUniverse frame{};
    return sink.latest_frame(frame) && frame == songBase;
  }), "clearing project memory state did not restore pure song base");

  aeylaTx.set_enabled(false);
  avolites.set_enabled(false);
  aeylaTx.stop();
  avolites.stop();
  sink.stop();
  aeylaRx.stop();

  std::cout << "AEYLA live-memory session PASS: Avolites OFF/ON learns sparse channels only\n";
  return EXIT_SUCCESS;
}
