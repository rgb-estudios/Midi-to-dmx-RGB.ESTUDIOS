# AEYLA R10.14 — DAW Session Recovery

R10.13 field-stable baseline plus DAW Save/Save As crash-recovery state.

Contract:
- Restore saved take-library locator and exact per-song take bindings.
- Restore blackout latch exactly as saved in the DAW session.
- If saved with physical Take ARM, re-arm only after project, library, take, universe, Art-Net backend, and host heartbeat validate.
- Never auto-start artistic playback on session open; reopen ready for the next PLAY/song trigger.
- Portable `.aeylashow` files remain physically disarmed/fail-safe across machines.
- MIDI preload remains an optional cache, never an ARM/PLAY prerequisite.
