# `.aeylashow` project format

## Packaging

A project is a ZIP-compatible package with extension `.aeylashow`.

```text
show.aeylashow
├── manifest.json
├── project.json
├── rig.json
├── patch.json
├── midi-map.json
├── palettes.json
├── looks.json
├── scenes.json
├── executors.json
├── profiles/*.json
├── media/*
└── checksums.json
```

## Requirements

- UTF-8 JSON.
- Semantic version in manifest.
- Stable UUIDs for all authored entities.
- Relative internal media paths.
- SHA-256 checksums for packaged media and JSON documents.
- Atomic save: write temp package, validate, then replace.
- Preserve last known good backup.

## Compatibility

- Patch versions may add optional fields without breaking older runtimes.
- Major schema changes require explicit migration.
- Runtime rejects newer unsupported major versions with a clear message.
- Editor keeps a migration log in the exported package.

## Correction delivery

The standard correction artefact is a complete project package, not an Ableton set. The runtime supports `Reload Show`; existing MIDI clips remain unchanged when executor IDs/notes are stable.

## Optional delta package

A later phase may introduce `.aeylapatch`, but full package replacement is preferred initially because it is easier to validate and roll back.
