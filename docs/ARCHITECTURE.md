# Architecture

## Data flow

```text
Visual source / scene / executor
            ↓
Low-resolution RGB canvas
            ↓ sample at logical fixture positions
Per-fixture RGB samples
            ↓ colour transform
Semantic attribute frames
            ↓ layer and priority mixer
Resolved semantic frames
            ↓ fixture profile compiler
DMX universe (512 bytes)
            ↓ output queue
Art-Net or supported USB-DMX backend
```

## Process architecture

```text
                         Shared C++ engine
┌─────────────┐  ┌──────────────┐  ┌────────────┐  ┌─────────────┐
│ Visual/media│→ │ Runtime/mixer│→ │ DMX compiler│→│ Output backend│
└─────────────┘  └──────────────┘  └────────────┘  └─────────────┘
        ↑              ↑                    ↑
        │              │                    │
  Standalone UI     MIDI engine        Profile/patch
        │
        └──────────────── Project package ────────────────┐
                                                          │
Ableton MIDI → thin VST3 host adapter → shared runtime ───┘
```

## Thread model

- **Host/audio callback:** copy MIDI and transport events into lock-free queues only.
- **Render/runtime thread:** update executors, decode/sample frame and resolve semantic state.
- **Output thread:** send latest immutable DMX frame at configured refresh rate.
- **Media decode thread(s):** decode ahead into bounded frame buffers.
- **UI thread:** editing and display only.

No filesystem, network, serial or codec operation may run in the host callback.

## State ownership

- Project file owns authored data.
- Runtime owns transient executor/layer state.
- Output backend owns device/network handles.
- VST3 state stores project identity, optional embedded snapshot, output override and safe runtime parameters.

## Determinism

Runtime frame is a function of:

```text
project version + timeline time + active MIDI events + velocity + runtime overrides
```

Wall-clock randomness must use a persisted seed. Procedural sources must be seekable or explicitly marked free-running.

## Error handling

Errors are categorized:

- validation error: project cannot load;
- degraded media: substitute safe colour/black and report missing media;
- profile error: disable affected fixture and report exact slot;
- backend error: disarm output and retain local preview;
- runtime overrun: reuse last safe frame, count event and warn;
- dangerous action: require explicit confirmation/arming.
