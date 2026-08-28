#include "AeylaTakeLibrarySession.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if(!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
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

  if(failures == 0) {
    std::cout << "All AEYLA Take library session tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
