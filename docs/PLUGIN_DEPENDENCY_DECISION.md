# Plugin framework and dependency decision

Decision status: **Proposed and approved for proof-of-concept; exact candidates recorded but not approved for release**.

## 1. Decision

Use **iPlug2** as the preferred framework for the first cross-platform standalone + VST3 proof, with the official Steinberg VST3 SDK underneath. Exact candidate revisions are recorded in `dependencies/proof-candidates.json`. They become approved pins only after the minimal proof passes CI, Validator and real Ableton on Windows and macOS.

## 2. Researched proof candidates — 2026-08-05

### iPlug2

Candidate revision:

```text
584df5a3306f3a9a62b5ebb803d3fb58134abdcf
```

Date: 2026-06-12.

Reasoning:

- The repository's latest visible tagged release is `v1.0.0-beta` from 2024.
- Its own release notes say source-code release downloads are out of date and recommend cloning the repository.
- The selected commit is the current reviewed master head found during research and includes current CMake/out-of-source development.
- Because it is a moving-framework development commit rather than a stable final release, it is suitable only as a proof candidate until the four product targets pass.

License: permissive zlib-like license, suitable for closed-source use subject to preserving notices.

### Steinberg VST3 SDK

Candidate tag and revision:

```text
v3.8.0_build_66
9fad9770f2ae8542ab1a548a68c1ad1ac690abe0
```

Release date: 2025-10-20.

Reasoning:

- It is the latest official tagged SDK located during the research.
- The official repository identifies the SDK as VST SDK 3.8.x.
- Its documented matrix covers MSVC 2022, macOS x86_64/Apple Silicon and Xcode 10–16.
- It includes the official Validator and is MIT licensed.

These revisions are **not yet vendored, fetched or approved** by this PR. Issue #10 performs that proof in an isolated branch.

## 3. Alternatives considered

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
- selected proof candidate is a development commit, so regression proof is mandatory;
- certain host edge cases may require dropping below wrappers to VST3 interfaces.

## 4. Proof-of-concept gates

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

A framework proof may reject one or both candidates. Rejection must record the exact build/host failure and compare the next candidate under the same tests.

## 5. Pinning rules

When accepted, record:

- exact iPlug2 commit SHA;
- exact VST3 SDK version/commit;
- archive/submodule checksum and retrieval method;
- exact graphics backend and compile flags;
- local patch files, if any;
- supported MSVC/Xcode/CMake versions;
- minimum Windows/macOS deployment targets;
- license text and notices;
- upgrade test procedure.

Dependency updates occur in isolated PRs with full Windows/macOS/Ableton regression testing. No opportunistic framework upgrade is allowed during a show-critical fix.

## 6. Boundary

Framework types may exist only in adapters and UI modules. The persisted project, semantic engine, executor state, host events and output backends use project-owned standard C++ types.

This allows:

- standalone and VST3 parity;
- framework replacement without rewriting show logic;
- deterministic unit tests without a DAW;
- output/backend testing without the UI;
- independent fuzzing of state and project parsers.

## 7. Primary references

- iPlug2 repository: https://github.com/iPlug2/iPlug2
- iPlug2 commit candidate: https://github.com/iPlug2/iPlug2/commit/584df5a3306f3a9a62b5ebb803d3fb58134abdcf
- iPlug2 tags/release note: https://github.com/iPlug2/iPlug2/tags
- iPlug2 organization/out-of-source recommendation: https://github.com/iplug2
- iPlug2 plugin configuration: https://github.com/iPlug2/iPlug2/wiki/Plugin-Configuration
- Steinberg VST3 SDK tag: https://github.com/steinbergmedia/vst3sdk/releases/tag/v3.8.0_build_66
- Steinberg VST3 SDK repository: https://github.com/steinbergmedia/vst3sdk
- VST3 developer portal: https://steinbergmedia.github.io/vst3_dev_portal/
