#include "show/show_program.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string_view>

namespace aeyla::show {
namespace {

constexpr std::size_t kMaximumSongs = 15U;
constexpr std::size_t kMaximumScenesPerSong = 2048U;
constexpr std::size_t kMaximumClipsPerSong = 100000U;
constexpr std::uint32_t kMaximumTransitionMs = 60000U;

bool valid_id(std::string_view value) noexcept {
  if (value.empty() || value.size() > 128U) return false;
  return std::all_of(value.begin(), value.end(), [](char character) {
    const unsigned char ch = static_cast<unsigned char>(character);
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
           ch == '.';
  });
}

bool power_of_two(std::uint8_t value) noexcept {
  return value != 0U && (value & static_cast<std::uint8_t>(value - 1U)) == 0U;
}

void error(ShowValidation& validation, std::string path, std::string message) {
  validation.diagnostics.push_back(
      {ShowDiagnosticSeverity::error, std::move(path), std::move(message)});
}

void validate_song(const SongProgram& song, std::size_t song_index,
                   const std::set<std::string>& available_look_ids,
                   ShowValidation& validation) {
  const std::string base = "songs[" + std::to_string(song_index) + "]";

  if (!valid_id(song.song_id))
    error(validation, base + ".songId", "song ID must be a stable ASCII identifier");
  if (song.name.empty() || song.name.size() > 256U)
    error(validation, base + ".name", "song name must contain 1 to 256 bytes");
  if (!std::isfinite(song.tempo_bpm) || song.tempo_bpm < 20.0 ||
      song.tempo_bpm > 300.0)
    error(validation, base + ".tempoBpm", "tempo must be finite and between 20 and 300 BPM");
  if (song.time_signature_numerator == 0U ||
      song.time_signature_numerator > 32U)
    error(validation, base + ".timeSignature.numerator",
          "time-signature numerator must be between 1 and 32");
  if (!power_of_two(song.time_signature_denominator) ||
      song.time_signature_denominator > 32U)
    error(validation, base + ".timeSignature.denominator",
          "time-signature denominator must be a power of two no greater than 32");
  if (song.ppq < 24U || song.ppq > 9600U)
    error(validation, base + ".ppq", "PPQ must be between 24 and 9600");
  if (song.length_ticks == 0U)
    error(validation, base + ".lengthTicks", "song length must be greater than zero");
  if (song.scenes.empty() || song.scenes.size() > kMaximumScenesPerSong)
    error(validation, base + ".scenes",
          "song must contain between 1 and 2048 scenes");
  if (song.clips.size() > kMaximumClipsPerSong)
    error(validation, base + ".clips", "song exceeds the 100000 clip limit");

  std::set<std::string> scene_ids;
  for (std::size_t index = 0; index < song.scenes.size(); ++index) {
    const auto& scene = song.scenes[index];
    const std::string path = base + ".scenes[" + std::to_string(index) + "]";
    if (!valid_id(scene.scene_id))
      error(validation, path + ".sceneId", "scene ID must be a stable ASCII identifier");
    else if (!scene_ids.insert(scene.scene_id).second)
      error(validation, path + ".sceneId", "scene ID must be unique within the song");
    if (scene.name.empty() || scene.name.size() > 256U)
      error(validation, path + ".name", "scene name must contain 1 to 256 bytes");
    if (!scene.blackout) {
      if (!valid_id(scene.look_id))
        error(validation, path + ".lookId", "non-blackout scene requires a valid look ID");
      else if (!available_look_ids.contains(scene.look_id))
        error(validation, path + ".lookId", "scene references a look that is not available");
    }
    if (scene.transition_in_ms > kMaximumTransitionMs ||
        scene.transition_out_ms > kMaximumTransitionMs)
      error(validation, path + ".transition",
            "scene transitions may not exceed 60000 ms");
  }

  std::set<std::string> clip_ids;
  std::vector<const MidiSceneClip*> ordered;
  ordered.reserve(song.clips.size());
  for (std::size_t index = 0; index < song.clips.size(); ++index) {
    const auto& clip = song.clips[index];
    const std::string path = base + ".clips[" + std::to_string(index) + "]";
    if (!valid_id(clip.clip_id))
      error(validation, path + ".clipId", "clip ID must be a stable ASCII identifier");
    else if (!clip_ids.insert(clip.clip_id).second)
      error(validation, path + ".clipId", "clip ID must be unique within the song");
    if (!scene_ids.contains(clip.scene_id))
      error(validation, path + ".sceneId", "clip references an unknown scene");
    if (clip.duration_ticks == 0U)
      error(validation, path + ".durationTicks", "clip duration must be greater than zero");
    if (clip.start_tick >= song.length_ticks)
      error(validation, path + ".startTick", "clip starts outside the song");
    if (clip.duration_ticks > song.length_ticks ||
        clip.start_tick > song.length_ticks -
                              std::min(clip.duration_ticks, song.length_ticks))
      error(validation, path + ".durationTicks", "clip extends beyond the song");
    if (clip.note > 127U)
      error(validation, path + ".note", "MIDI note must be between 0 and 127");
    if (clip.velocity == 0U || clip.velocity > 127U)
      error(validation, path + ".velocity", "MIDI velocity must be between 1 and 127");
    if (clip.channel == 0U || clip.channel > 16U)
      error(validation, path + ".channel", "MIDI channel must be between 1 and 16");
    ordered.push_back(&clip);
  }

  std::sort(ordered.begin(), ordered.end(), [](const MidiSceneClip* left,
                                                const MidiSceneClip* right) {
    if (left->start_tick != right->start_tick)
      return left->start_tick < right->start_tick;
    return left->clip_id < right->clip_id;
  });
  for (std::size_t index = 1U; index < ordered.size(); ++index) {
    const auto* previous = ordered[index - 1U];
    const auto* current = ordered[index];
    const std::uint64_t previous_end = previous->start_tick + previous->duration_ticks;
    if (current->start_tick < previous_end) {
      error(validation, base + ".clips",
            "Alpha 0.3 uses one deterministic scene lane; clips may touch but not overlap");
      break;
    }
  }
}

}  // namespace

bool ShowValidation::ok() const noexcept { return error_count() == 0U; }

std::size_t ShowValidation::error_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      diagnostics.begin(), diagnostics.end(), [](const ShowDiagnostic& diagnostic) {
        return diagnostic.severity == ShowDiagnosticSeverity::error;
      }));
}

ShowValidation validate_show_program(
    const ShowProgram& program,
    const std::set<std::string>& available_look_ids) {
  ShowValidation validation;
  if (program.songs.empty())
    error(validation, "songs", "show must contain at least one song");
  if (program.songs.size() > kMaximumSongs)
    error(validation, "songs", "show exceeds the 15 song limit");

  std::set<std::string> song_ids;
  for (std::size_t index = 0; index < program.songs.size(); ++index) {
    const auto& song = program.songs[index];
    if (valid_id(song.song_id) && !song_ids.insert(song.song_id).second)
      error(validation, "songs[" + std::to_string(index) + "].songId",
            "song ID must be unique within the show");
    validate_song(song, index, available_look_ids, validation);
  }
  return validation;
}

MidiCompilation compile_song_midi(
    const SongProgram& song,
    const std::set<std::string>& available_look_ids) {
  ShowProgram program;
  program.songs.push_back(song);
  MidiCompilation result;
  result.validation = validate_show_program(program, available_look_ids);
  if (!result.validation.ok()) return result;

  result.events.reserve(song.clips.size() * 2U);
  for (const auto& clip : song.clips) {
    result.events.push_back({clip.start_tick, MidiEventKind::note_on,
                             clip.note, clip.velocity, clip.channel,
                             song.song_id, clip.scene_id, clip.clip_id});
    result.events.push_back({clip.start_tick + clip.duration_ticks,
                             MidiEventKind::note_off, clip.note, 0U,
                             clip.channel, song.song_id, clip.scene_id,
                             clip.clip_id});
  }

  std::sort(result.events.begin(), result.events.end(),
            [](const CompiledMidiEvent& left, const CompiledMidiEvent& right) {
    if (left.tick != right.tick) return left.tick < right.tick;
    if (left.kind != right.kind)
      return left.kind == MidiEventKind::note_off;
    if (left.channel != right.channel) return left.channel < right.channel;
    if (left.note != right.note) return left.note < right.note;
    return left.clip_id < right.clip_id;
  });
  return result;
}

const MidiSceneClip* active_clip_at_tick(const SongProgram& song,
                                         std::uint64_t tick) noexcept {
  for (const auto& clip : song.clips) {
    if (tick >= clip.start_tick &&
        tick - clip.start_tick < clip.duration_ticks)
      return &clip;
  }
  return nullptr;
}

}  // namespace aeyla::show
