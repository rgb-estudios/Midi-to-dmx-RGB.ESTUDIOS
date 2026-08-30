#pragma once

#include "core/dmx_compiler.h"

#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace aeyla::output {

inline constexpr std::size_t kMaximumLiveMemories = 16U;
inline constexpr std::uint32_t kMaximumLiveMemoryFadeMs = 60000U;
using LiveMemoryMask = std::bitset<512U>;

enum class LiveMemoryControlMode : std::uint8_t {
  toggle = 0,
  fader,
};

struct LiveMemoryDefinition {
  std::string memory_id;
  std::string name;
  DmxUniverse target{};
  LiveMemoryMask mask{};
  LiveMemoryControlMode mode{LiveMemoryControlMode::toggle};
  std::uint32_t fade_ms{1000U};

  bool operator==(const LiveMemoryDefinition&) const = default;
};

struct LiveMemorySnapshot {
  bool configured{false};
  std::string memory_id;
  std::string name;
  LiveMemoryControlMode mode{LiveMemoryControlMode::toggle};
  std::uint32_t fade_ms{0U};
  std::size_t channel_count{0U};
  float level{0.0F};
  float target_level{0.0F};
  bool transitioning{false};
  std::uint64_t activation_serial{0U};
};

[[nodiscard]] bool validate_live_memory_definition(
    const LiveMemoryDefinition& definition,
    std::string& error_message) noexcept;

// Deterministic sparse/masked LTP-style layer for the live workspace.
//
// The engine never owns sockets and never decides output authority. It only
// composes live memories on top of a base DMX frame supplied by the existing
// Art-Net worker. This keeps ARM / DISARM / PANIC / fail-closed semantics in a
// single transport path.
//
// Each memory controls only its explicit mask. Active memories are applied in
// activation order (oldest -> newest). A newer memory therefore wins on
// overlapping channels, while fading it out continuously reveals the older
// memory or the underlying song/HOLD frame.
class DmxLiveMemoryEngine final {
 public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  [[nodiscard]] bool configure(std::size_t index,
                               const LiveMemoryDefinition& definition,
                               std::string& error_message);
  void clear(std::size_t index) noexcept;
  void clear_all() noexcept;

  [[nodiscard]] bool toggle(
      std::size_t index,
      TimePoint now = Clock::now()) noexcept;
  [[nodiscard]] bool set_target_level(
      std::size_t index,
      float level,
      TimePoint now = Clock::now()) noexcept;
  [[nodiscard]] bool set_direct_level(
      std::size_t index,
      float level,
      TimePoint now = Clock::now()) noexcept;

  // Safe boundary used by DISARM / PANIC / fail-closed. Definitions remain
  // available, but every live memory returns to OFF so re-arming can never
  // unexpectedly restore a previously active front light/haze/base look.
  void reset_levels() noexcept;

  [[nodiscard]] DmxUniverse compose(
      const DmxUniverse& base,
      TimePoint now = Clock::now()) noexcept;

  [[nodiscard]] LiveMemorySnapshot snapshot(
      std::size_t index,
      TimePoint now = Clock::now()) noexcept;

 private:
  struct RuntimeMemory {
    bool configured{false};
    LiveMemoryDefinition definition{};
    float start_level{0.0F};
    float current_level{0.0F};
    float target_level{0.0F};
    TimePoint transition_started{};
    std::chrono::milliseconds transition_duration{0};
    std::uint64_t activation_serial{0U};
  };

  static float clamp_level(float level) noexcept;
  static void update_level(RuntimeMemory& memory, TimePoint now) noexcept;
  void begin_transition(RuntimeMemory& memory,
                        float target,
                        TimePoint now) noexcept;

  mutable std::mutex mutex_;
  std::array<RuntimeMemory, kMaximumLiveMemories> memories_{};
  std::uint64_t next_activation_serial_{0U};
};

}  // namespace aeyla::output
