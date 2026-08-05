# Roadmap

Statuses use: Specified, Scaffolded, Implemented, Simulated, Hardware-tested, Show-tested.

## Stage 0 — foundation (current)

- Product scope: **Specified**.
- Semantic core: **Implemented** (initial subset).
- DMX compiler: **Implemented**.
- ArtDMX encoder: **Implemented/Simulated**.
- Visual interaction prototype: **Implemented as disposable browser prototype**.
- Production VST3/editor: **Specified**.

Exit: repository published, CI green, architecture approved.

## Stage 1 — core runtime

- Project model and schema loader.
- Semantic profiles with segments, reset protection and validation.
- Executor/layer state machine.
- Procedural visual frame providers.
- Deterministic seek/time model.
- Art-Net UDP backend.

Exit: command-line runtime responds to simulated MIDI and outputs verifiable ArtDMX packets.

## Stage 2 — standalone editor alpha

- Production UI shell.
- Canvas and 14 sample points.
- Source/palette/look/scene/executor editors.
- Fixture profile and patch editors.
- Project package save/load/export.
- Art-Net test output.

Exit: designer can author and export a complete project without Ableton.

## Stage 3 — VST3 runtime alpha

- iPlug2 integration.
- MIDI event bridge.
- Optional host transport bridge.
- Project load/reload.
- Output arm/blackout/status.
- Host state persistence.

Exit: Ableton triggers the same scenes as standalone with matching DMX captures.

## Stage 4 — media and USB

- Image loading.
- Windows Media Foundation H.264/MP4 decode.
- Decode buffering and seek tests.
- DMX USB Pro backend.
- Open DMX backend after priority backend is stable.

Exit: named hardware passes validation matrix.

## Stage 5 — rehearsal candidate

- Installer and signed binaries where possible.
- Crash recovery and backup.
- Two-hour soak tests.
- Session transfer test.
- Rig 10/14 comparison.
- Full operational manual.

Exit: rehearsal-approved release candidate.

## Stage 6 — show validation

- Full show session.
- Projection and lighting simultaneous load.
- Hardware failover drills.
- Correction/rollback drill.
- Show log and postmortem.

Exit: release marked Show-tested.
