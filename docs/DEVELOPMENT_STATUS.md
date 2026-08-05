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
| Executor/layer runtime | Specified | not implemented |
| Project package loader | Specified and schema scaffolded | not implemented |
| Windows standalone editor | Interaction prototype only | browser prototype; no native build |
| macOS standalone editor | Specified | no native build |
| Windows VST3/Ableton | Specified | no plugin binary or Ableton host test |
| macOS VST3/Ableton | Specified | no plugin binary or Ableton host test |
| Video decoding | Specified | not implemented |
| Hardware validation | Not tested | matrix empty |
| Show validation | Not tested | requires full system |

## Parallel-agent Art-Net report boundary

The parallel agent reports a simulated Art-Net implementation with Linux native tests, MinGW/Wine verification for the Windows path, broadcast, adapter binding and a macOS CI job added. The supplied patch and report are useful evidence, but the code is not yet visible in a repository branch or pull request at the time of this status update. It therefore does not change `main` status.

Even after the patch is published, Wine is not a real Windows host test, macOS CI is not a real Mac/Ableton host test, and no VST3 currently exists.

## Honest test boundary

The current `main` package proves architectural concepts and the semantic DMX compiler. It is not yet safe or complete enough for a live show. The first integrated usable milestone must satisfy all four required execution paths defined in `docs/PRODUCT_SPEC.md` and `docs/TEST_PLAN.md`.
