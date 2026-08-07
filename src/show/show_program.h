#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace aeyla::show {

constexpr std::uint32_t kDefaultPpq = 960U;

struct SceneDefinition {
  std::string scene_id;
  std::string name;
  std::string look_id;
  std::uint32_t transition_in_ms{0U};
  std::uint32_t transition_out_ms{0U};
  bool blackout{false};
  bool operator==(const SceneDefinition&) const = default;
};

// One authored block in the MIDI editor. Its duration is meaningful: Note On
// activates the scene and Note Off ends the block. Alpha 0.3 intentionally uses
// one scene lane, so clips may touch but may not overlap.
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

ShowValidation validate_show_program(
    const ShowProgram& program,
    const std::set<std::string>& available_look_ids);

MidiCompilation compile_song_midi(
    const SongProgram& song,
    const std::set<std::string>& available_look_ids);

[[nodiscard]] const MidiSceneClip* active_clip_at_tick(
    const SongProgram& song, std::uint64_t tick) noexcept;

}  // namespace aeyla::show
