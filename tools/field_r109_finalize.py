from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one match, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# macOS: do not rely on Cocoa filtering a custom file extension. The selected
# path is validated by OpenProjectFromUI/project package validation instead.
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
    'mDialogFileName, mDialogPath, EFileAction::Open, "aeylashow",',
    'mDialogFileName, mDialogPath, EFileAction::Open, "",')

# Save already appends .aeylashow internally; use the same unfiltered native
# dialog behavior so the custom extension cannot make the file invisible.
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
    'mDialogFileName, mDialogPath, EFileAction::Save, "aeylashow",',
    'mDialogFileName, mDialogPath, EFileAction::Save, "",')

# Cross-platform relink must validate the candidate directory before mutating
# the current library locator. A mistaken empty folder must be a no-op.
replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.h",
'''    {
      const std::scoped_lock lock(mModelMutex);
      aeyla::take_library_session::ensure_scope(
          this, mModel.project_document().project_id);
    }
    aeyla::take_library_session::set_directory(this, directory);
    const auto restored =
        aeyla::take_library_session::restore_persisted_state(this);
    {
      const std::scoped_lock lock(mModelMutex);
      RefreshHostStateCacheLocked();
    }

    const auto scan = aeyla::capture::scan_take_directory(directory, {});
    if(!scan.ok())
      return {false, {}, "VINCULAR TOMAS · " + scan.error};
    if(scan.entries.empty())
      return {false, {}, "VINCULAR TOMAS · la carpeta no contiene archivos .aeylatake válidos"};

    std::string message = "BIBLIOTECA DMX VINCULADA · " +
        std::to_string(scan.entries.size()) + " TOMAS";
    if(restored.restored_bindings > 0U || restored.missing_bindings > 0U)
      message += " · " + std::to_string(restored.restored_bindings) +
          " ASOCIACIONES RESTAURADAS" +
          (restored.missing_bindings == 0U
               ? std::string{}
               : " · " + std::to_string(restored.missing_bindings) +
                     " PENDIENTES");
    return {true, {}, std::move(message)};
''',
'''    // Validate the complete destination before changing the active library.
    // A wrong/empty folder must never discard a working association.
    const auto scan = aeyla::capture::scan_take_directory(directory, {});
    if(!scan.ok())
      return {false, {}, "VINCULAR TOMAS · " + scan.error};
    if(scan.entries.empty())
      return {false, {}, "VINCULAR TOMAS · la carpeta no contiene archivos .aeylatake válidos"};

    {
      const std::scoped_lock lock(mModelMutex);
      aeyla::take_library_session::ensure_scope(
          this, mModel.project_document().project_id);
    }
    // set_directory() restores persisted basename + trim bindings against this
    // exact directory; it never guesses a Windows/macOS path translation.
    aeyla::take_library_session::set_directory(this, directory);
    {
      const std::scoped_lock lock(mModelMutex);
      RefreshHostStateCacheLocked();
    }

    return {true, {}, "BIBLIOTECA DMX VINCULADA · " +
                        std::to_string(scan.entries.size()) + " TOMAS"};
''')

print("R10.9 final safety patch applied")
