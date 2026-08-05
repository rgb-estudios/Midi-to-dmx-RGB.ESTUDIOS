# Development status

Date: 2026-08-05

Canonical repository: `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`

| Component | Status | Evidence |
|---|---|---|
| Repository foundation | Implemented | canonical GitHub repository, CI and governance files |
| Product specification | Specified | `docs/PRODUCT_SPEC.md` |
| Architecture and safety | Specified | architecture/safety docs |
| Semantic attributes | Implemented | C++ core and tests |
| Profile channel reordering | Implemented | `tests/test_main.cpp` |
| RGB→RGBWALUV transform | Implemented (initial algorithm) | core source; calibration pending |
| DMX universe compiler | Implemented | core source and tests |
| ArtDMX packet encoding | Implemented/Simulated | packet tests; no node test yet |
| Art-Net UDP sender | Specified | not implemented |
| USB-DMX backends | Specified | not implemented |
| Executor/layer runtime | Specified | not implemented |
| Project package loader | Specified and schema scaffolded | not implemented |
| Standalone editor | Interaction prototype only | browser prototype + screenshot |
| VST3 runtime | Specified | not implemented |
| Video decoding | Specified | not implemented |
| Hardware validation | Not tested | matrix empty |
| Show validation | Not tested | requires full system |

## Honest test boundary

The current package proves architectural concepts and the semantic DMX compiler. It is not yet safe or complete enough for a live show.
