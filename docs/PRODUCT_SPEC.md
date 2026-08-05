# Product specification

## Product name

Working name: **AEYLA Visual DMX**.

## Problem

The lighting show must be authored without requiring the designer to own Ableton. The performance computer already uses Ableton and must trigger lighting through MIDI notes. Fixture models and channel orders may change at the venue without forcing the show to be reprogrammed.

## Non-negotiable platform baseline

The first usable product version must operate as one integrated system on:

- **Windows standalone editor**;
- **macOS standalone editor**;
- **Windows VST3 runtime inside Ableton Live**;
- **macOS VST3 runtime inside Ableton Live**.

Windows, macOS and Ableton integration are not later ports and are not optional stretch goals. A milestone cannot be described as a usable alpha, rehearsal candidate or release candidate unless all four execution paths above build, launch and pass the platform acceptance tests defined in `docs/TEST_PLAN.md`.

Linux remains a CI and development platform for shared core logic, but it is not a required user-facing release target.

## Product split

### Standalone Editor

Used by the lighting designer on a separate Windows or macOS computer without Ableton.

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
- no Ableton dependency;
- identical project behavior and visual hierarchy on Windows and macOS.

### VST3 Runtime

Used on the final Ableton computer on Windows or macOS.

Capabilities:

- load a packaged `.aeylashow` project;
- receive note-on, note-off and velocity;
- execute scenes and layers deterministically;
- optional host-transport position for visual timing;
- output Art-Net or supported USB-DMX;
- blackout, output arm, connection state and hot reload;
- no audio input, no audio output and no full editor;
- identical semantic, runtime, project and DMX behavior on Windows and macOS.

## Shared-engine requirement

The standalone editor and VST3 runtime must use the same implementation of:

- project loading and migration;
- visual rendering and sampling;
- semantic attribute generation;
- fixture profile compilation;
- executor/layer runtime;
- safety state;
- Art-Net and USB-DMX output backends.

Platform-specific code is restricted to windowing, plugin packaging, filesystem integration, network/USB system APIs, signing and installer concerns. No artistic or semantic behavior may be forked per operating system.

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
- mobile remote;
- more than one universe.

## First integrated milestone

The first integrated milestone is not complete until it includes:

1. a launchable standalone shell on Windows and macOS;
2. a loadable VST3 shell in Ableton Live on Windows and macOS;
3. one shared `.aeylashow` example loaded by all four paths;
4. one MIDI note triggering the same executor state and byte-identical DMX frame in both VST3 hosts and both standalone builds;
5. output starting disarmed in all four paths;
6. CI builds for Linux core, Windows and macOS;
7. real Ableton scan/load/save/reopen evidence on one Windows machine and one Mac before the milestone is called Hardware-tested or usable.

## Success criteria

1. Reordering every channel of a fixture profile preserves the programmed look.
2. Switching from rig 10 to rig 14 requires no MIDI or look changes.
3. A corrected show file can replace the previous file without editing Ableton.
4. Runtime can operate continuously for a two-hour soak test without output stalls.
5. Output starts disarmed and safe after every load/crash/restart path.
6. Editor remains visually legible at 1366×768 and equivalent macOS scaling.
7. Windows and macOS standalone builds render and save the same project semantics.
8. Windows and macOS Ableton hosts produce matching executor and DMX captures from the same MIDI sequence.
