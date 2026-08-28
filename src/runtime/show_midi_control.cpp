#include "runtime/show_midi_control.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace aeyla::runtime {
namespace {

constexpr std::uint8_t kMaximumLaunchBase =
    static_cast<std::uint8_t>(127U - (kShowMidiSongCapacity - 1U));

std::array<std::uint8_t, 5U> global_notes(
    const ShowMidiMapping& mapping) noexcept {
  return {mapping.previous_note, mapping.next_note, mapping.play_note,
          mapping.pause_note, mapping.stop_note};
}

bool inside_launch_range(const ShowMidiMapping& mapping,
                         std::uint8_t note) noexcept {
  const auto first = static_cast<unsigned>(mapping.launch_base_note);
  const auto value = static_cast<unsigned>(note);
  return value >= first && value < first + kShowMidiSongCapacity;
}

}  // namespace

ShowMidiEvent make_show_midi_event(
    ShowMidiCommand command,
    std::uint8_t song_index,
    std::uint8_t channel,
    std::uint8_t note,
    std::uint64_t completed_callback_samples,
    std::uint64_t completed_transport_samples,
    std::uint32_t sample_offset) noexcept {
  const auto offset = static_cast<std::uint64_t>(sample_offset);
  const auto saturating_add = [offset](std::uint64_t value) noexcept {
    return value > std::numeric_limits<std::uint64_t>::max() - offset
        ? std::numeric_limits<std::uint64_t>::max()
        : value + offset;
  };
  ShowMidiEvent event;
  event.command = command;
  event.song_index = song_index;
  event.channel = channel;
  event.note = note;
  event.trigger_sample = saturating_add(completed_transport_samples);
  event.ready_sample = saturating_add(completed_callback_samples);
  return event;
}

bool show_midi_event_ready(
    std::uint64_t completed_callback_samples,
    const ShowMidiEvent& event) noexcept {
  return completed_callback_samples > event.ready_sample;
}

std::uint64_t show_midi_elapsed_samples(
    std::uint64_t completed_transport_samples,
    const ShowMidiEvent& event) noexcept {
  return completed_transport_samples > event.trigger_sample
      ? completed_transport_samples - event.trigger_sample : 0U;
}

ShowMidiMappingError validate_show_midi_mapping(
    const ShowMidiMapping& mapping) noexcept {
  if(mapping.channel < 1U || mapping.channel > 16U)
    return ShowMidiMappingError::invalid_channel;
  if(mapping.launch_base_note > kMaximumLaunchBase)
    return ShowMidiMappingError::launch_range_overflow;

  const auto notes = global_notes(mapping);
  if(std::any_of(notes.begin(), notes.end(), [](std::uint8_t note) {
       return note > 127U;
     }))
    return ShowMidiMappingError::invalid_note;

  for(std::size_t left = 0U; left < notes.size(); ++left) {
    if(inside_launch_range(mapping, notes[left]))
      return ShowMidiMappingError::duplicate_note;
    for(std::size_t right = left + 1U; right < notes.size(); ++right) {
      if(notes[left] == notes[right])
        return ShowMidiMappingError::duplicate_note;
    }
  }
  return ShowMidiMappingError::none;
}

bool match_show_midi_note(const ShowMidiMapping& mapping,
                          std::uint8_t channel,
                          std::uint8_t note,
                          std::uint8_t velocity,
                          ShowMidiMatch& match) noexcept {
  if(!mapping.enabled || velocity == 0U || channel != mapping.channel ||
     validate_show_midi_mapping(mapping) != ShowMidiMappingError::none)
    return false;

  if(note == mapping.previous_note)
    match.command = ShowMidiCommand::previous_song;
  else if(note == mapping.next_note)
    match.command = ShowMidiCommand::next_song;
  else if(note == mapping.play_note)
    match.command = ShowMidiCommand::play_retrigger;
  else if(note == mapping.pause_note)
    match.command = ShowMidiCommand::pause_resume;
  else if(note == mapping.stop_note)
    match.command = ShowMidiCommand::stop_reset;
  else if(inside_launch_range(mapping, note)) {
    match.command = ShowMidiCommand::launch_song;
    match.song_index = static_cast<std::uint8_t>(note - mapping.launch_base_note);
  }
  else
    return false;
  return true;
}

bool assign_show_midi_note(ShowMidiMapping& mapping,
                           ShowMidiLearnTarget target,
                           std::uint8_t channel,
                           std::uint8_t note,
                           std::string& error_message) noexcept {
  error_message.clear();
  if(target == ShowMidiLearnTarget::none) {
    error_message = "No existe una asignación MIDI esperando aprendizaje";
    return false;
  }
  ShowMidiMapping candidate = mapping;
  candidate.channel = channel;
  switch(target) {
    case ShowMidiLearnTarget::previous_song: candidate.previous_note = note; break;
    case ShowMidiLearnTarget::next_song: candidate.next_note = note; break;
    case ShowMidiLearnTarget::play_retrigger: candidate.play_note = note; break;
    case ShowMidiLearnTarget::pause_resume: candidate.pause_note = note; break;
    case ShowMidiLearnTarget::stop_reset: candidate.stop_note = note; break;
    case ShowMidiLearnTarget::launch_song_base: candidate.launch_base_note = note; break;
    case ShowMidiLearnTarget::none: break;
  }

  const auto validation = validate_show_midi_mapping(candidate);
  if(validation != ShowMidiMappingError::none) {
    error_message = validation == ShowMidiMappingError::launch_range_overflow
        ? "La nota base debe dejar espacio para 15 canciones"
        : "La nota coincide con otro comando o con el rango de canciones";
    return false;
  }
  mapping = candidate;
  return true;
}

std::uint64_t pack_show_midi_mapping(const ShowMidiMapping& mapping) noexcept {
  const std::array<std::uint8_t, 8U> bytes{
      static_cast<std::uint8_t>(mapping.enabled ? 1U : 0U), mapping.channel,
      mapping.previous_note, mapping.next_note, mapping.play_note,
      mapping.pause_note, mapping.stop_note, mapping.launch_base_note};
  std::uint64_t packed = 0U;
  for(std::size_t index = 0U; index < bytes.size(); ++index)
    packed |= static_cast<std::uint64_t>(bytes[index]) << (index * 8U);
  return packed;
}

ShowMidiMapping unpack_show_midi_mapping(std::uint64_t packed) noexcept {
  ShowMidiMapping mapping;
  const auto byte = [&](std::size_t index) {
    return static_cast<std::uint8_t>((packed >> (index * 8U)) & 0xFFU);
  };
  mapping.enabled = byte(0U) != 0U;
  mapping.channel = byte(1U);
  mapping.previous_note = byte(2U);
  mapping.next_note = byte(3U);
  mapping.play_note = byte(4U);
  mapping.pause_note = byte(5U);
  mapping.stop_note = byte(6U);
  mapping.launch_base_note = byte(7U);
  return mapping;
}

const char* show_midi_mapping_error_name(ShowMidiMappingError error) noexcept {
  switch(error) {
    case ShowMidiMappingError::none: return "none";
    case ShowMidiMappingError::invalid_channel: return "invalid_channel";
    case ShowMidiMappingError::invalid_note: return "invalid_note";
    case ShowMidiMappingError::launch_range_overflow: return "launch_range_overflow";
    case ShowMidiMappingError::duplicate_note: return "duplicate_note";
  }
  return "unknown";
}

}  // namespace aeyla::runtime
