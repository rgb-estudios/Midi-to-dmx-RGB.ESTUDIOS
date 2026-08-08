#include "runtime/host_song_binding.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
}  // namespace

int main() {
  using namespace aeyla::runtime;
  using aeyla::show::SongProgram;

  SongProgram song;
  song.song_id = "song-a";
  song.name = "Song A";
  song.ppq = 960U;
  song.length_ticks = 8U * song.ppq;

  HostSongBinding binding{"song-a", 16.0};
  HostTransportSnapshot host;
  host.revision = 1U;
  host.ppq_position_valid = true;
  host.running = true;
  host.ppq_position = 16.0;

  auto projected = project_host_transport_to_song(host, binding, song);
  check(projected.state == SongProjectionState::in_song && projected.tick == 0U,
        "binding PPQ must map exactly to song tick zero");
  check(projected.host_running,
        "projection must preserve host running state for the caller's safety policy");

  host.ppq_position = 17.0;
  projected = project_host_transport_to_song(host, binding, song);
  check(projected.state == SongProjectionState::in_song && projected.tick == 960U,
        "one host quarter-note after anchor must map to one song PPQ");

  host.ppq_position = 16.5;
  projected = project_host_transport_to_song(host, binding, song);
  check(projected.state == SongProjectionState::in_song && projected.tick == 480U,
        "fractional host PPQ must map to fractional-song tick position");

  host.ppq_position = 15.999;
  projected = project_host_transport_to_song(host, binding, song);
  check(projected.state == SongProjectionState::before_song,
        "host positions before the explicit song anchor must not invent negative ticks");

  host.ppq_position = 24.0;
  projected = project_host_transport_to_song(host, binding, song);
  check(projected.state == SongProjectionState::after_song &&
            projected.tick == song.length_ticks,
        "song end boundary must map to explicit after-song state");

  host.ppq_position = 23.999;
  projected = project_host_transport_to_song(host, binding, song);
  check(projected.state == SongProjectionState::in_song &&
            projected.tick < song.length_ticks,
        "position immediately before song end must remain in-song");

  // Exact MIDI-tick boundaries can arrive with tiny floating representation
  // error. Recover the boundary, but do not round arbitrary half ticks early.
  host.ppq_position = 16.0 + (960.0 - 1e-9) / 960.0;
  projected = project_host_transport_to_song(host, binding, song);
  check(projected.tick == 960U,
        "tiny floating error at an exact tick boundary must not delay the cue");

  host.ppq_position = 16.0 + 959.4 / 960.0;
  projected = project_host_transport_to_song(host, binding, song);
  check(projected.tick == 959U,
        "fractional position must use floor semantics and never fire next cue early");

  HostTransportSnapshot unavailable;
  unavailable.revision = 0U;
  check(project_host_transport_to_song(unavailable, binding, song).state ==
            SongProjectionState::unavailable,
        "no host snapshot must remain unavailable");

  unavailable.revision = 1U;
  unavailable.ppq_position_valid = false;
  check(project_host_transport_to_song(unavailable, binding, song).state ==
            SongProjectionState::unavailable,
        "invalid host PPQ must fail closed instead of using sample/time guesses");

  HostSongBinding wrong_song{"other-song", 16.0};
  host.ppq_position = 17.0;
  check(project_host_transport_to_song(host, wrong_song, song).state ==
            SongProjectionState::unavailable,
        "binding for another song must not drive the selected SongProgram");

  HostSongBinding invalid_anchor{"song-a", std::numeric_limits<double>::infinity()};
  check(project_host_transport_to_song(host, invalid_anchor, song).state ==
            SongProjectionState::unavailable,
        "non-finite song anchor must be rejected");

  host.rendering_offline = true;
  projected = project_host_transport_to_song(host, binding, song);
  check(projected.rendering_offline,
        "offline-render state must survive projection so network output can be inhibited");

  if (failures == 0) {
    std::cout << "All AEYLA host-song binding tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
