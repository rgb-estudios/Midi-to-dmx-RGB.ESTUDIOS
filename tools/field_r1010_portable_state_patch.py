from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# ---------------------------------------------------------------------------
# .aeylashow: add a small opaque session.bin entry. The DMX recordings remain
# external; this entry carries portable host/session bindings only.
# ---------------------------------------------------------------------------
replace_once(
    "src/project/project_package.h",
    '#include <filesystem>\n#include <optional>\n#include <string>\n#include <vector>\n',
    '#include <cstdint>\n#include <filesystem>\n#include <optional>\n#include <span>\n#include <string>\n#include <vector>\n')
replace_once(
    "src/project/project_package.h",
'''  LiveMemoryPersistentState live_memory_state{};
  bool legacy_project_only{false};
  bool legacy_without_live_memory{false};
  std::vector<ProjectPackageDiagnostic> diagnostics;
''',
'''  LiveMemoryPersistentState live_memory_state{};
  // Opaque, bounded plug-in/session state. The project layer does not interpret
  // this payload; the product plug-in validates it with runtime/plugin_state.
  std::vector<std::uint8_t> portable_session_state{};
  bool legacy_project_only{false};
  bool legacy_without_live_memory{false};
  bool legacy_without_portable_session{false};
  std::vector<ProjectPackageDiagnostic> diagnostics;
''')
replace_once(
    "src/project/project_package.h",
'''// `.aeylashow` package contract:
// - R10.1 current format: deterministic root entries
//   `project.json` + `show.bin` + `live.bin`;
// - previous two-entry project+show packages remain readable and restore an
//   empty/OFF live-memory state;
// - legacy Alpha 0.3 `project.json`-only packages remain readable and migrate to
//   an empty authoring ShowProgram + empty/OFF live-memory state;
// - unknown entries, ZIP64, encryption and asset payloads are rejected;
// - project, show and live-memory state are validated together before publish;
// - live.bin contains configuration only. Runtime level/target, Learn state,
//   LTP serial and physical output ARM are never persisted.
''',
'''// `.aeylashow` package contract:
// - R10.10 current format: deterministic root entries
//   `project.json` + `show.bin` + `live.bin` + `session.bin`;
// - R10.1-R10.9 three-entry packages remain readable; they simply have no
//   portable host/take binding payload;
// - previous two-entry project+show packages remain readable and restore an
//   empty/OFF live-memory state;
// - legacy Alpha 0.3 `project.json`-only packages remain readable and migrate to
//   an empty authoring ShowProgram + empty/OFF live-memory state;
// - unknown entries, ZIP64, encryption and asset payloads are rejected;
// - project, show and live-memory state are validated together before publish;
// - session.bin is bounded to 64 KiB and contains no DMX frames. Physical ARM
//   is not part of the plug-in component state and all restore paths stay safe.
''')
replace_once(
    "src/project/project_package.h",
'''ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state);

// Compatibility overloads write the current three-entry package with an empty
// live-memory state so all newly saved projects migrate forward deterministically.
''',
'''ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state,
    std::span<const std::uint8_t> portable_session_state);

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state);

// Compatibility overloads retain the older three-entry package when no
// portable session payload is supplied.
''')

replace_once(
    "src/project/project_package.cpp",
'''constexpr std::uintmax_t kMaximumArchiveBytes = 9U * 1024U * 1024U;
constexpr std::uint64_t kMaximumProjectJsonBytes = 4U * 1024U * 1024U;
constexpr std::uint64_t kMaximumLiveMemoryBytes = 8192U;
constexpr const char* kProjectEntry = "project.json";
constexpr const char* kShowEntry = "show.bin";
constexpr const char* kLiveEntry = "live.bin";
''',
'''constexpr std::uintmax_t kMaximumArchiveBytes = 9U * 1024U * 1024U;
constexpr std::uint64_t kMaximumProjectJsonBytes = 4U * 1024U * 1024U;
constexpr std::uint64_t kMaximumLiveMemoryBytes = 8192U;
constexpr std::uint64_t kMaximumPortableSessionBytes = 64U * 1024U;
constexpr const char* kProjectEntry = "project.json";
constexpr const char* kShowEntry = "show.bin";
constexpr const char* kLiveEntry = "live.bin";
constexpr const char* kSessionEntry = "session.bin";
''')
replace_once(
    "src/project/project_package.cpp",
'''  const mz_uint file_count = mz_zip_reader_get_num_files(&archive);
  if (file_count < 1U || file_count > 3U) {
    add(result.diagnostics, "validate-entries", source.string(),
        "package must contain project.json, optionally show.bin, and optionally R10.1 live.bin only with show.bin");
    finish();
    return result;
  }

  std::optional<mz_uint> project_index;
  std::optional<mz_uint> show_index;
  std::optional<mz_uint> live_index;
  mz_zip_archive_file_stat project_stat{};
  mz_zip_archive_file_stat show_stat{};
  mz_zip_archive_file_stat live_stat{};
''',
'''  const mz_uint file_count = mz_zip_reader_get_num_files(&archive);
  if (file_count < 1U || file_count > 4U) {
    add(result.diagnostics, "validate-entries", source.string(),
        "package must contain project.json, optionally show.bin, live.bin and bounded session.bin in canonical order");
    finish();
    return result;
  }

  std::optional<mz_uint> project_index;
  std::optional<mz_uint> show_index;
  std::optional<mz_uint> live_index;
  std::optional<mz_uint> session_index;
  mz_zip_archive_file_stat project_stat{};
  mz_zip_archive_file_stat show_stat{};
  mz_zip_archive_file_stat live_stat{};
  mz_zip_archive_file_stat session_stat{};
''')
replace_once(
    "src/project/project_package.cpp",
'''    } else if (name == kLiveEntry) {
      if (live_index.has_value()) {
        add(result.diagnostics, "validate-entry", name,
            "duplicate live.bin entry is not permitted");
        finish();
        return result;
      }
      live_index = index;
      live_stat = stat;
    } else {
      add(result.diagnostics, "validate-entry", name,
          "only project.json, show.bin and live.bin are permitted at the archive root");
      finish();
      return result;
    }
''',
'''    } else if (name == kLiveEntry) {
      if (live_index.has_value()) {
        add(result.diagnostics, "validate-entry", name,
            "duplicate live.bin entry is not permitted");
        finish();
        return result;
      }
      live_index = index;
      live_stat = stat;
    } else if (name == kSessionEntry) {
      if (session_index.has_value()) {
        add(result.diagnostics, "validate-entry", name,
            "duplicate session.bin entry is not permitted");
        finish();
        return result;
      }
      session_index = index;
      session_stat = stat;
    } else {
      add(result.diagnostics, "validate-entry", name,
          "only project.json, show.bin, live.bin and session.bin are permitted at the archive root");
      finish();
      return result;
    }
''')
replace_once(
    "src/project/project_package.cpp",
'''  if (file_count == 3U && (!show_index.has_value() || !live_index.has_value())) {
    add(result.diagnostics, "validate-entry", source.string(),
        "three-entry packages must contain project.json + show.bin + live.bin");
    finish();
    return result;
  }
''',
'''  if (file_count == 3U && (!show_index.has_value() || !live_index.has_value() ||
                           session_index.has_value())) {
    add(result.diagnostics, "validate-entry", source.string(),
        "three-entry packages must contain project.json + show.bin + live.bin");
    finish();
    return result;
  }
  if (file_count == 4U && (!show_index.has_value() || !live_index.has_value() ||
                           !session_index.has_value())) {
    add(result.diagnostics, "validate-entry", source.string(),
        "four-entry packages must contain project.json + show.bin + live.bin + session.bin");
    finish();
    return result;
  }
''')
replace_once(
    "src/project/project_package.cpp",
'''  std::optional<std::vector<std::uint8_t>> live_bytes;
  if (live_index.has_value()) {
    live_bytes = extract_entry(
        archive, *live_index, live_stat, kMaximumLiveMemoryBytes,
        kLiveEntry, result.diagnostics);
    if (!live_bytes.has_value()) {
      finish();
      return result;
    }
  }
  finish();
''',
'''  std::optional<std::vector<std::uint8_t>> live_bytes;
  if (live_index.has_value()) {
    live_bytes = extract_entry(
        archive, *live_index, live_stat, kMaximumLiveMemoryBytes,
        kLiveEntry, result.diagnostics);
    if (!live_bytes.has_value()) {
      finish();
      return result;
    }
  }

  std::optional<std::vector<std::uint8_t>> session_bytes;
  if (session_index.has_value()) {
    session_bytes = extract_entry(
        archive, *session_index, session_stat, kMaximumPortableSessionBytes,
        kSessionEntry, result.diagnostics);
    if (!session_bytes.has_value()) {
      finish();
      return result;
    }
  }
  finish();
''')
replace_once(
    "src/project/project_package.cpp",
'''  result.document = std::move(parsed.document);
  result.show_program = std::move(program);
  result.live_memory_state = std::move(live_state);
  return result;
}

std::optional<std::vector<std::uint8_t>> build_archive(
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state,
    std::vector<ProjectPackageDiagnostic>& diagnostics) {
''',
'''  result.document = std::move(parsed.document);
  result.show_program = std::move(program);
  result.live_memory_state = std::move(live_state);
  if (session_bytes.has_value())
    result.portable_session_state = std::move(*session_bytes);
  else
    result.legacy_without_portable_session = true;
  return result;
}

std::optional<std::vector<std::uint8_t>> build_archive(
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state,
    std::span<const std::uint8_t> portable_session_state,
    std::vector<ProjectPackageDiagnostic>& diagnostics) {
''')
replace_once(
    "src/project/project_package.cpp",
'''  const std::string project_json = serialize_project_document(document);
  mz_zip_archive archive{};
  mz_zip_zero_struct(&archive);
  const std::size_t reserve =
      project_json.size() + encoded_show.bytes.size() + encoded_live.size() + 3072U;
''',
'''  if (portable_session_state.size() > kMaximumPortableSessionBytes) {
    add(diagnostics, "validate-session", kSessionEntry,
        "portable session state exceeds the 64 KiB bound");
    return std::nullopt;
  }

  const std::string project_json = serialize_project_document(document);
  mz_zip_archive archive{};
  mz_zip_zero_struct(&archive);
  const std::size_t reserve =
      project_json.size() + encoded_show.bytes.size() + encoded_live.size() +
      portable_session_state.size() + 4096U;
''')
replace_once(
    "src/project/project_package.cpp",
'''  if (!mz_zip_writer_add_mem(&archive, kLiveEntry,
                             encoded_live.data(), encoded_live.size(),
                             MZ_BEST_COMPRESSION)) {
    add(diagnostics, "add-entry", kLiveEntry, zip_error(archive));
    (void) mz_zip_writer_end(&archive);
    return std::nullopt;
  }

  void* heap = nullptr;
''',
'''  if (!mz_zip_writer_add_mem(&archive, kLiveEntry,
                             encoded_live.data(), encoded_live.size(),
                             MZ_BEST_COMPRESSION)) {
    add(diagnostics, "add-entry", kLiveEntry, zip_error(archive));
    (void) mz_zip_writer_end(&archive);
    return std::nullopt;
  }
  if (!portable_session_state.empty() &&
      !mz_zip_writer_add_mem(&archive, kSessionEntry,
                             portable_session_state.data(),
                             portable_session_state.size(), MZ_BEST_COMPRESSION)) {
    add(diagnostics, "add-entry", kSessionEntry, zip_error(archive));
    (void) mz_zip_writer_end(&archive);
    return std::nullopt;
  }

  void* heap = nullptr;
''')
replace_once(
    "src/project/project_package.cpp",
'''      verified.legacy_without_live_memory ||
      serialize_project_document(*verified.document) != project_json ||
      *verified.show_program != show_program ||
      verified.live_memory_state != live_memory_state) {
    add(diagnostics, "verify-archive", "<generated>",
        "generated package failed deterministic project+show+live read-back verification");
''',
'''      verified.legacy_without_live_memory ||
      serialize_project_document(*verified.document) != project_json ||
      *verified.show_program != show_program ||
      verified.live_memory_state != live_memory_state ||
      verified.portable_session_state !=
          std::vector<std::uint8_t>(portable_session_state.begin(),
                                    portable_session_state.end()) ||
      verified.legacy_without_portable_session != portable_session_state.empty()) {
    add(diagnostics, "verify-archive", "<generated>",
        "generated package failed deterministic project+show+live+session read-back verification");
''')
replace_once(
    "src/project/project_package.cpp",
'''ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state) {
''',
'''ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state,
    std::span<const std::uint8_t> portable_session_state) {
''')
replace_once(
    "src/project/project_package.cpp",
'''  const auto archive = build_archive(
      document, show_program, live_memory_state, result.diagnostics);
''',
'''  const auto archive = build_archive(
      document, show_program, live_memory_state, portable_session_state,
      result.diagnostics);
''')
replace_once(
    "src/project/project_package.cpp",
'''      verified.legacy_without_live_memory ||
      serialize_project_document(*verified.document) !=
          serialize_project_document(document) ||
      *verified.show_program != show_program ||
      verified.live_memory_state != live_memory_state) {
    add(result.diagnostics, "verify-temp", temporary.string(),
        "temporary package failed project+show+live read-back verification");
''',
'''      verified.legacy_without_live_memory ||
      serialize_project_document(*verified.document) !=
          serialize_project_document(document) ||
      *verified.show_program != show_program ||
      verified.live_memory_state != live_memory_state ||
      verified.portable_session_state !=
          std::vector<std::uint8_t>(portable_session_state.begin(),
                                    portable_session_state.end()) ||
      verified.legacy_without_portable_session != portable_session_state.empty()) {
    add(result.diagnostics, "verify-temp", temporary.string(),
        "temporary package failed project+show+live+session read-back verification");
''')
# Add compatibility 4-arg wrapper before existing show-only wrapper.
replace_once(
    "src/project/project_package.cpp",
'''ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program) {
  return save_project_package_atomic(
      target, document, show_program, LiveMemoryPersistentState{});
}
''',
'''ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program,
    const LiveMemoryPersistentState& live_memory_state) {
  return save_project_package_atomic(
      target, document, show_program, live_memory_state,
      std::span<const std::uint8_t>{});
}

ProjectPackageSaveResult save_project_package_atomic(
    const std::filesystem::path& target,
    const ProjectDocument& document,
    const show::ShowProgram& show_program) {
  return save_project_package_atomic(
      target, document, show_program, LiveMemoryPersistentState{});
}
''')

# ---------------------------------------------------------------------------
# ProjectFileController: carry opaque portable session bytes transactionally.
# ---------------------------------------------------------------------------
replace_once(
    "src/product/project_file_controller.h",
    '#include <filesystem>\n#include <string>\n#include <utility>\n#include <vector>\n',
    '#include <cstdint>\n#include <filesystem>\n#include <string>\n#include <utility>\n#include <vector>\n')
replace_once(
    "src/product/project_file_controller.h",
'''  void set_live_memory_state(project::LiveMemoryPersistentState state) {
    live_memory_state_ = std::move(state);
  }

 private:
''',
'''  void set_live_memory_state(project::LiveMemoryPersistentState state) {
    live_memory_state_ = std::move(state);
  }

  [[nodiscard]] const std::vector<std::uint8_t>& portable_session_state()
      const noexcept {
    return portable_session_state_;
  }

  void set_portable_session_state(std::vector<std::uint8_t> state) {
    portable_session_state_ = std::move(state);
  }

 private:
''')
replace_once(
    "src/product/project_file_controller.h",
'''  ProjectFileStatus status_{};
  project::LiveMemoryPersistentState live_memory_state_{};
};
''',
'''  ProjectFileStatus status_{};
  project::LiveMemoryPersistentState live_memory_state_{};
  std::vector<std::uint8_t> portable_session_state_{};
};
''')
replace_once(
    "src/product/project_file_controller.cpp",
'''  current_path_.clear();
  live_memory_state_ = project::LiveMemoryPersistentState{};
  model_.mark_project_unsaved();
''',
'''  current_path_.clear();
  live_memory_state_ = project::LiveMemoryPersistentState{};
  portable_session_state_.clear();
  model_.mark_project_unsaved();
''')
replace_once(
    "src/product/project_file_controller.cpp",
'''  live_memory_state_ = loaded.live_memory_state;
  current_path_ = path;
''',
'''  live_memory_state_ = loaded.live_memory_state;
  portable_session_state_ = std::move(loaded.portable_session_state);
  current_path_ = path;
''')
replace_once(
    "src/product/project_file_controller.cpp",
'''  const auto saved = project::save_project_package_atomic(
      path, document, show_program, live_memory_state_);
''',
'''  const auto saved = project::save_project_package_atomic(
      path, document, show_program, live_memory_state_,
      portable_session_state_);
''')
replace_once(
    "src/product/project_file_controller.cpp",
'''  return publish_success(operation,
                         operation == ProjectFileOperation::save
                             ? "Project, show and live-memory package saved and verified"
                             : "Project, show and live-memory package saved to a new path and verified");
''',
'''  return publish_success(operation,
                         operation == ProjectFileOperation::save
                             ? "Project, show, live-memory and portable session saved and verified"
                             : "Project, show, live-memory and portable session saved to a new path and verified");
''')

# ---------------------------------------------------------------------------
# Plug-in: Save embeds portable host state. Open restores it and resolves takes
# from TOMAS_DMX or the same directory, without restoring physical ARM.
# ---------------------------------------------------------------------------
replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.h",
'''    aeyla::product::ProjectFileStatus status;
    aeyla::project::LiveMemoryPersistentState liveState;
    {
      const std::scoped_lock lock(mModelMutex);
      status = mProjectFiles.open(path);
      if(status.succeeded)
      {
        liveState = mProjectFiles.live_memory_state();
        mLoadedTakeSongIndex.store(-1, std::memory_order_release);
        mActiveTakeSongIndex.store(-1, std::memory_order_release);
        SyncParametersFromProject();
        RefreshOutputBackendFromProjectLocked();
        if(ShowMidiMapping().enabled)
          mMidiPreflightCursor.store(0, std::memory_order_release);
      }
      SyncSnapshotToAtomicsLocked();
    }

    if(status.succeeded)
    {
      const auto restored = aeyla::live_memory_session::restore_persistent_state(
          this, liveState);
      if(!restored.succeeded)
      {
        status.succeeded = false;
        status.message = "Proyecto abierto en modo seguro, pero live.bin no pudo publicarse";
        status.diagnostics.push_back(restored.message);
      }
    }
    return status;
''',
'''    aeyla::product::ProjectFileStatus status;
    aeyla::project::LiveMemoryPersistentState liveState;
    std::vector<std::uint8_t> portableSession;
    std::string projectId;
    {
      const std::scoped_lock lock(mModelMutex);
      status = mProjectFiles.open(path);
      if(status.succeeded)
      {
        liveState = mProjectFiles.live_memory_state();
        portableSession = mProjectFiles.portable_session_state();
        projectId = mModel.project_document().project_id;
        mLoadedTakeSongIndex.store(-1, std::memory_order_release);
        mActiveTakeSongIndex.store(-1, std::memory_order_release);
        SyncParametersFromProject();
        RefreshOutputBackendFromProjectLocked();
        // Establish the new project identity before applying any embedded host
        // bindings. This clears stale bindings from the previous project.
        RefreshHostStateCacheLocked();
      }
      SyncSnapshotToAtomicsLocked();
    }

    if(status.succeeded)
    {
      const auto restored = aeyla::live_memory_session::restore_persistent_state(
          this, liveState);
      if(!restored.succeeded)
      {
        status.succeeded = false;
        status.message = "Proyecto abierto en modo seguro, pero live.bin no pudo publicarse";
        status.diagnostics.push_back(restored.message);
      }
    }

    if(status.succeeded && !portableSession.empty())
    {
      const auto decoded = aeyla::runtime::decode_plugin_component_state(
          portableSession);
      if(!decoded.ok())
      {
        status.succeeded = false;
        status.message = "Proyecto abierto en modo seguro, pero session.bin está dañado";
        status.diagnostics.push_back(
            std::string("session.bin: ") +
            aeyla::runtime::plugin_state_error_name(decoded.error));
        return status;
      }

      std::array<std::uint8_t, 16> currentUuid{};
      {
        const std::scoped_lock stateLock(mHostStateMutex);
        currentUuid = mHostStateCache.project_uuid;
      }
      if(decoded.state.project_uuid != currentUuid)
      {
        status.succeeded = false;
        status.message = "Proyecto abierto en modo seguro, pero session.bin pertenece a otro show";
        status.diagnostics.push_back("session.bin: project UUID mismatch");
        return status;
      }

      // Apply only portable authoring/session fields. Never restore locator
      // paths, blackout state, grand master or physical output authority.
      {
        const std::scoped_lock stateLock(mHostStateMutex);
        mHostStateCache.song_bindings = decoded.state.song_bindings;
        mHostStateCache.show_midi = decoded.state.show_midi;
        mHostStateCache.take_library_locator.clear();
        mHostStateCache.take_bindings = decoded.state.take_bindings;
      }
      mShowMidiMappingPacked.store(
          aeyla::runtime::pack_show_midi_mapping(decoded.state.show_midi),
          std::memory_order_release);
      mShowMidiCaptureStartNote.store(decoded.state.show_midi.capture_start_note,
                                      std::memory_order_release);
      mShowMidiCaptureStopNote.store(decoded.state.show_midi.capture_stop_note,
                                     std::memory_order_release);
      mMidiPreflightCursor.store(decoded.state.show_midi.enabled ? 0 : -1,
                                 std::memory_order_release);

      aeyla::take_library_session::stage_persisted_state(
          this, projectId, {}, decoded.state.take_bindings);

      std::filesystem::path resolvedDirectory;
      const std::array<std::filesystem::path, 2U> candidates{
          path.parent_path() / "TOMAS_DMX", path.parent_path()};
      for(const auto& candidate : candidates)
      {
        std::error_code fsError;
        if(candidate.empty() ||
           !std::filesystem::is_directory(candidate, fsError) || fsError)
          continue;
        const auto scan = aeyla::capture::scan_take_directory(candidate, {});
        if(scan.ok() && !scan.entries.empty())
        {
          resolvedDirectory = candidate;
          break;
        }
      }

      aeyla::take_library_session::PersistedRestoreStatus takeRestore;
      if(!resolvedDirectory.empty())
      {
        aeyla::take_library_session::set_directory(this, resolvedDirectory);
        takeRestore = aeyla::take_library_session::restore_persisted_state(this);
      }

      if(!decoded.state.take_bindings.empty())
      {
        if(resolvedDirectory.empty())
        {
          status.message += " · SESIÓN PORTABLE CARGADA; VINCULA LA CARPETA DE TOMAS UNA VEZ";
        }
        else if(takeRestore.missing_bindings != 0U ||
                takeRestore.restored_bindings != decoded.state.take_bindings.size())
        {
          status.succeeded = false;
          status.message = "Show abierto, pero la restauración DMX portable quedó incompleta";
          status.diagnostics.push_back(
              "Tomas restauradas: " + std::to_string(takeRestore.restored_bindings) +
              " / " + std::to_string(decoded.state.take_bindings.size()) +
              " · faltantes: " + std::to_string(takeRestore.missing_bindings));
          return status;
        }
        else
        {
          status.message += " · SESIÓN PORTABLE / " +
              std::to_string(takeRestore.restored_bindings) +
              " TOMAS DMX RESTAURADAS";
        }
      }

      {
        const std::scoped_lock lock(mModelMutex);
        RefreshHostStateCacheLocked();
        SyncSnapshotToAtomicsLocked();
      }
    }
    return status;
''')

# Save path: capture the exact current Reaper/Ableton host/session bindings into
# session.bin while stripping machine-specific paths and enforcing blackout-safe
# restore semantics.
replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.h",
'''    PrepareProjectForSave();
    const auto status = mProjectFiles.save(
        aeyla::product::current_utc_timestamp());
''',
'''    PrepareProjectForSave();
    RefreshHostStateCacheLocked();
    aeyla::runtime::PluginComponentState portableState;
    {
      const std::scoped_lock stateLock(mHostStateMutex);
      portableState = mHostStateCache;
    }
    portableState.project_checksum.fill(0U);
    portableState.locator_mode = aeyla::runtime::ProjectLocatorMode::none;
    portableState.project_locator.clear();
    portableState.take_library_locator.clear();
    portableState.blackout = true;
    const auto encodedPortable =
        aeyla::runtime::encode_plugin_component_state(portableState);
    if(!encodedPortable.ok())
      return {aeyla::product::ProjectFileOperation::save,
              false,
              mProjectFiles.current_path(),
              "No se pudo construir el estado portable del show",
              {aeyla::runtime::plugin_state_error_name(encodedPortable.error)}};
    mProjectFiles.set_portable_session_state(encodedPortable.bytes);
    const auto status = mProjectFiles.save(
        aeyla::product::current_utc_timestamp());
''')
replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.h",
'''    PrepareProjectForSave();
    const auto status = mProjectFiles.save_as(
        path, aeyla::product::current_utc_timestamp());
''',
'''    PrepareProjectForSave();
    RefreshHostStateCacheLocked();
    aeyla::runtime::PluginComponentState portableState;
    {
      const std::scoped_lock stateLock(mHostStateMutex);
      portableState = mHostStateCache;
    }
    portableState.project_checksum.fill(0U);
    portableState.locator_mode = aeyla::runtime::ProjectLocatorMode::none;
    portableState.project_locator.clear();
    portableState.take_library_locator.clear();
    portableState.blackout = true;
    const auto encodedPortable =
        aeyla::runtime::encode_plugin_component_state(portableState);
    if(!encodedPortable.ok())
      return {aeyla::product::ProjectFileOperation::save_as,
              false,
              mProjectFiles.current_path(),
              "No se pudo construir el estado portable del show",
              {aeyla::runtime::plugin_state_error_name(encodedPortable.error)}};
    mProjectFiles.set_portable_session_state(encodedPortable.bytes);
    const auto status = mProjectFiles.save_as(
        path, aeyla::product::current_utc_timestamp());
''')

replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
    '"RGB ESTUDIOS · SHOW / " + projectName + " · R10.9 PRETEST";',
    '"RGB ESTUDIOS · SHOW / " + projectName + " · R10.10 PORTABLE PRETEST";')

# ---------------------------------------------------------------------------
# Tests: prove 4-entry deterministic round-trip and controller persistence.
# ---------------------------------------------------------------------------
replace_once(
    "tests/test_project_package.cpp",
'''  const auto live_state = make_live_state();

  const auto target = directory / "show.aeylashow";
  const ProjectPackageSaveResult first =
      save_project_package_atomic(target, document, show_program, live_state);
  check(first.ok(), "first project+show+live .aeylashow package save must succeed");
''',
'''  const auto live_state = make_live_state();
  const std::vector<std::uint8_t> portable_session{
      0x41U, 0x45U, 0x59U, 0x4cU, 0x41U, 0x10U, 0x20U, 0x30U};

  const auto target = directory / "show.aeylashow";
  const ProjectPackageSaveResult first =
      save_project_package_atomic(target, document, show_program, live_state,
                                  portable_session);
  check(first.ok(), "first project+show+live+session .aeylashow package save must succeed");
''')
replace_once(
    "tests/test_project_package.cpp",
'''  check(!loaded.legacy_project_only && !loaded.legacy_without_live_memory,
        "current three-entry package must not be marked as legacy");
''',
'''  check(!loaded.legacy_project_only && !loaded.legacy_without_live_memory &&
            !loaded.legacy_without_portable_session,
        "current four-entry package must not be marked as legacy");
''')
replace_once(
    "tests/test_project_package.cpp",
'''  check(loaded.live_memory_state == live_state,
        "package round-trip must preserve sparse live-memory configuration");

  const auto second_target = directory / "show-copy.aeylashow";
  const ProjectPackageSaveResult deterministic =
      save_project_package_atomic(second_target, document, show_program, live_state);
''',
'''  check(loaded.live_memory_state == live_state,
        "package round-trip must preserve sparse live-memory configuration");
  check(loaded.portable_session_state == portable_session,
        "package round-trip must preserve bounded portable session bytes exactly");

  const auto second_target = directory / "show-copy.aeylashow";
  const ProjectPackageSaveResult deterministic =
      save_project_package_atomic(second_target, document, show_program, live_state,
                                  portable_session);
''')
replace_once(
    "tests/test_project_package.cpp",
'''  const ProjectPackageSaveResult replaced =
      save_project_package_atomic(target, replacement, replacement_show,
                                  replacement_live);
''',
'''  const ProjectPackageSaveResult replaced =
      save_project_package_atomic(target, replacement, replacement_show,
                                  replacement_live, portable_session);
''')
replace_once(
    "tests/test_project_package.cpp",
'''            current.show_program->songs.front().name == "Replacement Song" &&
            current.live_memory_state == replacement_live,
        "target must contain replacement project, show and live state together");
''',
'''            current.show_program->songs.front().name == "Replacement Song" &&
            current.live_memory_state == replacement_live &&
            current.portable_session_state == portable_session,
        "target must contain replacement project, show, live and session state together");
''')
replace_once(
    "tests/test_project_package.cpp",
'''            backup_loaded.document->name == document.name &&
            *backup_loaded.show_program == show_program &&
            backup_loaded.live_memory_state == live_state,
        "backup bytes must preserve the previous project+show+live transaction");
''',
'''            backup_loaded.document->name == document.name &&
            *backup_loaded.show_program == show_program &&
            backup_loaded.live_memory_state == live_state &&
            backup_loaded.portable_session_state == portable_session,
        "backup bytes must preserve the previous project+show+live+session transaction");
''')
replace_once(
    "tests/test_project_package.cpp",
'''  check(previous_loaded.ok() && !previous_loaded.legacy_project_only &&
            previous_loaded.legacy_without_live_memory &&
''',
'''  check(previous_loaded.ok() && !previous_loaded.legacy_project_only &&
            previous_loaded.legacy_without_live_memory &&
            previous_loaded.legacy_without_portable_session &&
''')
replace_once(
    "tests/test_project_package.cpp",
'''  check(legacy_loaded.ok() && legacy_loaded.legacy_project_only &&
            legacy_loaded.legacy_without_live_memory &&
''',
'''  check(legacy_loaded.ok() && legacy_loaded.legacy_project_only &&
            legacy_loaded.legacy_without_live_memory &&
            legacy_loaded.legacy_without_portable_session &&
''')
replace_once(
    "tests/test_project_package.cpp",
'''  check(empty_current_loaded.ok() && !empty_current_loaded.legacy_project_only &&
            !empty_current_loaded.legacy_without_live_memory &&
''',
'''  check(empty_current_loaded.ok() && !empty_current_loaded.legacy_project_only &&
            !empty_current_loaded.legacy_without_live_memory &&
            empty_current_loaded.legacy_without_portable_session &&
''')
# Add a bounded-session rejection after wrong extension check.
replace_once(
    "tests/test_project_package.cpp",
'''  const auto wrong_extension = directory / "show.zip";
  check(!save_project_package_atomic(wrong_extension, document, show_program,
                                     live_state).ok(),
        "save must reject a non-.aeylashow extension");

  auto with_asset = document;
''',
'''  const auto wrong_extension = directory / "show.zip";
  check(!save_project_package_atomic(wrong_extension, document, show_program,
                                     live_state).ok(),
        "save must reject a non-.aeylashow extension");

  const std::vector<std::uint8_t> oversized_session(64U * 1024U + 1U, 0x5aU);
  check(!save_project_package_atomic(directory / "oversized-session.aeylashow",
                                     document, show_program, live_state,
                                     oversized_session).ok(),
        "save must reject portable session state beyond 64 KiB");

  auto with_asset = document;
''')

replace_once(
    "tests/test_project_file_controller.cpp",
'''  const auto live_state = make_live_state();
  controller.set_live_memory_state(live_state);
  const auto package = directory / "controller.aeylashow";
''',
'''  const auto live_state = make_live_state();
  const std::vector<std::uint8_t> portable_session{0x01U, 0x02U, 0x03U, 0x04U};
  controller.set_live_memory_state(live_state);
  controller.set_portable_session_state(portable_session);
  const auto package = directory / "controller.aeylashow";
''')
replace_once(
    "tests/test_project_file_controller.cpp",
'''  check(reopened.live_memory_state() == live_state,
        "Open must restore the persisted live-memory configuration DTO");

  const std::string current_project_id = reopened_model.snapshot().project_id;
''',
'''  check(reopened.live_memory_state() == live_state,
        "Open must restore the persisted live-memory configuration DTO");
  check(reopened.portable_session_state() == portable_session,
        "Open must restore the opaque portable session payload exactly");

  const std::string current_project_id = reopened_model.snapshot().project_id;
''')
replace_once(
    "tests/test_project_file_controller.cpp",
'''  const auto current_show = reopened_model.show_program();
  const auto current_live = reopened.live_memory_state();
''',
'''  const auto current_show = reopened_model.show_program();
  const auto current_live = reopened.live_memory_state();
  const auto current_portable = reopened.portable_session_state();
''')
replace_once(
    "tests/test_project_file_controller.cpp",
'''            reopened_model.show_program() == current_show &&
            reopened.live_memory_state() == current_live,
        "corrupt Open must preserve current project+show+live runtime/path state");
''',
'''            reopened_model.show_program() == current_show &&
            reopened.live_memory_state() == current_live &&
            reopened.portable_session_state() == current_portable,
        "corrupt Open must preserve current project+show+live+session runtime/path state");
''')

print("R10.10 portable session-state patch applied")
