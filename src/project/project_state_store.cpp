#include "project/project_state_store.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace aeyla::project {
namespace {

constexpr std::uintmax_t kMaximumStateBytes = 4U * 1024U * 1024U;

void add(std::vector<ProjectFileDiagnostic>& diagnostics,
         std::string operation, std::string message) {
  diagnostics.push_back({std::move(operation), std::move(message)});
}

std::filesystem::path temporary_path(const std::filesystem::path& target) {
  auto result = target;
  result += ".tmp";
  return result;
}

std::filesystem::path backup_path(const std::filesystem::path& target) {
  auto result = target;
  result += ".bak";
  return result;
}

FILE* open_binary_write(const std::filesystem::path& path) {
#ifdef _WIN32
  FILE* file = nullptr;
  if (_wfopen_s(&file, path.c_str(), L"wb") != 0) return nullptr;
  return file;
#else
  return std::fopen(path.c_str(), "wb");
#endif
}

bool sync_file(FILE* file) {
  if (std::fflush(file) != 0) return false;
#ifdef _WIN32
  return _commit(_fileno(file)) == 0;
#else
  return fsync(fileno(file)) == 0;
#endif
}

bool write_and_sync(const std::filesystem::path& path,
                    const std::string& data,
                    std::vector<ProjectFileDiagnostic>& diagnostics) {
  FILE* file = open_binary_write(path);
  if (file == nullptr) {
    add(diagnostics, "open-temp", "could not open temporary state file");
    return false;
  }

  const std::size_t written =
      std::fwrite(data.data(), 1U, data.size(), file);
  if (written != data.size()) {
    add(diagnostics, "write-temp", "short write while storing project state");
    std::fclose(file);
    return false;
  }

  if (!sync_file(file)) {
    add(diagnostics, "sync-temp", "could not flush project state to stable storage");
    std::fclose(file);
    return false;
  }

  if (std::fclose(file) != 0) {
    add(diagnostics, "close-temp", "could not close temporary project state");
    return false;
  }
  return true;
}

std::optional<std::string> read_bounded(
    const std::filesystem::path& path,
    std::vector<ProjectFileDiagnostic>& diagnostics) {
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(path, error);
  if (error) {
    add(diagnostics, "stat", "could not determine project-state size: " + error.message());
    return std::nullopt;
  }
  if (size > kMaximumStateBytes) {
    add(diagnostics, "read", "project state exceeds the 4 MiB limit");
    return std::nullopt;
  }

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    add(diagnostics, "open", "could not open project state for reading");
    return std::nullopt;
  }
  std::string contents;
  contents.reserve(static_cast<std::size_t>(size));
  contents.assign(std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>());
  if (!input.eof() && input.fail()) {
    add(diagnostics, "read", "could not read complete project state");
    return std::nullopt;
  }
  if (contents.size() != size) {
    add(diagnostics, "read", "project state changed while being read");
    return std::nullopt;
  }
  return contents;
}

void remove_if_present(const std::filesystem::path& path,
                       std::vector<ProjectFileDiagnostic>& diagnostics,
                       std::string operation) {
  std::error_code error;
  const bool removed = std::filesystem::remove(path, error);
  (void) removed;
  if (error) add(diagnostics, std::move(operation), error.message());
}

}  // namespace

ProjectLoadResult load_project_state_json(const std::filesystem::path& source) {
  ProjectLoadResult result;
  result.source = source;
  const auto contents = read_bounded(source, result.diagnostics);
  if (!contents.has_value()) return result;
  result.parsed = deserialize_project_document(*contents);
  if (!result.parsed.ok()) {
    add(result.diagnostics, "validate", "project state is malformed or semantically invalid");
  }
  return result;
}

ProjectSaveResult save_project_state_json_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document) {
  ProjectSaveResult result;
  result.target = target;
  result.backup = backup_path(target);

  const ProjectValidation validation = validate_project_document(document);
  if (!validation.ok()) {
    add(result.diagnostics, "validate", "refused to save an invalid project document");
    return result;
  }

  if (target.empty() || target.filename().empty()) {
    add(result.diagnostics, "target", "target project-state path is empty");
    return result;
  }

  std::error_code error;
  const auto parent = target.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, error);
    if (error) {
      add(result.diagnostics, "create-directory", error.message());
      return result;
    }
  }

  const std::filesystem::path temporary = temporary_path(target);
  remove_if_present(temporary, result.diagnostics, "remove-stale-temp");
  if (!result.diagnostics.empty()) return result;

  const std::string serialized = serialize_project_document(document);
  if (!write_and_sync(temporary, serialized, result.diagnostics)) {
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  const ProjectLoadResult verified = load_project_state_json(temporary);
  if (!verified.ok() || !verified.parsed.document.has_value() ||
      serialize_project_document(*verified.parsed.document) != serialized) {
    add(result.diagnostics, "verify-temp",
        "temporary project state failed deterministic read-back verification");
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  remove_if_present(result.backup, result.diagnostics, "remove-stale-backup");
  if (!result.diagnostics.empty()) {
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  const bool target_existed = std::filesystem::exists(target, error);
  if (error) {
    add(result.diagnostics, "inspect-target", error.message());
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  if (target_existed) {
    std::filesystem::rename(target, result.backup, error);
    if (error) {
      add(result.diagnostics, "backup-current", error.message());
      remove_if_present(temporary, result.diagnostics, "cleanup-temp");
      return result;
    }
  }

  std::filesystem::rename(temporary, target, error);
  if (error) {
    add(result.diagnostics, "replace-target", error.message());
    if (target_existed) {
      std::error_code restore_error;
      std::filesystem::rename(result.backup, target, restore_error);
      if (restore_error)
        add(result.diagnostics, "restore-backup", restore_error.message());
    }
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  result.saved = true;
  return result;
}

}  // namespace aeyla::project
