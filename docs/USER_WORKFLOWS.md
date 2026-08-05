# User workflows

## Authoring without Ableton

1. Open Editor.
2. Create/open project.
3. Select rig 10 or 14.
4. Import or create visual source.
5. Position sample points.
6. Create palette/look/scene.
7. Assign executor MIDI note and trigger mode.
8. Preview locally.
9. Patch fixture profiles and addresses.
10. Test through Art-Net/USB if hardware is available.
11. Validate project.
12. Export `.aeylashow`.

## Sending a correction

1. Open last approved project.
2. Save As a new incremented version.
3. Make correction.
4. Run project validation and preview.
5. Export complete package.
6. Send package plus short change note.
7. Operator replaces file and clicks `Reload Show`.
8. Confirm test note, blackout and output state.
9. Keep previous package for rollback.

## Last-minute fixture replacement

1. Open Editor.
2. Duplicate nearest fixture profile.
3. Enter new channel footprint and semantic slots/ranges.
4. Run profile test sequence.
5. Replace profile across selected logical fixtures.
6. Preserve addresses or repatch as required.
7. Export corrected package.
8. No look/MIDI/Ableton edit should be necessary.

## Runtime startup

1. Connect node/interface and fixtures.
2. Open Ableton Set.
3. Load runtime/project if not embedded.
4. Verify project checksum/version.
5. Select output backend override.
6. Test one safe fixture/palette.
7. Test blackout and haze off.
8. Arm output.
9. Run show.

## Emergency

- Blackout executor and UI control always available.
- Haze emergency-off separate from blackout.
- Output disarm closes backend after safe frame.
