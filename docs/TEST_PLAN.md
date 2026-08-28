# Test plan

## Required platform matrix

Every integrated milestone must identify results separately for:

| Path | Required from first integrated milestone |
|---|---|
| Windows standalone | Yes |
| macOS standalone | Yes |
| Windows Ableton + VST3 | Yes |
| macOS Ableton + VST3 | Yes |
| Linux shared core/CI | Development support only |

A passing result on one operating system does not imply support on another. A CI build does not replace a real Ableton host test.

## Automated unit tests

- Semantic attribute clamp/read/write.
- RGB→RGBWAL transformation.
- Profile reorder preserves semantic result.
- Missing dimmer scales emitters.
- Range, constant, trigger, invert and home mapping.
- Address bounds and overlapping fixtures warnings.
- Rig 10 retains 14 logical positions but 10 active physical outputs.
- ArtDMX header, opcode, version, universe, sequence and length.
- Project migration and checksum validation.
- Executor trigger modes and priority resolution.
- Standalone and VST3 adapters produce identical runtime commands.
- Windows and macOS platform adapters normalize paths and network/device identifiers without changing project semantics.

## Golden tests

Persist known semantic frames and expected DMX frames for fixture profiles. Any difference requires deliberate approval and changelog entry.

The same golden inputs must be executed on Windows and macOS. Output must be byte-identical unless an explicitly documented platform representation does not enter the persisted or DMX data.

## Build and packaging tests

### Windows

- MSVC release build.
- Standalone launch smoke test.
- VST3 build and validator.
- Installer or development package extraction test.

### macOS

- Xcode/Clang release build.
- Standalone launch smoke test.
- VST3 build and validator.
- Architecture inspection for required Intel/Apple Silicon targets.
- App/plugin packaging and development signing/notarization behavior documented.

### CI

- Linux, windows-latest and macos-latest build/test matrix.
- Upload standalone/VST3 artifacts for Windows and macOS when those targets exist.
- CI status must name platform-specific failures instead of collapsing them into one generic status.

## UI tests

On Windows and macOS:

- 1366×768/1920×1080 or equivalent scaled viewport screenshots.
- Keyboard navigation.
- Profile reorder and bulk replacement.
- Missing-media presentation.
- Output state visibility.
- Project export/import round trip.
- Layout and typography parity review.
- Native file dialog and path behavior.

## Integration tests

Across all four required paths:

- Standalone and VST3 load identical project.
- Same MIDI sequence produces byte-identical executor state and DMX capture.
- Hot reload does not emit unsafe intermediate values.
- Output starts disarmed.
- Blackout overrides all artistic layers.
- Missing project and changed project states are explicit.
- Project edited/exported on Windows opens on macOS and vice versa.

## Ableton host tests

Run on one real Windows machine and one real Mac before any Host-tested claim. Record exact Ableton, OS, architecture, plugin build and project checksum.

- Plugin scan succeeds.
- Plugin loads in a MIDI track without audio requirements.
- Note-on, note-off and velocity reach the shared runtime.
- Host transport values are read only when required.
- Set save/reopen restores intended state.
- Device/track copy to another Set behaves correctly.
- Reload of corrected `.aeylashow` does not require editing MIDI clips.
- Ableton stop/start policy is deterministic.
- Plugin unload, Ableton close and crash-recovery paths leave output safe.
- Full session audio/video load does not cause DMX stalls.

Wine, another DAW or the Steinberg validator may supplement but cannot replace these tests.

## Hardware tests

Named evidence required for:

- Art-Net node model/firmware on Windows and macOS where applicable.
- Network adapter and OS version.
- DMX USB Pro model/driver on each supported OS.
- Open DMX model/driver where implemented.
- Every fixture profile used in the show.

Vendor lack of support for an OS/device combination must be documented as an explicit limitation; it must not silently disable the backend.

## Performance tests

For Windows standalone, macOS standalone, Windows Ableton and macOS Ableton:

- Two-hour continuous runtime.
- 44 Hz output timing distribution.
- CPU/GPU at full Ableton + video load for host paths.
- Media underrun count.
- MIDI-to-DMX latency.
- Project hot reload.
- Memory growth and shutdown time.

## Safety tests

Across all required execution paths:

- Startup disarmed.
- Blackout priority.
- Haze emergency-off.
- Reset protected trigger.
- Cable/network/device loss.
- Host crash/close.
- Reopen after unclean shutdown.
- Switching project or output backend cannot expose stale unsafe frames.
