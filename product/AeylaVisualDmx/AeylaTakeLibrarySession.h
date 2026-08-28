#pragma once

#include "capture/dmx_take_activity.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace aeyla::take_library_session {

struct TakeEditState {
  std::filesystem::path path;
  std::string take_name;
  bool raw_source{true};
  // Cached library position. Keeping this beside the edit state avoids
  // enumerating every Take file again on each 30 Hz UI redraw.
  std::size_t version_index{0U};
  std::size_t version_count{0U};
  std::uint64_t start_frame{0U};
  std::uint64_t end_frame_exclusive{0U};
  std::uint64_t frame_count{0U};
  std::uint16_t frames_per_second{0U};
  std::array<std::uint8_t, aeyla::capture::kMaximumTakeActivityBuckets>
      activity_level{};
  std::array<std::uint8_t, aeyla::capture::kMaximumTakeActivityBuckets>
      activity_motion{};
  std::size_t activity_count{0U};
};

void clear(const void* owner) noexcept;

// Evita heredar rutas/ediciones de otra instancia si el host reutiliza la
// dirección de memoria de un plugin destruido.
void ensure_scope(const void* owner, std::string_view project_id);

void set_directory(const void* owner, std::filesystem::path directory);
[[nodiscard]] std::filesystem::path directory(const void* owner);

void set_loaded_path(const void* owner, std::string_view song_id,
                     std::filesystem::path path);
[[nodiscard]] std::filesystem::path loaded_path(const void* owner,
                                                 std::string_view song_id);

void set_edit_state(const void* owner, std::string_view song_id,
                    TakeEditState state);
[[nodiscard]] std::optional<TakeEditState> edit_state(
    const void* owner, std::string_view song_id);
void clear_edit_state(const void* owner, std::string_view song_id) noexcept;

// Caches an unavailable/empty lookup so a Song without Takes does not
// enumerate the library twice per UI frame. Loading or setting an edit state
// invalidates this marker automatically.
void set_unavailable_reason(const void* owner, std::string_view song_id,
                            std::string reason);
[[nodiscard]] std::optional<std::string> unavailable_reason(
    const void* owner, std::string_view song_id);

void set_storage_message(const void* owner, std::string message);
[[nodiscard]] std::string storage_message(const void* owner);

}  // namespace aeyla::take_library_session
