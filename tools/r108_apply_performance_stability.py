from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly 1 match, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/capture/dmx_take_stream_writer.cpp",
    "constexpr std::uint64_t kMaximumDurationSeconds = 60U * 60U;\n",
    "constexpr std::uint64_t kMaximumDurationSeconds = 60U * 60U;\n"
    "// Recording shares the same storage subsystem as the DAW. A forced disk\n"
    "// cache flush every second can stall REAPER's audio recorder even though\n"
    "// this writer runs on a background thread. Keep lightweight header\n"
    "// checkpoints during REC and reserve the durable fsync/_commit for STOP.\n"
    "constexpr std::uint64_t kStreamCheckpointSeconds = 5U;\n",
)

replace_once(
    "src/capture/dmx_take_stream_writer.cpp",
    "        const auto written =\n"
    "            frames_written_.fetch_add(1U, std::memory_order_release) + 1U;\n"
    "        if(written % config_.frames_per_second == 0U &&\n"
    "           !patch_frame_count(written, true)) {\n"
    "          fail(\"Streamed Take durable checkpoint failed\");\n"
    "          accepting_.store(false, std::memory_order_release);\n"
    "          break;\n"
    "        }\n",
    "        const auto written =\n"
    "            frames_written_.fetch_add(1U, std::memory_order_release) + 1U;\n"
    "        const auto checkpoint_frames =\n"
    "            static_cast<std::uint64_t>(config_.frames_per_second) *\n"
    "            kStreamCheckpointSeconds;\n"
    "        if(checkpoint_frames > 0U && written % checkpoint_frames == 0U &&\n"
    "           !patch_frame_count(written, false)) {\n"
    "          fail(\"Streamed Take checkpoint failed\");\n"
    "          accepting_.store(false, std::memory_order_release);\n"
    "          break;\n"
    "        }\n",
)

replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.cpp",
    "  constexpr auto kRuntimePeriod = std::chrono::milliseconds(4);\n",
    "  // 125 Hz is comfortably above DMX frame rate and MIDI operator needs,\n"
    "  // while avoiding a 250 Hz full-model reconciliation loop beside the DAW.\n"
    "  constexpr auto kRuntimePeriod = std::chrono::milliseconds(8);\n",
)

replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.cpp",
    "      const auto capture = mArtNetCapture.stats();\n"
    "      (void)mCaptureSyncAnchor.observe(host, capture.recorded_frames);\n",
    "      // REC hot path: only read the lock-free frame counter. stats() also\n"
    "      // copies strings/paths and queries the disk writer, which is useful\n"
    "      // for UI diagnostics but unnecessary at runtime tick frequency.\n"
    "      const auto recordedFrames = mArtNetCapture.recorded_frames_fast();\n"
    "      (void)mCaptureSyncAnchor.observe(host, recordedFrames);\n",
)

replace_once(
    "src/capture/dmx_clip_playback_engine.cpp",
    "constexpr auto kWorkerSleep = std::chrono::milliseconds(1);\n",
    "// DMX runs at <=60 Hz (22.7 ms at the normal 44 Hz). Polling the file\n"
    "// cursor every 1 ms creates needless wakeups and mutex traffic beside the\n"
    "// audio engine. 4 ms preserves sub-frame response with far less pressure.\n"
    "constexpr auto kWorkerSleep = std::chrono::milliseconds(4);\n",
)

replace_once(
    "src/capture/dmx_take_scheduler.cpp",
    "  constexpr auto kLoopPeriod = std::chrono::milliseconds(2);\n",
    "  // The file player owns sample-clock playback. This scheduler primarily\n"
    "  // supervises heartbeat/authority, so a 5 ms cadence is sufficient and\n"
    "  // avoids a 500 Hz watchdog loop competing with the host.\n"
    "  constexpr auto kLoopPeriod = std::chrono::milliseconds(5);\n",
)

print("R10.8 performance/stability patch applied")
