#pragma once

#include "core/attributes.h"
#include "core/color_transform.h"
#include "core/dmx_compiler.h"
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
  bool project_valid{false};
  bool backend_ready{false};
  bool output_armed{false};
  bool blackout{true};
  bool rig14{false};
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

  void set_project_valid(bool valid) noexcept;
  void set_backend_ready(bool ready) noexcept;
  [[nodiscard]] bool request_arm() noexcept;
  void disarm(runtime::RuntimeSafetyReason reason =
                  runtime::RuntimeSafetyReason::operator_disarm) noexcept;
  void set_blackout(bool enabled) noexcept;

  void set_grand_master(float value) noexcept;
  void set_rig14(bool enabled) noexcept;
  void set_visual_source(VisualSource source) noexcept;
  void set_phase(float normalized_phase) noexcept;
  void set_white_extraction(float value) noexcept;
  void set_amber_extraction(float value) noexcept;
  void set_uv_manual(float value) noexcept;

  void handle_host_event(const runtime::HostEvent& event) noexcept;
  void release_transients() noexcept;

  [[nodiscard]] const ApplicationSnapshot& snapshot() const noexcept {
    return snapshot_;
  }

 private:
  void rebuild() noexcept;

  runtime::RuntimeSafetyState safety_{};
  ApplicationSnapshot snapshot_{};
  ColorTransformSettings color_settings_{};
  VisualSource authored_source_{VisualSource::gradient};
  bool rig14_{false};
  float phase_{0.0F};
  float executor_velocity_{0.0F};
  int active_executor_{-1};
};

}  // namespace aeyla::product
