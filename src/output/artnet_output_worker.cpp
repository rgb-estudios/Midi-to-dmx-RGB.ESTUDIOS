#include "output/artnet_output_worker.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <charconv>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <tuple>
#include <utility>

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

constexpr std::size_t kArtDmxHeaderBytes = 18U;
constexpr std::size_t kMaximumArtDmxPacketBytes = kArtDmxHeaderBytes + 512U;

void close_socket(SocketHandle socket) noexcept {
  if(socket == kInvalidSocket) return;
#ifdef _WIN32
  (void)closesocket(socket);
#else
  (void)close(socket);
#endif
}

bool parse_numeric_ipv4(std::string_view text,
                        std::array<std::uint8_t, 4>& octets) noexcept {
  if(text.empty()) return false;
  std::size_t begin = 0U;
  for(std::size_t index = 0U; index < octets.size(); ++index) {
    const std::size_t end = index + 1U == octets.size()
                                ? text.size()
                                : text.find('.', begin);
    if(end == std::string_view::npos || end == begin) return false;
    unsigned value = 0U;
    const char* first = text.data() + begin;
    const char* last = text.data() + end;
    const auto parsed = std::from_chars(first, last, value);
    if(parsed.ec != std::errc{} || parsed.ptr != last || value > 255U)
      return false;
    octets[index] = static_cast<std::uint8_t>(value);
    begin = end + 1U;
  }
  return begin == text.size() + 1U;
}

bool is_limited_broadcast(const std::array<std::uint8_t, 4>& octets) noexcept {
  return std::all_of(octets.begin(), octets.end(),
                     [](std::uint8_t value) { return value == 255U; });
}

bool valid_artnet_ipv4(const std::array<std::uint8_t, 4>& octets) noexcept {
  if(octets[0] == 0U || octets[0] >= 224U) return false;
  return !is_limited_broadcast(octets);
}

bool valid_config_ranges(const ArtNetOutputConfig& config,
                         std::string& error_message) {
  if(config.target_ipv4.empty()) {
    error_message = "Se requiere una dirección IPv4 de destino Art-Net";
    return false;
  }
  if(config.udp_port == 0U) {
    error_message = "El puerto UDP de Art-Net no puede ser cero";
    return false;
  }
  if(config.port_address > 0x7FFFU) {
    error_message = "La dirección de universo Art-Net supera 15 bits";
    return false;
  }
  if(config.channel_count < 2U || config.channel_count > 512U ||
     (config.channel_count % 2U) != 0U) {
    error_message = "ArtDMX requiere entre 2 y 512 canales y una cantidad par";
    return false;
  }
  if(config.frames_per_second < 1U || config.frames_per_second > 60U) {
    error_message = "La frecuencia Art-Net debe estar entre 1 y 60 Hz";
    return false;
  }
  return true;
}

bool validate_optional_source(std::string_view source,
                              std::string& error_message) noexcept {
  if(source.empty()) return true;
  std::array<std::uint8_t, 4> octets{};
  if(!parse_numeric_ipv4(source, octets)) {
    error_message = "La interfaz TX debe usar una IPv4 local numérica";
    return false;
  }
  if(!valid_artnet_ipv4(octets)) {
    error_message = "La IPv4 local TX no es válida para Art-Net";
    return false;
  }
  return true;
}

void build_artdmx_packet(std::array<std::uint8_t, kMaximumArtDmxPacketBytes>& packet,
                         const DmxUniverse& universe,
                         std::uint16_t port_address,
                         std::uint8_t sequence,
                         std::uint16_t channel_count) noexcept {
  packet.fill(0U);
  static constexpr std::array<std::uint8_t, 8> id = {
      'A', 'r', 't', '-', 'N', 'e', 't', 0U};
  std::copy(id.begin(), id.end(), packet.begin());
  packet[8] = 0x00U;
  packet[9] = 0x50U;
  packet[10] = 0x00U;
  packet[11] = 0x0EU;
  packet[12] = sequence;
  packet[13] = 0x00U;
  packet[14] = static_cast<std::uint8_t>(port_address & 0xFFU);
  packet[15] = static_cast<std::uint8_t>((port_address >> 8U) & 0x7FU);
  packet[16] = static_cast<std::uint8_t>((channel_count >> 8U) & 0xFFU);
  packet[17] = static_cast<std::uint8_t>(channel_count & 0xFFU);
  std::copy_n(universe.begin(), channel_count, packet.begin() + kArtDmxHeaderBytes);
}

}  // namespace

bool validate_artnet_output_config(const ArtNetOutputConfig& config,
                                   std::string& error_message) noexcept {
  error_message.clear();
  if(!valid_config_ranges(config, error_message)) return false;
  if(!validate_optional_source(config.source_ipv4, error_message)) return false;

  std::array<std::uint8_t, 4> octets{};
  if(!parse_numeric_ipv4(config.target_ipv4, octets)) {
    error_message = "El destino Art-Net debe ser una IPv4 numérica";
    return false;
  }
  if(!valid_artnet_ipv4(octets)) {
    error_message = "El destino Art-Net debe ser unicast o broadcast dirigido de subred";
    return false;
  }
  return true;
}

class ArtNetOutputWorker::Impl final {
 public:
  ~Impl() { shutdown(); }

  void set_preferred_source(std::string source) {
    const std::scoped_lock lock(preferred_source_mutex_);
    preferred_source_ipv4_ = std::move(source);
  }

  std::string preferred_source() const {
    const std::scoped_lock lock(preferred_source_mutex_);
    return preferred_source_ipv4_;
  }

  bool acquire_lease(const ArtNetOutputConfig& next_config,
                     std::string& error_message) {
    const OutputLeaseKey key{
        next_config.target_ipv4, next_config.udp_port, next_config.port_address};
    const std::scoped_lock lock(gOutputLeaseMutex);
    const auto [iterator, inserted] = gOutputLeases.insert(key);
    (void)iterator;
    if(!inserted) {
      error_message =
          "Otra instancia de AEYLA ya controla este destino/universo Art-Net";
      return false;
    }
    lease_key_ = key;
    return true;
  }

  void release_lease() noexcept {
    if(!lease_key_.has_value()) return;
    const std::scoped_lock lock(gOutputLeaseMutex);
    gOutputLeases.erase(*lease_key_);
    lease_key_.reset();
  }

  bool initialize_socket(const ArtNetOutputConfig& next_config,
                         std::string& error_message) {
#ifdef _WIN32
    WSADATA data{};
    const int startup = WSAStartup(MAKEWORD(2, 2), &data);
    if(startup != 0) {
      error_message = "No se pudo iniciar Winsock · código " +
                      std::to_string(startup);
      return false;
    }
    winsock_started_ = true;
#endif

    sockaddr_in next_target{};
    next_target.sin_family = AF_INET;
    next_target.sin_port = htons(next_config.udp_port);
    if(inet_pton(AF_INET, next_config.target_ipv4.c_str(),
                 &next_target.sin_addr) != 1) {
      error_message = "No se pudo interpretar la IPv4 de destino Art-Net";
      cleanup_winsock();
      return false;
    }

    SocketHandle next_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(next_socket == kInvalidSocket) {
      error_message = "No se pudo crear el socket UDP de Art-Net";
      cleanup_winsock();
      return false;
    }

#ifdef _WIN32
    const BOOL broadcast_enabled = TRUE;
    if(setsockopt(next_socket, SOL_SOCKET, SO_BROADCAST,
                  reinterpret_cast<const char*>(&broadcast_enabled),
                  sizeof(broadcast_enabled)) != 0) {
#else
    const int broadcast_enabled = 1;
    if(setsockopt(next_socket, SOL_SOCKET, SO_BROADCAST,
                  &broadcast_enabled, sizeof(broadcast_enabled)) != 0) {
#endif
      close_socket(next_socket);
      error_message = "No se pudo habilitar broadcast dirigido para Art-Net";
      cleanup_winsock();
      return false;
    }

    if(!next_config.source_ipv4.empty()) {
      sockaddr_in local{};
      local.sin_family = AF_INET;
      local.sin_port = 0U;
      if(inet_pton(AF_INET, next_config.source_ipv4.c_str(),
                   &local.sin_addr) != 1) {
        close_socket(next_socket);
        error_message = "No se pudo interpretar la IPv4 local de TX";
        cleanup_winsock();
        return false;
      }
      if(bind(next_socket, reinterpret_cast<const sockaddr*>(&local),
              sizeof(local)) != 0) {
        close_socket(next_socket);
        error_message = "No se pudo ligar Art-Net TX a la IPv4 local " +
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
    override_enabled_.store(false, std::memory_order_release);
    fail_closed_.store(false, std::memory_order_release);
    blackout_burst_remaining_.store(0U, std::memory_order_release);
    sent_packets_.store(0U, std::memory_order_relaxed);
    blackout_packets_.store(0U, std::memory_order_relaxed);
    send_errors_.store(0U, std::memory_order_relaxed);
    consecutive_send_errors_.store(0U, std::memory_order_relaxed);
    timing_misses_.store(0U, std::memory_order_relaxed);
    fail_closed_events_.store(0U, std::memory_order_relaxed);
    stale_publish_drops_.store(0U, std::memory_order_relaxed);
    last_sent_generation_.store(0U, std::memory_order_relaxed);
    published_generation_.store(0U, std::memory_order_relaxed);
    {
      const std::scoped_lock lock(frame_mutex_);
      latest_.fill(0U);
      latest_generation_ = 0U;
      has_frame_ = false;
      override_latest_.fill(0U);
      override_generation_ = 0U;
      has_override_frame_ = false;
    }

    running_.store(true, std::memory_order_release);
    try {
      worker_ = std::thread([this]() { run(); });
    } catch(...) {
      running_.store(false, std::memory_order_release);
      throw;
    }
  }

  void schedule_blackout_burst() noexcept {
    blackout_burst_remaining_.store(
        kAeylaArtNetBlackoutBurstFrames, std::memory_order_release);
    wake_.notify_all();
  }

  void consume_one_blackout_frame() noexcept {
    auto remaining = blackout_burst_remaining_.load(std::memory_order_acquire);
    while(remaining > 0U &&
          !blackout_burst_remaining_.compare_exchange_weak(
              remaining, remaining - 1U,
              std::memory_order_acq_rel, std::memory_order_acquire)) {
    }
  }

  void shutdown() noexcept {
    if(worker_.joinable()) {
      const bool was_enabled = enabled_.exchange(false, std::memory_order_acq_rel);
      const bool was_override =
          override_enabled_.exchange(false, std::memory_order_acq_rel);
      if(was_enabled || was_override)
        schedule_blackout_burst();
      stop_requested_.store(true, std::memory_order_release);
      wake_.notify_all();
      worker_.join();
    } else {
      enabled_.store(false, std::memory_order_release);
      override_enabled_.store(false, std::memory_order_release);
      stop_requested_.store(true, std::memory_order_release);
      blackout_burst_remaining_.store(0U, std::memory_order_release);
    }

    running_.store(false, std::memory_order_release);
    close_socket(socket_);
    socket_ = kInvalidSocket;
    cleanup_winsock();
    release_lease();
  }

  void publish(const DmxUniverse& universe, std::uint64_t generation) {
    const std::scoped_lock lock(frame_mutex_);
    if(has_frame_ && generation < latest_generation_) {
      stale_publish_drops_.fetch_add(1U, std::memory_order_relaxed);
      return;
    }
    latest_ = universe;
    latest_generation_ = generation;
    has_frame_ = true;
    published_generation_.store(generation, std::memory_order_release);
  }

  void publish_override(const DmxUniverse& universe, std::uint64_t generation) {
    const std::scoped_lock lock(frame_mutex_);
    override_latest_ = universe;
    override_generation_ = generation;
    has_override_frame_ = true;
  }

  void clear_fail_closed_for_explicit_rearm() noexcept {
    fail_closed_.store(false, std::memory_order_release);
    consecutive_send_errors_.store(0U, std::memory_order_relaxed);
    blackout_burst_remaining_.store(0U, std::memory_order_release);
  }

  void set_enabled(bool next_enabled) noexcept {
    if(next_enabled)
      clear_fail_closed_for_explicit_rearm();
    const bool previous = enabled_.exchange(next_enabled, std::memory_order_acq_rel);
    if(previous && !next_enabled &&
       !override_enabled_.load(std::memory_order_acquire))
      schedule_blackout_burst();
    if(next_enabled)
      blackout_burst_remaining_.store(0U, std::memory_order_release);
    wake_.notify_all();
  }

  void set_override_enabled(bool next_enabled) noexcept {
    if(next_enabled)
      clear_fail_closed_for_explicit_rearm();
    const bool previous =
        override_enabled_.exchange(next_enabled, std::memory_order_acq_rel);
    if(previous && !next_enabled &&
       !enabled_.load(std::memory_order_acquire))
      schedule_blackout_burst();
    if(next_enabled)
      blackout_burst_remaining_.store(0U, std::memory_order_release);
    wake_.notify_all();
  }

  bool override_enabled() const noexcept {
    return override_enabled_.load(std::memory_order_acquire);
  }

  ArtNetOutputStats stats() const noexcept {
    ArtNetOutputStats result;
    result.running = running_.load(std::memory_order_acquire);
    result.enabled = enabled_.load(std::memory_order_acquire);
    result.override_enabled = override_enabled_.load(std::memory_order_acquire);
    result.fail_closed = fail_closed_.load(std::memory_order_acquire);
    result.configured_fps = config_.frames_per_second;
    result.published_generation =
        published_generation_.load(std::memory_order_relaxed);
    result.last_sent_generation =
        last_sent_generation_.load(std::memory_order_relaxed);
    result.sent_packets = sent_packets_.load(std::memory_order_relaxed);
    result.blackout_packets = blackout_packets_.load(std::memory_order_relaxed);
    result.send_errors = send_errors_.load(std::memory_order_relaxed);
    result.consecutive_send_errors =
        consecutive_send_errors_.load(std::memory_order_relaxed);
    result.timing_misses = timing_misses_.load(std::memory_order_relaxed);
    result.fail_closed_events = fail_closed_events_.load(std::memory_order_relaxed);
    result.stale_publish_drops =
        stale_publish_drops_.load(std::memory_order_relaxed);
    return result;
  }

 private:
  void cleanup_winsock() noexcept {
#ifdef _WIN32
    if(winsock_started_) {
      (void)WSACleanup();
      winsock_started_ = false;
    }
#endif
  }

  bool snapshot_latest(DmxUniverse& universe,
                       std::uint64_t& generation) {
    const std::scoped_lock lock(frame_mutex_);
    if(!has_frame_) return false;
    universe = latest_;
    generation = latest_generation_;
    return true;
  }

  bool snapshot_override(DmxUniverse& universe,
                         std::uint64_t& generation) {
    const std::scoped_lock lock(frame_mutex_);
    if(!has_override_frame_) return false;
    universe = override_latest_;
    generation = override_generation_;
    return true;
  }

  void enter_fail_closed() noexcept {
    const bool already = fail_closed_.exchange(true, std::memory_order_acq_rel);
    enabled_.store(false, std::memory_order_release);
    override_enabled_.store(false, std::memory_order_release);
    if(already) return;
    fail_closed_events_.fetch_add(1U, std::memory_order_relaxed);
    schedule_blackout_burst();
  }

  void register_send_result(bool ok) noexcept {
    if(ok) {
      consecutive_send_errors_.store(0U, std::memory_order_relaxed);
      return;
    }
    send_errors_.fetch_add(1U, std::memory_order_relaxed);
    const auto consecutive =
        consecutive_send_errors_.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if(consecutive >= kAeylaArtNetFailClosedErrorThreshold)
      enter_fail_closed();
  }

  bool transmit(const DmxUniverse& universe, std::uint64_t generation,
                bool blackout) noexcept {
    std::array<std::uint8_t, kMaximumArtDmxPacketBytes> packet{};
    build_artdmx_packet(packet, universe, config_.port_address, sequence_,
                        config_.channel_count);
    const std::size_t packet_size =
        kArtDmxHeaderBytes + static_cast<std::size_t>(config_.channel_count);

#ifdef _WIN32
    const int sent = sendto(
        socket_, reinterpret_cast<const char*>(packet.data()),
        static_cast<int>(packet_size), 0,
        reinterpret_cast<const sockaddr*>(&target_), sizeof(target_));
    const bool ok = sent == static_cast<int>(packet_size);
#else
    const ssize_t sent = sendto(
        socket_, packet.data(), packet_size, 0,
        reinterpret_cast<const sockaddr*>(&target_), sizeof(target_));
    const bool ok = sent == static_cast<ssize_t>(packet_size);
#endif

    register_send_result(ok);
    if(!ok) return false;

    sent_packets_.fetch_add(1U, std::memory_order_relaxed);
    if(blackout)
      blackout_packets_.fetch_add(1U, std::memory_order_relaxed);
    else
      last_sent_generation_.store(generation, std::memory_order_relaxed);

    ++sequence_;
    if(sequence_ == 0U) sequence_ = 1U;
    return true;
  }

  void run() noexcept {
    sequence_ = 1U;
    const auto period = std::chrono::nanoseconds(
        1000000000LL / static_cast<long long>(config_.frames_per_second));
    auto next_deadline = std::chrono::steady_clock::now();
    const DmxUniverse blackout{};

    for(;;) {
      const auto now = std::chrono::steady_clock::now();
      if(now < next_deadline) {
        std::unique_lock lock(wake_mutex_);
        wake_.wait_until(lock, next_deadline);
        continue;
      }

      const auto remaining =
          blackout_burst_remaining_.load(std::memory_order_acquire);
      if(remaining > 0U) {
        (void)transmit(blackout, 0U, true);
        consume_one_blackout_frame();
      } else if(override_enabled_.load(std::memory_order_acquire)) {
        DmxUniverse latest{};
        std::uint64_t generation = 0U;
        if(snapshot_override(latest, generation))
          (void)transmit(latest, generation, false);
      } else if(enabled_.load(std::memory_order_acquire)) {
        DmxUniverse latest{};
        std::uint64_t generation = 0U;
        if(snapshot_latest(latest, generation))
          (void)transmit(latest, generation, false);
      }

      next_deadline += period;
      const auto after = std::chrono::steady_clock::now();
      if(next_deadline <= after) {
        const auto late = after - next_deadline;
        const auto missed =
            static_cast<std::uint64_t>(late / period) + 1U;
        timing_misses_.fetch_add(missed, std::memory_order_relaxed);
        next_deadline += period * static_cast<long long>(missed);
      }

      if(stop_requested_.load(std::memory_order_acquire) &&
         blackout_burst_remaining_.load(std::memory_order_acquire) == 0U)
        break;
    }

    running_.store(false, std::memory_order_release);
  }

  mutable std::mutex preferred_source_mutex_;
  std::string preferred_source_ipv4_;

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
  std::atomic<bool> override_enabled_{false};
  std::atomic<bool> fail_closed_{false};
  std::atomic<std::uint32_t> blackout_burst_remaining_{0U};

  mutable std::mutex frame_mutex_;
  DmxUniverse latest_{};
  std::uint64_t latest_generation_{0U};
  bool has_frame_{false};
  DmxUniverse override_latest_{};
  std::uint64_t override_generation_{0U};
  bool has_override_frame_{false};

  std::atomic<std::uint64_t> published_generation_{0U};
  std::atomic<std::uint64_t> last_sent_generation_{0U};
  std::atomic<std::uint64_t> sent_packets_{0U};
  std::atomic<std::uint64_t> blackout_packets_{0U};
  std::atomic<std::uint64_t> send_errors_{0U};
  std::atomic<std::uint64_t> consecutive_send_errors_{0U};
  std::atomic<std::uint64_t> timing_misses_{0U};
  std::atomic<std::uint64_t> fail_closed_events_{0U};
  std::atomic<std::uint64_t> stale_publish_drops_{0U};
  std::uint8_t sequence_{1U};
};

ArtNetOutputWorker::ArtNetOutputWorker() : impl_(std::make_unique<Impl>()) {}
ArtNetOutputWorker::~ArtNetOutputWorker() = default;

void ArtNetOutputWorker::set_preferred_source_ipv4(std::string source_ipv4) {
  impl_->set_preferred_source(std::move(source_ipv4));
}

bool ArtNetOutputWorker::start(const ArtNetOutputConfig& config,
                               std::string& error_message) {
  ArtNetOutputConfig effective = config;
  if(effective.source_ipv4.empty())
    effective.source_ipv4 = impl_->preferred_source();

  // Contrato R07: aunque una ruta heredada solicite 30/40 Hz, el producto
  // normaliza la transmisión a la misma cadencia de captura: 44 Hz.
  effective.frames_per_second = kAeylaArtNetFramesPerSecond;

  error_message.clear();
  if(!validate_artnet_output_config(effective, error_message)) return false;

  impl_->shutdown();
  if(!impl_->acquire_lease(effective, error_message)) return false;
  if(!impl_->initialize_socket(effective, error_message)) {
    impl_->shutdown();
    return false;
  }

  try {
    impl_->launch();
  } catch(...) {
    impl_->shutdown();
    error_message = "No se pudo iniciar el hilo de transmisión Art-Net";
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

void ArtNetOutputWorker::publish_override(const DmxUniverse& universe,
                                          std::uint64_t generation) {
  impl_->publish_override(universe, generation);
}

void ArtNetOutputWorker::set_override_enabled(bool enabled) noexcept {
  impl_->set_override_enabled(enabled);
}

bool ArtNetOutputWorker::override_enabled() const noexcept {
  return impl_->override_enabled();
}

ArtNetOutputStats ArtNetOutputWorker::stats() const noexcept {
  return impl_->stats();
}

}  // namespace aeyla::output
