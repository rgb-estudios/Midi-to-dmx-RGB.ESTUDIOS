from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one match, got {count}\n--- needle ---\n{old}")
    p.write_text(text.replace(old, new, 1))


def replace_all_exact(path, old, new, expected):
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != expected:
        raise SystemExit(f"{path}: expected {expected} matches, got {count}\n--- needle ---\n{old}")
    p.write_text(text.replace(old, new))


# ---------- Versioned host state: 1.7 ----------
replace_once(
    'src/runtime/plugin_state.h',
    'inline constexpr std::uint16_t kPluginStateFormatMinor = 6;',
    'inline constexpr std::uint16_t kPluginStateFormatMinor = 7;')

replace_once(
    'src/runtime/plugin_state.h',
    '// Authoritative VST3 component state. Output Arm is deliberately absent: every\n// instantiate/restore path starts disarmed regardless of previously saved UI.\nstruct PluginComponentState {',
    '// Authoritative VST3 component state. R10.14 persists the operator\n// recovery intent used by the DAW session: a show saved armed may reopen armed\n// only after project, take bindings, backend and host heartbeat validate again.\nstruct PluginComponentState {')

replace_once(
    'src/runtime/plugin_state.h',
    '  float grand_master{1.0F};\n  bool blackout{true};\n  ProjectLocatorMode locator_mode{ProjectLocatorMode::none};',
    '  float grand_master{1.0F};\n  bool blackout{true};\n  bool restore_output_armed{false};\n  bool restore_take_output_armed{false};\n  std::uint8_t restore_take_song_index{255U};\n  ProjectLocatorMode locator_mode{ProjectLocatorMode::none};')

replace_once(
    'src/runtime/plugin_state.h',
    '  invalid_song_binding,\n  invalid_show_midi_mapping,\n  invalid_take_binding',
    '  invalid_song_binding,\n  invalid_show_midi_mapping,\n  invalid_recovery_state,\n  invalid_take_binding')

replace_once(
    'src/runtime/plugin_state.cpp',
    "  if (state.take_library_locator.size() > kMaxTakeLibraryLocatorBytes ||\n      state.take_library_locator.find('\\0') != std::string::npos)\n    return PluginStateError::invalid_take_binding;",
    "  if (state.restore_output_armed && state.restore_take_output_armed)\n    return PluginStateError::invalid_recovery_state;\n  if (state.restore_take_output_armed) {\n    if (state.restore_take_song_index >= kShowMidiSongCapacity)\n      return PluginStateError::invalid_recovery_state;\n  } else if (state.restore_take_song_index != 255U) {\n    return PluginStateError::invalid_recovery_state;\n  }\n  if (state.take_library_locator.size() > kMaxTakeLibraryLocatorBytes ||\n      state.take_library_locator.find('\\0') != std::string::npos)\n    return PluginStateError::invalid_take_binding;")

replace_once(
    'src/runtime/plugin_state.cpp',
    '      kFixedPayloadSize + locator_size + binding_bytes + 8U +\n      take_binding_bytes + 2U + kShowMidiSongCapacity);',
    '      kFixedPayloadSize + locator_size + binding_bytes + 8U +\n      take_binding_bytes + 2U + kShowMidiSongCapacity + 2U);')

replace_once(
    'src/runtime/plugin_state.cpp',
    '    // State 1.5: arbitrary direct launch note per song.\n    result.bytes.insert(result.bytes.end(), state.song_launch_notes.begin(),\n                        state.song_launch_notes.end());',
    '    // State 1.5: arbitrary direct launch note per song.\n    result.bytes.insert(result.bytes.end(), state.song_launch_notes.begin(),\n                        state.song_launch_notes.end());\n    // State 1.7: exact DAW-session recovery intent. The physical authority is\n    // never trusted blindly on restore; runtime validation must re-establish it.\n    std::uint8_t recovery_flags = 0U;\n    if (state.restore_output_armed) recovery_flags |= 0x01U;\n    if (state.restore_take_output_armed) recovery_flags |= 0x02U;\n    result.bytes.push_back(recovery_flags);\n    result.bytes.push_back(state.restore_take_song_index);')

replace_once(
    'src/runtime/plugin_state.cpp',
    '  if (format_minor >= 5U) {\n    if (offset + kShowMidiSongCapacity > payload_end) {\n      result.error = PluginStateError::invalid_show_midi_mapping;\n      return result;\n    }\n    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),\n                kShowMidiSongCapacity, result.state.song_launch_notes.begin());\n    offset += kShowMidiSongCapacity;\n  } else if (format_minor >= 2U) {\n    // R10.12 migration is intentionally fail-safe: old contiguous banks\n    // become unassigned so every real song is learned explicitly.\n    result.state.song_launch_notes.fill(255U);\n  }\n\n  // Same-major future minor versions may append fields inside payload_size.',
    '  if (format_minor >= 5U) {\n    if (offset + kShowMidiSongCapacity > payload_end) {\n      result.error = PluginStateError::invalid_show_midi_mapping;\n      return result;\n    }\n    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),\n                kShowMidiSongCapacity, result.state.song_launch_notes.begin());\n    offset += kShowMidiSongCapacity;\n  } else if (format_minor >= 2U) {\n    // R10.12 migration is intentionally fail-safe: old contiguous banks\n    // become unassigned so every real song is learned explicitly.\n    result.state.song_launch_notes.fill(255U);\n  }\n\n  if (format_minor >= 7U) {\n    if (offset + 2U > payload_end) {\n      result.error = PluginStateError::invalid_recovery_state;\n      return result;\n    }\n    const std::uint8_t recovery_flags = bytes[offset++];\n    if ((recovery_flags & ~0x03U) != 0U) {\n      result.error = PluginStateError::invalid_recovery_state;\n      return result;\n    }\n    result.state.restore_output_armed = (recovery_flags & 0x01U) != 0U;\n    result.state.restore_take_output_armed = (recovery_flags & 0x02U) != 0U;\n    result.state.restore_take_song_index = bytes[offset++];\n  } else {\n    result.state.restore_output_armed = false;\n    result.state.restore_take_output_armed = false;\n    result.state.restore_take_song_index = 255U;\n  }\n\n  // Same-major future minor versions may append fields inside payload_size.')

replace_once(
    'src/runtime/plugin_state.cpp',
    '    case PluginStateError::invalid_show_midi_mapping: return "invalid_show_midi_mapping";\n    case PluginStateError::invalid_take_binding: return "invalid_take_binding";',
    '    case PluginStateError::invalid_show_midi_mapping: return "invalid_show_midi_mapping";\n    case PluginStateError::invalid_recovery_state: return "invalid_recovery_state";\n    case PluginStateError::invalid_take_binding: return "invalid_take_binding";')

# ---------- State tests ----------
replace_once(
    'tests/test_plugin_state.cpp',
    '  check(decoded.state.take_library_locator == state.take_library_locator &&\n            decoded.state.take_bindings == state.take_bindings,\n        "take library selection and trims must survive host-state round trip");\n\n  {\n    auto learned = state;',
    '  check(decoded.state.take_library_locator == state.take_library_locator &&\n            decoded.state.take_bindings == state.take_bindings,\n        "take library selection and trims must survive host-state round trip");\n\n  {\n    auto recovery = state;\n    recovery.blackout = false;\n    recovery.restore_take_output_armed = true;\n    recovery.restore_take_song_index = 1U;\n    const auto recoveryBytes = aeyla::runtime::encode_plugin_component_state(recovery);\n    check(recoveryBytes.ok(), "armed DAW recovery state must encode");\n    const auto recoveryRoundTrip =\n        aeyla::runtime::decode_plugin_component_state(recoveryBytes.bytes);\n    check(recoveryRoundTrip.ok() && recoveryRoundTrip.state == recovery,\n          "armed DAW recovery state must survive exact round trip");\n\n    auto conflictingAuthority = recovery;\n    conflictingAuthority.restore_output_armed = true;\n    check(aeyla::runtime::encode_plugin_component_state(conflictingAuthority).error ==\n              PluginStateError::invalid_recovery_state,\n          "model and Take authorities may not both auto-restore");\n\n    auto missingSong = recovery;\n    missingSong.restore_take_song_index = 255U;\n    check(aeyla::runtime::encode_plugin_component_state(missingSong).error ==\n              PluginStateError::invalid_recovery_state,\n          "armed Take recovery requires a concrete Song slot");\n\n    auto staleSongWithoutArm = state;\n    staleSongWithoutArm.restore_take_song_index = 2U;\n    check(aeyla::runtime::encode_plugin_component_state(staleSongWithoutArm).error ==\n              PluginStateError::invalid_recovery_state,\n          "disarmed recovery may not carry a stale Take Song slot");\n  }\n\n  {\n    auto learned = state;')

# ---------- Product runtime declarations ----------
replace_once(
    'product/AeylaVisualDmx/AeylaVisualDmx.h',
    '  void ApplyPendingHostStateLocked();\n  void ApplyPendingParameterStateLocked();\n  void DrainHostEventsLocked();',
    '  void ApplyPendingHostStateLocked();\n  void ApplyPendingParameterStateLocked();\n  void RestoreSavedDawSessionLocked(\n      const aeyla::runtime::HostTransportSnapshot& host);\n  void DrainHostEventsLocked();')

replace_once(
    'product/AeylaVisualDmx/AeylaVisualDmx.h',
    '  aeyla::runtime::PluginComponentState mHostStateCache{};\n  std::optional<aeyla::runtime::PluginComponentState> mPendingHostState;\n  std::optional<aeyla::runtime::ShowMidiEvent> mPendingShowMidiEvent;',
    '  aeyla::runtime::PluginComponentState mHostStateCache{};\n  std::optional<aeyla::runtime::PluginComponentState> mPendingHostState;\n  // R10.14: DAW Save/Save As may explicitly request restoration of the exact\n  // operator authority state after a crash/reopen. These fields are consumed\n  // only after host heartbeat, project, backend and Take bindings validate.\n  bool mDawSessionRecoveryPending{false};\n  bool mDawRestoreModelArm{false};\n  bool mDawRestoreTakeArm{false};\n  std::uint8_t mDawRestoreTakeSongIndex{255U};\n  std::optional<aeyla::runtime::ShowMidiEvent> mPendingShowMidiEvent;')

# Portable .aeylashow remains safe across machines; only the DAW host state
# may remember physical authority for crash recovery.
replace_all_exact(
    'product/AeylaVisualDmx/AeylaVisualDmx.h',
    '    portableState.blackout = true;\n    const auto encodedPortable =',
    '    portableState.blackout = true;\n    portableState.restore_output_armed = false;\n    portableState.restore_take_output_armed = false;\n    portableState.restore_take_song_index = 255U;\n    const auto encodedPortable =',
    2)

# ---------- Host serialization captures live authority atomically ----------
replace_once(
    'product/AeylaVisualDmx/AeylaVisualDmx.cpp',
    '    {\n      const std::scoped_lock lock(mHostStateMutex);\n      state = mHostStateCache;\n    }\n\n    const auto encoded = aeyla::runtime::encode_plugin_component_state(state);',
    '    {\n      const std::scoped_lock lock(mHostStateMutex);\n      state = mHostStateCache;\n    }\n\n    // DAW Save/Save As is the crash-recovery checkpoint. Snapshot physical\n    // authority at serialization time instead of depending on the next 8 ms\n    // runtime refresh, so an immediate save after ARM/APAGÓN is exact.\n    state.blackout = GlobalBlackout();\n    state.restore_output_armed = OutputArmed();\n    const int loadedTakeSong = mLoadedTakeSongIndex.load(std::memory_order_acquire);\n    const bool validLoadedTakeSong = loadedTakeSong >= 0 &&\n        loadedTakeSong < static_cast<int>(aeyla::runtime::kShowMidiSongCapacity);\n    state.restore_take_output_armed = TakeOutputArmed() && validLoadedTakeSong;\n    state.restore_take_song_index = state.restore_take_output_armed\n        ? static_cast<std::uint8_t>(loadedTakeSong) : 255U;\n\n    const auto encoded = aeyla::runtime::encode_plugin_component_state(state);')

# Runtime consumes the saved authority after parameter restoration and before MIDI commands.
replace_once(
    'product/AeylaVisualDmx/AeylaVisualDmx.cpp',
    '    if(!host.rendering_offline)\n      DrainShowMidiCommandsLocked(host);',
    '    if(!host.rendering_offline)\n    {\n      RestoreSavedDawSessionLocked(host);\n      DrainShowMidiCommandsLocked(host);\n    }')

# Do not falsely reject a fresh DAW restore merely because the new instance has
# not yet populated a checksum cache.
replace_once(
    'product/AeylaVisualDmx/AeylaVisualDmx.cpp',
    '  const bool checksumMismatch = !IsZero(pending->project_checksum) &&\n                                pending->project_checksum != previousCache.project_checksum;',
    '  const bool checksumMismatch = !IsZero(pending->project_checksum) &&\n                                !IsZero(previousCache.project_checksum) &&\n                                pending->project_checksum != previousCache.project_checksum;')

replace_once(
    'product/AeylaVisualDmx/AeylaVisualDmx.cpp',
    '  if(uuidMismatch || schemaMismatch || checksumMismatch)\n  {\n    mModel.set_project_valid(false);',
    '  if(uuidMismatch || schemaMismatch || checksumMismatch)\n  {\n    mDawSessionRecoveryPending = false;\n    mDawRestoreModelArm = false;\n    mDawRestoreTakeArm = false;\n    mDawRestoreTakeSongIndex = 255U;\n    mModel.set_project_valid(false);')

replace_once(
    'product/AeylaVisualDmx/AeylaVisualDmx.cpp',
    '  else\n  {\n    aeyla::take_library_session::stage_persisted_state(\n        this, mModel.project_document().project_id,\n        pending->take_library_locator, pending->take_bindings);\n  }\n\n  mParameterUpdatePending.store(true, std::memory_order_release);',
    '  else\n  {\n    aeyla::take_library_session::stage_persisted_state(\n        this, mModel.project_document().project_id,\n        pending->take_library_locator, pending->take_bindings);\n\n    // Restore the operator latch exactly as saved. Physical ARM itself is\n    // deferred until the library/backend/host heartbeat are all verified.\n    mParamBlackout.store(pending->blackout, std::memory_order_release);\n    mModel.set_blackout(pending->blackout);\n    mArtNetOutput.set_blackout_latched(pending->blackout);\n    mDawRestoreModelArm = pending->restore_output_armed;\n    mDawRestoreTakeArm = pending->restore_take_output_armed;\n    mDawRestoreTakeSongIndex = pending->restore_take_song_index;\n    mDawSessionRecoveryPending = mDawRestoreModelArm || mDawRestoreTakeArm;\n    SetShowMidiMessage(mDawSessionRecoveryPending\n        ? "SESIÓN DAW RESTAURADA · validando tomas/red antes de recuperar ARM"\n        : "SESIÓN DAW RESTAURADA · tomas y mapa listos · salida desarmada");\n  }\n\n  mParameterUpdatePending.store(true, std::memory_order_release);')

# Snapshot recovery fields continuously once startup recovery has completed.
replace_once(
    'product/AeylaVisualDmx/AeylaVisualDmx.cpp',
    '  auto hostTakeState =\n      aeyla::take_library_session::snapshot_for_host(this, songIds);\n\n  const std::scoped_lock lock(mHostStateMutex);',
    '  auto hostTakeState =\n      aeyla::take_library_session::snapshot_for_host(this, songIds);\n  const auto takeSchedulerStatus = mTakeScheduler.status();\n  const int loadedTakeSong = mLoadedTakeSongIndex.load(std::memory_order_acquire);\n\n  const std::scoped_lock lock(mHostStateMutex);')

replace_once(
    'product/AeylaVisualDmx/AeylaVisualDmx.cpp',
    '  mHostStateCache.grand_master = snapshot.grand_master;\n  mHostStateCache.blackout = snapshot.global_blackout;\n  mHostStateCache.take_library_locator =',
    '  mHostStateCache.grand_master = snapshot.grand_master;\n  mHostStateCache.blackout = snapshot.global_blackout;\n  if(!mDawSessionRecoveryPending)\n  {\n    mHostStateCache.restore_output_armed = snapshot.output_armed;\n    const bool validLoadedTakeSong = loadedTakeSong >= 0 &&\n        loadedTakeSong < static_cast<int>(aeyla::runtime::kShowMidiSongCapacity);\n    mHostStateCache.restore_take_output_armed =\n        takeSchedulerStatus.armed && validLoadedTakeSong;\n    mHostStateCache.restore_take_song_index =\n        mHostStateCache.restore_take_output_armed\n            ? static_cast<std::uint8_t>(loadedTakeSong) : 255U;\n  }\n  mHostStateCache.take_library_locator =')

# Actual restore helper. It prepares the saved Take at IN and ARM, but never calls play().
marker = 'void AeylaVisualDmx::ApplyPendingParameterStateLocked()\n{'
p = Path('product/AeylaVisualDmx/AeylaVisualDmx.cpp')
text = p.read_text()
if text.count(marker) != 1:
    raise SystemExit('AeylaVisualDmx.cpp: ApplyPendingParameterStateLocked marker mismatch')
helper = r'''void AeylaVisualDmx::RestoreSavedDawSessionLocked(
    const aeyla::runtime::HostTransportSnapshot& host)
{
  if(!mDawSessionRecoveryPending || host.rendering_offline || host.revision == 0U)
    return;

  const auto snapshot = mModel.snapshot();
  if(!RuntimeHealthy() || !snapshot.project_valid || !snapshot.backend_ready)
    return;

  const auto fail_recovery = [&](std::string message) {
    mTakeScheduler.disarm();
    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::project_reload);
    mDawSessionRecoveryPending = false;
    mDawRestoreModelArm = false;
    mDawRestoreTakeArm = false;
    mDawRestoreTakeSongIndex = 255U;
    SyncSnapshotToAtomicsLocked();
    SetShowMidiMessage("RECUPERACIÓN DAW BLOQUEADA · " + std::move(message));
  };

  if(mDawRestoreTakeArm)
  {
    const auto& show = mModel.show_program();
    const std::size_t songIndex = mDawRestoreTakeSongIndex;
    if(songIndex >= show.songs.size() ||
       songIndex >= aeyla::runtime::kShowMidiSongCapacity)
    {
      fail_recovery("la canción armada guardada ya no existe");
      return;
    }

    const auto& song = show.songs[songIndex];
    aeyla::take_library_session::ensure_scope(
        this, mModel.project_document().project_id);
    (void)aeyla::take_library_session::restore_persisted_state(this);
    const auto library = aeyla::take_library_session::directory(this);
    if(library.empty())
    {
      fail_recovery("la biblioteca DMX guardada no está disponible");
      return;
    }

    const auto scan = aeyla::capture::scan_take_directory(library, song.song_id);
    if(!scan.ok() || scan.entries.empty())
    {
      fail_recovery(scan.ok()
          ? "la canción armada no tiene una toma DMX"
          : "no se pudo leer la biblioteca · " + scan.error);
      return;
    }

    const auto restoredPath = aeyla::take_library_session::loaded_path(this, song.song_id);
    auto selected = scan.entries.end();
    if(!restoredPath.empty())
      selected = std::find_if(scan.entries.begin(), scan.entries.end(),
          [&](const auto& entry) { return entry.path == restoredPath; });
    if(selected == scan.entries.end() && scan.entries.size() == 1U)
      selected = scan.entries.begin();
    if(selected == scan.entries.end())
    {
      fail_recovery("la toma exacta guardada no pudo resolverse; no se eligió otra automáticamente");
      return;
    }

    const auto outputUniverse = mModel.project_document().output.universe;
    if(selected->port_address != outputUniverse)
    {
      fail_recovery("el universo de la toma guardada no coincide con la salida del proyecto");
      return;
    }

    const auto edited = aeyla::take_library_session::edit_state(this, song.song_id);
    const std::uint64_t expectedStart = edited.has_value() && edited->path == selected->path
        ? edited->start_frame : 0U;
    const std::uint64_t expectedEnd = edited.has_value() && edited->path == selected->path
        ? edited->end_frame_exclusive : selected->frame_count;

    std::string error;
    mTakeScheduler.disarm();
    mTakeScheduler.stop_reset();
    mTakeScheduler.attach(&mArtNetOutput, &mHostTransport);
    if(!mTakeScheduler.load_take_file(selected->path, GetSampleRate(), error))
    {
      fail_recovery("la toma guardada no superó validación · " + error);
      return;
    }
    if((expectedStart != 0U || expectedEnd != selected->frame_count) &&
       !mTakeScheduler.set_play_range(static_cast<std::size_t>(expectedStart),
                                      static_cast<std::size_t>(expectedEnd), error))
    {
      fail_recovery("no se pudo restaurar ENTRADA / SALIDA · " + error);
      return;
    }

    aeyla::take_library_session::set_loaded_path(this, song.song_id, selected->path);
    mLoadedTakeSongIndex.store(static_cast<int>(songIndex), std::memory_order_release);
    mActiveTakeSongIndex.store(-1, std::memory_order_release);
    (void)mModel.select_song(songIndex);
    mArtNetOutput.set_blackout_latched(mParamBlackout.load(std::memory_order_acquire));
    if(!mTakeScheduler.arm(error))
    {
      fail_recovery("la toma quedó cargada pero ARM no pudo recuperarse · " + error);
      return;
    }
  }

  if(mDawRestoreModelArm)
  {
    if(!mModel.request_arm())
    {
      fail_recovery("la autoridad de salida del modelo no pudo rearmarse");
      return;
    }
    mArtNetOutput.prepare_explicit_rearm();
  }

  mDawSessionRecoveryPending = false;
  mDawRestoreModelArm = false;
  mDawRestoreTakeArm = false;
  mDawRestoreTakeSongIndex = 255U;
  SyncSnapshotToAtomicsLocked();
  PublishOutputFrameLocked(false);
  RefreshHostStateCacheLocked();
  SetShowMidiMessage(
      std::string("SESIÓN DAW RECUPERADA · TOMAS VINCULADAS · ") +
      (TakeOutputArmed() || OutputArmed() ? "ARM RESTAURADO" : "DESARMADA") +
      (GlobalBlackout() ? " · APAGÓN ACTIVO" : " · APAGÓN LIBERADO") +
      " · LISTA PARA PLAY");
}

'''
text = text.replace(marker, helper + marker, 1)
p.write_text(text)

# R10.13 made preload optional for launching. Make ARM consistent.
replace_once(
    'product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp',
    '  if(ShowMidiMapping().enabled &&\n     mMidiPreflightCursor.load(std::memory_order_acquire) >= 0)\n    return {false, {},\n            "Espera a que MIDI / SHOW indique PRECARGA COMPLETA antes de armar"};\n',
    '')

replace_once(
    'product/AeylaVisualDmx/config.h',
    "// Custom component chunks contain versioned RGB Live Control project identity\n// and safe preferences, followed by iPlug's parameter state. Output Arm is\n// excluded.\n#define PLUG_DOES_STATE_CHUNKS 1",
    "// Custom component chunks contain versioned RGB Live Control project/session\n// state, followed by iPlug parameter state. R10.14 also stores explicit crash\n// recovery intent for ARM; runtime validation must succeed before rearming.\n#define PLUG_DOES_STATE_CHUNKS 1")

# Source-level invariants.
plugin_h = Path('src/runtime/plugin_state.h').read_text()
plugin_cpp = Path('src/runtime/plugin_state.cpp').read_text()
visual = Path('product/AeylaVisualDmx/AeylaVisualDmx.cpp').read_text()
player = Path('product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp').read_text()
assert 'kPluginStateFormatMinor = 7' in plugin_h
assert 'restore_take_output_armed' in plugin_h
assert 'RestoreSavedDawSessionLocked' in visual
assert 'SESIÓN DAW RECUPERADA' in visual
assert 'state.blackout = GlobalBlackout();' in visual
assert 'Espera a que MIDI / SHOW indique PRECARGA COMPLETA antes de armar' not in player
assert 'invalid_recovery_state' in plugin_cpp
