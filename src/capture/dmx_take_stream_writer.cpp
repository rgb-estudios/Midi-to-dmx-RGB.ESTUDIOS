#include "capture/dmx_take_stream_writer.h"

#include "capture/dmx_take_file_store.h"
#include "runtime/spsc_queue.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace aeyla::capture {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{
    'A', 'E', 'Y', 'L', 'A', 'T', 'K', '1'};
constexpr std::size_t kFixedHeaderBytes = 40U;
constexpr long kFrameCountOffset = 16L;
constexpr std::uint32_t kMaximumSongIdBytes = 256U;
constexpr std::uint32_t kMaximumSongNameBytes = 512U;
constexpr std::uint32_t kMaximumTakeNameBytes = 512U;
constexpr std::uint32_t kMaximumSourceIpv4Bytes = 64U;
constexpr std::uint64_t kMaximumDurationSeconds = 60U * 60U;
constexpr std::uintmax_t kMaximumTakeFileBytes = 128U * 1024U * 1024U;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  for(unsigned shift = 0U; shift < 32U; shift += 8U)
    bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
}

void append_u64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
  for(unsigned shift = 0U; shift < 64U; shift += 8U)
    bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
}

std::array<std::uint8_t, 8> encode_u64(std::uint64_t value) {
  std::array<std::uint8_t, 8> bytes{};
  for(unsigned shift = 0U; shift < 64U; shift += 8U)
    bytes[shift / 8U] = static_cast<std::uint8_t>((value >> shift) & 0xFFU);
  return bytes;
}

std::uint64_t fnv1a_update(std::uint64_t hash,
                           const std::uint8_t* data,
                           std::size_t size) noexcept {
  for(std::size_t index = 0U; index < size; ++index) {
    hash ^= static_cast<std::uint64_t>(data[index]);
    hash *= kFnvPrime;
  }
  return hash;
}

FILE* open_binary_update(const std::filesystem::path& path) {
#ifdef _WIN32
  FILE* file = nullptr;
  if(_wfopen_s(&file, path.c_str(), L"wb+") != 0) return nullptr;
  return file;
#else
  return std::fopen(path.c_str(), "wb+");
#endif
}

bool sync_file(FILE* file) noexcept {
  if(file == nullptr || std::fflush(file) != 0) return false;
#ifdef _WIN32
  return _commit(_fileno(file)) == 0;
#else
  return fsync(fileno(file)) == 0;
#endif
}

bool write_raw(FILE* file, const std::uint8_t* data, std::size_t size) noexcept {
  return size == 0U ||
         std::fwrite(data, 1U, size, file) == size;
}

bool write_text(FILE* file, const std::string& value) noexcept {
  return write_raw(file,
                   reinterpret_cast<const std::uint8_t*>(value.data()),
                   value.size());
}

std::filesystem::path temporary_path(const std::filesystem::path& target) {
  auto path = target;
  path += ".tmp";
  return path;
}

std::filesystem::path backup_path(const std::filesystem::path& target) {
  auto path = target;
  path += ".bak";
  return path;
}

void remove_quietly(const std::filesystem::path& path) noexcept {
  std::error_code error;
  (void)std::filesystem::remove(path, error);
}

bool validate_config(const DmxTakeStreamConfig& config,
                     std::string& error_message) {
  error_message.clear();
  if(config.target_path.empty() || config.target_path.filename().empty()) {
    error_message = "Streamed Take target path is empty";
    return false;
  }
  if(config.target_path.extension().string() !=
     std::string(kDmxTakeFileExtension)) {
    error_message = "Streamed Take target must use .aeylatake";
    return false;
  }
  if(config.song_id.empty() || config.song_id.size() > kMaximumSongIdBytes ||
     config.song_name.size() > kMaximumSongNameBytes ||
     config.take_name.empty() || config.take_name.size() > kMaximumTakeNameBytes ||
     config.source_ipv4.size() > kMaximumSourceIpv4Bytes) {
    error_message = "Streamed Take metadata is missing or exceeds bounds";
    return false;
  }
  if(config.port_address > 0x7FFFU) {
    error_message = "Streamed Take Art-Net port address exceeds 15 bits";
    return false;
  }
  if(config.frames_per_second < 1U || config.frames_per_second > 60U) {
    error_message = "Streamed Take FPS must be between 1 and 60";
    return false;
  }
  return true;
}

}  // namespace

class DmxTakeStreamWriter::Impl final {
 public:
  ~Impl() { abort(); }

  bool start(const DmxTakeStreamConfig& config,
             std::string& error_message) {
    abort();
    error_message.clear();
    if(!validate_config(config, error_message)) return false;

    std::string directory_error;
    if(!prepare_take_directory(config.target_path.parent_path(), directory_error)) {
      error_message = std::move(directory_error);
      return false;
    }

    config_ = config;
    temporary_path_ = temporary_path(config_.target_path);
    remove_quietly(temporary_path_);
    queue_.reset_consumer_side();
    frames_enqueued_.store(0U, std::memory_order_relaxed);
    frames_written_.store(0U, std::memory_order_relaxed);
    queue_overflows_.store(0U, std::memory_order_relaxed);
    peak_buffered_frames_.store(0U, std::memory_order_relaxed);
    failed_.store(false, std::memory_order_release);
    stop_requested_.store(false, std::memory_order_release);
    accepting_.store(false, std::memory_order_release);
    set_error({});

    file_ = open_binary_update(temporary_path_);
    if(file_ == nullptr) {
      error_message = "Could not open streamed Take temporary file";
      return false;
    }

    std::vector<std::uint8_t> header;
    header.reserve(kFixedHeaderBytes);
    header.insert(header.end(), kMagic.begin(), kMagic.end());
    append_u16(header, kDmxTakeFileVersion);
    append_u16(header, config_.port_address);
    append_u16(header, config_.frames_per_second);
    append_u16(header, 0U);
    append_u64(header, 0U);
    append_u32(header, static_cast<std::uint32_t>(config_.song_id.size()));
    append_u32(header, static_cast<std::uint32_t>(config_.song_name.size()));
    append_u32(header, static_cast<std::uint32_t>(config_.take_name.size()));
    append_u32(header, static_cast<std::uint32_t>(config_.source_ipv4.size()));
    if(header.size() != kFixedHeaderBytes ||
       !write_raw(file_, header.data(), header.size()) ||
       !write_text(file_, config_.song_id) ||
       !write_text(file_, config_.song_name) ||
       !write_text(file_, config_.take_name) ||
       !write_text(file_, config_.source_ipv4) ||
       std::fflush(file_) != 0) {
      close_file();
      remove_quietly(temporary_path_);
      error_message = "Could not initialize streamed Take file";
      return false;
    }

    running_.store(true, std::memory_order_release);
    accepting_.store(true, std::memory_order_release);
    try {
      worker_ = std::thread([this]() { run(); });
    } catch(...) {
      running_.store(false, std::memory_order_release);
      accepting_.store(false, std::memory_order_release);
      close_file();
      remove_quietly(temporary_path_);
      error_message = "Could not start streamed Take disk worker";
      return false;
    }
    return true;
  }

  bool try_push_frame(const DmxUniverse& frame) noexcept {
    if(!accepting_.load(std::memory_order_acquire) ||
       failed_.load(std::memory_order_acquire))
      return false;

    if(!queue_.try_push(frame)) {
      queue_overflows_.fetch_add(1U, std::memory_order_relaxed);
      fail("Streamed Take bounded queue overflow");
      accepting_.store(false, std::memory_order_release);
      wake_cv_.notify_one();
      return false;
    }

    const auto enqueued =
        frames_enqueued_.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const auto written = frames_written_.load(std::memory_order_relaxed);
    const auto buffered = enqueued > written
        ? static_cast<std::size_t>(enqueued - written)
        : 0U;
    auto peak = peak_buffered_frames_.load(std::memory_order_relaxed);
    while(buffered > peak &&
          !peak_buffered_frames_.compare_exchange_weak(
              peak, buffered, std::memory_order_relaxed,
              std::memory_order_relaxed)) {
    }
    wake_cv_.notify_one();
    return true;
  }

  bool finalize(std::string& error_message) {
    error_message.clear();
    accepting_.store(false, std::memory_order_release);
    stop_requested_.store(true, std::memory_order_release);
    wake_cv_.notify_one();
    if(worker_.joinable()) worker_.join();
    running_.store(false, std::memory_order_release);

    if(failed_.load(std::memory_order_acquire)) {
      error_message = error();
      close_file();
      remove_quietly(temporary_path_);
      return false;
    }

    const auto frame_count = frames_written_.load(std::memory_order_acquire);
    if(frame_count == 0U) {
      error_message = "Streamed Take contains no DMX frames";
      close_file();
      remove_quietly(temporary_path_);
      return false;
    }

    const auto maximum_frames =
        static_cast<std::uint64_t>(config_.frames_per_second) *
        kMaximumDurationSeconds;
    if(frame_count > maximum_frames) {
      error_message = "Streamed Take exceeds one-hour safety limit";
      close_file();
      remove_quietly(temporary_path_);
      return false;
    }

    if(!patch_frame_count(frame_count, true)) {
      error_message = "Could not finalize streamed Take frame count";
      close_file();
      remove_quietly(temporary_path_);
      return false;
    }

    const std::uint64_t metadata_bytes =
        config_.song_id.size() + config_.song_name.size() +
        config_.take_name.size() + config_.source_ipv4.size();
    const std::uint64_t content_bytes =
        static_cast<std::uint64_t>(kFixedHeaderBytes) + metadata_bytes +
        frame_count * 512ULL;
    if(content_bytes + 8ULL > kMaximumTakeFileBytes) {
      error_message = "Streamed Take exceeds 128 MiB file limit";
      close_file();
      remove_quietly(temporary_path_);
      return false;
    }

    if(std::fflush(file_) != 0 || std::fseek(file_, 0L, SEEK_SET) != 0) {
      error_message = "Could not rewind streamed Take for checksum";
      close_file();
      remove_quietly(temporary_path_);
      return false;
    }

    std::uint64_t hash = kFnvOffset;
    std::uint64_t remaining = content_bytes;
    std::array<std::uint8_t, 64U * 1024U> buffer{};
    while(remaining > 0U) {
      const std::size_t chunk = static_cast<std::size_t>(
          std::min<std::uint64_t>(remaining, buffer.size()));
      const std::size_t read = std::fread(buffer.data(), 1U, chunk, file_);
      if(read != chunk) {
        error_message = "Could not read streamed Take for checksum";
        close_file();
        remove_quietly(temporary_path_);
        return false;
      }
      hash = fnv1a_update(hash, buffer.data(), chunk);
      remaining -= chunk;
    }

    const auto trailer = encode_u64(hash);
    if(std::fseek(file_, 0L, SEEK_END) != 0 ||
       !write_raw(file_, trailer.data(), trailer.size()) ||
       !sync_file(file_)) {
      error_message = "Could not append streamed Take checksum";
      close_file();
      remove_quietly(temporary_path_);
      return false;
    }
    close_file();

    std::error_code fs_error;
    const auto final_size = std::filesystem::file_size(temporary_path_, fs_error);
    if(fs_error || final_size != content_bytes + 8ULL) {
      error_message = "Streamed Take final file size mismatch";
      remove_quietly(temporary_path_);
      return false;
    }

    const auto backup = backup_path(config_.target_path);
    const bool existed = std::filesystem::exists(config_.target_path, fs_error);
    if(fs_error) {
      error_message = "Could not inspect streamed Take target";
      remove_quietly(temporary_path_);
      return false;
    }
    if(existed) {
      remove_quietly(backup);
      std::filesystem::rename(config_.target_path, backup, fs_error);
      if(fs_error) {
        error_message = "Could not create streamed Take backup: " +
                        fs_error.message();
        remove_quietly(temporary_path_);
        return false;
      }
    }

    std::filesystem::rename(temporary_path_, config_.target_path, fs_error);
    if(fs_error) {
      if(existed) {
        std::error_code restore_error;
        std::filesystem::rename(backup, config_.target_path, restore_error);
      }
      error_message = "Could not install streamed Take: " + fs_error.message();
      remove_quietly(temporary_path_);
      return false;
    }
    return true;
  }

  void abort() noexcept {
    accepting_.store(false, std::memory_order_release);
    stop_requested_.store(true, std::memory_order_release);
    wake_cv_.notify_one();
    if(worker_.joinable()) worker_.join();
    running_.store(false, std::memory_order_release);
    close_file();
    if(!temporary_path_.empty()) remove_quietly(temporary_path_);
  }

  DmxTakeStreamStatus status() const {
    DmxTakeStreamStatus result;
    result.running = running_.load(std::memory_order_acquire);
    result.accepting_frames = accepting_.load(std::memory_order_acquire);
    result.failed = failed_.load(std::memory_order_acquire);
    result.frames_enqueued = frames_enqueued_.load(std::memory_order_relaxed);
    result.frames_written = frames_written_.load(std::memory_order_relaxed);
    result.queue_overflows = queue_overflows_.load(std::memory_order_relaxed);
    result.peak_buffered_frames =
        peak_buffered_frames_.load(std::memory_order_relaxed);
    result.target_path = config_.target_path;
    result.temporary_path = temporary_path_;
    result.error = error();
    return result;
  }

 private:
  void run() noexcept {
#ifdef _WIN32
    // DMX capture is only ~22 KiB/s at 44 Hz. Keep its disk worker in Windows
    // background mode so it cannot compete with REAPER's real-time audio I/O.
    (void)SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#endif
    while(!stop_requested_.load(std::memory_order_acquire) || !queue_.empty()) {
      DmxUniverse frame{};
      if(queue_.try_pop(frame)) {
        if(!write_raw(file_, frame.data(), frame.size())) {
          fail("Streamed Take disk write failed");
          accepting_.store(false, std::memory_order_release);
          break;
        }
        const auto written =
            frames_written_.fetch_add(1U, std::memory_order_release) + 1U;
        const auto checkpoint_frames =
            static_cast<std::uint64_t>(config_.frames_per_second) * 5U;
        if(checkpoint_frames > 0U && written % checkpoint_frames == 0U &&
           !patch_frame_count(written, false)) {
          fail("Streamed Take buffered checkpoint failed");
          accepting_.store(false, std::memory_order_release);
          break;
        }
        continue;
      }

      std::unique_lock lock(wake_mutex_);
      wake_cv_.wait_for(lock, std::chrono::milliseconds(50), [this]() {
        return stop_requested_.load(std::memory_order_acquire) ||
               !queue_.empty() || failed_.load(std::memory_order_acquire);
      });
      if(failed_.load(std::memory_order_acquire)) break;
    }
  }

  bool patch_frame_count(std::uint64_t frame_count, bool durable) noexcept {
    if(file_ == nullptr) return false;
    const long resume = std::ftell(file_);
    if(resume < 0L || std::fflush(file_) != 0 ||
       std::fseek(file_, kFrameCountOffset, SEEK_SET) != 0)
      return false;
    const auto bytes = encode_u64(frame_count);
    if(!write_raw(file_, bytes.data(), bytes.size()) ||
       std::fseek(file_, resume, SEEK_SET) != 0)
      return false;
    return durable ? sync_file(file_) : std::fflush(file_) == 0;
  }

  void close_file() noexcept {
    if(file_ != nullptr) {
      (void)std::fclose(file_);
      file_ = nullptr;
    }
  }

  void fail(std::string message) noexcept {
    failed_.store(true, std::memory_order_release);
    set_error(std::move(message));
  }

  void set_error(std::string message) noexcept {
    try {
      const std::scoped_lock lock(error_mutex_);
      error_ = std::move(message);
    } catch(...) {
    }
  }

  std::string error() const {
    const std::scoped_lock lock(error_mutex_);
    return error_;
  }

  DmxTakeStreamConfig config_;
  std::filesystem::path temporary_path_;
  FILE* file_{nullptr};

  runtime::SpscQueue<DmxUniverse, DmxTakeStreamWriter::kBufferedFrames + 1U>
      queue_;
  std::thread worker_;
  mutable std::mutex error_mutex_;
  std::string error_;
  std::mutex wake_mutex_;
  std::condition_variable wake_cv_;

  std::atomic<bool> running_{false};
  std::atomic<bool> accepting_{false};
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> failed_{false};
  std::atomic<std::uint64_t> frames_enqueued_{0U};
  std::atomic<std::uint64_t> frames_written_{0U};
  std::atomic<std::uint64_t> queue_overflows_{0U};
  std::atomic<std::size_t> peak_buffered_frames_{0U};
};

DmxTakeStreamWriter::DmxTakeStreamWriter()
    : impl_(std::make_unique<Impl>()) {}

DmxTakeStreamWriter::~DmxTakeStreamWriter() = default;

bool DmxTakeStreamWriter::start(const DmxTakeStreamConfig& config,
                                std::string& error_message) {
  return impl_->start(config, error_message);
}

bool DmxTakeStreamWriter::try_push_frame(const DmxUniverse& frame) noexcept {
  return impl_->try_push_frame(frame);
}

bool DmxTakeStreamWriter::finalize(std::string& error_message) {
  return impl_->finalize(error_message);
}

void DmxTakeStreamWriter::abort() noexcept {
  impl_->abort();
}

DmxTakeStreamStatus DmxTakeStreamWriter::status() const {
  return impl_->status();
}

}  // namespace aeyla::capture
