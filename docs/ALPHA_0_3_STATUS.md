# Alpha 0.3 implementation status

## Implemented in source

- Shared product application model.
- 14 logical fixtures with Rig 10/14 activation.
- Deterministic visual sampling.
- Semantic RGBWALUV transformation.
- Reference fixture profile and 512-byte DMX compilation.
- MIDI notes 36–43 through bounded host ingress.
- Non-realtime event consumption from APP/VST3 idle path.
- Startup disarmed and null-backend arm rejection.
- End-to-end model blackout.
- Unit test target for application-model safety and parity.

## Pending validation

- Linux, Windows and macOS core CI for the new model.
- Graphical APP/VST3 compilation after shared-engine linkage.
- Windows source-built UI launch without access violation.
- Physical macOS launch.
- Steinberg Validator.
- Real Ableton scan/load/MIDI/save/reopen on Windows and macOS.

## Not yet implemented

- `.aeylashow` project lifecycle.
- Modular production editor interface.
- Art-Net backend integration.
- USB-DMX.
- Installer/signing/notarization.
- Physical fixture output and soak testing.

Do not distribute this branch as a show build.
