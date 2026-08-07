#include "show/show_program.h"

#include <cstdlib>
#include <iostream>
#include <set>

int main() {
  using namespace aeyla::show;
  const std::set<std::string> looks{"look-red"};

  ShowProgram empty;
  if (!validate_show_program(empty, looks).ok()) {
    std::cerr << "FAIL: empty authoring project must remain saveable\n";
    return EXIT_FAILURE;
  }
  if (validate_show_program_for_performance(empty, looks).ok()) {
    std::cerr << "FAIL: empty authoring project must not pass Show Mode preflight\n";
    return EXIT_FAILURE;
  }

  SongProgram song;
  song.song_id = "song-1";
  song.name = "Song 1";
  song.length_ticks = 4U * song.ppq;
  song.scenes.push_back(
      {"scene-red", "Red", "look-red", 0U, 0U, false, CueBehavior::latch});
  song.clips.push_back(
      {"clip-red", "scene-red", 0U, song.length_ticks, 36U, 127U, 1U});
  empty.songs.push_back(song);

  if (!validate_show_program_for_performance(empty, looks).ok()) {
    std::cerr << "FAIL: one valid programmed song must pass show-program preflight\n";
    return EXIT_FAILURE;
  }

  empty.songs.front().scenes.front().look_id = "missing-look";
  if (validate_show_program_for_performance(empty, looks).ok()) {
    std::cerr << "FAIL: performance preflight must include authoring validation\n";
    return EXIT_FAILURE;
  }

  std::cout << "All AEYLA show performance-preflight tests passed.\n";
  return EXIT_SUCCESS;
}
