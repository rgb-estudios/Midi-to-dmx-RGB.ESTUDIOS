# Validation report — 2026-08-05

Commit under validation: foundation branch before publication.

## Environment

- Linux x86_64 container
- CMake 3.31.6
- GCC 14.2.0
- Chromium headless
- Python Playwright

## Core validation

Commands:

```bash
cmake -S . -B /tmp/aeyla-build
cmake --build /tmp/aeyla-build
ctest --test-dir /tmp/aeyla-build --output-on-failure
/tmp/aeyla-build/aeyla_core_demo
```

Result:

- Build passed with warnings treated as errors.
- 1/1 automated test executable passed.
- Semantic fixture reorder assertions passed.
- Rig-10 logical/physical assertions passed.
- ArtDMX packet assertions passed.
- Demo produced a 530-byte ArtDMX packet.

## UI prototype validation

Automated checks:

- page rendered at 1440×900;
- no JavaScript page errors;
- eight executors present;
- source selection changed canvas state;
- rig 14 control activated;
- semantic profile dialog opened;
- output arm state changed;
- screenshot captured.

## JSON validation

All repository JSON files parse successfully. Full JSON Schema instance validation remains a Stage 1 task.

## Not validated

- Windows compilation; GitHub Actions is configured but has not run because the repository is not yet published.
- Art-Net network transmission to a physical node.
- USB-DMX hardware.
- Ableton/VST3.
- Video decoding.
- Full standalone application.
- Show operation.
