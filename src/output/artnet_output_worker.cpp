#include "output/artnet_output_worker.h"

#include "core/artnet_packet.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <thread>
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

void close_socket(SocketHandle socket) noexcept {
  if (socket == kInvalidSocket) return;
#ifdef _WIN32
  (void) closesocket(socket);
#else
  (void) close(socket);
#endif
}

bool valid_config(const ArtNetOutputConfig& config,
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

}  // namespace

class ArtNetOutputWorker::Impl final {
 public:
  ~Impl() { shutdown(); }

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

    SocketHandle next_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (next_socket == kInvalidSocket) {
      error_message = "could not create UDP socket";
      cleanup_winsock();
      return false;
    }

    sockaddr_in next_target{};
    next_target.sin_family = AF_INET;
    next_target.sin_port = htons(next_config.udp_port);
    if (inet_pton(AF_INET, next_config.target_ipv4.c_str(),
                  &next_target.sin_addr) != 1) {
      close_socket(next_socket);
      error_message = "Art-Net target must be a numeric IPv4 address";
      cleanup_winsock();
      return false;
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
    stop_requested_.store(true, std::memory_order_release);
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
    running_.store(false, std::memory_order_release);
    enabled_.store(false, std::memory_order_release);
    close_socket(socket_);
    socket_ = kInvalidSocket;
    cleanup_winsock();
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

    while (!stop_requested_.load(std::memory_order_acquire)) {
      if (blackout_pending_.exchange(false, std::memory_order_acq_rel)) {
        (void) transmit(blackout, 0U, true);
        next_deadline = std::chrono::steady_clock::now() + period;
      }

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
  if (!valid_config(config, error_message)) return false;

  impl_->shutdown();
  if (!impl_->initialize_socket(config, error_message)) return false;

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
