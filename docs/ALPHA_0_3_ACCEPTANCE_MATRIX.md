# AEYLA Visual DMX — Alpha 0.3 acceptance matrix

This matrix is the release evidence record for the four mandatory product paths.

## Status definitions

- `NOT STARTED`
- `SOURCE-REVIEWED`
- `UNIT-TESTED`
- `CI-BUILT`
- `CI-LAUNCHED`
- `TARGET-LAUNCH-TESTED`
- `HOST-TESTED`
- `HARDWARE-TESTED`
- `FAILED`

Record exact versions, commit, project checksum and evidence location. Never infer one platform from another.

## Current baseline

| Test | Windows standalone | macOS standalone | Windows Ableton + VST3 | macOS Ableton + VST3 |
|---|---|---|---|---|
| Native target builds | CI-BUILT | CI-BUILT | CI-BUILT | CI-BUILT |
| Native process/UI launch | TARGET-LAUNCH-TESTED (`0.2.0-rsrcfix1`) / source CI screenshot FAILED | NOT STARTED | NOT STARTED | NOT STARTED |
| Clean install/extraction | TARGET-LAUNCH-TESTED from ZIP | NOT STARTED | NOT STARTED | NOT STARTED |
| Canonical `.aeylashow` loads | NOT STARTED | NOT STARTED | NOT STARTED | NOT STARTED |
| Project save/reopen | NOT STARTED | NOT STARTED | NOT STARTED | NOT STARTED |
| MIDI Note On/Off reaches shared ingress | NOT STARTED | NOT STARTED | SOURCE-REVIEWED UI proof only | SOURCE-REVIEWED UI proof only |
| Same executor state from golden MIDI | NOT STARTED | NOT STARTED | NOT STARTED | NOT STARTED |
| Byte-identical golden DMX frame | NOT STARTED | NOT STARTED | NOT STARTED | NOT STARTED |
| Output starts disarmed | SOURCE-REVIEWED | SOURCE-REVIEWED | SOURCE-REVIEWED | SOURCE-REVIEWED |
| Blackout end-to-end | NOT STARTED | NOT STARTED | NOT STARTED | NOT STARTED |
| Host/plugin scan | N/A | N/A | NOT STARTED | NOT STARTED |
| Plugin opens UI | N/A | N/A | NOT STARTED | NOT STARTED |
| Silent audio output | N/A | N/A | SOURCE-REVIEWED | SOURCE-REVIEWED |
| Ableton Set save/reopen | N/A | N/A | NOT STARTED | NOT STARTED |
| Device/track copy behavior | N/A | N/A | NOT STARTED | NOT STARTED |
| Plugin unload/host close safe | N/A | N/A | NOT STARTED | NOT STARTED |
| Art-Net packet receiver test | NOT STARTED | NOT STARTED | NOT STARTED | NOT STARTED |
| Named physical Art-Net node | NOT STARTED | NOT STARTED | NOT STARTED | NOT STARTED |
| Two-hour soak | NOT STARTED | NOT STARTED | NOT STARTED | NOT STARTED |

## Required machine record

### Windows test machine

- Machine: Claudio Huichalaf Lenovo Legion 5i Gen 10
- OS observed for `0.2.0-rsrcfix1`: Microsoft Windows NT 10.0.26200.0
- Architecture: x86-64
- GPU/driver: pending capture
- Display scaling/resolution: pending capture
- Ableton Live exact version: pending capture
- Network adapter/driver: pending capture

### macOS test machine

- Model: pending
- macOS version: pending
- Architecture: Apple Silicon required; Intel compatibility recorded separately
- Display scaling/resolution: pending
- Ableton Live exact version: pending
- Network adapter: pending

## Standalone acceptance procedure

For Windows and macOS independently:

1. Start from a clean extracted/installed package.
2. Launch without audio hardware or Ableton.
3. Confirm visible project, version, backend and output state.
4. Confirm output starts disarmed and blackout enabled.
5. Open the canonical project.
6. Select all 14 logical sample points and verify Rig 10 ghost behavior.
7. Reorder a fixture profile and confirm the semantic look is preserved.
8. Trigger the golden MIDI sequence and capture executor state and DMX frame.
9. Save As, close and reopen.
10. Open the project saved by the other operating system.
11. Test invalid project, missing media and unsupported schema messages.
12. Test keyboard navigation and 1366×768/equivalent scaled layout.
13. Test project reload while disarmed and while armed.
14. Test blackout, shutdown and unclean restart.

## Ableton/VST3 acceptance procedure

For Windows and macOS independently:

1. Install the exact build in the standard VST3 location.
2. Run Steinberg Validator and retain the report.
3. Launch the recorded Ableton Live version and force a rescan.
4. Confirm AEYLA appears as a MIDI/instrument device.
5. Insert it on a MIDI track without requiring an audio input.
6. Confirm stereo output remains silent.
7. Load the canonical `.aeylashow`.
8. Send the golden MIDI clip including Note On, Note Off and velocity.
9. Capture shared ingress, executor state and DMX frame.
10. Close the plugin UI and repeat the MIDI test.
11. Save, close and reopen the Ableton Set.
12. Duplicate the device/track and document project/output ownership behavior.
13. Stop/start transport according to the defined policy.
14. Hot reload the corrected project without editing MIDI clips.
15. Remove the device and close Ableton; verify safe output.
16. Repeat under representative audio/video session load.

## Golden parity record

The following artifacts must be generated from one canonical project and one canonical MIDI sequence:

- project SHA-256;
- MIDI sequence SHA-256;
- ordered runtime-command log;
- executor/layer snapshot;
- semantic fixture frames for all 14 logical fixtures;
- 512-byte DMX frame capture;
- ArtDMX packet capture when output is enabled.

All four paths must match byte-for-byte where the artifact is platform-neutral.

## Hardware acceptance

No hardware status may be recorded without:

- manufacturer and exact model;
- firmware/driver version;
- connection path and network plan;
- fixture model and exact channel mode;
- capture/report location;
- blackout, disconnect, reconnect and soak results.
