# Architecture decision records

## ADR-001 — Semantic fixture model

**Decision:** authored looks target semantic attributes; profiles map attributes to DMX slots/ranges.

**Reason:** fixture model and channel order may change at short notice.

**Consequence:** profile compiler and validation are critical; no direct channel programming in looks.

## ADR-002 — Cross-platform standalone authoring and VST3 runtime

**Decision:** full editing occurs in a free standalone application on Windows and macOS; Ableton Live on Windows and macOS uses a reduced VST3 runtime.

**Reason:** the designer must not purchase Ableton solely to edit lighting, while the final session must trigger the same shared show runtime on either production operating system.

**Consequence:** all four paths—Windows standalone, macOS standalone, Windows VST3 and macOS VST3—share engine and project format from the first integrated milestone. Corrections are delivered as project packages.

## ADR-003 — No audio analysis

**Decision:** runtime processes MIDI and optional host transport only.

**Reason:** authored synchronization and predictable operation are preferred.

## ADR-004 — Projector video remains in Ableton

**Decision:** plugin does not output projector video. Ableton's video window handles projection.

**Reason:** avoid duplicate video-server scope and preserve current show plan.

## ADR-005 — iPlug2/VST3 SDK proof before dependency pin

**Decision:** use iPlug2 plus the official Steinberg VST3 SDK as the preferred proof path for Windows/macOS standalone and VST3. Pin exact revisions only after a minimal silent-instrument proof passes CI, Validator and real Ableton on Windows and native Apple Silicon macOS.

**Reason:** one permissive C++ framework can provide the visual standalone and host adapters without duplicating the project runtime, while early host testing prevents a late Ableton incompatibility.

**Consequence:** framework types remain outside shared core/runtime/io models; dependency versions do not float; JUCE or direct VST3 SDK remain documented fallbacks if proof gates fail.

## ADR-006 — Art-Net first, named USB protocols second

**Decision:** Art-Net is initial production backend; USB support is implemented by protocol family.

**Reason:** no universal generic USB-DMX protocol exists.

## ADR-007 — Silent instrument topology in Ableton

**Decision:** the first VST3 proof is an instrument/generator with one event input bus, no audio input and one stereo output bus that always emits correctly flagged silence.

**Reason:** Ableton must host the device conventionally on a MIDI track even though AEYLA generates no audible audio.

**Consequence:** the host callback clears output buffers, consumes bounded VST3 events/process context and enqueues compact host events only. Whether generator categorization or infinite tail is required remains a measured Ableton host decision.

## ADR-008 — Output arm never restores armed

**Decision:** VST3 state, project reload, Set reopen and crash recovery always return output to disarmed.

**Reason:** an automatically restored live DMX/haze/strobe state is unsafe.

**Consequence:** arm state is not authoritatively persisted as true; the operator explicitly arms after project validation. Blackout and safe defaults have higher priority than artistic state.

## ADR-009 — Lock-free bounded host-event handoff

**Decision:** the VST3 callback and standalone MIDI adapter write compact trivially copyable events to a fixed-capacity SPSC queue consumed by the shared runtime.

**Reason:** the callback cannot perform network, file, media, allocation or blocking operations.

**Consequence:** queue overflow is observable and triggers transient release/haze-off safety handling; it is never silent.
