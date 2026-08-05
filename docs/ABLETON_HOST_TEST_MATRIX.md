# Ableton host-test matrix

## 1. Purpose

Compilation, Steinberg Validator, Wine and another DAW are useful but do not prove Ableton compatibility. This matrix defines the evidence required on real Windows and macOS systems.

## 2. Required test hosts

### Windows primary

- Real Windows 11 x64 production-class PC.
- Secondary Windows 10 22H2 where available.
- Ableton Live 12 current authorized build.
- Multiple buffer sizes and sample rates.

### macOS primary

- Real Apple Silicon Mac running Ableton natively.
- Universal VST3 verified as `arm64 + x86_64`.
- Ableton Live 12 current authorized build.
- Intel Mac test remains additional compatibility evidence, not a substitute for Apple Silicon.

Record exact:

- computer model and CPU;
- OS version/build;
- Ableton version/edition;
- plugin version and commit;
- audio interface/driver;
- sample rate and buffer;
- project package checksum;
- network/output configuration.

## 3. Scan and installation

- Clean plugin install.
- Ableton launches without crash.
- VST3 scan completes.
- Plugin appears once under the expected vendor/category.
- Update over previous alpha preserves identity.
- Uninstall removes binaries but preserves user projects.
- Missing/invalid signature behavior documented.
- macOS Gatekeeper accepts signed/notarized build.
- Apple Silicon Ableton recognizes the plugin without Rosetta.

## 4. Instantiation and audio topology

- Insert on a MIDI track.
- Event input bus active.
- Stereo output bus accepted.
- Audio output remains exactly silent.
- Output buffers are cleared and silence flags correct.
- Device UI may remain closed; runtime still works.
- Open/close UI repeatedly.
- Duplicate device and track.
- Delete device while output is armed.
- Undo/redo device insertion and deletion.

## 5. MIDI behavior

Canonical MIDI file and manual tests cover:

- note-on for every mapped executor;
- note-off for momentary executors;
- velocity 1, 64, 127;
- overlapping notes;
- repeated note-on without note-off;
- note-off without matching note-on;
- same note on different channels;
- dense burst beyond expected show load;
- looped MIDI clips;
- clip launch while transport stopped/playing;
- track mute, solo, arm and device bypass;
- All Notes Off and panic behavior;
- queue overflow test and safety response.

Capture the resulting executor timeline and DMX frames. Compare byte-for-byte with standalone using the same project and event sequence.

## 6. Transport behavior

- Play from start.
- Stop and resume.
- Start from arbitrary locator.
- Scrub/seek forward and backward.
- Loop with notes crossing the loop boundary.
- Tempo automation.
- Time-signature changes.
- Count-in and recording.
- Arrangement and Session View triggering.
- Ableton transport stopped while runtime output is armed.
- Device processing suspended/resumed by host optimization.

No stuck momentary, strobe or haze state is permitted.

## 7. State and Set persistence

- Save/reopen Set with project available.
- Reopen with project missing.
- Reopen with same path but wrong project UUID.
- Move Set plus companion `.aeylashow` package to another machine.
- Copy track/device to another Set.
- Duplicate Set folder.
- Replace project package with corrected version and reload.
- Upgrade plugin while opening an older Set.
- Downgrade behavior documented and safe.
- Corrupt plugin state test.
- Output always restores disarmed.

## 8. Output and failure tests

Using a software capture first, then named hardware:

- Art-Net unicast.
- Art-Net broadcast.
- Explicit NIC binding.
- Network cable removal/reconnect.
- Destination node power-cycle.
- Adapter IP change.
- Output backend start failure.
- Repeated arm/disarm.
- Blackout under load.
- Ableton crash/forced termination recovery observation.
- Windows sleep/wake and macOS sleep/wake are documented; show operation should prohibit sleep.

USB tests are separate per named protocol/device.

## 9. Performance matrix

Test at:

- sample rates: 44.1, 48, 88.2, 96 kHz; higher only if production uses it;
- buffers: 32, 64, 128, 256, 512, 1024, 2048 samples where interface supports them;
- Ableton CPU load: low, representative and stress;
- projector video active in the final session;
- UI closed and open;
- one-hour alpha soak and two-hour release soak.

Measure:

- MIDI event enqueue time;
- event queue high-water mark and overflow;
- executor-to-DMX latency;
- DMX refresh interval distribution;
- runtime missed ticks;
- media underruns;
- CPU/GPU and memory growth;
- output failures and recovery time.

## 10. Offline operations

- Export audio/video.
- Freeze/flatten where Ableton permits.
- Faster-than-realtime export.
- Project consolidation/collect all and save.

Expected: external DMX output remains disabled during offline processing unless an explicit future rehearsal tool implements a real-time-only export path.

## 11. UI and accessibility

- Windows scaling 100%, 125%, 150%, 200%.
- macOS Retina scaling on supported displays.
- Minimum Ableton-supported display sizes.
- Keyboard focus does not steal performance shortcuts unexpectedly.
- Persistent output state and blackout visible.
- Missing project and backend failure readable without opening logs.
- UI repaint cannot stall host processing.

## 12. Pass/fail rules

A platform passes only when every P0 case passes and all P1 failures have a documented workaround accepted by RGB Estudios.

Immediate release blockers:

- crash, hang or deadlock;
- audible non-zero output;
- stuck strobe/haze/momentary executor;
- output restored armed after load;
- lost note-off without safety recovery;
- different scene/DMX result between standalone and VST3;
- plugin missing in native Apple Silicon Ableton;
- corrupted Set or state;
- network/file work detected in host callback;
- unbounded memory growth.

## 13. Evidence template

```text
Status: Host-tested/Windows | Host-tested/macOS | Failed
Plugin version/commit:
OS/build:
Machine/CPU:
Ableton version/edition:
Audio interface/driver:
Sample rate/buffer:
Project UUID/checksum:
Cases passed:
Cases failed:
DMX capture hash:
Logs/screenshots/video:
Known limitations:
Operator:
Date/time:
```
