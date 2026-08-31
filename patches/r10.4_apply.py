from pathlib import Path


def replace_exact(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one match, found {count}")
    p.write_text(text.replace(old, new), encoding="utf-8")


replace_exact(
    "product/AeylaVisualDmx/AeylaVisualDmx.cpp",
    '''void AeylaVisualDmx::OnReset()\n{\n  mLastMidiNote.store(-1, std::memory_order_relaxed);\n  mAudioTransportRunning.store(false, std::memory_order_release);\n  mHostResetPending.store(true, std::memory_order_release);\n\n  aeyla::runtime::HostEvent event{};\n  event.type = aeyla::runtime::HostEventType::all_notes_off;\n  (void)mHostIngress.try_submit(event);\n}\n\nvoid AeylaVisualDmx::OnActivate(bool active)\n{\n  if(!active)\n    mHostDeactivationPending.store(true, std::memory_order_release);\n}\n''',
    '''void AeylaVisualDmx::OnReset()\n{\n  mLastMidiNote.store(-1, std::memory_order_relaxed);\n  mAudioTransportRunning.store(false, std::memory_order_release);\n\n  // Host/device reset is not an operator withdrawal command. REAPER may call\n  // this while rebuilding or refocusing the plug-in. Keep physical Art-Net\n  // authority untouched; only release transient MIDI state.\n  aeyla::runtime::HostEvent event{};\n  event.type = aeyla::runtime::HostEventType::all_notes_off;\n  (void)mHostIngress.try_submit(event);\n}\n\nvoid AeylaVisualDmx::OnActivate(bool active)\n{\n  // Editor/host activation is not DESARMAR and is not APAGÓN TOTAL.\n  // Physical authority survives normal focus/window lifecycle changes.\n  (void)active;\n}\n''')

replace_exact(
    "product/AeylaVisualDmx/AeylaVisualDmx.cpp",
    '''    if(mHostResetPending.exchange(false, std::memory_order_acq_rel))\n    {\n      ClearShowMidiCommandsLocked();\n      mTakeScheduler.stop_reset();\n      mTakeScheduler.disarm();\n      mLoadedTakeSongIndex.store(-1, std::memory_order_release);\n      mActiveTakeSongIndex.store(-1, std::memory_order_release);\n      if(ShowMidiMapping().enabled)\n        mMidiPreflightCursor.store(0, std::memory_order_release);\n      mModel.release_transients();\n      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::host_deactivation);\n      mModel.set_blackout(true);\n      mParamBlackout.store(true, std::memory_order_release);\n      SetShowMidiMessage(\n          "HOST RESET · SALIDA DESARMADA · toma invalidada y pendiente de recarga");\n    }\n\n    if(mHostDeactivationPending.exchange(false, std::memory_order_acq_rel))\n    {\n      ClearShowMidiCommandsLocked();\n      if(ShowMidiMapping().enabled)\n        mMidiPreflightCursor.store(0, std::memory_order_release);\n      mTakeScheduler.disarm();\n      mModel.release_transients();\n      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::host_deactivation);\n      mModel.set_blackout(true);\n      mParamBlackout.store(true, std::memory_order_release);\n    }\n''',
    '''    if(mHostResetPending.exchange(false, std::memory_order_acq_rel))\n    {\n      // Defensive compatibility for a reset flag queued before R10.4. Never\n      // convert a host reset into an operator blackout or scheduler disarm.\n      SetShowMidiMessage("HOST RESET · AUTORIDAD ART-NET CONSERVADA");\n    }\n\n    if(mHostDeactivationPending.exchange(false, std::memory_order_acq_rel))\n    {\n      // Losing editor/host activation is a normal DAW lifecycle event. It must\n      // not alter scheduler ARM, carrier, active Take or operator blackout.\n      SetShowMidiMessage("HOST INACTIVO · AUTORIDAD ART-NET CONSERVADA");\n    }\n''')

replace_exact(
    "product/AeylaVisualDmx/AeylaVisualDmx.cpp",
    '''bool AeylaVisualDmx::SelectAdjacentSongFromUI(int direction)\n{\n  if(direction == 0 || TakeRecording())\n    return false;\n  const bool preserveTakeAuthority = TakeOutputArmed();\n  if(!preserveTakeAuthority)\n  {\n    mTakeScheduler.stop_reset();\n    mTakeScheduler.disarm();\n    mActiveTakeSongIndex.store(-1, std::memory_order_release);\n  }\n  const std::scoped_lock lock(mModelMutex);\n''',
    '''bool AeylaVisualDmx::SelectAdjacentSongFromUI(int direction)\n{\n  if(direction == 0 || TakeRecording())\n    return false;\n  const std::scoped_lock lock(mModelMutex);\n''')

replace_exact(
    "product/AeylaVisualDmx/AeylaVisualDmx.cpp",
    '''  SetShowMidiMessage("PREPARADA · " + mModel.snapshot().active_song_name +\n                     " · la canción al aire continúa");\n  if(!preserveTakeAuthority)\n    mParamBlackout.store(true, std::memory_order_release);\n  mLastProjectedSongId.clear();\n''',
    '''  SetShowMidiMessage("PREPARADA · " + mModel.snapshot().active_song_name +\n                     " · la canción al aire continúa");\n  mLastProjectedSongId.clear();\n''')

replace_exact(
    "product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp",
    '''  // Only an armed Take owns the PREPARADA/ACTIVA two-state contract. In every\n  // other mode retain the original safe selection boundary.\n  if(!TakeOutputArmed())\n  {\n    mTakeScheduler.stop_reset();\n    mTakeScheduler.disarm();\n    mActiveTakeSongIndex.store(-1, std::memory_order_release);\n  }\n\n  const std::scoped_lock lock(mModelMutex);\n''',
    '''  // Selecting PREPARADA is metadata/navigation only. It never owns physical\n  // Art-Net authority and therefore may not disarm or latch blackout.\n  const std::scoped_lock lock(mModelMutex);\n''')

replace_exact(
    "src/product/application_model.cpp",
    '''bool ApplicationModel::select_song(std::size_t song_index) {\n  if (song_index >= show_program_.songs.size()) return false;\n  if (song_index == active_song_index_ && cue_runtime_.has_value()) return true;\n\n  safety_.disarm(runtime::RuntimeSafetyReason::project_reload);\n  safety_.set_blackout(true);\n  active_executor_ = -1;\n''',
    '''bool ApplicationModel::select_song(std::size_t song_index) {\n  if (song_index >= show_program_.songs.size()) return false;\n  if (song_index == active_song_index_ && cue_runtime_.has_value()) return true;\n\n  // Song selection is PREPARADA navigation, not a project reload and not an\n  // operator safety command. Preserve ARM/global blackout while rebuilding the\n  // artistic cue runtime for the newly prepared song.\n  active_executor_ = -1;\n''')

replace_exact(
    "src/runtime/runtime_safety_state.cpp",
    '''void RuntimeSafetyState::on_host_deactivation() noexcept {\n  output_armed_ = false;\n  blackout_ = true;\n  reason_ = RuntimeSafetyReason::host_deactivation;\n  request_safe_actions(true);\n}\n''',
    '''void RuntimeSafetyState::on_host_deactivation() noexcept {\n  // Host/editor deactivation is an ordinary DAW lifecycle event. It is not an\n  // operator DISARM and must not publish a safe/black frame. True runtime,\n  // backend, offline-render and shutdown faults retain fail-closed paths.\n}\n''')

replace_exact(
    "tests/test_runtime_safety.cpp",
    '''  state.set_project_valid(true);\n  state.set_backend_ready(true);\n  check(state.request_arm(), "may arm before host deactivation test");\n  state.on_host_deactivation();\n  check(!state.output_armed() && state.blackout(), "host deactivation must be safe");\n\n  state.set_project_valid(true);\n''',
    '''  state.set_project_valid(true);\n  state.set_backend_ready(true);\n  check(state.request_arm(), "may arm before host deactivation test");\n  state.set_blackout(false);\n  (void)state.consume_pending_actions();\n  state.on_host_deactivation();\n  check(state.output_armed() && !state.blackout(),\n        "host/editor deactivation must preserve explicit operator authority");\n  actions = state.consume_pending_actions();\n  check(!actions.release_transients && !actions.force_haze_zero &&\n            !actions.publish_safe_frame,\n        "host/editor deactivation must not request a physical withdrawal frame");\n\n  state.set_project_valid(true);\n''')

replace_exact(
    "tests/test_application_model.cpp",
    '''  model.set_blackout(false);\n  check(model.request_arm(),\n        "explicit blackout release plus ARM must recover a preflighted backend");\n\n  HostEvent note_on{};\n''',
    '''  model.set_blackout(false);\n  check(model.request_arm(),\n        "explicit blackout release plus ARM must recover a preflighted backend");\n\n  // R10.4 show contract: selecting PREPARADA is navigation only. It must not\n  // withdraw the manually armed Art-Net authority or re-latch APAGÓN TOTAL.\n  auto two_song_show = development_show;\n  auto second_song = two_song_show.songs.front();\n  second_song.song_id = "runtime-song-2";\n  second_song.name = "AEYLA Runtime Song 2";\n  second_song.scenes.front().scene_id = "scene-main-2";\n  second_song.scenes.front().name = "Main 2";\n  second_song.clips.front().clip_id = "clip-main-2";\n  second_song.clips.front().scene_id = "scene-main-2";\n  two_song_show.songs.push_back(std::move(second_song));\n  const auto two_song_loaded = model.replace_show_program(two_song_show);\n  check(two_song_loaded.ok(), "two-song authority regression show must validate");\n  model.set_blackout(false);\n  check(model.request_arm(), "two-song authority regression must arm explicitly");\n  check(model.select_song(1U), "second song must become PREPARADA");\n  check(model.snapshot().output_armed,\n        "selecting a different PREPARADA song must preserve output ARM");\n  check(!model.snapshot().global_blackout,\n        "selecting a different PREPARADA song must not latch APAGÓN TOTAL");\n  check(model.select_song(0U), "first song must become PREPARADA again");\n  check(model.snapshot().output_armed && !model.snapshot().global_blackout,\n        "repeated song navigation must preserve ARM and global blackout state");\n\n  HostEvent note_on{};\n''')

print("R10.4 authority patch applied deterministically")
