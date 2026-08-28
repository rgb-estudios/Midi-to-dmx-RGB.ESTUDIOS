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

## Recorded-Take Show transport

The recorded-Take transport is a separate command namespace from creative
executors. It defaults to disabled on channel 16 and exposes configurable
Previous, Next, Play/Retrigger, Pause/Resume, Stop/Reset and a consecutive
15-note direct-Song range. A mapped transport Note On is consumed before the
executor layer; its corresponding Note Off is also consumed.

Transport events carry their exact in-block sample offset across the realtime
queue. The runtime compensates its own scheduling delay, so the relative DMX
cursor resolves from the MIDI sample rather than from worker wake-up time.
MIDI cannot enable physical output or clear Blackout. Queue overflow is a
fail-safe boundary that disarms output and latches Blackout.
