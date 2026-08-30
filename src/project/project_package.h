#pragma once

#include "project/live_memory_state.h"
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
  LiveMemoryPersistentState live_memory_state{};
  bool legacy_project_only{false};
  bool legacy_without_live_memory{false};
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
// - R10.1 current format: deterministic root entries
//   `project.json` + `show.bin` + `live.bin`;
// - previous two-entry project+show packages remain readable and restore an
//   empty/OFF live-memory state;
// - legacy Alpha 0.3 `project.json`-only packages remain readable and migrate to
//   an empty authoring ShowProgram + empty/OFF live-memory state;
// - unknown entries, ZIP64, encryption and asset payloads are rejected;
// - project, show and live-memory state are validated together before publish;
// - live.bin contains configuration only. Runtime level/target, Learn state,
//   LTP serial and physical output ARM are never persisted.
ProjectPackageLoadResult load_project_package(
    const std::filesystem::path& source);

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state);

// Compatibility overloads write the current three-entry package with an empty
// live-memory state so all newly saved projects migrate forward deterministically.
ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program);

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document);

}  // namespace aeyla::project
