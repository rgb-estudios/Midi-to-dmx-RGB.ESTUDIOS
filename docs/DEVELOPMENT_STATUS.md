# Development status

Date: 2026-08-05

Canonical repository: `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`

| Component | Status | Evidence |
|---|---|---|
| Repository foundation | Implemented | canonical GitHub repository, CI and governance files |
| Cross-platform product baseline | Specified | Windows/macOS standalone and Windows/macOS Ableton/VST3 required from first integrated milestone |
| Product specification | Specified | `docs/PRODUCT_SPEC.md` |
| Architecture and safety | Specified | architecture/safety docs |
| Semantic attributes | Implemented | C++ core and tests |
| Profile channel reordering | Implemented | `tests/test_main.cpp` |
| RGB→RGBWALUV transform | Implemented (initial algorithm) | core source; calibration pending |
| DMX universe compiler | Implemented | core source and tests |
| ArtDMX packet encoding | Implemented/Simulated | packet tests; no node test yet |
| Art-Net UDP sender on main | Specified | parallel-agent implementation reported externally but not yet present in a repository PR/main |
| USB-DMX backends | Specified | not implemented |
| Host event representation | Implemented | `src/runtime/host_event.h`; trivially copyable fixed-size records |
| Callback-to-runtime SPSC queue | Implemented | `src/runtime/spsc_queue.h`; FIFO/wraparound/concurrent 200,000-event tests |
| Executor/layer runtime | Specified | not implemented |
| Project package loader | Specified and schema scaffolded | not implemented |
| Windows standalone editor | Interaction prototype only | browser prototype; no native build |
| macOS standalone editor | Specified | no native build |
| VST3 platform architecture | Specified | `docs/VST3_PLATFORM_ARCHITECTURE.md` |
| VST3 persistence/lifecycle | Specified | `docs/VST3_STATE_LIFECYCLE_SPEC.md` |
| Windows VST3/Ableton | Specified | no plugin binary or Ableton host test |
| macOS VST3/Ableton | Specified | no plugin binary or Ableton host test |
| Cross-platform release/signing | Specified | `docs/CROSS_PLATFORM_BUILD_RELEASE.md` |
| Ableton host validation | Specified | `docs/ABLETON_HOST_TEST_MATRIX.md` |
| Video decoding | Specified | not implemented |
| Hardware validation | Not tested | matrix empty |
| Show validation | Not tested | requires full system |

## Local validation for host-event foundation

The host event/SPSC queue change was configured and compiled locally with GCC 14.2.0, C++20 and warnings treated as errors. CTest passed `2/2` executables, including FIFO, queue-full, wraparound and a two-thread transfer of 200,000 ordered events. Windows and macOS CI evidence remains required before merge.

## Parallel-agent Art-Net report boundary

The parallel agent reports a simulated Art-Net implementation with Linux native tests, MinGW/Wine verification for the Windows path, broadcast, adapter binding and a macOS CI job added. The supplied patch and report are useful evidence, but the code is not yet visible in a repository branch or pull request at the time of this status update. It therefore does not change `main` status.

Even after the patch is published, Wine is not a real Windows host test, macOS CI is not a real Mac/Ableton host test, and no VST3 currently exists.

## Honest test boundary

The current `main` package proves architectural concepts and the semantic DMX compiler. The `agent/vst3-platform-foundation` branch additionally proves a bounded cross-thread host-event primitive. It is not yet safe or complete enough for a live show. The first integrated usable milestone must satisfy all four required execution paths defined in `docs/PRODUCT_SPEC.md` and `docs/TEST_PLAN.md`.
