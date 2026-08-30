#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace aeyla::project {

inline constexpr std::size_t kPersistentLiveMemoryCount = 4U;
inline constexpr std::size_t kMaximumLiveMemoryChannels = 512U;
inline constexpr std::uint32_t kMaximumPersistentLiveFadeMs = 60000U;

enum class PersistentLiveMemoryMode : std::uint8_t {
  toggle = 0U,
  fader = 1U,
};

enum class PersistentMidiBindingKind : std::uint8_t {
  none = 0U,
  note = 1U,
  control_change = 2U,
};

struct PersistentLiveMemoryChannel {
  std::uint16_t slot{1U};  // DMX slot 1..512.
  std::uint8_t value{0U};
  bool operator==(const PersistentLiveMemoryChannel&) const = default;
};

struct PersistentLiveMemory {
  bool configured{false};
  PersistentLiveMemoryMode mode{PersistentLiveMemoryMode::toggle};
  std::uint32_t fade_ms{1000U};
  PersistentMidiBindingKind midi_kind{PersistentMidiBindingKind::none};
  std::uint8_t midi_channel{0U};  // 1..16 when mapped, otherwise 0.
  std::uint8_t midi_number{0U};   // Note/CC 0..127 when mapped.
  std::vector<PersistentLiveMemoryChannel> channels;
  bool operator==(const PersistentLiveMemory&) const = default;
};

struct LiveMemoryPersistentState {
  std::array<PersistentLiveMemory, kPersistentLiveMemoryCount> memories{};
  bool operator==(const LiveMemoryPersistentState&) const = default;
};

struct LiveMemoryStateCodecResult {
  std::optional<LiveMemoryPersistentState> state;
  std::vector<std::string> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return state.has_value() && diagnostics.empty();
  }
};

[[nodiscard]] std::vector<std::string> validate_live_memory_persistent_state(
    const LiveMemoryPersistentState& state);

[[nodiscard]] std::vector<std::uint8_t> encode_live_memory_persistent_state(
    const LiveMemoryPersistentState& state,
    std::vector<std::string>& diagnostics);

[[nodiscard]] LiveMemoryStateCodecResult decode_live_memory_persistent_state(
    std::span<const std::uint8_t> bytes);

}  // namespace aeyla::project
