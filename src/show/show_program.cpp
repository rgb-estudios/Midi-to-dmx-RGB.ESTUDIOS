#include "show/show_program.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace aeyla::show {
namespace {

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

const SceneDefinition* find_scene(const SongProgram& song,
                                  std::string_view scene_id) noexcept {
  const auto found = std::find_if(
      song.scenes.begin(), song.scenes.end(),
      [&](const SceneDefinition& scene) { return scene.scene_id == scene_id; });
  return found == song.scenes.end() ? nullptr : &*found;
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
  if (song.scenes.size() > kMaximumScenesPerSong)
    error(validation, base + ".scenes",
          "song may not exceed 2048 scenes");
  if (song.clips.size() > kMaximumClipsPerSong)
    error(validation, base + ".clips", "song exceeds the 100000 clip limit");

  std::set<std::string> scene_ids;
  std::map<std::string, std::pair<std::uint8_t, std::uint8_t>>
      scene_midi_bindings;
  std::map<std::pair<std::uint8_t, std::uint8_t>, std::string>
      midi_bindings;
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
    if (scene.midi_binding.has_value()) {
      const auto& mapping = *scene.midi_binding;
      if (mapping.note > 127U || mapping.channel == 0U || mapping.channel > 16U) {
        error(validation, path + ".midiBinding",
              "Cue MIDI binding must use note 0..127 and channel 1..16");
      } else {
        scene_midi_bindings.emplace(
            scene.scene_id, std::make_pair(mapping.channel, mapping.note));
        const auto [binding, inserted] = midi_bindings.emplace(
            std::make_pair(mapping.channel, mapping.note), scene.scene_id);
        if (!inserted && binding->second != scene.scene_id)
          error(validation, path + ".midiBinding",
                "one MIDI note/channel may not address multiple Cues");
      }
    }
  }

  std::set<std::string> clip_ids;
  std::vector<const MidiSceneClip*> latch_clips;
  std::vector<const MidiSceneClip*> momentary_clips;
  latch_clips.reserve(song.clips.size());
  momentary_clips.reserve(song.clips.size());

  for (std::size_t index = 0; index < song.clips.size(); ++index) {
    const auto& clip = song.clips[index];
    const std::string path = base + ".clips[" + std::to_string(index) + "]";
    if (!valid_id(clip.clip_id))
      error(validation, path + ".clipId", "clip ID must be a stable ASCII identifier");
    else if (!clip_ids.insert(clip.clip_id).second)
      error(validation, path + ".clipId", "clip ID must be unique within the song");

    const SceneDefinition* scene = find_scene(song, clip.scene_id);
    if (scene == nullptr)
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

    const auto legacy_key = std::make_pair(clip.channel, clip.note);
    const auto scene_mapping = scene_midi_bindings.find(clip.scene_id);
    if (scene_mapping == scene_midi_bindings.end()) {
      scene_midi_bindings.emplace(clip.scene_id, legacy_key);
    } else if (scene_mapping->second != legacy_key) {
      error(validation, path + ".note",
            "Cue placements must use their Cue-owned MIDI binding");
    }
    const auto effective_mapping = scene_midi_bindings.find(clip.scene_id);
    if (effective_mapping != scene_midi_bindings.end()) {
      const auto [binding, inserted] = midi_bindings.emplace(
          effective_mapping->second, clip.scene_id);
      if (!inserted && binding->second != clip.scene_id)
        error(validation, path + ".note",
              "one MIDI note/channel may not address multiple Cues within a song");
    }

    if (scene != nullptr) {
      if (scene->behavior == CueBehavior::momentary)
        momentary_clips.push_back(&clip);
      else
        latch_clips.push_back(&clip);
    }
  }

  std::sort(latch_clips.begin(), latch_clips.end(),
            [](const MidiSceneClip* left, const MidiSceneClip* right) {
    if (left->start_tick != right->start_tick)
      return left->start_tick < right->start_tick;
    return left->clip_id < right->clip_id;
  });
  for (std::size_t index = 1U; index < latch_clips.size(); ++index) {
    if (latch_clips[index - 1U]->start_tick == latch_clips[index]->start_tick) {
      error(validation, base + ".clips",
            "two latch cues may not share the same start tick");
      break;
    }
  }

  std::sort(momentary_clips.begin(), momentary_clips.end(),
            [](const MidiSceneClip* left, const MidiSceneClip* right) {
    if (left->start_tick != right->start_tick)
      return left->start_tick < right->start_tick;
    return left->clip_id < right->clip_id;
  });
  for (std::size_t index = 1U; index < momentary_clips.size(); ++index) {
    const auto* previous = momentary_clips[index - 1U];
    const auto* current = momentary_clips[index];
    const std::uint64_t previous_end = previous->start_tick + previous->duration_ticks;
    if (current->start_tick < previous_end) {
      error(validation, base + ".clips",
            "momentary cue blocks may touch but may not overlap");
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

ShowValidation validate_show_program_for_performance(
    const ShowProgram& program,
    const std::set<std::string>& available_look_ids) {
  ShowValidation validation = validate_show_program(program, available_look_ids);
  if (program.songs.empty())
    error(validation, "songs", "performance requires at least one programmed song");
  for (std::size_t index = 0U; index < program.songs.size(); ++index) {
    const auto& song = program.songs[index];
    if (song.scenes.empty() || song.clips.empty())
      error(validation, "songs[" + std::to_string(index) + "]",
            "performance requires at least one stored Cue placement per Song");
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
    const SceneDefinition* scene = find_scene(song, clip.scene_id);
    const std::uint8_t note = scene != nullptr && scene->midi_binding.has_value()
        ? scene->midi_binding->note
        : clip.note;
    const std::uint8_t channel = scene != nullptr && scene->midi_binding.has_value()
        ? scene->midi_binding->channel
        : clip.channel;
    result.events.push_back({clip.start_tick, MidiEventKind::note_on,
                             note, clip.velocity, channel,
                             song.song_id, clip.scene_id, clip.clip_id});
    result.events.push_back({clip.start_tick + clip.duration_ticks,
                             MidiEventKind::note_off, note, 0U,
                             channel, song.song_id, clip.scene_id,
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

const SceneDefinition* resolved_scene_at_tick(const SongProgram& song,
                                               std::uint64_t tick) noexcept {
  if (tick >= song.length_ticks) return nullptr;

  const MidiSceneClip* latest_latch = nullptr;
  const MidiSceneClip* active_momentary = nullptr;

  for (const auto& clip : song.clips) {
    if (clip.start_tick > tick) continue;
    const SceneDefinition* scene = find_scene(song, clip.scene_id);
    if (scene == nullptr) continue;

    if (scene->behavior == CueBehavior::momentary) {
      if (tick - clip.start_tick < clip.duration_ticks &&
          (active_momentary == nullptr ||
           clip.start_tick > active_momentary->start_tick ||
           (clip.start_tick == active_momentary->start_tick &&
            clip.clip_id < active_momentary->clip_id))) {
        active_momentary = &clip;
      }
      continue;
    }

    if (latest_latch == nullptr || clip.start_tick > latest_latch->start_tick ||
        (clip.start_tick == latest_latch->start_tick &&
         clip.clip_id < latest_latch->clip_id)) {
      latest_latch = &clip;
    }
  }

  if (active_momentary != nullptr)
    return find_scene(song, active_momentary->scene_id);
  if (latest_latch != nullptr)
    return find_scene(song, latest_latch->scene_id);
  return nullptr;
}

void CueRuntime::seek(std::uint64_t tick) noexcept {
  tick_ = tick;
  timeline_scene_ = resolved_scene_at_tick(song_, tick);
  live_latch_scene_ = nullptr;
  momentary_scene_ = nullptr;
  momentary_note_ = 0U;
  momentary_channel_ = 0U;
}

void CueRuntime::advance(std::uint64_t tick) noexcept {
  tick_ = tick;
  timeline_scene_ = resolved_scene_at_tick(song_, tick);
}

void CueRuntime::transport_stop() noexcept {
  transport_running_ = false;
  timeline_scene_ = nullptr;
  live_latch_scene_ = nullptr;
  momentary_scene_ = nullptr;
  momentary_note_ = 0U;
  momentary_channel_ = 0U;
}

const SceneDefinition* CueRuntime::scene_for_midi(
    std::uint8_t note, std::uint8_t channel) const noexcept {
  for (const auto& scene : song_.scenes) {
    if (scene.midi_binding.has_value() &&
        scene.midi_binding->note == note &&
        scene.midi_binding->channel == channel)
      return &scene;
  }
  for (const auto& clip : song_.clips) {
    if (clip.note == note && clip.channel == channel)
      return find_scene(song_, clip.scene_id);
  }
  return nullptr;
}

void CueRuntime::note_on(std::uint8_t note, std::uint8_t velocity,
                         std::uint8_t channel) noexcept {
  if (velocity == 0U) {
    note_off(note, channel);
    return;
  }

  const SceneDefinition* scene = scene_for_midi(note, channel);
  if (scene == nullptr) return;

  if (scene->behavior == CueBehavior::momentary) {
    momentary_scene_ = scene;
    momentary_note_ = note;
    momentary_channel_ = channel;
  } else {
    live_latch_scene_ = scene;
  }
}

void CueRuntime::note_off(std::uint8_t note, std::uint8_t channel) noexcept {
  if (momentary_scene_ != nullptr && momentary_note_ == note &&
      momentary_channel_ == channel) {
    momentary_scene_ = nullptr;
    momentary_note_ = 0U;
    momentary_channel_ = 0U;
  }
}

void CueRuntime::all_notes_off() noexcept {
  momentary_scene_ = nullptr;
  momentary_note_ = 0U;
  momentary_channel_ = 0U;
}

const SceneDefinition* CueRuntime::effective_scene() const noexcept {
  if (momentary_scene_ != nullptr) return momentary_scene_;
  if (live_latch_scene_ != nullptr) return live_latch_scene_;
  return timeline_scene_;
}

}  // namespace aeyla::show
