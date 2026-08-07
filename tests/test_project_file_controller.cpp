#include "product/project_file_controller.h"

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
    const auto candidate =
        base / ("aeyla-file-controller-test-" + std::to_string(attempt));
    std::error_code error;
    if (std::filesystem::create_directory(candidate, error)) return candidate;
  }
  return {};
}
}  // namespace

int main() {
  using namespace aeyla::product;

  const auto directory = unique_test_directory();
  check(!directory.empty(), "test must create an isolated directory");
  if (directory.empty()) return EXIT_FAILURE;

  ApplicationModel model;
  ProjectFileController controller(model);

  const auto no_path = controller.save("2026-08-07T04:00:00Z");
  check(!no_path.succeeded,
        "Save without a current package path must require Save As");

  const auto created = controller.new_project(
      "11111111-aaaa-4bbb-8ccc-222222222222",
      "2026-08-07T04:00:00Z");
  check(created.succeeded, "New must create a valid default project");
  check(controller.current_path().empty(),
        "new unsaved project must not invent a package path");
  check(model.snapshot().blackout && !model.snapshot().output_armed,
        "New must publish only a blackout/disarmed runtime");
  check(model.snapshot().project_dirty,
        "new project must require an explicit first save");

  model.set_visual_source(VisualSource::chase);
  model.set_visual_speed(0.64F);
  model.set_rig14(true);

  const auto package = directory / "controller.aeylashow";
  const auto saved = controller.save_as(package, "2026-08-07T04:05:00Z");
  check(saved.succeeded, "Save As must create and verify a package");
  check(controller.current_path() == package,
        "Save As must adopt the selected package path");
  check(!model.snapshot().project_dirty,
        "successful Save As must clear project dirty state");

  model.set_white_extraction(0.77F);
  check(model.snapshot().project_dirty,
        "authored change after save must mark project dirty");
  const auto resaved = controller.save("2026-08-07T04:10:00Z");
  check(resaved.succeeded,
        "Save must replace the current package and preserve a backup");
  check(std::filesystem::exists(package.string() + ".bak"),
        "second save must preserve last-known-good package backup");

  ApplicationModel reopened_model;
  ProjectFileController reopened(reopened_model);
  const auto opened = reopened.open(package);
  check(opened.succeeded, "Open must validate and publish a saved package");
  check(reopened.current_path() == package,
        "Open must adopt the selected package path");
  check(reopened_model.snapshot().blackout &&
            !reopened_model.snapshot().output_armed,
        "Open must remain blackout and disarmed");
  check(!reopened_model.snapshot().project_dirty,
        "opened package must begin clean");
  check(reopened_model.project_document().visual.active_look_id == "look-chase" &&
            reopened_model.project_document().visual.speed == 0.64F &&
            reopened_model.project_document().visual.white_extraction == 0.77F,
        "Open must restore authored visual state from the package");

  const auto invalid = directory / "invalid.aeylashow";
  {
    std::ofstream output(invalid, std::ios::binary | std::ios::trunc);
    output << "not-a-project";
  }
  const auto rejected = reopened.open(invalid);
  check(!rejected.succeeded,
        "Open must reject corrupt package without changing current path");
  check(reopened.current_path() == package,
        "failed Open must preserve current valid project path");

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);
  check(!cleanup_error, "test must clean temporary controller files");

  if (failures == 0) {
    std::cout << "All AEYLA project file controller tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
