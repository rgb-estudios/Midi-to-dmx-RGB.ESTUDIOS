# Roadmap

Statuses use: Specified, Scaffolded, Implemented, Simulated, Host-tested, Hardware-tested, Show-tested.

## Roadmap principle

Windows, macOS and Ableton/VST3 integration are developed in parallel from the beginning. The project must not finish a Windows-only editor and attempt a macOS port later, nor complete a standalone product before discovering whether the shared runtime can operate correctly inside Ableton.

Every integrated milestone must preserve four execution paths:

1. Windows standalone.
2. macOS standalone.
3. Windows Ableton + VST3.
4. macOS Ableton + VST3.

## Stage 0 — foundation

- Product scope: **Specified**, pending merge of cross-platform acceptance update.
- Semantic core: **Implemented** (initial subset).
- DMX compiler: **Implemented**.
- ArtDMX encoder: **Implemented/Simulated**.
- Visual interaction prototype: **Implemented as disposable browser prototype**.
- Production standalone/VST3: **Specified**.

Exit:

- repository published;
- CI configured for Linux, Windows and macOS;
- architecture and four-path platform baseline approved;
- no documentation still describes macOS or Ableton as a later port.

## Stage 1 — cross-platform integration spine

Build the minimum end-to-end product skeleton before deep feature work.

- Pin one cross-platform framework/toolchain for standalone and VST3 targets.
- Build launchable standalone shells on Windows and macOS.
- Build loadable VST3 shells for Windows and macOS.
- Establish shared runtime API and host-safe command/frame exchange.
- Load one minimal `.aeylashow` example in all four paths.
- Receive one MIDI note in both Ableton hosts.
- Produce one deterministic semantic and DMX frame through the shared engine.
- Output remains disarmed; no hardware output required for this stage.
- Configure CI artifacts for Windows and macOS builds.

Exit:

- both standalone shells launch;
- both VST3 builds pass validator and scan/load in Ableton on real Windows and Mac test machines;
- the same test note produces matching output in all four paths;
- architecture proves that no OS or host path needs a duplicated engine.

## Stage 2 — core runtime and project model

- Project model and schema loader.
- Portable `.aeylashow` save/load and migration.
- Semantic profiles with segments, reset protection and validation.
- Executor/layer state machine.
- Procedural visual frame providers.
- Deterministic seek/time model.
- Art-Net UDP backend.
- Cross-platform adapter enumeration and configuration.

Exit:

- standalone and VST3 load the same project on Windows and macOS;
- simulated MIDI produces byte-identical DMX captures across all four paths;
- Art-Net remains at the accurately evidenced validation status.

## Stage 3 — standalone editor alpha on Windows and macOS

- Production UI shell on both operating systems.
- Canvas and 14 sample points.
- Source/palette/look/scene/executor editors.
- Fixture profile and patch editors.
- Project package save/load/export.
- Art-Net test output.
- Platform-native file dialogs and safe project paths.
- Visual parity review on Windows and macOS.

Exit:

- designer can author, reopen and export a complete project without Ableton on either supported OS;
- exported packages are interchangeable between Windows and macOS;
- the visual hierarchy remains approved on both platforms.

## Stage 4 — VST3 runtime alpha on Windows and macOS

- Complete MIDI event bridge.
- Optional host transport bridge.
- Project load/reload and checksum display.
- Output arm/blackout/status.
- Host state persistence.
- Save/reopen Ableton Set.
- Copy track/device to another Set.
- Missing-project and changed-project policy.
- Host stop/close/crash-safe behavior.

Exit:

- Ableton on Windows and macOS triggers the same scenes as both standalone builds with matching DMX captures;
- both host paths are explicitly Host-tested with recorded Ableton and OS versions.

## Stage 5 — media and USB parity

- Cross-platform image loading.
- Cross-platform H.264/MP4 decode strategy with platform adapters where required.
- Decode buffering and seek tests on Windows and macOS.
- DMX USB Pro backend on both operating systems.
- Open DMX backend only after priority backend is stable and explicitly tested.

Exit:

- named hardware and drivers pass the validation matrix on Windows and macOS where vendor support exists;
- any unsupported OS/device combination is shown clearly in the UI and documentation.

## Stage 6 — rehearsal candidate

- Windows installer and macOS app packaging.
- Signing/notarization plan and documented unsigned-development procedure.
- Crash recovery and backup.
- Two-hour soak tests in all required execution paths.
- Session transfer between Windows and macOS.
- Rig 10/14 comparison.
- Full operational manual.

Exit: rehearsal-approved release candidate on both operating systems and both Ableton hosts.

## Stage 7 — show validation

- Full show session.
- Projection and lighting simultaneous load.
- Hardware failover drills.
- Correction/rollback drill.
- Windows and macOS fallback package check.
- Show log and postmortem.

Exit: release marked Show-tested with the exact platform used in the show and a verified fallback path.
