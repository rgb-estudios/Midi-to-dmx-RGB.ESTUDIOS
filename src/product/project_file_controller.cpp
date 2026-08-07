#include "product/project_file_controller.h"

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
  model_.set_project_name("Untitled AEYLA Show");
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
  return save_as(current_path_, std::move(timestamp_utc));
}

ProjectFileStatus ProjectFileController::save_as(
    const std::filesystem::path& path,
    std::string timestamp_utc) {
  const auto document = model_.project_document_for_save(timestamp_utc);
  const auto saved = project::save_project_package_atomic(path, document);
  if (!saved.ok()) {
    return publish_failure(ProjectFileOperation::save_as,
                           "Could not save AEYLA project package",
                           flatten(saved.diagnostics));
  }

  current_path_ = path;
  model_.mark_project_saved(std::move(timestamp_utc));
  return publish_success(ProjectFileOperation::save_as,
                         "Project package saved and verified");
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
