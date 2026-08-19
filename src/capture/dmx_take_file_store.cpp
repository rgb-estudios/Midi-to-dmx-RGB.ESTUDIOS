#include "capture/dmx_take_file_store.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace aeyla::capture {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{
    'A', 'E', 'Y', 'L', 'A', 'T', 'K', '1'};
constexpr std::size_t kFixedHeaderBytes = 40U;
constexpr std::uint64_t kMaximumDurationSeconds = 60U * 60U;
constexpr std::uintmax_t kMaximumTakeFileBytes = 128U * 1024U * 1024U;
constexpr std::uint32_t kMaximumSongIdBytes = 256U;
constexpr std::uint32_t kMaximumSongNameBytes = 512U;
constexpr std::uint32_t kMaximumTakeNameBytes = 512U;
constexpr std::uint32_t kMaximumSourceIpv4Bytes = 64U;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void set_error(std::string& target, std::string message) {
  target = std::move(message);
}

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

bool read_u16(const std::vector<std::uint8_t>& bytes, std::size_t& cursor,
              std::uint16_t& value) noexcept {
  if(cursor + 2U > bytes.size()) return false;
  value = static_cast<std::uint16_t>(bytes[cursor]) |
          (static_cast<std::uint16_t>(bytes[cursor + 1U]) << 8U);
  cursor += 2U;
  return true;
}

bool read_u32(const std::vector<std::uint8_t>& bytes, std::size_t& cursor,
              std::uint32_t& value) noexcept {
  if(cursor + 4U > bytes.size()) return false;
  value = 0U;
  for(unsigned shift = 0U; shift < 32U; shift += 8U) {
    value |= static_cast<std::uint32_t>(bytes[cursor++]) << shift;
  }
  return true;
}

bool read_u64(const std::vector<std::uint8_t>& bytes, std::size_t& cursor,
              std::uint64_t& value) noexcept {
  if(cursor + 8U > bytes.size()) return false;
  value = 0U;
  for(unsigned shift = 0U; shift < 64U; shift += 8U) {
    value |= static_cast<std::uint64_t>(bytes[cursor++]) << shift;
  }
  return true;
}

std::uint64_t fnv1a(const std::uint8_t* data, std::size_t size) noexcept {
  std::uint64_t hash = kFnvOffset;
  for(std::size_t index = 0U; index < size; ++index) {
    hash ^= static_cast<std::uint64_t>(data[index]);
    hash *= kFnvPrime;
  }
  return hash;
}

bool valid_metadata_lengths(std::uint32_t song_id,
                            std::uint32_t song_name,
                            std::uint32_t take_name,
                            std::uint32_t source) noexcept {
  return song_id <= kMaximumSongIdBytes &&
         song_name <= kMaximumSongNameBytes &&
         take_name <= kMaximumTakeNameBytes &&
         source <= kMaximumSourceIpv4Bytes;
}

bool validate_take(const DmxTake& take, std::string& error_message) {
  if(take.frames_per_second < 1U || take.frames_per_second > 60U) {
    set_error(error_message, "Take FPS must be between 1 and 60");
    return false;
  }
  if(take.port_address > 0x7FFFU) {
    set_error(error_message, "Take Art-Net port address exceeds 15 bits");
    return false;
  }
  if(take.frames.empty()) {
    set_error(error_message, "Take contains no DMX frames");
    return false;
  }
  const std::uint64_t maximum_frames =
      static_cast<std::uint64_t>(take.frames_per_second) *
      kMaximumDurationSeconds;
  if(take.frames.size() > maximum_frames) {
    set_error(error_message, "Take exceeds the one-hour safety limit");
    return false;
  }
  if(take.name.size() > kMaximumTakeNameBytes) {
    set_error(error_message, "Take name is too long");
    return false;
  }
  if(take.source_ipv4.size() > kMaximumSourceIpv4Bytes) {
    set_error(error_message, "Take source IPv4 metadata is too long");
    return false;
  }
  return true;
}

FILE* open_binary_write(const std::filesystem::path& path) {
#ifdef _WIN32
  FILE* file = nullptr;
  if(_wfopen_s(&file, path.c_str(), L"wb") != 0) return nullptr;
  return file;
#else
  return std::fopen(path.c_str(), "wb");
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

bool write_bytes_sync(const std::filesystem::path& path,
                      const std::vector<std::uint8_t>& bytes,
                      std::string& error_message) {
  FILE* file = open_binary_write(path);
  if(file == nullptr) {
    set_error(error_message, "Could not open temporary Take file for writing");
    return false;
  }

  const std::size_t written =
      std::fwrite(bytes.data(), 1U, bytes.size(), file);
  if(written != bytes.size()) {
    set_error(error_message, "Short write while storing Take");
    std::fclose(file);
    return false;
  }
  if(!sync_file(file)) {
    set_error(error_message, "Could not flush Take to stable storage");
    std::fclose(file);
    return false;
  }
  if(std::fclose(file) != 0) {
    set_error(error_message, "Could not close Take file after writing");
    return false;
  }
  return true;
}

std::optional<std::vector<std::uint8_t>> read_bounded(
    const std::filesystem::path& source,
    std::string& error_message) {
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(source, error);
  if(error) {
    set_error(error_message, "Could not determine Take file size: " + error.message());
    return std::nullopt;
  }
  if(size < kFixedHeaderBytes + 8U || size > kMaximumTakeFileBytes) {
    set_error(error_message, "Take file size is outside the supported bounds");
    return std::nullopt;
  }

  std::ifstream input(source, std::ios::binary);
  if(!input) {
    set_error(error_message, "Could not open Take file");
    return std::nullopt;
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  if(input.gcount() != static_cast<std::streamsize>(bytes.size()) || input.bad()) {
    set_error(error_message, "Could not read the complete Take file");
    return std::nullopt;
  }
  return bytes;
}

std::filesystem::path temporary_path(const std::filesystem::path& target) {
  auto result = target;
  result += ".tmp";
  return result;
}

std::filesystem::path backup_path(const std::filesystem::path& target) {
  auto result = target;
  result += ".bak";
  return result;
}

void remove_quietly(const std::filesystem::path& path) noexcept {
  std::error_code error;
  (void)std::filesystem::remove(path, error);
}

std::string sanitized_component(std::string_view input) {
  std::string result;
  result.reserve(std::min<std::size_t>(input.size(), 64U));
  for(char character : input) {
    if(result.size() >= 64U) break;
    const unsigned char value = static_cast<unsigned char>(character);
    const bool portable = (value >= 'a' && value <= 'z') ||
                          (value >= 'A' && value <= 'Z') ||
                          (value >= '0' && value <= '9') ||
                          character == '-' || character == '_';
    if(portable)
      result.push_back(character);
    else if(character == ' ' || character == '.' || character == '/')
      result.push_back('_');
  }
  while(!result.empty() && result.back() == '_') result.pop_back();
  return result.empty() ? std::string("Take") : result;
}

std::string utc_file_stamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#ifdef _WIN32
  if(gmtime_s(&utc, &time) != 0) return "UTC";
#else
  if(gmtime_r(&time, &utc) == nullptr) return "UTC";
#endif
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count() % 1000LL;
  std::ostringstream stream;
  stream << std::put_time(&utc, "%Y%m%dT%H%M%S")
         << std::setw(3) << std::setfill('0') << milliseconds << 'Z';
  return stream.str();
}

struct DecodedHeader {
  std::uint16_t port_address{0U};
  std::uint16_t frames_per_second{0U};
  std::uint64_t frame_count{0U};
  std::uint32_t song_id_length{0U};
  std::uint32_t song_name_length{0U};
  std::uint32_t take_name_length{0U};
  std::uint32_t source_length{0U};
};

bool decode_fixed_header(const std::vector<std::uint8_t>& bytes,
                         DecodedHeader& header,
                         std::size_t& cursor,
                         std::string& error_message) {
  if(bytes.size() < kFixedHeaderBytes + 8U ||
     !std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
    set_error(error_message, "Not an AEYLA Take file or unsupported magic");
    return false;
  }
  cursor = kMagic.size();
  std::uint16_t version = 0U;
  std::uint16_t reserved = 0U;
  if(!read_u16(bytes, cursor, version) ||
     !read_u16(bytes, cursor, header.port_address) ||
     !read_u16(bytes, cursor, header.frames_per_second) ||
     !read_u16(bytes, cursor, reserved) ||
     !read_u64(bytes, cursor, header.frame_count) ||
     !read_u32(bytes, cursor, header.song_id_length) ||
     !read_u32(bytes, cursor, header.song_name_length) ||
     !read_u32(bytes, cursor, header.take_name_length) ||
     !read_u32(bytes, cursor, header.source_length)) {
    set_error(error_message, "Take header is truncated");
    return false;
  }
  if(version != kDmxTakeFileVersion || reserved != 0U) {
    set_error(error_message, "Unsupported AEYLA Take file version");
    return false;
  }
  if(header.port_address > 0x7FFFU ||
     header.frames_per_second < 1U || header.frames_per_second > 60U ||
     header.frame_count == 0U ||
     header.frame_count > static_cast<std::uint64_t>(header.frames_per_second) *
                              kMaximumDurationSeconds ||
     !valid_metadata_lengths(header.song_id_length, header.song_name_length,
                             header.take_name_length, header.source_length)) {
    set_error(error_message, "Take header contains invalid bounded metadata");
    return false;
  }
  return true;
}

bool read_string(const std::vector<std::uint8_t>& bytes,
                 std::size_t& cursor,
                 std::uint32_t length,
                 std::string& value) {
  if(cursor + static_cast<std::size_t>(length) > bytes.size()) return false;
  value.assign(reinterpret_cast<const char*>(bytes.data() + cursor), length);
  cursor += static_cast<std::size_t>(length);
  return true;
}

std::optional<StoredDmxTake> decode_complete(
    const std::filesystem::path& source,
    const std::vector<std::uint8_t>& bytes,
    std::string& error_message) {
  DecodedHeader header;
  std::size_t cursor = 0U;
  if(!decode_fixed_header(bytes, header, cursor, error_message))
    return std::nullopt;

  const std::uint64_t metadata_bytes =
      static_cast<std::uint64_t>(header.song_id_length) +
      static_cast<std::uint64_t>(header.song_name_length) +
      static_cast<std::uint64_t>(header.take_name_length) +
      static_cast<std::uint64_t>(header.source_length);
  const std::uint64_t payload_bytes = header.frame_count * 512ULL;
  const std::uint64_t expected = static_cast<std::uint64_t>(kFixedHeaderBytes) +
                                 metadata_bytes + payload_bytes + 8ULL;
  if(expected != bytes.size()) {
    set_error(error_message, "Take file length does not match its header");
    return std::nullopt;
  }

  StoredDmxTake result;
  result.source_path = source;
  if(!read_string(bytes, cursor, header.song_id_length, result.song_id) ||
     !read_string(bytes, cursor, header.song_name_length, result.song_name) ||
     !read_string(bytes, cursor, header.take_name_length, result.take.name) ||
     !read_string(bytes, cursor, header.source_length, result.take.source_ipv4)) {
    set_error(error_message, "Take metadata is truncated");
    return std::nullopt;
  }

  result.take.port_address = header.port_address;
  result.take.frames_per_second = header.frames_per_second;
  result.take.frames.resize(static_cast<std::size_t>(header.frame_count));
  for(auto& frame : result.take.frames) {
    if(cursor + frame.size() > bytes.size()) {
      set_error(error_message, "Take DMX payload is truncated");
      return std::nullopt;
    }
    std::memcpy(frame.data(), bytes.data() + cursor, frame.size());
    cursor += frame.size();
  }

  std::uint64_t stored_hash = 0U;
  if(!read_u64(bytes, cursor, stored_hash) || cursor != bytes.size()) {
    set_error(error_message, "Take checksum trailer is malformed");
    return std::nullopt;
  }
  const std::uint64_t computed_hash = fnv1a(bytes.data(), bytes.size() - 8U);
  if(stored_hash != computed_hash) {
    set_error(error_message, "Take checksum mismatch; file may be corrupted");
    return std::nullopt;
  }
  return result;
}

bool inspect_header(const std::filesystem::path& source,
                    TakeFileIndexEntry& entry) {
  std::error_code error;
  const auto size = std::filesystem::file_size(source, error);
  if(error || size < kFixedHeaderBytes + 8U || size > kMaximumTakeFileBytes)
    return false;

  std::ifstream input(source, std::ios::binary);
  if(!input) return false;
  std::vector<std::uint8_t> header_bytes(kFixedHeaderBytes);
  input.read(reinterpret_cast<char*>(header_bytes.data()),
             static_cast<std::streamsize>(header_bytes.size()));
  if(input.gcount() != static_cast<std::streamsize>(header_bytes.size()))
    return false;

  DecodedHeader header;
  std::size_t cursor = 0U;
  std::string decode_error;
  if(!decode_fixed_header(header_bytes, header, cursor, decode_error))
    return false;

  const std::uint64_t metadata_bytes =
      static_cast<std::uint64_t>(header.song_id_length) +
      static_cast<std::uint64_t>(header.song_name_length) +
      static_cast<std::uint64_t>(header.take_name_length) +
      static_cast<std::uint64_t>(header.source_length);
  const std::uint64_t expected = static_cast<std::uint64_t>(kFixedHeaderBytes) +
      metadata_bytes + header.frame_count * 512ULL + 8ULL;
  if(expected != size) return false;

  std::vector<char> metadata(static_cast<std::size_t>(metadata_bytes));
  if(!metadata.empty()) {
    input.read(metadata.data(), static_cast<std::streamsize>(metadata.size()));
    if(input.gcount() != static_cast<std::streamsize>(metadata.size()))
      return false;
  }

  std::size_t metadata_cursor = 0U;
  const auto take_text = [&](std::uint32_t length) -> std::string {
    const std::size_t count = static_cast<std::size_t>(length);
    if(metadata_cursor + count > metadata.size()) return {};
    std::string value(metadata.data() + metadata_cursor, count);
    metadata_cursor += count;
    return value;
  };
  entry.path = source;
  entry.song_id = take_text(header.song_id_length);
  entry.song_name = take_text(header.song_name_length);
  entry.take_name = take_text(header.take_name_length);
  entry.source_ipv4 = take_text(header.source_length);
  entry.port_address = header.port_address;
  entry.frames_per_second = header.frames_per_second;
  entry.frame_count = header.frame_count;
  entry.modified = std::filesystem::last_write_time(source, error);
  return !error;
}

}  // namespace

bool prepare_take_directory(const std::filesystem::path& directory,
                            std::string& error_message) {
  error_message.clear();
  if(directory.empty()) {
    set_error(error_message, "Take library directory is empty");
    return false;
  }
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if(error) {
    set_error(error_message, "Could not create Take library directory: " + error.message());
    return false;
  }
  if(!std::filesystem::is_directory(directory, error) || error) {
    set_error(error_message, "Selected Take library path is not a directory");
    return false;
  }

  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto probe = directory / (".aeyla_write_probe_" +
                                   std::to_string(stamp) + ".tmp");
  std::vector<std::uint8_t> payload{'A', 'E', 'Y', 'L', 'A'};
  if(!write_bytes_sync(probe, payload, error_message)) {
    remove_quietly(probe);
    return false;
  }
  remove_quietly(probe);
  return true;
}

std::filesystem::path make_take_file_path(
    const std::filesystem::path& directory,
    std::string_view song_name,
    std::string_view take_name) {
  const std::string base = sanitized_component(song_name) + "__" +
                           sanitized_component(take_name) + "__" +
                           utc_file_stamp();
  return directory / (base + std::string(kDmxTakeFileExtension));
}

bool save_take_file_atomic(const std::filesystem::path& target,
                           std::string_view song_id,
                           std::string_view song_name,
                           const DmxTake& take,
                           std::string& error_message) {
  error_message.clear();
  if(target.empty() || target.filename().empty()) {
    set_error(error_message, "Take target path is empty");
    return false;
  }
  if(song_id.empty() || song_id.size() > kMaximumSongIdBytes ||
     song_name.size() > kMaximumSongNameBytes) {
    set_error(error_message, "Song metadata is missing or exceeds Take bounds");
    return false;
  }
  if(!validate_take(take, error_message)) return false;

  std::string directory_error;
  if(!prepare_take_directory(target.parent_path(), directory_error)) {
    set_error(error_message, directory_error);
    return false;
  }

  const std::uint64_t frame_count = take.frames.size();
  const std::uint64_t metadata_bytes = song_id.size() + song_name.size() +
                                       take.name.size() + take.source_ipv4.size();
  const std::uint64_t total_without_checksum =
      static_cast<std::uint64_t>(kFixedHeaderBytes) + metadata_bytes +
      frame_count * 512ULL;
  if(total_without_checksum + 8ULL > kMaximumTakeFileBytes) {
    set_error(error_message, "Encoded Take exceeds the 128 MiB file limit");
    return false;
  }

  std::vector<std::uint8_t> bytes;
  bytes.reserve(static_cast<std::size_t>(total_without_checksum + 8ULL));
  bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
  append_u16(bytes, kDmxTakeFileVersion);
  append_u16(bytes, take.port_address);
  append_u16(bytes, take.frames_per_second);
  append_u16(bytes, 0U);
  append_u64(bytes, frame_count);
  append_u32(bytes, static_cast<std::uint32_t>(song_id.size()));
  append_u32(bytes, static_cast<std::uint32_t>(song_name.size()));
  append_u32(bytes, static_cast<std::uint32_t>(take.name.size()));
  append_u32(bytes, static_cast<std::uint32_t>(take.source_ipv4.size()));
  bytes.insert(bytes.end(), song_id.begin(), song_id.end());
  bytes.insert(bytes.end(), song_name.begin(), song_name.end());
  bytes.insert(bytes.end(), take.name.begin(), take.name.end());
  bytes.insert(bytes.end(), take.source_ipv4.begin(), take.source_ipv4.end());
  for(const auto& frame : take.frames)
    bytes.insert(bytes.end(), frame.begin(), frame.end());
  append_u64(bytes, fnv1a(bytes.data(), bytes.size()));

  const auto temporary = temporary_path(target);
  const auto backup = backup_path(target);
  remove_quietly(temporary);
  if(!write_bytes_sync(temporary, bytes, error_message)) {
    remove_quietly(temporary);
    return false;
  }

  std::string verify_error;
  const auto verified = load_take_file(temporary, verify_error);
  if(!verified.has_value() || verified->song_id != song_id ||
     verified->song_name != song_name || verified->take.name != take.name ||
     verified->take.frames.size() != take.frames.size()) {
    set_error(error_message, "Temporary Take failed deterministic read-back: " +
                             verify_error);
    remove_quietly(temporary);
    return false;
  }

  std::error_code error;
  const bool existed = std::filesystem::exists(target, error);
  if(error) {
    set_error(error_message, "Could not inspect existing Take target: " + error.message());
    remove_quietly(temporary);
    return false;
  }
  if(existed) {
    remove_quietly(backup);
    std::filesystem::rename(target, backup, error);
    if(error) {
      set_error(error_message, "Could not create Take backup: " + error.message());
      remove_quietly(temporary);
      return false;
    }
  }

  std::filesystem::rename(temporary, target, error);
  if(error) {
    if(existed) {
      std::error_code restore_error;
      std::filesystem::rename(backup, target, restore_error);
    }
    set_error(error_message, "Could not install completed Take: " + error.message());
    remove_quietly(temporary);
    return false;
  }
  return true;
}

std::optional<StoredDmxTake> load_take_file(
    const std::filesystem::path& source,
    std::string& error_message) {
  error_message.clear();
  const auto bytes = read_bounded(source, error_message);
  if(!bytes.has_value()) return std::nullopt;
  return decode_complete(source, *bytes, error_message);
}

TakeLibraryScanResult scan_take_directory(
    const std::filesystem::path& directory,
    std::string_view song_id_filter) {
  TakeLibraryScanResult result;
  if(directory.empty()) {
    result.error = "Take library directory is empty";
    return result;
  }
  std::error_code error;
  if(!std::filesystem::is_directory(directory, error) || error) {
    result.error = "Take library directory is unavailable";
    return result;
  }

  std::filesystem::directory_iterator iterator(directory, error);
  const std::filesystem::directory_iterator end;
  if(error) {
    result.error = "Could not enumerate Take library: " + error.message();
    return result;
  }
  for(; iterator != end; iterator.increment(error)) {
    if(error) {
      result.error = "Take library enumeration failed: " + error.message();
      return result;
    }
    const auto& entry = *iterator;
    if(!entry.is_regular_file(error) || error) {
      error.clear();
      continue;
    }
    if(entry.path().extension() != kDmxTakeFileExtension)
      continue;

    TakeFileIndexEntry indexed;
    if(!inspect_header(entry.path(), indexed)) {
      ++result.invalid_files;
      continue;
    }
    if(!song_id_filter.empty() && indexed.song_id != song_id_filter)
      continue;
    result.entries.push_back(std::move(indexed));
  }

  std::sort(result.entries.begin(), result.entries.end(),
            [](const TakeFileIndexEntry& a, const TakeFileIndexEntry& b) {
              if(a.modified != b.modified) return a.modified > b.modified;
              return a.path.filename().string() > b.path.filename().string();
            });
  return result;
}

}  // namespace aeyla::capture
