#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aeyla::project {

inline constexpr std::size_t kMaximumLooks = 2048U;

struct SchemaVersion {
  std::uint16_t major{2};
  std::uint16_t minor{0};
  bool operator==(const SchemaVersion&) const = default;
};

struct ChannelDocument {
  std::uint16_t slot{1};
  std::string attribute;
  bool operator==(const ChannelDocument&) const = default;
};

struct FixtureProfileDocument {
  std::string profile_id;
  std::string display_name;
  std::string manufacturer;
  std::string model;
  std::string mode_name;
  std::uint16_t footprint{1};
  std::vector<ChannelDocument> channels;
  std::vector<std::string> tags;
  bool operator==(const FixtureProfileDocument&) const = default;
};

struct FixtureDocument {
  std::string logical_fixture_id;
  std::string fixture_profile_id;
  std::uint16_t universe{0};
  std::uint16_t address{1};
  bool enabled{true};
  float x{0.0F};
  float y{0.0F};
  bool operator==(const FixtureDocument&) const = default;
};

struct LookDocument {
  std::string look_id;
  std::string name;
  std::string source{"gradient"};
  std::array<float, 3> primary_color{0.92F, 0.03F, 0.07F};
  std::array<float, 3> secondary_color{1.0F, 0.42F, 0.04F};
  float intensity{1.0F};
  float speed{0.35F};
  float white_extraction{0.20F};
  float amber_extraction{0.15F};
  float uv_manual{0.0F};
  std::array<bool, 14> fixture_mask{
      true, true, true, true, true, true, true,
      true, true, true, true, true, true, true};
  bool operator==(const LookDocument&) const = default;
};

struct VisualDocument {
  std::string active_look_id;
  float speed{0.35F};
  float white_extraction{0.20F};
  float amber_extraction{0.15F};
  float uv_manual{0.0F};
  bool operator==(const VisualDocument&) const = default;
};

struct MidiMappingDocument {
  std::uint8_t note{36};
  std::uint8_t executor{0};
  bool operator==(const MidiMappingDocument&) const = default;
};

struct OutputDocument {
  std::string backend{"none"};
  std::string target;
  std::uint16_t universe{0};
  bool armed{false};
  bool operator==(const OutputDocument&) const = default;
};

struct UiStateDocument {
  std::string selected_fixture_id;
  std::string active_panel{"editor"};
  bool operator==(const UiStateDocument&) const = default;
};

struct AssetDocument {
  std::string path;
  std::string sha256;
  bool operator==(const AssetDocument&) const = default;
};

struct ProjectDocument {
  SchemaVersion schema_version{};
  std::string app_version{"0.3.0-alpha.2"};
  std::string project_id;
  std::string created_at;
  std::string modified_at;
  std::string name{"Untitled Show"};
  std::vector<FixtureProfileDocument> fixture_profiles;
  std::vector<FixtureDocument> fixtures;
  std::vector<LookDocument> looks;
  VisualDocument visual{};
  std::vector<MidiMappingDocument> midi_mappings;
  OutputDocument output{};
  std::vector<std::string> recent_files;
  UiStateDocument ui_state{};
  std::vector<AssetDocument> assets;
  std::map<std::string, std::string> checksums;
  bool operator==(const ProjectDocument&) const = default;
};

enum class DiagnosticSeverity : std::uint8_t { warning, error };

struct ProjectDiagnostic {
  DiagnosticSeverity severity{DiagnosticSeverity::error};
  std::string path;
  std::string message;
  bool operator==(const ProjectDiagnostic&) const = default;
};

struct ProjectValidation {
  std::vector<ProjectDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept;
  [[nodiscard]] std::size_t error_count() const noexcept;
  [[nodiscard]] std::size_t warning_count() const noexcept;
};

struct ProjectParseResult {
  std::optional<ProjectDocument> document;
  ProjectValidation validation;

  [[nodiscard]] bool ok() const noexcept {
    return document.has_value() && validation.ok();
  }
};

ProjectDocument make_default_project_document(std::string project_id,
                                              std::string timestamp_utc);
ProjectValidation validate_project_document(const ProjectDocument& document);
std::string serialize_project_document(const ProjectDocument& document);
ProjectParseResult deserialize_project_document(std::string_view json);

[[nodiscard]] bool is_safe_package_path(std::string_view path) noexcept;
[[nodiscard]] bool is_canonical_uuid(std::string_view value) noexcept;

}  // namespace aeyla::project
