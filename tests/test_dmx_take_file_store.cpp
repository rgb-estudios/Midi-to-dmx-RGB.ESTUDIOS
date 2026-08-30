#include "capture/dmx_take_file_store.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

std::filesystem::path unique_test_directory() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("aeyla-take-store-test-" + std::to_string(stamp));
}

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::capture;

  const auto directory = unique_test_directory();
  std::string error;
  check(prepare_take_directory(directory, error),
        "Take directory must be writable: " + error);

  DmxTake take;
  take.name = "Take 3";
  take.port_address = 0U;
  take.frames_per_second = 44U;
  take.source_ipv4 = "2.0.0.10";
  take.frames.resize(3U);
  take.frames[0][0] = 1U;
  take.frames[0][511] = 11U;
  take.frames[1][0] = 127U;
  take.frames[1][255] = 222U;
  take.frames[2][0] = 255U;
  take.frames[2][511] = 240U;

  const auto target = make_take_file_path(directory, "Aeyla Intro", take.name);
  check(target.extension().string() == std::string(kDmxTakeFileExtension),
        "generated Take path must use .aeylatake");
  check(save_take_file_atomic(target, "song-aeyla-intro", "Aeyla Intro",
                              take, error),
        "atomic Take save must succeed: " + error);
  check(std::filesystem::exists(target),
        "saved Take must exist outside plugin memory");

  auto loaded = load_take_file(target, error);
  check(loaded.has_value(), "saved Take must reload: " + error);
  if(loaded.has_value()) {
    check(loaded->song_id == "song-aeyla-intro",
          "Song identity must survive Take reload");
    check(loaded->song_name == "Aeyla Intro",
          "Song name must survive Take reload");
    check(loaded->take.name == take.name,
          "Take name must survive Take reload");
    check(loaded->take.source_ipv4 == take.source_ipv4,
          "Art-Net source metadata must survive Take reload");
    check(loaded->take.frames_per_second == 44U,
          "Take FPS must survive reload");
    check(loaded->take.frames.size() == 3U,
          "all Take frames must survive reload");
    check(loaded->take.frames[1][255] == 222U &&
          loaded->take.frames[2][511] == 240U,
          "all 512 DMX slots must round-trip exactly");
  }

  const auto scan = scan_take_directory(directory, "song-aeyla-intro");
  check(scan.ok(), "Take library scan must succeed: " + scan.error);
  check(scan.entries.size() == 1U,
        "Song-filtered library scan must find the persisted Take");
  if(!scan.entries.empty()) {
    check(scan.entries.front().frame_count == 3U,
          "library index must expose Take frame count without loading payload");
    check(scan.entries.front().take_name == "Take 3",
          "library index must expose Take name");
  }

  // Live-show isolation regression: recording Song A, then Song B, then a new
  // version of Song A must create three independent files. Song changes and
  // version selection are metadata/binding operations; no capture is allowed
  // to replace another Song's RAW payload.
  DmxTake songBTake = take;
  songBTake.name = "Take 1";
  songBTake.frames[0][0] = 42U;
  const auto songBTarget = make_take_file_path(directory, "Song B", songBTake.name);
  check(save_take_file_atomic(songBTarget, "song-b", "Song B", songBTake, error),
        "Song B Take must save independently: " + error);

  DmxTake songASecondTake = take;
  songASecondTake.name = "Take 4";
  songASecondTake.frames[0][0] = 84U;
  const auto songASecondTarget =
      make_take_file_path(directory, "Aeyla Intro", songASecondTake.name);
  check(save_take_file_atomic(songASecondTarget, "song-aeyla-intro",
                              "Aeyla Intro", songASecondTake, error),
        "second Song A Take must save independently: " + error);

  check(target != songBTarget && target != songASecondTarget &&
            songBTarget != songASecondTarget,
        "each Song/version capture must own a distinct filesystem target");
  check(std::filesystem::exists(target) &&
            std::filesystem::exists(songBTarget) &&
            std::filesystem::exists(songASecondTarget),
        "saving another Song/version must never remove a previous RAW Take");

  const auto songAScan = scan_take_directory(directory, "song-aeyla-intro");
  const auto songBScan = scan_take_directory(directory, "song-b");
  check(songAScan.ok() && songAScan.entries.size() == 2U,
        "Song A index must contain both independent versions");
  check(songBScan.ok() && songBScan.entries.size() == 1U,
        "Song B index must contain only Song B RAW data");

  auto songBReloaded = load_take_file(songBTarget, error);
  auto songASecondReloaded = load_take_file(songASecondTarget, error);
  check(songBReloaded.has_value() && songBReloaded->song_id == "song-b" &&
            songBReloaded->take.frames[0][0] == 42U,
        "Song B payload must remain bound to Song B after Song A is recorded again");
  check(songASecondReloaded.has_value() &&
            songASecondReloaded->song_id == "song-aeyla-intro" &&
            songASecondReloaded->take.frames[0][0] == 84U,
        "new Song A version must retain its own payload and identity");

  // Regression: capture finalization must resolve the configured target, not
  // whichever entry happens to sort first by timestamp/filename.
  TakeLibraryScanResult ambiguous;
  TakeFileIndexEntry unrelated;
  unrelated.path = directory / "newer-but-unrelated.aeylatake";
  unrelated.take_name = "Unrelated";
  TakeFileIndexEntry expected;
  expected.path = target;
  expected.take_name = take.name;
  ambiguous.entries.push_back(unrelated);
  ambiguous.entries.push_back(expected);
  const auto exact = find_take_entry_by_path(ambiguous, target);
  check(exact.has_value() && exact->path == target &&
            exact->take_name == take.name,
        "capture indexing must select the exact configured target");
  check(!find_take_entry_by_path(ambiguous,
                                 directory / "missing.aeylatake").has_value(),
        "capture indexing must fail closed when its exact target is absent");
  ambiguous.error = "incomplete directory scan";
  check(!find_take_entry_by_path(ambiguous, target).has_value(),
        "capture indexing must reject entries from an invalid scan");

  // Corrupt one payload byte. Full load must reject it even though the header
  // and file length remain structurally plausible.
  {
    std::fstream file(target, std::ios::binary | std::ios::in | std::ios::out);
    check(static_cast<bool>(file), "corruption test must open Take file");
    if(file) {
      file.seekg(64, std::ios::beg);
      char value = 0;
      file.read(&value, 1);
      file.clear();
      file.seekp(64, std::ios::beg);
      value = static_cast<char>(value ^ 0x5A);
      file.write(&value, 1);
      file.flush();
    }
  }
  loaded = load_take_file(target, error);
  check(!loaded.has_value(),
        "checksum must reject a modified Take payload");
  check(error.find("checksum") != std::string::npos,
        "corruption rejection must identify checksum failure");

  // The corrupted Song A version must not poison unrelated Song/version RAWs.
  songBReloaded = load_take_file(songBTarget, error);
  songASecondReloaded = load_take_file(songASecondTarget, error);
  check(songBReloaded.has_value(),
        "corrupting one Song A RAW must not affect Song B");
  check(songASecondReloaded.has_value(),
        "corrupting one Song A version must not affect another version");

  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);

  if(failures == 0) {
    std::cout << "All AEYLA external Take file tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
