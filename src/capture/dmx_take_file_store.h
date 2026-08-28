#pragma once

#include "capture/artnet_capture_worker.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aeyla::capture {

inline constexpr std::string_view kDmxTakeFileExtension = ".aeylatake";
inline constexpr std::uint16_t kDmxTakeFileVersion = 1U;

struct StoredDmxTake {
  std::filesystem::path source_path;
  std::string song_id;
  std::string song_name;
  DmxTake take;
};

struct TakeFileIndexEntry {
  std::filesystem::path path;
  std::string song_id;
  std::string song_name;
  std::string take_name;
  std::string source_ipv4;
  std::uint16_t port_address{0U};
  std::uint16_t frames_per_second{0U};
  std::uint64_t frame_count{0U};
  std::filesystem::file_time_type modified{};
};

struct TakeLibraryScanResult {
  std::vector<TakeFileIndexEntry> entries;
  std::size_t invalid_files{0U};
  std::string error;

  [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

// Verifies that the selected location exists (or can be created) and that the
// process can create/delete a small probe file there. This is intentionally
// performed before recording so an external SSD/USB permission problem is
// discovered before the operator starts a Take.
[[nodiscard]] bool prepare_take_directory(
    const std::filesystem::path& directory, std::string& error_message);

// Generates a portable, collision-resistant filename while leaving the
// directory itself operator-controlled. The file embeds canonical Song metadata
// so moving/renaming the file does not break its identity.
[[nodiscard]] std::filesystem::path make_take_file_path(
    const std::filesystem::path& directory,
    std::string_view song_name,
    std::string_view take_name);

// Writes one Take atomically. The implementation fsyncs/commits the temporary
// file, reads it back through the real decoder (including checksum), then
// renames it into place. Existing targets receive a .bak recovery copy.
[[nodiscard]] bool save_take_file_atomic(
    const std::filesystem::path& target,
    std::string_view song_id,
    std::string_view song_name,
    const DmxTake& take,
    std::string& error_message);

// Fully validates checksum, exact size and bounded metadata/payload before
// returning a Take. Files are portable between Windows and macOS.
[[nodiscard]] std::optional<StoredDmxTake> load_take_file(
    const std::filesystem::path& source,
    std::string& error_message);

// Header-only indexing for responsive library browsing. Loading a selected Take
// still performs the full checksum validation above before playback is allowed.
[[nodiscard]] TakeLibraryScanResult scan_take_directory(
    const std::filesystem::path& directory,
    std::string_view song_id_filter = {});

// Resolves the exact file a streamed capture was configured to create. The
// product must not substitute "the newest" entry because filesystem timestamp
// ties or external files can otherwise bind MTC/trim metadata to another Take.
[[nodiscard]] std::optional<TakeFileIndexEntry> find_take_entry_by_path(
    const TakeLibraryScanResult& scan,
    const std::filesystem::path& expected_path);

}  // namespace aeyla::capture
