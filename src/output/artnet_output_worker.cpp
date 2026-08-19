#include "output/artnet_output_worker.h"

#include "core/artnet_packet.h"

#include <array>
#include <atomic>
#include <chrono>
#include <charconv>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <tuple>
#include <utility>
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
#include <unistd.h>
#endif

namespace aeyla::output {
namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

using OutputLeaseKey = std::tuple<std::string, std::uint16_t, std::uint16_t>;
std::mutex gOutputLeaseMutex;
std::set<OutputLeaseKey> gOutputLeases;

void close_socket(SocketHandle socket) noexcept {
  if (socket == kInvalidSocket) return;
#ifdef _WIN32
  (void) closesocket(socket);
#else
  (void) close(socket);
#endif
}

bool parse_numeric_ipv4(std::string_view text,
                        std::array<std::uint8_t, 4>& octets) noexcept {
  std::size_t begin = 0U;
  for (std::size_t index = 0U; index < octets.size(); ++index) {
    const std::size_t end = index + 1U == octets.size()
                                ? text.size()
                                : text.find('.', begin);
    if (end == std::string_view::npos || end == begin) return false;
    unsigned value = 0U;
    const char* first = text.data() + begin;
    const char* last = text.data() + end;
    const auto parsed = std::from_chars(first, last, value);
    if (parsed.ec != std::errc{} || parsed.ptr != last || value > 255U)
      return false;
    octets[index] = static_cast<std::uint8_t>(value);
    begin = end + 1U;
  }
  return begin == text.size() + 1U;
}

bool valid_unicast_octets(const std::array<std::uint8_t, 4>& octets) noexcept {
  // 0/8 is "this network" and 224/4 includes multicast plus the reserved
  // high range. Neither is a deterministic explicit interface/destination.
  return octets[0] != 0U && octets[0] < 224U;
}

bool valid_config_ranges(const ArtNetOutputConfig& config,
                         std::string& error_message) {
  if (config.target_ipv4.empty()) {
    error_message = "Art-Net target IPv4 address is required";
    return false;
  }
  if (config.udp_port == 0U) {
    error_message = "Art-Net UDP port must be non-zero";
    return false;
  }
  if (config.port_address > 0x7FFFU) {
    error_message = "Art-Net port address exceeds 15 bits";
    return false;
  }
  if (config.channel_count < 2U || config.channel_count > 512U ||
      (config.channel_count % 2U) != 0U) {
    error_message = "ArtDMX channel count must be even and between 2 and 512";
    return false;
  }
  if (config.frames_per_second < 1U || config.frames_per_second > 60U) {
    error_message = "Art-Net refresh rate must be between 1 and 60 FPS";
    return false;
  }
  return true;
}

bool safe_unicast_target(const in_addr& address) noexcept {
  const std::uint32_t host = ntohl(address.s_addr);
  if (host == 0U || host == 0xFFFFFFFFU) return false;
  if ((host & 0xF0000000U) == 0xE0000000U) return false;
  return true;
}

bool validate_optional_source(std::string_view source,
                              std::string& error_message) noexcept {
  if(source.empty()) return true;
  std::array<std::uint8_t, 4> octets{};
  if(!parse_numeric_ipv4(source, octets)) {
    error_message = "Art-Net TX source must be a numeric local IPv4 address";
    return false;
  }
  if(!valid_unicast_octets(octets)) {
    error_message = "Art-Net TX source must be a unicast local IPv4 address";
    return false;
  }
  return true;
}

}  // namespace

bool validate_artnet_output_config(const ArtNetOutputConfig& config,
                                   std::string& error_message) noexcept {
  error_message.clear();
  if (!valid_config_ranges(config, error_message)) return false;
  if (!validate_optional_source(config.source_ipv4, error_message)) return false;

  std::array<std::uint8_t, 4> octets{};
  if (!parse_numeric_ipv4(config.target_ipv4, octets)) {
    error_message = "Art-Net target must be a numeric IPv4 address";
    return false;
  }
  if (!valid_unicast_octets(octets)) {
    error_message = "Art-Net Alpha v1 requires a unicast IPv4 target";
    return false;
  }
  return true;
}

class ArtNetOutputWorker::Impl final {
 public:
  ~Impl() { shutdown(); }

  bool acquire_lease(const ArtNetOutputConfig& next_config,
                     std::string& error_message) {
    const OutputLeaseKey key{
        next_config.target_ipv4, next_config.udp_port, next_config.port_address};
    const std::scoped_lock lock(gOutputLeaseMutex);
    const auto [iterator, inserted] = gOutputLeases.insert(key);
    (void) iterator;
    if (!inserted) {
      error_message =
          "another AEYLA instance already owns this Art-Net target/universe";
      return false;
    }
    lease_key_ = key;
    return true;
  }

  void release_lease() noexcept {
    if (!lease_key_.has_value()) return;
    const std::scoped_lock lock(gOutputLeaseMutex);
    gOutputLeases.erase(*lease_key_);
    lease_key_.reset();
  }

  bool initialize_socket(const ArtNetOutputConfig& next_config,
                         std::string& error_message) {
#ifdef _WIN32
    WSADATA data{};
    const int startup = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup != 0) {
      error_message = "WSAStartup failed with code " + std::to_string(startup);
      return false;
    }
    winsock_started_ = true;
#endif

    sockaddr_in next_target{};
    next_target.sin_family = AF_INET;
    next_target.sin_port = htons(next_config.udp_port);
    if (inet_pton(AF_INET, next_config.target_ipv4.c_str(),
                  &next_target.sin_addr) != 1) {
      error_message = "Art-Net target must be a numeric IPv4 address";
      cleanup_winsock();
      return false;
    }
    if (!safe_unicast_target(next_target.sin_addr)) {
      error_message = "Art-Net Alpha v1 requires a unicast IPv4 target";
      cleanup_winsock();
      return false;
    }

    SocketHandle next_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (next_socket == kInvalidSocket) {
      error_message = "could not create UDP socket";
      cleanup_winsock();
      return false;
    }

    if(!next_config.source_ipv4.empty()) {
      sockaddr_in local{};
      local.sin_family = AF_INET;
      local.sin_port = 0;
      if(inet_pton(AF_INET, next_config.source_ipv4.c_str(),
                   &local.sin_addr) != 1) {
        close_socket(next_socket);
        error_message = "Art-Net TX source IPv4 could not be parsed";
        cleanup_winsock();
        return false;
      }
      if(bind(next_socket, reinterpret_cast<const sockaddr*>(&local),
              sizeof(local)) != 0) {
        close_socket(next_socket);
        error_message = "could not bind Art-Net TX to local IPv4 " +
                        next_config.source_ipv4;
        cleanup_winsock();
        return false;
      }
    }

    socket_ = next_socket;
    target_ = next_target;
    config_ = next_config;
    return true;
  }

  void launch() {
    stop_requested_.store(false, std::memory_order_release);
    enabled_.store(false, std::memory_order_release);
    blackout_pending_.store(false, std::memory_order_release);
    sent_packets_.store(0U, std::memory_order_relaxed);
    blackout_packets_.store(0U, std::memory_order_relaxed);
    send_errors_.store(0U, std::memory_order_relaxed);
    stale_publish_drops_.store(0U, std::memory_order_relaxed);
    last_sent_generation_.store(0U, std::memory_order_relaxed);
    published_generation_.store(0U, std::memory_order_relaxed);
    {
      const std::scoped_lock lock(frame_mutex_);
      latest_.fill(0U);
      latest_generation_ = 0U;
      has_frame_ = false;
    }

    worker_ = std::thread([this]() { run(); });
  }

  void shutdown() noexcept {
    if (worker_.joinable()) {
      const bool was_enabled =
          enabled_.exchange(false, std::memory_order_acq_rel);
      if (was_enabled)
        blackout_pending_.store(true, std::memory_order_release);
      stop_requested_.store(true, std::memory_order_release);
      wake_.notify_all();
      worker_.join();
    } else {
      enabled_.store(false, std::memory_order_release);
      stop_requested_.store(true, std::memory_order_release);
    }

    running_.store(false, std::memory_order_release);
    close_socket(socket_);
    socket_ = kInvalidSocket;
    cleanup_winsock();
    release_lease();
  }

  void publish(const DmxUniverse& universe, std::uint64_t generation) {
    const std::scoped_lock lock(frame_mutex_);
    if (has_frame_ && generation < latest_generation_) {
      stale_publish_drops_.fetch_add(1U, std::memory_order_relaxed);
      return;
    }
    latest_ = universe;
    latest_generation_ = generation;
    has_frame_ = true;
    published_generation_.store(generation, std::memory_order_release);
  }

  void set_enabled(bool next_enabled) noexcept {
    const bool previous = enabled_.exchange(next_enabled, std::memory_order_acq_rel);
    if (previous && !next_enabled)
      blackout_pending_.store(true, std::memory_order_release);
    wake_.notify_all();
  }

  ArtNetOutputStats stats() const noexcept {
    ArtNetOutputStats result;
    result.running = running_.load(std::memory_order_acquire);
    result.enabled = enabled_.load(std::memory_order_acquire);
    result.published_generation =
        published_generation_.load(std::memory_order_relaxed);
    result.last_sent_generation =
        last_sent_generation_.load(std::memory_order_relaxed);
    result.sent_packets = sent_packets_.load(std::memory_order_relaxed);
    result.blackout_packets = blackout_packets_.load(std::memory_order_relaxed);
    result.send_errors = send_errors_.load(std::memory_order_relaxed);
    result.stale_publish_drops =
        stale_publish_drops_.load(std::memory_order_relaxed);
    return result;
  }

 private:
  void cleanup_winsock() noexcept {
#ifdef _WIN32
    if (winsock_started_) {
      (void) WSACleanup();
      winsock_started_ = false;
    }
#endif
  }

  bool snapshot_latest(DmxUniverse& universe,
                       std::uint64_t& generation) {
    const std::scoped_lock lock(frame_mutex_);
    if (!has_frame_) return false;
    universe = latest_;
    generation = latest_generation_;
    return true;
  }

  bool transmit(const DmxUniverse& universe, std::uint64_t generation,
                bool blackout) noexcept {
    try {
      const std::vector<std::uint8_t> packet = make_artdmx_packet(
          universe, config_.port_address, sequence_, config_.channel_count);

#ifdef _WIN32
      const int sent = sendto(
          socket_, reinterpret_cast<const char*>(packet.data()),
          static_cast<int>(packet.size()), 0,
          reinterpret_cast<const sockaddr*>(&target_), sizeof(target_));
      const bool ok = sent == static_cast<int>(packet.size());
#else
      const ssize_t sent = sendto(
          socket_, packet.data(), packet.size(), 0,
          reinterpret_cast<const sockaddr*>(&target_), sizeof(target_));
      const bool ok = sent == static_cast<ssize_t>(packet.size());
#endif

      if (!ok) {
        send_errors_.fetch_add(1U, std::memory_order_relaxed);
        return false;
      }

      sent_packets_.fetch_add(1U, std::memory_order_relaxed);
      if (blackout)
        blackout_packets_.fetch_add(1U, std::memory_order_relaxed);
      else
        last_sent_generation_.store(generation, std::memory_order_relaxed);

      ++sequence_;
      if (sequence_ == 0U) sequence_ = 1U;
      return true;
    } catch (...) {
      send_errors_.fetch_add(1U, std::memory_order_relaxed);
      return false;
    }
  }

  void run() noexcept {
    running_.store(true, std::memory_order_release);
    sequence_ = 1U;

    const auto period = std::chrono::nanoseconds(
        1000000000LL / static_cast<long long>(config_.frames_per_second));
    auto next_deadline = std::chrono::steady_clock::now();
    const DmxUniverse blackout{};

    for (;;) {
      if (blackout_pending_.exchange(false, std::memory_order_acq_rel)) {
        (void) transmit(blackout, 0U, true);
        next_deadline = std::chrono::steady_clock::now() + period;
      }

      if (stop_requested_.load(std::memory_order_acquire)) break;

      const auto now = std::chrono::steady_clock::now();
      if (now < next_deadline) {
        std::unique_lock lock(wake_mutex_);
        wake_.wait_until(lock, next_deadline);
        continue;
      }

      if (enabled_.load(std::memory_order_acquire)) {
        DmxUniverse latest{};
        std::uint64_t generation = 0U;
        if (snapshot_latest(latest, generation))
          (void) transmit(latest, generation, false);
      }

      next_deadline = std::chrono::steady_clock::now() + period;
    }

    running_.store(false, std::memory_order_release);
  }

  ArtNetOutputConfig config_{};
  SocketHandle socket_{kInvalidSocket};
  sockaddr_in target_{};
  std::optional<OutputLeaseKey> lease_key_;
#ifdef _WIN32
  bool winsock_started_{false};
#endif

  std::thread worker_;
  std::condition_variable wake_;
  std::mutex wake_mutex_;
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> enabled_{false};
  std::atomic<bool> blackout_pending_{false};

  mutable std::mutex frame_mutex_;
  DmxUniverse latest_{};
  std::uint64_t latest_generation_{0U};
  bool has_frame_{false};

  std::atomic<std::uint64_t> published_generation_{0U};
  std::atomic<std::uint64_t> last_sent_generation_{0U};
  std::atomic<std::uint64_t> sent_packets_{0U};
  std::atomic<std::uint64_t> blackout_packets_{0U};
  std::atomic<std::uint64_t> send_errors_{0U};
  std::atomic<std::uint64_t> stale_publish_drops_{0U};
  std::uint8_t sequence_{1U};
};

ArtNetOutputWorker::ArtNetOutputWorker() : impl_(std::make_unique<Impl>()) {}

ArtNetOutputWorker::~ArtNetOutputWorker() = default;

bool ArtNetOutputWorker::start(const ArtNetOutputConfig& config,
                               std::string& error_message) {
  error_message.clear();
  if (!validate_artnet_output_config(config, error_message)) return false;

  impl_->shutdown();
  if (!impl_->acquire_lease(config, error_message)) return false;
  if (!impl_->initialize_socket(config, error_message)) {
    impl_->shutdown();
    return false;
  }

  try {
    impl_->launch();
  } catch (...) {
    impl_->shutdown();
    error_message = "could not start Art-Net worker thread";
    return false;
  }
  return true;
}

void ArtNetOutputWorker::stop() noexcept { impl_->shutdown(); }

void ArtNetOutputWorker::publish_latest(const DmxUniverse& universe,
                                        std::uint64_t generation) {
  impl_->publish(universe, generation);
}

void ArtNetOutputWorker::set_enabled(bool enabled) noexcept {
  impl_->set_enabled(enabled);
}

ArtNetOutputStats ArtNetOutputWorker::stats() const noexcept {
  return impl_->stats();
}

}  // namespace aeyla::output
