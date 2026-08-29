from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected old block exactly once, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


header = Path("product/AeylaVisualDmx/AeylaVisualDmx.h")
source = Path("product/AeylaVisualDmx/AeylaVisualDmx.cpp")

replace_once(
    header,
    "  std::atomic<bool> mHostDeactivationPending{false};\n  std::atomic<bool> mHostStateRestoreRejected{false};",
    "  std::atomic<bool> mHostDeactivationPending{false};\n"
    "  // OnReset may execute on a host processing thread. It only raises this\n"
    "  // lock-free request; RuntimeTick performs the destructive scheduler/model\n"
    "  // reset outside the audio callback. This prevents a take loaded at an old\n"
    "  // sample rate from surviving an Ableton audio-engine reset.\n"
    "  std::atomic<bool> mHostResetPending{false};\n"
    "  std::atomic<bool> mHostStateRestoreRejected{false};",
)

replace_once(
    source,
    "void AeylaVisualDmx::OnReset()\n"
    "{\n"
    "  mLastMidiNote.store(-1, std::memory_order_relaxed);\n\n"
    "  aeyla::runtime::HostEvent event{};\n"
    "  event.type = aeyla::runtime::HostEventType::all_notes_off;\n"
    "  (void) mHostIngress.try_submit(event);\n"
    "}",
    "void AeylaVisualDmx::OnReset()\n"
    "{\n"
    "  mLastMidiNote.store(-1, std::memory_order_relaxed);\n"
    "  // Ableton may call OnReset when its audio engine/device/sample rate is\n"
    "  // rebuilt. Never perform scheduler locks or file I/O here. Mark the next\n"
    "  // transport callback as a fresh edge and ask the runtime worker to\n"
    "  // invalidate any file-backed take that was calibrated to the old rate.\n"
    "  mAudioTransportRunning.store(false, std::memory_order_release);\n"
    "  mHostResetPending.store(true, std::memory_order_release);\n\n"
    "  aeyla::runtime::HostEvent event{};\n"
    "  event.type = aeyla::runtime::HostEventType::all_notes_off;\n"
    "  (void) mHostIngress.try_submit(event);\n"
    "}",
)

replace_once(
    source,
    "    ApplyPendingHostStateLocked();\n\n"
    "    if(mHostDeactivationPending.exchange(false, std::memory_order_acq_rel))",
    "    ApplyPendingHostStateLocked();\n\n"
    "    if(mHostResetPending.exchange(false, std::memory_order_acq_rel))\n"
    "    {\n"
    "      // A host audio-engine reset may change sample rate. A loaded clip's\n"
    "      // sample-domain conversion is therefore no longer authoritative.\n"
    "      // Fail closed now; the next PLAY/preflight reloads the .aeylatake with\n"
    "      // the current GetSampleRate() instead of continuing with stale timing.\n"
    "      ClearShowMidiCommandsLocked();\n"
    "      mTakeScheduler.stop_reset();\n"
    "      mTakeScheduler.disarm();\n"
    "      mLoadedTakeSongIndex.store(-1, std::memory_order_release);\n"
    "      mActiveTakeSongIndex.store(-1, std::memory_order_release);\n"
    "      if(ShowMidiMapping().enabled)\n"
    "        mMidiPreflightCursor.store(0, std::memory_order_release);\n"
    "      mModel.release_transients();\n"
    "      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::host_deactivation);\n"
    "      mModel.set_blackout(true);\n"
    "      mParamBlackout.store(true, std::memory_order_release);\n"
    "      SetShowMidiMessage(\n"
    "          \"HOST RESET · SALIDA DESARMADA · toma invalidada y pendiente de recarga\");\n"
    "    }\n\n"
    "    if(mHostDeactivationPending.exchange(false, std::memory_order_acq_rel))",
)

print("R08 host-reset patch applied with exact-match guards")
