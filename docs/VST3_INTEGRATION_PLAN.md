# VST3 integration plan

## Role

The VST3 is a thin Ableton host adapter around the shared AEYLA runtime. It does not duplicate editor, semantic, project, media or DMX logic.

The VST3 shell must exist from the first integrated milestone on Windows and macOS. Deep runtime features may remain scaffolded, but host loading, MIDI receipt, state persistence and safe lifecycle behavior must be exercised early.

Read these specifications together:

1. [`VST3_PLATFORM_ARCHITECTURE.md`](VST3_PLATFORM_ARCHITECTURE.md) — concrete plugin topology, callback and threading contract.
2. [`VST3_STATE_LIFECYCLE_SPEC.md`](VST3_STATE_LIFECYCLE_SPEC.md) — persistence, reload and safety lifecycle.
3. [`PLUGIN_DEPENDENCY_DECISION.md`](PLUGIN_DEPENDENCY_DECISION.md) — iPlug2/VST3 SDK proof and pinning.
4. [`ABLETON_HOST_TEST_MATRIX.md`](ABLETON_HOST_TEST_MATRIX.md) — real host evidence.
5. [`CROSS_PLATFORM_BUILD_RELEASE.md`](CROSS_PLATFORM_BUILD_RELEASE.md) — artifacts, installers, signing and notarization.
6. [`BUG_PREVENTION_AND_QA.md`](BUG_PREVENTION_AND_QA.md) — quality gates.

## Required hosts

- Ableton Live 12 on Windows x64.
- Ableton Live 12 on macOS, running natively on Apple Silicon.
- Universal macOS plugin build (`arm64 + x86_64`) unless a later approved decision removes Intel.

Live 11 compatibility can be added as a separate matrix; it is not allowed to dilute the Live 12 baseline.

## First plugin topology

Implement a silent VST3 instrument/generator:

- one event input bus;
- no audio input;
- one stereo audio output bus;
- output buffers always zeroed and silence flags set;
- note-on, note-off and velocity accepted as VST3 events;
- requested process context limited to fields actually used;
- network, USB, project and media work isolated from the host callback.

This topology is provisional until it passes the real Ableton host matrix on both operating systems.

## Host inputs

- note-on/note-off/velocity;
- host sample position and tempo when transport-synchronized sources are selected;
- project locator plus project UUID/checksum;
- output arm, blackout and grand master parameters.

## Host outputs

- guaranteed silent stereo audio;
- status UI;
- DMX through shared backend threads only.

## Host callback handoff

The callback converts VST3 events to `aeyla::runtime::HostEvent` and pushes them into the fixed-capacity `SpscQueue` in `src/runtime`.

The callback may only:

- clear audio buffers/set silence flags;
- inspect bounded VST3 event/parameter data;
- inspect valid requested process-context fields;
- enqueue compact host events;
- update atomics.

It may not allocate, lock, block, log synchronously, parse files, decode media, use sockets/USB, create/join threads or call UI code.

Queue overflow triggers a documented safety response; it is never ignored silently.

## State

VST3 component state stores:

- state format and plugin versions;
- project UUID/schema/checksum;
- portable project locator;
- safe global parameters;
- backend configuration values, not device handles;
- no restored armed state.

Controller state stores UI-only presentation state. Project loading/reload occurs asynchronously and publishes a fully validated immutable snapshot.

## Framework and target policy

Use one pinned iPlug2 commit with a pinned official VST3 SDK after the proof gates in `PLUGIN_DEPENDENCY_DECISION.md` pass.

The framework must produce from one source architecture:

- Windows standalone;
- macOS standalone;
- Windows VST3;
- macOS universal VST3.

Framework types remain in adapters/UI and do not leak into shared project/core/runtime/io models.

## Stage 1 host spine

The first VST3 implementation is deliberately small but real:

1. Build Windows and macOS binaries.
2. Pass Steinberg Validator on both.
3. Scan and load in real Ableton on Windows and native Apple Silicon macOS.
4. Expose stable Output Arm, Blackout and Grand Master parameters.
5. Receive note-on/off/velocity and enqueue shared host events.
6. Produce one deterministic semantic/DMX test frame through the shared engine; hardware output may remain disabled.
7. Save/reopen an Ableton Set safely.
8. Copy the device/track to another Set predictably.
9. Close/unload with transient release, haze off and output safe.
10. Compare canonical standalone and VST3 event/DMX captures.

This stage occurs before full editor, media or USB implementation.

## Acceptance language

- **Scaffolded:** binaries exist but host behavior is incomplete.
- **Implemented/Windows or macOS:** platform CI build, automated adapter tests and Validator pass.
- **Host-tested/Windows:** real Windows Ableton scan/load/MIDI/state evidence exists.
- **Host-tested/macOS:** real native Apple Silicon Ableton evidence exists.
- **Hardware-tested:** named output hardware receives verified frames.
- **Show-tested:** full final session passes rehearsal/show soak and recovery drills.

Never call the plugin Ableton-compatible based only on compilation, Wine, Validator or another DAW.
