from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
path = ROOT / "product/AeylaVisualDmx/AeylaMidiShowIntegration.cpp"
text = path.read_text(encoding="utf-8")

text = text.replace(
    '"MIDI SHOW ACTIVO · canal " + std::to_string(mapping.channel) +\n            " · N41 PANIC · N42 GRABAR · reloj por muestras del DAW"',
    '"MIDI SHOW ACTIVO · canal " + std::to_string(mapping.channel) +\n            " · N41 PANIC · N42 REC START · N43 REC STOP · reloj por muestras del DAW"')

start_marker = "  const auto apply_midi_capture_toggle = [&]() {"
end_marker = "\n  if(mShowMidiIngress.consume_panic_request())"
start = text.find(start_marker)
end = text.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("capture lambda markers not found")

new_block = r'''  const auto apply_midi_capture_command = [&](aeyla::runtime::ShowMidiCommand command) {
    const bool request_start = command == aeyla::runtime::ShowMidiCommand::capture_start;
    const bool request_stop = command == aeyla::runtime::ShowMidiCommand::capture_stop;
    if(!request_start && !request_stop)
      return;

    if(NetworkConfigurationBusy()) {
      SetShowMidiMessage("MIDI REC BLOQUEADO · espera a que termine el cambio de red");
      return;
    }

    const auto snapshot = mModel.snapshot();
    const std::string project_id = snapshot.project_id;
    const std::string song_id = snapshot.active_song_id;
    const std::string song_name = snapshot.active_song_name;
    const std::uint16_t universe = mModel.project_document().output.universe;
    aeyla::take_library_session::ensure_scope(this, project_id);

    if(request_stop) {
      if(!mArtNetCapture.streamed_recording_active()) {
        SetShowMidiMessage("REC STOP IGNORADO · no existe una captura MIDI activa");
        return;
      }
      const auto expected_target = mActiveCaptureTarget;
      std::string error;
      if(!mArtNetCapture.end_streamed_recording(error)) {
        mActiveCaptureTarget.clear();
        mCaptureSyncAnchor.reset();
        aeyla::take_library_session::set_storage_message(this, "ERROR DE GRABACIÓN MIDI · " + error);
        SetShowMidiMessage("REC STOP · no fue posible cerrar la toma · " + error);
        return;
      }
      mActiveCaptureTarget.clear();
      const auto library = aeyla::take_library_session::directory(this);
      const auto scan = aeyla::capture::scan_take_directory(library, song_id);
      const auto captured = aeyla::capture::find_take_entry_by_path(scan, expected_target);
      if(!scan.ok() || !captured.has_value()) {
        mCaptureSyncAnchor.reset();
        SetShowMidiMessage("REC STOP · archivo final no verificable · " +
            (scan.ok() ? std::string("no apareció en el índice") : scan.error));
        return;
      }
      const auto& newest = *captured;
      aeyla::take_library_session::TakeEditState edit;
      edit.path = newest.path;
      edit.take_name = newest.take_name;
      edit.raw_source = true;
      edit.start_frame = 0U;
      edit.end_frame_exclusive = newest.frame_count;
      edit.frame_count = newest.frame_count;
      edit.frames_per_second = newest.frames_per_second;
      edit.version_count = scan.entries.size();
      edit.version_index = 0U;
      aeyla::take_library_session::set_edit_state(this, song_id, edit);
      aeyla::take_library_session::set_loaded_path(this, song_id, newest.path);
      mCaptureSyncAnchor.reset();
      mMidiPreflightCursor.store(ShowMidiMapping().enabled ? 0 : -1, std::memory_order_release);
      const std::string sync_text = " · CERO = REC START · cuadro 0";
      aeyla::take_library_session::set_storage_message(
          this, "GUARDADA POR MIDI · " + newest.path.filename().string() + sync_text);
      SetShowMidiMessage("REC STOP · " + newest.take_name + " · " +
          std::to_string(newest.frame_count) + " cuadros" + sync_text + " · RAW preservado");
      return;
    }

    if(mArtNetCapture.streamed_recording_active()) {
      SetShowMidiMessage("REC START IGNORADO · la captura ya está activa · usa N43 para detener");
      return;
    }
    if(mArtNetCapture.stats().recording) {
      SetShowMidiMessage("REC START BLOQUEADO · existe otra captura activa; deténla desde su origen");
      return;
    }
    if(OutputArmed() || TakeOutputArmed()) {
      SetShowMidiMessage("REC START BLOQUEADO · desarma la salida física antes de capturar");
      return;
    }
    if(TakePlaying()) {
      SetShowMidiMessage("REC START BLOQUEADO · detén la reproducción de la toma actual");
      return;
    }
    if(song_id.empty()) {
      SetShowMidiMessage("REC START BLOQUEADO · selecciona primero una canción");
      return;
    }
    const auto library = aeyla::take_library_session::directory(this);
    if(library.empty()) {
      SetShowMidiMessage("REC START BLOQUEADO · selecciona una vez la BIBLIOTECA desde la interfaz");
      return;
    }
    std::string directory_error;
    if(!aeyla::capture::prepare_take_directory(library, directory_error)) {
      SetShowMidiMessage("REC START BLOQUEADO · biblioteca sin escritura · " + directory_error);
      return;
    }
    const auto stats = mArtNetCapture.stats();
    if(!stats.running) {
      SetShowMidiMessage("REC START BLOQUEADO · RX Art-Net no está activo · REESCANEA la red");
      return;
    }
    if(!stats.signal_present || stats.source_ipv4.empty()) {
      SetShowMidiMessage("REC START BLOQUEADO · RX sin señal Art-Net válida de Avolites");
      return;
    }
    const auto scan = aeyla::capture::scan_take_directory(library, song_id);
    const std::size_t next_number = scan.ok() ? scan.entries.size() + 1U : 1U;
    const std::string take_name = "Toma " + std::to_string(next_number);
    const auto target = aeyla::capture::make_take_file_path(
        library, song_name.empty() ? song_id : song_name, take_name);
    aeyla::capture::DmxTakeStreamConfig stream;
    stream.target_path = target;
    stream.song_id = song_id;
    stream.song_name = song_name;
    stream.take_name = take_name;
    stream.source_ipv4 = stats.source_ipv4;
    stream.port_address = universe;
    stream.frames_per_second = 44U;

    // REC START itself is the capture origin. No MTC, PLAY edge or N38 marker
    // is allowed to move CERO after recording begins.
    mCaptureSyncAnchor.reset();
    std::string error;
    if(!mArtNetCapture.begin_streamed_recording(stream, error)) {
      mActiveCaptureTarget.clear();
      SetShowMidiMessage("REC START BLOQUEADO · no se pudo iniciar captura · " + error);
      return;
    }
    mActiveCaptureTarget = target;
    aeyla::take_library_session::set_storage_message(
        this, "GRABANDO POR MIDI · " + target.filename().string() + " · CERO = REC START");
    SetShowMidiMessage("REC START · " + take_name + " · " + stats.source_ipv4 +
        " · 44 Hz · CERO fijado por la nota MIDI · N43 detiene");
  };
'''
text = text[:start] + new_block + text[end:]

old_ready = '''    // CAPTURE is an operational disk command, not an artistic event. Execute it
    // immediately on the runtime worker so a stopped/suspended DAW does not
    // require a second audio block merely to start REC. All artistic commands
    // retain sample-ready scheduling.
    if(pending_command != aeyla::runtime::ShowMidiCommand::capture_toggle &&
       !aeyla::runtime::show_midi_event_ready(
           completed, *mPendingShowMidiEvent))
      return;'''
new_ready = '''    // REC START/STOP are operational disk commands, not artistic transport
    // events. Execute them immediately on the runtime worker. Artistic commands
    // retain sample-ready scheduling.
    const bool capture_command =
        pending_command == aeyla::runtime::ShowMidiCommand::capture_start ||
        pending_command == aeyla::runtime::ShowMidiCommand::capture_stop;
    if(!capture_command &&
       !aeyla::runtime::show_midi_event_ready(completed, *mPendingShowMidiEvent))
      return;'''
if old_ready not in text:
    raise SystemExit("ready scheduling block not found")
text = text.replace(old_ready, new_ready, 1)

old_dispatch = '''    if(event.command == aeyla::runtime::ShowMidiCommand::capture_toggle) {
      apply_midi_capture_toggle();
      continue;
    }'''
new_dispatch = '''    if(event.command == aeyla::runtime::ShowMidiCommand::capture_start ||
       event.command == aeyla::runtime::ShowMidiCommand::capture_stop) {
      apply_midi_capture_command(event.command);
      continue;
    }'''
if old_dispatch not in text:
    raise SystemExit("capture dispatch block not found")
text = text.replace(old_dispatch, new_dispatch, 1)

record_start = text.find("    if(TakeRecording()) {", text.find(new_dispatch))
record_end = text.find("\n    switch(event.command)", record_start)
if record_start < 0 or record_end < 0:
    raise SystemExit("recording artistic block not found")
text = text[:record_start] + '''    if(TakeRecording()) {
      SetShowMidiMessage(
          "MIDI SHOW IGNORADO · durante REC sólo N43 REC STOP controla la captura");
      continue;
    }
''' + text[record_end:]

old_switch = '''      case aeyla::runtime::ShowMidiCommand::capture_toggle:
        apply_midi_capture_toggle();
        break;'''
new_switch = '''      case aeyla::runtime::ShowMidiCommand::capture_start:
      case aeyla::runtime::ShowMidiCommand::capture_stop:
        apply_midi_capture_command(event.command);
        break;'''
if old_switch not in text:
    raise SystemExit("capture switch case not found")
text = text.replace(old_switch, new_switch, 1)

if "capture_toggle" in text or "apply_midi_capture_toggle" in text:
    raise SystemExit("stale capture toggle remains in integration")
if "PLAY/N38 fijará el cero" in text or "usa N38 para fijar el cero" in text:
    raise SystemExit("stale transport capture-zero language remains")
path.write_text(text, encoding="utf-8")

# Make the architecture visible and remove MTC from the operator-facing UI.
main = ROOT / "product/AeylaVisualDmx/AeylaMainControl.h"
ui = main.read_text(encoding="utf-8")
ui = ui.replace("RGB ESTUDIOS · R07 PRETEST", "RGB ESTUDIOS · R09 PRETEST · MIDI REC DIRECT")
ui = ui.replace(
    '"MTC: mismo DAW → Avolites · MIDI no arma Art-Net ni desactiva APAGÓN"',
    '"CAPTURA: N42 REC START · N43 REC STOP · CERO = REC START"')
ui = ui.replace(
    '"MTC puede salir del mismo DAW hacia Avolites. AEYLA no lo recircula: la nota MIDI y el audio comparten el reloj más preciso del host."',
    '"CAPTURA DMX: N42 REC START fija CERO · N43 REC STOP finaliza · AEYLA no usa MTC."')
ui = ui.replace("MTC/safety footer", "capture/safety footer")
if "MTC:" in ui or "MTC puede" in ui:
    raise SystemExit("stale MTC UI remains")
main.write_text(ui, encoding="utf-8")

status = ROOT / "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h"
beacon = status.read_text(encoding="utf-8")
beacon = beacon.replace("R08.2 verification beacon", "R09 verification beacon")
beacon = beacon.replace("R08.2 PRETEST  ·  N42 MIDI REC  ·  CAPTURANDO",
                        "R09 PRETEST  ·  N42 START / N43 STOP  ·  CAPTURANDO")
beacon = beacon.replace("R08.2 PRETEST  ·  N42 MIDI REC  ·  LISTO",
                        "R09 PRETEST  ·  N42 START / N43 STOP  ·  LISTO")
beacon = beacon.replace("R08.2 PRETEST  ·  N42 MIDI REC  ·  MIDI SHOW OFF",
                        "R09 PRETEST  ·  N42 START / N43 STOP  ·  MIDI SHOW OFF")
beacon = beacon.replace("fixed N42 capture trigger state", "fixed N42/N43 capture trigger state")
beacon = beacon.replace("N42 MIDI REC · CAPTURANDO", "N42 START · N43 STOP · CAPTURANDO")
beacon = beacon.replace("N42 MIDI REC · LISTO", "N42 START · N43 STOP · LISTO")
beacon = beacon.replace("N42 MIDI REC · ACTIVA MIDI SHOW", "N42 START · N43 STOP · ACTIVA MIDI SHOW")
status.write_text(beacon, encoding="utf-8")

print("patched R09 runtime + UI; MTC removed from operator flow")
