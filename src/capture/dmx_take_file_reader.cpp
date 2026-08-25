#include "capture/dmx_take_file_reader.h"

#include "capture/dmx_take_file_store.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>

namespace aeyla::capture {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{
    'A', 'E', 'Y', 'L', 'A', 'T', 'K', '1'};
constexpr std::size_t kFixedHeaderBytes = 40U;
constexpr std::uintmax_t kMaximumTakeFileBytes = 128U * 1024U * 1024U;
constexpr std::uint64_t kMaximumDurationSeconds = 60U * 60U;
constexpr std::uint32_t kMaximumSongIdBytes = 256U;
constexpr std::uint32_t kMaximumSongNameBytes = 512U;
constexpr std::uint32_t kMaximumTakeNameBytes = 512U;
constexpr std::uint32_t kMaximumSourceIpv4Bytes = 64U;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

struct Header {
  std::uint16_t version{0U};
  std::uint16_t port_address{0U};
  std::uint16_t frames_per_second{0U};
  std::uint16_t reserved{0U};
  std::uint64_t frame_count{0U};
  std::uint32_t song_id_length{0U};
  std::uint32_t song_name_length{0U};
  std::uint32_t take_name_length{0U};
  std::uint32_t source_length{0U};
};

bool read_u16(std::span<const std::uint8_t> bytes,
              std::size_t& cursor,
              std::uint16_t& value) noexcept {
  if(cursor + 2U > bytes.size()) return false;
  value = static_cast<std::uint16_t>(bytes[cursor]) |
          static_cast<std::uint16_t>(bytes[cursor + 1U] << 8U);
  cursor += 2U;
  return true;
}

bool read_u32(std::span<const std::uint8_t> bytes,
              std::size_t& cursor,
              std::uint32_t& value) noexcept {
  if(cursor + 4U > bytes.size()) return false;
  value = 0U;
  for(unsigned shift = 0U; shift < 32U; shift += 8U)
    value |= static_cast<std::uint32_t>(bytes[cursor++]) << shift;
  return true;
}

bool read_u64(std::span<const std::uint8_t> bytes,
              std::size_t& cursor,
              std::uint64_t& value) noexcept {
  if(cursor + 8U > bytes.size()) return false;
  value = 0U;
  for(unsigned shift = 0U; shift < 64U; shift += 8U)
    value |= static_cast<std::uint64_t>(bytes[cursor++]) << shift;
  return true;
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

std::uint64_t decode_u64(const std::array<std::uint8_t, 8>& bytes) noexcept {
  std::size_t cursor = 0U;
  std::uint64_t value = 0U;
  (void)read_u64(std::span<const std::uint8_t>(bytes.data(), bytes.size()),
                 cursor, value);
  return value;
}

bool read_exact(std::ifstream& input,
                std::uint8_t* data,
                std::size_t size) {
  if(size == 0U) return true;
  input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
  return input.gcount() == static_cast<std::streamsize>(size);
}

bool decode_header(const std::array<std::uint8_t, kFixedHeaderBytes>& bytes,
                   Header& header,
                   std::string& error_message) {
  if(!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
    error_message = "Not an AEYLA Take file or unsupported magic";
    return false;
  }

  std::size_t cursor = kMagic.size();
  if(!read_u16(bytes, cursor, header.version) ||
     !read_u16(bytes, cursor, header.port_address) ||
     !read_u16(bytes, cursor, header.frames_per_second) ||
     !read_u16(bytes, cursor, header.reserved) ||
     !read_u64(bytes, cursor, header.frame_count) ||
     !read_u32(bytes, cursor, header.song_id_length) ||
     !read_u32(bytes, cursor, header.song_name_length) ||
     !read_u32(bytes, cursor, header.take_name_length) ||
     !read_u32(bytes, cursor, header.source_length) ||
     cursor != bytes.size()) {
    error_message = "Take header is truncated";
    return false;
  }

  if(header.version != kDmxTakeFileVersion || header.reserved != 0U) {
    error_message = "Unsupported AEYLA Take file version";
    return false;
  }
  if(header.port_address > 0x7FFFU ||
     header.frames_per_second < 1U || header.frames_per_second > 60U ||
     header.frame_count == 0U ||
     header.frame_count > static_cast<std::uint64_t>(header.frames_per_second) *
                              kMaximumDurationSeconds ||
     header.song_id_length == 0U ||
     header.song_id_length > kMaximumSongIdBytes ||
     header.song_name_length > kMaximumSongNameBytes ||
     header.take_name_length == 0U ||
     header.take_name_length > kMaximumTakeNameBytes ||
     header.source_length > kMaximumSourceIpv4Bytes) {
    error_message = "Take header contains invalid bounded metadata";
    return false;
  }
  return true;
}

std::uint64_t metadata_bytes(const Header& header) noexcept {
  return static_cast<std::uint64_t>(header.song_id_length) +
         static_cast<std::uint64_t>(header.song_name_length) +
         static_cast<std::uint64_t>(header.take_name_length) +
         static_cast<std::uint64_t>(header.source_length);
}

std::uint64_t expected_size(const Header& header) noexcept {
  return static_cast<std::uint64_t>(kFixedHeaderBytes) +
         metadata_bytes(header) + header.frame_count * 512ULL + 8ULL;
}

bool read_string(std::ifstream& input,
                 std::uint32_t length,
                 std::string& value) {
  value.resize(length);
  return length == 0U ||
         read_exact(input,
                    reinterpret_cast<std::uint8_t*>(value.data()),
                    value.size());
}

}  // namespace

DmxTakeFileReader::~DmxTakeFileReader() { close(); }

bool DmxTakeFileReader::open(const std::filesystem::path& path,
                             std::string& error_message) {
  std::scoped_lock lock(mutex_);
  close_locked();
  error_message.clear();

  std::error_code fs_error;
  const auto file_size = std::filesystem::file_size(path, fs_error);
  if(fs_error || file_size < kFixedHeaderBytes + 8U ||
     file_size > kMaximumTakeFileBytes) {
    error_message = "Take file size is outside the supported bounds";
    return false;
  }

  std::ifstream verify(path, std::ios::binary);
  if(!verify) {
    error_message = "Could not open Take file";
    return false;
  }

  std::array<std::uint8_t, kFixedHeaderBytes> header_bytes{};
  if(!read_exact(verify, header_bytes.data(), header_bytes.size())) {
    error_message = "Take header is truncated";
    return false;
  }

  Header header;
  if(!decode_header(header_bytes, header, error_message)) return false;
  if(expected_size(header) != file_size) {
    error_message = "Take file length does not match its header";
    return false;
  }

  DmxTakeFileReaderInfo decoded;
  decoded.path = path;
  decoded.port_address = header.port_address;
  decoded.frames_per_second = header.frames_per_second;
  decoded.frame_count = header.frame_count;
  decoded.payload_offset =
      static_cast<std::uint64_t>(kFixedHeaderBytes) + metadata_bytes(header);

  if(!read_string(verify, header.song_id_length, decoded.song_id) ||
     !read_string(verify, header.song_name_length, decoded.song_name) ||
     !read_string(verify, header.take_name_length, decoded.take_name) ||
     !read_string(verify, header.source_length, decoded.source_ipv4)) {
    error_message = "Take metadata is truncated";
    return false;
  }

  // Full integrity validation without retaining payload. The file is immutable
  // once finalized, so one checksum pass at open is sufficient for playback.
  verify.clear();
  verify.seekg(0, std::ios::beg);
  if(!verify) {
    error_message = "Could not rewind Take for checksum validation";
    return false;
  }

  std::uint64_t remaining = static_cast<std::uint64_t>(file_size) - 8ULL;
  std::uint64_t hash = kFnvOffset;
  std::array<std::uint8_t, 64U * 1024U> hash_buffer{};
  while(remaining > 0U) {
    const std::size_t chunk = static_cast<std::size_t>(
        std::min<std::uint64_t>(remaining, hash_buffer.size()));
    if(!read_exact(verify, hash_buffer.data(), chunk)) {
      error_message = "Take checksum verification read failed";
      return false;
    }
    hash = fnv1a_update(hash, hash_buffer.data(), chunk);
    remaining -= chunk;
  }

  std::array<std::uint8_t, 8> trailer{};
  if(!read_exact(verify, trailer.data(), trailer.size())) {
    error_message = "Take checksum trailer is truncated";
    return false;
  }
  if(hash != decode_u64(trailer)) {
    error_message = "Take checksum mismatch; file may be corrupted";
    return false;
  }

  input_.open(path, std::ios::binary);
  if(!input_) {
    error_message = "Could not reopen validated Take for playback";
    return false;
  }

  decoded.open = true;
  info_ = std::move(decoded);
  cache_start_frame_ = 0U;
  cache_count_ = 0U;
  return true;
}

void DmxTakeFileReader::close() noexcept {
  const std::scoped_lock lock(mutex_);
  close_locked();
}

void DmxTakeFileReader::close_locked() noexcept {
  if(input_.is_open()) input_.close();
  info_ = {};
  cache_start_frame_ = 0U;
  cache_count_ = 0U;
}

bool DmxTakeFileReader::read_frame(std::uint64_t frame_index,
                                   DmxUniverse& frame,
                                   std::string& error_message) {
  const std::scoped_lock lock(mutex_);
  error_message.clear();
  if(!info_.open || !input_.is_open()) {
    error_message = "No validated Take file is open";
    return false;
  }
  if(frame_index >= info_.frame_count) {
    error_message = "Requested DMX frame is outside Take bounds";
    return false;
  }

  const bool cached = cache_count_ > 0U &&
      frame_index >= cache_start_frame_ &&
      frame_index < cache_start_frame_ + cache_count_;
  if(!cached && !load_cache_locked(frame_index, error_message))
    return false;

  frame = cache_[static_cast<std::size_t>(frame_index - cache_start_frame_)];
  return true;
}

bool DmxTakeFileReader::load_cache_locked(std::uint64_t frame_index,
                                          std::string& error_message) {
  const std::uint64_t block_start =
      (frame_index / static_cast<std::uint64_t>(kCacheFrames)) *
      static_cast<std::uint64_t>(kCacheFrames);
  const std::uint64_t available = info_.frame_count - block_start;
  const std::size_t count = static_cast<std::size_t>(
      std::min<std::uint64_t>(available, kCacheFrames));
  if(count == 0U) {
    error_message = "Take cache request resolved to zero frames";
    return false;
  }

  const std::uint64_t byte_offset =
      info_.payload_offset + block_start * 512ULL;
  if(byte_offset > static_cast<std::uint64_t>(
                       std::numeric_limits<std::streamoff>::max())) {
    error_message = "Take payload offset exceeds platform stream bounds";
    return false;
  }

  input_.clear();
  input_.seekg(static_cast<std::streamoff>(byte_offset), std::ios::beg);
  if(!input_) {
    error_message = "Could not seek Take playback cache";
    return false;
  }

  const std::size_t byte_count = count * 512U;
  input_.read(reinterpret_cast<char*>(cache_.data()),
              static_cast<std::streamsize>(byte_count));
  if(input_.gcount() != static_cast<std::streamsize>(byte_count)) {
    error_message = "Take playback cache read was truncated";
    cache_count_ = 0U;
    return false;
  }

  cache_start_frame_ = block_start;
  cache_count_ = count;
  return true;
}

DmxTakeFileReaderInfo DmxTakeFileReader::info() const {
  const std::scoped_lock lock(mutex_);
  return info_;
}

}  // namespace aeyla::capture
