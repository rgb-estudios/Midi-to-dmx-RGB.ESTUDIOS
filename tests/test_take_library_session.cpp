#include "AeylaTakeLibrarySession.h"
#include "capture/dmx_take_file_store.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if(!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

std::filesystem::path make_temp_library() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("aeyla-take-session-" + std::to_string(stamp));
}
}  // namespace

int main() {
  using namespace aeyla::take_library_session;

  int first_owner = 0;
  int second_owner = 0;
  const auto library = std::filesystem::path("take-library-a");
  const auto take = library / "song-a__take-1.aeylatake";

  ensure_scope(&first_owner, "project-a");
  set_directory(&first_owner, library);
  set_loaded_path(&first_owner, "song-a", take);
  set_storage_message(&first_owner, "READY");
  set_unavailable_reason(&first_owner, "song-empty", "SIN TOMA");
  check(unavailable_reason(&first_owner, "song-empty") ==
            std::optional<std::string>("SIN TOMA"),
        "empty Song lookup must be cacheable without filesystem polling");

  TakeEditState edit;
  edit.path = take;
  edit.take_name = "Toma 1";
  edit.version_index = 2U;
  edit.version_count = 3U;
  edit.start_frame = 11U;
  edit.end_frame_exclusive = 99U;
  edit.frame_count = 120U;
  edit.frames_per_second = 44U;
  edit.activity_count = 1U;
  set_edit_state(&first_owner, "song-a", edit);
  set_unavailable_reason(&first_owner, "song-a", "STALE");
  set_loaded_path(&first_owner, "song-a", take);
  check(!unavailable_reason(&first_owner, "song-a").has_value(),
        "loading an exact Take must invalidate a negative lookup");

  ensure_scope(&first_owner, "project-a");
  const auto preserved = edit_state(&first_owner, "song-a");
  check(preserved.has_value() && preserved->version_index == 2U &&
            preserved->version_count == 3U,
        "same project scope must preserve cached Take metadata");

  ensure_scope(&second_owner, "project-a");
  check(directory(&second_owner).empty() &&
            !edit_state(&second_owner, "song-a").has_value(),
        "two plugin owners must never share Take session state");

  ensure_scope(&first_owner, "project-b");
  check(directory(&first_owner).empty() &&
            loaded_path(&first_owner, "song-a").empty() &&
            !edit_state(&first_owner, "song-a").has_value() &&
            !unavailable_reason(&first_owner, "song-empty").has_value() &&
            storage_message(&first_owner).empty(),
        "changing project identity must invalidate every cached Take field");

  set_directory(&first_owner, library);
  set_edit_state(&first_owner, "song-a", edit);
  clear(&first_owner);
  check(directory(&first_owner).empty() &&
            !edit_state(&first_owner, "song-a").has_value(),
        "plugin destruction must be able to remove its complete session");
  clear(&second_owner);

  // Host-state 1.3 gate: save the selected file + trims, destroy the complete
  // in-memory session, then reconstruct it from the exact same library path.
  const auto portableLibrary = make_temp_library();
  std::string error;
  check(aeyla::capture::prepare_take_directory(portableLibrary, error),
        "temporary Take library must be writable: " + error);

  aeyla::capture::DmxTake recorded;
  recorded.name = "Toma 1";
  recorded.port_address = 0U;
  recorded.frames_per_second = 44U;
  recorded.source_ipv4 = "2.0.0.10";
  recorded.frames.resize(12U);
  for(std::size_t index = 0U; index < recorded.frames.size(); ++index)
    recorded.frames[index][0] = static_cast<std::uint8_t>(index + 1U);
  const auto portableTake = aeyla::capture::make_take_file_path(
      portableLibrary, "Song A", "Toma 1");
  check(aeyla::capture::save_take_file_atomic(
            portableTake, "song-a", "Song A", recorded, error),
        "portable Take must save: " + error);

  int persisted_owner = 0;
  ensure_scope(&persisted_owner, "project-persisted");
  set_directory(&persisted_owner, portableLibrary);
  TakeEditState persistedEdit;
  persistedEdit.path = portableTake;
  persistedEdit.take_name = "Toma 1";
  persistedEdit.start_frame = 3U;
  persistedEdit.end_frame_exclusive = 10U;
  persistedEdit.frame_count = 12U;
  persistedEdit.frames_per_second = 44U;
  set_edit_state(&persisted_owner, "song-a", persistedEdit);
  set_loaded_path(&persisted_owner, "song-a", portableTake);

  const std::vector<std::string> songs{"song-a"};
  const auto hostSnapshot = snapshot_for_host(&persisted_owner, songs);
  check(!hostSnapshot.library_locator.empty(),
        "host snapshot must persist the selected Take library locator");
  check(hostSnapshot.bindings.size() == 1U &&
            hostSnapshot.bindings.front().song_id == "song-a" &&
            hostSnapshot.bindings.front().start_frame == 3U &&
            hostSnapshot.bindings.front().end_frame_exclusive == 10U,
        "host snapshot must persist exact Take basename and IN/OUT trims");

  clear(&persisted_owner);
  stage_persisted_state(&persisted_owner, "project-persisted",
                        hostSnapshot.library_locator, hostSnapshot.bindings);
  const auto reopened = edit_state(&persisted_owner, "song-a");
  check(!directory(&persisted_owner).empty() && reopened.has_value() &&
            reopened->path.filename() == portableTake.filename() &&
            reopened->start_frame == 3U &&
            reopened->end_frame_exclusive == 10U,
        "same-machine host reopen must restore library, Take and trims without arming output");

  // Cross-platform gate: an unavailable saved path must not be guessed. The
  // bindings survive while unresolved and are applied only after the operator
  // explicitly selects the real library on the destination machine.
  const auto missingLocator =
      (portableLibrary.parent_path() / "definitely-missing-aeyla-library").generic_string();
  clear(&persisted_owner);
  stage_persisted_state(&persisted_owner, "project-persisted",
                        missingLocator, hostSnapshot.bindings);
  check(directory(&persisted_owner).empty() &&
            !edit_state(&persisted_owner, "song-a").has_value(),
        "unavailable saved locator must remain fail-closed");
  const auto unresolvedSnapshot = snapshot_for_host(&persisted_owner, songs);
  check(unresolvedSnapshot.bindings.size() == 1U &&
            unresolvedSnapshot.bindings.front().file_name ==
                hostSnapshot.bindings.front().file_name,
        "unresolved cross-platform move must preserve pending Take binding");

  set_directory(&persisted_owner, portableLibrary);
  const auto rebound = edit_state(&persisted_owner, "song-a");
  check(rebound.has_value() && rebound->path.filename() == portableTake.filename() &&
            rebound->start_frame == 3U &&
            rebound->end_frame_exclusive == 10U,
        "manual destination-library selection must rebind the persisted Take and trims");

  clear(&persisted_owner);
  std::error_code cleanupError;
  std::filesystem::remove_all(portableLibrary, cleanupError);

  if(failures == 0) {
    std::cout << "All AEYLA Take library session tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
