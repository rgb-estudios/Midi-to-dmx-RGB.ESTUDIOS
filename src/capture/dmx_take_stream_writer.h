#pragma once

#include "core/dmx_compiler.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace aeyla::capture {

struct DmxTakeStreamConfig {
  std::filesystem::path target_path;
  std::string song_id;
  std::string song_name;
  std::string take_name;
  std::string source_ipv4;
  std::uint16_t port_address{0U};
  std::uint16_t frames_per_second{44U};
};

struct DmxTakeStreamStatus {
  bool running{false};
  bool accepting_frames{false};
  bool failed{false};
  std::uint64_t frames_enqueued{0U};
  std::uint64_t frames_written{0U};
  std::uint64_t queue_overflows{0U};
  std::size_t peak_buffered_frames{0U};
  std::filesystem::path target_path;
  std::filesystem::path temporary_path;
  std::string error;
};

// Bounded-RAM stream-to-disk writer for captured DMX Takes.
//
// Producer contract:
// - one capture/sampler thread calls try_push_frame();
// - try_push_frame() performs no allocation, file I/O or waiting;
// - queue overflow is a hard visible failure, never a silent frame drop.
//
// Consumer contract:
// - one background thread owns file I/O;
// - the v1 .aeylatake header frame count is checkpointed periodically;
// - finalize() writes the checksum trailer, fsyncs and atomically installs the
//   completed file.
class DmxTakeStreamWriter final {
 public:
  static constexpr std::size_t kBufferedFrames = 1024U;
  static constexpr std::size_t kBufferedBytes = kBufferedFrames * 512U;

  DmxTakeStreamWriter();
  ~DmxTakeStreamWriter();

  DmxTakeStreamWriter(const DmxTakeStreamWriter&) = delete;
  DmxTakeStreamWriter& operator=(const DmxTakeStreamWriter&) = delete;

  [[nodiscard]] bool start(const DmxTakeStreamConfig& config,
                           std::string& error_message);
  [[nodiscard]] bool try_push_frame(const DmxUniverse& frame) noexcept;
  [[nodiscard]] bool finalize(std::string& error_message);
  void abort() noexcept;

  [[nodiscard]] DmxTakeStreamStatus status() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

static_assert(DmxTakeStreamWriter::kBufferedBytes <= 1024U * 1024U,
              "AEYLA streamed capture buffer must remain at or below 1 MiB");

}  // namespace aeyla::capture
