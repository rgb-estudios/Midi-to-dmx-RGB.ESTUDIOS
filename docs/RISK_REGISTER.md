# Risk register

| ID | Risk | Severity | Mitigation |
|---|---|---:|---|
| R1 | VST3 host thread blocked by media/network | Critical | strict thread separation and stress tests |
| R2 | Unsafe reset/macro mapping | Critical | protected attributes, ranges, confirmation and safe homes |
| R3 | Fixture swap changes look | High | semantic profiles and golden DMX tests |
| R4 | Project/media paths break on transfer | High | packaged relative paths and checksums |
| R5 | Standalone/VST3 output differs | High | shared engine and byte-identical integration tests |
| R6 | Cheap USB interface timing instability | High | Art-Net priority; named hardware soak tests |
| R7 | Visual editor becomes unusable/cluttered | High | enforce visual design system and screenshot review |
| R8 | Video decode overload alongside Ableton projection | High | low-resolution source, buffered decode, performance budget |
| R9 | Hot reload emits transient values | Critical | compile off-line, validate, atomic state swap at frame boundary |
| R10 | Scope expands into full console/video server | High | non-goals and ADR approval process |
