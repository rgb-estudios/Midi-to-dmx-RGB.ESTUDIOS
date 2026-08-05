# Test plan

## Automated unit tests

- Semantic attribute clamp/read/write.
- RGB→RGBWAL transformation.
- Profile reorder preserves semantic result.
- Missing dimmer scales emitters.
- Range, constant, trigger, invert and home mapping.
- Address bounds and overlapping fixtures warnings.
- Rig 10 retains 14 logical positions but 10 active physical outputs.
- ArtDMX header, opcode, version, universe, sequence and length.
- Project migration and checksum validation.
- Executor trigger modes and priority resolution.

## Golden tests

Persist known semantic frames and expected DMX frames for fixture profiles. Any difference requires deliberate approval and changelog entry.

## UI tests

- 1366×768 and 1920×1080 screenshots.
- Keyboard navigation.
- Profile reorder and bulk replacement.
- Missing-media presentation.
- Output state visibility.
- Project export/import round trip.

## Integration tests

- Standalone and VST3 load identical project.
- Same MIDI sequence produces byte-identical DMX capture.
- Hot reload does not emit unsafe intermediate values.
- Ableton stop/start policy.
- Media seek and one-shot restart.

## Hardware tests

Named evidence required for:

- Art-Net node model/firmware.
- Network adapter and Windows version.
- DMX USB Pro model/driver.
- Open DMX model/driver.
- Every fixture profile used in the show.

## Performance tests

- Two-hour continuous runtime.
- 40 Hz output timing distribution.
- CPU/GPU at full Ableton + video load.
- Media underrun count.
- MIDI-to-DMX latency.
- Project hot reload.

## Safety tests

- Startup disarmed.
- Blackout priority.
- Haze emergency-off.
- Reset protected trigger.
- Cable/network/device loss.
- Host crash/close.
- Reopen after unclean shutdown.
