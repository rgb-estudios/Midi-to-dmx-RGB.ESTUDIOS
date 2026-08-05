# AGENTS.md — mandatory instructions for AI agents and future maintainers

Read this file before changing code, schemas, UI, fixtures, build files or documentation.

## 1. Product objective

AEYLA Visual DMX maps visual material and reusable scenes to DMX lighting. The user edits projects in a free standalone Windows application and sends a portable show file to a separate Ableton computer. Ableton loads a VST3 runtime and triggers executors with MIDI notes.

The application is **not**:

- an audio-reactive tool;
- a replacement for a full lighting console;
- a universal fixture library;
- a video-output server for the projector;
- an Ableton project editor.

## 2. Invariants — do not break

1. **Semantic first:** looks output attributes such as `dimmer`, `red`, `strobe` and `zoom`; they never output hard-coded channel numbers.
2. **Patch independence:** changing fixture model, mode, address or output backend must not require reprogramming looks, scenes, executors or MIDI clips.
3. **Shared engine:** standalone and VST3 use the same core rendering, semantic, profile, project and DMX code.
4. **Portable project:** a `.aeylashow` package must contain all referenced media or explicitly mark externally linked media.
5. **Safe startup:** dimmer, strobe, haze, macros and reset start safe; DMX output starts disarmed.
6. **No audio dependency:** do not add audio input or audio analysis unless a separately approved product decision explicitly changes scope.
7. **Visual quality:** do not reduce the editor to generic developer UI. Follow `docs/VISUAL_DESIGN_SYSTEM.md`.
8. **Determinism:** the same project, timestamp, MIDI state and fixture profile must generate the same semantic frame and DMX frame.
9. **Real-time isolation:** network/USB I/O and media decoding must never block the host audio thread.
10. **Backward compatibility:** project migrations are explicit and tested.

## 3. Supported semantic attributes

Canonical identifiers are defined in `src/core/attributes.h` and the JSON schemas. Minimum set:

- dimmer
- shutter
- strobe
- red
- green
- blue
- white
- amber
- uv
- lime
- macro
- speed
- reset
- zoom
- fan
- haze
- constant
- ignored

Do not rename identifiers casually. Add migration logic before changing persisted names.

## 4. Required workflow for every change

1. Read the relevant specification and ADRs.
2. State the exact layer being changed: visual source, semantic engine, profile compiler, project format, output backend, UI or runtime adapter.
3. Add or update tests before claiming completion.
4. Build the core and run CTest.
5. Validate JSON examples against schemas when schemas change.
6. For UI changes, capture before/after screenshots and verify at 1366×768 and 1920×1080.
7. Update `CHANGELOG.md` and any affected docs.
8. Do not mark hardware features “verified” without a real-device test record.

## 5. Completion vocabulary

Use these exact statuses:

- **Specified:** behaviour is documented only.
- **Scaffolded:** code structure exists but is not fully executable.
- **Implemented:** automated tests pass.
- **Simulated:** validated against software-generated inputs/packets.
- **Hardware-tested:** validated with named physical hardware and recorded conditions.
- **Show-tested:** used successfully in a full rehearsal/show soak test.

Never substitute “done” for these statuses.

## 6. Architecture boundaries

- `src/core`: deterministic code, no UI, DAW or hardware dependency.
- `src/io`: Art-Net, USB and device enumeration; no artistic logic.
- `src/media`: image/video decoding and visual-frame providers.
- `src/runtime`: executor state, layer mixing and show clock.
- `apps/editor`: standalone editor UI.
- `plugins/vst3`: thin host adapter only.
- `prototype/ui`: disposable interaction prototype; never treat as production implementation.

## 7. Safety review triggers

A change requires explicit safety review when it affects:

- blackout priority;
- haze or fan output;
- reset or lamp-control ranges;
- strobe limits;
- output arming;
- output on host stop/close;
- fixture profile defaults;
- DMX timeout behaviour;
- project auto-load or hot reload.

## 8. Future-agent audit checklist

Before beginning an audit, report:

- current build status;
- current test count and failures;
- schema version;
- hardware-validation matrix;
- unresolved P0/P1 issues;
- differences between standalone and VST3 behaviour;
- visual regressions;
- project migration risks;
- third-party dependency and licence changes.

## 9. Prohibited shortcuts

- Do not map visual RGB directly into physical slots.
- Do not perform UDP or serial writes in the VST audio callback.
- Do not embed absolute local media paths in exported projects.
- Do not silently clamp dangerous reset/macro ranges without warning the user.
- Do not introduce a dependency without documenting licence, version pinning and update procedure.
- Do not claim “generic USB DMX” compatibility. Name and test each protocol family.
