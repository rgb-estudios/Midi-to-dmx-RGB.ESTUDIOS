# Changelog

## 2026-08-28 — R07 continuidad Art-Net con REAPER inactivo

- La reproducción manual de una toma usa un reloj monotónico independiente del callback de audio.
- Cambiar el foco desde REAPER a Capture ya no corta Art-Net cuando REAPER muestra `audio device closed`.
- La reproducción DAW/MIDI conserva el reloj por muestras y el fail-closed por pérdida real de heartbeat.
- APAGÓN, renderizado sin conexión, descarga y cierre conservan su prioridad de seguridad.

## Unreleased

### Fixed — R07 global blackout / Take authority split

- Separated the global operator/safety blackout latch from the Show renderer's
  effective artistic black state. A new Song, missing Cue or out-of-range host
  position can keep the semantic Show frame black without reactivating the red
  `APAGÓN` control.
- Made `ARMAR SALIDA DE TOMA` consult only the global latch, allowing an
  explicitly selected recorded Take to become the independent Art-Net
  authority while the Show renderer has no resolved Cue.
- Stopped persisting a transient Cue blackout as the host's global blackout
  preference and added regressions for both an empty Song and an out-of-range
  Song position.

### Safety boundary — R07 global blackout / Take authority split

- Activating global `APAGÓN` still disarms both model and Take authority and
  schedules the deterministic zero-DMX burst. Project/backend/runtime/offline
  failures continue to force the same global latch; only artistic Cue black is
  excluded from the Take arm gate.

### Fixed — R07 headless/minimized Art-Net stability

- Added a regression that runs `ARMAR → REPRODUCIR` for one second without any
  UI/idle calls and verifies relative sample-clock advance, received DMX frame,
  zero send errors and a sustained 44 Hz packet envelope.
- Kept file reads, cursor publication and UDP pacing on independent workers;
  minimizing or closing the plugin window no longer participates in runtime
  scheduling.
- Latched three consecutive UDP failures until an explicit operator re-arm,
  and made that latch disarm both model and Take authority. Isolated send errors
  remain visible but do not immediately interrupt output.

### Added — R07 Windows IPv4/subnet configuration

- Replaced the cosmetic network field with a real Ethernet adapter workflow.
  A least-privilege elevated helper adds a secondary IPv4 alias, preserves the
  existing network, validates adapter identity/address/bind and rolls back the
  exact alias on failed verification.
- Added strict IPv4/mask parsing, directed-broadcast derivation and a bounded
  nonce-bound request/result protocol. REAPER and the VST3 remain unelevated.
- Embedded and hashed `AeylaNetworkHelper.exe` in the Windows VST3 PRETEST.

### Changed — R07 operator surface hierarchy

- Split the crowded center/right surface into `TOMA / EDICIÓN` and
  `RED / SALIDA` workspaces while keeping ARMAR and APAGÓN persistent.
- Raised operational typography to 12 px and made setlist, workspace, network
  diagnostics and footer regions responsive at compact window heights.

### Validation boundary — R07 P0 repairs

- IPv4/protocol, output-worker and headless ARM/PLAY loopback tests pass in a
  strict local C++ build. Windows helper/VST3 compilation, REAPER interaction,
  real UAC/IP rollback, physical Art-Net node/DMX and soak remain open.

### Fixed — R07 real-time Take Art-Net authority

- Fixed the operator sequence `ARMAR SALIDA DE TOMA` followed by `REPRODUCIR`:
  PLAY now reuses the already validated clip instead of reloading it and
  silently removing its physical-output authority.
- Made the host audio callback the single relative Take clock. Each processed
  block advances by its exact sample count; absolute REAPER Arrangement
  position is now heartbeat/safety information only and can no longer freeze,
  duplicate or relocate Take playback after Stop, Seek or Loop.
- Reject reloading a Take while output is armed and distinguish `ARMADA ·
  ESPERA REPRODUCIR`, `PREVIA SIN SALIDA FÍSICA` and `AL AIRE` in operator
  feedback instead of reporting armed-but-idle output as live.
- Added a loopback Art-Net regression covering ARM → PLAY, a stopped absolute
  host position, callback-driven progression and retained physical authority.

### Safety boundary — R07 real-time Take Art-Net authority

- The audio callback performs atomic clock publication only; file reads and UDP
  transmission remain on their dedicated workers. Offline render and lost-host
  heartbeat continue to fail closed and require explicit re-arm.
- Simultaneous capture and RX→TX monitoring remains intentionally unsupported
  by product decision. R07 only permits capture → RAW → edit/consolidate → TX.
- The affected native test passes in strict local C++ compilation and simulated
  loopback Art-Net. Windows/macOS product CI, REAPER host evidence and physical
  node/DMX validation remain open before hardware- or show-tested status.

### Fixed — R07 free-point Take marking

- Replaced the prominent second-based trim controls with a playhead-first
  workflow: freely click/drag the timeline, then mark IN or OUT at the current
  DMX frame.
- Added large labelled IN/OUT grips, editable `MM:SS.mmm` fields, direct
  navigation to each boundary and one-frame nudging (about 22.7 ms at 44 Hz).
- Expanded handle hit targets so the visible grip, not only its three-pixel
  boundary line, owns the drag interaction.

### Added — R07 advanced DMX Take editor

- Replaced the duration-only trim bar with a bounded, file-backed activity
  envelope built from real 512-channel level and motion peaks.
- Added draggable IN/OUT handles, a relative playhead, safe stopped/disarmed
  scrub preview, pointer-anchored horizontal zoom and Shift-drag panning.
- Added chronological Take-version navigation and an explicit return-to-RAW
  action; changing versions never mutates or replaces a source recording.
- Made playback and physical-output arming honor the selected version and its
  non-destructive IN/OUT range instead of silently reselecting the newest file.

### Safety boundary — R07 advanced DMX Take editor

- Scrub reads and holds a frame locally but cannot publish Art-Net; it fails
  closed while physical Take output is armed or transport is running.
- The activity envelope is capped at 256 buckets and the file reader remains
  bounded-cache; no complete Take payload is retained in RAM.
- Native core/runtime tests pass locally. Windows product compilation, REAPER
  interaction, physical Art-Net/DMX hardware and the two-hour field soak remain
  required before any Show Ready claim.

### Added — R07 clip consolidation UI

- Connected the visible `CONSOLIDAR CLIP` action to the file-backed DMX Take
  consolidator, producing a new bounded 44 Hz `.aeylatake` from the active
  non-destructive IN/OUT range while preserving the source RAW file.
- Made the newly consolidated clip the active playback source and surfaced an
  explicit Spanish success/failure result to the operator.

### Changed — R07 operator surface

- Updated the native control surface and project actions to Spanish and
  corrected the visible PRETEST identity from R03 to R07.
- Aligned graphical-product Art-Net setup explicitly with the contractual
  44 Hz output cadence; the worker continues to normalize legacy requests.

### Validation boundary — R07 clip consolidation UI

- Core tests and product CI must pass on the resulting commit. REAPER host
  interaction, physical Art-Net/DMX hardware and the field soak remain open
  until evidence is captured on the target systems.

### Added — native installer delivery

- Added a reproducible Windows x64 Inno Setup installer for the integrated
  VST3, including clean install/uninstall smoke validation and exact bundle
  hash comparison on the native Windows runner.
- Added a macOS 11+ universal installer package containing the VST3 and AUv2,
  with architecture, code-signature, install, `auval` and exact uninstall
  checks on the native macOS runner.
- Added a third cross-platform manual pack with Windows/macOS VST3 bundles,
  the Logic AUv2, selective maintenance tools, build identity and SHA-256
  checksums generated from the same commit.
- Made packaging jobs checkout and record the PR branch HEAD explicitly instead
  of GitHub's temporary pull-request merge commit, so every manifest and
  delivered artifact resolves to the canonical source revision.
- Deliberately excluded the Windows standalone from installers while the
  OpenGL startup P0 tracked in issue #17 remains unresolved.

### Added — CP-AEYLA-0.3.3

- Connected the graphical product to the dedicated latest-frame-only Art-Net
  UDP worker at a fixed 40 FPS, outside the host audio callback.
- Added visible `OUTPUT SETUP` configuration using numeric
  `IPv4@universe`, plus `OFF` to disable physical output.
- Added deterministic output preflight, one-owner target/universe leasing,
  packet/error counters and send-error fail-closed disarm + blackout.
- Added application-model tests for persisted Art-Net configuration and
  transport tests for strict numeric unicast target validation.
- Changed the REAPER gate to use the host's supported command-line ReaScript
  dispatch without the incompatible isolated-instance flag.

### Safety boundary — CP-AEYLA-0.3.3

- Configuration, project reload, backend disable, offline render, send error
  and shutdown all disable transmission and preserve explicit manual re-arm.
- UDP socket readiness does not prove that a physical Art-Net node received a
  packet. Named node/PAR validation, disconnect detection and show soak remain
  mandatory before Show Candidate.

### Added — CP-AEYLA-0.3.2

- Added a 4 ms independent control/runtime worker so Cue, transport, MIDI and
  safety updates no longer depend on the editor receiving `OnIdle`.
- Added persistent per-Song DAW start-PPQ bindings to plugin state, with a
  visible `SET SONG START` workflow and safe handling for unbound Songs.
- Added source-level Look → Cue → Song authoring: complete Look storage,
  primary/secondary palette, Look intensity, fixture mask, up to 15 Songs,
  stored-Look/Song navigation and Cue placement at the DAW playhead.
- Added authoring projection beyond a Song's initial end; storing a Cue extends
  the Song within bounded tick limits.
- Added explicit offline-render disarm/blackout and latched runtime-fault ARM
  inhibition.

### Changed — CP-AEYLA-0.3.2

- Bumped project schema to 2.0 so each Look owns colors, intensity, speed,
  white/amber extraction, UV and fourteen fixture-participation flags; schema
  1.x migrates deterministically.
- Bumped `show.bin` to 1.1 and made Scene/Cue own MIDI Learn mapping. Legacy
  1.0 clips migrate only when their mapping is unambiguous.
- Split editable Look Intensity from operator Grand Master.
- Made every Programmer/Rig edit force disarm + blackout; Grand Master and
  explicit Blackout remain operator controls.
- Made timeline playback reconstruct continuously from absolute DAW PPQ while
  preserving live overrides only during continuous forward playback.
- Documented the real two-entry `.aeylashow` package instead of the obsolete
  foundation-era conceptual archive layout.

### Validation boundary — CP-AEYLA-0.3.2

- Eight affected strict GCC test executables pass locally; full CTest, iPlug2
  product build, platform CI and real-host evidence remain pending on the new
  SHA.
- Windows OpenGL issue #17, REAPER evidence, hardware validation and show soak
  remain open.

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
- Added Microsoft ProcDump as the deterministic crash-capture fallback because
  hosted Windows runners may suppress WER LocalDumps.
- Root-caused the Windows startup crash to NanoVG calling a null
  `glCreateProgram` pointer after incomplete OpenGL loading; tracked as P0 #17.
- Fixed the host-transport seqlock read barrier so weakly ordered CPUs cannot
  combine sample/PPQ/tempo fields from different host publications.
- Scoped GCC's known ThreadSanitizer fence warning out of `-Werror`; all other
  warnings remain errors and the mailbox concurrency test still runs.
- Made the headless macOS REAPER smoke test feed the official DMG license
  response through stdin (macOS 15 exposes no `-acceptlicense` attach option)
  and retain its mount transcript.

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
