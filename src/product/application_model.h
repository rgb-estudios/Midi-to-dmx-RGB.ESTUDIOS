#pragma once

#include "core/attributes.h"
#include "core/color_transform.h"
#include "core/dmx_compiler.h"
#include "project/project_document.h"
#include "runtime/host_event.h"
#include "runtime/runtime_safety_state.h"
#include "show/show_program.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aeyla::product {

enum class VisualSource : std::uint8_t {
  solid = 0,
  gradient,
  wave,
  noise,
  chase
};

struct FixtureSnapshot {
  std::string logical_id;
  float x{0.0F};
  float y{0.0F};
  bool active{false};
  std::uint16_t address{1};
  RgbColor sampled_rgb{};
  AttributeFrame semantic{};
};

struct ApplicationSnapshot {
  std::uint64_t generation{0};
  std::string project_id;
  std::string project_name;
  bool project_valid{false};
  bool project_dirty{false};
  bool performance_ready{false};
  bool backend_ready{false};
  bool output_armed{false};
  bool blackout{true};
  bool rig14{false};
  std::size_t song_count{0U};
  std::size_t active_song_index{0U};
  std::string active_song_id;
  std::string active_song_name;
  std::string active_scene_id;
  std::string active_scene_name;
  bool active_scene_momentary{false};
  std::uint16_t output_universe{0};
  int active_executor{-1};
  float executor_velocity{0.0F};
  float grand_master{1.0F};
  float phase{0.0F};
  VisualSource source{VisualSource::gradient};
  std::array<FixtureSnapshot, 14> fixtures{};
  DmxUniverse dmx{};
  std::vector<std::string> warnings;
};

struct AuthoringResult {
  bool succeeded{false};
  std::string object_id;
  std::string message;
};

class ApplicationModel final {
 public:
  ApplicationModel();

  // Transactional project+show publication. Validation is pure: on failure the
  // currently loaded valid runtime remains untouched. On success the complete
  // bundle is published disarmed + blackout as one state transition.
  project::ProjectValidation load_project_bundle(
      const project::ProjectDocument& document,
      const show::ShowProgram& show_program);

  // Compatibility path for project-only callers/new projects. It intentionally
  // publishes an empty authoring show, which is saveable but not show-ready.
  project::ProjectValidation load_project_document(
      const project::ProjectDocument& document);

  [[nodiscard]] const project::ProjectDocument& project_document() const noexcept {
    return project_;
  }

  [[nodiscard]] const show::ShowProgram& show_program() const noexcept {
    return show_program_;
  }

  [[nodiscard]] project::ProjectDocument project_document_for_save(
      std::string modified_at) const;
  [[nodiscard]] show::ShowProgram show_program_for_save() const {
    return show_program_;
  }

  // Authoring replacement validates against the current project's Look IDs.
  // A successful replacement forces output safe because show semantics changed.
  show::ShowValidation replace_show_program(const show::ShowProgram& program);
  [[nodiscard]] show::ShowValidation show_performance_validation() const;
  [[nodiscard]] AuthoringResult store_current_look();
  [[nodiscard]] AuthoringResult create_song();
  [[nodiscard]] AuthoringResult store_cue_at_tick(
      std::uint64_t tick,
      show::CueBehavior behavior = show::CueBehavior::latch);
  [[nodiscard]] bool select_look(std::size_t look_index);
  [[nodiscard]] bool toggle_active_look_fixture(std::size_t fixture_index);
  [[nodiscard]] bool active_look_fixture_enabled(
      std::size_t fixture_index) const noexcept;
  [[nodiscard]] bool set_active_look_color(bool secondary,
                                           const RgbColor& color);
  [[nodiscard]] std::array<float, 3> active_look_color(
      bool secondary) const noexcept;
  [[nodiscard]] bool set_active_look_intensity(float intensity);
  [[nodiscard]] float active_look_intensity() const noexcept;

  // Active song is runtime/session state, not authored show data. Switching it
  // never mutates `.aeylashow` but always forces output safe before playback.
  [[nodiscard]] bool select_song(std::size_t song_index);
  void seek_active_song_tick(std::uint64_t tick);
  void advance_active_song_tick(std::uint64_t tick);

  void mark_project_saved(std::string modified_at);
  void mark_project_unsaved() {
    project_dirty_ = true;
    rebuild();
  }

  void set_project_valid(bool valid);
  void set_project_name(std::string name);
  void set_backend_ready(bool ready);
  [[nodiscard]] bool request_arm();
  void disarm(runtime::RuntimeSafetyReason reason =
                  runtime::RuntimeSafetyReason::operator_disarm);
  void set_blackout(bool enabled);

  void set_grand_master(float value);
  void set_rig14(bool enabled);
  void set_visual_source(VisualSource source);
  void set_visual_speed(float value);
  void set_phase(float normalized_phase);
  void set_white_extraction(float value);
  void set_amber_extraction(float value);
  void set_uv_manual(float value);

  void handle_host_event(const runtime::HostEvent& event);
  void release_transients();

  [[nodiscard]] const ApplicationSnapshot& snapshot() const noexcept {
    return snapshot_;
  }

 private:
  void mark_dirty() noexcept;
  void rebuild_cue_runtime();
  void apply_cue_runtime_state();
  void rebuild();

  runtime::RuntimeSafetyState safety_{};
  project::ProjectDocument project_{};
  show::ShowProgram show_program_{};
  std::optional<show::CueRuntime> cue_runtime_{};
  ApplicationSnapshot snapshot_{};
  ColorTransformSettings color_settings_{};
  VisualSource authored_source_{VisualSource::gradient};
  std::optional<VisualSource> cue_source_override_{};
  std::optional<std::string> cue_look_id_{};
  bool cue_scene_blackout_{false};
  bool rig14_{false};
  bool project_dirty_{true};
  bool performance_ready_{false};
  float phase_{0.0F};
  float executor_velocity_{0.0F};
  int active_executor_{-1};
  std::size_t active_song_index_{0U};
  std::string active_scene_id_;
  std::string active_scene_name_;
  bool active_scene_momentary_{false};
};

}  // namespace aeyla::product
