#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace aeyla::take_library_session {

void clear(const void* owner) noexcept;

// Prevents a DAW from inheriting a stale folder if it destroys a plug-in
// instance and later reuses the same address for a different AEYLA project.
// Changing project identity clears directory, loaded-path and storage state.
void ensure_scope(const void* owner, std::string_view project_id);

void set_directory(const void* owner, std::filesystem::path directory);
[[nodiscard]] std::filesystem::path directory(const void* owner);

void set_loaded_path(const void* owner, std::string_view song_id,
                     std::filesystem::path path);
[[nodiscard]] std::filesystem::path loaded_path(const void* owner,
                                                 std::string_view song_id);

void set_storage_message(const void* owner, std::string message);
[[nodiscard]] std::string storage_message(const void* owner);

}  // namespace aeyla::take_library_session
