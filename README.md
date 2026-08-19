# AEYLA Visual DMX

AEYLA Visual DMX is a cross-platform lighting authoring and playback system for
the Aeyla live show. It converts named Looks and Cues into semantic lighting
attributes and then maps those attributes to fixture-specific DMX channels.

One logical AEYLA instance owns the whole lighting show:

- up to 15 internal Songs;
- Rig 10 physical PAR / Rig 14 programmed PAR;
- one DMX universe over Art-Net in v1;
- VST3 for REAPER/Ableton on Windows and macOS;
- AUv2 for Logic Pro on macOS;
- the same authoring workflow in the standalone application.

The DAW owns audio, stems, click and global transport. AEYLA owns Rig, Patch,
Looks, Cues, lighting timeline, MIDI Learn, deterministic DMX and output safety.
MIDI is a transport/control protocol, not the creative programming language.

## Current repository state

This repository is an auditable **Alpha 0.3 development branch**, not a
show-ready build.
It currently includes:

- C++ semantic lighting core.
- DMX compiler and ArtDMX packet encoder.
- Unit tests proving that fixture channel reordering does not alter programmed looks.
- Native iPlug2 APP/VST3/AUv2 product surface under active integration.
- A source-level `Look → Cue → Song` authoring slice with complete Look schema,
  15-Song navigation and explicit DAW Song-start bindings.
- An independent lighting runtime worker so musical state is not owned by the
  editor's `OnIdle` callback, plus an offline-render disarm/blackout inhibit.
- Configurable Art-Net unicast output at 40 FPS on its own network worker, with
  startup-disarmed ownership, latest-frame publication and fail-closed send
  errors.
- JSON schemas for projects and fixture profiles.
- Full architecture, product, visual, QA, release and agent documentation.
- GitHub Actions CI configuration.

Current hard limits remain: Art-Net is integrated in source but has not been
validated with the named node/PAR hardware, node reachability is not yet
monitored, the Windows standalone OpenGL startup P0 is unresolved, host
save/reopen and editor-closed behavior require real REAPER/Ableton/Logic
evidence, and no hardware or full-show soak has been performed.

## Alpha installer outputs

The native packaging lane produces exactly three user-facing files from one
commit:

1. Windows x64 `.exe` installer for the VST3 used by REAPER and Ableton.
2. macOS 11+ universal `.pkg` installer for VST3 and Logic AUv2.
3. Cross-platform manual VST3 pack with selective install, audit and uninstall
   tools.

Alpha installers are unsigned and explicitly labeled `UNSIGNED`. They are test
candidates, not rehearsal or show releases. The Windows standalone is excluded
while OpenGL issue #17 remains unresolved; packaging never waives a failed
runtime gate.

## First test

### Core

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/aeyla_core_demo
```

### Visual prototype

Open:

```text
prototype/ui/index.html
```

No web server or installation is required.

## Visual prototype

![AEYLA Visual DMX editor prototype](docs/ui-prototype-preview.jpg)

The browser prototype is an interaction and visual-design reference, not the production standalone application.

## Repository principles

1. Looks target semantic attributes, never absolute DMX channels.
2. Fixture profiles translate semantic attributes to physical slots and ranges.
3. Standalone and VST3 load the same `.aeylashow` package.
4. AEYLA does not reproduce or analyse production audio; it receives MIDI and
   host transport position.
5. Art-Net is the only output backend in v1. USB-DMX is explicitly out of v1.
6. Safety states take precedence over artistic states.
7. Visual design is part of the product specification and must be tested.

See [`AGENTS.md`](AGENTS.md),
[`docs/AEYLA_WORKFLOW_CONTRACT_ES.md`](docs/AEYLA_WORKFLOW_CONTRACT_ES.md) and
[`docs/AEYLA_VALIDATION_MATRIX.md`](docs/AEYLA_VALIDATION_MATRIX.md) before
changing product behaviour.

## Canonical repository

```text
https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS
```

This public repository is the sole canonical source for the project. Do not mirror active development into the Tour Manager or RGB Estudios website repositories.

## Source references

- iPlug2 supports VST3 and standalone targets and uses a permissive zlib-like licence: https://github.com/iPlug2/iPlug2
- Official Art-Net specification: https://art-net.org.uk/art-net-specification/
- Steinberg VST3 build documentation: https://steinbergmedia.github.io/vst3_dev_portal/
- ENTTEC DMX USB Pro API: https://support.enttec.com/dmx/usbdmx-dmx-usb-pro-70304/dmx-usb-pro-api
