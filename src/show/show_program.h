#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace aeyla::show {

constexpr std::uint32_t kDefaultPpq = 960U;
constexpr std::size_t kMaximumSongs = 15U;
constexpr std::size_t kMaximumScenesPerSong = 2048U;
constexpr std::size_t kMaximumClipsPerSong = 100000U;
constexpr std::uint32_t kMaximumTransitionMs = 60000U;

enum class CueBehavior : std::uint8_t {
  latch,
  momentary,
};

struct MidiBinding {
  std::uint8_t note{36U};
  std::uint8_t channel{1U};
  bool operator==(const MidiBinding&) const = default;
};

struct SceneDefinition {
  std::string scene_id;
  std::string name;
  std::string look_id;
  std::uint32_t transition_in_ms{0U};
  std::uint32_t transition_out_ms{0U};
  bool blackout{false};
  CueBehavior behavior{CueBehavior::latch};
  std::optional<MidiBinding> midi_binding{};
  bool operator==(const SceneDefinition&) const = default;
};

// One authored cue placement in the song timeline. MIDI note/channel are a
// portable control representation, not the creative identity of the cue.
//
// For latch scenes, start_tick is the important semantic boundary: the scene
// remains effective after Note Off until another latch cue supersedes it.
// duration_ticks remains useful for editor display and portable MIDI export.
//
// For momentary scenes, duration_ticks is semantic: the cue overrides the
// current latch only for [start_tick, start_tick + duration_ticks).
struct MidiSceneClip {
  std::string clip_id;
  std::string scene_id;
  std::uint64_t start_tick{0U};
  std::uint64_t duration_ticks{kDefaultPpq};
  std::uint8_t note{36U};
  std::uint8_t velocity{127U};
  std::uint8_t channel{1U};
  bool operator==(const MidiSceneClip&) const = default;
};

struct SongProgram {
  std::string song_id;
  std::string name;
  double tempo_bpm{120.0};
  std::uint8_t time_signature_numerator{4U};
  std::uint8_t time_signature_denominator{4U};
  std::uint32_t ppq{kDefaultPpq};
  std::uint64_t length_ticks{16U * kDefaultPpq};
  std::vector<SceneDefinition> scenes;
  std::vector<MidiSceneClip> clips;
  bool operator==(const SongProgram&) const = default;
};

struct ShowProgram {
  std::vector<SongProgram> songs;
  bool operator==(const ShowProgram&) const = default;
};

enum class ShowDiagnosticSeverity : std::uint8_t { warning, error };

struct ShowDiagnostic {
  ShowDiagnosticSeverity severity{ShowDiagnosticSeverity::error};
  std::string path;
  std::string message;
  bool operator==(const ShowDiagnostic&) const = default;
};

struct ShowValidation {
  std::vector<ShowDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept;
  [[nodiscard]] std::size_t error_count() const noexcept;
};

enum class MidiEventKind : std::uint8_t { note_off, note_on };

struct CompiledMidiEvent {
  std::uint64_t tick{0U};
  MidiEventKind kind{MidiEventKind::note_on};
  std::uint8_t note{0U};
  std::uint8_t velocity{0U};
  std::uint8_t channel{1U};
  std::string song_id;
  std::string scene_id;
  std::string clip_id;
  bool operator==(const CompiledMidiEvent&) const = default;
};

struct MidiCompilation {
  ShowValidation validation;
  std::vector<CompiledMidiEvent> events;

  [[nodiscard]] bool ok() const noexcept { return validation.ok(); }
};

// Authoring/storage validity. Zero songs is valid so a new rig/project can be
// saved before musical programming begins. Maximum remains exactly 15 songs.
ShowValidation validate_show_program(
    const ShowProgram& program,
    const std::set<std::string>& available_look_ids);

// Show-mode/Arm preflight. Includes authoring validation and additionally
// requires at least one fully valid song.
ShowValidation validate_show_program_for_performance(
    const ShowProgram& program,
    const std::set<std::string>& available_look_ids);

MidiCompilation compile_song_midi(
    const SongProgram& song,
    const std::set<std::string>& available_look_ids);

// Geometric/editor lookup: returns a clip only while the playhead is inside its
// authored block. This function does NOT implement latch persistence.
[[nodiscard]] const MidiSceneClip* active_clip_at_tick(
    const SongProgram& song, std::uint64_t tick) noexcept;

// Show playback lookup: reconstructs the effective scene from the absolute
// playhead position. Momentary cues override the latest latch while active;
// otherwise the most recent latch persists until a later latch cue or song end.
[[nodiscard]] const SceneDefinition* resolved_scene_at_tick(
    const SongProgram& song, std::uint64_t tick) noexcept;

// Deterministic live/host-facing cue state. Timeline seek is authoritative and
// clears manual overrides. Latch Note Off never cancels the selected cue;
// momentary Note Off releases only the matching active override.
//
// The runtime owns its loaded song copy so project-vector reallocation or a
// project reload can never leave dangling pointers in live playback state.
class CueRuntime final {
 public:
  explicit CueRuntime(const SongProgram& song) : song_(song) {}

  void seek(std::uint64_t tick) noexcept;
  // Continuous host playback updates timeline state without discarding a live
  // manual latch/momentary override. Seek/Loop uses seek() and clears them.
  void advance(std::uint64_t tick) noexcept;
  void transport_start() noexcept { transport_running_ = true; }
  void transport_stop() noexcept;

  void note_on(std::uint8_t note, std::uint8_t velocity,
               std::uint8_t channel) noexcept;
  void note_off(std::uint8_t note, std::uint8_t channel) noexcept;
  void all_notes_off() noexcept;

  [[nodiscard]] const SceneDefinition* effective_scene() const noexcept;
  [[nodiscard]] const SceneDefinition* timeline_scene() const noexcept {
    return timeline_scene_;
  }
  [[nodiscard]] const SceneDefinition* live_latch_scene() const noexcept {
    return live_latch_scene_;
  }
  [[nodiscard]] const SceneDefinition* momentary_scene() const noexcept {
    return momentary_scene_;
  }
  [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }
  [[nodiscard]] bool transport_running() const noexcept {
    return transport_running_;
  }

 private:
  [[nodiscard]] const SceneDefinition* scene_for_midi(
      std::uint8_t note, std::uint8_t channel) const noexcept;

  SongProgram song_;
  const SceneDefinition* timeline_scene_{nullptr};
  const SceneDefinition* live_latch_scene_{nullptr};
  const SceneDefinition* momentary_scene_{nullptr};
  std::uint64_t tick_{0U};
  std::uint8_t momentary_note_{0U};
  std::uint8_t momentary_channel_{0U};
  bool transport_running_{false};
};

}  // namespace aeyla::show
