#include "AeylaTakeLibrarySession.h"

#include "capture/dmx_take_file_store.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace aeyla::take_library_session {
namespace {

struct SessionState {
  std::string project_id;
  std::filesystem::path directory;
  std::map<std::string, std::filesystem::path> loaded_paths;
  std::map<std::string, TakeEditState> edit_states;
  std::map<std::string, std::string> unavailable_reasons;
  std::string storage_message;
  std::string persisted_library_locator;
  std::vector<aeyla::runtime::SessionTakeBinding> persisted_bindings;
};

std::mutex gMutex;
std::map<const void*, SessionState> gSessions;

SessionState& state_for(const void* owner) {
  return gSessions[owner];
}

std::string PathToUtf8(const std::filesystem::path& path) {
  const auto utf8 = path.generic_u8string();
  return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

bool IsSafeTakeFileName(std::string_view value) {
  constexpr std::string_view suffix = ".aeylatake";
  return !value.empty() && value != "." && value != ".." &&
         value.find('\0') == std::string_view::npos &&
         value.find('/') == std::string_view::npos &&
         value.find('\\') == std::string_view::npos &&
         value.size() >= suffix.size() &&
         value.substr(value.size() - suffix.size()) == suffix;
}

bool IsRawTakeName(std::string_view name) {
  return !name.starts_with("Clip consolidado ") &&
         !name.starts_with("Muestra DMX consolidada ");
}

std::optional<aeyla::runtime::SessionTakeBinding> MakeHostBinding(
    std::string_view songId,
    const std::filesystem::path& path,
    std::uint64_t startFrame,
    std::uint64_t endFrameExclusive,
    std::uint64_t frameCount) {
  if(songId.empty() || path.empty()) return std::nullopt;
  const std::string fileName = PathToUtf8(path.filename());
  if(!IsSafeTakeFileName(fileName)) return std::nullopt;

  aeyla::runtime::SessionTakeBinding binding;
  binding.song_id = std::string(songId);
  binding.file_name = fileName;
  if(frameCount > 0U && startFrame == 0U &&
     endFrameExclusive == frameCount) {
    binding.start_frame = 0U;
    binding.end_frame_exclusive = 0U;
  } else if(endFrameExclusive > startFrame &&
            endFrameExclusive - startFrame >= 2U) {
    binding.start_frame = startFrame;
    binding.end_frame_exclusive = endFrameExclusive;
  } else {
    return std::nullopt;
  }
  return binding;
}

std::optional<aeyla::runtime::SessionTakeBinding> PendingBindingForSong(
    const std::vector<aeyla::runtime::SessionTakeBinding>& bindings,
    std::string_view songId) {
  const auto found = std::find_if(
      bindings.begin(), bindings.end(), [&](const auto& binding) {
        return binding.song_id == songId;
      });
  if(found == bindings.end()) return std::nullopt;
  return *found;
}

void ApplyVersionMetadata(
    TakeEditState& state,
    const std::vector<aeyla::capture::TakeFileIndexEntry>& newestFirst) {
  state.version_count = newestFirst.size();
  state.version_index = 0U;
  for(auto entry = newestFirst.rbegin(); entry != newestFirst.rend(); ++entry) {
    if(entry->path != state.path) continue;
    state.version_index = static_cast<std::size_t>(
        std::distance(newestFirst.rbegin(), entry));
    return;
  }
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
  {
    const std::scoped_lock lock(gMutex);
    auto& state = state_for(owner);
    state.directory = std::move(directory);
    state.loaded_paths.clear();
    state.edit_states.clear();
    state.unavailable_reasons.clear();
    state.storage_message.clear();
  }

  // A manually selected directory is the only cross-platform rebinding step.
  // If host state 1.3 has pending basename/trim bindings, resolve them against
  // this exact directory now; never guess a Windows <-> macOS path mapping.
  (void)restore_persisted_state(owner);
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

void stage_persisted_state(
    const void* owner,
    std::string_view project_id,
    std::string library_locator,
    std::vector<aeyla::runtime::SessionTakeBinding> bindings) {
  ensure_scope(owner, project_id);
  {
    const std::scoped_lock lock(gMutex);
    auto& state = state_for(owner);
    state.directory.clear();
    state.loaded_paths.clear();
    state.edit_states.clear();
    state.unavailable_reasons.clear();
    state.storage_message.clear();
    state.persisted_library_locator = std::move(library_locator);
    state.persisted_bindings = std::move(bindings);
  }
  (void)restore_persisted_state(owner);
}

PersistedRestoreStatus restore_persisted_state(const void* owner) {
  PersistedRestoreStatus status;
  std::string projectId;
  std::filesystem::path candidate;
  std::string savedLocator;
  std::vector<aeyla::runtime::SessionTakeBinding> bindings;
  bool usingSavedLocator = false;
  {
    const std::scoped_lock lock(gMutex);
    const auto session = gSessions.find(owner);
    if(session == gSessions.end()) return status;
    projectId = session->second.project_id;
    candidate = session->second.directory;
    savedLocator = session->second.persisted_library_locator;
    bindings = session->second.persisted_bindings;
  }

  if(candidate.empty() && !savedLocator.empty()) {
    try {
      std::u8string locatorUtf8;
      locatorUtf8.reserve(savedLocator.size());
      for(const unsigned char byte : savedLocator)
        locatorUtf8.push_back(static_cast<char8_t>(byte));
      candidate = std::filesystem::path(locatorUtf8).lexically_normal();
      usingSavedLocator = true;
    } catch(...) {
      candidate.clear();
    }
  }

  std::error_code filesystemError;
  if(candidate.empty() ||
     !std::filesystem::is_directory(candidate, filesystemError) ||
     filesystemError) {
    if(!savedLocator.empty() || !bindings.empty()) {
      const std::scoped_lock lock(gMutex);
      const auto session = gSessions.find(owner);
      if(session != gSessions.end() && session->second.project_id == projectId) {
        session->second.storage_message =
            "BIBLIOTECA GUARDADA NO ENCONTRADA · selecciona la carpeta de tomas una vez";
      }
    }
    return status;
  }
  status.library_resolved = true;

  if(bindings.empty()) {
    if(usingSavedLocator) {
      const std::scoped_lock lock(gMutex);
      const auto session = gSessions.find(owner);
      if(session != gSessions.end() && session->second.project_id == projectId) {
        session->second.directory = candidate;
        session->second.storage_message = "BIBLIOTECA RESTAURADA DESDE EL HOST";
      }
    }
    return status;
  }

  std::map<std::string, std::filesystem::path> restoredPaths;
  std::map<std::string, TakeEditState> restoredEdits;
  std::map<std::string, std::string> unavailable;

  for(const auto& binding : bindings) {
    const auto scan = aeyla::capture::scan_take_directory(candidate,
                                                           binding.song_id);
    if(!scan.ok()) {
      ++status.missing_bindings;
      unavailable[binding.song_id] =
          "No se pudo indexar la biblioteca guardada · " + scan.error;
      continue;
    }
    const auto match = std::find_if(
        scan.entries.begin(), scan.entries.end(), [&](const auto& entry) {
          return PathToUtf8(entry.path.filename()) == binding.file_name;
        });
    if(match == scan.entries.end()) {
      ++status.missing_bindings;
      unavailable[binding.song_id] =
          "La toma guardada no existe en la biblioteca seleccionada";
      continue;
    }

    const bool fullFile = binding.start_frame == 0U &&
                          binding.end_frame_exclusive == 0U;
    const std::uint64_t start = fullFile ? 0U : binding.start_frame;
    const std::uint64_t end = fullFile ? match->frame_count
                                       : binding.end_frame_exclusive;
    if(match->frame_count < 2U || end > match->frame_count ||
       end <= start || end - start < 2U) {
      ++status.missing_bindings;
      unavailable[binding.song_id] =
          "La geometría de la toma guardada ya no coincide con sus trims";
      continue;
    }

    TakeEditState edit;
    edit.path = match->path;
    edit.take_name = match->take_name;
    edit.raw_source = IsRawTakeName(match->take_name);
    edit.start_frame = start;
    edit.end_frame_exclusive = end;
    edit.frame_count = match->frame_count;
    edit.frames_per_second = match->frames_per_second;
    ApplyVersionMetadata(edit, scan.entries);
    restoredPaths[binding.song_id] = match->path;
    restoredEdits[binding.song_id] = std::move(edit);
    ++status.restored_bindings;
  }

  {
    const std::scoped_lock lock(gMutex);
    const auto session = gSessions.find(owner);
    if(session == gSessions.end() || session->second.project_id != projectId)
      return {};
    auto& state = session->second;
    state.directory = candidate;
    state.loaded_paths = std::move(restoredPaths);
    state.edit_states = std::move(restoredEdits);
    state.unavailable_reasons = std::move(unavailable);
    state.storage_message = status.missing_bindings == 0U
        ? "BIBLIOTECA / TOMAS RESTAURADAS · " +
              std::to_string(status.restored_bindings) + " LISTAS · SALIDA DESARMADA"
        : "RESTAURACIÓN PARCIAL · " +
              std::to_string(status.restored_bindings) + " LISTAS · " +
              std::to_string(status.missing_bindings) + " PENDIENTES";
  }
  return status;
}

HostTakeStateSnapshot snapshot_for_host(
    const void* owner,
    const std::vector<std::string>& song_ids) {
  HostTakeStateSnapshot snapshot;
  const std::scoped_lock lock(gMutex);
  const auto session = gSessions.find(owner);
  if(session == gSessions.end()) return snapshot;
  auto& state = session->second;

  if(state.directory.empty()) {
    snapshot.library_locator = state.persisted_library_locator;
    snapshot.bindings = state.persisted_bindings;
    return snapshot;
  }

  snapshot.library_locator = PathToUtf8(state.directory.lexically_normal());
  snapshot.bindings.reserve(std::min(
      song_ids.size(), aeyla::runtime::kMaxSessionTakeBindings));
  for(const auto& songId : song_ids) {
    if(snapshot.bindings.size() >= aeyla::runtime::kMaxSessionTakeBindings)
      break;

    const auto edited = state.edit_states.find(songId);
    if(edited != state.edit_states.end()) {
      const auto binding = MakeHostBinding(
          songId, edited->second.path, edited->second.start_frame,
          edited->second.end_frame_exclusive, edited->second.frame_count);
      if(binding.has_value()) {
        snapshot.bindings.push_back(*binding);
        continue;
      }
    }

    const auto loaded = state.loaded_paths.find(songId);
    if(loaded != state.loaded_paths.end() && !loaded->second.empty()) {
      aeyla::runtime::SessionTakeBinding binding;
      binding.song_id = songId;
      binding.file_name = PathToUtf8(loaded->second.filename());
      if(IsSafeTakeFileName(binding.file_name)) {
        snapshot.bindings.push_back(std::move(binding));
        continue;
      }
    }

    const auto pending = PendingBindingForSong(state.persisted_bindings, songId);
    if(pending.has_value()) snapshot.bindings.push_back(*pending);
  }

  state.persisted_library_locator = snapshot.library_locator;
  state.persisted_bindings = snapshot.bindings;
  return snapshot;
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
