from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one match, got {count}: {old[:80]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# 1) Streamed capture: keep the continuous record path sequential and cheap.
# The temporary file is not promoted to a valid Take until finalize(), so a
# durable fsync/_commit every second only competes with the DAW's own disk I/O.
replace_once(
    "src/capture/dmx_take_stream_writer.cpp",
    "#ifdef _WIN32\n#include <io.h>\n#else",
    "#ifdef _WIN32\n#ifndef NOMINMAX\n#define NOMINMAX\n#endif\n#include <windows.h>\n#include <io.h>\n#else",
)
replace_once(
    "src/capture/dmx_take_stream_writer.cpp",
    "       !write_text(file_, config_.take_name) ||\n       !write_text(file_, config_.source_ipv4) ||\n       !sync_file(file_)) {",
    "       !write_text(file_, config_.take_name) ||\n       !write_text(file_, config_.source_ipv4) ||\n       std::fflush(file_) != 0) {",
)
replace_once(
    "src/capture/dmx_take_stream_writer.cpp",
    "  void run() noexcept {\n    while(!stop_requested_.load(std::memory_order_acquire) || !queue_.empty()) {",
    "  void run() noexcept {\n#ifdef _WIN32\n    // DMX capture is only ~22 KiB/s at 44 Hz. Keep its disk worker in Windows\n    // background mode so it cannot compete with REAPER's real-time audio I/O.\n    (void)SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);\n#endif\n    while(!stop_requested_.load(std::memory_order_acquire) || !queue_.empty()) {",
)
replace_once(
    "src/capture/dmx_take_stream_writer.cpp",
    "        if(written % config_.frames_per_second == 0U &&\n           !patch_frame_count(written, true)) {\n          fail(\"Streamed Take durable checkpoint failed\");",
    "        const auto checkpoint_frames =\n            static_cast<std::uint64_t>(config_.frames_per_second) * 5U;\n        if(checkpoint_frames > 0U && written % checkpoint_frames == 0U &&\n           !patch_frame_count(written, false)) {\n          fail(\"Streamed Take buffered checkpoint failed\");",
)

# 2) A file-backed Take still needs the scheduler as host-heartbeat watchdog,
# but it does not need the legacy 500 Hz polling cadence. Preserve the 2 ms
# legacy timing and the clip engine's 1 ms safety timing; use 10 ms only for
# the file-mode watchdog, which remains far inside the 750 ms fail-closed gate.
replace_once(
    "src/capture/dmx_take_scheduler.cpp",
    "  constexpr auto kLoopPeriod = std::chrono::milliseconds(2);\n  constexpr auto kHeartbeatTimeout = std::chrono::milliseconds(750);",
    "  constexpr auto kLegacyLoopPeriod = std::chrono::milliseconds(2);\n  constexpr auto kFileWatchdogPeriod = std::chrono::milliseconds(10);\n  constexpr auto kHeartbeatTimeout = std::chrono::milliseconds(750);",
)
replace_once(
    "src/capture/dmx_take_scheduler.cpp",
    "      std::this_thread::sleep_for(kLoopPeriod);\n      continue;",
    "      std::this_thread::sleep_for(kFileWatchdogPeriod);\n      continue;",
)
replace_once(
    "src/capture/dmx_take_scheduler.cpp",
    "    std::this_thread::sleep_for(kLoopPeriod);\n  }",
    "    std::this_thread::sleep_for(kLegacyLoopPeriod);\n  }",
)

# 3) Bind EN VIVO runtime once per plugin instance. Pure UI reads must not
# repeatedly acquire the global session mutex just to re-register identical
# worker pointers, and the 250 Hz runtime loop must not do it either.
replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.cpp",
    "#endif\n\n  StartRuntimeWorker();",
    "#endif\n\n  aeyla::live_memory_session::register_runtime(\n      this, &mArtNetOutput, &mArtNetCapture);\n  StartRuntimeWorker();",
)
replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.cpp",
    "\n  aeyla::live_memory_session::register_runtime(\n      this, &mArtNetOutput, &mArtNetCapture);\n\n  aeyla::runtime::HostEvent event{};",
    "\n  aeyla::runtime::HostEvent event{};",
)
replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.h",
    "  {\n    aeyla::live_memory_session::register_runtime(\n        this, &mArtNetOutput, &mArtNetCapture);\n    return aeyla::live_memory_session::view(this, index);\n  }",
    "  {\n    return aeyla::live_memory_session::view(this, index);\n  }",
)

print("R10.8 RT stability patch applied")
