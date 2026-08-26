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

void set_storage_message(const void* owner, std::string message);
[[nodiscard]] std::string storage_message(const void* owner);

}  // namespace aeyla::take_library_session
