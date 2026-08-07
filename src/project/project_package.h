#pragma once

#include "project/project_document.h"
#include "show/show_program.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace aeyla::project {

struct ProjectPackageDiagnostic {
  std::string operation;
  std::string entry;
  std::string message;
  bool operator==(const ProjectPackageDiagnostic&) const = default;
};

struct ProjectPackageLoadResult {
  std::filesystem::path source;
  std::optional<ProjectDocument> document;
  std::optional<show::ShowProgram> show_program;
  bool legacy_project_only{false};
  std::vector<ProjectPackageDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return document.has_value() && show_program.has_value() &&
           diagnostics.empty();
  }
};

struct ProjectPackageSaveResult {
  bool saved{false};
  std::filesystem::path target;
  std::filesystem::path backup;
  std::vector<ProjectPackageDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return saved && diagnostics.empty();
  }
};

// `.aeylashow` package contract:
// - current format: deterministic root entries `project.json` + `show.bin`;
// - legacy Alpha 0.3 `project.json`-only packages remain readable and migrate to
//   an empty authoring ShowProgram;
// - unknown entries, ZIP64, encryption and asset payloads are rejected;
// - project and show are validated together before either is published.
ProjectPackageLoadResult load_project_package(
    const std::filesystem::path& source);

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program);

// Compatibility overload for code/tests that have not authored songs yet.
// Saving still writes the current two-entry format with an empty `show.bin`.
ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document);

}  // namespace aeyla::project
