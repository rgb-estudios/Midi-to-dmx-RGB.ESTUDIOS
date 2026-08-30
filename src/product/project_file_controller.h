#pragma once

#include "product/application_model.h"
#include "project/project_package.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace aeyla::product {

enum class ProjectFileOperation : std::uint8_t {
  none,
  new_project,
  open,
  save,
  save_as
};

struct ProjectFileStatus {
  ProjectFileOperation operation{ProjectFileOperation::none};
  bool succeeded{false};
  std::filesystem::path current_path;
  std::string message;
  std::vector<std::string> diagnostics;
};

class ProjectFileController final {
 public:
  explicit ProjectFileController(ApplicationModel& model) : model_(model) {}

  ProjectFileStatus new_project(std::string project_uuid,
                                std::string timestamp_utc);
  ProjectFileStatus open(const std::filesystem::path& path);
  ProjectFileStatus save(std::string timestamp_utc);
  ProjectFileStatus save_as(const std::filesystem::path& path,
                            std::string timestamp_utc);

  [[nodiscard]] const std::filesystem::path& current_path() const noexcept {
    return current_path_;
  }

  [[nodiscard]] const ProjectFileStatus& status() const noexcept {
    return status_;
  }

  [[nodiscard]] const project::LiveMemoryPersistentState& live_memory_state()
      const noexcept {
    return live_memory_state_;
  }

  void set_live_memory_state(project::LiveMemoryPersistentState state) {
    live_memory_state_ = std::move(state);
  }

 private:
  ProjectFileStatus save_to(const std::filesystem::path& path,
                            std::string timestamp_utc,
                            ProjectFileOperation operation);
  ProjectFileStatus publish_failure(ProjectFileOperation operation,
                                    std::string message,
                                    std::vector<std::string> diagnostics = {});
  ProjectFileStatus publish_success(ProjectFileOperation operation,
                                    std::string message);

  ApplicationModel& model_;
  std::filesystem::path current_path_;
  ProjectFileStatus status_{};
  project::LiveMemoryPersistentState live_memory_state_{};
};

}  // namespace aeyla::product
