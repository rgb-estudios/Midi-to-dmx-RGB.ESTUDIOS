# Changelog

## Unreleased

### Fixed — CP-AEYLA-0.3.1

- Corrected the obsolete application-model regression that expected diagnostic
  executor behaviour after an authored Show had taken ownership of MIDI.
- Normalized every macOS build/product metadata path to the supported macOS 11
  floor required by `std::filesystem`.
- Ensured universal macOS dependencies build both arm64 and x86_64 slices,
  preventing the Intel plugin slice from linking an arm64-only miniz archive.
- Removed the duplicate visual-only executor and lower ARM implementations from
  the native UI.
- Removed model mutation and wall-clock animation from UI drawing.
- Derived animation phase deterministically from absolute host PPQ, with tests
  for pause, seek/pre-roll wrapping and invalid host state.
- Completed standalone safety-reason reporting for `show_not_ready` so strict
  Clang/macOS builds cannot silently omit a safety state.
- Corrected the macOS universal-binary `lipo` gate so the input path is not
  misparsed as an architecture name.
- Made the Windows native-window gate preserve a full WER crash dump, matching
  PDB, executable and textual evidence even when startup fails early.
- Fixed the host-transport seqlock read barrier so weakly ordered CPUs cannot
  combine sample/PPQ/tempo fields from different host publications.
- Scoped GCC's known ThreadSanitizer fence warning out of `-Werror`; all other
  warnings remain errors and the mailbox concurrency test still runs.
- Made the headless macOS REAPER smoke test explicitly accept the official DMG
  software-license agreement instead of cancelling during mount.

### Added

- Concrete VST3 platform architecture for a silent MIDI instrument/generator with a stereo silent output bus, native VST3 note events, process-context handling and strict realtime callback boundaries.
- Versioned VST3 component/controller state, transactional project reload and safe lifecycle specification.
- Windows/macOS build, installer, signing and notarization plan.
- Real Ableton host-test matrix for Windows and native Apple Silicon macOS.
- Framework decision and proof gates for iPlug2 plus the official VST3 SDK.
- Bug-prevention, fuzzing, sanitizer, concurrency and release quality gates.
- Trivially copyable host-event records and a fixed-capacity lock-free SPSC queue for the future Ableton callback-to-runtime handoff.
- Realtime-safe host-event ingress that counts overflow and requests transient-release/haze-off safety handling.
- Versioned bounded VST3 component-state serializer with project UUID/checksum, schema version, safe global state and validated locator policy. Output Arm is deliberately not persisted.
- Shared runtime safety state for startup, project/backend validation, arm/disarm, blackout, reload, overflow, host deactivation and shutdown.
- Automated tests covering queue FIFO/capacity/wraparound, 200,000 concurrent events, overflow response, state round-trip/corruption/truncation/path validation and runtime safety transitions.
- ASan/UBSan and ThreadSanitizer GitHub Actions quality gates.

### Changed

- Windows standalone, macOS standalone, Windows Ableton/VST3 and macOS Ableton/VST3 are mandatory product paths from the first integrated milestone.
- The roadmap creates a cross-platform standalone/VST3 integration spine before deep editor, media or hardware work.
- Agent completion language distinguishes CI implementation, Ableton host testing, hardware testing and show testing.
- The test plan requires platform-qualified evidence and byte-identical shared-engine behavior across all four execution paths.

## 0.0.1-foundation — 2026-08-05

### Added

- Canonical public repository configuration and parallel-AI handoff.
- Product and architecture specification.
- Semantic fixture model.
- DMX compiler prototype.
- ArtDMX packet encoder.
- Rig-10 and Rig-14 examples.
- Browser-based visual editor prototype.
- Project/profile schemas.
- Automated core tests.
- GitHub CI and project governance files.
