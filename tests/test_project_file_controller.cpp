#include "product/project_file_controller.h"
#include "product/project_identity.h"
#include "project/project_document.h"

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

aeyla::show::ShowProgram make_show(
    const aeyla::project::ProjectDocument& document) {
  using namespace aeyla::show;
  ShowProgram program;
  SongProgram song;
  song.song_id = "controller-song";
  song.name = "Controller Song";
  song.length_ticks = 8U * song.ppq;
  song.scenes = {
      {"scene-chase", "Chase", "look-chase", 500U, 250U, false,
       CueBehavior::latch},
      {"scene-hit", "White Hit", document.looks.front().look_id,
       0U, 0U, false, CueBehavior::momentary},
  };
  song.clips = {
      {"clip-chase", "scene-chase", 0U, song.length_ticks, 36U, 127U, 1U},
      {"clip-hit", "scene-hit", 4U * song.ppq, song.ppq / 4U,
       37U, 127U, 1U},
  };
  song.scenes[0].midi_binding = MidiBinding{36U, 1U};
  song.scenes[1].midi_binding = MidiBinding{37U, 1U};
  program.songs.push_back(std::move(song));
  return program;
}
}  // namespace

int main() {
  using namespace aeyla::product;

  const std::string generated_uuid = generate_project_uuid();
  const std::string second_uuid = generate_project_uuid();
  check(aeyla::project::is_canonical_uuid(generated_uuid),
        "generated project UUID must be lowercase canonical UUIDv4 text");
  check(generated_uuid != second_uuid,
        "consecutive project UUIDs should not collide");
  const std::string timestamp = current_utc_timestamp();
  check(timestamp.size() == 20U && timestamp[4] == '-' &&
            timestamp[10] == 'T' && timestamp.back() == 'Z',
        "UTC save timestamp must use YYYY-MM-DDTHH:MM:SSZ form");

  const auto directory = unique_test_directory();
  check(!directory.empty(), "test must create an isolated directory");
  if (directory.empty()) return EXIT_FAILURE;

  ApplicationModel model;
  ProjectFileController controller(model);

  const auto no_path = controller.save("2026-08-07T04:00:00Z");
  check(!no_path.succeeded &&
            no_path.operation == ProjectFileOperation::save,
        "Save without a current path must fail as a Save operation");

  const auto created = controller.new_project(
      "11111111-aaaa-4bbb-8ccc-222222222222",
      "2026-08-07T04:00:00Z");
  check(created.succeeded &&
            created.operation == ProjectFileOperation::new_project,
        "New must create and report a valid default project");
  check(controller.current_path().empty(),
        "new unsaved project must not invent a package path");
  check(model.snapshot().blackout && !model.snapshot().output_armed,
        "New must publish only a blackout/disarmed runtime");
  check(model.snapshot().project_dirty,
        "new project must require an explicit first save");
  check(model.snapshot().song_count == 0U && !model.snapshot().performance_ready,
        "new project must begin as editable but not performance-ready");

  model.set_visual_source(VisualSource::chase);
  model.set_visual_speed(0.64F);
  model.set_rig14(true);
  const auto authored_show = make_show(model.project_document());
  check(model.replace_show_program(authored_show).ok(),
        "controller test must author a valid show before saving");
  check(model.snapshot().song_count == 1U && model.snapshot().performance_ready,
        "authored show must satisfy performance preflight before save");

  const auto package = directory / "controller.aeylashow";
  const auto saved = controller.save_as(package, "2026-08-07T04:05:00Z");
  check(saved.succeeded &&
            saved.operation == ProjectFileOperation::save_as,
        "Save As must create, verify and report a new-path project+show save");
  check(controller.current_path() == package,
        "Save As must adopt the selected package path");
  check(!model.snapshot().project_dirty,
        "successful Save As must clear project+show dirty state");

  model.set_white_extraction(0.77F);
  check(model.snapshot().project_dirty,
        "authored change after save must mark project dirty");
  const auto resaved = controller.save("2026-08-07T04:10:00Z");
  check(resaved.succeeded &&
            resaved.operation == ProjectFileOperation::save,
        "Save must replace the current package and report Save");
  check(std::filesystem::exists(package.string() + ".bak"),
        "second save must preserve last-known-good package backup");

  ApplicationModel reopened_model;
  ProjectFileController reopened(reopened_model);
  const auto opened = reopened.open(package);
  check(opened.succeeded && opened.operation == ProjectFileOperation::open,
        "Open must validate and report a saved project+show package");
  check(reopened.current_path() == package,
        "Open must adopt the selected package path");
  check(reopened_model.snapshot().blackout &&
            !reopened_model.snapshot().output_armed,
        "Open must remain blackout and disarmed");
  check(!reopened_model.snapshot().project_dirty,
        "opened package must begin clean");
  check(reopened_model.snapshot().song_count == 1U &&
            reopened_model.snapshot().performance_ready &&
            reopened_model.show_program() == authored_show,
        "Open must restore the authored show program beside the project");
  check(reopened_model.project_document().visual.active_look_id == "look-chase" &&
            reopened_model.project_document().visual.speed == 0.64F &&
            reopened_model.project_document().visual.white_extraction == 0.77F,
        "Open must restore authored visual state from the package");

  const std::string current_project_id = reopened_model.snapshot().project_id;
  const auto current_show = reopened_model.show_program();
  const auto invalid = directory / "invalid.aeylashow";
  {
    std::ofstream output(invalid, std::ios::binary | std::ios::trunc);
    output << "not-a-project";
  }
  const auto rejected = reopened.open(invalid);
  check(!rejected.succeeded,
        "Open must reject corrupt package without changing current path");
  check(reopened.current_path() == package &&
            reopened_model.snapshot().project_id == current_project_id &&
            reopened_model.snapshot().project_valid &&
            reopened_model.show_program() == current_show,
        "corrupt Open must preserve the current valid project+show runtime and path");

  auto incompatible = aeyla::project::make_default_project_document(
      "99999999-aaaa-4bbb-8ccc-333333333333",
      "2026-08-07T04:15:00Z");
  incompatible.fixtures[0].universe = 1U;
  const auto incompatible_path = directory / "incompatible.aeylashow";
  const auto incompatible_saved =
      aeyla::project::save_project_package_atomic(incompatible_path, incompatible);
  check(incompatible_saved.ok(),
        "test fixture must be a package-valid but runtime-incompatible document");

  const auto incompatible_open = reopened.open(incompatible_path);
  check(!incompatible_open.succeeded,
        "Open must reject a package outside Alpha 0.3 runtime scope");
  check(reopened.current_path() == package &&
            reopened_model.snapshot().project_id == current_project_id &&
            reopened_model.snapshot().project_valid &&
            reopened_model.show_program() == current_show &&
            reopened_model.snapshot().blackout &&
            !reopened_model.snapshot().output_armed,
        "runtime-incompatible Open must preserve the current safe valid project+show");

  auto partial_rig = aeyla::project::make_default_project_document(
      "77777777-aaaa-4bbb-8ccc-444444444444",
      "2026-08-07T04:20:00Z");
  partial_rig.fixtures[9].enabled = false;
  const auto partial_rig_path = directory / "partial-rig.aeylashow";
  const auto partial_rig_saved =
      aeyla::project::save_project_package_atomic(partial_rig_path, partial_rig);
  check(partial_rig_saved.ok(),
        "test fixture must be package-valid before runtime rig-mode validation");
  const auto partial_rig_open = reopened.open(partial_rig_path);
  check(!partial_rig_open.succeeded,
        "Open must reject activation states outside canonical Rig 10/Rig 14");
  check(reopened.current_path() == package &&
            reopened_model.snapshot().project_id == current_project_id &&
            reopened_model.snapshot().project_valid &&
            reopened_model.show_program() == current_show,
        "unsupported rig-mode Open must not normalize or replace the current show");

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
