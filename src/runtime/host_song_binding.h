#pragma once

#include "runtime/host_transport_mailbox.h"
#include "show/show_program.h"

#include <cstdint>
#include <optional>
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

// Projects a DAW playhead into an authoring tick without clipping to the
// Song's current end. This lets STORE CUE extend an authoring Song while still
// rejecting unavailable transport and positions before the explicit anchor.
[[nodiscard]] std::optional<std::uint64_t>
project_host_transport_to_authoring_tick(
    const HostTransportSnapshot& host,
    const HostSongBinding& binding,
    const show::SongProgram& song) noexcept;

// Deterministic animation phase derived only from absolute DAW transport.
// The result is stable across Play/Pause/Seek/Loop and does not depend on UI
// repaint rate or wall-clock time. Offline state is intentionally not rejected:
// artistic state may still be reconstructed while the output layer separately
// enforces the hard offline-render network inhibit.
[[nodiscard]] std::optional<float> phase_from_host_ppq(
    const HostTransportSnapshot& host,
    double cycles_per_quarter_note) noexcept;

}  // namespace aeyla::runtime
