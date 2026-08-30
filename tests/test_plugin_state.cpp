#include "runtime/plugin_state.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

using aeyla::runtime::PluginComponentState;
using aeyla::runtime::PluginStateError;
using aeyla::runtime::ProjectLocatorMode;
using aeyla::runtime::SessionSongBinding;
using aeyla::runtime::SessionTakeBinding;
}  // namespace

int main() {
  PluginComponentState state;
  for (std::size_t i = 0; i < state.project_uuid.size(); ++i) {
    state.project_uuid[i] = static_cast<std::uint8_t>(i + 1);
  }
  for (std::size_t i = 0; i < state.project_checksum.size(); ++i) {
    state.project_checksum[i] = static_cast<std::uint8_t>(255U - i);
  }
  state.project_schema_major = 1;
  state.project_schema_minor = 4;
  state.grand_master = 0.625F;
  state.blackout = true;
  state.locator_mode = ProjectLocatorMode::relative_companion;
  state.project_locator = "show/AEYLA_SHOW.aeylashow";
  state.song_bindings = {
      SessionSongBinding{"song-intro", 16.0},
      SessionSongBinding{"song-final", 144.5},
  };
  state.show_midi.enabled = true;
  state.show_midi.channel = 12U;
  state.show_midi.play_note = 72U;
  state.show_midi.capture_start_note = 74U;
  state.show_midi.capture_stop_note = 75U;
  state.take_library_locator = "C:/AEYLA/Takes";
  state.take_bindings = {
      SessionTakeBinding{"song-intro", "Intro_Toma_2.aeylatake", 44U, 440U},
      SessionTakeBinding{"song-final", "Final_Clip.aeylatake", 0U, 0U},
  };

  const auto encoded = aeyla::runtime::encode_plugin_component_state(state);
  check(encoded.ok(), "valid state should encode");
  check(!encoded.bytes.empty(), "encoded state should not be empty");

  const auto decoded = aeyla::runtime::decode_plugin_component_state(encoded.bytes);
  check(decoded.ok(), "valid encoded state should decode");
  check(decoded.state == state, "state should survive exact round trip");
  check(decoded.state.blackout, "blackout should persist as safe state");
  check(decoded.state.song_bindings == state.song_bindings,
        "all 15-song session bindings must survive host-state round trip");
  check(decoded.state.show_midi == state.show_midi,
        "MIDI Show mapping must survive host-state round trip");
  check(decoded.state.take_library_locator == state.take_library_locator &&
            decoded.state.take_bindings == state.take_bindings,
        "take library selection and trims must survive host-state round trip");

  // Every truncated prefix must fail; no partial state may be accepted.
  for (std::size_t size = 0; size < encoded.bytes.size(); ++size) {
    const auto truncated = aeyla::runtime::decode_plugin_component_state(
        std::span<const std::uint8_t>(encoded.bytes.data(), size));
    check(!truncated.ok(), "truncated prefix must fail");
  }

  // Corrupted magic.
  {
    auto bytes = encoded.bytes;
    bytes[0] ^= 0xFFU;
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.error == PluginStateError::bad_magic, "bad magic must be rejected");
  }

  // Unsupported major version (little endian at bytes 8..9).
  {
    auto bytes = encoded.bytes;
    bytes[8] = 2;
    bytes[9] = 0;
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.error == PluginStateError::unsupported_major_version,
          "unsupported major version must be rejected");
  }

  // Payload larger than actual bytes.
  {
    auto bytes = encoded.bytes;
    bytes[12] = 0xFFU;
    bytes[13] = 0xFFU;
    bytes[14] = 0;
    bytes[15] = 0;
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.error == PluginStateError::invalid_payload_size,
          "oversized payload must be rejected");
  }

  // Unsafe relative locators must not encode.
  {
    auto invalid = state;
    invalid.project_locator = "../outside.aeylashow";
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::unsafe_relative_locator,
          "relative parent traversal must be rejected");
  }

  {
    auto invalid = state;
    invalid.show_midi.next_note = invalid.show_midi.play_note;
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_show_midi_mapping,
          "colliding MIDI Show notes must be rejected");
  }
  {
    auto invalid = state;
    invalid.project_locator = "C:\\absolute\\show.aeylashow";
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::unsafe_relative_locator,
          "Windows absolute path must be rejected in relative mode");
  }
  {
    auto invalid = state;
    invalid.project_locator = std::string("show/ok", 7) + '\0' + "hidden.aeylashow";
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_argument,
          "embedded NUL in locator must be rejected");
  }

  // Locator/mode consistency.
  {
    auto invalid = state;
    invalid.locator_mode = ProjectLocatorMode::none;
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::inconsistent_locator,
          "none mode with locator must be rejected");
  }

  // Invalid master values must be rejected, never silently clamped.
  for (float value : {-0.01F, 1.01F, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
    auto invalid = state;
    invalid.grand_master = value;
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_grand_master,
          "invalid grand master must be rejected");
  }

  // Oversized locators must be rejected before allocation-heavy serialization.
  {
    auto invalid = state;
    invalid.project_locator.assign(aeyla::runtime::kMaxProjectLocatorBytes + 1, 'a');
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::locator_too_large,
          "oversized locator must be rejected");
  }

  // Song-to-host bindings are bounded, unique and finite. They are session
  // placement data and must never admit an ambiguous or corrupt mapping.
  {
    auto invalid = state;
    invalid.song_bindings.push_back(SessionSongBinding{"song-intro", 32.0});
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_song_binding,
          "duplicate song binding must be rejected");
  }
  {
    auto invalid = state;
    invalid.song_bindings.front().host_start_ppq =
        std::numeric_limits<double>::quiet_NaN();
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_song_binding,
          "non-finite host start PPQ must be rejected");
  }

  // Take bindings are basenames only and trims have one unambiguous full-file
  // sentinel (0/0). Never admit traversal, arbitrary extensions or one-frame
  // / overflow-prone ranges into host state.
  {
    auto invalid = state;
    invalid.take_bindings.front().file_name = "../escape.aeylatake";
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_take_binding,
          "take filename traversal must be rejected");
  }
  {
    auto invalid = state;
    invalid.take_bindings.front().file_name = "Intro_Toma_2.wav";
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_take_binding,
          "take binding must reference an .aeylatake basename");
  }
  {
    auto invalid = state;
    invalid.take_bindings.front().start_frame = 44U;
    invalid.take_bindings.front().end_frame_exclusive = 0U;
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_take_binding,
          "only 0/0 may represent full-file playback");
  }
  {
    auto invalid = state;
    invalid.take_bindings.front().end_frame_exclusive =
        invalid.take_bindings.front().start_frame + 1U;
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_take_binding,
          "take trim must leave at least two DMX frames");
  }
  {
    auto invalid = state;
    invalid.take_bindings.front().start_frame =
        std::numeric_limits<std::uint64_t>::max();
    invalid.take_bindings.front().end_frame_exclusive = 0U;
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_take_binding,
          "take trim validation must not overflow at UINT64_MAX");
  }

  // Legacy format 1.0 contains no binding-count tail. It remains readable and
  // migrates deterministically to an empty binding list (safe until SET START).
  {
    PluginComponentState legacy_state = state;
    legacy_state.song_bindings.clear();
    legacy_state.take_library_locator.clear();
    legacy_state.take_bindings.clear();
    auto bytes = aeyla::runtime::encode_plugin_component_state(legacy_state).bytes;
    check(bytes.size() >= 16U, "current state must contain 1.1/1.2/1.3 tails");
    bytes.resize(bytes.size() - 18U);
    bytes[10] = 0U;
    bytes[11] = 0U;
    std::uint32_t payload_size = static_cast<std::uint32_t>(bytes[12]) |
                                 (static_cast<std::uint32_t>(bytes[13]) << 8U) |
                                 (static_cast<std::uint32_t>(bytes[14]) << 16U) |
                                 (static_cast<std::uint32_t>(bytes[15]) << 24U);
    payload_size -= 18U;
    bytes[12] = static_cast<std::uint8_t>(payload_size & 0xFFU);
    bytes[13] = static_cast<std::uint8_t>((payload_size >> 8U) & 0xFFU);
    bytes[14] = static_cast<std::uint8_t>((payload_size >> 16U) & 0xFFU);
    bytes[15] = static_cast<std::uint8_t>((payload_size >> 24U) & 0xFFU);
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    legacy_state.show_midi = {};
    check(result.ok() && result.state == legacy_state,
          "legacy 1.0 host state must migrate to no session bindings");
  }

  // R07 PRETEST sessions used format 1.1: Song bindings are present but the
  // MIDI Show map is not. They migrate to the safe disabled default without
  // losing their placement data.
  {
    auto legacy11 = state;
    legacy11.take_library_locator.clear();
    legacy11.take_bindings.clear();
    auto bytes = aeyla::runtime::encode_plugin_component_state(legacy11).bytes;
    bytes.resize(bytes.size() - 16U);
    bytes[10] = 1U;
    bytes[11] = 0U;
    std::uint32_t payload_size = static_cast<std::uint32_t>(bytes[12]) |
                                 (static_cast<std::uint32_t>(bytes[13]) << 8U) |
                                 (static_cast<std::uint32_t>(bytes[14]) << 16U) |
                                 (static_cast<std::uint32_t>(bytes[15]) << 24U);
    payload_size -= 16U;
    bytes[12] = static_cast<std::uint8_t>(payload_size & 0xFFU);
    bytes[13] = static_cast<std::uint8_t>((payload_size >> 8U) & 0xFFU);
    bytes[14] = static_cast<std::uint8_t>((payload_size >> 16U) & 0xFFU);
    bytes[15] = static_cast<std::uint8_t>((payload_size >> 24U) & 0xFFU);
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    auto expected = legacy11;
    expected.show_midi = {};
    check(result.ok() && result.state == expected,
          "legacy 1.1 state must migrate to disabled MIDI Show defaults");
  }

  // R08 format 1.2 contains MIDI Show state but no take-library tail.
  {
    auto legacy12 = state;
    legacy12.take_library_locator.clear();
    legacy12.take_bindings.clear();
    auto bytes = aeyla::runtime::encode_plugin_component_state(legacy12).bytes;
    bytes.resize(bytes.size() - 8U);
    bytes[10] = 2U;
    bytes[11] = 0U;
    std::uint32_t payload_size = static_cast<std::uint32_t>(bytes[12]) |
                                 (static_cast<std::uint32_t>(bytes[13]) << 8U) |
                                 (static_cast<std::uint32_t>(bytes[14]) << 16U) |
                                 (static_cast<std::uint32_t>(bytes[15]) << 24U);
    payload_size -= 8U;
    bytes[12] = static_cast<std::uint8_t>(payload_size & 0xFFU);
    bytes[13] = static_cast<std::uint8_t>((payload_size >> 8U) & 0xFFU);
    bytes[14] = static_cast<std::uint8_t>((payload_size >> 16U) & 0xFFU);
    bytes[15] = static_cast<std::uint8_t>((payload_size >> 24U) & 0xFFU);
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    auto expected = legacy12;
    expected.show_midi.capture_start_note = aeyla::runtime::kShowMidiCaptureStartNote;
    expected.show_midi.capture_stop_note = aeyla::runtime::kShowMidiCaptureStopNote;
    check(result.ok() && result.state == expected,
          "legacy 1.2 state must migrate with default capture notes and empty take bindings");
  }

  // R08 state 1.3 contains take bindings but predates learned REC notes. It
  // restores N42/N43 defaults without changing any existing take selection.
  {
    auto bytes = encoded.bytes;
    bytes.resize(bytes.size() - 2U);
    bytes[10] = 3U;
    bytes[11] = 0U;
    std::uint32_t payload_size = static_cast<std::uint32_t>(bytes[12]) |
                                 (static_cast<std::uint32_t>(bytes[13]) << 8U) |
                                 (static_cast<std::uint32_t>(bytes[14]) << 16U) |
                                 (static_cast<std::uint32_t>(bytes[15]) << 24U);
    payload_size -= 2U;
    bytes[12] = static_cast<std::uint8_t>(payload_size & 0xFFU);
    bytes[13] = static_cast<std::uint8_t>((payload_size >> 8U) & 0xFFU);
    bytes[14] = static_cast<std::uint8_t>((payload_size >> 16U) & 0xFFU);
    bytes[15] = static_cast<std::uint8_t>((payload_size >> 24U) & 0xFFU);
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    auto expected = state;
    expected.show_midi.capture_start_note = aeyla::runtime::kShowMidiCaptureStartNote;
    expected.show_midi.capture_stop_note = aeyla::runtime::kShowMidiCaptureStopNote;
    check(result.ok() && result.state == expected,
          "legacy 1.3 state must migrate to default capture notes");
  }

  // Same-major future minor payloads may append fields and remain readable.
  {
    auto bytes = encoded.bytes;
    bytes[10] = static_cast<std::uint8_t>(
        aeyla::runtime::kPluginStateFormatMinor + 1U);
    bytes[11] = 0;
    std::uint32_t payload_size = static_cast<std::uint32_t>(bytes[12]) |
                                 (static_cast<std::uint32_t>(bytes[13]) << 8U) |
                                 (static_cast<std::uint32_t>(bytes[14]) << 16U) |
                                 (static_cast<std::uint32_t>(bytes[15]) << 24U);
    payload_size += 4;
    bytes[12] = static_cast<std::uint8_t>(payload_size & 0xFFU);
    bytes[13] = static_cast<std::uint8_t>((payload_size >> 8U) & 0xFFU);
    bytes[14] = static_cast<std::uint8_t>((payload_size >> 16U) & 0xFFU);
    bytes[15] = static_cast<std::uint8_t>((payload_size >> 24U) & 0xFFU);
    bytes.insert(bytes.end(), {0x12U, 0x34U, 0x56U, 0x78U});
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.ok() && result.state == state,
          "same-major future minor state with appended fields should remain readable");
  }

  // Unknown locator mode must be rejected after decoding.
  {
    auto bytes = encoded.bytes;
    bytes[25] = 0xFFU;
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.error == PluginStateError::invalid_locator_mode,
          "unknown locator mode must be rejected");
  }

  // Deterministic malformed-input sweep: decoder must never throw or crash.
  {
    std::mt19937 generator(0xAE71A5U);
    std::uniform_int_distribution<int> length_distribution(0, 1024);
    std::uniform_int_distribution<int> byte_distribution(0, 255);
    for (int case_index = 0; case_index < 10000; ++case_index) {
      std::vector<std::uint8_t> bytes(
          static_cast<std::size_t>(length_distribution(generator)));
      for (auto& byte : bytes) {
        byte = static_cast<std::uint8_t>(byte_distribution(generator));
      }
      (void)aeyla::runtime::decode_plugin_component_state(bytes);
    }
    check(true, "malformed-input sweep completed");
  }

  if (failures == 0) {
    std::cout << "All plugin state tests passed.\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
