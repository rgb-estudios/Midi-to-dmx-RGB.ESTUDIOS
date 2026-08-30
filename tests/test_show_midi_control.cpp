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
        "reserved PANIC note must decode on configured Show channel");
  check(!match_show_midi_note(mapping, 15U, kShowMidiPanicNote, 127U, match),
        "PANIC must not leak across MIDI channels");

  check(match_show_midi_note(mapping, 16U, kShowMidiCaptureStartNote, 127U, match) &&
            match.command == ShowMidiCommand::capture_start,
        "reserved N42 must decode as REC START");
  check(match_show_midi_note(mapping, 16U, kShowMidiCaptureStopNote, 127U, match) &&
            match.command == ShowMidiCommand::capture_stop,
        "reserved N43 must decode as REC STOP");
  check(!match_show_midi_note(mapping, 15U, kShowMidiCaptureStartNote, 127U, match),
        "REC START must follow configured Show MIDI channel");
  check(!match_show_midi_note(mapping, 15U, kShowMidiCaptureStopNote, 127U, match),
        "REC STOP must follow configured Show MIDI channel");
  check(!match_show_midi_note(mapping, 16U, kShowMidiCaptureStartNote, 0U, match) &&
            !match_show_midi_note(mapping, 16U, kShowMidiCaptureStopNote, 0U, match),
        "velocity-zero REC notes must not trigger capture commands");

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
            mapping == before_collision && error.find("PANIC") != std::string::npos,
        "PANIC note must not be learnable by another command");
  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::stop_reset,
                               12U, kShowMidiCaptureStartNote, error) &&
            mapping == before_collision && error.find("REC START") != std::string::npos,
        "N42 REC START must not be learnable by another command");
  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::stop_reset,
                               12U, kShowMidiCaptureStopNote, error) &&
            mapping == before_collision && error.find("REC STOP") != std::string::npos,
        "N43 REC STOP must not be learnable by another command");
  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::launch_song_base,
                               12U, 120U, error),
        "launch base must reserve all 15 direct Song notes");

  for(const auto fixed : {kShowMidiPanicNote,
                          kShowMidiCaptureStartNote,
                          kShowMidiCaptureStopNote}) {
    const auto first = static_cast<std::uint8_t>(
        fixed - (kShowMidiSongCapacity - 1U));
    check(!assign_show_midi_note(mapping,
                                 ShowMidiLearnTarget::launch_song_base,
                                 12U, first, error),
          "new Song launch bank must not cross a fixed operational note");
  }

  // Restored legacy collisions remain decodable; fixed operational commands
  // shadow them at runtime for backward compatibility.
  {
    ShowMidiMapping legacy = mapping;
    legacy.stop_note = kShowMidiPanicNote;
    check(validate_show_midi_mapping(legacy) == ShowMidiMappingError::none,
          "legacy N41 collision must remain state-compatible");
    check(match_show_midi_note(legacy, legacy.channel, kShowMidiPanicNote,
                               127U, match) &&
              match.command == ShowMidiCommand::panic_blackout,
          "PANIC must shadow a legacy N41 assignment");
  }
  {
    ShowMidiMapping legacy = mapping;
    legacy.stop_note = kShowMidiCaptureStartNote;
    check(validate_show_midi_mapping(legacy) == ShowMidiMappingError::none,
          "legacy N42 collision must remain state-compatible");
    check(match_show_midi_note(legacy, legacy.channel,
                               kShowMidiCaptureStartNote, 127U, match) &&
              match.command == ShowMidiCommand::capture_start,
          "REC START must shadow a legacy N42 assignment");
  }
  {
    ShowMidiMapping legacy = mapping;
    legacy.stop_note = kShowMidiCaptureStopNote;
    check(validate_show_midi_mapping(legacy) == ShowMidiMappingError::none,
          "legacy N43 collision must remain state-compatible");
    check(match_show_midi_note(legacy, legacy.channel,
                               kShowMidiCaptureStopNote, 127U, match) &&
              match.command == ShowMidiCommand::capture_stop,
          "REC STOP must shadow a legacy N43 assignment");
  }

  const auto stamped = make_show_midi_event(
      ShowMidiCommand::play_retrigger, 0U, 12U, 72U,
      4000U, 2000U, 64U, 137U);
  check(stamped.ready_sample == 4064U && stamped.trigger_sample == 2064U,
        "MIDI event must retain separate callback and running clocks");
  check(stamped.capture_frame_snapshot == 137U,
        "MIDI event must preserve capture frame seen at ingress");
  check(!show_midi_event_ready(4064U, stamped) &&
            show_midi_event_ready(8192U, stamped),
        "runtime readiness must use callback clock");
  check(show_midi_elapsed_samples(2064U, stamped) == 0U &&
            show_midi_elapsed_samples(2576U, stamped) == 512U,
        "stopped blocks must not advance artistic compensation");

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
    ShowMidiIngress<4U> panic_ingress;
    ShowMidiEvent panic_event;
    panic_event.command = ShowMidiCommand::panic_blackout;
    check(panic_ingress.try_submit(panic_event),
          "PANIC request must bypass queue capacity dependency");
    check(panic_ingress.consume_panic_request() &&
              !panic_ingress.consume_panic_request(),
          "PANIC request must be a one-shot atomic boundary");
  }

  {
    ShowMidiIngress<4U> capture_ingress;
    ShowMidiEvent start_event;
    start_event.command = ShowMidiCommand::capture_start;
    start_event.capture_frame_snapshot = 55U;
    ShowMidiEvent stop_event;
    stop_event.command = ShowMidiCommand::capture_stop;
    stop_event.capture_frame_snapshot = 66U;
    check(capture_ingress.try_submit(start_event) &&
              capture_ingress.try_submit(stop_event),
          "REC START and STOP must enter runtime queue independently");
    ShowMidiEvent consumed_start;
    ShowMidiEvent consumed_stop;
    check(capture_ingress.try_consume(consumed_start) &&
              consumed_start.command == ShowMidiCommand::capture_start &&
              consumed_start.capture_frame_snapshot == 55U,
          "REC START must reach runtime unchanged");
    check(capture_ingress.try_consume(consumed_stop) &&
              consumed_stop.command == ShowMidiCommand::capture_stop &&
              consumed_stop.capture_frame_snapshot == 66U,
          "REC STOP must reach runtime unchanged");
  }

  {
    ShowMidiIngress<4U> capture_overflow;
    ShowMidiEvent artistic;
    artistic.command = ShowMidiCommand::play_retrigger;
    check(capture_overflow.try_submit(artistic), "overflow setup one");
    check(capture_overflow.try_submit(artistic), "overflow setup two");
    check(capture_overflow.try_submit(artistic), "overflow setup three");

    ShowMidiEvent start_event;
    start_event.command = ShowMidiCommand::capture_start;
    ShowMidiEvent stop_event;
    stop_event.command = ShowMidiCommand::capture_stop;
    check(capture_overflow.try_submit(start_event),
          "REC START must be preserved through full artistic queue");
    check(capture_overflow.try_submit(stop_event),
          "REC STOP must be preserved through full artistic queue");
    check(capture_overflow.consume_safety_stop_request() &&
              !capture_overflow.consume_safety_stop_request(),
          "capture overflow must request fail-safe boundary");
    check(capture_overflow.dropped_events() == 0U,
          "preserved capture commands must not count as dropped");

    ShowMidiEvent recovered;
    check(capture_overflow.try_consume(recovered) &&
              recovered.command == ShowMidiCommand::capture_stop &&
              recovered.note == kShowMidiCaptureStopNote,
          "REC STOP fallback must be consumed before START and stale artistic events");
    check(capture_overflow.try_consume(recovered) &&
              recovered.command == ShowMidiCommand::capture_start &&
              recovered.note == kShowMidiCaptureStartNote,
          "REC START fallback must remain distinct from STOP");
  }

  ShowMidiIngress<4U> ingress;
  ShowMidiEvent event;
  event.trigger_sample = 1234U;
  event.ready_sample = 5678U;
  event.capture_frame_snapshot = 91U;
  check(ingress.try_submit(event), "first event must enter bounded queue");
  check(ingress.try_submit(event), "second event must enter bounded queue");
  check(ingress.try_submit(event), "third event must enter bounded queue");
  check(!ingress.try_submit(event), "full queue must reject newest artistic event");
  check(ingress.dropped_events() == 1U &&
            ingress.consume_safety_stop_request() &&
            !ingress.consume_safety_stop_request(),
        "artistic overflow must produce one visible safety-stop request");

  if(failures == 0) {
    std::cout << "All Show MIDI control tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
