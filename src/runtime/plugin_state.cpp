#include "runtime/plugin_state.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <new>
#include <string_view>

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
  return PluginStateError::none;
}

}  // namespace

PluginStateEncodeResult encode_plugin_component_state(
    const PluginComponentState& state) noexcept {
  PluginStateEncodeResult result;
  result.error = validate_state(state);
  if (result.error != PluginStateError::none) return result;

  const auto locator_size = static_cast<std::uint32_t>(state.project_locator.size());
  const auto payload_size = static_cast<std::uint32_t>(kFixedPayloadSize + locator_size);
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
  (void)format_minor;
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
  }
  return "unknown";
}

}  // namespace aeyla::runtime
