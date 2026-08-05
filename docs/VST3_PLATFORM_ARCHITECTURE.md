# VST3 platform architecture

Status: **Specified**, with the host-event queue **Implemented** by automated tests.

## 1. Required execution paths

AEYLA Visual DMX is not complete unless one shared codebase produces and validates all four paths:

1. Windows x64 standalone editor.
2. macOS universal standalone editor.
3. Windows x64 VST3 loaded by Ableton Live.
4. macOS universal VST3 loaded natively by Ableton Live.

The standalone and VST3 are adapters around the same project, runtime, semantic, media and output libraries. Platform-specific code is restricted to framework glue, OS integration, device enumeration, installers and signing.

## 2. Plugin identity inside Ableton

The first host proof will implement the runtime as a **silent VST3 instrument/generator**:

- one VST3 event input bus;
- no audio input bus;
- one stereo audio output bus;
- output buffers always cleared to zero;
- output silence flags always set correctly;
- note-on, note-off and velocity received as VST3 events;
- no audio analysis and no audible output.

The stereo silent output is intentional. It gives Ableton a conventional instrument topology on a MIDI track while preserving the product requirement that the plugin produces no sound. This topology remains a hypothesis until real Ableton host tests pass on Windows and macOS; do not rely only on the Steinberg validator.

The processor should use an instrument/generator category and evaluate whether `kInfiniteTail` is required to prevent host suspension. This decision must be based on measured Ableton behavior, not assumption. Network output must not depend on the host calling `process()` continuously: an independent runtime/output thread owns the last safe state.

## 3. Framework decision

The preferred implementation path is **iPlug2 plus the official Steinberg VST3 SDK**, subject to a pinned proof-of-concept commit.

Reasons:

- one C++ framework can produce VST3 and standalone targets on Windows and macOS;
- iPlug2 supports VST3 and native standalone applications;
- its permissive license is compatible with RGB Estudios retaining proprietary product code;
- it avoids separate UI/runtime implementations per operating system;
- it permits direct access to VST3 lifecycle, MIDI event and state behavior when required.

Before production code is accepted:

1. Pin an exact iPlug2 commit and exact VST3 SDK version.
2. Record licenses and source revisions in `THIRD_PARTY_NOTICES.md`.
3. Build the untouched framework examples on Windows and macOS CI.
4. Build a minimal AEYLA silent-instrument proof on both systems.
5. Validate it with Steinberg Validator and real Ableton.
6. Never track a floating `main`, `master` or `develop` dependency in release builds.

See `docs/PLUGIN_DEPENDENCY_DECISION.md`.

## 4. Component structure

```text
Ableton / standalone MIDI input
              |
              v
       Host adapter layer
  (VST3 processor or standalone MIDI)
              |
              v
  bounded lock-free HostEvent queue
              |
              v
        shared runtime thread
              |
    +---------+----------+
    |                    |
visual/media engine   scene/executor state
    |                    |
    +---------+----------+
              |
       semantic fixtures
              |
       fixture compiler
              |
          DMX frame
              |
       output backend thread
      Art-Net / supported USB
```

### Shared libraries

- `src/core`: attributes, color transforms, fixture compilation and deterministic data types.
- `src/runtime`: executors, layers, clock, host events, project state and safety state.
- `src/media`: image/video decode and visual-frame providers.
- `src/io`: Art-Net, USB-DMX, adapters and output threads.
- `apps/editor`: standalone UI and OS adapter.
- `plugins/vst3`: thin VST3 processor/controller adapter.

No plugin-only artistic engine is allowed.

## 5. Realtime callback contract

The VST3 processing callback may only perform bounded operations:

1. Clear the stereo output buffers and set silence flags.
2. Read note events from the VST3 event list.
3. Read host-owned parameter changes.
4. Read requested transport fields from `ProcessContext` when valid.
5. Convert them into compact `HostEvent` records.
6. Attempt to push records into the preallocated SPSC queue.
7. Update atomic diagnostics and safety flags.
8. Return.

The callback must never:

- allocate or free memory;
- use a mutex, condition variable or blocking primitive;
- open, read or write files;
- parse JSON or ZIP data;
- decode images or video;
- call UDP, serial, USB or device APIs;
- create or join threads;
- write logs through a potentially blocking logger;
- call UI methods;
- wait for another thread;
- throw exceptions across the host boundary.

The processor and edit controller must remain valid if the host instantiates the processor without opening the UI.

## 6. Host event representation

`src/runtime/host_event.h` defines trivially copyable records containing:

- event type;
- MIDI channel;
- note number;
- normalized velocity/value;
- sample offset within the current process block;
- project sample position when supplied by the host.

`src/runtime/spsc_queue.h` is a fixed-size single-producer/single-consumer ring buffer. It has no allocation or locks. One producer and one consumer are permitted.

### Overflow policy

Queue overflow is a safety event, not a silent data loss condition.

Required response:

1. Increment an atomic overflow counter.
2. Set an atomic `transient_release_requested` flag.
3. On the runtime thread, release momentary/flash/strobe executors and force haze output to zero.
4. Keep the base scene only if its state is known and deterministic; otherwise enter blackout.
5. Present a persistent UI warning until acknowledged.
6. Record the overflow in the diagnostic report outside the callback.

Queue capacity and runtime frequency must be selected from stress-test evidence.

## 7. MIDI and executor mapping

VST3 note events are the canonical trigger input:

- Note On: activate executor according to its trigger mode.
- Note Off: release momentary/flash executors.
- Velocity: normalized intensity or executor-defined value when enabled.
- Repeated Note On: defined per trigger mode; never implementation-dependent.
- All Notes Off / host reset: release transient executors and haze.

MIDI CC is not consumed as opaque MIDI bytes. Functions exposed to the host as controls must be regular VST3 parameters, optionally with `IMidiMapping` where required.

## 8. Transport synchronization

The plugin may request only the process context fields it uses through `IProcessContextRequirements`:

- playing state;
- project time in samples;
- tempo;
- time signature;
- cycle state and boundaries when loop behavior is implemented.

Every field must be checked for validity before use. Missing host context is a supported condition.

Transport behavior:

- Start: resume transport-synchronized sources from the host position.
- Stop: apply the configured stop policy; default releases transient effects and haze while preserving or fading the base scene.
- Seek: deterministic seek without replaying historical MIDI events.
- Loop boundary: explicitly tested; no duplicate one-shot or stuck momentary state.
- Offline render/export: external DMX output disabled by default.

## 9. Runtime and output threads

The runtime thread consumes host events and advances the deterministic show state. The output backend owns its own I/O thread and sends the latest immutable DMX frame.

Recommended initial frequencies, subject to measurement:

- host callback: host-defined;
- runtime visual/state tick: 120 Hz maximum;
- DMX/Art-Net output: 40 Hz;
- UI refresh: 30–60 Hz;
- media decoding: independent buffered worker(s).

No thread may hold an artistic-state lock while performing I/O. Prefer immutable snapshots, atomics, SPSC queues and short ownership transfers.

## 10. Processor/controller separation

Follow the VST3 processor/controller model:

- Processor owns host-visible component state and receives events/automation.
- Controller owns editor presentation and UI-only state.
- The processor must run correctly without a controller.
- Parameter synchronization uses host-approved VST3 mechanisms.
- UI and processor never share mutable containers directly.

Do not mark the processor as distributable until all processor dependencies can actually operate without the controller and without process-local UI assumptions.

## 11. Required VST3 parameters

The initial exported parameter surface is intentionally small:

- `Output Arm` — discrete, defaults and restores to off.
- `Blackout` — discrete, highest priority.
- `Grand Master` — continuous, normalized.
- `Project Reload` — protected momentary command or message, not an automatable preset change.
- `Runtime Safe Mode` — read-only/status where framework permits.

Scene selection remains MIDI-note/executor driven. Do not export hundreds of fixture values as host parameters.

## 12. Platform targets

### Windows

- 64-bit VST3 only.
- MSVC 2022 release build.
- Windows 10 22H2 and Windows 11 target matrix.
- Standard VST3 system install location.
- No dependency on a developer runtime not installed by the package.

### macOS

- Universal binary: `arm64 + x86_64` unless a later approved decision removes Intel.
- Native Apple Silicon operation is mandatory because Universal Ableton only scans native Apple Silicon VST3 plugins.
- Minimum deployment target aligned with the supported Ableton Live 12 baseline; current product baseline is macOS 11.7.10 or later.
- Developer ID signing, Hardened Runtime and notarization required for distributed builds.

## 13. Acceptance boundary

A plugin build is not “Ableton compatible” because it compiles or passes Validator.

Required levels:

- **Implemented/Windows:** Windows CI build and validator pass.
- **Implemented/macOS:** macOS universal CI build and validator pass.
- **Host-tested/Windows:** real Ableton test matrix passes on a real Windows PC.
- **Host-tested/macOS:** real Ableton test matrix passes natively on a real Apple Silicon Mac; Intel test is additional.
- **Hardware-tested:** named node/interface and fixtures receive expected output.
- **Show-tested:** full rehearsal/show soak and recovery drills pass.

See `docs/ABLETON_HOST_TEST_MATRIX.md`.

## 14. Primary references

- Ableton VST on Windows: https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows
- Ableton AU/VST on macOS: https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS
- Ableton Apple Silicon plugin behavior: https://help.ableton.com/hc/en-us/articles/4410323149074-Plug-ins-on-Mac-in-Live-11-1-and-later
- Ableton system requirements: https://help.ableton.com/hc/en-us/articles/115001663530-Live-Minimum-System-Requirements
- VST3 API architecture: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/API%2BDocumentation/Index.html
- VST3 MIDI events: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/About%2BMIDI/Index.html
- VST3 process context: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Change%2BHistory/3.7.0/IProcessContextRequirements.html
- VST3 persistence: https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Persistence.html
- VST3 silence flags: https://steinbergmedia.github.io/vst3_dev_portal/pages/Tutorials/How%2Bto%2Buse%2Bthe%2Bsilence%2Bflags.html
- VST3 validator: https://steinbergmedia.github.io/vst3_dev_portal/pages/What%2Bis%2Bthe%2BVST%2B3%2BSDK/Validator.html
- iPlug2: https://github.com/iPlug2/iPlug2
