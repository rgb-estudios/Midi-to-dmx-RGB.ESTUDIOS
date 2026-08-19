# AEYLA Visual DMX — Alpha 0.3 development changelog

## 0.3.0-a1 — shared product spine (in progress)

### Added

- Shared `aeyla::product::ApplicationModel` for standalone and VST3 adapters.
- Fourteen logical fixture snapshots with Rig 10/14 activation.
- Reference 10-channel RGBWALUV fixture profile.
- Deterministic visual-source sampling for Solid, Gradient, Wave, Noise and Chase.
- MIDI executors on notes 36–43 with Note On, Note Off and velocity.
- Semantic fixture frames compiled into one 512-byte DMX universe.
- End-to-end blackout override in the shared model.
- Backend/project arm gates through `RuntimeSafetyState`.
- Unit coverage for startup safety, rig parity, MIDI execution, deterministic DMX and backend loss.
- Product audit and four-platform acceptance matrix.

### Changed

- Graphical APP and VST3 now compile the shared core/product sources.
- The VST3 callback submits compact host events to a bounded SPSC ingress.
- Model and DMX work runs from the non-realtime idle path.
- The diagnostic/null backend no longer reports itself ready and cannot arm real output.

### Still excluded

- `.aeylashow` load/save.
- Art-Net transmission.
- USB-DMX.
- Ableton host validation.
- Physical Mac launch validation.
- Installer/signing/notarization.
- Final modular editor interface.

This remains a development build and is not show-ready.
