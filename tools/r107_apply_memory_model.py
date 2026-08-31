from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}\n--- needle ---\n{old[:500]}")
    p.write_text(text.replace(old, new), encoding="utf-8")


Path("src/project/live_memory_state.h").write_text(r'''#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace aeyla::project {

inline constexpr std::size_t kPersistentLiveMemoryCapacity = 8U;
// Compatibility alias for code that treats this as the compile-time storage bound.
inline constexpr std::size_t kPersistentLiveMemoryCount = kPersistentLiveMemoryCapacity;
inline constexpr std::size_t kDefaultPersistentLiveMemoryCount = 4U;
inline constexpr std::size_t kMaximumLiveMemoryChannels = 512U;
inline constexpr std::size_t kMaximumPersistentLiveMemoryNameBytes = 48U;
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
  std::string name;
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
  std::uint8_t memory_count{
      static_cast<std::uint8_t>(kDefaultPersistentLiveMemoryCount)};
  std::array<PersistentLiveMemory, kPersistentLiveMemoryCapacity> memories{};

  LiveMemoryPersistentState() {
    static constexpr std::array<const char*, kPersistentLiveMemoryCapacity>
        kDefaultNames{
            "FRONTAL", "HUMO / HAZE", "BASE BLANCA", "TEST LUMINARIAS",
            "MEMORIA 5", "MEMORIA 6", "MEMORIA 7", "MEMORIA 8"};
    for(std::size_t index = 0U; index < memories.size(); ++index)
      memories[index].name = kDefaultNames[index];

    // Match the operator workspace defaults exactly: HUMO/HAZE is a continuous
    // fader even before it has learned any DMX channels. This keeps legacy
    // packages and fresh projects identical to a freshly initialized session.
    memories[1].mode = PersistentLiveMemoryMode::fader;
  }

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
''', encoding="utf-8")


Path("src/project/live_memory_state.cpp").write_text(r'''#include "project/live_memory_state.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace aeyla::project {
namespace {

constexpr std::array<std::uint8_t, 8U> kMagic{
    'A', 'E', 'Y', 'L', 'A', 'L', 'I', 'V'};
constexpr std::uint16_t kLegacyFormatVersion = 1U;
constexpr std::uint16_t kFormatVersion = 2U;
constexpr std::size_t kFixedHeaderBytes = 12U;
constexpr std::size_t kMemoryHeaderBytes = 12U;
constexpr std::size_t kEncodedChannelBytes = 3U;
constexpr std::size_t kMaximumEncodedBytes =
    kFixedHeaderBytes +
    kPersistentLiveMemoryCapacity *
        (kMemoryHeaderBytes + kMaximumPersistentLiveMemoryNameBytes +
         kMaximumLiveMemoryChannels * kEncodedChannelBytes);

void append_u8(std::vector<std::uint8_t>& bytes, std::uint8_t value) {
  bytes.push_back(value);
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

class Reader final {
 public:
  explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  [[nodiscard]] bool read_u8(std::uint8_t& value) noexcept {
    if(offset_ >= bytes_.size()) return false;
    value = bytes_[offset_++];
    return true;
  }

  [[nodiscard]] bool read_u16(std::uint16_t& value) noexcept {
    if(bytes_.size() - offset_ < 2U) return false;
    value = static_cast<std::uint16_t>(bytes_[offset_]) |
            static_cast<std::uint16_t>(bytes_[offset_ + 1U] << 8U);
    offset_ += 2U;
    return true;
  }

  [[nodiscard]] bool read_u32(std::uint32_t& value) noexcept {
    if(bytes_.size() - offset_ < 4U) return false;
    value = static_cast<std::uint32_t>(bytes_[offset_]) |
            (static_cast<std::uint32_t>(bytes_[offset_ + 1U]) << 8U) |
            (static_cast<std::uint32_t>(bytes_[offset_ + 2U]) << 16U) |
            (static_cast<std::uint32_t>(bytes_[offset_ + 3U]) << 24U);
    offset_ += 4U;
    return true;
  }

  [[nodiscard]] bool read_string(std::size_t size, std::string& value) {
    if(bytes_.size() - offset_ < size) return false;
    value.assign(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
    offset_ += size;
    return true;
  }

  [[nodiscard]] std::size_t remaining() const noexcept {
    return bytes_.size() - offset_;
  }

 private:
  std::span<const std::uint8_t> bytes_;
  std::size_t offset_{0U};
};

std::string memory_path(std::size_t index) {
  return "liveMemories[" + std::to_string(index) + "]";
}

bool known_mode(PersistentLiveMemoryMode mode) noexcept {
  return mode == PersistentLiveMemoryMode::toggle ||
         mode == PersistentLiveMemoryMode::fader;
}

bool known_midi_kind(PersistentMidiBindingKind kind) noexcept {
  return kind == PersistentMidiBindingKind::none ||
         kind == PersistentMidiBindingKind::note ||
         kind == PersistentMidiBindingKind::control_change;
}

bool valid_name(std::string_view name) noexcept {
  if(name.empty() || name.size() > kMaximumPersistentLiveMemoryNameBytes)
    return false;
  return std::none_of(name.begin(), name.end(), [](unsigned char value) {
    return value == 0U || value == '\n' || value == '\r';
  });
}

std::uint32_t midi_binding_key(PersistentMidiBindingKind kind,
                               std::uint8_t channel,
                               std::uint8_t number) noexcept {
  return (static_cast<std::uint32_t>(kind) << 16U) |
         (static_cast<std::uint32_t>(channel) << 8U) |
         static_cast<std::uint32_t>(number);
}

}  // namespace

std::vector<std::string> validate_live_memory_persistent_state(
    const LiveMemoryPersistentState& state) {
  std::vector<std::string> diagnostics;
  std::set<std::uint32_t> midi_bindings;

  if(state.memory_count == 0U ||
     state.memory_count > kPersistentLiveMemoryCapacity) {
    diagnostics.push_back("liveMemories: active memory count must be 1..8");
    return diagnostics;
  }

  for(std::size_t index = 0U; index < state.memories.size(); ++index) {
    const auto& memory = state.memories[index];
    const std::string path = memory_path(index);
    const bool active = index < static_cast<std::size_t>(state.memory_count);

    if(!active) {
      if(memory.configured || !memory.channels.empty() ||
         memory.midi_kind != PersistentMidiBindingKind::none ||
         memory.midi_channel != 0U || memory.midi_number != 0U)
        diagnostics.push_back(path + ": inactive memory must not own DMX or MIDI state");
      continue;
    }

    if(!valid_name(memory.name))
      diagnostics.push_back(path + ".name: must contain 1..48 UTF-8 bytes without line breaks");
    if(!known_mode(memory.mode))
      diagnostics.push_back(path + ".mode: unsupported mode");
    if(memory.fade_ms > kMaximumPersistentLiveFadeMs)
      diagnostics.push_back(path + ".fadeMs: fade exceeds 60000 ms");
    if(!known_midi_kind(memory.midi_kind))
      diagnostics.push_back(path + ".midi.kind: unsupported MIDI binding kind");

    if(memory.configured) {
      if(memory.channels.empty())
        diagnostics.push_back(path + ".channels: configured memory requires at least one DMX channel");
    } else if(!memory.channels.empty()) {
      diagnostics.push_back(path + ".channels: unconfigured memory must not own DMX channels");
    }

    if(memory.channels.size() > kMaximumLiveMemoryChannels)
      diagnostics.push_back(path + ".channels: channel count exceeds 512");

    std::uint16_t previous_slot = 0U;
    for(std::size_t channel_index = 0U;
        channel_index < memory.channels.size(); ++channel_index) {
      const auto& channel = memory.channels[channel_index];
      const std::string channel_path = path + ".channels[" +
          std::to_string(channel_index) + "]";
      if(channel.slot == 0U || channel.slot > 512U)
        diagnostics.push_back(channel_path + ".slot: DMX slot must be 1..512");
      if(channel.slot <= previous_slot)
        diagnostics.push_back(channel_path + ".slot: DMX slots must be strictly increasing and unique");
      previous_slot = channel.slot;
    }

    if(memory.midi_kind == PersistentMidiBindingKind::none) {
      if(memory.midi_channel != 0U || memory.midi_number != 0U)
        diagnostics.push_back(path + ".midi: unmapped memory must store channel=0 and number=0");
    } else {
      if(memory.midi_channel == 0U || memory.midi_channel > 16U)
        diagnostics.push_back(path + ".midi.channel: MIDI channel must be 1..16");
      if(memory.midi_number > 127U)
        diagnostics.push_back(path + ".midi.number: MIDI Note/CC must be 0..127");
      if(memory.mode == PersistentLiveMemoryMode::toggle &&
         memory.midi_kind != PersistentMidiBindingKind::note)
        diagnostics.push_back(path + ".midi.kind: BOTON/TOGGLE memories may only map MIDI Note");
      if(memory.mode == PersistentLiveMemoryMode::fader &&
         memory.midi_kind != PersistentMidiBindingKind::control_change)
        diagnostics.push_back(path + ".midi.kind: FADER memories may only map MIDI CC");

      const auto key = midi_binding_key(
          memory.midi_kind, memory.midi_channel, memory.midi_number);
      if(!midi_bindings.insert(key).second)
        diagnostics.push_back(path + ".midi: duplicate live-memory MIDI binding");
    }
  }

  return diagnostics;
}

std::vector<std::uint8_t> encode_live_memory_persistent_state(
    const LiveMemoryPersistentState& state,
    std::vector<std::string>& diagnostics) {
  diagnostics = validate_live_memory_persistent_state(state);
  if(!diagnostics.empty()) return {};

  std::vector<std::uint8_t> bytes;
  bytes.reserve(kMaximumEncodedBytes);
  bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
  append_u16(bytes, kFormatVersion);
  append_u8(bytes, state.memory_count);
  append_u8(bytes, 0U);

  for(std::size_t index = 0U;
      index < static_cast<std::size_t>(state.memory_count); ++index) {
    const auto& memory = state.memories[index];
    append_u8(bytes, memory.configured ? 1U : 0U);
    append_u8(bytes, static_cast<std::uint8_t>(memory.mode));
    append_u8(bytes, static_cast<std::uint8_t>(memory.midi_kind));
    append_u8(bytes, memory.midi_channel);
    append_u8(bytes, memory.midi_number);
    append_u8(bytes, static_cast<std::uint8_t>(memory.name.size()));
    append_u32(bytes, memory.fade_ms);
    append_u16(bytes, static_cast<std::uint16_t>(memory.channels.size()));
    bytes.insert(bytes.end(), memory.name.begin(), memory.name.end());
    for(const auto& channel : memory.channels) {
      append_u16(bytes, channel.slot);
      append_u8(bytes, channel.value);
    }
  }

  return bytes;
}

LiveMemoryStateCodecResult decode_live_memory_persistent_state(
    std::span<const std::uint8_t> bytes) {
  LiveMemoryStateCodecResult result;
  if(bytes.size() < kFixedHeaderBytes) {
    result.diagnostics.push_back("live.bin: truncated header");
    return result;
  }
  if(bytes.size() > kMaximumEncodedBytes) {
    result.diagnostics.push_back("live.bin: payload exceeds bounded size");
    return result;
  }

  Reader reader(bytes);
  for(std::size_t index = 0U; index < kMagic.size(); ++index) {
    std::uint8_t value = 0U;
    if(!reader.read_u8(value) || value != kMagic[index]) {
      result.diagnostics.push_back("live.bin: invalid magic");
      return result;
    }
  }

  std::uint16_t version = 0U;
  std::uint8_t memory_count = 0U;
  std::uint8_t reserved = 0U;
  if(!reader.read_u16(version) || !reader.read_u8(memory_count) ||
     !reader.read_u8(reserved)) {
    result.diagnostics.push_back("live.bin: truncated format header");
    return result;
  }
  if(version != kLegacyFormatVersion && version != kFormatVersion) {
    result.diagnostics.push_back("live.bin: unsupported format version " +
                                 std::to_string(version));
    return result;
  }
  if(version == kLegacyFormatVersion &&
     memory_count != kDefaultPersistentLiveMemoryCount) {
    result.diagnostics.push_back("live.bin v1: memory count must be exactly 4");
    return result;
  }
  if(version == kFormatVersion &&
     (memory_count == 0U || memory_count > kPersistentLiveMemoryCapacity)) {
    result.diagnostics.push_back("live.bin v2: memory count must be 1..8");
    return result;
  }
  if(reserved != 0U) {
    result.diagnostics.push_back("live.bin: non-zero reserved header byte");
    return result;
  }

  LiveMemoryPersistentState state;
  state.memory_count = memory_count;
  for(std::size_t index = 0U;
      index < static_cast<std::size_t>(memory_count); ++index) {
    std::uint8_t configured = 0U;
    std::uint8_t mode = 0U;
    std::uint8_t midi_kind = 0U;
    std::uint8_t midi_channel = 0U;
    std::uint8_t midi_number = 0U;
    std::uint8_t name_length = 0U;
    std::uint32_t fade_ms = 0U;
    std::uint16_t channel_count = 0U;
    if(!reader.read_u8(configured) || !reader.read_u8(mode) ||
       !reader.read_u8(midi_kind) || !reader.read_u8(midi_channel) ||
       !reader.read_u8(midi_number) || !reader.read_u8(name_length) ||
       !reader.read_u32(fade_ms) || !reader.read_u16(channel_count)) {
      result.diagnostics.push_back(memory_path(index) + ": truncated memory header");
      return result;
    }
    if(configured > 1U) {
      result.diagnostics.push_back(memory_path(index) + ".configured: invalid boolean byte");
      return result;
    }
    if(version == kLegacyFormatVersion && name_length != 0U) {
      result.diagnostics.push_back(memory_path(index) + ": non-zero legacy reserved byte");
      return result;
    }
    if(version == kFormatVersion &&
       (name_length == 0U || name_length > kMaximumPersistentLiveMemoryNameBytes)) {
      result.diagnostics.push_back(memory_path(index) + ".name: invalid encoded name length");
      return result;
    }
    const std::size_t encoded_name_bytes =
        version == kFormatVersion ? static_cast<std::size_t>(name_length) : 0U;
    if(channel_count > kMaximumLiveMemoryChannels ||
       reader.remaining() < encoded_name_bytes +
                                static_cast<std::size_t>(channel_count) *
                                    kEncodedChannelBytes) {
      result.diagnostics.push_back(memory_path(index) +
                                   ".channels: invalid or truncated payload");
      return result;
    }

    auto& memory = state.memories[index];
    if(version == kFormatVersion) {
      std::string name;
      if(!reader.read_string(encoded_name_bytes, name)) {
        result.diagnostics.push_back(memory_path(index) + ".name: truncated payload");
        return result;
      }
      memory.name = std::move(name);
    }
    memory.configured = configured != 0U;
    memory.mode = static_cast<PersistentLiveMemoryMode>(mode);
    memory.fade_ms = fade_ms;
    memory.midi_kind = static_cast<PersistentMidiBindingKind>(midi_kind);
    memory.midi_channel = midi_channel;
    memory.midi_number = midi_number;
    memory.channels.reserve(channel_count);
    for(std::uint16_t channel_index = 0U;
        channel_index < channel_count; ++channel_index) {
      PersistentLiveMemoryChannel channel;
      if(!reader.read_u16(channel.slot) || !reader.read_u8(channel.value)) {
        result.diagnostics.push_back(memory_path(index) +
                                     ".channels: truncated channel entry");
        return result;
      }
      memory.channels.push_back(channel);
    }
  }

  if(reader.remaining() != 0U) {
    result.diagnostics.push_back("live.bin: trailing bytes are not permitted");
    return result;
  }

  result.diagnostics = validate_live_memory_persistent_state(state);
  if(!result.diagnostics.empty()) return result;
  result.state = std::move(state);
  return result;
}

}  // namespace aeyla::project
''', encoding="utf-8")


Path("product/AeylaVisualDmx/AeylaLiveMemorySession.h").write_text(r'''#pragma once

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
''', encoding="utf-8")


session_path = "product/AeylaVisualDmx/AeylaLiveMemorySession.cpp"
replace_once(session_path,
'''struct SessionState {\n  output::ArtNetOutputWorker* output_worker{nullptr};\n  capture::ArtNetCaptureWorker* capture_worker{nullptr};\n  std::array<SlotState, kOperatorMemoryCount> slots{};\n  bool persistence_dirty{false};\n};''',
'''struct SessionState {\n  output::ArtNetOutputWorker* output_worker{nullptr};\n  capture::ArtNetCaptureWorker* capture_worker{nullptr};\n  std::array<SlotState, kOperatorMemoryCapacity> slots{};\n  std::size_t memory_count{kDefaultOperatorMemoryCount};\n  bool persistence_dirty{false};\n};''')

replace_once(session_path,
'''constexpr std::array<const char*, kOperatorMemoryCount> kMemoryIds{\n    "front", "haze", "white-base", "fixture-test"};\nconstexpr std::array<const char*, kOperatorMemoryCount> kMemoryNames{\n    "FRONTAL", "HUMO / HAZE", "BASE BLANCA", "TEST LUMINARIAS"};''',
'''constexpr std::array<const char*, kOperatorMemoryCapacity> kMemoryIds{\n    "front", "haze", "white-base", "fixture-test",\n    "live-5", "live-6", "live-7", "live-8"};\nconstexpr std::array<const char*, kOperatorMemoryCapacity> kMemoryNames{\n    "FRONTAL", "HUMO / HAZE", "BASE BLANCA", "TEST LUMINARIAS",\n    "MEMORIA 5", "MEMORIA 6", "MEMORIA 7", "MEMORIA 8"};''')

replace_once(session_path,
'''void initialize_session(SessionState& session) {\n  session = SessionState{};\n  for(std::size_t index = 0U; index < session.slots.size(); ++index)\n    initialize_slot(index, session.slots[index]);\n}''',
'''void initialize_session(SessionState& session) {\n  session = SessionState{};\n  session.memory_count = kDefaultOperatorMemoryCount;\n  for(std::size_t index = 0U; index < session.slots.size(); ++index)\n    initialize_slot(index, session.slots[index]);\n}''')

replace_once(session_path,
'''  for(std::size_t index = 0U; index < session.slots.size(); ++index) {\n    if(!session.slots[index].configured) continue;\n    std::string ignored;''',
'''  for(std::size_t index = 0U; index < session.memory_count; ++index) {\n    if(!session.slots[index].configured) continue;\n    std::string ignored;''')

replace_once(session_path,
'''MemoryView view(const void* owner, std::size_t index) {\n  MemoryView result;\n  if(owner == nullptr || index >= kOperatorMemoryCount)\n    return result;\n\n  const std::scoped_lock lock(gMutex);\n  auto& session = ensure_session_locked(owner);\n  const auto& slot = session.slots[index];''',
'''std::size_t memory_count(const void* owner) noexcept {\n  if(owner == nullptr) return kDefaultOperatorMemoryCount;\n  const std::scoped_lock lock(gMutex);\n  const auto iterator = gSessions.find(owner);\n  return iterator == gSessions.end()\n      ? kDefaultOperatorMemoryCount\n      : iterator->second.memory_count;\n}\n\nMemoryView view(const void* owner, std::size_t index) {\n  MemoryView result;\n  if(owner == nullptr || index >= kOperatorMemoryCapacity)\n    return result;\n\n  const std::scoped_lock lock(gMutex);\n  auto& session = ensure_session_locked(owner);\n  if(index >= session.memory_count) return result;\n  const auto& slot = session.slots[index];''')

replace_once(session_path,
'''  return result;\n}\n\nproject::LiveMemoryPersistentState persistent_state(const void* owner) {''',
'''  return result;\n}\n\nActionResult add_memory(const void* owner) {\n  if(owner == nullptr)\n    return {false, "No hay instancia RGB Live Control"};\n  const std::scoped_lock lock(gMutex);\n  auto& session = ensure_session_locked(owner);\n  if(session.memory_count >= kOperatorMemoryCapacity)\n    return {false, "Límite EN VIVO alcanzado · máximo 8 memorias"};\n\n  const std::size_t index = session.memory_count;\n  initialize_slot(index, session.slots[index]);\n  ++session.memory_count;\n  session.persistence_dirty = true;\n  return {true, session.slots[index].definition.name + " · memoria añadida"};\n}\n\nActionResult rename_memory(const void* owner,\n                           std::size_t index,\n                           std::string_view requested_name) {\n  if(owner == nullptr || index >= kOperatorMemoryCapacity)\n    return invalid_index();\n\n  while(!requested_name.empty() &&\n        (requested_name.front() == ' ' || requested_name.front() == '\\t'))\n    requested_name.remove_prefix(1U);\n  while(!requested_name.empty() &&\n        (requested_name.back() == ' ' || requested_name.back() == '\\t'))\n    requested_name.remove_suffix(1U);\n  if(requested_name.empty())\n    return {false, "El nombre de la memoria no puede estar vacío"};\n  if(requested_name.size() > project::kMaximumPersistentLiveMemoryNameBytes)\n    return {false, "Nombre demasiado largo · máximo 48 bytes"};\n  if(std::any_of(requested_name.begin(), requested_name.end(),\n                 [](unsigned char value) {\n                   return value == 0U || value == '\\n' || value == '\\r';\n                 }))\n    return {false, "El nombre de la memoria contiene caracteres no permitidos"};\n\n  const std::scoped_lock lock(gMutex);\n  auto& session = ensure_session_locked(owner);\n  if(index >= session.memory_count) return invalid_index();\n  auto& slot = session.slots[index];\n  const std::string normalized(requested_name);\n  if(slot.definition.name == normalized)\n    return {true, slot.definition.name + " · nombre sin cambios"};\n  slot.definition.name = normalized;\n  session.persistence_dirty = true;\n  return {true, slot.definition.name + " · nombre actualizado"};\n}\n\nproject::LiveMemoryPersistentState persistent_state(const void* owner) {''')

replace_once(session_path,
'''  const std::scoped_lock lock(gMutex);\n  auto& session = ensure_session_locked(owner);\n  for(std::size_t index = 0U; index < session.slots.size(); ++index) {\n    const auto& slot = session.slots[index];\n    auto& persistent = result.memories[index];\n    persistent.configured = slot.configured;''',
'''  const std::scoped_lock lock(gMutex);\n  auto& session = ensure_session_locked(owner);\n  result.memory_count = static_cast<std::uint8_t>(session.memory_count);\n  for(std::size_t index = 0U; index < session.memory_count; ++index) {\n    const auto& slot = session.slots[index];\n    auto& persistent = result.memories[index];\n    persistent.name = slot.definition.name;\n    persistent.configured = slot.configured;''')

replace_once(session_path,
'''  std::array<SlotState, kOperatorMemoryCount> restored{};\n  for(std::size_t index = 0U; index < restored.size(); ++index) {\n    initialize_slot(index, restored[index]);\n    const auto& source = state.memories[index];\n    auto& target = restored[index];\n    target.configured = source.configured;''',
'''  std::array<SlotState, kOperatorMemoryCapacity> restored{};\n  for(std::size_t index = 0U; index < restored.size(); ++index) {\n    initialize_slot(index, restored[index]);\n    if(index >= static_cast<std::size_t>(state.memory_count)) continue;\n    const auto& source = state.memories[index];\n    auto& target = restored[index];\n    target.definition.name = source.name;\n    target.configured = source.configured;''')

replace_once(session_path,
'''    for(std::size_t index = 0U; index < restored.size(); ++index) {\n      if(!restored[index].configured) continue;''',
'''    for(std::size_t index = 0U;\n        index < static_cast<std::size_t>(state.memory_count); ++index) {\n      if(!restored[index].configured) continue;''')

replace_once(session_path,
'''  session.slots = std::move(restored);\n  session.persistence_dirty = false;''',
'''  session.slots = std::move(restored);\n  session.memory_count = state.memory_count;\n  session.persistence_dirty = false;''')

# Limit MIDI Learn and mapped-event scans to memories that are actually exposed.
replace_once(session_path,
'''  for(std::size_t index = 0U; index < session.slots.size(); ++index) {\n    auto& slot = session.slots[index];\n    if(!slot.midi_learn_pending) continue;''',
'''  for(std::size_t index = 0U; index < session.memory_count; ++index) {\n    auto& slot = session.slots[index];\n    if(!slot.midi_learn_pending) continue;''')
replace_once(session_path,
'''  for(std::size_t index = 0U; index < session.slots.size(); ++index) {\n    auto& slot = session.slots[index];\n    if(slot.midi_kind == MidiBindingKind::none ||''',
'''  for(std::size_t index = 0U; index < session.memory_count; ++index) {\n    auto& slot = session.slots[index];\n    if(slot.midi_kind == MidiBindingKind::none ||''')
replace_once(session_path,
'''  for(std::size_t index = 0U; index < session.slots.size(); ++index) {\n    if(index == keep_index) continue;''',
'''  for(std::size_t index = 0U; index < session.memory_count; ++index) {\n    if(index == keep_index) continue;''')

# Public authoring/operation functions must reject hidden capacity slots.
for needle in [
    "ActionResult learn_from_avolites(const void* owner, std::size_t index)",
    "ActionResult cancel_learn(const void* owner, std::size_t index)",
    "ActionResult toggle(const void* owner, std::size_t index)",
    "ActionResult set_fader_level(const void* owner,\n                             std::size_t index,\n                             float level)",
    "ActionResult cycle_fade(const void* owner,\n                        std::size_t index,\n                        int direction)",
    "ActionResult toggle_mode(const void* owner, std::size_t index)",
    "ActionResult arm_midi_learn(const void* owner, std::size_t index)",
    "ActionResult clear_midi_binding(const void* owner, std::size_t index)",
]:
    p = Path(session_path)
    text = p.read_text(encoding="utf-8")
    start = text.find(needle)
    if start < 0:
        raise SystemExit(f"missing function anchor: {needle}")
    session_anchor = "  auto& session = ensure_session_locked(owner);\n"
    pos = text.find(session_anchor, start)
    if pos < 0:
        raise SystemExit(f"missing session anchor after: {needle}")
    insert_pos = pos + len(session_anchor)
    guard = "  if(index >= session.memory_count) return invalid_index();\n"
    if text.startswith(guard, insert_pos):
        continue
    text = text[:insert_pos] + guard + text[insert_pos:]
    p.write_text(text, encoding="utf-8")

# AeylaVisualDmx public bridge: dynamic count + add/rename operations.
plug_path = "product/AeylaVisualDmx/AeylaVisualDmx.h"
replace_once(plug_path,
'''  [[nodiscard]] std::size_t LiveMemoryCount() const noexcept\n  {\n    return aeyla::live_memory_session::kOperatorMemoryCount;\n  }''',
'''  [[nodiscard]] std::size_t LiveMemoryCount() const noexcept\n  {\n    return aeyla::live_memory_session::memory_count(this);\n  }''')
replace_once(plug_path,
'''  [[nodiscard]] aeyla::live_memory_session::MemoryView LiveMemoryViewFromUI(\n      std::size_t index)\n  {\n    aeyla::live_memory_session::register_runtime(\n        this, &mArtNetOutput, &mArtNetCapture);\n    return aeyla::live_memory_session::view(this, index);\n  }\n\n  [[nodiscard]] aeyla::product::AuthoringResult LearnLiveMemoryFromAvolitesFromUI(''',
'''  [[nodiscard]] aeyla::live_memory_session::MemoryView LiveMemoryViewFromUI(\n      std::size_t index)\n  {\n    aeyla::live_memory_session::register_runtime(\n        this, &mArtNetOutput, &mArtNetCapture);\n    return aeyla::live_memory_session::view(this, index);\n  }\n\n  [[nodiscard]] aeyla::product::AuthoringResult AddLiveMemoryFromUI()\n  {\n    aeyla::live_memory_session::register_runtime(\n        this, &mArtNetOutput, &mArtNetCapture);\n    const auto result = aeyla::live_memory_session::add_memory(this);\n    if(result.succeeded) CommitLiveMemoryPersistenceIfDirtyFromUI();\n    return {result.succeeded, {}, result.message};\n  }\n\n  [[nodiscard]] aeyla::product::AuthoringResult RenameLiveMemoryFromUI(\n      std::size_t index, std::string_view name)\n  {\n    aeyla::live_memory_session::register_runtime(\n        this, &mArtNetOutput, &mArtNetCapture);\n    const auto result = aeyla::live_memory_session::rename_memory(\n        this, index, name);\n    if(result.succeeded) CommitLiveMemoryPersistenceIfDirtyFromUI();\n    return {result.succeeded, {}, result.message};\n  }\n\n  [[nodiscard]] aeyla::product::AuthoringResult LearnLiveMemoryFromAvolitesFromUI(''')

# Codec regression coverage: v2 names/count + v1 compatibility.
test_state = "tests/test_live_memory_state.cpp"
replace_once(test_state,
'''  LiveMemoryPersistentState state;\n\n  auto& front = state.memories[0];''',
'''  LiveMemoryPersistentState state;\n  state.memory_count = 6U;\n  state.memories[0].name = "CONTRA VIOLINES";\n  state.memories[4].name = "STROBE";\n  state.memories[5].name = "PÚBLICO";\n\n  auto& front = state.memories[0];''')
replace_once(test_state,
'''  auto badVersion = encoded;\n  badVersion[8] = 2U;''',
'''  auto badVersion = encoded;\n  badVersion[8] = 99U;''')
replace_once(test_state,
'''  require(!decode_live_memory_persistent_state(badVersion).ok(),\n          "unsupported live.bin version was accepted");\n\n  auto trailing = encoded;''',
'''  require(!decode_live_memory_persistent_state(badVersion).ok(),\n          "unsupported live.bin version was accepted");\n\n  // R10.7 backward compatibility: decode the exact v1/4-memory shape and\n  // synthesize the historical default names without rewriting the file.\n  std::vector<std::uint8_t> legacyV1{\n      'A','E','Y','L','A','L','I','V', 1U,0U, 4U,0U};\n  const auto appendLegacyMemory = [&](std::uint8_t mode) {\n    legacyV1.push_back(0U);  // configured\n    legacyV1.push_back(mode);\n    legacyV1.push_back(0U);  // MIDI kind\n    legacyV1.push_back(0U);  // MIDI channel\n    legacyV1.push_back(0U);  // MIDI number\n    legacyV1.push_back(0U);  // reserved in v1\n    legacyV1.push_back(0xE8U);\n    legacyV1.push_back(0x03U);\n    legacyV1.push_back(0U);\n    legacyV1.push_back(0U);  // 1000 ms\n    legacyV1.push_back(0U);\n    legacyV1.push_back(0U);  // zero channels\n  };\n  appendLegacyMemory(0U);\n  appendLegacyMemory(1U);\n  appendLegacyMemory(0U);\n  appendLegacyMemory(0U);\n  const auto legacyDecoded = decode_live_memory_persistent_state(legacyV1);\n  require(legacyDecoded.ok(),\n          legacyDecoded.diagnostics.empty()\n              ? "legacy v1 decode failed without diagnostic"\n              : legacyDecoded.diagnostics.front());\n  require(legacyDecoded.state->memory_count == 4U &&\n              legacyDecoded.state->memories[0].name == "FRONTAL" &&\n              legacyDecoded.state->memories[1].name == "HUMO / HAZE" &&\n              legacyDecoded.state->memories[3].name == "TEST LUMINARIAS",\n          "legacy v1 names/count were not migrated in memory");\n\n  auto tooLongName = state;\n  tooLongName.memories[0].name.assign(49U, 'X');\n  diagnostics.clear();\n  require(encode_live_memory_persistent_state(tooLongName, diagnostics).empty() &&\n              !diagnostics.empty(),\n          "live-memory name longer than 48 bytes was encoded");\n\n  auto tooMany = state;\n  tooMany.memory_count = 9U;\n  diagnostics.clear();\n  require(encode_live_memory_persistent_state(tooMany, diagnostics).empty() &&\n              !diagnostics.empty(),\n          "live-memory count above 8 was encoded");\n\n  auto trailing = encoded;''')

# Session regression coverage: dynamic count, add and rename without touching DMX.
test_session = "tests/test_live_memory_session.cpp"
replace_once(test_session,
'''  int owner = 0;\n  register_runtime(&owner, &aeylaTx, &aeylaRx);\n\n  // R10.3: MIDI authoring is independent''',
'''  int owner = 0;\n  register_runtime(&owner, &aeylaTx, &aeylaRx);\n\n  require(memory_count(&owner) == 4U,\n          "fresh EN VIVO session must expose four memories");\n  const auto renamedFront = rename_memory(&owner, 0U, "CONTRA VIOLINES");\n  require(renamedFront.succeeded && view(&owner, 0U).name == "CONTRA VIOLINES",\n          "live-memory rename did not update the operator view");\n  const auto addedFive = add_memory(&owner);\n  const auto addedSix = add_memory(&owner);\n  require(addedFive.succeeded && addedSix.succeeded && memory_count(&owner) == 6U,\n          "adding live memories did not extend the active count to six");\n  const auto renamedFive = rename_memory(&owner, 4U, "STROBE");\n  require(renamedFive.succeeded && view(&owner, 4U).name == "STROBE",\n          "new live memory could not be renamed");\n  const auto authoredLive = persistent_state(&owner);\n  require(authoredLive.memory_count == 6U &&\n              authoredLive.memories[0].name == "CONTRA VIOLINES" &&\n              authoredLive.memories[4].name == "STROBE",\n          "dynamic live-memory count/name did not enter persistent state");\n\n  // R10.3: MIDI authoring is independent''')
replace_once(test_session,
'''  std::cout << "RGB Live Control live-memory PASS: pre-DMX MIDI Learn + sparse Avolites learn + safe Note/CC control\\n";''',
'''  std::cout << "RGB Live Control live-memory PASS: expandable/renamable + pre-DMX MIDI Learn + sparse Avolites learn + safe Note/CC control\\n";''')

print("R10.7 live-memory v2 model patch applied")
