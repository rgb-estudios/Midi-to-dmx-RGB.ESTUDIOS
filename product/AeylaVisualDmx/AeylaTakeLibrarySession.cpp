#include "AeylaTakeLibrarySession.h"

#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace aeyla::take_library_session {
namespace {

struct SessionState {
  std::string project_id;
  std::filesystem::path directory;
  std::map<std::string, std::filesystem::path> loaded_paths;
  std::map<std::string, TakeEditState> edit_states;
  std::map<std::string, std::string> unavailable_reasons;
  std::string storage_message;
};

std::mutex gMutex;
std::map<const void*, SessionState> gSessions;

SessionState& state_for(const void* owner) {
  return gSessions[owner];
}

}  // namespace

void clear(const void* owner) noexcept {
  try {
    const std::scoped_lock lock(gMutex);
    gSessions.erase(owner);
  } catch(...) {
    // Destruction must remain fail-safe even if the platform mutex reports an
    // exceptional condition while the host is unloading the plug-in.
  }
}

void ensure_scope(const void* owner, std::string_view project_id) {
  const std::scoped_lock lock(gMutex);
  auto& state = state_for(owner);
  const std::string next(project_id);
  if(state.project_id == next) return;
  state = {};
  state.project_id = next;
}

void set_directory(const void* owner, std::filesystem::path directory) {
  const std::scoped_lock lock(gMutex);
  auto& state = state_for(owner);
  state.directory = std::move(directory);
  state.loaded_paths.clear();
  state.edit_states.clear();
  state.unavailable_reasons.clear();
  state.storage_message.clear();
}

std::filesystem::path directory(const void* owner) {
  const std::scoped_lock lock(gMutex);
  const auto found = gSessions.find(owner);
  return found == gSessions.end() ? std::filesystem::path{} : found->second.directory;
}

void set_loaded_path(const void* owner, std::string_view song_id,
                     std::filesystem::path path) {
  const std::scoped_lock lock(gMutex);
  auto& state = state_for(owner);
  const std::string key(song_id);
  state.loaded_paths[key] = std::move(path);
  state.unavailable_reasons.erase(key);
}

std::filesystem::path loaded_path(const void* owner,
                                  std::string_view song_id) {
  const std::scoped_lock lock(gMutex);
  const auto session = gSessions.find(owner);
  if(session == gSessions.end()) return {};
  const auto found = session->second.loaded_paths.find(std::string(song_id));
  return found == session->second.loaded_paths.end()
             ? std::filesystem::path{}
             : found->second;
}

void set_edit_state(const void* owner, std::string_view song_id,
                    TakeEditState state) {
  const std::scoped_lock lock(gMutex);
  auto& session = state_for(owner);
  const std::string key(song_id);
  session.edit_states[key] = std::move(state);
  session.unavailable_reasons.erase(key);
}

std::optional<TakeEditState> edit_state(const void* owner,
                                        std::string_view song_id) {
  const std::scoped_lock lock(gMutex);
  const auto session = gSessions.find(owner);
  if(session == gSessions.end()) return std::nullopt;
  const auto found = session->second.edit_states.find(std::string(song_id));
  if(found == session->second.edit_states.end()) return std::nullopt;
  return found->second;
}

void clear_edit_state(const void* owner, std::string_view song_id) noexcept {
  try {
    const std::scoped_lock lock(gMutex);
    const auto session = gSessions.find(owner);
    if(session == gSessions.end()) return;
    session->second.edit_states.erase(std::string(song_id));
  } catch(...) {
  }
}

void set_unavailable_reason(const void* owner, std::string_view song_id,
                            std::string reason) {
  const std::scoped_lock lock(gMutex);
  state_for(owner).unavailable_reasons[std::string(song_id)] =
      std::move(reason);
}

std::optional<std::string> unavailable_reason(
    const void* owner, std::string_view song_id) {
  const std::scoped_lock lock(gMutex);
  const auto session = gSessions.find(owner);
  if(session == gSessions.end()) return std::nullopt;
  const auto found = session->second.unavailable_reasons.find(
      std::string(song_id));
  if(found == session->second.unavailable_reasons.end()) return std::nullopt;
  return found->second;
}

void set_storage_message(const void* owner, std::string message) {
  const std::scoped_lock lock(gMutex);
  state_for(owner).storage_message = std::move(message);
}

std::string storage_message(const void* owner) {
  const std::scoped_lock lock(gMutex);
  const auto found = gSessions.find(owner);
  return found == gSessions.end() ? std::string{} : found->second.storage_message;
}

}  // namespace aeyla::take_library_session
