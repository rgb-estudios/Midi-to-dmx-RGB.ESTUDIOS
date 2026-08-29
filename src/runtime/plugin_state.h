#pragma once

#include "runtime/show_midi_control.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace aeyla::runtime {

inline constexpr std::uint16_t kPluginStateFormatMajor = 1;
inline constexpr std::uint16_t kPluginStateFormatMinor = 3;
inline constexpr std::size_t kMaxProjectLocatorBytes = 4096;
inline constexpr std::size_t kMaxTakeLibraryLocatorBytes = 4096;
inline constexpr std::size_t kMaxTakeFileNameBytes = 512;
inline constexpr std::size_t kMaxPluginStateBytes = 64 * 1024;
inline constexpr std::size_t kMaxSessionSongBindings = 15;
inline constexpr std::size_t kMaxSessionTakeBindings = 15;
inline constexpr std::size_t kMaxSessionSongIdBytes = 128;

enum class ProjectLocatorMode : std::uint8_t {
  none = 0,
  relative_companion = 1,
  absolute_development = 2,
  project_library_id = 3
};

struct SessionSongBinding {
  std::string song_id;
  double host_start_ppq{0.0};
  bool operator==(const SessionSongBinding&) const = default;
};

// Portable per-song DMX take selection. Only the basename is persisted so
// moving a library between Windows and macOS never bakes a platform path
// into every song. 0/0 is the only full-file sentinel.
struct SessionTakeBinding {
  std::string song_id;
  std::string file_name;
  std::uint64_t start_frame{0U};
  std::uint64_t end_frame_exclusive{0U};
  bool operator==(const SessionTakeBinding&) const = default;
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
  std::vector<SessionSongBinding> song_bindings{};
  ShowMidiMapping show_midi{};
  std::string take_library_locator{};
  std::vector<SessionTakeBinding> take_bindings{};

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
  inconsistent_locator,
  invalid_song_binding,
  invalid_show_midi_mapping,
  invalid_take_binding
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
