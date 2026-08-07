#include "product/application_model.h"

#include "core/fixture_profile.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
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

RgbColor sample_source(VisualSource source, float x, float y, float phase) {
  const float nx = clamp01(x);
  const float ny = clamp01(y);
  const float p = phase - std::floor(phase);

  switch (source) {
    case VisualSource::solid:
      return {0.85F, 0.02F, 0.04F};

    case VisualSource::gradient:
      if (nx < 0.5F) {
        return mix({0.03F, 0.08F, 0.30F}, {0.92F, 0.03F, 0.07F}, nx * 2.0F);
      }
      return mix({0.92F, 0.03F, 0.07F}, {1.0F, 0.42F, 0.04F}, (nx - 0.5F) * 2.0F);

    case VisualSource::wave: {
      const float value = 0.5F + 0.5F * std::sin((nx * 4.0F + p * 2.0F) * kPi);
      return mix({0.01F, 0.06F, 0.18F}, {0.05F, 0.78F, 1.0F}, value);
    }

    case VisualSource::noise: {
      const auto xi = static_cast<std::uint32_t>(nx * 1024.0F);
      const auto yi = static_cast<std::uint32_t>(ny * 1024.0F);
      const auto pi = static_cast<std::uint32_t>(p * 120.0F);
      std::uint32_t seed = xi * 73856093U ^ yi * 19349663U ^ pi * 83492791U;
      seed ^= seed >> 13U;
      seed *= 1274126177U;
      const float value = static_cast<float>((seed >> 8U) & 255U) / 255.0F;
      return mix({0.03F, 0.01F, 0.05F}, {0.90F, 0.02F, 0.12F}, value);
    }

    case VisualSource::chase: {
      const float raw_distance = std::fabs(nx - p);
      const float wrapped_distance = std::min(raw_distance, 1.0F - raw_distance);
      const float value = clamp01(1.0F - wrapped_distance * 7.0F);
      return mix({0.01F, 0.01F, 0.02F}, {1.0F, 0.03F, 0.06F}, value);
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

}  // namespace

ApplicationModel::ApplicationModel()
    : project_(project::make_default_project_document(
          kDevelopmentProjectId, kDevelopmentTimestamp)) {
  color_settings_.intensity = 1.0F;
  color_settings_.white_extraction = project_.visual.white_extraction;
  color_settings_.amber_extraction = project_.visual.amber_extraction;
  color_settings_.lime_extraction = 0.20F;
  color_settings_.uv_manual = project_.visual.uv_manual;

  safety_.set_project_valid(project::validate_project_document(project_).ok());
  // A simulated/null backend must not satisfy the real output-arm gate.
  safety_.set_backend_ready(false);
  rebuild();
}

project::ProjectValidation ApplicationModel::load_project_document(
    const project::ProjectDocument& document) {
  safety_.begin_project_reload();
  active_executor_ = -1;
  executor_velocity_ = 0.0F;

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

  auto active_look = std::find_if(
      document.looks.begin(), document.looks.end(),
      [&](const project::LookDocument& look) {
        return look.look_id == document.visual.active_look_id;
      });
  if (active_look == document.looks.end() ||
      !source_from_name(active_look->source).has_value()) {
    add_runtime_error(validation, "visual.activeLookId",
                      "active look cannot be converted to a runtime source");
  }

  if (!validation.ok()) {
    safety_.complete_project_reload(false);
    rebuild();
    return validation;
  }

  project_ = document;
  project_.output.armed = false;
  authored_source_ = *source_from_name(active_look->source);
  color_settings_.white_extraction = project_.visual.white_extraction;
  color_settings_.amber_extraction = project_.visual.amber_extraction;
  color_settings_.uv_manual = project_.visual.uv_manual;
  rig14_ = false;

  safety_.complete_project_reload(true);
  rebuild();
  return validation;
}

void ApplicationModel::set_project_valid(bool valid) {
  safety_.set_project_valid(valid);
  rebuild();
}

void ApplicationModel::set_backend_ready(bool ready) {
  safety_.set_backend_ready(ready);
  rebuild();
}

bool ApplicationModel::request_arm() {
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
  rig14_ = enabled;
  rebuild();
}

void ApplicationModel::set_visual_source(VisualSource source) {
  authored_source_ = source;
  rebuild();
}

void ApplicationModel::set_phase(float normalized_phase) {
  phase_ = normalized_phase - std::floor(normalized_phase);
  rebuild();
}

void ApplicationModel::set_white_extraction(float value) {
  color_settings_.white_extraction = clamp01(value);
  rebuild();
}

void ApplicationModel::set_amber_extraction(float value) {
  color_settings_.amber_extraction = clamp01(value);
  rebuild();
}

void ApplicationModel::set_uv_manual(float value) {
  color_settings_.uv_manual = clamp01(value);
  rebuild();
}

void ApplicationModel::handle_host_event(const runtime::HostEvent& event) {
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
  active_executor_ = -1;
  executor_velocity_ = 0.0F;
  rebuild();
}

void ApplicationModel::rebuild() {
  std::map<std::string, FixtureProfile> profiles;
  for (const auto& document : project_.fixture_profiles) {
    profiles.emplace(document.profile_id, make_runtime_profile(document));
  }

  const VisualSource source = effective_source(authored_source_, active_executor_);
  const bool blackout = safety_.blackout();

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

    RgbColor sampled = sample_source(source, document.x, document.y, phase_);
    ColorTransformSettings settings = color_settings_;
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

    const bool active = rig14_ ? true : document.enabled;
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
  snapshot_.backend_ready = safety_.backend_ready();
  snapshot_.output_armed = safety_.output_armed();
  snapshot_.blackout = blackout;
  snapshot_.rig14 = rig14_;
  snapshot_.output_universe = project_.output.universe;
  snapshot_.active_executor = active_executor_;
  snapshot_.executor_velocity = executor_velocity_;
  snapshot_.grand_master = color_settings_.intensity;
  snapshot_.phase = phase_;
  snapshot_.source = source;
  snapshot_.dmx = compiled.universe;
}

}  // namespace aeyla::product
