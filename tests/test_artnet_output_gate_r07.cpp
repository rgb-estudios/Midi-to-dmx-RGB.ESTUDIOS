#include "output/artnet_output_worker.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
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
  if(!condition) {
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

void close_socket_safe(TestSocket socket) {
  if(socket == kInvalidSocket) return;
#ifdef _WIN32
  (void)closesocket(socket);
#else
  (void)close(socket);
#endif
}

struct Receiver {
  TestSocket socket{kInvalidSocket};
  std::uint16_t port{0U};
#ifdef _WIN32
  bool winsock{false};
#endif
  ~Receiver() {
    close_socket_safe(socket);
#ifdef _WIN32
    if(winsock) (void)WSACleanup();
#endif
  }
};

bool open_receiver(Receiver& receiver) {
#ifdef _WIN32
  WSADATA data{};
  if(WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
  receiver.winsock = true;
#endif
  receiver.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(receiver.socket == kInvalidSocket) return false;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(0U);
  if(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) return false;
  if(bind(receiver.socket, reinterpret_cast<const sockaddr*>(&address),
          sizeof(address)) != 0) return false;
  sockaddr_in bound{};
#ifdef _WIN32
  int bound_size = sizeof(bound);
#else
  socklen_t bound_size = sizeof(bound);
#endif
  if(getsockname(receiver.socket, reinterpret_cast<sockaddr*>(&bound),
                 &bound_size) != 0) return false;
  receiver.port = ntohs(bound.sin_port);
#ifdef _WIN32
  const DWORD timeout_ms = 1500U;
  return setsockopt(receiver.socket, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout_ms),
                    sizeof(timeout_ms)) == 0;
#else
  timeval timeout{};
  timeout.tv_sec = 1;
  timeout.tv_usec = 500000;
  return setsockopt(receiver.socket, SOL_SOCKET, SO_RCVTIMEO,
                    &timeout, sizeof(timeout)) == 0;
#endif
}

std::vector<std::uint8_t> receive_packet(TestSocket socket) {
  std::vector<std::uint8_t> bytes(1024U, 0U);
#ifdef _WIN32
  const int received = recvfrom(socket, reinterpret_cast<char*>(bytes.data()),
                                static_cast<int>(bytes.size()), 0, nullptr, nullptr);
  if(received <= 0) return {};
  bytes.resize(static_cast<std::size_t>(received));
#else
  const ssize_t received = recvfrom(socket, bytes.data(), bytes.size(),
                                    0, nullptr, nullptr);
  if(received <= 0) return {};
  bytes.resize(static_cast<std::size_t>(received));
#endif
  return bytes;
}

bool is_artdmx(const std::vector<std::uint8_t>& packet) {
  static constexpr std::array<std::uint8_t, 8> id = {
      'A', 'r', 't', '-', 'N', 'e', 't', 0U};
  return packet.size() >= 18U &&
         std::equal(id.begin(), id.end(), packet.begin()) &&
         packet[8] == 0x00U && packet[9] == 0x50U;
}

bool payload_zero(const std::vector<std::uint8_t>& packet) {
  if(!is_artdmx(packet)) return false;
  return std::all_of(packet.begin() + 18, packet.end(),
                     [](std::uint8_t value) { return value == 0U; });
}

template <typename Predicate>
bool wait_until(Predicate&& predicate,
                std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while(std::chrono::steady_clock::now() < deadline) {
    if(predicate()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return predicate();
}

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::output;

  std::string error;
  ArtNetOutputConfig validation;
  validation.target_ipv4 = "2.255.255.255";
  check(validate_artnet_output_config(validation, error),
        "directed subnet broadcast must pass preflight");
  validation.target_ipv4 = "255.255.255.255";
  check(!validate_artnet_output_config(validation, error),
        "limited broadcast must remain rejected");

  Receiver receiver;
  check(open_receiver(receiver), "loopback receiver must open");
  if(receiver.socket == kInvalidSocket || receiver.port == 0U)
    return EXIT_FAILURE;

  ArtNetOutputWorker worker;
  ArtNetOutputConfig config;
  config.target_ipv4 = "127.0.0.1";
  config.udp_port = receiver.port;
  config.frames_per_second = 30U;  // ruta heredada deliberada
  check(worker.start(config, error), "worker must start: " + error);
  check(worker.stats().configured_fps == kAeylaArtNetFramesPerSecond,
        "R07 must normalize every TX route to 44 Hz");

  DmxUniverse live{};
  live[0] = 200U;
  live[1] = 111U;
  worker.publish_latest(live, 1U);
  worker.set_enabled(true);

  bool observed_live = false;
  for(int attempt = 0; attempt < 5; ++attempt) {
    const auto packet = receive_packet(receiver.socket);
    if(is_artdmx(packet) && packet.size() > 19U &&
       packet[18] == 200U && packet[19] == 111U) {
      observed_live = true;
      break;
    }
  }
  check(observed_live, "44 Hz TX must deliver the latest DMX frame");

  worker.set_enabled(false);
  int blackout_count = 0;
  for(int attempt = 0; attempt < 10 &&
      blackout_count < static_cast<int>(kAeylaArtNetBlackoutBurstFrames); ++attempt) {
    const auto packet = receive_packet(receiver.socket);
    if(payload_zero(packet)) ++blackout_count;
  }
  check(blackout_count >= static_cast<int>(kAeylaArtNetBlackoutBurstFrames),
        "disarm must emit the complete multi-frame BLACKOUT burst");

  // UDP delivery can unblock this receiver before the worker publishes the
  // corresponding atomic counters. Wait for that bounded telemetry handoff;
  // the packet-level burst requirement above remains unchanged.
  check(wait_until(
            [&worker]() {
              return worker.stats().blackout_packets >=
                     kAeylaArtNetBlackoutBurstFrames;
            },
            std::chrono::seconds(2)),
        "BLACKOUT telemetry must converge after the complete UDP burst");

  const auto after_blackout = worker.stats();
  check(after_blackout.blackout_packets >= kAeylaArtNetBlackoutBurstFrames,
        "BLACKOUT burst must be visible in telemetry");
  check(after_blackout.send_errors == 0U,
        "loopback TX gate must complete without send errors");
  check(!after_blackout.fail_closed,
        "healthy loopback must not enter fail-closed");

  worker.stop();
  const auto stopped = worker.stats();
  check(!stopped.running && !stopped.enabled && !stopped.override_enabled,
        "stop must leave all Art-Net authorities disabled");

  if(failures == 0) {
    std::cout << "All AEYLA Art-Net R07 gate tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
