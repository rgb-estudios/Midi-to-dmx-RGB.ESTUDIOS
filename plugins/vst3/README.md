# AEYLA VST3 adapter

This directory will contain only the thin framework/host adapter. It must not contain a second artistic runtime, fixture compiler, project model, media engine or output implementation.

## Required dependency direction

```text
plugins/vst3
  -> src/runtime
  -> src/core

plugins/vst3
  -> src/io only through runtime-owned non-realtime interfaces
```

Shared libraries must never depend on `plugins/vst3` or iPlug2 types.

## First target

Build a silent VST3 instrument/generator on Windows and macOS:

- one event input bus;
- no audio input;
- one stereo silent output;
- Note On, Note Off and velocity -> `HostEventIngress`;
- Output Arm, Blackout and Grand Master parameters;
- requested transport context fields only;
- versioned component/controller state;
- no real network output required for the first host scan proof.

## Expected adapter responsibilities

### Processor

- register event/audio buses;
- clear output buffers and set silence flags;
- read VST3 note events and process context;
- normalize and enqueue `HostEvent` records;
- read bounded parameter changes;
- expose overflow/safety atomics;
- serialize/deserialize component state;
- coordinate lifecycle without blocking the host callback.

### Controller/UI

- expose minimal status and safe parameters;
- load project selection/reload requests through asynchronous commands;
- serialize UI-only controller state;
- remain optional: processor works when UI is never opened.

### Prohibited

- socket/USB/file/media operations in `process()`;
- allocations or mutex waits in `process()`;
- direct channel-DMX logic;
- direct fixture/profile parsing;
- storing live device handles in VST3 state;
- restoring output armed;
- separate Windows/macOS behavior beyond framework/OS adapters.

## Suggested file structure after framework pin

```text
plugins/vst3/
  config.h
  AeylaVst3.h
  AeylaVst3.cpp
  AeylaVst3Controller.h
  AeylaVst3Controller.cpp
  AeylaVst3State.h
  AeylaVst3State.cpp
  resources/
  tests/
```

Names may change to match the pinned framework, but responsibilities may not.

## Implementation sequence

1. Pin iPlug2/VST3 SDK revisions in an isolated PR.
2. Build unchanged example targets in Windows/macOS CI.
3. Create silent plugin with stable processor/controller UIDs.
4. Pass events into `HostEventIngress`.
5. Add silent-output and state tests.
6. Run Steinberg Validator.
7. Install and scan in real Ableton Windows/macOS.
8. Add deterministic test executor/DMX snapshot.
9. Connect shared runtime/output after host spine is proven.

## Mandatory reading

- `AGENTS.md`
- `docs/VST3_PLATFORM_ARCHITECTURE.md`
- `docs/VST3_STATE_LIFECYCLE_SPEC.md`
- `docs/PLUGIN_DEPENDENCY_DECISION.md`
- `docs/ABLETON_HOST_TEST_MATRIX.md`
- `docs/CROSS_PLATFORM_BUILD_RELEASE.md`
- `docs/BUG_PREVENTION_AND_QA.md`
- `docs/VST3_RESEARCH_NOTES_2026-08-05.md`
