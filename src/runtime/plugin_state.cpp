#include "runtime/plugin_state.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <new>
#include <set>
#include <string_view>
#include <utility>

namespace aeyla::runtime {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{'A', 'E', 'Y', 'L', 'V', 'S', 'T', '3'};
constexpr std::size_t kHeaderSize = 8 + 2 + 2 + 4;
constexpr std::size_t kFixedPayloadSize = 2 + 2 + 4 + 1 + 1 + 2 + 16 + 32 + 4;

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
  for (std::size_t index = 0; index < 8U; ++index)
    out.push_back(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
}

bool read_u16(std::span<const std::uint8_t> bytes, std::size_t& offset,
              std::uint16_t& value) noexcept {
  if (offset + 2 > bytes.size()) return false;
  value = static_cast<std::uint16_t>(bytes[offset]) |
          (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
  offset += 2;
  return true;
}

bool read_u32(std::span<const std::uint8_t> bytes, std::size_t& offset,
              std::uint32_t& value) noexcept {
  if (offset + 4 > bytes.size()) return false;
  value = static_cast<std::uint32_t>(bytes[offset]) |
          (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
          (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
          (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
  offset += 4;
  return true;
}

bool read_u64(std::span<const std::uint8_t> bytes, std::size_t& offset,
              std::uint64_t& value) noexcept {
  if (offset + 8U > bytes.size()) return false;
  value = 0U;
  for (std::size_t index = 0; index < 8U; ++index)
    value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8U);
  offset += 8U;
  return true;
}

bool valid_binding_id(std::string_view value) noexcept {
  if (value.empty() || value.size() > kMaxSessionSongIdBytes) return false;
  return std::all_of(value.begin(), value.end(), [](char character) {
    const auto ch = static_cast<unsigned char>(character);
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.';
  });
}

bool valid_take_file_name(std::string_view value) noexcept {
  constexpr std::string_view suffix = ".aeylatake";
  if (value.empty() || value.size() > kMaxTakeFileNameBytes ||
      value == "." || value == ".." ||
      value.find('\0') != std::string_view::npos ||
      value.find('/') != std::string_view::npos ||
      value.find('\\') != std::string_view::npos ||
      value.size() < suffix.size() ||
      value.substr(value.size() - suffix.size()) != suffix)
    return false;
  return true;
}

bool is_known_locator_mode(ProjectLocatorMode mode) noexcept {
  switch (mode) {
    case ProjectLocatorMode::none:
    case ProjectLocatorMode::relative_companion:
    case ProjectLocatorMode::absolute_development:
    case ProjectLocatorMode::project_library_id:
      return true;
  }
  return false;
}

bool is_safe_relative_locator(std::string_view locator) noexcept {
  if (locator.empty()) return false;
  if (locator.front() == '/' || locator.front() == '\\') return false;
  if (locator.size() >= 2 && locator[1] == ':') return false;
  if (locator.find('\0') != std::string_view::npos) return false;

  std::size_t start = 0;
  while (start <= locator.size()) {
    const auto end = locator.find_first_of("/\\", start);
    const auto component = locator.substr(
        start, end == std::string_view::npos ? locator.size() - start : end - start);
    if (component == "..") return false;
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return true;
}

PluginStateError validate_state(const PluginComponentState& state) noexcept {
  if (!std::isfinite(state.grand_master) || state.grand_master < 0.0F ||
      state.grand_master > 1.0F) {
    return PluginStateError::invalid_grand_master;
  }
  if (!is_known_locator_mode(state.locator_mode)) {
    return PluginStateError::invalid_locator_mode;
  }
  if (state.project_locator.size() > kMaxProjectLocatorBytes) {
    return PluginStateError::locator_too_large;
  }
  if (state.project_locator.find('\0') != std::string::npos) {
    return PluginStateError::invalid_argument;
  }
  if (state.locator_mode == ProjectLocatorMode::none && !state.project_locator.empty()) {
    return PluginStateError::inconsistent_locator;
  }
  if (state.locator_mode != ProjectLocatorMode::none && state.project_locator.empty()) {
    return PluginStateError::inconsistent_locator;
  }
  if (state.locator_mode == ProjectLocatorMode::relative_companion &&
      !is_safe_relative_locator(state.project_locator)) {
    return PluginStateError::unsafe_relative_locator;
  }
  if (state.song_bindings.size() > kMaxSessionSongBindings)
    return PluginStateError::invalid_song_binding;
  std::set<std::string> song_ids;
  for (const auto& binding : state.song_bindings) {
    if (!valid_binding_id(binding.song_id) ||
        !std::isfinite(binding.host_start_ppq) ||
        std::fabs(binding.host_start_ppq) > 1000000000.0 ||
        !song_ids.insert(binding.song_id).second) {
      return PluginStateError::invalid_song_binding;
    }
  }
  if (validate_show_midi_mapping(state.show_midi) !=
      ShowMidiMappingError::none)
    return PluginStateError::invalid_show_midi_mapping;
  if (state.take_library_locator.size() > kMaxTakeLibraryLocatorBytes ||
      state.take_library_locator.find('\0') != std::string::npos)
    return PluginStateError::invalid_take_binding;
  if (state.take_bindings.size() > kMaxSessionTakeBindings)
    return PluginStateError::invalid_take_binding;
  std::set<std::string> take_song_ids;
  for (const auto& binding : state.take_bindings) {
    const bool full_file = binding.start_frame == 0U &&
                           binding.end_frame_exclusive == 0U;
    const bool valid_trim = binding.end_frame_exclusive > binding.start_frame &&
                            binding.end_frame_exclusive - binding.start_frame >= 2U;
    if (!valid_binding_id(binding.song_id) ||
        !valid_take_file_name(binding.file_name) ||
        (!full_file && !valid_trim) ||
        !take_song_ids.insert(binding.song_id).second)
      return PluginStateError::invalid_take_binding;
  }
  return PluginStateError::none;
}

}  // namespace

PluginStateEncodeResult encode_plugin_component_state(
    const PluginComponentState& state) noexcept {
  PluginStateEncodeResult result;
  result.error = validate_state(state);
  if (result.error != PluginStateError::none) return result;

  const auto locator_size = static_cast<std::uint32_t>(state.project_locator.size());
  std::size_t binding_bytes = 2U;
  for (const auto& binding : state.song_bindings)
    binding_bytes += 2U + binding.song_id.size() + 8U;
  std::size_t take_binding_bytes =
      4U + state.take_library_locator.size() + 2U;
  for (const auto& binding : state.take_bindings)
    take_binding_bytes += 2U + binding.song_id.size() + 2U +
                          binding.file_name.size() + 8U + 8U;
  const auto payload_size = static_cast<std::uint32_t>(
      kFixedPayloadSize + locator_size + binding_bytes + 8U +
      take_binding_bytes + 2U);
  const auto total_size = kHeaderSize + static_cast<std::size_t>(payload_size);
  if (total_size > kMaxPluginStateBytes) {
    result.error = PluginStateError::locator_too_large;
    return result;
  }

  try {
    result.bytes.reserve(total_size);
    result.bytes.insert(result.bytes.end(), kMagic.begin(), kMagic.end());
    append_u16(result.bytes, kPluginStateFormatMajor);
    append_u16(result.bytes, kPluginStateFormatMinor);
    append_u32(result.bytes, payload_size);
    append_u16(result.bytes, state.project_schema_major);
    append_u16(result.bytes, state.project_schema_minor);
    append_u32(result.bytes, std::bit_cast<std::uint32_t>(state.grand_master));
    result.bytes.push_back(state.blackout ? 1U : 0U);
    result.bytes.push_back(static_cast<std::uint8_t>(state.locator_mode));
    append_u16(result.bytes, 0U);  // reserved
    result.bytes.insert(result.bytes.end(), state.project_uuid.begin(), state.project_uuid.end());
    result.bytes.insert(result.bytes.end(), state.project_checksum.begin(), state.project_checksum.end());
    append_u32(result.bytes, locator_size);
    result.bytes.insert(result.bytes.end(), state.project_locator.begin(), state.project_locator.end());
    append_u16(result.bytes,
               static_cast<std::uint16_t>(state.song_bindings.size()));
    for (const auto& binding : state.song_bindings) {
      append_u16(result.bytes,
                 static_cast<std::uint16_t>(binding.song_id.size()));
      result.bytes.insert(result.bytes.end(), binding.song_id.begin(),
                          binding.song_id.end());
      append_u64(result.bytes,
                 std::bit_cast<std::uint64_t>(binding.host_start_ppq));
    }
    append_u64(result.bytes, pack_show_midi_mapping(state.show_midi));
    append_u32(result.bytes, static_cast<std::uint32_t>(
        state.take_library_locator.size()));
    result.bytes.insert(result.bytes.end(), state.take_library_locator.begin(),
                        state.take_library_locator.end());
    append_u16(result.bytes,
               static_cast<std::uint16_t>(state.take_bindings.size()));
    for (const auto& binding : state.take_bindings) {
      append_u16(result.bytes,
                 static_cast<std::uint16_t>(binding.song_id.size()));
      result.bytes.insert(result.bytes.end(), binding.song_id.begin(),
                          binding.song_id.end());
      append_u16(result.bytes,
                 static_cast<std::uint16_t>(binding.file_name.size()));
      result.bytes.insert(result.bytes.end(), binding.file_name.begin(),
                          binding.file_name.end());
      append_u64(result.bytes, binding.start_frame);
      append_u64(result.bytes, binding.end_frame_exclusive);
    }
    // State 1.4: learned capture boundary notes. Appended so 1.3 readers can
    // safely ignore them while 1.4 restores them exactly.
    result.bytes.push_back(state.show_midi.capture_start_note);
    result.bytes.push_back(state.show_midi.capture_stop_note);
  } catch (const std::bad_alloc&) {
    result.bytes.clear();
    result.error = PluginStateError::allocation_failure;
  } catch (...) {
    result.bytes.clear();
    result.error = PluginStateError::invalid_argument;
  }
  return result;
}

PluginStateDecodeResult decode_plugin_component_state(
    std::span<const std::uint8_t> bytes) noexcept {
  PluginStateDecodeResult result;
  if (bytes.size() < kHeaderSize) {
    result.error = PluginStateError::truncated;
    return result;
  }
  if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
    result.error = PluginStateError::bad_magic;
    return result;
  }

  std::size_t offset = kMagic.size();
  std::uint16_t format_major = 0;
  std::uint16_t format_minor = 0;
  std::uint32_t payload_size = 0;
  if (!read_u16(bytes, offset, format_major) || !read_u16(bytes, offset, format_minor) ||
      !read_u32(bytes, offset, payload_size)) {
    result.error = PluginStateError::truncated;
    return result;
  }
  if (format_major != kPluginStateFormatMajor) {
    result.error = PluginStateError::unsupported_major_version;
    return result;
  }
  if (payload_size < kFixedPayloadSize || payload_size > kMaxPluginStateBytes - kHeaderSize ||
      offset + payload_size > bytes.size()) {
    result.error = PluginStateError::invalid_payload_size;
    return result;
  }
  const auto payload_end = offset + payload_size;

  std::uint32_t grand_master_bits = 0;
  std::uint16_t reserved = 0;
  if (!read_u16(bytes, offset, result.state.project_schema_major) ||
      !read_u16(bytes, offset, result.state.project_schema_minor) ||
      !read_u32(bytes, offset, grand_master_bits)) {
    result.error = PluginStateError::truncated;
    return result;
  }
  result.state.grand_master = std::bit_cast<float>(grand_master_bits);
  if (offset + 2 > payload_end) {
    result.error = PluginStateError::truncated;
    return result;
  }
  result.state.blackout = bytes[offset++] != 0;
  result.state.locator_mode = static_cast<ProjectLocatorMode>(bytes[offset++]);
  if (!read_u16(bytes, offset, reserved)) {
    result.error = PluginStateError::truncated;
    return result;
  }
  (void)reserved;

  if (offset + result.state.project_uuid.size() + result.state.project_checksum.size() > payload_end) {
    result.error = PluginStateError::truncated;
    return result;
  }
  std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
              result.state.project_uuid.size(), result.state.project_uuid.begin());
  offset += result.state.project_uuid.size();
  std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
              result.state.project_checksum.size(), result.state.project_checksum.begin());
  offset += result.state.project_checksum.size();

  std::uint32_t locator_size = 0;
  if (!read_u32(bytes, offset, locator_size)) {
    result.error = PluginStateError::truncated;
    return result;
  }
  if (locator_size > kMaxProjectLocatorBytes || offset + locator_size > payload_end) {
    result.error = PluginStateError::invalid_payload_size;
    return result;
  }
  try {
    result.state.project_locator.assign(
        reinterpret_cast<const char*>(bytes.data() + offset), locator_size);
  } catch (const std::bad_alloc&) {
    result.error = PluginStateError::allocation_failure;
    return result;
  } catch (...) {
    result.error = PluginStateError::invalid_argument;
    return result;
  }
  offset += locator_size;

  if (format_minor >= 1U) {
    std::uint16_t binding_count = 0U;
    if (!read_u16(bytes.first(payload_end), offset, binding_count) ||
        binding_count > kMaxSessionSongBindings) {
      result.error = PluginStateError::invalid_song_binding;
      return result;
    }
    try {
      result.state.song_bindings.reserve(binding_count);
      for (std::uint16_t index = 0U; index < binding_count; ++index) {
        std::uint16_t id_size = 0U;
        if (!read_u16(bytes.first(payload_end), offset, id_size) ||
            id_size == 0U || id_size > kMaxSessionSongIdBytes ||
            offset + id_size + 8U > payload_end) {
          result.error = PluginStateError::invalid_song_binding;
          return result;
        }
        SessionSongBinding binding;
        binding.song_id.assign(
            reinterpret_cast<const char*>(bytes.data() + offset), id_size);
        offset += id_size;
        std::uint64_t start_bits = 0U;
        if (!read_u64(bytes.first(payload_end), offset, start_bits)) {
          result.error = PluginStateError::invalid_song_binding;
          return result;
        }
        binding.host_start_ppq = std::bit_cast<double>(start_bits);
        result.state.song_bindings.push_back(std::move(binding));
      }
    } catch (...) {
      result.error = PluginStateError::allocation_failure;
      return result;
    }
  }

  if (format_minor >= 2U) {
    std::uint64_t packed_mapping = 0U;
    if (!read_u64(bytes.first(payload_end), offset, packed_mapping)) {
      result.error = PluginStateError::invalid_show_midi_mapping;
      return result;
    }
    result.state.show_midi = unpack_show_midi_mapping(packed_mapping);
  }

  if (format_minor >= 3U) {
    std::uint32_t take_locator_size = 0U;
    if (!read_u32(bytes.first(payload_end), offset, take_locator_size) ||
        take_locator_size > kMaxTakeLibraryLocatorBytes ||
        offset + take_locator_size > payload_end) {
      result.error = PluginStateError::invalid_take_binding;
      return result;
    }
    try {
      result.state.take_library_locator.assign(
          reinterpret_cast<const char*>(bytes.data() + offset),
          take_locator_size);
    } catch (...) {
      result.error = PluginStateError::allocation_failure;
      return result;
    }
    offset += take_locator_size;

    std::uint16_t take_count = 0U;
    if (!read_u16(bytes.first(payload_end), offset, take_count) ||
        take_count > kMaxSessionTakeBindings) {
      result.error = PluginStateError::invalid_take_binding;
      return result;
    }
    try {
      result.state.take_bindings.reserve(take_count);
      for (std::uint16_t index = 0U; index < take_count; ++index) {
        std::uint16_t id_size = 0U;
        std::uint16_t file_size = 0U;
        if (!read_u16(bytes.first(payload_end), offset, id_size) ||
            id_size == 0U || id_size > kMaxSessionSongIdBytes ||
            offset + id_size > payload_end) {
          result.error = PluginStateError::invalid_take_binding;
          return result;
        }
        SessionTakeBinding binding;
        binding.song_id.assign(
            reinterpret_cast<const char*>(bytes.data() + offset), id_size);
        offset += id_size;
        if (!read_u16(bytes.first(payload_end), offset, file_size) ||
            file_size == 0U || file_size > kMaxTakeFileNameBytes ||
            offset + file_size + 16U > payload_end) {
          result.error = PluginStateError::invalid_take_binding;
          return result;
        }
        binding.file_name.assign(
            reinterpret_cast<const char*>(bytes.data() + offset), file_size);
        offset += file_size;
        if (!read_u64(bytes.first(payload_end), offset, binding.start_frame) ||
            !read_u64(bytes.first(payload_end), offset,
                      binding.end_frame_exclusive)) {
          result.error = PluginStateError::invalid_take_binding;
          return result;
        }
        result.state.take_bindings.push_back(std::move(binding));
      }
    } catch (...) {
      result.error = PluginStateError::allocation_failure;
      return result;
    }
  }

  if (format_minor >= 4U) {
    if (offset + 2U > payload_end) {
      result.error = PluginStateError::invalid_show_midi_mapping;
      return result;
    }
    result.state.show_midi.capture_start_note = bytes[offset++];
    result.state.show_midi.capture_stop_note = bytes[offset++];
  }

  // Same-major future minor versions may append fields inside payload_size.
  if (offset > payload_end) {
    result.error = PluginStateError::invalid_payload_size;
    return result;
  }

  result.error = validate_state(result.state);
  return result;
}

const char* plugin_state_error_name(PluginStateError error) noexcept {
  switch (error) {
    case PluginStateError::none: return "none";
    case PluginStateError::invalid_argument: return "invalid_argument";
    case PluginStateError::locator_too_large: return "locator_too_large";
    case PluginStateError::unsafe_relative_locator: return "unsafe_relative_locator";
    case PluginStateError::allocation_failure: return "allocation_failure";
    case PluginStateError::truncated: return "truncated";
    case PluginStateError::bad_magic: return "bad_magic";
    case PluginStateError::unsupported_major_version: return "unsupported_major_version";
    case PluginStateError::invalid_payload_size: return "invalid_payload_size";
    case PluginStateError::invalid_locator_mode: return "invalid_locator_mode";
    case PluginStateError::invalid_grand_master: return "invalid_grand_master";
    case PluginStateError::inconsistent_locator: return "inconsistent_locator";
    case PluginStateError::invalid_song_binding: return "invalid_song_binding";
    case PluginStateError::invalid_show_midi_mapping: return "invalid_show_midi_mapping";
    case PluginStateError::invalid_take_binding: return "invalid_take_binding";
  }
  return "unknown";
}

}  // namespace aeyla::runtime
