# VST3 and Ableton research notes — 2026-08-05

Purpose: preserve the official-source findings that drive the plugin architecture. Future agents must verify sources again before changing version-dependent claims.

## Ableton findings

### Supported formats and architecture

- Ableton Live supports VST3 on Windows and macOS.
- Windows Live supports only 64-bit plugins from Live 10 onward.
- On Apple Silicon, Universal Live 11.1 and later recognizes VST2/VST3 plugins only when they contain native Apple Silicon code; an Intel-only VST3 requires running Live through Rosetta and is not acceptable for the primary product path.
- Live 12 currently supports Windows 10 22H2/Windows 11 and macOS 11.7.10 or later, with Intel and Apple Silicon on macOS.

Architecture consequence:

- Windows target is x86-64.
- macOS target is Universal (`arm64 + x86_64`) with native Apple Silicon mandatory.
- Real Ableton testing is required in addition to CI and Validator.

Sources:

- https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows
- https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS
- https://help.ableton.com/hc/en-us/articles/4410323149074-Plug-ins-on-Mac-in-Live-11-1-and-later
- https://help.ableton.com/hc/en-us/articles/115001663530-Live-Minimum-System-Requirements

## VST3 MIDI/event findings

- VST3 does not pass opaque MIDI 1.0 byte streams as VST2 did.
- Note On and Note Off arrive as VST3 events on an event bus.
- MIDI CC-oriented controls should be represented as plugin parameters and optionally mapped through `IMidiMapping`.

Architecture consequence:

- Executor triggers use VST3 Note On/Off events.
- Host-automatable global controls are stable VST3 parameters.
- The adapter converts events into project-owned `HostEvent` records.

Sources:

- https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/About%2BMIDI/Index.html
- https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Communication.html
- https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Parameters%2BAutomation/Index.html

## Processor/controller findings

- VST3 recommends separation of processing component and edit controller.
- A host may instantiate/process the processor without creating the UI controller.
- Direct mutable sharing between processing and UI threads is unsafe; mutexes have unbounded execution time and can glitch/block realtime processing.

Architecture consequence:

- The processor must be fully functional with the UI closed or absent.
- Shared runtime state crosses boundaries through parameters, immutable snapshots, atomics and bounded queues.
- No UI pointer is required by the runtime/output path.

Sources:

- https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/API%2BDocumentation/Index.html
- https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Data%2BExchange/Index.html

## Persistence findings

Official VST3 save sequence:

1. processor/component `getState`;
2. controller `getState`.

Official load sequence:

1. component `setState`;
2. controller `setComponentState` with component state;
3. controller `setState` with controller/UI state.

Architecture consequence:

- Authoritative project/runtime identity belongs to component state.
- UI layout belongs to controller state.
- State parsing is versioned and bounded.
- Output Arm is forced off on restore regardless of saved data.

Sources:

- https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Persistence.html
- https://steinbergmedia.github.io/vst3_dev_portal/pages/Tutorials/Code%2Byour%2Bfirst%2Bplug-in.html

## Process context findings

- Since VST3 3.7, plugins must implement `IProcessContextRequirements` to reliably request the transport fields they need.
- Process context provides playing/cycle flags, project time in samples, tempo, time signature and other fields with validity flags.

Architecture consequence:

- Request only required flags.
- Check validity before reading every optional field.
- Transport stop, seek and loop behavior must be explicitly tested.

Sources:

- https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Change%2BHistory/3.7.0/IProcessContextRequirements.html
- https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html

## Silent output and host scheduling findings

- A plugin that outputs silence must clear its audio buffers and set output silence flags correctly.
- Steinberg notes that generator categorization or infinite tail can be used when a plugin must remain processed, but exact host behavior is host-specific.

Architecture consequence:

- First AEYLA VST3 proof uses an instrument/generator topology with one event input and a stereo silent output.
- It zeroes buffers and sets silence flags on every process call.
- Whether Ableton requires generator category and/or infinite tail must be decided from real host evidence.
- Network output cannot rely on continuous `process()` calls.

Sources:

- https://steinbergmedia.github.io/vst3_dev_portal/pages/Tutorials/How%2Bto%2Buse%2Bthe%2Bsilence%2Bflags.html
- https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Processing.html

## Validation findings

- Steinberg supplies a cross-platform Validator intended for build-server integration.
- Validator verifies VST3 conformance but is not Ableton.

Architecture consequence:

Required sequence:

1. unit/integration tests;
2. Windows/macOS builds;
3. Steinberg Validator;
4. real Ableton scan/load/MIDI/state tests per OS;
5. real output hardware;
6. rehearsal/show soak.

Source:

- https://steinbergmedia.github.io/vst3_dev_portal/pages/What%2Bis%2Bthe%2BVST%2B3%2BSDK/Validator.html

## macOS distribution findings

- Distributed macOS software and downloaded plugins should be signed and notarized.
- Notarization requires Developer ID signing, Hardened Runtime, secure timestamp and current `notarytool` workflow.
- Apple explicitly notes that quarantined plugins on modern macOS may require notarization for smooth loading.

Architecture consequence:

- Standalone app, VST3 and installer/disk image are signed correctly.
- Hardened Runtime starts with no unnecessary entitlement exceptions.
- CI separates unsigned PR builds from protected signed/notarized release jobs.

Sources:

- https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution
- https://developer.apple.com/documentation/security/hardened-runtime
- https://developer.apple.com/documentation/xcode/creating-distribution-signed-code-for-the-mac/

## Framework findings

- iPlug2 targets VST3 and standalone Win32/macOS applications from a C++ framework and uses a permissive license.
- The iPlug2 organization recommends its out-of-source template for new projects and CI.

Architecture consequence:

- iPlug2 is the preferred proof framework, not an irrevocable dependency.
- Pin exact revisions after proof gates pass.
- Shared engine types remain framework-independent.

Sources:

- https://github.com/iPlug2/iPlug2
- https://github.com/iplug2
- https://github.com/iPlug2/iPlug2/wiki/Plugin-Configuration

## Unresolved questions requiring proof

1. Does Ableton keep the silent generator processed without `kInfiniteTail`?
2. Does the iPlug2 VST3 wrapper expose all required event/process-context/state behavior cleanly?
3. Which iPlug2 graphics backend gives stable Windows/macOS UI and acceptable GPU behavior alongside Ableton video?
4. What exact project-locator policy provides the best Set transfer behavior?
5. What happens when Ableton deactivates/bypasses/removes the device while Art-Net is armed?
6. What minimum queue capacity avoids overflow under pathological MIDI clips?
7. Does hot reload remain glitch-free under final video/session load?

These questions are acceptance tests, not assumptions.
