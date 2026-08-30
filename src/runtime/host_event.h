#pragma once

#include <cstdint>
#include <type_traits>

namespace aeyla::runtime {

enum class HostEventType : std::uint8_t {
  note_on,
  note_off,
  control_change,
  all_notes_off,
  transport_started,
  transport_stopped,
  transport_seek
};

// Compact event copied from the VST3 process callback into a bounded SPSC queue.
// It intentionally owns no memory and performs no work. MIDI `channel` is
// normalized at the wrapper boundary to the authored/user convention 1..16.
// For note events `note` is the MIDI note number; for control_change it carries
// the CC/controller number. `value` is normalized to 0..1 in both cases.
// `sample_offset` is the event position within the current host process block;
// `project_sample` is set only when the host provides a valid project sample
// position. Absolute host transport itself uses HostTransportMailbox rather
// than this queue so a MIDI burst cannot hide Stop/Seek/Loop truth.
struct HostEvent {
  HostEventType type{HostEventType::note_off};
  std::uint8_t channel{0};
  std::uint8_t note{0};
  std::uint8_t reserved{0};
  float value{0.0F};
  std::int32_t sample_offset{0};
  std::int64_t project_sample{-1};
};

static_assert(std::is_trivially_copyable_v<HostEvent>);
static_assert(std::is_trivially_destructible_v<HostEvent>);

}  // namespace aeyla::runtime
