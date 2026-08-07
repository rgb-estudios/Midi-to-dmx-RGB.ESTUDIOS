#pragma once

#include "project/project_document.h"

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
  std::vector<ProjectPackageDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return document.has_value() && diagnostics.empty();
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

// Alpha 0.3 package slice: one deterministic project.json ZIP entry.
// Media/assets remain deliberately rejected until SHA-256 verification and
// bounded asset streaming are implemented.
ProjectPackageLoadResult load_project_package(
    const std::filesystem::path& source);

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document);

}  // namespace aeyla::project
