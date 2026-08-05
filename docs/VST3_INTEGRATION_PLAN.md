# VST3 integration plan

## Role

The VST3 is a thin host adapter around the shared runtime. It does not duplicate editor logic.

## Host inputs

- note-on/note-off/velocity;
- host sample position and tempo when transport-synced source is selected;
- project path or embedded project snapshot;
- output arm/blackout/runtime parameters.

## Host outputs

- no audio;
- status UI only;
- DMX through shared backend threads.

## State

VST3 state stores:

- runtime settings;
- project UUID/version/checksum;
- portable project reference or embedded package according to final policy;
- output override;
- no live device handle.

## Planned framework

Use iPlug2 after pinning a tested commit. iPlug2 supports VST3 and standalone application targets under a permissive licence. Do not begin adapter work until Stage 1 runtime APIs are stable.

## Validation

- Steinberg validator.
- Ableton plugin scan and reload.
- Save/reopen Set.
- Copy track to another Set.
- Missing project and changed project behaviour.
- MIDI timing capture against standalone.
