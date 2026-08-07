#include "project/project_state_store.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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
    const auto candidate = base / ("aeyla-project-store-test-" + std::to_string(attempt));
    std::error_code error;
    if (std::filesystem::create_directory(candidate, error)) return candidate;
  }
  return {};
}
}  // namespace

int main() {
  using namespace aeyla::project;

  const std::filesystem::path directory = unique_test_directory();
  check(!directory.empty(), "test must create an isolated temporary directory");
  if (directory.empty()) return EXIT_FAILURE;

  const auto target = directory / "state" / "project.json";
  auto first = make_default_project_document(
      "12345678-1234-4234-8234-123456789abc", "2026-08-07T02:00:00Z");
  first.name = "First Atomic State";

  const ProjectSaveResult first_save = save_project_state_json_atomic(target, first);
  check(first_save.ok(), "first atomic save must succeed");
  check(std::filesystem::exists(target), "first save must create target state");
  check(!std::filesystem::exists(target.string() + ".tmp"),
        "successful save must not leave temporary state");
  check(!std::filesystem::exists(target.string() + ".bak"),
        "first save has no previous version to back up");

  const ProjectLoadResult first_load = load_project_state_json(target);
  check(first_load.ok(), "saved project state must load and validate");
  if (first_load.parsed.document.has_value())
    check(*first_load.parsed.document == first,
          "loaded state must exactly match first saved document");

  auto second = first;
  second.name = "Second Atomic State";
  second.modified_at = "2026-08-07T02:05:00Z";
  const ProjectSaveResult second_save = save_project_state_json_atomic(target, second);
  check(second_save.ok(), "second atomic save must succeed");
  check(std::filesystem::exists(second_save.backup),
        "second save must preserve the previous target as backup");

  const ProjectLoadResult current = load_project_state_json(target);
  const ProjectLoadResult backup = load_project_state_json(second_save.backup);
  check(current.ok() && current.parsed.document.has_value() &&
            current.parsed.document->name == second.name,
        "target must contain second project state");
  check(backup.ok() && backup.parsed.document.has_value() &&
            backup.parsed.document->name == first.name,
        "backup must contain first project state");

  auto invalid = second;
  invalid.fixtures[1].address = invalid.fixtures[0].address;
  const ProjectSaveResult rejected = save_project_state_json_atomic(target, invalid);
  check(!rejected.ok(), "invalid project must be rejected before disk replacement");
  const ProjectLoadResult after_rejection = load_project_state_json(target);
  check(after_rejection.ok() && after_rejection.parsed.document.has_value() &&
            after_rejection.parsed.document->name == second.name,
        "failed save must leave the last valid target unchanged");

  {
    std::ofstream corrupt(target, std::ios::binary | std::ios::trunc);
    corrupt << "{\"truncated\":";
  }
  const ProjectLoadResult corrupt_load = load_project_state_json(target);
  check(!corrupt_load.ok(), "truncated project state must be rejected");
  const ProjectLoadResult backup_after_corruption = load_project_state_json(second_save.backup);
  check(backup_after_corruption.ok(), "backup must remain readable after target corruption");

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);
  check(!cleanup_error, "test must clean temporary files");

  if (failures == 0) {
    std::cout << "All AEYLA project state store tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
