# AGENTS.md — mandatory instructions for AI agents and future maintainers

Read this file before changing code, schemas, UI, fixtures, build files or documentation.

## 1. Product objective

AEYLA Visual DMX maps reusable Looks and Cues to deterministic DMX lighting. A
single AEYLA instance owns the complete lighting show (up to 15 internal Songs)
inside REAPER, Ableton Live or Logic Pro, while the DAW remains authoritative
for audio and transport. The standalone application and every plugin format
share the same authoring model, runtime and portable `.aeylashow` file.

Windows standalone, macOS standalone, Windows REAPER/Ableton VST3, macOS
REAPER/Ableton VST3 and Logic Pro AUv2 are mandatory product paths. They are not
later ports.

The application is **not**:

- an audio-reactive tool;
- a replacement for a full lighting console;
- a universal fixture library;
- a video-output server for the projector;
- a DAW project editor or audio player.

## 2. Invariants — do not break

1. **Semantic first:** looks output attributes such as `dimmer`, `red`, `strobe` and `zoom`; they never output hard-coded channel numbers.
2. **Patch independence:** changing fixture model, mode, address or output backend must not require reprogramming looks, scenes, executors or MIDI clips.
3. **Shared engine:** standalone and VST3 use the same core rendering, semantic, profile, project, runtime, safety and DMX code.
4. **Cross-platform parity:** Windows and macOS may use different OS adapters, packaging and signing, but must not fork artistic, semantic, project or executor behavior.
5. **Portable project:** a `.aeylashow` package must contain all referenced media or explicitly mark externally linked media.
6. **Safe startup:** dimmer, strobe, haze, macros and reset start safe; DMX output starts disarmed.
7. **No audio dependency:** optional linked audio/waveform metadata may assist
   visual authoring, but AEYLA never owns production playback, audio routing or
   audio analysis unless a separately approved decision changes scope.
8. **Visual quality:** do not reduce the editor to generic developer UI. Follow `docs/VISUAL_DESIGN_SYSTEM.md` on both Windows and macOS.
9. **Determinism:** the same project, timestamp, MIDI state and fixture profile must generate the same semantic frame and DMX frame on standalone and VST3 across both operating systems.
10. **Real-time isolation:** network/USB I/O, project I/O and media decoding must never block the host audio thread.
11. **Backward compatibility:** project migrations are explicit and tested.
12. **Early host proof:** do not postpone all Ableton/VST3 work until the rest of the runtime is complete. Maintain a thin loadable host adapter from the first integrated milestone so architectural mistakes are found early.

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
2. State the exact layer being changed: visual source, semantic engine, profile compiler, project format, output backend, UI, runtime or host adapter.
3. State platform impact for Windows standalone, macOS standalone, Windows VST3 and macOS VST3.
4. Add or update tests before claiming completion.
5. Build the core and run CTest.
6. Keep CI green on Linux, Windows and macOS when the changed layer is portable.
7. Validate JSON examples against schemas when schemas change.
8. For UI changes, capture Windows and macOS evidence at required resolutions/scaling.
9. For VST3 changes, run validator plus Ableton scan/load/save/reopen on both operating systems before claiming host support.
10. Update `CHANGELOG.md` and any affected docs.
11. Do not mark hardware features “verified” without a real-device test record.

## 5. Completion vocabulary

Use these exact statuses:

- **Specified:** behaviour is documented only.
- **Scaffolded:** code structure exists but is not fully executable.
- **Implemented:** automated tests pass on every required CI platform for the component.
- **Simulated:** validated against software-generated inputs/packets on the named operating systems.
- **Host-tested:** loaded and exercised in the named Ableton Live version and operating system, with evidence.
- **Hardware-tested:** validated with named physical hardware and recorded conditions.
- **Show-tested:** used successfully in a full rehearsal/show soak test.

Never substitute “done” for these statuses. Never use an unqualified `Implemented` or `Host-tested` when only one required operating system was validated.

## 6. Architecture boundaries

- `src/core`: deterministic code, no UI, DAW or hardware dependency.
- `src/io`: Art-Net, USB and device enumeration; no artistic logic.
- `src/media`: image/video decoding and visual-frame providers through cross-platform interfaces.
- `src/runtime`: executor state, layer mixing and show clock.
- `apps/editor`: shared standalone editor plus thin Windows/macOS platform packaging.
- `product/AeylaVisualDmx`: thin VST3/AUv2/standalone adapters and native UI;
  no duplicated artistic logic.
- `prototype/ui`: disposable interaction prototype; never treat as production implementation.

## 7. Platform acceptance gates

No integrated alpha or later milestone may pass unless:

- Windows standalone launches and loads the canonical example project;
- macOS standalone launches and loads the same project;
- Windows REAPER and Ableton scan and load the VST3;
- macOS REAPER and Ableton scan and load the universal VST3;
- Logic Pro scans and loads the AUv2;
- the same MIDI sequence produces matching executor state and DMX output in all four paths;
- save/reopen and missing-project behavior are documented and tested;
- output starts disarmed in all four paths.

CI compilation alone does not equal Ableton compatibility. Wine does not equal a real Windows host test. A macOS CI build does not equal a real Mac Ableton host test.

## 8. Safety review triggers

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

## 9. Future-agent audit checklist

Before beginning an audit, report:

- current build status on Linux, Windows and macOS;
- current Ableton host-test status on Windows and macOS;
- current test count and failures;
- schema version;
- hardware-validation matrix;
- unresolved P0/P1 issues;
- differences between standalone and VST3 behaviour;
- differences between Windows and macOS behaviour;
- visual regressions;
- project migration risks;
- third-party dependency and licence changes.

## 10. Prohibited shortcuts

- Do not map visual RGB directly into physical slots.
- Do not perform UDP or serial writes in the VST audio callback.
- Do not embed absolute local media paths in exported projects.
- Do not silently clamp dangerous reset/macro ranges without warning the user.
- Do not introduce a dependency without documenting licence, version pinning and update procedure.
- Do not claim “generic USB DMX” compatibility. Name and test each protocol family.
- Do not claim macOS support from a Linux POSIX code path alone.
- Do not claim Windows support from MinGW/Wine alone.
- Do not claim Ableton compatibility before a real VST3 has been scanned, loaded, saved and reopened in Ableton on the named OS.
