#include "AeylaLiveMemorySession.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

void require(bool condition, const std::string& message) {
  if(condition) return;
  std::cerr << "FAILED: " << message << '\n';
  std::exit(EXIT_FAILURE);
}

template <typename Predicate>
bool wait_until(Predicate predicate,
                std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while(std::chrono::steady_clock::now() < deadline) {
    if(predicate()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return predicate();
}

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::capture;
  using namespace aeyla::output;
  using namespace aeyla::live_memory_session;

  constexpr std::uint16_t kAvolitesPort = 17673U;
  constexpr std::uint16_t kAeylaTxPort = 17674U;

  std::string error;

  ArtNetCaptureWorker aeylaRx;
  ArtNetCaptureConfig rxConfig;
  rxConfig.listen_ipv4 = "127.0.0.1";
  rxConfig.udp_port = kAvolitesPort;
  rxConfig.port_address = 0U;
  rxConfig.frames_per_second = 44U;
  require(aeylaRx.start(rxConfig, error), error);

  ArtNetOutputWorker avolites;
  ArtNetOutputConfig avolitesConfig;
  avolitesConfig.source_ipv4 = "127.0.0.1";
  avolitesConfig.target_ipv4 = "127.0.0.1";
  avolitesConfig.udp_port = kAvolitesPort;
  avolitesConfig.port_address = 0U;
  avolitesConfig.channel_count = 512U;
  require(avolites.start(avolitesConfig, error), error);

  ArtNetCaptureWorker sink;
  ArtNetCaptureConfig sinkConfig = rxConfig;
  sinkConfig.udp_port = kAeylaTxPort;
  require(sink.start(sinkConfig, error), error);

  ArtNetOutputWorker aeylaTx;
  ArtNetOutputConfig txConfig = avolitesConfig;
  txConfig.udp_port = kAeylaTxPort;
  require(aeylaTx.start(txConfig, error), error);

  DmxUniverse songBase{};
  songBase[0] = 10U;
  songBase[1] = 30U;
  songBase[2] = 50U;
  songBase[100] = 77U;
  aeylaTx.publish_latest(songBase, 1U);
  aeylaTx.prepare_explicit_rearm();
  aeylaTx.set_enabled(true);

  int owner = 0;
  register_runtime(&owner, &aeylaTx, &aeylaRx);

  require(memory_count(&owner) == 4U,
          "fresh EN VIVO session must expose four memories");
  const auto renamedFront = rename_memory(&owner, 0U, "CONTRA VIOLINES");
  require(renamedFront.succeeded && view(&owner, 0U).name == "CONTRA VIOLINES",
          "live-memory rename did not update the operator view");
  const auto addedFive = add_memory(&owner);
  const auto addedSix = add_memory(&owner);
  require(addedFive.succeeded && addedSix.succeeded && memory_count(&owner) == 6U,
          "adding live memories did not extend the active count to six");
  const auto renamedFive = rename_memory(&owner, 4U, "STROBE");
  require(renamedFive.succeeded && view(&owner, 4U).name == "STROBE",
          "new live memory could not be renamed");
  const auto authoredLive = persistent_state(&owner);
  require(authoredLive.memory_count == 6U &&
              authoredLive.memories[0].name == "CONTRA VIOLINES" &&
              authoredLive.memories[4].name == "STROBE",
          "dynamic live-memory count/name did not enter persistent state");

  // R10.3: MIDI authoring is independent of Avolites DMX authoring. This lets
  // the show be mapped before the console look exists. The event used for
  // Learn is non-destructive, and subsequent mapped events remain physically
  // inert until the memory owns a validated sparse DMX definition.
  const auto preDmxMidiLearn = arm_midi_learn(&owner, 0U);
  require(preDmxMidiLearn.succeeded,
          "MIDI Learn must be available before DMX Learn: " + preDmxMidiLearn.message);
  runtime::HostEvent preDmxNote{};
  preDmxNote.type = runtime::HostEventType::note_on;
  preDmxNote.channel = 1U;
  preDmxNote.note = 60U;
  preDmxNote.value = 1.0F;
  require(process_midi_event(&owner, preDmxNote),
          "pre-DMX MIDI Learn note was not consumed");
  auto preDmxView = view(&owner, 0U);
  require(!preDmxView.configured &&
              preDmxView.midi_kind == MidiBindingKind::note &&
              preDmxView.midi_channel == 1U &&
              preDmxView.midi_number == 60U,
          "pre-DMX MIDI binding was not retained independently");
  require(preDmxView.target_level < 0.01F,
          "MIDI Learn before DMX unexpectedly changed target level");
  require(process_midi_event(&owner, preDmxNote),
          "mapped pre-DMX NoteOn should still be consumed");
  require(view(&owner, 0U).target_level < 0.01F,
          "mapped pre-DMX NoteOn unexpectedly generated physical live-memory state");

  DmxUniverse avolitesOff{};
  avolitesOff[100] = 5U;
  avolites.publish_latest(avolitesOff, 1U);
  avolites.set_enabled(true);
  require(wait_until([&]() { return aeylaRx.stats().packets_accepted >= 2U; }),
          "AEYLA RX did not receive Avolites baseline");

  const auto first = learn_from_avolites(&owner, 0U);
  require(first.succeeded, first.message);
  require(view(&owner, 0U).learning,
          "first learn press must retain the OFF baseline as step 1/2");

  DmxUniverse avolitesOn = avolitesOff;
  avolitesOn[0] = 200U;
  avolitesOn[2] = 100U;
  auto acceptedBefore = aeylaRx.stats().packets_accepted;
  avolites.publish_latest(avolitesOn, 2U);
  require(wait_until([&]() {
    return aeylaRx.stats().packets_accepted >= acceptedBefore + 2U;
  }), "AEYLA RX did not receive Avolites ON state");

  const auto second = learn_from_avolites(&owner, 0U);
  require(second.succeeded, second.message);
  const auto learned = view(&owner, 0U);
  require(learned.configured && !learned.learning,
          "second learn press must finish the memory");
  require(learned.channel_count == 2U,
          "two-snapshot learn must own only the two channels changed in Avolites");

  const auto toggled = toggle(&owner, 0U);
  require(toggled.succeeded, toggled.message);

  DmxUniverse composed{};
  require(wait_until([&]() {
    const auto memory = view(&owner, 0U);
    DmxUniverse frame{};
    if(memory.level <= 0.99F || !sink.latest_frame(frame)) return false;
    if(frame[0] != 200U || frame[2] != 100U) return false;
    composed = frame;
    return true;
  }, std::chrono::milliseconds(1800)),
          "FRONTAL memory did not reach its learned Art-Net targets after 1 s fade");

  require(composed[0] == 200U && composed[2] == 100U,
          "learned channels did not reach Avolites targets");
  require(composed[1] == songBase[1],
          "unrelated song channel was overwritten by learned memory");
  require(composed[100] == songBase[100],
          "unrelated Avolites baseline value leaked into memory mask");

  // R10.1 show-safety: reconfiguring a live memory rebuilds its runtime and
  // would otherwise snap it OFF. Authoring changes are therefore rejected
  // while the memory is active or transitioning.
  const auto activeFadeEdit = cycle_fade(&owner, 0U, 1);
  require(!activeFadeEdit.succeeded,
          "fade edit unexpectedly succeeded while FRONTAL was active");
  const auto activeModeEdit = toggle_mode(&owner, 0U);
  require(!activeModeEdit.succeeded,
          "mode edit unexpectedly succeeded while FRONTAL was active");
  const auto activeDmxLearn = learn_from_avolites(&owner, 0U);
  require(!activeDmxLearn.succeeded,
          "DMX relearn unexpectedly started while FRONTAL was active");
  const auto stillLive = view(&owner, 0U);
  require(stillLive.target_level > 0.99F &&
              stillLive.mode == LiveMemoryControlMode::toggle &&
              stillLive.fade_ms == 1000U &&
              !stillLive.learning,
          "blocked live-memory edit changed FRONTAL runtime/configuration");

  // MIDI Learn is deliberately non-destructive: the event used to bind the
  // control must never move live DMX. The next matching event operates it.
  reset_levels(&owner);
  const auto noteLearn = arm_midi_learn(&owner, 0U);
  require(noteLearn.succeeded, noteLearn.message);
  runtime::HostEvent noteOn{};
  noteOn.type = runtime::HostEventType::note_on;
  noteOn.channel = 3U;
  noteOn.note = 64U;
  noteOn.value = 1.0F;
  require(process_midi_event(&owner, noteOn),
          "MIDI Learn note event was not consumed");
  auto frontMidi = view(&owner, 0U);
  require(frontMidi.midi_kind == MidiBindingKind::note &&
              frontMidi.midi_channel == 3U && frontMidi.midi_number == 64U,
          "FRONTAL did not retain learned MIDI note/channel");
  require(frontMidi.target_level < 0.01F,
          "MIDI Learn note unexpectedly changed FRONTAL target");

  require(process_midi_event(&owner, noteOn),
          "mapped NoteOn was not consumed");
  require(view(&owner, 0U).target_level > 0.99F,
          "second NoteOn did not toggle FRONTAL target ON");

  runtime::HostEvent noteOff = noteOn;
  noteOff.type = runtime::HostEventType::note_off;
  noteOff.value = 0.0F;
  require(process_midi_event(&owner, noteOff),
          "learned NoteOff should be consumed by live memory mapping");
  require(view(&owner, 0U).target_level > 0.99F,
          "NoteOff unexpectedly toggled FRONTAL a second time");

  // HUMO/HAZE is a real continuous fader. Learn must not move it; the next
  // CC74 value controls interpolation from the current song frame to target.
  avolites.publish_latest(avolitesOff, 3U);
  acceptedBefore = aeylaRx.stats().packets_accepted;
  require(wait_until([&]() {
    return aeylaRx.stats().packets_accepted >= acceptedBefore + 2U;
  }), "AEYLA RX did not return to Avolites OFF baseline for HAZE");
  const auto hazeFirst = learn_from_avolites(&owner, 1U);
  require(hazeFirst.succeeded, hazeFirst.message);

  DmxUniverse hazeOn = avolitesOff;
  hazeOn[1] = 180U;
  acceptedBefore = aeylaRx.stats().packets_accepted;
  avolites.publish_latest(hazeOn, 4U);
  require(wait_until([&]() {
    return aeylaRx.stats().packets_accepted >= acceptedBefore + 2U;
  }), "AEYLA RX did not receive HAZE ON state");
  const auto hazeSecond = learn_from_avolites(&owner, 1U);
  require(hazeSecond.succeeded, hazeSecond.message);
  require(view(&owner, 1U).mode == LiveMemoryControlMode::fader,
          "HAZE must default to FADER mode");

  const auto ccLearn = arm_midi_learn(&owner, 1U);
  require(ccLearn.succeeded, ccLearn.message);
  runtime::HostEvent cc{};
  cc.type = runtime::HostEventType::note_on;  // envelope type is ignored for CC
  cc.channel = 2U;
  cc.note = 74U;
  cc.reserved = 1U;
  cc.value = 0.25F;
  require(process_midi_event(&owner, cc),
          "MIDI Learn CC event was not consumed");
  const auto hazeMidi = view(&owner, 1U);
  require(hazeMidi.midi_kind == MidiBindingKind::control_change &&
              hazeMidi.midi_channel == 2U && hazeMidi.midi_number == 74U,
          "HAZE did not retain learned MIDI CC/channel");
  require(hazeMidi.level < 0.01F,
          "MIDI Learn CC unexpectedly moved HAZE fader");

  require(process_midi_event(&owner, cc),
          "mapped CC 25 percent was not consumed");
  require(view(&owner, 1U).level > 0.24F && view(&owner, 1U).level < 0.26F,
          "mapped CC did not set HAZE fader to 25 percent");
  require(wait_until([&]() {
    DmxUniverse frame{};
    return sink.latest_frame(frame) && frame[1] == 68U;
  }), "25 percent HAZE CC did not produce expected physical Art-Net level");

  cc.value = 1.0F;
  require(process_midi_event(&owner, cc),
          "mapped CC 100 percent was not consumed");
  require(wait_until([&]() {
    DmxUniverse frame{};
    return sink.latest_frame(frame) && frame[1] == 180U;
  }), "100 percent HAZE CC did not reach learned target");

  const auto clearedCc = clear_midi_binding(&owner, 1U);
  require(clearedCc.succeeded, clearedCc.message);
  cc.value = 0.0F;
  require(!process_midi_event(&owner, cc),
          "cleared CC mapping should become inert");

  clear(&owner);
  aeylaTx.publish_latest(songBase, 2U);
  require(wait_until([&]() {
    DmxUniverse frame{};
    return sink.latest_frame(frame) && frame == songBase;
  }), "clearing project memory state did not restore pure song base");

  aeylaTx.set_enabled(false);
  avolites.set_enabled(false);
  aeylaTx.stop();
  avolites.stop();
  sink.stop();
  aeylaRx.stop();

  std::cout << "RGB Live Control live-memory PASS: expandable/renamable + pre-DMX MIDI Learn + sparse Avolites learn + safe Note/CC control\n";
  return EXIT_SUCCESS;
}
