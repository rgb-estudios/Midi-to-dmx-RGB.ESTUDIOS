#pragma once

#include "runtime/host_transport_mailbox.h"
#include "show/show_program.h"

#include <cstdint>
#include <string>

namespace aeyla::runtime {

struct HostSongBinding {
  std::string song_id;
  double host_start_ppq{0.0};
  bool operator==(const HostSongBinding&) const = default;
};

enum class SongProjectionState : std::uint8_t {
  unavailable,
  before_song,
  in_song,
  after_song,
};

struct SongTransportProjection {
  SongProjectionState state{SongProjectionState::unavailable};
  std::uint64_t tick{0U};
  double relative_ppq{0.0};
  bool host_running{false};
  bool rendering_offline{false};
};

// Projects absolute DAW PPQ into a song-relative AEYLA tick without relying on
// event history. This function is pure and does not mutate CueRuntime.
//
// Rules:
// - binding.song_id must match the selected SongProgram;
// - host PPQ must be valid and finite;
// - positions before the binding are `before_song`;
// - positions at/after song.length_ticks are `after_song`;
// - in-song ticks use floor semantics so a cue is never fired early because of
//   fractional host position; a tiny epsilon only compensates binary floating
//   representation at exact tick boundaries.
SongTransportProjection project_host_transport_to_song(
    const HostTransportSnapshot& host,
    const HostSongBinding& binding,
    const show::SongProgram& song) noexcept;

}  // namespace aeyla::runtime
