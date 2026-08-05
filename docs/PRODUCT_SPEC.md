# Product specification

## Product name

Working name: **AEYLA Visual DMX**.

## Problem

The lighting show must be authored without requiring the designer to own Ableton. The performance computer already uses Ableton and must trigger lighting through MIDI notes. Fixture models and channel orders may change at the venue without forcing the show to be reprogrammed.

## Product split

### Standalone Editor

Used by the lighting designer on a separate Windows computer.

Capabilities:

- visual canvas with 14 logical fixture sample points;
- rig 10/14 modes;
- solid, gradient, procedural, image and video sources;
- palettes, looks, scenes and executors;
- semantic fixture-profile editor;
- physical patch editor;
- MIDI note assignment and simulation;
- Art-Net and supported USB-DMX output for testing;
- project validation, packaging and export;
- no Ableton dependency.

### VST3 Runtime

Used on the final Ableton computer.

Capabilities:

- load a packaged `.aeylashow` project;
- receive note-on, note-off and velocity;
- execute scenes and layers deterministically;
- optional host-transport position for visual timing;
- output Art-Net or supported USB-DMX;
- blackout, output arm, connection state and hot reload;
- no audio input, no audio output and no full editor.

## Core objects

- **Visual source:** produces a low-resolution RGB frame.
- **Logical fixture:** position and identity used by visual sampling.
- **Semantic frame:** normalized lighting attributes for one logical fixture.
- **Fixture profile:** maps attributes to DMX slots and ranges.
- **Patch:** binds logical fixtures to profiles, universes and addresses.
- **Palette:** reusable attribute values.
- **Look:** visual source plus transforms and targeting.
- **Scene:** complete layered state.
- **Executor:** MIDI-addressable action.
- **Output backend:** sends compiled DMX through a named protocol.

## Version 1 required attributes

`dimmer`, `shutter`, `strobe`, `red`, `green`, `blue`, `white`, `amber`, `uv`, `lime`, `macro`, `speed`, `reset`, `zoom`, `fan`, `haze`.

## Required trigger modes

- momentary;
- toggle;
- latch/exclusive;
- one-shot;
- loop;
- flash/overlay;
- replace;
- release.

## Required layers

1. Base.
2. Movement/texture.
3. FX/override.
4. Safety override (internal, highest priority).

## Non-goals for version 1

- projector/video output;
- audio analysis;
- universal commercial fixture library;
- 3D visualizer;
- RDM;
- sACN;
- multi-user editing;
- macOS build;
- mobile remote;
- more than one universe.

## Success criteria

1. Reordering every channel of a fixture profile preserves the programmed look.
2. Switching from rig 10 to rig 14 requires no MIDI or look changes.
3. A corrected show file can replace the previous file without editing Ableton.
4. Runtime can operate continuously for a two-hour soak test without output stalls.
5. Output starts disarmed and safe after every load/crash/restart path.
6. Editor remains visually legible at 1366×768.
