# VST3 state, lifecycle and safety specification

## 1. Objective

Ableton must be able to save, close, reopen, duplicate and transfer the AEYLA device without producing an unsafe lighting state or requiring the designer to edit the Ableton Set.

The plugin state and the `.aeylashow` project are related but not identical:

- Ableton/VST3 state identifies the project and stores host-specific settings.
- `.aeylashow` contains the authored show, semantic profiles, patch, executors and media package.
- Neither state stores open sockets, USB handles, threads or decoded media buffers.

## 2. Safe lifecycle state machine

```text
Created
  -> Initialized (output disarmed, blackout active internally)
  -> Project validating
  -> Ready / Disarmed
  -> Armed
  -> Running
  -> Stopping / transient release
  -> Ready / Disarmed
  -> Terminated

Any error
  -> Safe Error State
```

### Non-negotiable rules

- Output Arm always starts off after instantiation, reload, crash recovery and Set reopen.
- Haze, reset, macros and strobe start at safe values.
- Missing/corrupt/incompatible project cannot arm.
- Hot reload cannot emit partially loaded frames.
- Plugin unload sends the configured safe frame from the output thread when possible, then closes the backend.
- The callback never waits for lifecycle operations.

## 3. VST3 component state

The processor component state is a versioned binary structure with explicit endianness and bounds checks.

Minimum fields:

- magic bytes and state format version;
- plugin build version;
- project UUID;
- expected `.aeylashow` schema major/minor;
- project package checksum;
- project locator policy and locator value;
- grand master value;
- blackout preference;
- output backend configuration override, excluding secrets and handles;
- compatibility flags;
- reserved length for forward-compatible sections.

Fields deliberately excluded:

- `Output Arm = true`;
- active socket/USB handle;
- absolute media paths outside the project package;
- decoded frames;
- UI geometry;
- transient note/executor state;
- last haze/strobe/reset value;
- passwords or signing credentials.

On state restore, stored arm state is ignored and the runtime enters `Ready / Disarmed`.

## 4. VST3 controller state

Controller state is UI-only:

- selected panel/tab;
- window size;
- canvas zoom and pan;
- last inspected logical fixture;
- optional non-safety display preferences.

Controller state must not contain the authoritative project, output arm, blackout, patch or runtime state.

## 5. Persistence sequence

Implement and test the VST3 persistence sequence:

### Save

1. Processor `getState` serializes component state.
2. Controller `getState` serializes UI-only state.

### Load

1. Processor `setState` validates and stages component state.
2. Controller `setComponentState` synchronizes host-visible parameters.
3. Controller `setState` restores UI-only state.
4. Project loading occurs outside the host callback.
5. Runtime publishes a complete validated snapshot atomically.
6. Output remains disarmed.

All readers reject truncated, oversized, corrupted and unsupported-major-version state without undefined behavior.

## 6. Project locator policy

The first implementation supports these locator modes:

1. **Absolute development path** — allowed only for local development; export validation warns and release packaging rejects it.
2. **Relative companion path** — preferred for the final Ableton session and correction workflow.
3. **User project library ID** — optional later mapping to a configured local library.

The plugin state stores project UUID and checksum in addition to the path. A file at the same path with a different project UUID must not load silently.

Media are not embedded inside the Ableton Set. The `.aeylashow` package must contain or explicitly declare its media. Corrections are delivered by replacing a complete project package and using controlled reload.

## 7. Controlled reload

Reload is asynchronous and transactional:

1. UI or host parameter requests reload.
2. Loader thread reads the candidate package.
3. Schema, checksum, profiles, patch, MIDI map and media references are validated.
4. Candidate runtime snapshot is built off-thread.
5. Runtime reaches a safe frame boundary.
6. Transient executors release; haze goes to zero.
7. One atomic snapshot swap occurs.
8. The new project remains disarmed unless the operator explicitly arms it.
9. On failure, the last known-good project remains loaded and a persistent error is shown.

Never mutate the live project graph in place.

## 8. Missing or invalid project

Behavior:

- show project UUID/checksum expected by the Set;
- show attempted path and reason without exposing private data in logs;
- remain disarmed;
- compile and publish only the all-zero/safe frame;
- allow browsing for a replacement package from the UI thread;
- require explicit operator confirmation when project UUID differs;
- never create an empty replacement automatically.

## 9. Host transport lifecycle

### Host starts processing

- Do not automatically arm.
- Resume runtime tick if project is valid.
- Apply current transport position to synchronized sources.

### Host stops processing or transport

Default policy:

- release momentary, flash and strobe layers;
- haze output to zero;
- preserve/fade base scene according to project setting;
- retain network backend only if output remains explicitly armed;
- never infer that transport stop means process termination.

### Host bypass/device off

Bypass/device-off behavior must be tested in Ableton. The safe target is transient release and haze off. The plugin cannot assume the host continues calling `process()` indefinitely after device deactivation, so the control path must receive lifecycle callbacks and the output thread must own its shutdown policy.

### Offline export/freeze

External DMX output is disabled by default during offline processing or export. Rendering a song faster than real time must not send accelerated lighting commands.

## 10. Parameter policy

Host-automatable parameters are stable IDs and never reordered after release.

- Output Arm: discrete; state restore forces off.
- Blackout: discrete; safety priority; may restore on.
- Grand Master: continuous normalized.
- Optional safe global values only.

Changing project/scene/preset must not be implemented as an automatable parameter that mutates many other automated parameters. Scene execution remains event/executor driven.

## 11. Plugin UID and compatibility

Processor and controller UIDs become permanent once an external Ableton Set is saved. Changing a UID makes Ableton treat the build as a different plugin.

Before the first distributable alpha:

- generate and record permanent processor/controller UIDs;
- record vendor name and bundle identifiers;
- freeze parameter IDs;
- define state major/minor migration rules;
- add a compatibility fixture containing saved states from every released alpha.

## 12. Error taxonomy

- Project error: invalid/missing package; disarmed, last known-good retained.
- Backend error: network/USB unavailable; runtime remains alive, output marked failed.
- Realtime overflow: release transient layers/haze and warn persistently.
- State error: reject state, use safe defaults, never partially apply.
- Media error: affected source falls back to defined safe color/black; project remains explicit about degradation.
- Fatal initialization error: plugin loads a diagnostic UI if possible but remains silent and disarmed.

## 13. Test requirements

- Save/reopen Ableton Set.
- Duplicate device and track.
- Copy track to another Set.
- Replace project package without editing notes.
- Missing project at load.
- Wrong UUID at same path.
- Corrupt/truncated/oversized VST3 state.
- Previous state schema migration.
- Hot reload while stopped, playing and looped.
- Device bypass/on/off.
- Ableton close with output armed.
- Plugin update with existing Set.
- Offline export cannot emit external DMX.
- Project loader cancellation and rapid repeated reload requests.

## 14. Primary references

- VST3 persistence: https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Persistence.html
- VST3 first-plugin state example: https://steinbergmedia.github.io/vst3_dev_portal/pages/Tutorials/Code%2Byour%2Bfirst%2Bplug-in.html
- VST3 processor/controller architecture: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/API%2BDocumentation/Index.html
- VST3 parameters/automation: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Parameters%2BAutomation/Index.html
