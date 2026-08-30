#pragma once

#include "runtime/spsc_queue.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace aeyla::runtime {

inline constexpr std::size_t kShowMidiSongCapacity = 15U;
// Fixed operational safety/recording controls remain outside the persisted
// legacy MIDI map. They intentionally shadow any restored legacy collision.
inline constexpr std::uint8_t kShowMidiPanicNote = 41U;
inline constexpr std::uint8_t kShowMidiCaptureStartNote = 42U;
inline constexpr std::uint8_t kShowMidiCaptureStopNote = 43U;

enum class ShowMidiCommand : std::uint8_t {
  previous_song = 0,
  next_song,
  play_retrigger,
  pause_resume,
  stop_reset,
  panic_blackout,
  capture_start,
  capture_stop,
  launch_song,
};

enum class ShowMidiLearnTarget : std::uint8_t {
  none = 0,
  previous_song,
  next_song,
  play_retrigger,
  pause_resume,
  stop_reset,
  launch_song_base,
};

// Defaults deliberately live on MIDI channel 16 and remain disabled until the
// operator explicitly enables Show control. Direct Song launch occupies 15
// consecutive notes. Configurable artistic values are persisted in VST3
// component state; PANIC N41, REC START N42 and REC STOP N43 are fixed runtime
// reservations so recording commands are unambiguous and fail-safe.
struct ShowMidiMapping {
  bool enabled{false};
  std::uint8_t channel{16U};
  std::uint8_t previous_note{36U};
  std::uint8_t next_note{37U};
  std::uint8_t play_note{38U};
  std::uint8_t pause_note{39U};
  std::uint8_t stop_note{40U};
  std::uint8_t launch_base_note{48U};

  bool operator==(const ShowMidiMapping&) const = default;
};

enum class ShowMidiMappingError : std::uint8_t {
  none = 0,
  invalid_channel,
  invalid_note,
  launch_range_overflow,
  duplicate_note,
};

struct ShowMidiMatch {
  ShowMidiCommand command{ShowMidiCommand::play_retrigger};
  std::uint8_t song_index{0U};
};

struct ShowMidiEvent {
  ShowMidiCommand command{ShowMidiCommand::play_retrigger};
  std::uint8_t song_index{0U};
  std::uint8_t channel{1U};
  std::uint8_t note{0U};
  // Artistic timeline: samples processed while the DAW transport was running,
  // plus the event offset inside its audio block. It is independent of
  // Arrangement seeks and excludes stopped blocks.
  std::uint64_t trigger_sample{0U};
  // Scheduling timeline: every completed callback sample plus the event
  // offset. This lets the runtime wait for the event's block even when the DAW
  // is stopped, without contaminating the artistic cursor above.
  std::uint64_t ready_sample{0U};
  // Snapshot of the deterministic 44 Hz capture timeline taken at MIDI ingress.
  // Kept for diagnostics/backward compatibility; REC START itself is the new
  // capture origin and does not depend on MTC or a later transport marker.
  std::uint64_t capture_frame_snapshot{0U};
};

[[nodiscard]] ShowMidiEvent make_show_midi_event(
    ShowMidiCommand command,
    std::uint8_t song_index,
    std::uint8_t channel,
    std::uint8_t note,
    std::uint64_t completed_callback_samples,
    std::uint64_t completed_transport_samples,
    std::uint32_t sample_offset,
    std::uint64_t capture_frame_snapshot = 0U) noexcept;

[[nodiscard]] bool show_midi_event_ready(
    std::uint64_t completed_callback_samples,
    const ShowMidiEvent& event) noexcept;

[[nodiscard]] std::uint64_t show_midi_elapsed_samples(
    std::uint64_t completed_transport_samples,
    const ShowMidiEvent& event) noexcept;

[[nodiscard]] ShowMidiMappingError validate_show_midi_mapping(
    const ShowMidiMapping& mapping) noexcept;

[[nodiscard]] bool match_show_midi_note(const ShowMidiMapping& mapping,
                                        std::uint8_t channel,
                                        std::uint8_t note,
                                        std::uint8_t velocity,
                                        ShowMidiMatch& match) noexcept;

[[nodiscard]] bool assign_show_midi_note(ShowMidiMapping& mapping,
                                         ShowMidiLearnTarget target,
                                         std::uint8_t channel,
                                         std::uint8_t note,
                                         std::string& error_message) noexcept;

[[nodiscard]] std::uint64_t pack_show_midi_mapping(
    const ShowMidiMapping& mapping) noexcept;

[[nodiscard]] ShowMidiMapping unpack_show_midi_mapping(
    std::uint64_t packed) noexcept;

[[nodiscard]] const char* show_midi_mapping_error_name(
    ShowMidiMappingError error) noexcept;

template <std::size_t StorageCapacity>
class ShowMidiIngress final {
 public:
  [[nodiscard]] bool try_submit(const ShowMidiEvent& event) noexcept {
    // PANIC is not an artistic transport event and must not wait for queue
    // capacity or sample-ready scheduling. The audio callback only raises this
    // lock-free flag; the non-realtime runtime worker performs the safety work.
    if(event.command == ShowMidiCommand::panic_blackout) {
      panic_requested_.store(true, std::memory_order_release);
      return true;
    }
    if(queue_.try_push(event))
      return true;

    // REC START/STOP are operational rather than artistic. Preserve their
    // semantics separately on overflow so STOP can never turn into START (the
    // principal risk of the former toggle design). STOP is consumed first.
    if(event.command == ShowMidiCommand::capture_stop) {
      capture_stop_fallback_requests_.fetch_add(1U, std::memory_order_release);
      safety_stop_requested_.store(true, std::memory_order_release);
      return true;
    }
    if(event.command == ShowMidiCommand::capture_start) {
      capture_start_fallback_requests_.fetch_add(1U, std::memory_order_release);
      safety_stop_requested_.store(true, std::memory_order_release);
      return true;
    }

    dropped_events_.fetch_add(1U, std::memory_order_relaxed);
    safety_stop_requested_.store(true, std::memory_order_release);
    return false;
  }

  [[nodiscard]] bool try_consume(ShowMidiEvent& event) noexcept {
    auto pending_stop = capture_stop_fallback_requests_.load(
        std::memory_order_acquire);
    while(pending_stop > 0U) {
      if(capture_stop_fallback_requests_.compare_exchange_weak(
             pending_stop, pending_stop - 1U,
             std::memory_order_acq_rel, std::memory_order_acquire)) {
        event = {};
        event.command = ShowMidiCommand::capture_stop;
        event.note = kShowMidiCaptureStopNote;
        return true;
      }
    }

    auto pending_start = capture_start_fallback_requests_.load(
        std::memory_order_acquire);
    while(pending_start > 0U) {
      if(capture_start_fallback_requests_.compare_exchange_weak(
             pending_start, pending_start - 1U,
             std::memory_order_acq_rel, std::memory_order_acquire)) {
        event = {};
        event.command = ShowMidiCommand::capture_start;
        event.note = kShowMidiCaptureStartNote;
        return true;
      }
    }
    return queue_.try_pop(event);
  }

  [[nodiscard]] bool consume_panic_request() noexcept {
    return panic_requested_.exchange(false, std::memory_order_acq_rel);
  }

  [[nodiscard]] bool consume_safety_stop_request() noexcept {
    return safety_stop_requested_.exchange(false, std::memory_order_acq_rel);
  }

  [[nodiscard]] std::uint64_t dropped_events() const noexcept {
    return dropped_events_.load(std::memory_order_relaxed);
  }

 private:
  SpscQueue<ShowMidiEvent, StorageCapacity> queue_{};
  std::atomic<std::uint64_t> dropped_events_{0U};
  std::atomic<std::uint32_t> capture_start_fallback_requests_{0U};
  std::atomic<std::uint32_t> capture_stop_fallback_requests_{0U};
  std::atomic<bool> panic_requested_{false};
  std::atomic<bool> safety_stop_requested_{false};
};

}  // namespace aeyla::runtime
