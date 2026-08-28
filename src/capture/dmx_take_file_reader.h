#pragma once

#include "core/dmx_compiler.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace aeyla::capture {

struct DmxTakeFileReaderInfo {
  bool open{false};
  std::filesystem::path path;
  std::string song_id;
  std::string song_name;
  std::string take_name;
  std::string source_ipv4;
  std::uint16_t port_address{0U};
  std::uint16_t frames_per_second{0U};
  std::uint64_t frame_count{0U};
  std::uint64_t payload_offset{0U};
};

// Constant-memory random-access reader for v1 .aeylatake files.
//
// open() validates the complete checksum once without loading the DMX payload.
// read_frame() then serves arbitrary seek/loop positions through a fixed 64 KiB
// cache (128 frames). File I/O belongs on AEYLA's non-realtime playback worker,
// never the audio callback.
class DmxTakeFileReader final {
 public:
  static constexpr std::size_t kCacheFrames = 128U;
  static constexpr std::size_t kCacheBytes = kCacheFrames * 512U;

  DmxTakeFileReader() = default;
  ~DmxTakeFileReader();

  DmxTakeFileReader(const DmxTakeFileReader&) = delete;
  DmxTakeFileReader& operator=(const DmxTakeFileReader&) = delete;

  [[nodiscard]] bool open(const std::filesystem::path& path,
                          std::string& error_message);
  void close() noexcept;

  [[nodiscard]] bool read_frame(std::uint64_t frame_index,
                                DmxUniverse& frame,
                                std::string& error_message);
  [[nodiscard]] DmxTakeFileReaderInfo info() const;

  // Constant-time ownership handoff used by the dual-phase show switch: a
  // candidate is fully validated while the old reader remains authoritative,
  // then both file handles/caches are exchanged under their mutexes.
  void swap(DmxTakeFileReader& other);

 private:
  [[nodiscard]] bool load_cache_locked(std::uint64_t frame_index,
                                       std::string& error_message);
  void close_locked() noexcept;

  mutable std::mutex mutex_;
  std::ifstream input_;
  DmxTakeFileReaderInfo info_{};
  std::array<DmxUniverse, kCacheFrames> cache_{};
  std::uint64_t cache_start_frame_{0U};
  std::size_t cache_count_{0U};
};

static_assert(DmxTakeFileReader::kCacheBytes == 64U * 1024U,
              "AEYLA file-backed playback cache must remain 64 KiB");

}  // namespace aeyla::capture
