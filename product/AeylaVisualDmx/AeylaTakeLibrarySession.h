#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace aeyla::take_library_session {

void clear(const void* owner) noexcept;

void set_directory(const void* owner, std::filesystem::path directory);
[[nodiscard]] std::filesystem::path directory(const void* owner);

void set_loaded_path(const void* owner, std::string_view song_id,
                     std::filesystem::path path);
[[nodiscard]] std::filesystem::path loaded_path(const void* owner,
                                                 std::string_view song_id);

void set_storage_message(const void* owner, std::string message);
[[nodiscard]] std::string storage_message(const void* owner);

}  // namespace aeyla::take_library_session
