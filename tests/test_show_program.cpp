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
      {"scene-intro", "Intro", "look-gradient", 1000U, 250U, false},
      {"scene-red", "Red Impact", "look-solid", 250U, 250U, false},
      {"scene-blackout", "Blackout", "", 0U, 0U, true},
  };
  song.clips = {
      {"clip-intro", "scene-intro", 0U, 4U * song.ppq, 36U, 110U, 1U},
      {"clip-red", "scene-red", 4U * song.ppq, 3U * song.ppq, 37U, 127U, 1U},
      {"clip-blackout", "scene-blackout", 7U * song.ppq, song.ppq, 38U, 127U, 1U},
  };
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
  check(valid.ok(), "canonical AEYLA scene program should validate");

  const auto fifteen_songs = make_show_with_song_count(15U);
  check(validate_show_program(fifteen_songs, looks).ok(),
        "AEYLA v1 must accept a show containing exactly 15 songs");

  const auto sixteen_songs = make_show_with_song_count(16U);
  check(!validate_show_program(sixteen_songs, looks).ok(),
        "AEYLA v1 must reject a show containing a 16th song");

  const auto compiled = compile_song_midi(program.songs.front(), looks);
  check(compiled.ok(), "valid scene program should compile to MIDI events");
  check(compiled.events.size() == 6U,
        "three scene clips must produce three Note On and three Note Off events");
  check(compiled.events[0].kind == MidiEventKind::note_on &&
            compiled.events[0].tick == 0U && compiled.events[0].note == 36U,
        "first clip must begin with its authored Note On");
  check(compiled.events[1].kind == MidiEventKind::note_off &&
            compiled.events[1].tick == 4U * program.songs.front().ppq,
        "touching clips must emit Note Off at the boundary");
  check(compiled.events[2].kind == MidiEventKind::note_on &&
            compiled.events[2].tick == compiled.events[1].tick,
        "next scene Note On must share the boundary tick");
  check(compiled.events[1].kind == MidiEventKind::note_off &&
            compiled.events[2].kind == MidiEventKind::note_on,
        "Note Off must be ordered before Note On at the same tick");

  const auto* intro = active_clip_at_tick(program.songs.front(), 100U);
  check(intro != nullptr && intro->clip_id == "clip-intro",
        "active clip lookup must return the scene block under the playhead");
  const auto* boundary = active_clip_at_tick(
      program.songs.front(), 4U * program.songs.front().ppq);
  check(boundary != nullptr && boundary->clip_id == "clip-red",
        "scene boundary must activate the next clip without overlap");
  check(active_clip_at_tick(program.songs.front(),
                            program.songs.front().length_ticks) == nullptr,
        "song end must not report an active scene");

  auto overlap = program;
  overlap.songs.front().clips[1].start_tick -= 1U;
  check(!validate_show_program(overlap, looks).ok(),
        "overlapping scene clips must be rejected in the single-lane alpha");

  auto missing_look = program;
  missing_look.songs.front().scenes[0].look_id = "look-does-not-exist";
  check(!validate_show_program(missing_look, looks).ok(),
        "scene referencing an unavailable look must be rejected");

  auto beyond_song = program;
  beyond_song.songs.front().clips.back().duration_ticks += 1U;
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

  if (failures == 0) {
    std::cout << "All AEYLA show-program tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
