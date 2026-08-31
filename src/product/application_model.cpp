#include "product/application_model.h"

#include "core/fixture_profile.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace aeyla::product {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr const char* kDevelopmentProjectId =
    "00000000-0000-4000-8000-000000000003";
constexpr const char* kDevelopmentTimestamp = "2026-08-07T00:00:00Z";

float clamp01(float value) {
  if (!std::isfinite(value)) return 0.0F;
  return std::clamp(value, 0.0F, 1.0F);
}

RgbColor mix(const RgbColor& a, const RgbColor& b, float amount) {
  const float t = clamp01(amount);
  return {
      a.red + (b.red - a.red) * t,
      a.green + (b.green - a.green) * t,
      a.blue + (b.blue - a.blue) * t,
  };
}

std::optional<Attribute> attribute_from_name(std::string_view name) {
  if (name == "dimmer") return Attribute::Dimmer;
  if (name == "shutter") return Attribute::Shutter;
  if (name == "strobe") return Attribute::Strobe;
  if (name == "red") return Attribute::Red;
  if (name == "green") return Attribute::Green;
  if (name == "blue") return Attribute::Blue;
  if (name == "white") return Attribute::White;
  if (name == "amber") return Attribute::Amber;
  if (name == "uv") return Attribute::UV;
  if (name == "lime") return Attribute::Lime;
  if (name == "macro") return Attribute::Macro;
  if (name == "speed") return Attribute::Speed;
  if (name == "reset") return Attribute::Reset;
  if (name == "zoom") return Attribute::Zoom;
  if (name == "fan") return Attribute::Fan;
  if (name == "haze") return Attribute::Haze;
  return std::nullopt;
}

std::optional<VisualSource> source_from_name(std::string_view name) {
  if (name == "solid") return VisualSource::solid;
  if (name == "gradient") return VisualSource::gradient;
  if (name == "wave") return VisualSource::wave;
  if (name == "noise") return VisualSource::noise;
  if (name == "chase") return VisualSource::chase;
  return std::nullopt;
}

std::string_view source_name(VisualSource source) {
  switch (source) {
    case VisualSource::solid: return "solid";
    case VisualSource::gradient: return "gradient";
    case VisualSource::wave: return "wave";
    case VisualSource::noise: return "noise";
    case VisualSource::chase: return "chase";
  }
  return "gradient";
}

FixtureProfile make_runtime_profile(
    const project::FixtureProfileDocument& document) {
  FixtureProfile profile;
  profile.id = document.profile_id;
  profile.name = document.display_name;
  profile.footprint = document.footprint;
  profile.channels.reserve(document.channels.size());
  for (const auto& channel : document.channels) {
    if (const auto attribute = attribute_from_name(channel.attribute)) {
      profile.channels.push_back({channel.slot, *attribute});
    }
  }
  return profile;
}

RgbColor sample_source(VisualSource source, const RgbColor& primary,
                       const RgbColor& secondary, float x, float y,
                       float phase) {
  const float nx = clamp01(x);
  const float ny = clamp01(y);
  const float p = phase - std::floor(phase);

  switch (source) {
    case VisualSource::solid:
      return primary;

    case VisualSource::gradient:
      return mix(primary, secondary, nx);

    case VisualSource::wave: {
      const float value = 0.5F + 0.5F * std::sin((nx * 4.0F + p * 2.0F) * kPi);
      return mix(primary, secondary, value);
    }

    case VisualSource::noise: {
      const auto xi = static_cast<std::uint32_t>(nx * 1024.0F);
      const auto yi = static_cast<std::uint32_t>(ny * 1024.0F);
      const auto pi = static_cast<std::uint32_t>(p * 120.0F);
      std::uint32_t seed = xi * 73856093U ^ yi * 19349663U ^ pi * 83492791U;
      seed ^= seed >> 13U;
      seed *= 1274126177U;
      const float value = static_cast<float>((seed >> 8U) & 255U) / 255.0F;
      return mix(primary, secondary, value);
    }

    case VisualSource::chase: {
      const float raw_distance = std::fabs(nx - p);
      const float wrapped_distance = std::min(raw_distance, 1.0F - raw_distance);
      const float value = clamp01(1.0F - wrapped_distance * 7.0F);
      return mix(primary, secondary, value);
    }
  }

  return {};
}

VisualSource effective_source(VisualSource authored, int active_executor) {
  if (active_executor >= 0 && active_executor <= 4) {
    return static_cast<VisualSource>(active_executor);
  }
  return authored;
}

void add_runtime_error(project::ProjectValidation& validation,
                       std::string path, std::string message) {
  validation.diagnostics.push_back(
      {project::DiagnosticSeverity::error, std::move(path), std::move(message)});
}

std::set<std::string> available_look_ids(
    const project::ProjectDocument& document) {
  std::set<std::string> result;
  for (const auto& look : document.looks) result.insert(look.look_id);
  return result;
}

void append_show_validation(project::ProjectValidation& destination,
                            const show::ShowValidation& source) {
  for (const auto& diagnostic : source.diagnostics) {
    const auto severity =
        diagnostic.severity == show::ShowDiagnosticSeverity::error
            ? project::DiagnosticSeverity::error
            : project::DiagnosticSeverity::warning;
    destination.diagnostics.push_back(
        {severity, "show." + diagnostic.path, diagnostic.message});
  }
}

project::ProjectValidation validate_runtime_bundle(
    const project::ProjectDocument& document,
    const show::ShowProgram& show_program) {
  project::ProjectValidation validation =
      project::validate_project_document(document);

  for (std::size_t index = 0; index < document.fixtures.size(); ++index) {
    if (document.fixtures[index].universe != document.output.universe) {
      add_runtime_error(
          validation,
          "rig.fixtures[" + std::to_string(index) + "].universe",
          "Alpha 0.3 supports one output universe; fixture must match output.universe");
    }
  }

  const auto active_look = std::find_if(
      document.looks.begin(), document.looks.end(),
      [&](const project::LookDocument& look) {
        return look.look_id == document.visual.active_look_id;
      });
  if (active_look == document.looks.end() ||
      !source_from_name(active_look->source).has_value()) {
    add_runtime_error(validation, "visual.activeLookId",
                      "active look cannot be converted to a runtime source");
  }

  append_show_validation(
      validation,
      show::validate_show_program(show_program, available_look_ids(document)));
  return validation;
}

}  // namespace

ApplicationModel::ApplicationModel()
    : project_(project::make_default_project_document(
          kDevelopmentProjectId, kDevelopmentTimestamp)) {
  color_settings_.intensity = 1.0F;
  color_settings_.white_extraction = project_.visual.white_extraction;
  color_settings_.amber_extraction = project_.visual.amber_extraction;
  color_settings_.lime_extraction = 0.20F;
  color_settings_.uv_manual = project_.visual.uv_manual;

  const auto validation = validate_runtime_bundle(project_, show_program_);
  safety_.set_project_valid(validation.ok());
  performance_ready_ = show::validate_show_program_for_performance(
      show_program_, available_look_ids(project_)).ok();
  // A simulated/null backend must not satisfy the real output-arm gate.
  safety_.set_backend_ready(false);
  rebuild_cue_runtime();
  rebuild();
}

project::ProjectValidation ApplicationModel::load_project_bundle(
    const project::ProjectDocument& document,
    const show::ShowProgram& show_program) {
  // Validate everything before touching the current runtime. Failed Open must
  // leave the currently loaded valid project+show untouched.
  project::ProjectValidation validation =
      validate_runtime_bundle(document, show_program);
  if (!validation.ok()) return validation;

  const auto active_look = std::find_if(
      document.looks.begin(), document.looks.end(),
      [&](const project::LookDocument& look) {
        return look.look_id == document.visual.active_look_id;
      });

  safety_.begin_project_reload();
  active_executor_ = -1;
  executor_velocity_ = 0.0F;

  project_ = document;
  project_.output.armed = false;
  project_.visual.speed = active_look->speed;
  project_.visual.white_extraction = active_look->white_extraction;
  project_.visual.amber_extraction = active_look->amber_extraction;
  project_.visual.uv_manual = active_look->uv_manual;
  show_program_ = show_program;
  active_song_index_ = 0U;
  authored_source_ = *source_from_name(active_look->source);
  color_settings_.white_extraction = project_.visual.white_extraction;
  color_settings_.amber_extraction = project_.visual.amber_extraction;
  color_settings_.uv_manual = project_.visual.uv_manual;
  rig14_ = project_.fixtures.size() == 14U &&
           std::all_of(project_.fixtures.begin(), project_.fixtures.end(),
                       [](const project::FixtureDocument& fixture) {
                         return fixture.enabled;
                       });
  project_dirty_ = false;
  performance_ready_ = show::validate_show_program_for_performance(
      show_program_, available_look_ids(project_)).ok();
  rebuild_cue_runtime();

  safety_.complete_project_reload(true);
  rebuild();
  return validation;
}

project::ProjectValidation ApplicationModel::load_project_document(
    const project::ProjectDocument& document) {
  return load_project_bundle(document, show::ShowProgram{});
}

project::ProjectDocument ApplicationModel::project_document_for_save(
    std::string modified_at) const {
  project::ProjectDocument copy = project_;
  copy.modified_at = std::move(modified_at);
  copy.output.armed = false;
  return copy;
}

show::ShowValidation ApplicationModel::replace_show_program(
    const show::ShowProgram& program) {
  const auto validation =
      show::validate_show_program(program, available_look_ids(project_));
  if (!validation.ok() || program == show_program_) return validation;

  show_program_ = program;
  if (active_song_index_ >= show_program_.songs.size()) active_song_index_ = 0U;
  performance_ready_ = show::validate_show_program_for_performance(
      show_program_, available_look_ids(project_)).ok();
  active_executor_ = -1;
  executor_velocity_ = 0.0F;
  rebuild_cue_runtime();
  safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
  safety_.set_blackout(true);
  mark_dirty();
  rebuild();
  return validation;
}

show::ShowValidation ApplicationModel::show_performance_validation() const {
  return show::validate_show_program_for_performance(
      show_program_, available_look_ids(project_));
}

AuthoringResult ApplicationModel::store_current_look() {
  if (project_.looks.size() >= project::kMaximumLooks)
    return {false, {}, "The project already contains the 2048-Look maximum"};
  const auto active = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& look) {
        return look.look_id == project_.visual.active_look_id;
      });
  if (active == project_.looks.end())
    return {false, {}, "No active Look is available to store"};

  std::size_t number = project_.looks.size() + 1U;
  std::string id;
  do {
    id = "look-user-" + std::to_string(number++);
  } while (std::any_of(project_.looks.begin(), project_.looks.end(),
                       [&](const project::LookDocument& look) {
                         return look.look_id == id;
                       }));

  project::LookDocument stored = *active;
  stored.look_id = id;
  stored.name = "Look " + std::to_string(project_.looks.size() + 1U);

  project::ProjectDocument candidate = project_;
  candidate.looks.push_back(stored);
  candidate.visual.active_look_id = id;
  const auto validation = validate_runtime_bundle(candidate, show_program_);
  if (!validation.ok())
    return {false, {}, "Stored Look failed project validation"};

  project_ = std::move(candidate);
  authored_source_ = source_from_name(stored.source).value_or(authored_source_);
  safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
  safety_.set_blackout(true);
  mark_dirty();
  rebuild();
  return {true, id, stored.name};
}

AuthoringResult ApplicationModel::create_song() {
  if (show_program_.songs.size() >= show::kMaximumSongs)
    return {false, {}, "The Show already contains the 15-Song maximum"};

  std::size_t number = show_program_.songs.size() + 1U;
  std::string id;
  do {
    id = "song-" + std::to_string(number++);
  } while (std::any_of(show_program_.songs.begin(), show_program_.songs.end(),
                       [&](const show::SongProgram& song) {
                         return song.song_id == id;
                       }));

  show::ShowProgram candidate = show_program_;
  show::SongProgram song;
  song.song_id = id;
  song.name = "Song " + std::to_string(candidate.songs.size() + 1U);
  song.length_ticks = 16U * song.ppq;
  candidate.songs.push_back(song);
  const auto validation = replace_show_program(candidate);
  if (!validation.ok())
    return {false, {}, "New Song failed authoring validation"};

  (void)select_song(candidate.songs.size() - 1U);
  return {true, id, song.name};
}

AuthoringResult ApplicationModel::store_cue_at_tick(
    std::uint64_t tick, show::CueBehavior behavior) {
  if (active_song_index_ >= show_program_.songs.size())
    return {false, {}, "Create or select a Song before storing a Cue"};
  if (project_.visual.active_look_id.empty())
    return {false, {}, "Store or select a Look before storing a Cue"};

  show::ShowProgram candidate = show_program_;
  auto& song = candidate.songs[active_song_index_];
  if (behavior == show::CueBehavior::latch &&
      std::any_of(song.clips.begin(), song.clips.end(),
                  [&](const show::MidiSceneClip& clip) {
                    if (clip.start_tick != tick) return false;
                    const auto scene = std::find_if(
                        song.scenes.begin(), song.scenes.end(),
                        [&](const show::SceneDefinition& item) {
                          return item.scene_id == clip.scene_id;
                        });
                    return scene != song.scenes.end() &&
                           scene->behavior == show::CueBehavior::latch;
                  })) {
    return {false, {}, "A latch Cue already exists at this playhead position"};
  }

  std::array<bool, 128> used_notes{};
  for (const auto& scene : song.scenes)
    if (scene.midi_binding.has_value() && scene.midi_binding->channel == 1U)
      used_notes[scene.midi_binding->note] = true;
  for (const auto& clip : song.clips)
    if (clip.channel == 1U) used_notes[clip.note] = true;
  std::optional<std::uint8_t> note;
  for (std::uint16_t candidate_note = 36U; candidate_note <= 127U;
       ++candidate_note) {
    if (!used_notes[candidate_note]) {
      note = static_cast<std::uint8_t>(candidate_note);
      break;
    }
  }
  if (!note.has_value())
    return {false, {}, "No free MIDI Learn note remains in the active Song"};

  std::size_t number = song.scenes.size() + 1U;
  std::string scene_id;
  std::string clip_id;
  do {
    scene_id = "cue-" + std::to_string(number);
    clip_id = "placement-" + std::to_string(number);
    ++number;
  } while (std::any_of(song.scenes.begin(), song.scenes.end(),
                       [&](const show::SceneDefinition& item) {
                         return item.scene_id == scene_id;
                       }) ||
           std::any_of(song.clips.begin(), song.clips.end(),
                       [&](const show::MidiSceneClip& item) {
                         return item.clip_id == clip_id;
                       }));
  const std::size_t display_number = number - 1U;
  if (tick >= song.length_ticks) {
    const std::uint64_t extension = 4U * static_cast<std::uint64_t>(song.ppq);
    if (tick > std::numeric_limits<std::uint64_t>::max() - extension)
      return {false, {}, "Cue playhead position is outside supported bounds"};
    song.length_ticks = tick + extension;
  }
  const std::uint64_t requested_duration =
      behavior == show::CueBehavior::momentary
          ? std::max<std::uint64_t>(1U, song.ppq / 4U)
          : static_cast<std::uint64_t>(song.ppq);
  const std::uint64_t duration =
      std::min(requested_duration, song.length_ticks - tick);

  show::SceneDefinition scene;
  scene.scene_id = scene_id;
  scene.name = "Cue " + std::to_string(display_number);
  scene.look_id = project_.visual.active_look_id;
  scene.transition_in_ms = behavior == show::CueBehavior::latch ? 250U : 0U;
  scene.transition_out_ms = behavior == show::CueBehavior::latch ? 250U : 0U;
  scene.behavior = behavior;
  scene.midi_binding = show::MidiBinding{*note, 1U};
  song.scenes.push_back(scene);
  song.clips.push_back(
      {clip_id, scene_id, tick, duration, *note, 127U, 1U});

  const auto validation = replace_show_program(candidate);
  if (!validation.ok())
    return {false, {}, "Stored Cue failed Show validation"};
  return {true, scene_id, scene.name};
}

bool ApplicationModel::select_look(std::size_t look_index) {
  if (look_index >= project_.looks.size()) return false;
  const auto& look = project_.looks[look_index];
  if (project_.visual.active_look_id == look.look_id) return true;

  project_.visual.active_look_id = look.look_id;
  project_.visual.speed = look.speed;
  project_.visual.white_extraction = look.white_extraction;
  project_.visual.amber_extraction = look.amber_extraction;
  project_.visual.uv_manual = look.uv_manual;
  color_settings_.white_extraction = look.white_extraction;
  color_settings_.amber_extraction = look.amber_extraction;
  color_settings_.uv_manual = look.uv_manual;
  authored_source_ = source_from_name(look.source).value_or(authored_source_);
  safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
  safety_.set_blackout(true);
  mark_dirty();
  rebuild();
  return true;
}

bool ApplicationModel::toggle_active_look_fixture(std::size_t fixture_index) {
  if (fixture_index >= project_.fixtures.size() || fixture_index >= 14U)
    return false;
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  if (look == project_.looks.end()) return false;
  look->fixture_mask[fixture_index] = !look->fixture_mask[fixture_index];
  safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
  safety_.set_blackout(true);
  mark_dirty();
  rebuild();
  return true;
}

bool ApplicationModel::active_look_fixture_enabled(
    std::size_t fixture_index) const noexcept {
  if (fixture_index >= 14U) return false;
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  return look != project_.looks.end() && look->fixture_mask[fixture_index];
}

bool ApplicationModel::set_active_look_color(bool secondary,
                                             const RgbColor& color) {
  if (!std::isfinite(color.red) || !std::isfinite(color.green) ||
      !std::isfinite(color.blue)) return false;
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  if (look == project_.looks.end()) return false;
  const std::array<float, 3> next{
      clamp01(color.red), clamp01(color.green), clamp01(color.blue)};
  auto& destination = secondary ? look->secondary_color : look->primary_color;
  if (destination == next) return true;
  destination = next;
  safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
  safety_.set_blackout(true);
  mark_dirty();
  rebuild();
  return true;
}

std::array<float, 3> ApplicationModel::active_look_color(
    bool secondary) const noexcept {
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  if (look == project_.looks.end()) return {};
  return secondary ? look->secondary_color : look->primary_color;
}

bool ApplicationModel::set_active_look_intensity(float intensity) {
  if (!std::isfinite(intensity)) return false;
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  if (look == project_.looks.end()) return false;
  const float next = clamp01(intensity);
  if (look->intensity == next) return true;
  look->intensity = next;
  safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
  safety_.set_blackout(true);
  mark_dirty();
  rebuild();
  return true;
}

float ApplicationModel::active_look_intensity() const noexcept {
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  return look == project_.looks.end() ? 0.0F : look->intensity;
}

AuthoringResult ApplicationModel::configure_artnet_output(
    std::string target_ipv4, std::uint16_t port_address) {
  if (target_ipv4.empty())
    return {false, {}, "Art-Net target IPv4 address is required"};
  if (port_address > 0x7FFFU)
    return {false, {}, "Art-Net port address exceeds 15 bits"};

  project::ProjectDocument candidate = project_;
  candidate.output.backend = "artnet";
  candidate.output.target = std::move(target_ipv4);
  candidate.output.universe = port_address;
  candidate.output.armed = false;
  for (auto& fixture : candidate.fixtures)
    fixture.universe = port_address;

  const auto validation = validate_runtime_bundle(candidate, show_program_);
  if (!validation.ok())
    return {false, {}, "Art-Net configuration failed project validation"};

  project_ = std::move(candidate);
  safety_.set_backend_ready(false);
  safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
  safety_.set_blackout(true);
  mark_dirty();
  rebuild();
  return {true, project_.output.target, "Art-Net configuration stored"};
}

AuthoringResult ApplicationModel::disable_output_backend() {
  project::ProjectDocument candidate = project_;
  candidate.output.backend = "none";
  candidate.output.target.clear();
  candidate.output.armed = false;

  const auto validation = validate_runtime_bundle(candidate, show_program_);
  if (!validation.ok())
    return {false, {}, "Disabled output configuration failed project validation"};

  project_ = std::move(candidate);
  safety_.set_backend_ready(false);
  safety_.disarm(runtime::RuntimeSafetyReason::backend_unavailable);
  safety_.set_blackout(true);
  mark_dirty();
  rebuild();
  return {true, {}, "Physical output disabled"};
}

bool ApplicationModel::select_song(std::size_t song_index) {
  if (song_index >= show_program_.songs.size()) return false;
  if (song_index == active_song_index_ && cue_runtime_.has_value()) return true;

  // Song selection is PREPARADA navigation, not a project reload and not an
  // operator safety command. Preserve ARM/global blackout while rebuilding the
  // artistic cue runtime for the newly prepared song.
  active_executor_ = -1;
  executor_velocity_ = 0.0F;
  active_song_index_ = song_index;
  rebuild_cue_runtime();
  rebuild();
  return true;
}

void ApplicationModel::seek_active_song_tick(std::uint64_t tick) {
  if (!cue_runtime_.has_value()) return;
  cue_runtime_->seek(tick);
  apply_cue_runtime_state();
  rebuild();
}

void ApplicationModel::advance_active_song_tick(std::uint64_t tick) {
  if (!cue_runtime_.has_value()) return;
  cue_runtime_->advance(tick);
  apply_cue_runtime_state();
  rebuild();
}

void ApplicationModel::mark_project_saved(std::string modified_at) {
  project_.modified_at = std::move(modified_at);
  project_.output.armed = false;
  project_dirty_ = false;
  rebuild();
}

void ApplicationModel::set_project_valid(bool valid) {
  safety_.set_project_valid(valid);
  rebuild();
}

void ApplicationModel::set_project_name(std::string name) {
  if (name.empty() || project_.name == name) return;
  project_.name = std::move(name);
  mark_dirty();
  rebuild();
}

void ApplicationModel::set_backend_ready(bool ready) {
  safety_.set_backend_ready(ready);
  rebuild();
}

bool ApplicationModel::request_arm() {
  if (!performance_ready_) {
    safety_.disarm(runtime::RuntimeSafetyReason::show_not_ready);
    safety_.set_blackout(true);
    rebuild();
    return false;
  }

  const bool armed = safety_.request_arm();
  rebuild();
  return armed;
}

void ApplicationModel::disarm(runtime::RuntimeSafetyReason reason) {
  safety_.disarm(reason);
  rebuild();
}

void ApplicationModel::set_blackout(bool enabled) {
  safety_.set_blackout(enabled);
  rebuild();
}

void ApplicationModel::set_grand_master(float value) {
  color_settings_.intensity = clamp01(value);
  rebuild();
}

void ApplicationModel::set_rig14(bool enabled) {
  bool changed = rig14_ != enabled;
  rig14_ = enabled;
  for (std::size_t index = 0; index < project_.fixtures.size(); ++index) {
    const bool desired = enabled || index < 10U;
    if (project_.fixtures[index].enabled != desired) {
      project_.fixtures[index].enabled = desired;
      changed = true;
    }
  }
  if (changed) {
    safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
    safety_.set_blackout(true);
    mark_dirty();
  }
  rebuild();
}

void ApplicationModel::set_visual_source(VisualSource source) {
  bool changed = authored_source_ != source;
  authored_source_ = source;
  const auto name = source_name(source);
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.source == name;
      });
  if (look != project_.looks.end() &&
      project_.visual.active_look_id != look->look_id) {
    project_.visual.active_look_id = look->look_id;
    project_.visual.speed = look->speed;
    project_.visual.white_extraction = look->white_extraction;
    project_.visual.amber_extraction = look->amber_extraction;
    project_.visual.uv_manual = look->uv_manual;
    color_settings_.white_extraction = look->white_extraction;
    color_settings_.amber_extraction = look->amber_extraction;
    color_settings_.uv_manual = look->uv_manual;
    changed = true;
  }
  if (changed) {
    safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
    safety_.set_blackout(true);
    mark_dirty();
  }
  rebuild();
}

void ApplicationModel::set_visual_speed(float value) {
  const float next = clamp01(value);
  bool changed = false;
  if (project_.visual.speed != next) {
    project_.visual.speed = next;
    changed = true;
  }
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  if (look != project_.looks.end() && look->speed != next) {
    look->speed = next;
    changed = true;
  }
  if (changed) {
    safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
    safety_.set_blackout(true);
    mark_dirty();
  }
  rebuild();
}

void ApplicationModel::set_phase(float normalized_phase) {
  if (!std::isfinite(normalized_phase)) normalized_phase = 0.0F;
  phase_ = normalized_phase - std::floor(normalized_phase);
  rebuild();
}

void ApplicationModel::set_white_extraction(float value) {
  const float next = clamp01(value);
  bool changed = false;
  if (color_settings_.white_extraction != next ||
      project_.visual.white_extraction != next) {
    color_settings_.white_extraction = next;
    project_.visual.white_extraction = next;
    changed = true;
  }
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  if (look != project_.looks.end() && look->white_extraction != next) {
    look->white_extraction = next;
    changed = true;
  }
  if (changed) {
    safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
    safety_.set_blackout(true);
    mark_dirty();
  }
  rebuild();
}

void ApplicationModel::set_amber_extraction(float value) {
  const float next = clamp01(value);
  bool changed = false;
  if (color_settings_.amber_extraction != next ||
      project_.visual.amber_extraction != next) {
    color_settings_.amber_extraction = next;
    project_.visual.amber_extraction = next;
    changed = true;
  }
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  if (look != project_.looks.end() && look->amber_extraction != next) {
    look->amber_extraction = next;
    changed = true;
  }
  if (changed) {
    safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
    safety_.set_blackout(true);
    mark_dirty();
  }
  rebuild();
}

void ApplicationModel::set_uv_manual(float value) {
  const float next = clamp01(value);
  bool changed = false;
  if (color_settings_.uv_manual != next || project_.visual.uv_manual != next) {
    color_settings_.uv_manual = next;
    project_.visual.uv_manual = next;
    changed = true;
  }
  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == project_.visual.active_look_id;
      });
  if (look != project_.looks.end() && look->uv_manual != next) {
    look->uv_manual = next;
    changed = true;
  }
  if (changed) {
    safety_.disarm(runtime::RuntimeSafetyReason::project_reload);
    safety_.set_blackout(true);
    mark_dirty();
  }
  rebuild();
}

void ApplicationModel::handle_host_event(const runtime::HostEvent& event) {
  if (cue_runtime_.has_value()) {
    switch (event.type) {
      case runtime::HostEventType::note_on: {
        const float normalized = clamp01(event.value);
        if (normalized <= 0.0F) {
          cue_runtime_->note_off(event.note, event.channel);
        } else {
          const auto velocity = static_cast<std::uint8_t>(std::clamp(
              static_cast<int>(std::lround(normalized * 127.0F)), 1, 127));
          cue_runtime_->note_on(event.note, velocity, event.channel);
        }
        break;
      }

      case runtime::HostEventType::note_off:
        cue_runtime_->note_off(event.note, event.channel);
        break;

      case runtime::HostEventType::all_notes_off:
        cue_runtime_->all_notes_off();
        break;

      case runtime::HostEventType::transport_started:
        cue_runtime_->transport_start();
        break;

      case runtime::HostEventType::transport_stopped:
        cue_runtime_->transport_stop();
        break;

      case runtime::HostEventType::transport_seek:
        // Absolute host position is consumed through HostTransportMailbox and
        // converted to song-relative ticks by the DAW-session binding layer.
        // Never guess a song tick from this discrete marker.
        break;
    }

    apply_cue_runtime_state();
    rebuild();
    return;
  }

  // Diagnostic/manual fallback used only before a musical show is authored.
  switch (event.type) {
    case runtime::HostEventType::note_on:
      if (event.note >= 36 && event.note <= 43 && event.value > 0.0F) {
        active_executor_ = static_cast<int>(event.note) - 36;
        executor_velocity_ = clamp01(event.value);
      }
      break;

    case runtime::HostEventType::note_off:
      if (active_executor_ == static_cast<int>(event.note) - 36) {
        active_executor_ = -1;
        executor_velocity_ = 0.0F;
      }
      break;

    case runtime::HostEventType::all_notes_off:
    case runtime::HostEventType::transport_stopped:
      active_executor_ = -1;
      executor_velocity_ = 0.0F;
      break;

    case runtime::HostEventType::transport_started:
    case runtime::HostEventType::transport_seek:
      break;
  }

  rebuild();
}

void ApplicationModel::release_transients() {
  if (cue_runtime_.has_value()) {
    cue_runtime_->all_notes_off();
    apply_cue_runtime_state();
  } else {
    active_executor_ = -1;
    executor_velocity_ = 0.0F;
  }
  rebuild();
}

void ApplicationModel::mark_dirty() noexcept {
  project_dirty_ = true;
}

void ApplicationModel::rebuild_cue_runtime() {
  cue_runtime_.reset();
  cue_source_override_.reset();
  cue_look_id_.reset();
  cue_scene_blackout_ = false;
  active_scene_id_.clear();
  active_scene_name_.clear();
  active_scene_momentary_ = false;

  if (show_program_.songs.empty()) {
    active_song_index_ = 0U;
    return;
  }
  if (active_song_index_ >= show_program_.songs.size()) active_song_index_ = 0U;

  cue_runtime_.emplace(show_program_.songs[active_song_index_]);
  cue_runtime_->seek(0U);
  apply_cue_runtime_state();
}

void ApplicationModel::apply_cue_runtime_state() {
  cue_source_override_.reset();
  cue_look_id_.reset();
  cue_scene_blackout_ = false;
  active_scene_id_.clear();
  active_scene_name_.clear();
  active_scene_momentary_ = false;

  if (!cue_runtime_.has_value()) return;

  const show::SceneDefinition* scene = cue_runtime_->effective_scene();
  if (scene == nullptr) {
    // Once a musical show exists, absence of a resolved cue is not permission
    // to fall back to the editable preview Look. It is a deterministic safe
    // state before the first cue, after Stop, or outside song bounds.
    cue_scene_blackout_ = true;
    return;
  }

  active_scene_id_ = scene->scene_id;
  active_scene_name_ = scene->name;
  active_scene_momentary_ = scene->behavior == show::CueBehavior::momentary;
  if (scene->blackout) {
    cue_scene_blackout_ = true;
    return;
  }

  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        return candidate.look_id == scene->look_id;
      });
  if (look == project_.looks.end()) {
    cue_scene_blackout_ = true;
    return;
  }

  const auto source = source_from_name(look->source);
  if (!source.has_value()) {
    cue_scene_blackout_ = true;
    return;
  }
  cue_source_override_ = *source;
  cue_look_id_ = look->look_id;
}

void ApplicationModel::rebuild() {
  std::map<std::string, FixtureProfile> profiles;
  for (const auto& document : project_.fixture_profiles) {
    profiles.emplace(document.profile_id, make_runtime_profile(document));
  }

  const auto look = std::find_if(
      project_.looks.begin(), project_.looks.end(),
      [&](const project::LookDocument& candidate) {
        const std::string& id = cue_look_id_.has_value()
            ? *cue_look_id_
            : project_.visual.active_look_id;
        return candidate.look_id == id;
      });
  const project::LookDocument* effective_look =
      look == project_.looks.end() ? nullptr : &*look;
  const VisualSource look_source = effective_look == nullptr
      ? authored_source_
      : source_from_name(effective_look->source).value_or(authored_source_);
  const VisualSource source = effective_source(look_source, active_executor_);
  const RgbColor primary = effective_look == nullptr
      ? RgbColor{0.92F, 0.03F, 0.07F}
      : RgbColor{effective_look->primary_color[0],
                 effective_look->primary_color[1],
                 effective_look->primary_color[2]};
  const RgbColor secondary = effective_look == nullptr
      ? RgbColor{1.0F, 0.42F, 0.04F}
      : RgbColor{effective_look->secondary_color[0],
                 effective_look->secondary_color[1],
                 effective_look->secondary_color[2]};
  const bool blackout = safety_.blackout() || cue_scene_blackout_;

  std::vector<PatchedFixture> patched;
  patched.reserve(project_.fixtures.size());
  snapshot_.warnings.clear();

  for (auto& fixture : snapshot_.fixtures) fixture = {};

  const std::size_t fixture_count =
      std::min(project_.fixtures.size(), snapshot_.fixtures.size());
  for (std::size_t index = 0; index < fixture_count; ++index) {
    const auto& document = project_.fixtures[index];
    const auto profile_it = profiles.find(document.fixture_profile_id);
    if (profile_it == profiles.end()) {
      snapshot_.warnings.push_back(
          "Missing runtime profile for fixture '" + document.logical_fixture_id + "'");
      continue;
    }

    RgbColor sampled = sample_source(source, primary, secondary,
                                     document.x, document.y, phase_);
    ColorTransformSettings settings = color_settings_;
    if (effective_look != nullptr) {
      settings.intensity *= effective_look->intensity;
      settings.white_extraction = effective_look->white_extraction;
      settings.amber_extraction = effective_look->amber_extraction;
      settings.uv_manual = effective_look->uv_manual;
    }
    if (active_executor_ >= 0) {
      settings.intensity *= std::max(0.05F, executor_velocity_);
    }

    if (active_executor_ == 5) {
      sampled = {1.0F, 1.0F, 1.0F};
      settings.white_extraction = 1.0F;
    } else if (active_executor_ == 6) {
      sampled = {0.03F, 0.0F, 0.08F};
      settings.uv_manual = std::max(settings.uv_manual, executor_velocity_);
    }

    AttributeFrame semantic = transform_rgb(sampled, settings);
    set(semantic, Attribute::Shutter, blackout ? 0.0F : 1.0F);
    set(semantic, Attribute::Strobe,
        !blackout && active_executor_ == 7 ? executor_velocity_ : 0.0F);
    if (blackout) semantic.fill(0.0F);

    const bool active = document.enabled &&
        (effective_look == nullptr ||
         effective_look->fixture_mask[index]);
    patched.push_back({document.logical_fixture_id, document.address, active,
                       profile_it->second, semantic});

    auto& fixture = snapshot_.fixtures[index];
    fixture.logical_id = document.logical_fixture_id;
    fixture.x = document.x;
    fixture.y = document.y;
    fixture.active = active;
    fixture.address = document.address;
    fixture.sampled_rgb = sampled;
    fixture.semantic = semantic;
  }

  const CompileResult compiled = compile_dmx(patched);
  snapshot_.warnings.insert(snapshot_.warnings.end(), compiled.warnings.begin(),
                            compiled.warnings.end());

  ++snapshot_.generation;
  snapshot_.project_id = project_.project_id;
  snapshot_.project_name = project_.name;
  snapshot_.project_valid = safety_.project_valid();
  snapshot_.project_dirty = project_dirty_;
  snapshot_.performance_ready = performance_ready_;
  snapshot_.backend_ready = safety_.backend_ready();
  snapshot_.output_armed = safety_.output_armed();
  snapshot_.global_blackout = safety_.blackout();
  snapshot_.blackout = blackout;
  snapshot_.rig14 = rig14_;
  snapshot_.song_count = show_program_.songs.size();
  snapshot_.active_song_index = active_song_index_;
  snapshot_.active_song_id.clear();
  snapshot_.active_song_name.clear();
  if (!show_program_.songs.empty() && active_song_index_ < show_program_.songs.size()) {
    snapshot_.active_song_id = show_program_.songs[active_song_index_].song_id;
    snapshot_.active_song_name = show_program_.songs[active_song_index_].name;
  }
  snapshot_.active_scene_id = active_scene_id_;
  snapshot_.active_scene_name = active_scene_name_;
  snapshot_.active_scene_momentary = active_scene_momentary_;
  snapshot_.output_universe = project_.output.universe;
  snapshot_.active_executor = active_executor_;
  snapshot_.executor_velocity = executor_velocity_;
  snapshot_.grand_master = color_settings_.intensity;
  snapshot_.phase = phase_;
  snapshot_.source = source;
  snapshot_.dmx = compiled.universe;
}

}  // namespace aeyla::product
