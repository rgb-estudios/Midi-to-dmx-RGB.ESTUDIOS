# VST3 integration plan

## Role

The VST3 is a thin host adapter around the shared runtime. It does not duplicate editor logic.

The VST3 shell must exist from the first integrated milestone on both Windows and macOS. Deep runtime features may remain scaffolded, but host loading, MIDI receipt, state persistence and safe lifecycle behavior must be exercised early so Ableton compatibility is not deferred until the end of the project.

## Required hosts

- Ableton Live on Windows x64.
- Ableton Live on macOS, including Apple Silicon native execution where supported by the selected framework and dependencies.

Exact minimum Ableton and operating-system versions must be pinned before the first Host-tested claim. CI compilation alone is insufficient.

## Host inputs

- note-on/note-off/velocity;
- host sample position and tempo when transport-synced source is selected;
- project path or embedded project snapshot;
- output arm/blackout/runtime parameters.

## Host outputs

- no audio;
- status UI only;
- DMX through shared backend threads.

## State

VST3 state stores:

- runtime settings;
- project UUID/version/checksum;
- portable project reference or embedded package according to final policy;
- output override;
- no live device handle;
- no absolute path as the only project reference.

## Framework and target policy

Use one pinned iPlug2 commit or another explicitly approved cross-platform framework capable of producing:

- Windows standalone;
- macOS standalone;
- Windows VST3;
- macOS VST3.

The framework decision must document:

- exact commit/version;
- licence;
- Windows compiler and SDK;
- macOS/Xcode deployment target;
- Intel/Apple Silicon architecture policy;
- VST3 SDK integration;
- signing/notarization implications;
- reproducible build commands.

Do not create separate product engines for standalone and VST3 or for Windows and macOS.

## Stage 1 host spine

The first VST3 implementation must be deliberately small but real:

1. Plugin scans and loads in Ableton on Windows and macOS.
2. Plugin exposes persistent output-arm and blackout parameters.
3. Plugin receives note-on, note-off and velocity.
4. MIDI events enter the shared runtime command interface.
5. A canonical test executor produces a deterministic semantic/DMX frame without performing socket work in the audio callback.
6. Save/reopen of the Ableton Set preserves the expected state.
7. Closing or unloading the plugin leaves the backend in a safe state.

This stage should occur before the full editor, media engine or USB backends are complete.

## Threading contract

The host callback may only:

- decode/copy bounded MIDI and host timing data;
- enqueue commands into a bounded lock-free or proven non-blocking handoff;
- read immutable status snapshots where safe.

The host callback may not:

- open/read/write files;
- allocate unbounded memory;
- decode images or video;
- enumerate devices;
- open or write sockets/USB;
- wait on mutexes controlled by non-real-time threads;
- reload projects directly.

## Validation matrix

### Automated/build

- Linux shared-core CI.
- Windows MSVC VST3 and standalone build.
- macOS VST3 and standalone build.
- Steinberg validator on each produced VST3 where available.
- architecture-specific checks for macOS outputs.

### Real host validation

On both Windows and macOS:

- Ableton plugin scan.
- Initial load.
- MIDI note-on/off and velocity capture.
- Save/reopen Set.
- Copy track/device to another Set.
- Missing project and changed project behavior.
- Output starts disarmed.
- Blackout priority.
- Stop/start transport policy.
- Plugin unload/host close safety.
- DMX capture comparison against standalone.

## Acceptance language

- **Scaffolded:** binaries exist but host behavior is incomplete.
- **Implemented:** CI build and automated adapter tests pass for both Windows and macOS.
- **Host-tested:** real Ableton scan/load/save/reopen and MIDI/DMX evidence exists for the named OS and Ableton version.
- **Show-tested:** the plugin is used successfully in the final session under full show load.

Do not call the VST3 compatible with Ableton based only on the VST3 validator or another DAW.
