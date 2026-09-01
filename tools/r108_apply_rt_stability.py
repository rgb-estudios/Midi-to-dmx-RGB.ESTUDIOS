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

# 2) Give the disk queue a full 1 MiB of bounded headroom. This is still tiny
# for a plugin but absorbs tens of seconds of transient storage contention.
replace_once(
    "src/capture/dmx_take_stream_writer.h",
    "  static constexpr std::size_t kBufferedFrames = 1024U;",
    "  static constexpr std::size_t kBufferedFrames = 2048U;",
)

# 3) File-backed Takes already own a dedicated clip worker. Do not also start
# the legacy in-memory scheduler thread at 500 Hz.
replace_once(
    "src/capture/dmx_take_scheduler.cpp",
    "  file_mode_.store(true, std::memory_order_release);\n  ensure_thread();\n  return true;",
    "  file_mode_.store(true, std::memory_order_release);\n  return true;",
)
replace_once(
    "src/capture/dmx_take_scheduler.cpp",
    "  error_message.clear();\n  ensure_thread();\n\n  output::ArtNetOutputWorker* output = nullptr;",
    "  error_message.clear();\n\n  output::ArtNetOutputWorker* output = nullptr;",
)
replace_once(
    "src/capture/dmx_take_scheduler.cpp",
    "    return true;\n  }\n\n  const std::scoped_lock lock(mutex_);\n  if(take_ == nullptr || !hold_valid_) {",
    "    return true;\n  }\n\n  // Legacy in-memory Takes still need the scheduler worker; file-backed Takes\n  // above are driven exclusively by DmxClipPlaybackEngine.\n  ensure_thread();\n  const std::scoped_lock lock(mutex_);\n  if(take_ == nullptr || !hold_valid_) {",
)

# 4) The clip engine does not need 1000 wakeups/s to feed a 44 Hz Art-Net
# carrier. 2 ms preserves sub-frame responsiveness while halving wakeups.
replace_once(
    "src/capture/dmx_clip_playback_engine.cpp",
    "constexpr auto kWorkerSleep = std::chrono::milliseconds(1);",
    "constexpr auto kWorkerSleep = std::chrono::milliseconds(2);",
)

# 5) Bind EN VIVO runtime once per plugin instance. Pure UI reads must not
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
