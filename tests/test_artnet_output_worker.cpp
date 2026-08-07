#include "output/artnet_output_worker.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

#ifdef _WIN32
using TestSocket = SOCKET;
constexpr TestSocket kInvalidSocket = INVALID_SOCKET;
#else
using TestSocket = int;
constexpr TestSocket kInvalidSocket = -1;
#endif

void close_test_socket(TestSocket socket) {
  if (socket == kInvalidSocket) return;
#ifdef _WIN32
  (void) closesocket(socket);
#else
  (void) close(socket);
#endif
}

struct UdpReceiver {
  TestSocket socket{kInvalidSocket};
  std::uint16_t port{0U};
#ifdef _WIN32
  bool winsock_started{false};
#endif

  ~UdpReceiver() {
    close_test_socket(socket);
#ifdef _WIN32
    if (winsock_started) (void) WSACleanup();
#endif
  }
};

bool open_receiver(UdpReceiver& receiver) {
#ifdef _WIN32
  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
  receiver.winsock_started = true;
#endif

  receiver.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (receiver.socket == kInvalidSocket) return false;

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(0U);
  if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) return false;
  if (bind(receiver.socket, reinterpret_cast<const sockaddr*>(&address),
           sizeof(address)) != 0)
    return false;

  sockaddr_in bound{};
#ifdef _WIN32
  int bound_size = sizeof(bound);
#else
  socklen_t bound_size = sizeof(bound);
#endif
  if (getsockname(receiver.socket, reinterpret_cast<sockaddr*>(&bound),
                  &bound_size) != 0)
    return false;
  receiver.port = ntohs(bound.sin_port);

#ifdef _WIN32
  const DWORD timeout_ms = 2000U;
  if (setsockopt(receiver.socket, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeout_ms),
                 sizeof(timeout_ms)) != 0)
    return false;
#else
  timeval timeout{};
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  if (setsockopt(receiver.socket, SOL_SOCKET, SO_RCVTIMEO,
                 &timeout, sizeof(timeout)) != 0)
    return false;
#endif
  return true;
}

std::vector<std::uint8_t> receive_packet(TestSocket socket) {
  std::vector<std::uint8_t> buffer(1024U, 0U);
#ifdef _WIN32
  const int received = recvfrom(
      socket, reinterpret_cast<char*>(buffer.data()),
      static_cast<int>(buffer.size()), 0, nullptr, nullptr);
  if (received <= 0) return {};
  buffer.resize(static_cast<std::size_t>(received));
#else
  const ssize_t received = recvfrom(
      socket, buffer.data(), buffer.size(), 0, nullptr, nullptr);
  if (received <= 0) return {};
  buffer.resize(static_cast<std::size_t>(received));
#endif
  return buffer;
}

bool is_artdmx(const std::vector<std::uint8_t>& packet) {
  static constexpr std::array<std::uint8_t, 8> id = {
      'A', 'r', 't', '-', 'N', 'e', 't', 0U};
  return packet.size() >= 20U &&
         std::equal(id.begin(), id.end(), packet.begin()) &&
         packet[8] == 0x00U && packet[9] == 0x50U &&
         packet[10] == 0x00U && packet[11] == 0x0EU;
}

bool payload_is_zero(const std::vector<std::uint8_t>& packet) {
  if (packet.size() < 18U) return false;
  for (std::size_t index = 18U; index < packet.size(); ++index) {
    if (packet[index] != 0U) return false;
  }
  return true;
}

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::output;

  UdpReceiver receiver;
  check(open_receiver(receiver), "loopback UDP receiver must open");
  if (receiver.socket == kInvalidSocket || receiver.port == 0U)
    return EXIT_FAILURE;

  ArtNetOutputWorker worker;
  ArtNetOutputConfig config;
  config.target_ipv4 = "127.0.0.1";
  config.udp_port = receiver.port;
  config.port_address = 0U;
  config.channel_count = 512U;
  config.frames_per_second = 30U;

  std::string error;
  check(worker.start(config, error),
        "Art-Net worker must start on a numeric loopback IPv4 target: " + error);

  // Publish a burst before enabling output. The worker must not queue 100
  // historical frames; its first emitted frame must be the latest generation.
  for (std::uint64_t generation = 1U; generation <= 100U; ++generation) {
    DmxUniverse frame{};
    frame[0] = static_cast<std::uint8_t>(generation);
    frame[1] = static_cast<std::uint8_t>(255U - generation);
    worker.publish_latest(frame, generation);
  }

  worker.set_enabled(true);
  const auto first = receive_packet(receiver.socket);
  check(is_artdmx(first), "first worker datagram must be a valid ArtDMX packet");
  check(first.size() == 530U,
        "512-channel ArtDMX packet must contain 18-byte header plus 512 slots");
  check(first.size() > 19U && first[18] == 100U && first[19] == 155U,
        "first enabled packet must contain generation 100, not queued history");

  // A stale producer frame must never move the mailbox backwards.
  DmxUniverse stale{};
  stale[0] = 50U;
  worker.publish_latest(stale, 50U);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  check(worker.stats().stale_publish_drops == 1U,
        "stale generation publish must be rejected explicitly");
  check(worker.stats().published_generation == 100U,
        "stale publish must not replace the latest generation");

  DmxUniverse next{};
  next[0] = 101U;
  next[1] = 77U;
  worker.publish_latest(next, 101U);

  bool observed_101 = false;
  for (int attempt = 0; attempt < 4; ++attempt) {
    const auto packet = receive_packet(receiver.socket);
    if (is_artdmx(packet) && packet.size() > 19U &&
        packet[18] == 101U && packet[19] == 77U) {
      observed_101 = true;
      break;
    }
  }
  check(observed_101,
        "worker must refresh the newly published latest frame at fixed cadence");

  worker.set_enabled(false);
  bool observed_blackout = false;
  for (int attempt = 0; attempt < 4; ++attempt) {
    const auto packet = receive_packet(receiver.socket);
    if (is_artdmx(packet) && payload_is_zero(packet)) {
      observed_blackout = true;
      break;
    }
  }
  check(observed_blackout,
        "disabling output must emit a zero-DMX ArtDMX safety packet");

  const auto before_stop = worker.stats();
  check(before_stop.sent_packets >= 3U,
        "worker must report emitted ArtDMX packets");
  check(before_stop.blackout_packets >= 1U,
        "worker must account for explicit blackout transmission");
  check(before_stop.last_sent_generation == 101U,
        "worker stats must identify the latest non-blackout generation sent");
  check(before_stop.send_errors == 0U,
        "loopback campaign must complete without UDP send errors");

  worker.stop();
  const auto stopped = worker.stats();
  check(!stopped.running && !stopped.enabled,
        "stop must join the network thread and leave output disabled");

  ArtNetOutputWorker invalid_worker;
  ArtNetOutputConfig invalid = config;
  invalid.target_ipv4 = "localhost";
  error.clear();
  check(!invalid_worker.start(invalid, error),
        "runtime must reject DNS hostnames and require numeric IPv4");

  if (failures == 0) {
    std::cout << "All AEYLA Art-Net output worker tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
