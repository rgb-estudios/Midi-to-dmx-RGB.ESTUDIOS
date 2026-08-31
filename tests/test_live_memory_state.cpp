#include "project/live_memory_state.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
  if(condition) return;
  std::cerr << "FAILED: " << message << '\n';
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main() {
  using namespace aeyla::project;

  LiveMemoryPersistentState state;

  auto& front = state.memories[0];
  front.configured = true;
  front.mode = PersistentLiveMemoryMode::toggle;
  front.fade_ms = 1000U;
  front.midi_kind = PersistentMidiBindingKind::note;
  front.midi_channel = 3U;
  front.midi_number = 64U;
  front.channels = {{1U, 200U}, {3U, 100U}, {101U, 18U}};

  auto& haze = state.memories[1];
  haze.configured = true;
  haze.mode = PersistentLiveMemoryMode::fader;
  haze.fade_ms = 1500U;
  haze.midi_kind = PersistentMidiBindingKind::control_change;
  haze.midi_channel = 2U;
  haze.midi_number = 74U;
  haze.channels = {{2U, 180U}};

  state.memories[2].mode = PersistentLiveMemoryMode::toggle;
  state.memories[2].fade_ms = 100U;
  state.memories[3].mode = PersistentLiveMemoryMode::fader;
  state.memories[3].fade_ms = 1000U;

  std::vector<std::string> diagnostics;
  const auto encoded = encode_live_memory_persistent_state(state, diagnostics);
  require(diagnostics.empty(), "valid live state produced encode diagnostics");
  require(!encoded.empty(), "valid live state encoded to empty payload");
  require(encoded.size() < 7000U, "live.bin exceeded expected compact bound");

  const auto decoded = decode_live_memory_persistent_state(encoded);
  require(decoded.ok(), decoded.diagnostics.empty()
                            ? "roundtrip decode failed without diagnostic"
                            : decoded.diagnostics.front());
  require(*decoded.state == state, "live.bin roundtrip changed persistent state");

  // Runtime safety state is intentionally absent from the persistent schema.
  // Only definitions/configuration are represented; there is no ON level,
  // target level, transition, activation serial, ARM or Learn-pending field.

  auto badMagic = encoded;
  badMagic[0] = 'X';
  require(!decode_live_memory_persistent_state(badMagic).ok(),
          "invalid live.bin magic was accepted");

  auto badVersion = encoded;
  badVersion[8] = 2U;
  badVersion[9] = 0U;
  require(!decode_live_memory_persistent_state(badVersion).ok(),
          "unsupported live.bin version was accepted");

  auto trailing = encoded;
  trailing.push_back(0U);
  require(!decode_live_memory_persistent_state(trailing).ok(),
          "trailing live.bin bytes were accepted");

  auto truncated = encoded;
  truncated.resize(truncated.size() - 1U);
  require(!decode_live_memory_persistent_state(truncated).ok(),
          "truncated live.bin was accepted");

  auto duplicateSlots = state;
  duplicateSlots.memories[0].channels = {{1U, 1U}, {1U, 2U}};
  diagnostics.clear();
  require(encode_live_memory_persistent_state(duplicateSlots, diagnostics).empty() &&
              !diagnostics.empty(),
          "duplicate/non-increasing DMX slots were encoded");

  auto wrongMidiForFader = state;
  wrongMidiForFader.memories[1].midi_kind = PersistentMidiBindingKind::note;
  diagnostics.clear();
  require(encode_live_memory_persistent_state(wrongMidiForFader, diagnostics).empty() &&
              !diagnostics.empty(),
          "FADER with MIDI Note mapping was encoded");

  auto wrongMidiForToggle = state;
  wrongMidiForToggle.memories[0].midi_kind =
      PersistentMidiBindingKind::control_change;
  diagnostics.clear();
  require(encode_live_memory_persistent_state(wrongMidiForToggle, diagnostics).empty() &&
              !diagnostics.empty(),
          "TOGGLE with MIDI CC mapping was encoded");

  auto duplicateMidi = state;
  duplicateMidi.memories[2].configured = true;
  duplicateMidi.memories[2].channels = {{4U, 255U}};
  duplicateMidi.memories[2].midi_kind = PersistentMidiBindingKind::note;
  duplicateMidi.memories[2].midi_channel = 3U;
  duplicateMidi.memories[2].midi_number = 64U;
  diagnostics.clear();
  require(encode_live_memory_persistent_state(duplicateMidi, diagnostics).empty() &&
              !diagnostics.empty(),
          "duplicate live-memory MIDI mapping was encoded");

  auto unconfiguredChannels = state;
  unconfiguredChannels.memories[2].channels = {{5U, 10U}};
  diagnostics.clear();
  require(encode_live_memory_persistent_state(unconfiguredChannels, diagnostics).empty() &&
              !diagnostics.empty(),
          "unconfigured memory with DMX ownership was encoded");

  auto excessiveFade = state;
  excessiveFade.memories[0].fade_ms = 60001U;
  diagnostics.clear();
  require(encode_live_memory_persistent_state(excessiveFade, diagnostics).empty() &&
              !diagnostics.empty(),
          "unsafe live-memory fade was encoded");

  std::cout << "AEYLA live-memory persistent codec PASS\n";
  return EXIT_SUCCESS;
}
