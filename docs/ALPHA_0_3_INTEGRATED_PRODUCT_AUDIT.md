# AEYLA Visual DMX — Alpha 0.3 integrated product audit

Date: 2026-08-06
Branch: `agent/alpha-0.3-integrated-product`
Status: **audited baseline; implementation in progress; not show-ready**

## 1. Audit purpose

Turn the existing graphical proof into one coherent product that behaves identically as:

1. Windows standalone;
2. macOS standalone;
3. Windows VST3 in Ableton Live;
4. macOS VST3 in Ableton Live.

The current `0.2.x` artifacts are useful proofs of window creation, packaging and graphical direction. They are not yet an integrated lighting application.

## 2. Evidence vocabulary

Every claim in this project must use one of these labels:

- **Source-reviewed**: implementation inspected in the repository.
- **Unit-tested**: deterministic automated tests pass.
- **CI-built**: the platform target compiles in CI.
- **CI-launched**: CI creates and exercises the native process/window.
- **Target-launch-tested**: launched on a named physical user machine.
- **Host-tested**: scanned, loaded and exercised in the named Ableton version on physical hardware.
- **Hardware-tested**: sent output through a named interface/node/firmware to named fixtures.

A stronger label may not be inferred from a weaker one.

## 3. Current verified state

| Path or subsystem | Verified state | Boundary |
|---|---|---|
| Windows standalone `0.2.0-rsrcfix1` | **Target-launch-tested** | Native window created on Windows 10.0.26200.0; no product workflow or DMX claim. |
| Windows graphical source build | **CI-built** | APP and VST3 compile. Screenshot launch currently exits with `0xC0000005`; CI launch gate is not passed. |
| macOS standalone | **CI-built** | Universal package builds; no physical Mac launch evidence recorded. |
| Windows VST3 | **CI-built** | No real Ableton scan/load/save/reopen evidence. |
| macOS VST3 | **CI-built** | No real Apple Silicon Ableton evidence. |
| MIDI in graphical VST3 | **Source-reviewed** | Notes 36–43 update UI executor state only. |
| Visual canvas | **Source-reviewed** | Five hard-coded procedural previews and fourteen hard-coded sample positions. |
| Project loading/saving | **Not implemented in graphical product** | No `.aeylashow` round trip is linked. |
| Semantic fixture/profile engine | **Existing shared foundation, not integrated into graphical product** | UI selection does not compile a real semantic or DMX frame. |
| Art-Net | **Not connected to graphical product** | UI explicitly reports simulated/no DMX. |
| USB-DMX | **Not implemented** | No device claim. |
| Output safety | **Partially implemented** | Shared safety type exists, but graphical UI/runtime state is not yet one authoritative flow. |

## 4. P0 findings

### P0-01 — The graphical product is not linked to the product engine

`product/AeylaVisualDmx/CMakeLists.txt` links the iPlug2 APP/VST3 targets and `runtime_safety_state.cpp`, but not the shared project loader, semantic compiler, fixture profile compiler, executor/layer runtime, DMX frame compiler or output backend.

**Impact:** APP and VST3 look related, but they do not yet prove byte-identical lighting behavior.

**Required correction:** introduce a product-owned `AeylaApplicationModel` and `AeylaRuntime` used by both adapters. UI and host code may submit commands and read immutable snapshots only.

### P0-02 — UI state is split from runtime state

The graphical proof keeps selection, manual executor and animation phase inside `AeylaMainControl`; plugin parameters hold other values; atomics hold MIDI indicators and armed state; `RuntimeSafetyState` holds a separate safety model.

**Impact:** the screen can display state that is not the state used to compile or transmit DMX.

**Required correction:** one authoritative runtime snapshot containing project validity, backend state, arm, blackout, executor/layer state, fixture semantic values and the latest DMX frame generation.

### P0-03 — Output Arm is misleading with the null backend

The graphical constructor marks project and backend ready even though the product states that no DMX backend is connected.

**Impact:** `OUTPUT ARMED` may appear without any real output path, weakening operator trust.

**Required correction:** null/diagnostic backend remains visibly `SIMULATED`; it cannot satisfy the real-output gate. Arming requires a valid project plus a configured and healthy named backend.

### P0-04 — Blackout is not wired through the complete safety path

The UI changes the Blackout parameter, but the graphical adapter does not demonstrate a call into the shared safety state, semantic override, DMX safe-frame publication and backend transmission.

**Impact:** the highest-priority safety action is currently visual/parametric rather than end-to-end verified.

**Required correction:** blackout must become a runtime command with highest priority, deterministic DMX output and tests in all four paths.

### P0-05 — VST3 MIDI is a display proof, not a lighting runtime

The VST3 receives Note On/Off, counts events and selects executors 1–8. It does not feed the bounded shared ingress and consume the same executor/semantic/DMX engine as standalone.

**Impact:** Ableton can animate the UI without proving the show output.

**Required correction:** VST3 callback copies compact events into the bounded ingress only. A non-realtime runtime thread applies those events to the same engine used by standalone.

### P0-06 — No portable project lifecycle exists in the graphical product

There is no New/Open/Save/Save As, dirty state, atomic save, backup recovery, schema validation, migration or canonical example loading in the current UI.

**Impact:** the product cannot author, transfer or reproduce a show.

**Required correction:** implement `.aeylashow` revision 1 round trip before adding media/video scope.

### P0-07 — Windows source CI launch still crashes

The source build compiles APP and VST3, but the Windows screenshot launch gate exits with access violation `0xC0000005`. The separately repaired target binary creates a real window on the user's PC.

**Impact:** the binary hotfix proves the missing resource diagnosis but does not replace a clean reproducible source build.

**Required correction:** diagnose the screenshot/graphics shutdown path, compile resources from source and publish only CI-generated artifacts.

### P0-08 — macOS and Ableton remain unvalidated on physical hosts

macOS targets build, but no physical Mac launch, plugin scan, MIDI execution or Set save/reopen evidence exists. Windows Ableton evidence is also absent.

**Impact:** cross-platform support cannot yet be claimed.

**Required correction:** complete the acceptance matrix in `ALPHA_0_3_ACCEPTANCE_MATRIX.md` with exact OS, architecture and Ableton versions.

## 5. P1 interface findings

### P1-01 — One monolithic control owns the whole interface

`AeylaMainControl` draws and hit-tests the top bar, source rail, canvas, inspector and executors manually.

**Consequences:**

- difficult keyboard/focus support;
- no reusable controls;
- high regression risk when layout changes;
- no independent tests for source, fixture, inspector or executor interactions;
- hard-coded dimensions and content;
- no scroll/virtualization strategy for profiles, patch or larger source libraries.

**Correction:** split into product surfaces backed by the application model:

- `SystemBarControl`;
- `LibraryRailControl`;
- `CanvasControl`;
- `InspectorControl`;
- `ExecutorDockControl`;
- reusable button, slider, table-row, status and dialog controls.

### P1-02 — The interface is a visual demo rather than an editor

The current five source buttons, fourteen fixture points and eight executor pads are hard-coded. Fixture selection changes only an index. There is no creation, naming, duplication, reorder, delete, targeting, channel-order editing or patching.

**Correction:** make the interface document-driven. Every visible authored object must have a stable project ID and command-based edit path.

### P1-03 — Missing professional editing behaviors

Required but absent:

- undo/redo;
- dirty-state and save confirmation;
- keyboard navigation and shortcuts;
- native open/save dialogs;
- copy/duplicate/delete;
- inline validation and recoverable errors;
- tooltips for truncated names;
- visible backend diagnostics;
- explicit disabled/unavailable states;
- scalable profile/patch tables.

### P1-04 — Visual hierarchy needs refinement, not replacement

The existing dark, canvas-dominant direction is valid, but operational density and hierarchy need work:

- reduce permanent explanatory copy;
- preserve more vertical area for canvas and executor dock at 1366×768;
- distinguish authored colour from system status;
- expose project/version/backend/validation in the top bar;
- show fixture semantic values contextually;
- move advanced output and profile editing into focused inspector modes;
- never let source animation compete with blackout or output status.

## 6. Alpha 0.3 architecture

```text
Standalone adapter ─┐
                    ├─> Command ingress ─> AeylaApplicationModel
VST3 adapter ───────┘                         │
                                              ├─ Project document / migration
                                              ├─ Executor + layer runtime
                                              ├─ Visual source + sampler
                                              ├─ Semantic fixture frames
                                              ├─ Profile + patch compiler
                                              ├─ 512-byte DMX frame
                                              ├─ Runtime safety state
                                              └─ Output backend service

UI <──────────── immutable RuntimeSnapshot / EditorSnapshot
Output thread <─ latest immutable DMX frame + safety commands
```

Rules:

1. UI never owns canonical show state.
2. VST3 callback never performs file, socket, USB, media or UI work.
3. APP and VST3 produce identical commands for identical MIDI.
4. Output thread sends the latest immutable frame at the configured cadence.
5. Output Arm is ephemeral and never restored as true.
6. Blackout and shutdown publish safe output through the same backend path.
7. Project semantics never fork by operating system.

## 7. Update sequence

### 0.3.0-a1 — Shared product spine

- create `AeylaApplicationModel`, command API and immutable snapshots;
- connect canonical example project;
- connect shared executor, semantic and DMX compiler;
- make one MIDI note produce one deterministic executor state and DMX frame;
- expose a DMX frame monitor while output remains simulated;
- add golden parity tests.

### 0.3.0-a2 — Real editor lifecycle

- New/Open/Save/Save As;
- `.aeylashow` schema validation and atomic save;
- fixture profile/channel reorder editor;
- patch editor;
- dirty state, undo/redo and recovery;
- document-driven sources, fixtures and executors.

### 0.3.0-a3 — VST3 host integration

- bounded callback ingress;
- same application/runtime engine as standalone;
- packaged project locator and hot reload;
- safe state persistence;
- Steinberg Validator;
- real Ableton Windows and macOS tests.

### 0.3.0-a4 — Output integration

- Art-Net unicast backend and configuration;
- network-adapter selection and diagnostics;
- arm gating and connection-loss handling;
- packet capture/golden receiver test;
- named physical node/fixture validation;
- USB-DMX only after exact supported devices are selected.

### 0.3.0-a5 — Packaging and operational QA

- Windows installer + portable ZIP;
- macOS app/VST3 package with signing/notarization behavior documented;
- resolution/scaling matrix;
- two-hour soak tests;
- crash/reopen, network loss, project reload and shutdown tests;
- release manifest and checksums.

## 8. Alpha 0.3 release gate

A package may be called **Integrated Alpha 0.3** only when:

- all four paths load the same project;
- the same MIDI sequence produces byte-identical executor and DMX captures;
- project save/open round trip passes Windows ↔ macOS;
- Windows and macOS standalone launch tests pass from clean source builds;
- VST3 passes Validator and real Ableton scan/load/save/reopen on both operating systems;
- Art-Net is either physically validated or clearly disabled and excluded from the product claim;
- output always starts disarmed;
- blackout is verified end to end;
- known limitations are visible in the application and release notes.

Until then, use **Graphical Development Build** or a narrower platform-qualified label.
