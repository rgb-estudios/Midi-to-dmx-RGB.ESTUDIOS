#pragma once

#include "project/live_memory_state.h"
#include "project/project_document.h"
#include "show/show_program.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
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
  // Opaque, bounded plug-in/session state. The project layer does not interpret
  // this payload; the product plug-in validates it with runtime/plugin_state.
  std::vector<std::uint8_t> portable_session_state{};
  bool legacy_project_only{false};
  bool legacy_without_live_memory{false};
  bool legacy_without_portable_session{false};
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
// - R10.10 current format: deterministic root entries
//   `project.json` + `show.bin` + `live.bin` + `session.bin`;
// - R10.1-R10.9 three-entry packages remain readable; they simply have no
//   portable host/take binding payload;
// - previous two-entry project+show packages remain readable and restore an
//   empty/OFF live-memory state;
// - legacy Alpha 0.3 `project.json`-only packages remain readable and migrate to
//   an empty authoring ShowProgram + empty/OFF live-memory state;
// - unknown entries, ZIP64, encryption and asset payloads are rejected;
// - project, show and live-memory state are validated together before publish;
// - session.bin is bounded to 64 KiB and contains no DMX frames. Physical ARM
//   is not part of the plug-in component state and all restore paths stay safe.
ProjectPackageLoadResult load_project_package(
    const std::filesystem::path& source);

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state,
    std::span<const std::uint8_t> portable_session_state);

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state);

// Compatibility overloads retain the older three-entry package when no
// portable session payload is supplied.
ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program);

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document);

}  // namespace aeyla::project
