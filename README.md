# AEYLA Visual DMX

AEYLA Visual DMX is a Windows-first visual-to-DMX system designed for the Aeyla live show.
It converts visual sources (solid colours, gradients, procedural animations, images and short videos) into semantic lighting attributes, then maps those attributes to fixture-specific DMX channels.

The project has two products that share one engine and one portable show file:

- **AEYLA Visual DMX Editor** — standalone editor used without Ableton.
- **AEYLA Visual DMX Runtime** — VST3 runtime loaded in Ableton to receive MIDI notes and output DMX.

## Current repository state

This repository is an auditable **foundation and executable prototype**, not a finished show-ready VST3.
It currently includes:

- C++ semantic lighting core.
- DMX compiler and ArtDMX packet encoder.
- Unit tests proving that fixture channel reordering does not alter programmed looks.
- Interactive browser prototype of the standalone editor.
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
4. The VST3 does not analyse audio; it receives MIDI and optional host transport position only.
5. Output backends are replaceable: Art-Net, DMX USB Pro compatible and Open DMX/FTDI.
6. Safety states take precedence over artistic states.
7. Visual design is part of the product specification and must be tested.

See [`AGENTS.md`](AGENTS.md) before any automated edit and [`docs/INDEX.md`](docs/INDEX.md) for the complete documentation map.

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
