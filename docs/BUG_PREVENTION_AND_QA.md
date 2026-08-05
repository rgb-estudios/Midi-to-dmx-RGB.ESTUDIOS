# Bug prevention, verification and quality gates

## 1. No-zero-bug guarantee

No responsible software process can guarantee that a cross-platform plugin has zero bugs. The project goal is to prevent high-risk defect classes, detect regressions automatically and avoid claiming readiness without evidence.

## 2. High-risk defect classes

P0 categories:

- host crash/hang/deadlock;
- non-realtime-safe work in callback;
- unsafe startup or restored armed state;
- stuck strobe, haze or momentary executor;
- lost/corrupt Ableton state;
- platform parity mismatch;
- project reload emitting partial frames;
- different standalone/VST3 output for identical inputs;
- plugin missing on Apple Silicon;
- installer/signing/notarization failure;
- unbounded memory/CPU growth;
- network loss causing undefined output.

## 3. Automated test layers

### Unit

- semantic attributes and fixture profiles;
- RGBWALUV conversion;
- event/executor state machines;
- SPSC queue and overflow;
- state binary reader/writer;
- project migrations;
- ArtDMX/USB packets;
- safe-state policy.

### Golden/deterministic

- fixed MIDI + project + time sequence -> semantic frame hashes;
- semantic frames -> exact DMX bytes;
- standalone and VST3 adapter feeds -> identical results;
- released fixture profile compatibility corpus.

### Fuzz/property

- VST3 component/controller state streams;
- `.aeylashow` manifest and profile JSON;
- ZIP/package boundaries;
- invalid MIDI/event sequences;
- fixture ranges/address overlaps;
- hot reload and migration inputs.

### Concurrency

- ThreadSanitizer-capable core/runtime tests;
- repeated start/stop/destruction;
- queue saturation;
- backend loss while reload occurs;
- UI open/close during output;
- host callback producer plus runtime consumer soak.

### Static/sanitizer

- warnings as errors;
- clang-tidy ruleset;
- AddressSanitizer/UndefinedBehaviorSanitizer on non-release jobs;
- platform-native analyzers where available;
- dependency vulnerability/license review.

## 4. Realtime audit

Every callback-path change documents:

- functions reachable from process callback;
- allocation status;
- lock/blocking status;
- worst-case loop bounds;
- exception behavior;
- queue/overflow behavior;
- test evidence.

CI should eventually enforce forbidden symbols/calls in callback-related translation units where practical.

## 5. Review requirements

A PR affecting plugin/runtime must include:

- architecture layer;
- Windows impact;
- macOS impact;
- Ableton lifecycle impact;
- safety impact;
- tests added;
- state/schema compatibility;
- screenshots for UI;
- performance measurements for hot paths;
- known limitations.

No self-authored AI PR is merged solely because tests pass. Review must inspect lifecycle, concurrency and safety assumptions.

## 6. Release testing

Before rehearsal candidate:

- clean Windows/macOS install;
- native Ableton host tests;
- real Art-Net node and final fixtures;
- two-hour combined soak;
- network/device failure drills;
- project correction/reload/rollback;
- plugin update with saved Set;
- installer update/uninstall;
- signed/notarized package checks;
- final visual review at production display scaling.

## 7. Defect handling

Every confirmed defect receives:

- minimal reproduction;
- severity and affected versions;
- root cause;
- failing automated test added first where possible;
- fix and regression evidence;
- compatibility/safety assessment;
- release-note entry;
- rollback path.

Never hide a known show-safety defect behind a UI warning.
