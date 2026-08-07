#pragma once

#include "project/project_document.h"

#include <filesystem>
#include <string>
#include <vector>

namespace aeyla::project {

struct ProjectFileDiagnostic {
  std::string operation;
  std::string message;
  bool operator==(const ProjectFileDiagnostic&) const = default;
};

struct ProjectSaveResult {
  bool saved{false};
  std::filesystem::path target;
  std::filesystem::path backup;
  std::vector<ProjectFileDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return saved && diagnostics.empty();
  }
};

struct ProjectLoadResult {
  ProjectParseResult parsed;
  std::filesystem::path source;
  std::vector<ProjectFileDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return diagnostics.empty() && parsed.ok();
  }
};

ProjectSaveResult save_project_state_json_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document);

ProjectLoadResult load_project_state_json(
    const std::filesystem::path& source);

}  // namespace aeyla::project
