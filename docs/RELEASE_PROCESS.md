# Release process

## Versioning

- App version: semantic versioning.
- Project schema version: independent semantic version.
- Build includes Git commit SHA.

## Required checks

1. Core build and tests.
2. Schema/example validation.
3. UI screenshot comparison.
4. VST3 validation when plugin exists.
5. Standalone clean-machine launch.
6. Project round trip.
7. Safety checklist.
8. Hardware matrix relevant to release.

## Artefacts

- `AEYLA-Visual-DMX-Editor-<version>-win64.exe`
- `AEYLA Visual DMX Runtime.vst3`
- checksums file;
- release notes;
- fixture profile pack;
- example project;
- operator manual;
- rollback package.

## Release channels

- `alpha`: editor/developer testing.
- `rehearsal`: target hardware and session testing.
- `show`: only after full rehearsal soak and recovery drills.

## Rollback

Keep previous binary, VST3 and `.aeylashow`. A project correction never deletes the last known good package.
