#include "runtime/host_song_binding.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aeyla::runtime {

SongTransportProjection project_host_transport_to_song(
    const HostTransportSnapshot& host,
    const HostSongBinding& binding,
    const show::SongProgram& song) noexcept {
  SongTransportProjection result;
  result.host_running = host.running;
  result.rendering_offline = host.rendering_offline;

  if (host.revision == 0U || !host.ppq_position_valid ||
      !std::isfinite(host.ppq_position) ||
      !std::isfinite(binding.host_start_ppq) ||
      binding.song_id.empty() || binding.song_id != song.song_id ||
      song.ppq == 0U || song.length_ticks == 0U) {
    return result;
  }

  const double relative = host.ppq_position - binding.host_start_ppq;
  result.relative_ppq = relative;

  constexpr double kBoundaryEpsilonPpq = 1e-10;
  if (relative < -kBoundaryEpsilonPpq) {
    result.state = SongProjectionState::before_song;
    return result;
  }

  const double non_negative_relative = std::max(0.0, relative);
  const long double scaled =
      static_cast<long double>(non_negative_relative) *
      static_cast<long double>(song.ppq);
  if (!std::isfinite(static_cast<double>(scaled)) || scaled < 0.0L) {
    return result;
  }

  // Do not fire a cue early. The epsilon is much smaller than one MIDI tick and
  // exists only to recover exact tick boundaries represented as 959.999999999x.
  constexpr long double kBoundaryEpsilonTicks = 1e-7L;
  const long double floored = std::floor(scaled + kBoundaryEpsilonTicks);
  if (floored >= static_cast<long double>(song.length_ticks)) {
    result.state = SongProjectionState::after_song;
    result.tick = song.length_ticks;
    return result;
  }

  if (floored > static_cast<long double>(
                    std::numeric_limits<std::uint64_t>::max())) {
    return result;
  }

  result.state = SongProjectionState::in_song;
  result.tick = static_cast<std::uint64_t>(floored);
  return result;
}

std::optional<float> phase_from_host_ppq(
    const HostTransportSnapshot& host,
    double cycles_per_quarter_note) noexcept {
  if (host.revision == 0U || !host.ppq_position_valid ||
      !std::isfinite(host.ppq_position) ||
      !std::isfinite(cycles_per_quarter_note) ||
      cycles_per_quarter_note < 0.0 || cycles_per_quarter_note > 64.0) {
    return std::nullopt;
  }

  const long double unwrapped =
      static_cast<long double>(host.ppq_position) *
      static_cast<long double>(cycles_per_quarter_note);
  if (!std::isfinite(static_cast<double>(unwrapped))) return std::nullopt;

  long double phase = std::fmod(unwrapped, 1.0L);
  if (phase < 0.0L) phase += 1.0L;
  return static_cast<float>(phase);
}

}  // namespace aeyla::runtime
