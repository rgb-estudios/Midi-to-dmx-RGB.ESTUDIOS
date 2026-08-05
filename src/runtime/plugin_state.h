#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace aeyla::runtime {

inline constexpr std::uint16_t kPluginStateFormatMajor = 1;
inline constexpr std::uint16_t kPluginStateFormatMinor = 0;
inline constexpr std::size_t kMaxProjectLocatorBytes = 4096;
inline constexpr std::size_t kMaxPluginStateBytes = 64 * 1024;

enum class ProjectLocatorMode : std::uint8_t {
  none = 0,
  relative_companion = 1,
  absolute_development = 2,
  project_library_id = 3
};

// Authoritative VST3 component state. Output Arm is deliberately absent: every
// instantiate/restore path starts disarmed regardless of previously saved UI.
struct PluginComponentState {
  std::array<std::uint8_t, 16> project_uuid{};
  std::array<std::uint8_t, 32> project_checksum{};
  std::uint16_t project_schema_major{1};
  std::uint16_t project_schema_minor{0};
  float grand_master{1.0F};
  bool blackout{true};
  ProjectLocatorMode locator_mode{ProjectLocatorMode::none};
  std::string project_locator{};

  bool operator==(const PluginComponentState&) const = default;
};

enum class PluginStateError : std::uint8_t {
  none,
  invalid_argument,
  locator_too_large,
  unsafe_relative_locator,
  allocation_failure,
  truncated,
  bad_magic,
  unsupported_major_version,
  invalid_payload_size,
  invalid_locator_mode,
  invalid_grand_master,
  inconsistent_locator
};

struct PluginStateEncodeResult {
  std::vector<std::uint8_t> bytes;
  PluginStateError error{PluginStateError::none};

  [[nodiscard]] bool ok() const noexcept { return error == PluginStateError::none; }
};

struct PluginStateDecodeResult {
  PluginComponentState state{};
  PluginStateError error{PluginStateError::none};

  [[nodiscard]] bool ok() const noexcept { return error == PluginStateError::none; }
};

[[nodiscard]] PluginStateEncodeResult encode_plugin_component_state(
    const PluginComponentState& state) noexcept;

[[nodiscard]] PluginStateDecodeResult decode_plugin_component_state(
    std::span<const std::uint8_t> bytes) noexcept;

[[nodiscard]] const char* plugin_state_error_name(PluginStateError error) noexcept;

}  // namespace aeyla::runtime
