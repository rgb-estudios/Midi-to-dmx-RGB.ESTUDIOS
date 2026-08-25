#pragma once

#include "capture/dmx_take_file_reader.h"
#include "output/artnet_output_worker.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace aeyla::capture {

enum class DmxClipTransportState : std::uint8_t {
  ready = 0,
  playing,
  paused,
  ended,
  fault,
};

struct DmxClipPlaybackStatus {
  bool running{false};
  bool loaded{false};
  bool armed{false};
  bool hold_valid{false};
  bool host_heartbeat_ok{false};
  bool rendering_offline{false};
  DmxClipTransportState transport{DmxClipTransportState::ready};
  std::uint64_t current_frame{0U};
  std::uint64_t range_start_frame{0U};
  std::uint64_t range_end_frame_exclusive{0U};
  std::uint64_t cursor_samples{0U};
  double progress{0.0};
  std::string error;
};

// File-backed DMX clip player driven by a RELATIVE sample cursor.
//
// Product contract R06:
// - the absolute DAW arrangement position is NOT the artistic clock;
// - the DAW supplies MIDI commands and processed sample blocks;
// - PLAY/RETRIGGER starts the consolidated clip at sample cursor 0;
// - PAUSE holds cursor + DMX, RESUME continues from that cursor;
// - advance_samples() is the only way the artistic cursor advances;
// - wall clock is reserved for host-liveness watchdogs in the integration
//   layer, never for artistic playback position.
//
// This keeps a clip independent from track order, Arrangement position and
// host seeks while preserving sample-derived timing.
class DmxClipPlaybackEngine final {
 public:
  DmxClipPlaybackEngine();
  ~DmxClipPlaybackEngine();

  DmxClipPlaybackEngine(const DmxClipPlaybackEngine&) = delete;
  DmxClipPlaybackEngine& operator=(const DmxClipPlaybackEngine&) = delete;

  void attach(output::ArtNetOutputWorker* output) noexcept;

  [[nodiscard]] bool load_clip(const std::filesystem::path& path,
                               double sample_rate,
                               std::string& error_message);
  void unload() noexcept;

  [[nodiscard]] bool set_play_range(std::uint64_t start_frame,
                                    std::uint64_t end_frame_exclusive,
                                    std::string& error_message);
  void reset_play_range() noexcept;

  [[nodiscard]] bool arm(std::string& error_message);
  void disarm() noexcept;

  // Runtime transport commands. These are intended to be called by the
  // non-realtime bridge after MIDI events have been ordered by sampleOffset.
  [[nodiscard]] bool play_from_start(std::string& error_message);
  [[nodiscard]] bool pause(std::string& error_message);
  [[nodiscard]] bool resume(std::string& error_message);
  void stop_and_reset() noexcept;

  // Advance the relative artistic cursor by exactly the number of processed
  // host samples that occur while transport == playing. The integration layer
  // must account for a MIDI event's sampleOffset so samples before the event do
  // not advance a newly launched clip.
  void advance_samples(std::uint32_t processed_samples,
                       bool rendering_offline) noexcept;

  // Host callback watchdog state is supplied by the integration layer. A dead
  // host disables physical clip authority but never changes the artistic
  // cursor by wall time.
  void set_host_heartbeat_ok(bool ok) noexcept;

  [[nodiscard]] DmxClipPlaybackStatus status() const;

 private:
  void ensure_thread();
  void shutdown() noexcept;
  void run() noexcept;
  void set_error(std::string message) noexcept;
  [[nodiscard]] std::string error() const;
  [[nodiscard]] bool publish_cursor_frame_locked();

  mutable std::mutex mutex_;
  output::ArtNetOutputWorker* output_{nullptr};
  DmxTakeFileReader reader_{};
  double sample_rate_{0.0};
  std::uint64_t range_start_frame_{0U};
  std::uint64_t range_end_frame_exclusive_{0U};
  std::uint64_t cursor_samples_{0U};
  DmxUniverse hold_frame_{};
  bool hold_valid_{false};
  std::uint64_t generation_{3000000000ULL};

  mutable std::mutex error_mutex_;
  std::string error_;

  std::thread worker_;
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> loaded_{false};
  std::atomic<bool> armed_{false};
  std::atomic<bool> heartbeat_ok_{false};
  std::atomic<bool> rendering_offline_{false};
  std::atomic<DmxClipTransportState> transport_{DmxClipTransportState::ready};
  std::atomic<std::uint64_t> current_frame_{0U};
  std::atomic<double> progress_{0.0};
};

}  // namespace aeyla::capture
