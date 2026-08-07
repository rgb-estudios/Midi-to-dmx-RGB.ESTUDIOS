#pragma once

#include "core/attributes.h"
#include "core/color_transform.h"
#include "core/dmx_compiler.h"
#include "project/project_document.h"
#include "runtime/host_event.h"
#include "runtime/runtime_safety_state.h"

#include <array>
#include <cstdint>
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
  bool backend_ready{false};
  bool output_armed{false};
  bool blackout{true};
  bool rig14{false};
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

// Product-owned document/runtime state shared by standalone and VST3 adapters.
// It performs no file, socket, USB, media or UI work and must never be called
// directly from an audio callback. Host callbacks submit compact HostEvent
// values to a bounded ingress; the non-realtime product thread drains them into
// this model.
class ApplicationModel final {
 public:
  ApplicationModel();

  project::ProjectValidation load_project_document(
      const project::ProjectDocument& document);

  [[nodiscard]] const project::ProjectDocument& project_document() const noexcept {
    return project_;
  }

  void set_project_valid(bool valid);
  void set_backend_ready(bool ready);
  [[nodiscard]] bool request_arm();
  void disarm(runtime::RuntimeSafetyReason reason =
                  runtime::RuntimeSafetyReason::operator_disarm);
  void set_blackout(bool enabled);

  void set_grand_master(float value);
  void set_rig14(bool enabled);
  void set_visual_source(VisualSource source);
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
  void rebuild();

  runtime::RuntimeSafetyState safety_{};
  project::ProjectDocument project_{};
  ApplicationSnapshot snapshot_{};
  ColorTransformSettings color_settings_{};
  VisualSource authored_source_{VisualSource::gradient};
  bool rig14_{false};
  float phase_{0.0F};
  float executor_velocity_{0.0F};
  int active_executor_{-1};
};

}  // namespace aeyla::product
