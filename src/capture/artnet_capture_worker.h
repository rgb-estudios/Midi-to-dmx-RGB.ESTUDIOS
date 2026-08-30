#pragma once

#include "capture/dmx_take_stream_writer.h"
#include "core/dmx_compiler.h"

#include <algorithm>
#include <cstddef>
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

  // Non-destructive playback edit. trim_end_frame_exclusive == 0 means the
  // original final frame boundary. The v1 .aeylatake payload remains untouched;
  // these edit boundaries are runtime metadata for the current gate.
  std::size_t trim_start_frame{0U};
  std::size_t trim_end_frame_exclusive{0U};

  [[nodiscard]] std::size_t effective_start_frame() const noexcept {
    if(frames.empty()) return 0U;
    return std::min(trim_start_frame, frames.size() - 1U);
  }

  [[nodiscard]] std::size_t effective_end_frame_exclusive() const noexcept {
    if(frames.empty()) return 0U;
    if(trim_end_frame_exclusive == 0U)
      return frames.size();
    return std::clamp(trim_end_frame_exclusive,
                      effective_start_frame() + 1U, frames.size());
  }

  [[nodiscard]] double duration_seconds() const noexcept {
    return frames_per_second == 0U
               ? 0.0
               : static_cast<double>(frames.size()) /
                     static_cast<double>(frames_per_second);
  }

  [[nodiscard]] double effective_duration_seconds() const noexcept {
    if(frames_per_second == 0U || frames.empty()) return 0.0;
    const auto start = effective_start_frame();
    const auto end = effective_end_frame_exclusive();
    return end <= start
               ? 0.0
               : static_cast<double>(end - start) /
                     static_cast<double>(frames_per_second);
  }
};

struct ArtNetCaptureStats {
  bool running{false};
  bool recording{false};
  bool streaming_to_disk{false};
  bool storage_failed{false};
  bool signal_present{false};
  bool overflowed{false};
  std::uint64_t packets_received{0U};
  std::uint64_t packets_accepted{0U};
  std::uint64_t invalid_packets{0U};
  std::uint64_t ignored_packets{0U};
  std::uint64_t recorded_frames{0U};
  std::uint64_t sequence_gaps{0U};
  std::size_t peak_buffered_frames{0U};
  double last_packet_age_ms{0.0};
  std::string listen_ipv4;
  std::string source_ipv4;
  std::string storage_error;
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

  // Legacy bounded-duration in-memory capture retained for compatibility tests.
  // Product capture should use begin_streamed_recording().
  [[nodiscard]] bool begin_recording(std::string& error_message);
  [[nodiscard]] std::optional<DmxTake> end_recording(std::string name);

  // Production capture path. Frames are sampled at the configured rate and
  // passed into a fixed 512 KiB queue backed by a dedicated disk thread.
  [[nodiscard]] bool begin_streamed_recording(
      const DmxTakeStreamConfig& config,
      std::string& error_message);
  [[nodiscard]] bool end_streamed_recording(std::string& error_message);
  [[nodiscard]] bool streamed_recording_active() const noexcept;

  void discard_recording() noexcept;

  // Realtime-safe capture timeline snapshot. This is intentionally much
  // narrower than stats(): the MIDI/audio callback can timestamp a show marker
  // against the DMX sampler without taking frame/string/storage locks. The
  // value is the number of complete 44 Hz frames already accepted by the
  // recording path at the instant of the load.
  [[nodiscard]] std::uint64_t recorded_frames_fast() const noexcept;

  [[nodiscard]] bool latest_frame(DmxUniverse& frame) const noexcept;
  [[nodiscard]] ArtNetCaptureStats stats() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace aeyla::capture
