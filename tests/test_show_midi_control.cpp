#include "runtime/show_midi_control.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if(!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

}  // namespace

int main() {
  using namespace aeyla::runtime;

  ShowMidiMapping mapping;
  check(validate_show_midi_mapping(mapping) == ShowMidiMappingError::none,
        "default mapping must be valid");
  check(unpack_show_midi_mapping(pack_show_midi_mapping(mapping)) == mapping,
        "packed mapping must round-trip exactly");

  ShowMidiMatch match;
  check(!match_show_midi_note(mapping, 16U, mapping.play_note, 127U, match),
        "disabled mapping must not consume notes");
  mapping.enabled = true;
  check(match_show_midi_note(mapping, 16U, mapping.play_note, 127U, match) &&
            match.command == ShowMidiCommand::play_retrigger,
        "PLAY note must decode on configured channel");
  check(!match_show_midi_note(mapping, 15U, mapping.play_note, 127U, match),
        "wrong MIDI channel must pass through");
  check(!match_show_midi_note(mapping, 16U, mapping.play_note, 0U, match),
        "velocity-zero Note On must not trigger transport");

  check(match_show_midi_note(mapping, 16U, kShowMidiPanicNote, 127U, match) &&
            match.command == ShowMidiCommand::panic_blackout,
        "reserved PANIC note must decode on the configured Show channel");
  check(!match_show_midi_note(mapping, 15U, kShowMidiPanicNote, 127U, match),
        "PANIC must not leak across MIDI channels");
  check(!match_show_midi_note(mapping, 16U, kShowMidiPanicNote, 0U, match),
        "velocity-zero PANIC must not trigger safety action");

  for(std::uint8_t song = 0U; song < kShowMidiSongCapacity; ++song) {
    const auto note = static_cast<std::uint8_t>(mapping.launch_base_note + song);
    check(match_show_midi_note(mapping, 16U, note, 100U, match) &&
              match.command == ShowMidiCommand::launch_song &&
              match.song_index == song,
          "direct Song note must resolve its stable zero-based index");
  }

  std::string error;
  check(assign_show_midi_note(mapping, ShowMidiLearnTarget::play_retrigger,
                              12U, 72U, error) &&
            mapping.channel == 12U && mapping.play_note == 72U,
        "MIDI Learn must capture channel and note together");
  const auto before_collision = mapping;
  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::next_song,
                               12U, 72U, error) &&
            mapping == before_collision && !error.empty(),
        "MIDI Learn collision must be rejected without altering mapping");
  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::stop_reset,
                               12U, kShowMidiPanicNote, error) &&
            mapping == before_collision &&
            error.find("PANIC") != std::string::npos,
        "reserved PANIC note must not be learnable by another command");
  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::launch_song_base,
                               12U, 120U, error),
        "launch base must reserve all 15 direct Song notes");

  // Callback scheduling and artistic transport are deliberately different
  // clocks. Stopped DAW blocks may make an event ready for the worker but must
  // never be counted as elapsed DMX playback time.
  const auto stamped = make_show_midi_event(
      ShowMidiCommand::play_retrigger, 0U, 12U, 72U,
      4000U, 2000U, 64U);
  check(stamped.ready_sample == 4064U && stamped.trigger_sample == 2064U,
        "MIDI event must retain separate callback and running clocks");
  check(!show_midi_event_ready(4064U, stamped) &&
            show_midi_event_ready(4065U, stamped),
        "sample-zero race guard must wait beyond the containing block offset");
  check(show_midi_elapsed_samples(2064U, stamped) == 0U &&
            show_midi_elapsed_samples(2576U, stamped) == 512U,
        "stopped blocks must not advance artistic MIDI compensation");

  {
    auto invalid = mapping;
    invalid.channel = 0U;
    check(validate_show_midi_mapping(invalid) ==
              ShowMidiMappingError::invalid_channel,
          "MIDI channel zero must be rejected");
  }
  {
    auto invalid = mapping;
    invalid.launch_base_note = invalid.stop_note;
    check(validate_show_midi_mapping(invalid) ==
              ShowMidiMappingError::duplicate_note,
          "global command inside launch range must be rejected");
  }
  {
    auto invalid = mapping;
    invalid.launch_base_note = static_cast<std::uint8_t>(
        kShowMidiPanicNote - (kShowMidiSongCapacity - 1U));
    check(validate_show_midi_mapping(invalid) ==
              ShowMidiMappingError::duplicate_note,
          "Song launch bank must never overlap the fixed PANIC note");
  }

  // Queue overflow is a safety boundary: it is observable and requests one
  // fail-safe stop. Consuming that request rearms the detector for a later
  // independent overflow period.
  ShowMidiIngress<4U> ingress;
  ShowMidiEvent event;
  event.trigger_sample = 1234U;
  event.ready_sample = 5678U;
  check(ingress.try_submit(event), "first event must enter bounded queue");
  check(ingress.try_submit(event), "second event must enter bounded queue");
  check(ingress.try_submit(event), "third event must enter bounded queue");
  check(!ingress.try_submit(event), "full queue must reject newest event");
  check(ingress.dropped_events() == 1U &&
            ingress.consume_safety_stop_request() &&
            !ingress.consume_safety_stop_request(),
        "overflow must produce one visible safety-stop request");
  ShowMidiEvent consumed;
  check(ingress.try_consume(consumed) &&
            consumed.trigger_sample == event.trigger_sample &&
            consumed.ready_sample == event.ready_sample,
        "bounded ingress must preserve artistic and scheduling sample clocks");

  if(failures == 0) {
    std::cout << "All Show MIDI control tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
