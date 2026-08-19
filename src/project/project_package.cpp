#include "project/project_package.h"

#include "miniz.h"
#include "show/show_program_codec.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
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

constexpr std::uintmax_t kMaximumArchiveBytes = 9U * 1024U * 1024U;
constexpr std::uint64_t kMaximumProjectJsonBytes = 4U * 1024U * 1024U;
constexpr const char* kProjectEntry = "project.json";
constexpr const char* kShowEntry = "show.bin";

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
        "archive size must be between 1 byte and 9 MiB");
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

std::set<std::string> available_look_ids(const ProjectDocument& document) {
  std::set<std::string> result;
  for (const auto& look : document.looks) result.insert(look.look_id);
  return result;
}

std::optional<std::vector<std::uint8_t>> extract_entry(
    mz_zip_archive& archive,
    mz_uint index,
    const mz_zip_archive_file_stat& stat,
    std::uint64_t maximum_size,
    std::string_view entry_name,
    std::vector<ProjectPackageDiagnostic>& diagnostics) {
  if (stat.m_is_directory) {
    add(diagnostics, "validate-entry", std::string(entry_name),
        "package entries must be files at the archive root");
    return std::nullopt;
  }
  if (stat.m_is_encrypted) {
    add(diagnostics, "validate-entry", std::string(entry_name),
        "encrypted project packages are not supported");
    return std::nullopt;
  }
  if (!stat.m_is_supported) {
    add(diagnostics, "validate-entry", std::string(entry_name),
        "entry uses an unsupported ZIP feature or compression method");
    return std::nullopt;
  }
  if (stat.m_uncomp_size == 0U || stat.m_uncomp_size > maximum_size) {
    add(diagnostics, "validate-entry", std::string(entry_name),
        "entry is empty or exceeds its bounded size limit");
    return std::nullopt;
  }

  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(stat.m_uncomp_size));
  if (!mz_zip_reader_extract_to_mem(&archive, index, bytes.data(), bytes.size(), 0)) {
    add(diagnostics, "extract-entry", std::string(entry_name), zip_error(archive));
    return std::nullopt;
  }
  return bytes;
}

ProjectPackageLoadResult load_bytes(
    std::span<const std::uint8_t> bytes,
    const std::filesystem::path& source) {
  ProjectPackageLoadResult result;
  result.source = source;

  if (bytes.empty() || bytes.size() > kMaximumArchiveBytes) {
    add(result.diagnostics, "validate-archive", source.string(),
        "archive size must be between 1 byte and 9 MiB");
    return result;
  }

  mz_zip_error validation_error = MZ_ZIP_NO_ERROR;
  if (!mz_zip_validate_mem_archive(bytes.data(), bytes.size(), 0,
                                   &validation_error)) {
    const char* text = mz_zip_get_error_string(validation_error);
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
        "ZIP64 is not permitted for AEYLA project packages");
    finish();
    return result;
  }

  const mz_uint file_count = mz_zip_reader_get_num_files(&archive);
  if (file_count != 1U && file_count != 2U) {
    add(result.diagnostics, "validate-entries", source.string(),
        "package must contain legacy project.json only or current project.json + show.bin");
    finish();
    return result;
  }

  std::optional<mz_uint> project_index;
  std::optional<mz_uint> show_index;
  mz_zip_archive_file_stat project_stat{};
  mz_zip_archive_file_stat show_stat{};

  for (mz_uint index = 0U; index < file_count; ++index) {
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&archive, index, &stat)) {
      add(result.diagnostics, "inspect-entry", source.string(), zip_error(archive));
      finish();
      return result;
    }

    const std::string name(stat.m_filename);
    if (name == kProjectEntry) {
      if (project_index.has_value()) {
        add(result.diagnostics, "validate-entry", name,
            "duplicate project.json entry is not permitted");
        finish();
        return result;
      }
      project_index = index;
      project_stat = stat;
    } else if (name == kShowEntry) {
      if (show_index.has_value()) {
        add(result.diagnostics, "validate-entry", name,
            "duplicate show.bin entry is not permitted");
        finish();
        return result;
      }
      show_index = index;
      show_stat = stat;
    } else {
      add(result.diagnostics, "validate-entry", name,
          "only project.json and show.bin are permitted at the archive root");
      finish();
      return result;
    }
  }

  if (!project_index.has_value()) {
    add(result.diagnostics, "validate-entry", kProjectEntry,
        "project.json is required");
    finish();
    return result;
  }
  if (file_count == 2U && !show_index.has_value()) {
    add(result.diagnostics, "validate-entry", kShowEntry,
        "two-entry packages must contain show.bin");
    finish();
    return result;
  }

  const auto project_bytes = extract_entry(
      archive, *project_index, project_stat, kMaximumProjectJsonBytes,
      kProjectEntry, result.diagnostics);
  if (!project_bytes.has_value()) {
    finish();
    return result;
  }

  std::optional<std::vector<std::uint8_t>> show_bytes;
  if (show_index.has_value()) {
    show_bytes = extract_entry(
        archive, *show_index, show_stat,
        static_cast<std::uint64_t>(show::kMaximumEncodedShowBytes),
        kShowEntry, result.diagnostics);
    if (!show_bytes.has_value()) {
      finish();
      return result;
    }
  }
  finish();

  const std::string_view json(
      reinterpret_cast<const char*>(project_bytes->data()), project_bytes->size());
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

  show::ShowProgram program;
  if (show_bytes.has_value()) {
    const auto decoded = show::decode_show_program(
        *show_bytes, available_look_ids(*parsed.document));
    if (!decoded.ok() || !decoded.program.has_value()) {
      for (const auto& diagnostic : decoded.diagnostics) {
        add(result.diagnostics, "validate-show", kShowEntry,
            "offset " + std::to_string(diagnostic.offset) + ": " +
                diagnostic.message);
      }
      return result;
    }
    program = *decoded.program;
  } else {
    // Legacy project-only package. Empty authoring state is valid but will not
    // pass performance preflight until songs are authored.
    result.legacy_project_only = true;
  }

  result.document = std::move(parsed.document);
  result.show_program = std::move(program);
  return result;
}

std::optional<std::vector<std::uint8_t>> build_archive(
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
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

  const auto encoded_show = show::encode_show_program(
      show_program, available_look_ids(document));
  if (!encoded_show.ok()) {
    for (const auto& diagnostic : encoded_show.diagnostics) {
      add(diagnostics, "validate-show", kShowEntry,
          "offset " + std::to_string(diagnostic.offset) + ": " +
              diagnostic.message);
    }
    return std::nullopt;
  }

  const std::string project_json = serialize_project_document(document);
  mz_zip_archive archive{};
  mz_zip_zero_struct(&archive);
  const std::size_t reserve =
      project_json.size() + encoded_show.bytes.size() + 2048U;
  if (!mz_zip_writer_init_heap(&archive, 0U, reserve)) {
    add(diagnostics, "create-archive", kProjectEntry, zip_error(archive));
    return std::nullopt;
  }

  if (!mz_zip_writer_add_mem(&archive, kProjectEntry, project_json.data(),
                             project_json.size(), MZ_BEST_COMPRESSION)) {
    add(diagnostics, "add-entry", kProjectEntry, zip_error(archive));
    (void) mz_zip_writer_end(&archive);
    return std::nullopt;
  }
  if (!mz_zip_writer_add_mem(&archive, kShowEntry,
                             encoded_show.bytes.data(),
                             encoded_show.bytes.size(), MZ_BEST_COMPRESSION)) {
    add(diagnostics, "add-entry", kShowEntry, zip_error(archive));
    (void) mz_zip_writer_end(&archive);
    return std::nullopt;
  }

  void* heap = nullptr;
  std::size_t heap_size = 0U;
  if (!mz_zip_writer_finalize_heap_archive(&archive, &heap, &heap_size)) {
    add(diagnostics, "finalize-archive", "<generated>", zip_error(archive));
    (void) mz_zip_writer_end(&archive);
    return std::nullopt;
  }
  (void) mz_zip_writer_end(&archive);

  if (heap == nullptr || heap_size == 0U || heap_size > kMaximumArchiveBytes) {
    add(diagnostics, "finalize-archive", "<generated>",
        "generated archive is empty or exceeds the 9 MiB limit");
    if (heap != nullptr) mz_free(heap);
    return std::nullopt;
  }

  std::vector<std::uint8_t> bytes(heap_size);
  std::memcpy(bytes.data(), heap, heap_size);
  mz_free(heap);

  const auto verified = load_bytes(bytes, "<generated-memory-package>");
  if (!verified.ok() || !verified.document.has_value() ||
      !verified.show_program.has_value() || verified.legacy_project_only ||
      serialize_project_document(*verified.document) != project_json ||
      *verified.show_program != show_program) {
    add(diagnostics, "verify-archive", "<generated>",
        "generated package failed deterministic project+show read-back verification");
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
    const ProjectDocument& document,
    const show::ShowProgram& show_program) {
  ProjectPackageSaveResult result;
  result.target = target;
  result.backup = backup_path(target);

  if (!has_package_extension(target)) {
    add(result.diagnostics, "validate-path", target.string(),
        "project package must use the .aeylashow extension");
    return result;
  }

  const auto archive = build_archive(document, show_program, result.diagnostics);
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
  const auto temporary_bytes = read_bounded(temporary, result.diagnostics);
  if (!temporary_bytes.has_value()) {
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }
  const auto verified = load_bytes(*temporary_bytes, temporary);
  if (!verified.ok() || !verified.document.has_value() ||
      !verified.show_program.has_value() || verified.legacy_project_only ||
      serialize_project_document(*verified.document) !=
          serialize_project_document(document) ||
      *verified.show_program != show_program) {
    add(result.diagnostics, "verify-temp", temporary.string(),
        "temporary package failed project+show read-back verification");
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  remove_if_present(result.backup, result.diagnostics,
                    "remove-stale-backup");
  if (!result.diagnostics.empty()) {
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  const bool target_existed = std::filesystem::exists(target, error);
  if (error) {
    add(result.diagnostics, "inspect-target", target.string(), error.message());
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  if (target_existed) {
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
    if (target_existed) {
      std::error_code restore_error;
      std::filesystem::rename(result.backup, target, restore_error);
      if (restore_error)
        add(result.diagnostics, "restore-backup", result.backup.string(),
            restore_error.message());
    }
    remove_if_present(temporary, result.diagnostics, "cleanup-temp");
    return result;
  }

  result.saved = true;
  return result;
}

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document) {
  return save_project_package_atomic(target, document, show::ShowProgram{});
}

}  // namespace aeyla::project
