# `.aeylashow` project format

## Current Alpha 0.3 package

A `.aeylashow` file is a bounded ZIP archive with exactly two root entries:

```text
show.aeylashow
├── project.json   # rig, patch, complete Looks, output preferences and UI state
└── show.bin       # up to 15 Songs, Cues and song-relative Cue placements
```

Legacy Alpha packages containing only `project.json` remain readable. They
migrate to an empty authoring Show and therefore cannot pass the ARM
performance gate until at least one Cue is stored in every Song.

The current implementation rejects unknown entries, directories, encryption,
ZIP64, archives over 9 MiB, oversized entries and asset-bearing documents. A
save writes a temporary package, flushes it, reads it back, rotates the previous
valid file to `.bak` and then publishes the replacement.

## Versioning

- `project.json` currently uses schema `2.0`.
- Project schema `1.x` is migrated explicitly to `2.0`; every legacy Look gets
  deterministic colors, intensity, effect speed, RGBW/A/UV extraction and a
  fourteen-position fixture mask.
- A newer unsupported project major is rejected.
- `show.bin` currently uses codec `1.1`.
- Show codec `1.0` is migrated by deriving a Cue-owned MIDI binding from legacy
  placement fields only when the mapping is unambiguous; ambiguous packages
  fail closed.
- DAW placement (`Song ID → host start PPQ`) is session/plugin state, not part
  of the portable `.aeylashow`.

## Complete Look contract

Every persisted Look owns its source, primary and secondary RGB colors,
intensity, animation speed, white extraction, amber extraction, UV amount and a
fourteen-position fixture mask. Grand Master, Blackout and Output Arm are
operator/runtime controls and are not captured as artistic Look content.

## Media boundary

The document model contains future asset/checksum records, but packages with
assets are deliberately blocked until bounded streaming SHA-256 verification
and portable embedded-media handling are implemented. Absolute local media
paths are never accepted as a portability substitute.

## Schema files in this repository

`schemas/aeylashow.schema.json` and `examples/projects/aeyla-rig10.json` are
foundation-era conceptual examples; they are not the runtime `project.json`
contract and must not be used to author current packages. The C++ bounded
parser/serializer plus its migration tests are authoritative for Alpha 0.3.
