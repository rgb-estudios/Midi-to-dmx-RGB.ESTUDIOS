#include "show/show_program.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

aeyla::show::SongProgram make_song() {
  using namespace aeyla::show;
  SongProgram song;
  song.song_id = "aeyla-pilot-song";
  song.name = "AEYLA Pilot Song";
  song.tempo_bpm = 120.0;
  song.ppq = 960U;
  song.length_ticks = 8U * song.ppq;
  song.scenes = {
      {"scene-intro", "Intro", "look-gradient", 1000U, 250U, false,
       CueBehavior::latch},
      {"scene-red", "Red Impact", "look-solid", 250U, 250U, false,
       CueBehavior::latch},
      {"scene-blackout", "Blackout", "", 0U, 0U, true,
       CueBehavior::latch},
      {"scene-white-hit", "White Hit", "look-solid", 0U, 0U, false,
       CueBehavior::momentary},
  };
  song.clips = {
      // Latch blocks intentionally contain a gap: the Intro must remain the
      // effective scene between its editor block ending and the Red cue start.
      {"clip-intro", "scene-intro", 0U, song.ppq, 36U, 110U, 1U},
      {"clip-red", "scene-red", 4U * song.ppq, 3U * song.ppq, 37U, 127U, 1U},
      {"clip-blackout", "scene-blackout", 7U * song.ppq, song.ppq, 38U, 127U, 1U},
      // Momentary cues may overlay the current latch and return to it.
      {"clip-white-hit", "scene-white-hit", 5U * song.ppq, song.ppq / 2U,
       39U, 127U, 1U},
  };
  song.scenes[0].midi_binding = MidiBinding{36U, 1U};
  song.scenes[1].midi_binding = MidiBinding{37U, 1U};
  song.scenes[2].midi_binding = MidiBinding{38U, 1U};
  song.scenes[3].midi_binding = MidiBinding{39U, 1U};
  return song;
}

aeyla::show::ShowProgram make_show_with_song_count(std::size_t count) {
  aeyla::show::ShowProgram program;
  for (std::size_t index = 0; index < count; ++index) {
    auto song = make_song();
    song.song_id = "aeyla-song-" + std::to_string(index + 1U);
    song.name = "AEYLA Song " + std::to_string(index + 1U);
    program.songs.push_back(std::move(song));
  }
  return program;
}
}  // namespace

int main() {
  using namespace aeyla::show;
  const std::set<std::string> looks{"look-gradient", "look-solid"};

  ShowProgram program;
  program.songs.push_back(make_song());
  const auto valid = validate_show_program(program, looks);
  check(valid.ok(), "canonical AEYLA cue program should validate");

  const auto fifteen_songs = make_show_with_song_count(15U);
  check(validate_show_program(fifteen_songs, looks).ok(),
        "AEYLA v1 must accept a show containing exactly 15 songs");

  const auto sixteen_songs = make_show_with_song_count(16U);
  check(!validate_show_program(sixteen_songs, looks).ok(),
        "AEYLA v1 must reject a show containing a 16th song");

  const auto compiled = compile_song_midi(program.songs.front(), looks);
  check(compiled.ok(), "valid cue program should compile to MIDI events");
  check(compiled.events.size() == 8U,
        "four cue placements must produce portable Note On/Off pairs");
  check(compiled.events[0].kind == MidiEventKind::note_on &&
            compiled.events[0].tick == 0U && compiled.events[0].note == 36U,
        "first cue must begin with its authored Note On");

  bool boundary_order_ok = false;
  const std::uint64_t boundary_tick = 7U * program.songs.front().ppq;
  for (std::size_t index = 1U; index < compiled.events.size(); ++index) {
    const auto& previous = compiled.events[index - 1U];
    const auto& current = compiled.events[index];
    if (previous.tick == boundary_tick && current.tick == boundary_tick &&
        previous.kind == MidiEventKind::note_off && previous.note == 37U &&
        current.kind == MidiEventKind::note_on && current.note == 38U) {
      boundary_order_ok = true;
      break;
    }
  }
  check(boundary_order_ok,
        "portable MIDI export must order Note Off before Note On at a shared boundary");

  const auto* intro_block = active_clip_at_tick(program.songs.front(), 100U);
  check(intro_block != nullptr && intro_block->clip_id == "clip-intro",
        "editor lookup must return a block under the playhead");
  check(active_clip_at_tick(program.songs.front(), 2U * program.songs.front().ppq) == nullptr,
        "editor lookup may be empty in a gap between latch cue blocks");

  const auto* persisted_intro = resolved_scene_at_tick(
      program.songs.front(), 2U * program.songs.front().ppq);
  check(persisted_intro != nullptr && persisted_intro->scene_id == "scene-intro",
        "latch cue must persist through an editor/timeline gap");

  const auto* white_hit = resolved_scene_at_tick(
      program.songs.front(), 5U * program.songs.front().ppq + 1U);
  check(white_hit != nullptr && white_hit->scene_id == "scene-white-hit",
        "active momentary cue must override the current latch");

  const auto* returned_red = resolved_scene_at_tick(
      program.songs.front(), 6U * program.songs.front().ppq);
  check(returned_red != nullptr && returned_red->scene_id == "scene-red",
        "expired momentary cue must return to the latest latch scene");

  const auto* boundary = resolved_scene_at_tick(
      program.songs.front(), 7U * program.songs.front().ppq);
  check(boundary != nullptr && boundary->scene_id == "scene-blackout",
        "latch boundary must activate the new cue deterministically");
  check(resolved_scene_at_tick(program.songs.front(),
                               program.songs.front().length_ticks) == nullptr,
        "song end must not report an effective scene");

  auto momentary_overlap = program;
  momentary_overlap.songs.front().clips.push_back(
      {"clip-white-hit-2", "scene-white-hit",
       5U * momentary_overlap.songs.front().ppq + 10U,
       momentary_overlap.songs.front().ppq / 2U,
       39U, 127U, 1U});
  check(!validate_show_program(momentary_overlap, looks).ok(),
        "overlapping momentary cue blocks must be rejected");

  auto duplicate_latch_boundary = program;
  duplicate_latch_boundary.songs.front().clips.push_back(
      {"clip-intro-same-tick", "scene-intro", 0U, 1U, 36U, 110U, 1U});
  check(!validate_show_program(duplicate_latch_boundary, looks).ok(),
        "two latch cues may not share one start tick");

  auto conflicting_midi = program;
  conflicting_midi.songs.front().clips.push_back(
      {"clip-red-conflict", "scene-red", 3U * conflicting_midi.songs.front().ppq,
       1U, 36U, 127U, 1U});
  check(!validate_show_program(conflicting_midi, looks).ok(),
        "one MIDI note/channel may not address two different scenes in one song");

  auto missing_look = program;
  missing_look.songs.front().scenes[0].look_id = "look-does-not-exist";
  check(!validate_show_program(missing_look, looks).ok(),
        "scene referencing an unavailable look must be rejected");

  auto beyond_song = program;
  beyond_song.songs.front().clips[2].duration_ticks += 1U;
  check(!validate_show_program(beyond_song, looks).ok(),
        "clip extending beyond song duration must be rejected");

  auto duplicate_id = program;
  duplicate_id.songs.front().scenes[1].scene_id =
      duplicate_id.songs.front().scenes[0].scene_id;
  check(!validate_show_program(duplicate_id, looks).ok(),
        "duplicate scene IDs must be rejected");

  auto invalid_midi = program;
  invalid_midi.songs.front().clips[0].velocity = 0U;
  invalid_midi.songs.front().clips[0].channel = 0U;
  check(!validate_show_program(invalid_midi, looks).ok(),
        "zero velocity and MIDI channel zero must be rejected");

  // Live MIDI must never be the memory mechanism for timeline playback.
  // Seek reconstructs from absolute position and clears manual overrides.
  CueRuntime runtime(program.songs.front());
  runtime.seek(2U * program.songs.front().ppq);
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-intro",
        "runtime seek must reconstruct the persisted Intro latch from position");

  runtime.note_on(37U, 100U, 1U);
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-red",
        "latch Note On must select the mapped cue");
  runtime.note_off(37U, 1U);
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-red",
        "latch Note Off must not cancel a persistent cue");

  runtime.advance(2U * program.songs.front().ppq + 100U);
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-red",
        "continuous timeline advance must preserve a live latch override");

  runtime.note_on(39U, 127U, 1U);
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-white-hit",
        "momentary Note On must override the current latch");
  runtime.note_off(39U, 1U);
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-red",
        "momentary Note Off must return to the prior latch");

  runtime.note_on(39U, 127U, 1U);
  runtime.all_notes_off();
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-red",
        "All Notes Off must release momentary state without erasing latch state");

  runtime.seek(100U);
  check(runtime.live_latch_scene() == nullptr && runtime.momentary_scene() == nullptr,
        "seek must clear manual latch and momentary overrides");
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-intro",
        "seek after live overrides must reconstruct timeline truth");

  runtime.seek(5U * program.songs.front().ppq + 1U);
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-white-hit",
        "seek directly into a momentary cue must reconstruct its override");
  runtime.seek(6U * program.songs.front().ppq);
  check(runtime.effective_scene() != nullptr &&
            runtime.effective_scene()->scene_id == "scene-red",
        "seek after a momentary cue must deterministically restore the underlying latch");

  runtime.transport_start();
  check(runtime.transport_running(), "transport start must be represented explicitly");
  runtime.transport_stop();
  check(!runtime.transport_running() && runtime.effective_scene() == nullptr,
        "transport stop must clear runtime cue state deterministically");

  if (failures == 0) {
    std::cout << "All AEYLA show-program tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
