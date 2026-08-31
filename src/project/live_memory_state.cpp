#include "project/live_memory_state.h"

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
