# Plugin framework and dependency decision

Decision status: **Proposed and approved for proof-of-concept; not yet pinned for release**.

## 1. Decision

Use **iPlug2** as the preferred framework for the first cross-platform standalone + VST3 proof, with the official Steinberg VST3 SDK underneath. Pin exact revisions only after a minimal proof passes CI, Validator and real Ableton on Windows and macOS.

## 2. Alternatives considered

### Direct Steinberg VST3 SDK + custom standalone/UI

Advantages:

- maximum control over VST3 behavior;
- minimal wrapper abstraction;
- direct access to newest SDK features.

Costs/risks:

- separate standalone platform layer and UI infrastructure;
- more Windows/macOS lifecycle and graphics code owned by this small project;
- increased maintenance and packaging burden;
- slower delivery of a visually rich editor.

Use direct SDK code only where iPlug2 does not expose required VST3 behavior safely.

### JUCE

Advantages:

- mature cross-platform plugin and standalone ecosystem;
- extensive audio/MIDI/UI/device support;
- large community and validation tooling.

Costs/risks:

- licensing/commercial obligations require separate legal and budget review;
- heavier framework footprint than required for a MIDI-to-DMX runtime;
- risk of adopting broad audio abstractions not needed by the product.

JUCE remains a fallback if the iPlug2 proof fails Ableton, UI or release requirements.

### iPlug2

Advantages:

- VST3 and standalone targets from a shared C++ project;
- Windows/macOS support;
- permissive license;
- suitable graphics/UI layer for the visual editor;
- lower licensing friction for RGB Estudios;
- framework organization recommends iPlug2OOS for out-of-source projects and CI setup.

Risks:

- smaller ecosystem than JUCE;
- fewer third-party examples for non-audio network-controlled instruments;
- documentation and APIs may require direct source inspection;
- certain host edge cases may require dropping below wrappers to VST3 interfaces.

## 3. Proof-of-concept gates

The framework is formally pinned only after one branch produces:

- Windows standalone shell;
- macOS universal standalone shell;
- Windows VST3 silent instrument;
- macOS universal VST3 silent instrument;
- note-on/off/velocity into the shared event queue;
- silent stereo output with correct silence flags;
- state save/reopen;
- Steinberg Validator success;
- Ableton scan/load on real Windows;
- Ableton native scan/load on real Apple Silicon macOS;
- no framework code copied into `src/core`, `src/runtime` or `src/io`.

## 4. Pinning rules

When accepted, record:

- exact iPlug2 commit SHA;
- exact VST3 SDK version/commit;
- exact graphics backend and compile flags;
- local patch files, if any;
- supported MSVC/Xcode/CMake versions;
- minimum Windows/macOS deployment targets;
- license text and notices;
- upgrade test procedure.

Dependency updates occur in isolated PRs with full Windows/macOS/Ableton regression testing. No opportunistic framework upgrade is allowed during a show-critical fix.

## 5. Boundary

Framework types may exist only in adapters and UI modules. The persisted project, semantic engine, executor state, host events and output backends use project-owned standard C++ types.

This allows:

- standalone and VST3 parity;
- framework replacement without rewriting show logic;
- deterministic unit tests without a DAW;
- output/backend testing without the UI;
- independent fuzzing of state and project parsers.

## 6. Primary references

- iPlug2 repository: https://github.com/iPlug2/iPlug2
- iPlug2 organization/out-of-source recommendation: https://github.com/iplug2
- iPlug2 plugin configuration: https://github.com/iPlug2/iPlug2/wiki/Plugin-Configuration
- Steinberg VST3 SDK: https://github.com/steinbergmedia/vst3sdk
- VST3 developer portal: https://steinbergmedia.github.io/vst3_dev_portal/
