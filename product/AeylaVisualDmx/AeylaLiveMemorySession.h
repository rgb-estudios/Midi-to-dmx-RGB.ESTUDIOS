#pragma once

#include "capture/artnet_capture_worker.h"
#include "output/artnet_output_worker.h"
#include "project/live_memory_state.h"
#include "runtime/host_event.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace aeyla::live_memory_session {

inline constexpr std::size_t kOperatorMemoryCapacity = 8U;
inline constexpr std::size_t kOperatorMemoryCount = kOperatorMemoryCapacity;
inline constexpr std::size_t kDefaultOperatorMemoryCount = 4U;
static_assert(kOperatorMemoryCapacity == project::kPersistentLiveMemoryCapacity);
static_assert(kDefaultOperatorMemoryCount ==
              project::kDefaultPersistentLiveMemoryCount);

enum class MidiBindingKind : std::uint8_t {
  none = 0,
  note,
  control_change,
};

struct ActionResult {
  bool succeeded{false};
  std::string message;
};

struct MemoryView {
  bool configured{false};
  bool learning{false};
  std::string name;
  output::LiveMemoryControlMode mode{output::LiveMemoryControlMode::toggle};
  std::uint32_t fade_ms{1000U};
  std::size_t channel_count{0U};
  float level{0.0F};
  float target_level{0.0F};
  bool transitioning{false};

  bool midi_learning{false};
  MidiBindingKind midi_kind{MidiBindingKind::none};
  std::uint8_t midi_channel{0U};
  std::uint8_t midi_number{0U};
};

// Session data is scoped by the concrete plugin instance. Nothing is shared
// between two DAW instances even if they point to the same Art-Net network.
void register_runtime(const void* owner,
                      output::ArtNetOutputWorker* output_worker,
                      capture::ArtNetCaptureWorker* capture_worker);
void clear(const void* owner) noexcept;

[[nodiscard]] std::size_t memory_count(const void* owner) noexcept;
[[nodiscard]] MemoryView view(const void* owner, std::size_t index);
[[nodiscard]] ActionResult add_memory(const void* owner);
[[nodiscard]] ActionResult rename_memory(const void* owner,
                                         std::size_t index,
                                         std::string_view name);

// Persistence contains authored/operator configuration only. Runtime level,
// transition/LTP state, Learn baseline/pending state and physical ARM are never
// exported and therefore always restore safe/OFF.
[[nodiscard]] project::LiveMemoryPersistentState persistent_state(
    const void* owner);
[[nodiscard]] ActionResult restore_persistent_state(
    const void* owner,
    const project::LiveMemoryPersistentState& state);
[[nodiscard]] bool consume_persistence_dirty(const void* owner) noexcept;

// Two-step learn from Avolites:
// 1) first press while the Avolites memory is OFF captures the RX baseline;
// 2) second press while the Avolites memory is ON captures only the slots that
//    actually changed. This avoids treating unrelated zero-valued slots as part
//    of the learned memory.
[[nodiscard]] ActionResult learn_from_avolites(const void* owner,
                                               std::size_t index);
[[nodiscard]] ActionResult cancel_learn(const void* owner,
                                        std::size_t index);

[[nodiscard]] ActionResult toggle(const void* owner, std::size_t index);
[[nodiscard]] ActionResult set_fader_level(const void* owner,
                                           std::size_t index,
                                           float level);
[[nodiscard]] ActionResult cycle_fade(const void* owner,
                                      std::size_t index,
                                      int direction);
[[nodiscard]] ActionResult toggle_mode(const void* owner,
                                       std::size_t index);

// MIDI Learn is mode-aware. Toggle memories learn a Note; fader memories learn
// a Control Change. The incoming HostEvent is consumed on the non-realtime
// runtime thread; this module never runs network/mutex work in ProcessMidiMsg.
[[nodiscard]] ActionResult arm_midi_learn(const void* owner,
                                          std::size_t index);
[[nodiscard]] ActionResult clear_midi_binding(const void* owner,
                                              std::size_t index);
[[nodiscard]] bool process_midi_event(const void* owner,
                                      const runtime::HostEvent& event);

void reset_levels(const void* owner) noexcept;

}  // namespace aeyla::live_memory_session
