#include "capture/artnet_capture_worker.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
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
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace aeyla::capture {
namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

constexpr std::size_t kArtDmxHeaderBytes = 18U;
constexpr std::uint16_t kArtDmxOpcode = 0x5000U;
constexpr auto kSignalTimeout = std::chrono::milliseconds(750);

void close_socket(SocketHandle socket) noexcept {
  if(socket == kInvalidSocket) return;
#ifdef _WIN32
  (void)closesocket(socket);
#else
  (void)close(socket);
#endif
}

bool parse_ipv4(std::string_view text, in_addr& address) noexcept {
  if(text.empty()) return false;
  std::string value(text);
  return inet_pton(AF_INET, value.c_str(), &address) == 1;
}

bool valid_local_ipv4(std::string_view text) noexcept {
  in_addr address{};
  return parse_ipv4(text, address);
}

std::string ipv4_text(const sockaddr_in& address) {
  std::array<char, INET_ADDRSTRLEN> text{};
  if(inet_ntop(AF_INET, &address.sin_addr, text.data(), text.size()) == nullptr)
    return {};
  return text.data();
}

struct ParsedArtDmx {
  DmxUniverse frame{};
  std::uint16_t port_address{0U};
  std::uint8_t sequence{0U};
};

bool parse_artdmx(const std::uint8_t* data, std::size_t size,
                  ParsedArtDmx& parsed) noexcept {
  static constexpr std::array<std::uint8_t, 8> kId{
      'A', 'r', 't', '-', 'N', 'e', 't', 0};
  if(data == nullptr || size < kArtDmxHeaderBytes) return false;
  if(!std::equal(kId.begin(), kId.end(), data)) return false;

  const std::uint16_t opcode = static_cast<std::uint16_t>(data[8]) |
      (static_cast<std::uint16_t>(data[9]) << 8U);
  if(opcode != kArtDmxOpcode) return false;

  const std::uint16_t protocol_version =
      (static_cast<std::uint16_t>(data[10]) << 8U) |
      static_cast<std::uint16_t>(data[11]);
  if(protocol_version < 14U) return false;

  const std::uint16_t length =
      (static_cast<std::uint16_t>(data[16]) << 8U) |
      static_cast<std::uint16_t>(data[17]);
  if(length < 2U || length > 512U || (length % 2U) != 0U) return false;
  if(size < kArtDmxHeaderBytes + static_cast<std::size_t>(length)) return false;

  parsed.frame.fill(0U);
  std::copy_n(data + kArtDmxHeaderBytes, length, parsed.frame.begin());
  parsed.sequence = data[12];
  parsed.port_address = static_cast<std::uint16_t>(data[14]) |
      (static_cast<std::uint16_t>(data[15] & 0x7FU) << 8U);
  return true;
}

}  // namespace

bool validate_artnet_capture_config(const ArtNetCaptureConfig& config,
                                    std::string& error_message) noexcept {
  error_message.clear();
  if(!valid_local_ipv4(config.listen_ipv4)) {
    error_message = "Capture listen address must be a numeric local IPv4";
    return false;
  }
  if(!config.source_ipv4.empty() && !valid_local_ipv4(config.source_ipv4)) {
    error_message = "Capture source filter must be a numeric IPv4";
    return false;
  }
  if(config.udp_port == 0U) {
    error_message = "Capture UDP port must be non-zero";
    return false;
  }
  if(config.port_address > 0x7FFFU) {
    error_message = "Capture Art-Net port address exceeds 15 bits";
    return false;
  }
  if(config.frames_per_second < 1U || config.frames_per_second > 60U) {
    error_message = "Capture rate must be between 1 and 60 FPS";
    return false;
  }
  return true;
}

class ArtNetCaptureWorker::Impl final {
 public:
  ~Impl() { shutdown(); }

  bool start(const ArtNetCaptureConfig& config, std::string& error_message) {
    shutdown();
    error_message.clear();
    if(!validate_artnet_capture_config(config, error_message)) return false;

#ifdef _WIN32
    WSADATA data{};
    const int startup = WSAStartup(MAKEWORD(2, 2), &data);
    if(startup != 0) {
      error_message = "WSAStartup failed with code " + std::to_string(startup);
      return false;
    }
    winsock_started_ = true;
#endif

    SocketHandle socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(socket_handle == kInvalidSocket) {
      error_message = "Could not create Art-Net capture UDP socket";
      cleanup_winsock();
      return false;
    }

    int enabled = 1;
    (void)setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&enabled), sizeof(enabled));
    (void)setsockopt(socket_handle, SOL_SOCKET, SO_BROADCAST,
                     reinterpret_cast<const char*>(&enabled), sizeof(enabled));

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(config.udp_port);
    if(inet_pton(AF_INET, config.listen_ipv4.c_str(), &local.sin_addr) != 1) {
      close_socket(socket_handle);
      cleanup_winsock();
      error_message = "Capture listen IPv4 could not be parsed";
      return false;
    }
    if(bind(socket_handle, reinterpret_cast<const sockaddr*>(&local),
            sizeof(local)) != 0) {
      close_socket(socket_handle);
      cleanup_winsock();
      error_message = "Could not bind Art-Net capture to " + config.listen_ipv4 +
                      ":" + std::to_string(config.udp_port);
      return false;
    }

    socket_ = socket_handle;
    config_ = config;
    reset_state();
    stop_requested_.store(false, std::memory_order_release);
    try {
      worker_ = std::thread([this]() { run(); });
    } catch(...) {
      close_socket(socket_);
      socket_ = kInvalidSocket;
      cleanup_winsock();
      error_message = "Could not start Art-Net capture worker";
      return false;
    }
    return true;
  }

  void shutdown() noexcept {
    recording_.store(false, std::memory_order_release);
    streamed_mode_.store(false, std::memory_order_release);
    stop_requested_.store(true, std::memory_order_release);
    if(worker_.joinable()) worker_.join();
    stream_writer_.abort();
    running_.store(false, std::memory_order_release);
    close_socket(socket_);
    socket_ = kInvalidSocket;
    cleanup_winsock();
  }

  bool begin_recording(std::string& error_message) {
    error_message.clear();
    if(!running_.load(std::memory_order_acquire)) {
      error_message = "Art-Net capture input is not running";
      return false;
    }
    if(recording_.load(std::memory_order_acquire)) {
      error_message = "A DMX Take is already recording";
      return false;
    }

    stream_writer_.abort();
    streamed_mode_.store(false, std::memory_order_release);
    {
      const std::scoped_lock lock(record_mutex_);
      recording_frames_.clear();
      recording_frames_.reserve(static_cast<std::size_t>(
          config_.frames_per_second) * 60U * 10U);
      recording_source_.clear();
      overflowed_.store(false, std::memory_order_release);
      recorded_frames_.store(0U, std::memory_order_relaxed);
    }
    recording_.store(true, std::memory_order_release);
    return true;
  }

  std::optional<DmxTake> end_recording(std::string name) {
    if(streamed_mode_.load(std::memory_order_acquire))
      return std::nullopt;
    if(!recording_.exchange(false, std::memory_order_acq_rel))
      return std::nullopt;

    DmxTake result;
    result.name = std::move(name);
    result.port_address = config_.port_address;
    result.frames_per_second = config_.frames_per_second;
    {
      const std::scoped_lock lock(record_mutex_);
      result.source_ipv4 = recording_source_;
      result.frames = std::move(recording_frames_);
      recording_frames_.clear();
      recording_source_.clear();
    }
    return result;
  }

  bool begin_streamed_recording(const DmxTakeStreamConfig& stream_config,
                                std::string& error_message) {
    error_message.clear();
    if(!running_.load(std::memory_order_acquire)) {
      error_message = "Art-Net capture input is not running";
      return false;
    }
    if(recording_.load(std::memory_order_acquire)) {
      error_message = "A DMX Take is already recording";
      return false;
    }
    if(stream_config.port_address != config_.port_address ||
       stream_config.frames_per_second != config_.frames_per_second) {
      error_message = "Streamed Take metadata must match active capture universe and FPS";
      return false;
    }
    if(stream_config.source_ipv4.empty()) {
      error_message = "Streamed Take requires the current Art-Net source IPv4";
      return false;
    }

    {
      const std::scoped_lock lock(record_mutex_);
      recording_frames_.clear();
      recording_source_ = stream_config.source_ipv4;
      overflowed_.store(false, std::memory_order_release);
      recorded_frames_.store(0U, std::memory_order_relaxed);
    }

    if(!stream_writer_.start(stream_config, error_message)) {
      const std::scoped_lock lock(record_mutex_);
      recording_source_.clear();
      return false;
    }

    streamed_mode_.store(true, std::memory_order_release);
    recording_.store(true, std::memory_order_release);
    return true;
  }

  bool end_streamed_recording(std::string& error_message) {
    error_message.clear();
    if(!streamed_mode_.exchange(false, std::memory_order_acq_rel)) {
      error_message = "No streamed DMX Take is active";
      return false;
    }

    recording_.store(false, std::memory_order_release);
    const bool finalized = stream_writer_.finalize(error_message);
    const auto writer_status = stream_writer_.status();
    recorded_frames_.store(writer_status.frames_written,
                           std::memory_order_relaxed);
    if(!finalized || writer_status.failed)
      overflowed_.store(true, std::memory_order_release);
    {
      const std::scoped_lock lock(record_mutex_);
      recording_source_.clear();
    }
    return finalized;
  }

  bool streamed_recording_active() const noexcept {
    return streamed_mode_.load(std::memory_order_acquire);
  }

  void discard_recording() noexcept {
    recording_.store(false, std::memory_order_release);
    const bool was_streamed =
        streamed_mode_.exchange(false, std::memory_order_acq_rel);
    if(was_streamed)
      stream_writer_.abort();
    const std::scoped_lock lock(record_mutex_);
    recording_frames_.clear();
    recording_source_.clear();
    recorded_frames_.store(0U, std::memory_order_relaxed);
    overflowed_.store(false, std::memory_order_release);
  }

  bool latest_frame(DmxUniverse& frame) const noexcept {
    const std::scoped_lock lock(frame_mutex_);
    if(!has_latest_) return false;
    frame = latest_;
    return true;
  }

  ArtNetCaptureStats stats() const noexcept {
    ArtNetCaptureStats result;
    result.running = running_.load(std::memory_order_acquire);
    result.recording = recording_.load(std::memory_order_acquire);
    result.streaming_to_disk = streamed_mode_.load(std::memory_order_acquire);
    result.overflowed = overflowed_.load(std::memory_order_acquire);
    result.packets_received = packets_received_.load(std::memory_order_relaxed);
    result.packets_accepted = packets_accepted_.load(std::memory_order_relaxed);
    result.invalid_packets = invalid_packets_.load(std::memory_order_relaxed);
    result.ignored_packets = ignored_packets_.load(std::memory_order_relaxed);
    result.recorded_frames = recorded_frames_.load(std::memory_order_relaxed);
    result.sequence_gaps = sequence_gaps_.load(std::memory_order_relaxed);
    result.listen_ipv4 = config_.listen_ipv4;
    result.port_address = config_.port_address;

    const auto storage = stream_writer_.status();
    result.storage_failed = storage.failed;
    result.storage_error = storage.error;
    result.peak_buffered_frames = storage.peak_buffered_frames;
    if(result.streaming_to_disk || storage.frames_written > result.recorded_frames)
      result.recorded_frames = storage.frames_written;

    std::chrono::steady_clock::time_point last_packet;
    {
      const std::scoped_lock lock(frame_mutex_);
      result.source_ipv4 = latest_source_;
      last_packet = last_packet_time_;
      result.signal_present = has_latest_ &&
          std::chrono::steady_clock::now() - last_packet_time_ <= kSignalTimeout;
    }
    if(last_packet.time_since_epoch().count() != 0) {
      result.last_packet_age_ms = static_cast<double>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - last_packet).count()) / 1000.0;
    }
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

  void reset_state() {
    stream_writer_.abort();
    streamed_mode_.store(false, std::memory_order_release);
    packets_received_.store(0U, std::memory_order_relaxed);
    packets_accepted_.store(0U, std::memory_order_relaxed);
    invalid_packets_.store(0U, std::memory_order_relaxed);
    ignored_packets_.store(0U, std::memory_order_relaxed);
    recorded_frames_.store(0U, std::memory_order_relaxed);
    sequence_gaps_.store(0U, std::memory_order_relaxed);
    overflowed_.store(false, std::memory_order_release);
    last_sequence_ = 0U;
    last_sequence_source_.clear();
    {
      const std::scoped_lock lock(frame_mutex_);
      latest_.fill(0U);
      has_latest_ = false;
      latest_source_.clear();
      last_packet_time_ = {};
    }
    {
      const std::scoped_lock lock(record_mutex_);
      recording_frames_.clear();
      recording_source_.clear();
    }
  }

  bool source_allowed(const std::string& source) {
    if(!config_.source_ipv4.empty() && source != config_.source_ipv4)
      return false;
    if(!recording_.load(std::memory_order_acquire)) return true;

    const std::scoped_lock lock(record_mutex_);
    if(recording_source_.empty()) {
      recording_source_ = source;
      return true;
    }
    return recording_source_ == source;
  }

  void accept_packet(const ParsedArtDmx& parsed, const std::string& source) {
    if(parsed.port_address != config_.port_address || !source_allowed(source)) {
      ignored_packets_.fetch_add(1U, std::memory_order_relaxed);
      return;
    }

    if(parsed.sequence != 0U) {
      if(last_sequence_ != 0U && source == last_sequence_source_) {
        const std::uint8_t expected = last_sequence_ == 255U
                                          ? 1U
                                          : static_cast<std::uint8_t>(last_sequence_ + 1U);
        if(parsed.sequence != expected)
          sequence_gaps_.fetch_add(1U, std::memory_order_relaxed);
      }
      last_sequence_ = parsed.sequence;
      last_sequence_source_ = source;
    }

    {
      const std::scoped_lock lock(frame_mutex_);
      latest_ = parsed.frame;
      has_latest_ = true;
      latest_source_ = source;
      last_packet_time_ = std::chrono::steady_clock::now();
    }
    packets_accepted_.fetch_add(1U, std::memory_order_relaxed);
  }

  void sample_recording() {
    if(!recording_.load(std::memory_order_acquire)) return;

    DmxUniverse frame{};
    {
      const std::scoped_lock lock(frame_mutex_);
      if(has_latest_) frame = latest_;
    }

    if(streamed_mode_.load(std::memory_order_acquire)) {
      if(!stream_writer_.try_push_frame(frame)) {
        overflowed_.store(true, std::memory_order_release);
        recording_.store(false, std::memory_order_release);
        return;
      }
      recorded_frames_.fetch_add(1U, std::memory_order_relaxed);
      return;
    }

    const std::size_t maximum = static_cast<std::size_t>(
        config_.frames_per_second) * 60U * 60U;
    const std::scoped_lock lock(record_mutex_);
    if(!recording_.load(std::memory_order_acquire)) return;
    if(recording_frames_.size() >= maximum) {
      overflowed_.store(true, std::memory_order_release);
      recording_.store(false, std::memory_order_release);
      return;
    }
    recording_frames_.push_back(frame);
    recorded_frames_.store(recording_frames_.size(), std::memory_order_relaxed);
  }

  void run() noexcept {
    running_.store(true, std::memory_order_release);
    const auto sample_period = std::chrono::nanoseconds(
        1000000000LL / static_cast<long long>(config_.frames_per_second));
    auto next_sample = std::chrono::steady_clock::now();
    std::array<std::uint8_t, 2048> buffer{};

    while(!stop_requested_.load(std::memory_order_acquire)) {
      fd_set read_set;
      FD_ZERO(&read_set);
      FD_SET(socket_, &read_set);
      timeval timeout{};
      timeout.tv_sec = 0;
      timeout.tv_usec = 5000;
#ifdef _WIN32
      const int ready = select(0, &read_set, nullptr, nullptr, &timeout);
#else
      const int ready = select(socket_ + 1, &read_set, nullptr, nullptr, &timeout);
#endif
      if(ready > 0 && FD_ISSET(socket_, &read_set)) {
        sockaddr_in source_address{};
#ifdef _WIN32
        int source_length = sizeof(source_address);
        const int count = recvfrom(
            socket_, reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()), 0,
            reinterpret_cast<sockaddr*>(&source_address), &source_length);
        if(count > 0) {
          packets_received_.fetch_add(1U, std::memory_order_relaxed);
          ParsedArtDmx parsed;
          if(parse_artdmx(buffer.data(), static_cast<std::size_t>(count), parsed))
            accept_packet(parsed, ipv4_text(source_address));
          else
            invalid_packets_.fetch_add(1U, std::memory_order_relaxed);
        }
#else
        socklen_t source_length = sizeof(source_address);
        const ssize_t count = recvfrom(
            socket_, buffer.data(), buffer.size(), 0,
            reinterpret_cast<sockaddr*>(&source_address), &source_length);
        if(count > 0) {
          packets_received_.fetch_add(1U, std::memory_order_relaxed);
          ParsedArtDmx parsed;
          if(parse_artdmx(buffer.data(), static_cast<std::size_t>(count), parsed))
            accept_packet(parsed, ipv4_text(source_address));
          else
            invalid_packets_.fetch_add(1U, std::memory_order_relaxed);
        }
#endif
      }

      const auto now = std::chrono::steady_clock::now();
      while(now >= next_sample) {
        sample_recording();
        next_sample += sample_period;
      }
      if(next_sample + sample_period < now)
        next_sample = now + sample_period;
    }

    running_.store(false, std::memory_order_release);
  }

  ArtNetCaptureConfig config_{};
  SocketHandle socket_{kInvalidSocket};
#ifdef _WIN32
  bool winsock_started_{false};
#endif
  std::thread worker_;
  std::atomic<bool> stop_requested_{true};
  std::atomic<bool> running_{false};
  std::atomic<bool> recording_{false};
  std::atomic<bool> streamed_mode_{false};

  mutable std::mutex frame_mutex_;
  DmxUniverse latest_{};
  bool has_latest_{false};
  std::string latest_source_;
  std::chrono::steady_clock::time_point last_packet_time_{};

  mutable std::mutex record_mutex_;
  std::vector<DmxUniverse> recording_frames_;
  std::string recording_source_;
  DmxTakeStreamWriter stream_writer_{};

  std::atomic<std::uint64_t> packets_received_{0U};
  std::atomic<std::uint64_t> packets_accepted_{0U};
  std::atomic<std::uint64_t> invalid_packets_{0U};
  std::atomic<std::uint64_t> ignored_packets_{0U};
  std::atomic<std::uint64_t> recorded_frames_{0U};
  std::atomic<std::uint64_t> sequence_gaps_{0U};
  std::atomic<bool> overflowed_{false};
  std::uint8_t last_sequence_{0U};
  std::string last_sequence_source_;
};

ArtNetCaptureWorker::ArtNetCaptureWorker() : impl_(std::make_unique<Impl>()) {}
ArtNetCaptureWorker::~ArtNetCaptureWorker() = default;

bool ArtNetCaptureWorker::start(const ArtNetCaptureConfig& config,
                                std::string& error_message) {
  return impl_->start(config, error_message);
}

void ArtNetCaptureWorker::stop() noexcept { impl_->shutdown(); }

bool ArtNetCaptureWorker::begin_recording(std::string& error_message) {
  return impl_->begin_recording(error_message);
}

std::optional<DmxTake> ArtNetCaptureWorker::end_recording(std::string name) {
  return impl_->end_recording(std::move(name));
}

bool ArtNetCaptureWorker::begin_streamed_recording(
    const DmxTakeStreamConfig& config,
    std::string& error_message) {
  return impl_->begin_streamed_recording(config, error_message);
}

bool ArtNetCaptureWorker::end_streamed_recording(std::string& error_message) {
  return impl_->end_streamed_recording(error_message);
}

bool ArtNetCaptureWorker::streamed_recording_active() const noexcept {
  return impl_->streamed_recording_active();
}

void ArtNetCaptureWorker::discard_recording() noexcept {
  impl_->discard_recording();
}

bool ArtNetCaptureWorker::latest_frame(DmxUniverse& frame) const noexcept {
  return impl_->latest_frame(frame);
}

ArtNetCaptureStats ArtNetCaptureWorker::stats() const noexcept {
  return impl_->stats();
}

}  // namespace aeyla::capture
