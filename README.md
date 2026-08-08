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
- JSON schemas for projects and fixture profiles.
- Full architecture, product, visual, QA, release and agent documentation.
- GitHub Actions CI configuration.

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
