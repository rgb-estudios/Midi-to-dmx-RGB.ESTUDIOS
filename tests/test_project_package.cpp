#include "project/project_package.h"

#include "miniz.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

std::filesystem::path unique_test_directory() {
  const auto base = std::filesystem::temp_directory_path();
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto candidate = base / ("aeyla-package-test-" + std::to_string(attempt));
    std::error_code error;
    if (std::filesystem::create_directory(candidate, error)) return candidate;
  }
  return {};
}

std::vector<std::uint8_t> read_all(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

bool write_raw_zip(const std::filesystem::path& path,
                   const std::vector<std::pair<std::string, std::string>>& entries) {
  mz_zip_archive archive{};
  mz_zip_zero_struct(&archive);
  if (!mz_zip_writer_init_heap(&archive, 0U, 4096U)) return false;

  for (const auto& [name, contents] : entries) {
    if (!mz_zip_writer_add_mem(&archive, name.c_str(), contents.data(),
                               contents.size(), MZ_BEST_COMPRESSION)) {
      (void) mz_zip_writer_end(&archive);
      return false;
    }
  }

  void* heap = nullptr;
  std::size_t heap_size = 0U;
  if (!mz_zip_writer_finalize_heap_archive(&archive, &heap, &heap_size)) {
    (void) mz_zip_writer_end(&archive);
    return false;
  }
  (void) mz_zip_writer_end(&archive);

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(static_cast<const char*>(heap),
               static_cast<std::streamsize>(heap_size));
  mz_free(heap);
  return static_cast<bool>(output);
}
}  // namespace

int main() {
  using namespace aeyla::project;

  const std::filesystem::path directory = unique_test_directory();
  check(!directory.empty(), "test must create an isolated temporary directory");
  if (directory.empty()) return EXIT_FAILURE;

  auto document = make_default_project_document(
      "abcdef12-3456-4789-8abc-def012345678", "2026-08-07T03:00:00Z");
  document.name = "AEYLA Package Round Trip";

  const auto target = directory / "show.aeylashow";
  const ProjectPackageSaveResult first =
      save_project_package_atomic(target, document);
  check(first.ok(), "first .aeylashow package save must succeed");
  check(std::filesystem::exists(target), "package target must exist after save");
  check(!std::filesystem::exists(target.string() + ".tmp"),
        "successful package save must not leave a temporary file");

  const ProjectPackageLoadResult loaded = load_project_package(target);
  check(loaded.ok(), "saved .aeylashow package must load");
  if (loaded.document.has_value())
    check(*loaded.document == document,
          "package round-trip must preserve the authored document");

  const auto second_target = directory / "show-copy.aeylashow";
  const ProjectPackageSaveResult deterministic =
      save_project_package_atomic(second_target, document);
  check(deterministic.ok(), "second independent package save must succeed");
  check(read_all(target) == read_all(second_target),
        "identical project documents must produce byte-identical packages");

  auto replacement = document;
  replacement.name = "AEYLA Package Replacement";
  replacement.modified_at = "2026-08-07T03:10:00Z";
  const ProjectPackageSaveResult replaced =
      save_project_package_atomic(target, replacement);
  check(replaced.ok(), "atomic package replacement must succeed");
  check(std::filesystem::exists(replaced.backup),
        "replacement must preserve the previous package as backup");
  const auto current = load_project_package(target);
  const auto backup = load_project_package(replaced.backup);
  check(current.ok() && current.document.has_value() &&
            current.document->name == replacement.name,
        "target must contain replacement project");
  // Backup has .bak appended, so public loader intentionally rejects it by
  // extension. Rename a copy to prove the backup bytes remain a valid package.
  const auto backup_copy = directory / "backup-copy.aeylashow";
  std::filesystem::copy_file(replaced.backup, backup_copy,
                             std::filesystem::copy_options::overwrite_existing);
  const auto backup_loaded = load_project_package(backup_copy);
  check(!backup.ok() && backup_loaded.ok() && backup_loaded.document.has_value() &&
            backup_loaded.document->name == document.name,
        "backup bytes must preserve the previous valid package");

  const auto wrong_extension = directory / "show.zip";
  check(!save_project_package_atomic(wrong_extension, document).ok(),
        "save must reject a non-.aeylashow extension");

  auto with_asset = document;
  with_asset.assets.push_back({"assets/intro.mov", std::string(64U, 'a')});
  check(!save_project_package_atomic(directory / "assets.aeylashow", with_asset).ok(),
        "asset-bearing package must remain blocked until SHA verification exists");

  const std::string project_json = serialize_project_document(document);
  const auto extra_entry = directory / "extra-entry.aeylashow";
  check(write_raw_zip(extra_entry,
                      {{"project.json", project_json}, {"assets/extra.bin", "x"}}),
        "test must construct an archive with an unexpected entry");
  check(!load_project_package(extra_entry).ok(),
        "loader must reject unexpected archive entries");

  const auto traversal_entry = directory / "traversal.aeylashow";
  check(write_raw_zip(traversal_entry, {{"../project.json", project_json}}),
        "test must construct a traversal archive");
  check(!load_project_package(traversal_entry).ok(),
        "loader must reject traversal or non-root project entries");

  const auto duplicate_entry = directory / "duplicate.aeylashow";
  check(write_raw_zip(duplicate_entry,
                      {{"project.json", project_json},
                       {"project.json", project_json}}),
        "test must construct duplicate project entries");
  check(!load_project_package(duplicate_entry).ok(),
        "loader must reject duplicate project.json entries");

  const auto corrupt = directory / "corrupt.aeylashow";
  {
    std::ofstream output(corrupt, std::ios::binary | std::ios::trunc);
    output << "not-a-zip";
  }
  check(!load_project_package(corrupt).ok(),
        "loader must reject corrupted package bytes");

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);
  check(!cleanup_error, "test must clean temporary package files");

  if (failures == 0) {
    std::cout << "All AEYLA project package tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
