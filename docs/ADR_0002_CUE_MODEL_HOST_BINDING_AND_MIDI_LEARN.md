# ADR 0002 — Cue model, MIDI Learn ownership and DAW session binding

- Status: **Accepted**
- Date: 2026-08-07
- Scope: AEYLA Visual DMX Alpha 0.3+

## Context

The initial Alpha model represented a timeline block as a MIDI note with note/channel/velocity stored directly on the clip. That was useful to prove MIDI ingress, but it leaks the piano-roll implementation into the lighting authoring model.

The product workflow is Cue-first: the operator thinks in named lighting states and transitions. MIDI is a control/capture surface, and the DAW is transport/playback.

Two additional requirements make the distinction important:

1. The same Cue may appear many times in one song.
2. The same portable `.aeylashow` may be used in different DAW sessions where the song starts at a different host position.

## Decision 1 — Scene/Cue owns MIDI Learn mapping

The final authoring model SHALL treat MIDI binding as metadata of a Cue/Scene, not of each timeline placement.

Conceptual model:

```text
SceneDefinition
  id
  name
  Look reference
  behavior: latch | momentary
  transitions
  optional MIDI binding:
    channel 1..16
    note 0..127

SceneClip / CuePlacement
  id
  scene reference
  start tick
  duration (semantic for momentary; editor/export metadata for latch)
```

Consequences:

- `MIDI LEARN` selects a Scene and captures one physical key/pad mapping.
- Reusing the Scene at multiple timeline positions does not duplicate its mapping.
- Live MIDI can trigger an authored Scene even before it has been captured into a timeline placement.
- Duplicate `(channel,note)` mappings between different Scenes in the same active song are invalid.
- Timeline playback never requires MIDI events to reconstruct state.
- MIDI export/fallback is compiled *from* Cue placements plus Scene mappings; it is not the source of truth.

The Alpha `MidiSceneClip.note/channel/velocity` fields remain only as portable
placement/legacy-codec compatibility. `SceneDefinition.midi_binding` is now the
runtime and authoring source of truth.

## Decision 2 — Timeline placement is not MIDI identity

The internal timeline SHALL be expressed in song-relative ticks and named Cue placements.

A visual editor may display blocks, fades and momentary duration, but the primary label is the Cue name. Raw note numbers are secondary diagnostics only.

## Decision 3 — DAW placement is session state

`.aeylashow` stores song-relative musical programming.

A concrete REAPER/Ableton/Logic instance stores a separate session binding:

```text
Song ID -> Host Start PPQ
```

The user establishes it with a workflow equivalent to:

`SET SONG START = PLAYHEAD`

The runtime computes:

```text
relative PPQ = host PPQ - host start PPQ
song tick    = floor(relative PPQ * song PPQ)
```

The runtime SHALL NOT silently assume host PPQ 0 is song start.

## Decision 4 — Seek is authoritative

On Play, Seek or Loop, the current absolute host position is projected to a song tick and the Cue state is reconstructed from that tick.

MIDI event history is never required to know which Cue should be active.

A missing/invalid binding cannot produce an artistic output by accident. In Show Mode it resolves to a safe non-output/black state until corrected.

## Decision 5 — Offline render is not a show transport

DAW offline render/bounce must never become an accelerated lighting performance. The realtime output lifecycle SHALL have a hard offline-render inhibit separate from musical Cue resolution.

## Decision 6 — Look must become a complete artistic state

A Scene references a Look. A production Look must be reproducible without relying on mutable global editor settings or hardcoded source colors.

The persisted project schema v2 now makes every Look own:

- primary color;
- secondary color where the source requires it;
- intensity;
- visual source/type;
- speed/rate;
- white extraction;
- amber extraction;
- UV amount;
- fixture participation/mask or equivalent grouping state required by Rig 10/Rig 14.

Grand Master, Output Arm, Blackout and Panic remain runtime/operator safety controls and are not artistic Look content.

Transition interpolation is only considered complete when it interpolates between two complete Look states deterministically.

## Migration requirements

Codec/runtime migration status in CP-AEYLA-0.3.2:

1. Scene/Cue ownership: implemented in source and locally unit-tested.
2. `show.bin` 1.1 plus 1.0 migration: implemented in source and locally tested.
3. Ambiguous legacy mapping: fails closed in codec regression.
4. CueRuntime and MIDI compiler: resolve from Scene mapping.
5. Store Cue UI: allocates a hidden mapping without exposing note numbers.
6. Manual `MIDI LEARN` UI and timeline editing remain **Specified**.

## Rejected alternatives

### Piano-roll as the primary lighting editor

Rejected because it forces lighting semantics into note lengths/numbers, obscures named Cue state, and makes the workflow DAW-specific.

### Store a MIDI binding on every timeline clip

Rejected because it duplicates control identity and permits the same Cue to acquire inconsistent mappings at different positions.

### Store DAW start PPQ inside `.aeylashow`

Rejected because it couples portable show programming to one arrangement/session layout and breaks clean portability between REAPER, Ableton and Logic.
