#pragma once

#include "core/dmx_compiler.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aeyla::capture {

struct ArtNetCaptureConfig {
  // Bind to one selected local IPv4 address. Use 0.0.0.0 only as an explicit
  // diagnostic mode when the operator intentionally wants every interface.
  std::string listen_ipv4{"0.0.0.0"};
  std::uint16_t udp_port{6454U};
  std::uint16_t port_address{0U};
  std::uint16_t frames_per_second{44U};
  // Empty means accept any ArtDMX source. Once recording begins, the first
  // accepted source becomes authoritative until that recording ends.
  std::string source_ipv4;
};

struct DmxTake {
  std::string name;
  std::uint16_t port_address{0U};
  std::uint16_t frames_per_second{44U};
  std::string source_ipv4;
  std::vector<DmxUniverse> frames;

  [[nodiscard]] double duration_seconds() const noexcept {
    return frames_per_second == 0U
               ? 0.0
               : static_cast<double>(frames.size()) /
                     static_cast<double>(frames_per_second);
  }
};

struct ArtNetCaptureStats {
  bool running{false};
  bool recording{false};
  bool signal_present{false};
  bool overflowed{false};
  std::uint64_t packets_received{0U};
  std::uint64_t packets_accepted{0U};
  std::uint64_t invalid_packets{0U};
  std::uint64_t ignored_packets{0U};
  std::uint64_t recorded_frames{0U};
  std::uint64_t sequence_gaps{0U};
  double last_packet_age_ms{0.0};
  std::string listen_ipv4;
  std::string source_ipv4;
  std::uint16_t port_address{0U};
};

[[nodiscard]] bool validate_artnet_capture_config(
    const ArtNetCaptureConfig& config, std::string& error_message) noexcept;

// Dedicated non-realtime Art-Net receiver + deterministic DMX sampler.
//
// Network packets may arrive irregularly. The worker maintains the latest
// complete 512-byte DMX state and samples that state at a fixed FPS while
// recording. This prevents network jitter from becoming timeline jitter in a
// captured Take.
class ArtNetCaptureWorker final {
 public:
  ArtNetCaptureWorker();
  ~ArtNetCaptureWorker();

  ArtNetCaptureWorker(const ArtNetCaptureWorker&) = delete;
  ArtNetCaptureWorker& operator=(const ArtNetCaptureWorker&) = delete;

  [[nodiscard]] bool start(const ArtNetCaptureConfig& config,
                           std::string& error_message);
  void stop() noexcept;

  [[nodiscard]] bool begin_recording(std::string& error_message);
  [[nodiscard]] std::optional<DmxTake> end_recording(std::string name);
  void discard_recording() noexcept;

  [[nodiscard]] bool latest_frame(DmxUniverse& frame) const noexcept;
  [[nodiscard]] ArtNetCaptureStats stats() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace aeyla::capture
