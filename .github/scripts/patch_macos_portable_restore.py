from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"{label}: expected source pattern not found")
    return text.replace(old, new, 1)


# macOS native chooser: do not ask Cocoa to understand the custom project UTI.
# AEYLA validates the selected file after the chooser returns.
ui = Path("product/AeylaVisualDmx/AeylaRuntimeStatusControl.h")
s = ui.read_text(encoding="utf-8")
s = replace_once(
    s,
    'EFileAction::Open, ".aeylashow",',
    'EFileAction::Open, "",',
    "macOS open chooser",
)
s = replace_once(
    s,
    'EFileAction::Save, ".aeylashow",',
    'EFileAction::Save, "aeylashow",',
    "macOS save chooser",
)
ui.write_text(s, encoding="utf-8")

# On explicit project open, bind a sibling TOMAS_DMX directory immediately.
hdr = Path("product/AeylaVisualDmx/AeylaVisualDmx.h")
h = hdr.read_text(encoding="utf-8")
open_tail = '''    if(status.succeeded)
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
  }

  aeyla::product::ProjectFileStatus SaveProjectFromUI()
'''
open_tail_fixed = '''    if(status.succeeded)
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

    // R10.8.3 field portability. A portable show folder is:
    //   SHOW.aeylashow
    //   TOMAS_DMX/
    // The first successful Open binds that library automatically.
    if(status.succeeded && !path.empty())
    {
      std::error_code libraryError;
      const auto portableLibrary = path.parent_path() / "TOMAS_DMX";
      if(std::filesystem::is_directory(portableLibrary, libraryError) &&
         !libraryError)
      {
        std::string projectId;
        {
          const std::scoped_lock lock(mModelMutex);
          projectId = mModel.project_document().project_id;
        }
        aeyla::take_library_session::ensure_scope(this, projectId);
        aeyla::take_library_session::set_directory(this, portableLibrary);
        aeyla::take_library_session::set_storage_message(
            this, "TOMAS_DMX AUTO-VINCULADA · biblioteca portable lista");
        if(ShowMidiMapping().enabled)
          mMidiPreflightCursor.store(0, std::memory_order_release);
      }
    }
    return status;
  }

  aeyla::product::ProjectFileStatus SaveProjectFromUI()
'''
h = replace_once(h, open_tail, open_tail_fixed, "OpenProject portable library")
hdr.write_text(h, encoding="utf-8")

# Persist/reopen the project locator in Ableton state and fall back to sibling
# TOMAS_DMX when a Windows take-library path cannot resolve on macOS.
cpp = Path("product/AeylaVisualDmx/AeylaVisualDmx.cpp")
c = cpp.read_text(encoding="utf-8")

refresh_point = '''  mHostStateCache.project_schema_minor =
      mModel.project_document().schema_version.minor;
  mHostStateCache.grand_master = snapshot.grand_master;
'''
refresh_fixed = '''  mHostStateCache.project_schema_minor =
      mModel.project_document().schema_version.minor;

  const auto currentProjectPath = mProjectFiles.current_path();
  if(!currentProjectPath.empty())
  {
    const auto utf8 = currentProjectPath.lexically_normal().generic_u8string();
    if(utf8.size() <= aeyla::runtime::kMaxProjectLocatorBytes)
    {
      mHostStateCache.locator_mode =
          aeyla::runtime::ProjectLocatorMode::absolute_development;
      mHostStateCache.project_locator.assign(
          reinterpret_cast<const char*>(utf8.data()), utf8.size());
    }
    else
    {
      mHostStateCache.locator_mode = aeyla::runtime::ProjectLocatorMode::none;
      mHostStateCache.project_locator.clear();
    }
  }
  else
  {
    mHostStateCache.locator_mode = aeyla::runtime::ProjectLocatorMode::none;
    mHostStateCache.project_locator.clear();
  }

  mHostStateCache.grand_master = snapshot.grand_master;
'''
c = replace_once(c, refresh_point, refresh_fixed, "host project locator save")

pending_point = '''  ClearShowMidiCommandsLocked();
  mTakeScheduler.disarm();
  mModel.release_transients();
  mModel.disarm(aeyla::runtime::RuntimeSafetyReason::project_reload);

  std::array<std::uint8_t, 16> currentUuid{};
'''
pending_fixed = '''  ClearShowMidiCommandsLocked();
  mTakeScheduler.disarm();
  mModel.release_transients();
  mModel.disarm(aeyla::runtime::RuntimeSafetyReason::project_reload);

  // Reopen the .aeylashow saved with the Ableton session before identity
  // validation. This prevents a fresh plug-in instance from comparing the
  // saved show UUID against the default blank project and discarding bindings.
  if(pending->locator_mode ==
         aeyla::runtime::ProjectLocatorMode::absolute_development &&
     !pending->project_locator.empty())
  {
    try
    {
      std::u8string locatorUtf8;
      locatorUtf8.reserve(pending->project_locator.size());
      for(const unsigned char byte : pending->project_locator)
        locatorUtf8.push_back(static_cast<char8_t>(byte));
      const auto locator =
          std::filesystem::path(locatorUtf8).lexically_normal();
      std::error_code locatorError;
      if(std::filesystem::is_regular_file(locator, locatorError) &&
         !locatorError)
      {
        const auto reopened = mProjectFiles.open(locator);
        if(reopened.succeeded)
        {
          mLoadedTakeSongIndex.store(-1, std::memory_order_release);
          mActiveTakeSongIndex.store(-1, std::memory_order_release);
          SyncParametersFromProject();
          RefreshOutputBackendFromProjectLocked();
          (void)aeyla::live_memory_session::restore_persistent_state(
              this, mProjectFiles.live_memory_state());
        }
      }
    }
    catch(...)
    {
      // Host restore remains fail-safe; identity validation below will reject
      // an unresolved locator and physical output remains disarmed.
    }
  }

  std::array<std::uint8_t, 16> currentUuid{};
'''
c = replace_once(c, pending_point, pending_fixed, "host project auto reopen")

stage_point = '''  else
  {
    aeyla::take_library_session::stage_persisted_state(
        this, mModel.project_document().project_id,
        pending->take_library_locator, pending->take_bindings);
  }

  mParameterUpdatePending.store(true, std::memory_order_release);
'''
stage_fixed = '''  else
  {
    aeyla::take_library_session::stage_persisted_state(
        this, mModel.project_document().project_id,
        pending->take_library_locator, pending->take_bindings);

    // Cross-platform fallback: a saved C:\\... take path is intentionally not
    // guessed on macOS. If it cannot resolve, use the portable sibling folder
    // and let set_directory() re-apply basename/trim bindings against it.
    if(aeyla::take_library_session::directory(this).empty())
    {
      const auto projectPath = mProjectFiles.current_path();
      if(!projectPath.empty())
      {
        std::error_code libraryError;
        const auto portableLibrary = projectPath.parent_path() / "TOMAS_DMX";
        if(std::filesystem::is_directory(portableLibrary, libraryError) &&
           !libraryError)
        {
          aeyla::take_library_session::set_directory(this, portableLibrary);
          aeyla::take_library_session::set_storage_message(
              this, "TOMAS_DMX AUTO-RESTAURADA · sesión portable lista");
        }
      }
    }
  }

  mParameterUpdatePending.store(true, std::memory_order_release);
'''
c = replace_once(c, stage_point, stage_fixed, "portable take restore fallback")
cpp.write_text(c, encoding="utf-8")

print("R10.8.3 macOS portable restore patch applied")
