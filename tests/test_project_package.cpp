#include "project/project_package.h"
#include "show/show_program_codec.h"

#include "miniz.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
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

aeyla::show::ShowProgram make_show(const aeyla::project::ProjectDocument& document,
                                    std::string song_name = "Package Song") {
  using namespace aeyla::show;
  ShowProgram program;
  SongProgram song;
  song.song_id = "package-song";
  song.name = std::move(song_name);
  song.length_ticks = 4U * song.ppq;
  song.scenes.push_back({"scene-main", "Main", document.looks.front().look_id,
                         250U, 250U, false, CueBehavior::latch});
  song.clips.push_back({"clip-main", "scene-main", 0U, song.length_ticks,
                        36U, 127U, 1U});
  program.songs.push_back(std::move(song));
  return program;
}

std::set<std::string> look_ids(const aeyla::project::ProjectDocument& document) {
  std::set<std::string> result;
  for (const auto& look : document.looks) result.insert(look.look_id);
  return result;
}

std::string binary_string(const std::vector<std::uint8_t>& bytes) {
  return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
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
  const auto show_program = make_show(document);

  const auto target = directory / "show.aeylashow";
  const ProjectPackageSaveResult first =
      save_project_package_atomic(target, document, show_program);
  check(first.ok(), "first project+show .aeylashow package save must succeed");
  check(std::filesystem::exists(target), "package target must exist after save");
  check(!std::filesystem::exists(target.string() + ".tmp"),
        "successful package save must not leave a temporary file");

  const ProjectPackageLoadResult loaded = load_project_package(target);
  check(loaded.ok(), "saved project+show .aeylashow package must load");
  check(!loaded.legacy_project_only,
        "current two-entry package must not be marked as legacy");
  if (loaded.document.has_value())
    check(*loaded.document == document,
          "package round-trip must preserve the authored project document");
  if (loaded.show_program.has_value())
    check(*loaded.show_program == show_program,
          "package round-trip must preserve songs, scenes, clips and MIDI mappings");

  const auto second_target = directory / "show-copy.aeylashow";
  const ProjectPackageSaveResult deterministic =
      save_project_package_atomic(second_target, document, show_program);
  check(deterministic.ok(), "second independent package save must succeed");
  check(read_all(target) == read_all(second_target),
        "identical project+show state must produce byte-identical packages");

  auto replacement = document;
  replacement.name = "AEYLA Package Replacement";
  replacement.modified_at = "2026-08-07T03:10:00Z";
  const auto replacement_show = make_show(replacement, "Replacement Song");
  const ProjectPackageSaveResult replaced =
      save_project_package_atomic(target, replacement, replacement_show);
  check(replaced.ok(), "atomic project+show package replacement must succeed");
  check(std::filesystem::exists(replaced.backup),
        "replacement must preserve the previous package as backup");
  const auto current = load_project_package(target);
  const auto backup = load_project_package(replaced.backup);
  check(current.ok() && current.document.has_value() &&
            current.show_program.has_value() &&
            current.document->name == replacement.name &&
            current.show_program->songs.front().name == "Replacement Song",
        "target must contain replacement project and replacement show together");
  // Backup has .bak appended, so public loader intentionally rejects it by
  // extension. Rename a copy to prove the backup bytes remain a valid package.
  const auto backup_copy = directory / "backup-copy.aeylashow";
  std::filesystem::copy_file(replaced.backup, backup_copy,
                             std::filesystem::copy_options::overwrite_existing);
  const auto backup_loaded = load_project_package(backup_copy);
  check(!backup.ok() && backup_loaded.ok() && backup_loaded.document.has_value() &&
            backup_loaded.show_program.has_value() &&
            backup_loaded.document->name == document.name &&
            *backup_loaded.show_program == show_program,
        "backup bytes must preserve the previous project+show transaction");

  // Legacy Alpha package compatibility: project.json only becomes a valid
  // empty authoring show. It remains ineligible for Show Mode until programmed.
  const std::string project_json = serialize_project_document(document);
  const auto legacy = directory / "legacy-project-only.aeylashow";
  check(write_raw_zip(legacy, {{"project.json", project_json}}),
        "test must construct a legacy project-only archive");
  const auto legacy_loaded = load_project_package(legacy);
  check(legacy_loaded.ok() && legacy_loaded.legacy_project_only &&
            legacy_loaded.show_program.has_value() &&
            legacy_loaded.show_program->songs.empty(),
        "legacy project-only package must migrate to an empty authoring show");
  check(!aeyla::show::validate_show_program_for_performance(
             *legacy_loaded.show_program, look_ids(document)).ok(),
        "legacy empty show must not accidentally pass performance preflight");

  // The compatibility save overload writes current format, not legacy format.
  const auto empty_current = directory / "empty-current.aeylashow";
  check(save_project_package_atomic(empty_current, document).ok(),
        "project-only save API must write a valid current empty-show package");
  const auto empty_current_loaded = load_project_package(empty_current);
  check(empty_current_loaded.ok() && !empty_current_loaded.legacy_project_only &&
            empty_current_loaded.show_program.has_value() &&
            empty_current_loaded.show_program->songs.empty(),
        "compatibility save overload must emit project.json + empty show.bin");

  const auto wrong_extension = directory / "show.zip";
  check(!save_project_package_atomic(wrong_extension, document, show_program).ok(),
        "save must reject a non-.aeylashow extension");

  auto with_asset = document;
  with_asset.assets.push_back({"assets/intro.mov", std::string(64U, 'a')});
  check(!save_project_package_atomic(directory / "assets.aeylashow",
                                     with_asset, show_program).ok(),
        "asset-bearing package must remain blocked until SHA verification exists");

  auto invalid_show = show_program;
  invalid_show.songs.front().scenes.front().look_id = "missing-look";
  check(!save_project_package_atomic(directory / "invalid-show.aeylashow",
                                     document, invalid_show).ok(),
        "package save must reject a show referencing a missing project look");

  const auto encoded_show = aeyla::show::encode_show_program(
      show_program, look_ids(document));
  check(encoded_show.ok(), "test fixture show.bin must encode");

  const auto extra_entry = directory / "extra-entry.aeylashow";
  check(write_raw_zip(extra_entry,
                      {{"project.json", project_json},
                       {"show.bin", binary_string(encoded_show.bytes)},
                       {"assets/extra.bin", "x"}}),
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

  const auto corrupt_show = directory / "corrupt-show.aeylashow";
  check(write_raw_zip(corrupt_show,
                      {{"project.json", project_json},
                       {"show.bin", "not-a-valid-show"}}),
        "test must construct a package with corrupt show.bin");
  const auto corrupt_show_loaded = load_project_package(corrupt_show);
  check(!corrupt_show_loaded.ok() && !corrupt_show_loaded.document.has_value(),
        "corrupt show.bin must fail the transaction before project publication");

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
