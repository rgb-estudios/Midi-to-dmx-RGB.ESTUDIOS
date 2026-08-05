# MIDI executors

## Input

- MIDI note-on.
- MIDI note-off.
- Velocity 1–127.
- Configurable channel, default channel 1.
- No dependence on MIDI clock for version 1.

## Executor identity

Executors have stable UUIDs and human labels. MIDI note assignment is editable without changing scene identity.

## Trigger modes

- **Momentary:** active from note-on until note-off.
- **Toggle:** each note-on toggles state.
- **Exclusive/latch:** activates and releases other executors in the same exclusive group.
- **One-shot:** starts once and ends at source duration.
- **Loop:** repeats until release.
- **Flash:** temporary high-priority overlay.
- **Replace:** replaces selected layers/groups.
- **Release:** releases named executor/group.

## Velocity

Per-executor options:

- ignore velocity;
- scale intensity;
- scale speed;
- select variant/range.

## Layer resolution

- Intensity: HTP by default.
- Colour/zoom/macro: LTP by priority and activation timestamp.
- Blackout: absolute highest priority.
- Reset: never mixed; protected action only.
- Haze: explicit maximum and emergency-off override.

## Note map stability

The editor validates duplicate note/channel assignments. Export can include a printable note map and Standard MIDI File reference, but the show package remains the source of truth.
