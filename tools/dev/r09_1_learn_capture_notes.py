from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]

def load(rel):
    p = ROOT / rel
    return p, p.read_text(encoding='utf-8')

def save(p, text):
    p.write_text(text, encoding='utf-8')
    print('patched', p.relative_to(ROOT))

def replace_once(text, old, new, label):
    if old not in text:
        raise SystemExit(f'missing pattern: {label}')
    return text.replace(old, new, 1)

# ---------------------------------------------------------------------------
# Show MIDI model: REC START/STOP become persisted, learnable mapping fields.
# ---------------------------------------------------------------------------
p, t = load('src/runtime/show_midi_control.h')
t = replace_once(t,
'''// Fixed operational safety/recording controls remain outside the persisted
// legacy MIDI map. They intentionally shadow any restored legacy collision.
inline constexpr std::uint8_t kShowMidiPanicNote = 41U;
inline constexpr std::uint8_t kShowMidiCaptureStartNote = 42U;
inline constexpr std::uint8_t kShowMidiCaptureStopNote = 43U;''',
'''// PANIC remains a fixed one-way safety reservation. Capture notes use 42/43
// only as defaults; R09.1 makes REC START and REC STOP independently learnable
// and persistent so the DAW session can own the capture boundary explicitly.
inline constexpr std::uint8_t kShowMidiPanicNote = 41U;
inline constexpr std::uint8_t kShowMidiCaptureStartNote = 42U;
inline constexpr std::uint8_t kShowMidiCaptureStopNote = 43U;''', 'header constants')
t = replace_once(t,
'''  stop_reset,
  launch_song_base,
};''',
'''  stop_reset,
  capture_start,
  capture_stop,
  launch_song_base,
};''', 'learn targets')
t = replace_once(t,
'''// Defaults deliberately live on MIDI channel 16 and remain disabled until the
// operator explicitly enables Show control. Direct Song launch occupies 15
// consecutive notes. Configurable artistic values are persisted in VST3
// component state; PANIC N41, REC START N42 and REC STOP N43 are fixed runtime
// reservations so recording commands are unambiguous and fail-safe.''',
'''// Defaults deliberately live on MIDI channel 16 and remain disabled until the
// operator explicitly enables Show control. Direct Song launch occupies 15
// consecutive notes. PANIC N41 is fixed; REC START/STOP default to N42/N43 but
// are independently learnable. All configurable values persist in host state.''', 'mapping comment')
t = replace_once(t,
'''  std::uint8_t stop_note{40U};
  std::uint8_t launch_base_note{48U};''',
'''  std::uint8_t stop_note{40U};
  std::uint8_t capture_start_note{kShowMidiCaptureStartNote};
  std::uint8_t capture_stop_note{kShowMidiCaptureStopNote};
  std::uint8_t launch_base_note{48U};''', 'capture fields')
save(p, t)

p, t = load('src/runtime/show_midi_control.cpp')
t = replace_once(t,
'''std::array<std::uint8_t, 5U> global_notes(
    const ShowMidiMapping& mapping) noexcept {
  return {mapping.previous_note, mapping.next_note, mapping.play_note,
          mapping.pause_note, mapping.stop_note};
}''',
'''std::array<std::uint8_t, 7U> global_notes(
    const ShowMidiMapping& mapping) noexcept {
  return {mapping.previous_note, mapping.next_note, mapping.play_note,
          mapping.pause_note, mapping.stop_note, mapping.capture_start_note,
          mapping.capture_stop_note};
}''', 'global notes')
t = replace_once(t,
'''  // Legacy state predates fixed N41/N42/N43 reservations. Restored collisions
  // remain decodable; fixed operational commands shadow them at runtime. MIDI
  // Learn below prevents creating new collisions.
  for(std::size_t left = 0U; left < notes.size(); ++left) {
    if(inside_launch_range(mapping, notes[left]))
      return ShowMidiMappingError::duplicate_note;
    for(std::size_t right = left + 1U; right < notes.size(); ++right) {
      if(notes[left] == notes[right])
        return ShowMidiMappingError::duplicate_note;
    }
  }
  return ShowMidiMappingError::none;''',
'''  // PANIC stays fixed and may never be shadowed. Every other command,
  // including REC START/STOP, is configurable but must remain unique and out
  // of the 15-note direct-song bank.
  if(range_contains_fixed_note(mapping.launch_base_note, kShowMidiPanicNote))
    return ShowMidiMappingError::duplicate_note;
  for(std::size_t left = 0U; left < notes.size(); ++left) {
    if(notes[left] == kShowMidiPanicNote || inside_launch_range(mapping, notes[left]))
      return ShowMidiMappingError::duplicate_note;
    for(std::size_t right = left + 1U; right < notes.size(); ++right) {
      if(notes[left] == notes[right])
        return ShowMidiMappingError::duplicate_note;
    }
  }
  return ShowMidiMappingError::none;''', 'strict validation')
t = replace_once(t,
'''  // Fixed operational controls always win over a legacy mapping collision.
  if(note == kShowMidiPanicNote)
    match.command = ShowMidiCommand::panic_blackout;
  else if(note == kShowMidiCaptureStartNote)
    match.command = ShowMidiCommand::capture_start;
  else if(note == kShowMidiCaptureStopNote)
    match.command = ShowMidiCommand::capture_stop;''',
'''  // PANIC always wins. Capture commands follow their learned persistent notes.
  if(note == kShowMidiPanicNote)
    match.command = ShowMidiCommand::panic_blackout;
  else if(note == mapping.capture_start_note)
    match.command = ShowMidiCommand::capture_start;
  else if(note == mapping.capture_stop_note)
    match.command = ShowMidiCommand::capture_stop;''', 'match capture notes')
start = t.find('bool assign_show_midi_note(')
end = t.find('\nstd::uint64_t pack_show_midi_mapping', start)
if start < 0 or end < 0:
    raise SystemExit('assign_show_midi_note block not found')
assign = r'''bool assign_show_midi_note(ShowMidiMapping& mapping,
                           ShowMidiLearnTarget target,
                           std::uint8_t channel,
                           std::uint8_t note,
                           std::string& error_message) noexcept {
  error_message.clear();
  if(target == ShowMidiLearnTarget::none) {
    error_message = "No existe una asignación MIDI esperando aprendizaje";
    return false;
  }
  if(channel < 1U || channel > 16U || note > 127U) {
    error_message = "Canal o nota MIDI fuera de rango";
    return false;
  }
  if(note == kShowMidiPanicNote) {
    error_message = "La nota 41 está reservada para PANIC / APAGÓN";
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
    case ShowMidiLearnTarget::capture_start: candidate.capture_start_note = note; break;
    case ShowMidiLearnTarget::capture_stop: candidate.capture_stop_note = note; break;
    case ShowMidiLearnTarget::launch_song_base: candidate.launch_base_note = note; break;
    case ShowMidiLearnTarget::none: break;
  }

  const auto validation = validate_show_midi_mapping(candidate);
  if(validation != ShowMidiMappingError::none) {
    error_message = validation == ShowMidiMappingError::launch_range_overflow
        ? "La nota base debe dejar espacio para 15 canciones"
        : "La nota coincide con otro comando, PANIC o el rango de canciones";
    return false;
  }
  mapping = candidate;
  return true;
}
'''
t = t[:start] + assign + t[end:]
t = replace_once(t,
'''std::uint64_t pack_show_midi_mapping(const ShowMidiMapping& mapping) noexcept {
  const std::array<std::uint8_t, 8U> bytes{''',
'''// Legacy 64-bit realtime pack keeps the original artistic map. R09.1 stores
// learned REC START/STOP separately in lock-free atomics and appends them to
// host-state format 1.4, preserving compatibility with 1.2/1.3 sessions.
std::uint64_t pack_show_midi_mapping(const ShowMidiMapping& mapping) noexcept {
  const std::array<std::uint8_t, 8U> bytes{''', 'pack comment')
save(p, t)

# ---------------------------------------------------------------------------
# Host state 1.4 appends two bytes after the 1.3 take-binding tail.
# ---------------------------------------------------------------------------
p, t = load('src/runtime/plugin_state.h')
t = replace_once(t,
'inline constexpr std::uint16_t kPluginStateFormatMinor = 3;',
'inline constexpr std::uint16_t kPluginStateFormatMinor = 4;', 'state minor')
save(p, t)

p, t = load('src/runtime/plugin_state.cpp')
t = replace_once(t,
'''      kFixedPayloadSize + locator_size + binding_bytes + 8U +
      take_binding_bytes);''',
'''      kFixedPayloadSize + locator_size + binding_bytes + 8U +
      take_binding_bytes + 2U);''', 'payload size capture tail')
needle = '''      append_u64(result.bytes, binding.start_frame);
      append_u64(result.bytes, binding.end_frame_exclusive);
    }
  } catch (const std::bad_alloc&) {'''
replacement = '''      append_u64(result.bytes, binding.start_frame);
      append_u64(result.bytes, binding.end_frame_exclusive);
    }
    // State 1.4: learned capture boundary notes. Appended so 1.3 readers can
    // safely ignore them while 1.4 restores them exactly.
    result.bytes.push_back(state.show_midi.capture_start_note);
    result.bytes.push_back(state.show_midi.capture_stop_note);
  } catch (const std::bad_alloc&) {'''
t = replace_once(t, needle, replacement, 'encode capture tail')
needle = '''  // Same-major future minor versions may append fields inside payload_size.
  if (offset > payload_end) {'''
replacement = '''  if (format_minor >= 4U) {
    if (offset + 2U > payload_end) {
      result.error = PluginStateError::invalid_show_midi_mapping;
      return result;
    }
    result.state.show_midi.capture_start_note = bytes[offset++];
    result.state.show_midi.capture_stop_note = bytes[offset++];
  }

  // Same-major future minor versions may append fields inside payload_size.
  if (offset > payload_end) {'''
t = replace_once(t, needle, replacement, 'decode capture tail')
save(p, t)

# ---------------------------------------------------------------------------
# Plugin lock-free bridge: retain 64-bit artistic map + two learned REC atomics.
# ---------------------------------------------------------------------------
p, t = load('product/AeylaVisualDmx/AeylaVisualDmx.h')
t = replace_once(t,
'''  std::atomic<std::uint64_t> mShowMidiMappingPacked{0U};
  std::atomic<std::uint32_t> mPendingMidiLearnPacked{0U};''',
'''  std::atomic<std::uint64_t> mShowMidiMappingPacked{0U};
  std::atomic<std::uint8_t> mShowMidiCaptureStartNote{
      aeyla::runtime::kShowMidiCaptureStartNote};
  std::atomic<std::uint8_t> mShowMidiCaptureStopNote{
      aeyla::runtime::kShowMidiCaptureStopNote};
  std::atomic<std::uint32_t> mPendingMidiLearnPacked{0U};''', 'capture note atomics')
save(p, t)

p, t = load('product/AeylaVisualDmx/AeylaVisualDmx.cpp')
needle = '''  mShowMidiMappingPacked.store(aeyla::runtime::pack_show_midi_mapping(
                                   mHostStateCache.show_midi),
                               std::memory_order_release);'''
replacement = needle + '''
  mShowMidiCaptureStartNote.store(mHostStateCache.show_midi.capture_start_note,
                                  std::memory_order_release);
  mShowMidiCaptureStopNote.store(mHostStateCache.show_midi.capture_stop_note,
                                 std::memory_order_release);'''
t = replace_once(t, needle, replacement, 'constructor capture atomics')
t = replace_once(t,
'''    const auto showMapping = aeyla::runtime::unpack_show_midi_mapping(
        mShowMidiMappingPacked.load(std::memory_order_acquire));''',
'''    const auto showMapping = ShowMidiMapping();''', 'ProcessMidi mapping')
needle = '''  mShowMidiMappingPacked.store(
      aeyla::runtime::pack_show_midi_mapping(pending->show_midi),
      std::memory_order_release);
  mMidiPreflightCursor.store(pending->show_midi.enabled ? 0 : -1,'''
replacement = '''  mShowMidiMappingPacked.store(
      aeyla::runtime::pack_show_midi_mapping(pending->show_midi),
      std::memory_order_release);
  mShowMidiCaptureStartNote.store(pending->show_midi.capture_start_note,
                                  std::memory_order_release);
  mShowMidiCaptureStopNote.store(pending->show_midi.capture_stop_note,
                                 std::memory_order_release);
  mMidiPreflightCursor.store(pending->show_midi.enabled ? 0 : -1,'''
t = replace_once(t, needle, replacement, 'restore capture atomics')
save(p, t)

p, t = load('product/AeylaVisualDmx/AeylaMidiShowIntegration.cpp')
t = replace_once(t,
'''    case Target::stop_reset: return "STOP / RESET";
    case Target::launch_song_base: return "BASE DE 15 CANCIONES";''',
'''    case Target::stop_reset: return "STOP / RESET";
    case Target::capture_start: return "REC START";
    case Target::capture_stop: return "REC STOP";
    case Target::launch_song_base: return "BASE DE 15 CANCIONES";''', 'learn target names')
t = replace_once(t,
'''aeyla::runtime::ShowMidiMapping AeylaVisualDmx::ShowMidiMapping() const noexcept
{
  return aeyla::runtime::unpack_show_midi_mapping(
      mShowMidiMappingPacked.load(std::memory_order_acquire));
}''',
'''aeyla::runtime::ShowMidiMapping AeylaVisualDmx::ShowMidiMapping() const noexcept
{
  auto mapping = aeyla::runtime::unpack_show_midi_mapping(
      mShowMidiMappingPacked.load(std::memory_order_acquire));
  mapping.capture_start_note = mShowMidiCaptureStartNote.load(
      std::memory_order_acquire);
  mapping.capture_stop_note = mShowMidiCaptureStopNote.load(
      std::memory_order_acquire);
  return mapping;
}''', 'ShowMidiMapping atomics')
t = replace_once(t,
'''  mShowMidiMappingPacked.store(
      aeyla::runtime::pack_show_midi_mapping(mapping),
      std::memory_order_release);
  const std::scoped_lock lock(mHostStateMutex);''',
'''  mShowMidiMappingPacked.store(
      aeyla::runtime::pack_show_midi_mapping(mapping),
      std::memory_order_release);
  mShowMidiCaptureStartNote.store(mapping.capture_start_note,
                                  std::memory_order_release);
  mShowMidiCaptureStopNote.store(mapping.capture_stop_note,
                                 std::memory_order_release);
  const std::scoped_lock lock(mHostStateMutex);''', 'Sync mapping capture atomics')
t = replace_once(t,
'''      ? "MIDI SHOW ACTIVO · canal " + std::to_string(mapping.channel) +
            " · N41 PANIC · N42 REC START · N43 REC STOP · reloj por muestras del DAW"''',
'''      ? "MIDI SHOW ACTIVO · canal " + std::to_string(mapping.channel) +
            " · N41 PANIC · N" + std::to_string(mapping.capture_start_note) +
            " REC START · N" + std::to_string(mapping.capture_stop_note) +
            " REC STOP · reloj por muestras del DAW"''', 'toggle status dynamic notes')
save(p, t)

# ---------------------------------------------------------------------------
# UI: expose two dedicated learn rows and reflect actual learned notes.
# ---------------------------------------------------------------------------
p, t = load('product/AeylaVisualDmx/AeylaMainControl.h')
t = replace_once(t,
'''      constexpr std::array<aeyla::runtime::ShowMidiLearnTarget, 6U> targets{
          aeyla::runtime::ShowMidiLearnTarget::previous_song,
          aeyla::runtime::ShowMidiLearnTarget::next_song,
          aeyla::runtime::ShowMidiLearnTarget::play_retrigger,
          aeyla::runtime::ShowMidiLearnTarget::pause_resume,
          aeyla::runtime::ShowMidiLearnTarget::stop_reset,
          aeyla::runtime::ShowMidiLearnTarget::launch_song_base};''',
'''      constexpr std::array<aeyla::runtime::ShowMidiLearnTarget, 8U> targets{
          aeyla::runtime::ShowMidiLearnTarget::previous_song,
          aeyla::runtime::ShowMidiLearnTarget::next_song,
          aeyla::runtime::ShowMidiLearnTarget::play_retrigger,
          aeyla::runtime::ShowMidiLearnTarget::pause_resume,
          aeyla::runtime::ShowMidiLearnTarget::stop_reset,
          aeyla::runtime::ShowMidiLearnTarget::capture_start,
          aeyla::runtime::ShowMidiLearnTarget::capture_stop,
          aeyla::runtime::ShowMidiLearnTarget::launch_song_base};''', 'mouse learn targets')
t = replace_once(t,
'''    // capture/safety footer into two lines while retaining all six controls.
    const float midiFixedBelowRows = mCompactMidi ? 163.0F : 264.0F;
    const float midiRowHeight = std::clamp(
        (midi.B - rowTop - midiFixedBelowRows) / 6.0F,
        mCompactMidi ? 28.0F : 34.0F,
        mCompactMidi ? 34.0F : 44.0F);''',
'''    // capture/safety footer into two lines while retaining all eight controls.
    const float midiFixedBelowRows = mCompactMidi ? 154.0F : 224.0F;
    const float rowSpace = midi.B - rowTop - midiFixedBelowRows -
        rowGap * static_cast<float>(mMidiRows.size() - 1U);
    const float midiRowHeight = std::clamp(
        rowSpace / static_cast<float>(mMidiRows.size()),
        mCompactMidi ? 24.0F : 28.0F,
        mCompactMidi ? 31.0F : 36.0F);''', 'midi layout 8 rows')
t = replace_once(t,
'''    constexpr std::array<const char*, 6U> labels{
        "CANCIÓN ANTERIOR",
        "SIGUIENTE CANCIÓN",
        "PLAY / REINICIAR DESDE CERO",
        "PAUSA / REANUDAR",
        "STOP / RESET A CERO",
        "LANZAR CANCIONES 01–15"};
    const std::array<std::uint8_t, 6U> notes{
        mapping.previous_note, mapping.next_note, mapping.play_note,
        mapping.pause_note, mapping.stop_note, mapping.launch_base_note};
    constexpr std::array<aeyla::runtime::ShowMidiLearnTarget, 6U> targets{
        aeyla::runtime::ShowMidiLearnTarget::previous_song,
        aeyla::runtime::ShowMidiLearnTarget::next_song,
        aeyla::runtime::ShowMidiLearnTarget::play_retrigger,
        aeyla::runtime::ShowMidiLearnTarget::pause_resume,
        aeyla::runtime::ShowMidiLearnTarget::stop_reset,
        aeyla::runtime::ShowMidiLearnTarget::launch_song_base};''',
'''    constexpr std::array<const char*, 8U> labels{
        "CANCIÓN ANTERIOR",
        "SIGUIENTE CANCIÓN",
        "PLAY / REINICIAR DESDE CERO",
        "PAUSA / REANUDAR",
        "STOP / RESET A CERO",
        "REC START · INICIO CAPTURA",
        "REC STOP · FIN CAPTURA",
        "LANZAR CANCIONES 01–15"};
    const std::array<std::uint8_t, 8U> notes{
        mapping.previous_note, mapping.next_note, mapping.play_note,
        mapping.pause_note, mapping.stop_note, mapping.capture_start_note,
        mapping.capture_stop_note, mapping.launch_base_note};
    constexpr std::array<aeyla::runtime::ShowMidiLearnTarget, 8U> targets{
        aeyla::runtime::ShowMidiLearnTarget::previous_song,
        aeyla::runtime::ShowMidiLearnTarget::next_song,
        aeyla::runtime::ShowMidiLearnTarget::play_retrigger,
        aeyla::runtime::ShowMidiLearnTarget::pause_resume,
        aeyla::runtime::ShowMidiLearnTarget::stop_reset,
        aeyla::runtime::ShowMidiLearnTarget::capture_start,
        aeyla::runtime::ShowMidiLearnTarget::capture_stop,
        aeyla::runtime::ShowMidiLearnTarget::launch_song_base};''', 'draw arrays 8 rows')
t = replace_once(t, '      if(index == 5U)\n', '      if(index == 7U)\n', 'launch range row index')
t = replace_once(t,
'''                 "CAPTURA: N42 REC START · N43 REC STOP · CERO = REC START",''',
'''                 ("CAPTURA: N" + std::to_string(mapping.capture_start_note) +
                  " REC START · N" + std::to_string(mapping.capture_stop_note) +
                  " REC STOP · CERO = REC START").c_str(),''', 'compact footer dynamic')
t = replace_once(t,
'''                 "CAPTURA DMX: N42 REC START fija CERO · N43 REC STOP finaliza · AEYLA no usa MTC.",''',
'''                 ("CAPTURA DMX: N" + std::to_string(mapping.capture_start_note) +
                  " REC START fija CERO · N" +
                  std::to_string(mapping.capture_stop_note) +
                  " REC STOP finaliza · AEYLA no usa MTC.").c_str(),''', 'full footer dynamic')
t = replace_once(t,
'''  std::array<IRECT, 6U> mMidiRows{};
  std::array<IRECT, 6U> mMidiLearnButtons{};''',
'''  std::array<IRECT, 8U> mMidiRows{};
  std::array<IRECT, 8U> mMidiLearnButtons{};''', 'member arrays 8')
save(p, t)

p, t = load('product/AeylaVisualDmx/AeylaRuntimeStatusControl.h')
# Insert mapping snapshot immediately after existing midiShowEnabled snapshot.
t = replace_once(t,
'''    const bool recording = mPlug.TakeRecording();
    const bool midiShowEnabled = mPlug.ShowMidiMapping().enabled;''',
'''    const bool recording = mPlug.TakeRecording();
    const auto midiMapping = mPlug.ShowMidiMapping();
    const bool midiShowEnabled = midiMapping.enabled;''', 'runtime mapping snapshot')
start = t.find('    const std::string verifyLabel = recording')
end = t.find('    g.DrawText(IText(10.5F', start)
if start < 0 or end < 0:
    raise SystemExit('verify label block not found')
new = '''    const std::string captureKeys =
        "N" + std::to_string(midiMapping.capture_start_note) + " START / N" +
        std::to_string(midiMapping.capture_stop_note) + " STOP";
    const std::string verifyLabel = recording
        ? "R09.1 PRETEST  ·  " + captureKeys + "  ·  CAPTURANDO"
        : (midiShowEnabled
              ? "R09.1 PRETEST  ·  " + captureKeys + "  ·  LISTO"
              : "R09.1 PRETEST  ·  " + captureKeys + "  ·  MIDI SHOW OFF");
'''
t = t[:start] + new + t[end:]
start = t.find('    const std::string midiRecState = recording')
end = t.find('    g.DrawText(IText(9.5F', start)
if start < 0 or end < 0:
    raise SystemExit('midi rec state block not found')
new = '''    const std::string midiRecState = recording
        ? captureKeys + " · CAPTURANDO"
        : (midiShowEnabled
              ? captureKeys + " · LISTO"
              : captureKeys + " · ACTIVA MIDI SHOW");
'''
t = t[:start] + new + t[end:]
save(p, t)

# ---------------------------------------------------------------------------
# Tests: learnable capture commands and state 1.4 migration/persistence.
# ---------------------------------------------------------------------------
p, t = load('tests/test_show_midi_control.cpp')
start = t.find('  // Restored legacy collisions remain decodable; fixed operational commands')
end = t.find('  const auto stamped = make_show_midi_event(', start)
if start < 0 or end < 0:
    raise SystemExit('legacy collision test block not found')
new = r'''  // R09.1: REC boundaries are independently learnable. START/STOP remain
  // idempotent commands; learning one may never collide with another control.
  {
    ShowMidiMapping learned = mapping;
    check(assign_show_midi_note(learned, ShowMidiLearnTarget::capture_start,
                                12U, 74U, error) &&
              learned.capture_start_note == 74U,
          "REC START must be MIDI-learnable");
    check(assign_show_midi_note(learned, ShowMidiLearnTarget::capture_stop,
                                12U, 75U, error) &&
              learned.capture_stop_note == 75U,
          "REC STOP must be MIDI-learnable");
    check(match_show_midi_note(learned, 12U, 74U, 127U, match) &&
              match.command == ShowMidiCommand::capture_start,
          "learned REC START note must trigger capture start");
    check(match_show_midi_note(learned, 12U, 75U, 127U, match) &&
              match.command == ShowMidiCommand::capture_stop,
          "learned REC STOP note must trigger capture stop");
    const auto before = learned;
    check(!assign_show_midi_note(learned, ShowMidiLearnTarget::capture_stop,
                                 12U, 74U, error) && learned == before,
          "REC START/STOP collision must be rejected atomically");
    check(!assign_show_midi_note(learned, ShowMidiLearnTarget::capture_start,
                                 12U, kShowMidiPanicNote, error) &&
              learned == before && error.find("PANIC") != std::string::npos,
          "PANIC must remain unavailable to capture MIDI Learn");
  }

'''
t = t[:start] + new + t[end:]
# Old explicit fixed-reservation Learn assertions now only need collision behavior.
t = t.replace('''  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::stop_reset,
                               12U, kShowMidiCaptureStartNote, error) &&
            mapping == before_collision && error.find("REC START") != std::string::npos,
        "N42 REC START must not be learnable by another command");
  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::stop_reset,
                               12U, kShowMidiCaptureStopNote, error) &&
            mapping == before_collision && error.find("REC STOP") != std::string::npos,
        "N43 REC STOP must not be learnable by another command");
''', '''  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::stop_reset,
                               12U, mapping.capture_start_note, error) &&
            mapping == before_collision && !error.empty(),
        "REC START note must not collide with another command");
  check(!assign_show_midi_note(mapping, ShowMidiLearnTarget::stop_reset,
                               12U, mapping.capture_stop_note, error) &&
            mapping == before_collision && !error.empty(),
        "REC STOP note must not collide with another command");
''')
# Operational range collision list stays correct for defaults; PANIC is fixed.
save(p, t)

p, t = load('tests/test_plugin_state.cpp')
t = replace_once(t,
'''  state.show_midi.play_note = 72U;
  state.take_library_locator = "C:/AEYLA/Takes";''',
'''  state.show_midi.play_note = 72U;
  state.show_midi.capture_start_note = 74U;
  state.show_midi.capture_stop_note = 75U;
  state.take_library_locator = "C:/AEYLA/Takes";''', 'state custom capture notes')
t = t.replace('bytes.resize(bytes.size() - 16U);', 'bytes.resize(bytes.size() - 18U);', 1)
t = t.replace('payload_size -= 16U;', 'payload_size -= 18U;', 1)
t = t.replace('bytes.resize(bytes.size() - 14U);', 'bytes.resize(bytes.size() - 16U);', 1)
t = t.replace('payload_size -= 14U;', 'payload_size -= 16U;', 1)
t = t.replace('bytes.resize(bytes.size() - 6U);', 'bytes.resize(bytes.size() - 8U);', 1)
t = t.replace('payload_size -= 6U;', 'payload_size -= 8U;', 1)
t = replace_once(t,
'''    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.ok() && result.state == legacy12,
          "legacy 1.2 state must migrate with empty take bindings");
  }

  // Same-major future minor payloads may append fields and remain readable.''',
'''    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    auto expected = legacy12;
    expected.show_midi.capture_start_note = aeyla::runtime::kShowMidiCaptureStartNote;
    expected.show_midi.capture_stop_note = aeyla::runtime::kShowMidiCaptureStopNote;
    check(result.ok() && result.state == expected,
          "legacy 1.2 state must migrate with default capture notes and empty take bindings");
  }

  // R08 state 1.3 contains take bindings but predates learned REC notes. It
  // restores N42/N43 defaults without changing any existing take selection.
  {
    auto bytes = encoded.bytes;
    bytes.resize(bytes.size() - 2U);
    bytes[10] = 3U;
    bytes[11] = 0U;
    std::uint32_t payload_size = static_cast<std::uint32_t>(bytes[12]) |
                                 (static_cast<std::uint32_t>(bytes[13]) << 8U) |
                                 (static_cast<std::uint32_t>(bytes[14]) << 16U) |
                                 (static_cast<std::uint32_t>(bytes[15]) << 24U);
    payload_size -= 2U;
    bytes[12] = static_cast<std::uint8_t>(payload_size & 0xFFU);
    bytes[13] = static_cast<std::uint8_t>((payload_size >> 8U) & 0xFFU);
    bytes[14] = static_cast<std::uint8_t>((payload_size >> 16U) & 0xFFU);
    bytes[15] = static_cast<std::uint8_t>((payload_size >> 24U) & 0xFFU);
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    auto expected = state;
    expected.show_midi.capture_start_note = aeyla::runtime::kShowMidiCaptureStartNote;
    expected.show_midi.capture_stop_note = aeyla::runtime::kShowMidiCaptureStopNote;
    check(result.ok() && result.state == expected,
          "legacy 1.3 state must migrate to default capture notes");
  }

  // Same-major future minor payloads may append fields and remain readable.''', 'legacy 1.3 test')
save(p, t)

# Version identity: distinguish the learnable capture build from R09 fixed-note.
p, t = load('product/AeylaVisualDmx/config.h')
t = replace_once(t, '#define PLUG_VERSION_HEX 0x00000305', '#define PLUG_VERSION_HEX 0x00000306', 'version hex')
t = replace_once(t, '#define PLUG_VERSION_STR "0.3.5-alpha"', '#define PLUG_VERSION_STR "0.3.6-alpha"', 'version string')
save(p, t)

# Final static checks.
for rel in [
    'src/runtime/show_midi_control.h',
    'src/runtime/show_midi_control.cpp',
    'product/AeylaVisualDmx/AeylaMainControl.h',
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
]:
    text = (ROOT / rel).read_text(encoding='utf-8')
    if 'R08.2 PRETEST' in text:
        raise SystemExit(f'stale R08.2 identity in {rel}')

print('R09.1 learnable REC START/STOP patch complete')
