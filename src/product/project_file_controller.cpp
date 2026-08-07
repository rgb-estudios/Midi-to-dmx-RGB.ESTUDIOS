#include "product/project_file_controller.h"

#include <algorithm>
#include <utility>

namespace aeyla::product {
namespace {

std::vector<std::string> flatten(
    const std::vector<project::ProjectPackageDiagnostic>& diagnostics) {
  std::vector<std::string> result;
  result.reserve(diagnostics.size());
  for (const auto& diagnostic : diagnostics) {
    std::string line = diagnostic.operation;
    if (!diagnostic.entry.empty()) line += " [" + diagnostic.entry + "]";
    if (!diagnostic.message.empty()) line += ": " + diagnostic.message;
    result.push_back(std::move(line));
  }
  return result;
}

std::vector<std::string> flatten(
    const project::ProjectValidation& validation) {
  std::vector<std::string> result;
  result.reserve(validation.diagnostics.size());
  for (const auto& diagnostic : validation.diagnostics) {
    std::string line = diagnostic.path;
    if (!diagnostic.message.empty()) line += ": " + diagnostic.message;
    result.push_back(std::move(line));
  }
  return result;
}

bool supported_alpha03_rig_mode(const project::ProjectDocument& document) {
  if (document.fixtures.size() != 14U) return false;

  bool rig10 = true;
  bool rig14 = true;
  for (std::size_t index = 0U; index < document.fixtures.size(); ++index) {
    const bool enabled = document.fixtures[index].enabled;
    rig10 = rig10 && (enabled == (index < 10U));
    rig14 = rig14 && enabled;
  }
  return rig10 || rig14;
}

}  // namespace

ProjectFileStatus ProjectFileController::new_project(
    std::string project_uuid,
    std::string timestamp_utc) {
  auto document = project::make_default_project_document(
      std::move(project_uuid), timestamp_utc);
  const auto validation = model_.load_project_document(document);
  if (!validation.ok()) {
    return publish_failure(ProjectFileOperation::new_project,
                           "Could not create a valid AEYLA project",
                           flatten(validation));
  }

  current_path_.clear();
  model_.mark_project_unsaved();
  return publish_success(ProjectFileOperation::new_project,
                         "New project created in blackout and disarmed");
}

ProjectFileStatus ProjectFileController::open(
    const std::filesystem::path& path) {
  const auto loaded = project::load_project_package(path);
  if (!loaded.ok() || !loaded.document.has_value()) {
    return publish_failure(ProjectFileOperation::open,
                           "Could not open AEYLA project package",
                           flatten(loaded.diagnostics));
  }

  if (!supported_alpha03_rig_mode(*loaded.document)) {
    return publish_failure(
        ProjectFileOperation::open,
        "Project package is incompatible with this runtime",
        {"rig.fixtures: Alpha 0.3 accepts only the canonical Rig 10 or Rig 14 activation pattern"});
  }

  // Preflight the document in an isolated runtime. A package may be valid JSON
  // and ZIP while violating a product runtime constraint such as Alpha 0.3's
  // single-universe scope. Failed Open must not invalidate the current show.
  ApplicationModel candidate;
  const auto preflight = candidate.load_project_document(*loaded.document);
  if (!preflight.ok()) {
    return publish_failure(ProjectFileOperation::open,
                           "Project package is incompatible with this runtime",
                           flatten(preflight));
  }

  const auto validation = model_.load_project_document(*loaded.document);
  if (!validation.ok()) {
    return publish_failure(ProjectFileOperation::open,
                           "Project package could not be published to the runtime",
                           flatten(validation));
  }

  current_path_ = path;
  return publish_success(ProjectFileOperation::open,
                         "Project opened in blackout and disarmed");
}

ProjectFileStatus ProjectFileController::save(std::string timestamp_utc) {
  if (current_path_.empty()) {
    return publish_failure(ProjectFileOperation::save,
                           "Save requires a path; use Save As");
  }
  return save_to(current_path_, std::move(timestamp_utc),
                 ProjectFileOperation::save);
}

ProjectFileStatus ProjectFileController::save_as(
    const std::filesystem::path& path,
    std::string timestamp_utc) {
  return save_to(path, std::move(timestamp_utc),
                 ProjectFileOperation::save_as);
}

ProjectFileStatus ProjectFileController::save_to(
    const std::filesystem::path& path,
    std::string timestamp_utc,
    ProjectFileOperation operation) {
  const auto document = model_.project_document_for_save(timestamp_utc);
  const auto saved = project::save_project_package_atomic(path, document);
  if (!saved.ok()) {
    return publish_failure(operation,
                           "Could not save AEYLA project package",
                           flatten(saved.diagnostics));
  }

  current_path_ = path;
  model_.mark_project_saved(std::move(timestamp_utc));
  return publish_success(operation,
                         operation == ProjectFileOperation::save
                             ? "Project package saved and verified"
                             : "Project package saved to a new path and verified");
}

ProjectFileStatus ProjectFileController::publish_failure(
    ProjectFileOperation operation,
    std::string message,
    std::vector<std::string> diagnostics) {
  status_.operation = operation;
  status_.succeeded = false;
  status_.current_path = current_path_;
  status_.message = std::move(message);
  status_.diagnostics = std::move(diagnostics);
  return status_;
}

ProjectFileStatus ProjectFileController::publish_success(
    ProjectFileOperation operation,
    std::string message) {
  status_.operation = operation;
  status_.succeeded = true;
  status_.current_path = current_path_;
  status_.message = std::move(message);
  status_.diagnostics.clear();
  return status_;
}

}  // namespace aeyla::product
