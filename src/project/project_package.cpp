#include "project/project_package.h"

#include "miniz.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace aeyla::project {
namespace {

constexpr std::uintmax_t kMaximumArchiveBytes = 8U * 1024U * 1024U;
constexpr std::uint64_t kMaximumProjectJsonBytes = 4U * 1024U * 1024U;
constexpr const char* kProjectEntry = "project.json";

void add(std::vector<ProjectPackageDiagnostic>& diagnostics,
         std::string operation, std::string entry, std::string message) {
  diagnostics.push_back(
      {std::move(operation), std::move(entry), std::move(message)});
}

std::string zip_error(mz_zip_archive& archive) {
  const mz_zip_error error = mz_zip_get_last_error(&archive);
  const char* text = mz_zip_get_error_string(error);
  return text == nullptr ? "unknown ZIP error" : std::string(text);
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

bool has_package_extension(const std::filesystem::path& path) {
  return path.extension() == std::filesystem::path(".aeylashow");
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

bool write_bytes_and_sync(
    const std::filesystem::path& path,
    std::span<const std::uint8_t> bytes,
    std::vector<ProjectPackageDiagnostic>& diagnostics) {
  FILE* file = open_binary_write(path);
  if (file == nullptr) {
    add(diagnostics, "open-temp", path.string(),
        "could not open temporary package");
    return false;
  }

  const std::size_t written =
      std::fwrite(bytes.data(), 1U, bytes.size(), file);
  if (written != bytes.size()) {
    add(diagnostics, "write-temp", path.string(),
        "short write while storing project package");
    std::fclose(file);
    return false;
  }

  if (!sync_file(file)) {
    add(diagnostics, "sync-temp", path.string(),
        "could not flush project package to stable storage");
    std::fclose(file);
    return false;
  }

  if (std::fclose(file) != 0) {
    add(diagnostics, "close-temp", path.string(),
        "could not close temporary project package");
    return false;
  }
  return true;
}

std::optional<std::vector<std::uint8_t>> read_bounded(
    const std::filesystem::path& path,
    std::vector<ProjectPackageDiagnostic>& diagnostics) {
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(path, error);
  if (error) {
    add(diagnostics, "stat", path.string(), error.message());
    return std::nullopt;
  }
  if (size == 0U || size > kMaximumArchiveBytes) {
    add(diagnostics, "read", path.string(),
        "archive size must be between 1 byte and 8 MiB");
    return std::nullopt;
  }

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    add(diagnostics, "open", path.string(),
        "could not open project package for reading");
    return std::nullopt;
  }

  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  if (!input || static_cast<std::size_t>(input.gcount()) != bytes.size()) {
    add(diagnostics, "read", path.string(),
        "could not read the complete project package");
    return std::nullopt;
  }
  return bytes;
}

void remove_if_present(
    const std::filesystem::path& path,
    std::vector<ProjectPackageDiagnostic>& diagnostics,
    std::string operation) {
  std::error_code error;
  (void) std::filesystem::remove(path, error);
  if (error) add(diagnostics, std::move(operation), path.string(), error.message());
}

ProjectPackageLoadResult load_bytes(
    std::span<const std::uint8_t> bytes,
    const std::filesystem::path& source) {
  ProjectPackageLoadResult result;
  result.source = source;

  if (bytes.empty() || bytes.size() > kMaximumArchiveBytes) {
    add(result.diagnostics, "validate-archive", source.string(),
        "archive size must be between 1 byte and 8 MiB");
    return result;
  }

  mz_zip_error validationError = MZ_ZIP_NO_ERROR;
  if (!mz_zip_validate_mem_archive(bytes.data(), bytes.size(), 0,
                                   &validationError)) {
    const char* text = mz_zip_get_error_string(validationError);
    add(result.diagnostics, "validate-archive", source.string(),
        text == nullptr ? "invalid ZIP archive" : text);
    return result;
  }

  mz_zip_archive archive{};
  mz_zip_zero_struct(&archive);
  if (!mz_zip_reader_init_mem(&archive, bytes.data(), bytes.size(), 0)) {
    add(result.diagnostics, "open-archive", source.string(), zip_error(archive));
    return result;
  }

  const auto finish = [&]() { (void) mz_zip_reader_end(&archive); };
  if (mz_zip_is_zip64(&archive)) {
    add(result.diagnostics, "validate-archive", source.string(),
        "ZIP64 is not permitted for Alpha 0.3 project packages");
    finish();
    return result;
  }

  const mz_uint fileCount = mz_zip_reader_get_num_files(&archive);
  if (fileCount != 1U) {
    add(result.diagnostics, "validate-entries", source.string(),
        "Alpha 0.3 package must contain exactly one project.json entry");
    finish();
    return result;
  }

  mz_zip_archive_file_stat stat{};
  if (!mz_zip_reader_file_stat(&archive, 0U, &stat)) {
    add(result.diagnostics, "inspect-entry", kProjectEntry, zip_error(archive));
    finish();
    return result;
  }

  if (std::strcmp(stat.m_filename, kProjectEntry) != 0 || stat.m_is_directory) {
    add(result.diagnostics, "validate-entry", stat.m_filename,
        "the only permitted entry is project.json at the archive root");
    finish();
    return result;
  }
  if (stat.m_is_encrypted) {
    add(result.diagnostics, "validate-entry", kProjectEntry,
        "encrypted project packages are not supported");
    finish();
    return result;
  }
  if (!stat.m_is_supported) {
    add(result.diagnostics, "validate-entry", kProjectEntry,
        "project.json uses an unsupported ZIP feature or compression method");
    finish();
    return result;
  }
  if (stat.m_uncomp_size == 0U || stat.m_uncomp_size > kMaximumProjectJsonBytes) {
    add(result.diagnostics, "validate-entry", kProjectEntry,
        "project.json size must be between 1 byte and 4 MiB");
    finish();
    return result;
  }

  std::vector<std::uint8_t> projectBytes(
      static_cast<std::size_t>(stat.m_uncomp_size));
  if (!mz_zip_reader_extract_to_mem(&archive, 0U, projectBytes.data(),
                                    projectBytes.size(), 0)) {
    add(result.diagnostics, "extract-entry", kProjectEntry, zip_error(archive));
    finish();
    return result;
  }
  finish();

  const std::string_view json(
      reinterpret_cast<const char*>(projectBytes.data()), projectBytes.size());
  ProjectParseResult parsed = deserialize_project_document(json);
  if (!parsed.ok()) {
    for (const auto& diagnostic : parsed.validation.diagnostics) {
      add(result.diagnostics, "validate-project", diagnostic.path,
          diagnostic.message);
    }
    return result;
  }
  if (!parsed.document->assets.empty() || !parsed.document->checksums.empty()) {
    add(result.diagnostics, "validate-project", "assets/checksums",
        "asset-bearing packages are blocked until SHA-256 streaming verification is implemented");
    return result;
  }

  result.document = std::move(parsed.document);
  return result;
}

std::optional<std::vector<std::uint8_t>> build_archive(
    const ProjectDocument& document,
    std::vector<ProjectPackageDiagnostic>& diagnostics) {
  const ProjectValidation validation = validate_project_document(document);
  if (!validation.ok()) {
    for (const auto& diagnostic : validation.diagnostics) {
      if (diagnostic.severity == DiagnosticSeverity::error)
        add(diagnostics, "validate-project", diagnostic.path,
            diagnostic.message);
    }
    return std::nullopt;
  }
  if (!document.assets.empty() || !document.checksums.empty()) {
    add(diagnostics, "validate-project", "assets/checksums",
        "asset-bearing packages are blocked until SHA-256 streaming verification is implemented");
    return std::nullopt;
  }

  const std::string projectJson = serialize_project_document(document);
  mz_zip_archive archive{};
  mz_zip_zero_struct(&archive);
  if (!mz_zip_writer_init_heap(&archive, 0U, projectJson.size() + 1024U)) {
    add(diagnostics, "create-archive", kProjectEntry, zip_error(archive));
    return std::nullopt;
  }

  if (!mz_zip_writer_add_mem(&archive, kProjectEntry, projectJson.data(),
                             projectJson.size(), MZ_BEST_COMPRESSION)) {
    add(diagnostics, "add-entry", kProjectEntry, zip_error(archive));
    (void) mz_zip_writer_end(&archive);
    return std::nullopt;
  }

  void* heap = nullptr;
  std::size_t heapSize = 0U;
  if (!mz_zip_writer_finalize_heap_archive(&archive, &heap, &heapSize)) {
    add(diagnostics, "finalize-archive", kProjectEntry, zip_error(archive));
    (void) mz_zip_writer_end(&archive);
    return std::nullopt;
  }
  (void) mz_zip_writer_end(&archive);

  if (heap == nullptr || heapSize == 0U || heapSize > kMaximumArchiveBytes) {
    add(diagnostics, "finalize-archive", kProjectEntry,
        "generated archive is empty or exceeds the 8 MiB limit");
    if (heap != nullptr) mz_free(heap);
    return std::nullopt;
  }

  std::vector<std::uint8_t> bytes(heapSize);
  std::memcpy(bytes.data(), heap, heapSize);
  mz_free(heap);

  const auto verified = load_bytes(bytes, "<generated-memory-package>");
  if (!verified.ok() || !verified.document.has_value() ||
      serialize_project_document(*verified.document) != projectJson) {
    add(diagnostics, "verify-archive", kProjectEntry,
        "generated package failed deterministic read-back verification");
    return std::nullopt;
  }
  return bytes;
}

}  // namespace

ProjectPackageLoadResult load_project_package(
    const std::filesystem::path& source) {
  ProjectPackageLoadResult result;
  result.source = source;
  if (!has_package_extension(source)) {
    add(result.diagnostics, "validate-path", source.string(),
        "project package must use the .aeylashow extension");
    return result;
  }

  const auto bytes = read_bounded(source, result.diagnostics);
  if (!bytes.has_value()) return result;
  return load_bytes(*bytes, source);
}

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document) {
  ProjectPackageSaveResult result;
  result.target = target;
  result.backup = backup_path(target);

  if (!has_package_extension(target)) {
    add(result.diagnostics, "validate-path", target.string(),
        "project package must use the .aeylashow extension");
    return result;
  }

  const auto archive = build_archive(document, result.diagnostics);
  if (!archive.has_value()) return result;

  std::error_code error;
  const auto parent = target.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, error);
    if (error) {
      add(result.diagnostics, "create-directory", parent.string(),
          error.message());
      return result;
    }
  }

  const std::filesystem::path temporary = temporary_path(target);
  remove_if_present(temporary, result.diagnostics, "remove-stale-temp");
  if (!result.diagnostics.empty()) return result;

  if (!write_bytes_and_sync(temporary, *archive, result.diagnostics)) {
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  // Temporary files intentionally have .tmp appended, so verify their bytes
  // directly rather than relaxing the public extension contract.
  const auto temporaryBytes = read_bounded(temporary, result.diagnostics);
  if (!temporaryBytes.has_value()) {
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }
  const auto verified = load_bytes(*temporaryBytes, temporary);
  if (!verified.ok() || !verified.document.has_value() ||
      serialize_project_document(*verified.document) !=
          serialize_project_document(document)) {
    add(result.diagnostics, "verify-temp", temporary.string(),
        "temporary package failed read-back verification");
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  remove_if_present(result.backup, result.diagnostics,
                    "remove-stale-backup");
  if (!result.diagnostics.empty()) {
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  const bool targetExisted = std::filesystem::exists(target, error);
  if (error) {
    add(result.diagnostics, "inspect-target", target.string(), error.message());
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  if (targetExisted) {
    std::filesystem::rename(target, result.backup, error);
    if (error) {
      add(result.diagnostics, "backup-current", target.string(), error.message());
      remove_if_present(temporary, result.diagnostics, "cleanup-temp");
      return result;
    }
  }

  std::filesystem::rename(temporary, target, error);
  if (error) {
    add(result.diagnostics, "replace-target", target.string(), error.message());
    if (targetExisted) {
      std::error_code restoreError;
      std::filesystem::rename(result.backup, target, restoreError);
      if (restoreError)
        add(result.diagnostics, "restore-backup", result.backup.string(),
            restoreError.message());
    }
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  result.saved = true;
  return result;
}

}  // namespace aeyla::project
