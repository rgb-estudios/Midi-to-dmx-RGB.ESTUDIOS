# Validation report — 2026-08-05

Foundation source validated locally before publication; canonical GitHub foundation commit: `689d09694dc50bb98eaeb3723dee44155ca6d680`.

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

## GitHub publication validation

- Canonical public repository is available.
- README, source, schemas, prototype, governance files and agent handoff were published to `main`.
- Issues #1–#5 were created for governance, Art-Net, project persistence, native UI and hardware validation.
- GitHub Actions workflow files are present. At the time of this report the connector had not yet returned a workflow run for the publication commit; CI status therefore remains pending external confirmation.

## Not validated

- Windows compilation through GitHub Actions or a target Windows workstation.
- Art-Net network transmission to a physical node.
- USB-DMX hardware.
- Ableton/VST3.
- Video decoding.
- Full standalone application.
- Show operation.
