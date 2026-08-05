# Cross-platform build, installation, signing and release

## 1. Product baseline

Initial support baseline follows Ableton Live 12 requirements:

### Windows

- Windows 10 22H2 and Windows 11 22H2 or later.
- x86-64 native build.
- MSVC 2022 release toolchain.
- AVX2 may be assumed only if the product explicitly targets Live 12; shared libraries should avoid unnecessary CPU-specific compilation until benchmarked.

### macOS

- macOS 11.7.10 or later.
- Universal binary containing `arm64` and `x86_64`.
- Xcode/Clang toolchain pinned by the release workflow.
- Native Apple Silicon VST3 is mandatory; Rosetta-only builds are rejected.

Supporting Live 11 can be added as a separate compatibility matrix. It must not silently lower or change the Live 12 baseline.

## 2. Produced artifacts

Every tagged alpha/release build produces:

### Windows

- `AEYLA Visual DMX Editor.exe`.
- `AEYLA Visual DMX.vst3` bundle.
- installer `.exe` or `.msi`.
- portable editor ZIP for diagnostics.
- debug symbols stored as restricted CI artifacts.
- checksums and build manifest.

### macOS

- `AEYLA Visual DMX Editor.app` universal bundle.
- `AEYLA Visual DMX.vst3` universal bundle.
- signed/notarized `.pkg` or `.dmg` installer.
- debug symbols stored as restricted CI artifacts.
- checksums and build manifest.

No release artifact may contain source checkout paths, developer certificates, API tokens or private show media.

## 3. Standard VST3 installation paths

### Windows global

```text
C:\Program Files\Common Files\VST3\AEYLA Visual DMX.vst3
```

### macOS global

```text
/Library/Audio/Plug-Ins/VST3/AEYLA Visual DMX.vst3
```

A per-user development install may be supported, but production installers default to the standard global VST3 location so Ableton can scan predictably.

## 4. Dependency pinning

Dependencies are acquired by exact commit/tag and verified checksum.

Required manifest fields:

- dependency name;
- repository/source;
- exact revision;
- license;
- local patches;
- target platforms;
- update owner;
- last compatibility test.

The first framework proof must pin:

- iPlug2;
- VST3 SDK;
- UI/graphics backend chosen through iPlug2;
- JSON and ZIP libraries used by `.aeylashow`;
- media decoder libraries when introduced;
- installer tool versions.

No release downloads dependencies from a floating branch.

## 5. Windows build and package

Required CI steps:

1. Checkout with pinned dependencies.
2. Configure CMake for x64 Release.
3. Build shared libraries, standalone and VST3.
4. Run unit/integration tests.
5. Run Steinberg Validator.
6. Install into a temporary VST3 directory and launch the test host.
7. Build installer and portable ZIP.
8. Install silently into a clean Windows runner where possible.
9. Verify files, uninstall and verify cleanup.
10. Publish checksums, manifest and symbols.

### Windows signing

Unsigned builds are acceptable only for internal alpha and must be labeled. Public release requires Authenticode signing of executable, installer and relevant binaries. Signing credentials stay in protected CI secrets; forks and pull requests never receive them.

The installer must:

- display product/version/channel;
- install the editor and VST3 separately selectable where possible;
- preserve user project files during update/uninstall;
- support repair/reinstall;
- log installation failures;
- never install private example media without explicit inclusion.

## 6. macOS build and package

Required CI steps:

1. Configure universal architectures `arm64;x86_64`.
2. Set deployment target to the approved baseline.
3. Build shared libraries, standalone and VST3.
4. Verify both architectures with `lipo`/Mach-O inspection.
5. Run unit/integration tests per architecture where practical.
6. Run Steinberg Validator natively.
7. Sign nested code from the inside out.
8. Enable Hardened Runtime with minimum entitlements.
9. Verify signatures with `codesign` and Gatekeeper assessment.
10. Build signed installer package/disk image.
11. Submit with `notarytool`, wait for acceptance and staple the ticket.
12. Test installation and Ableton scan on a clean real Mac.

### Entitlements

Start with no exceptions. Add an entitlement only when a documented feature requires it and a test proves the need. The standalone and plugin must not request microphone, camera, contacts or other unrelated permissions.

Because the VST3 is loaded in Ableton's process, it must avoid assumptions that only hold in the standalone app. Framework, media and network code must work under the host's runtime environment and threading model.

## 7. CI lanes

### Pull request lane

- Linux core tests.
- Windows core/runtime/build tests.
- macOS core/runtime/build tests.
- static analysis.
- JSON/schema validation.
- sanitizer jobs where supported.
- VST3 Validator when the plugin target exists.
- no signing/notarization secrets.

### Main/nightly lane

- full Windows/macOS artifacts;
- installer smoke tests;
- additional plugin validation as supplementary evidence;
- deterministic DMX golden captures;
- stress and repeated lifecycle tests.

### Release lane

- clean tagged commit;
- signed Windows artifacts;
- signed/notarized macOS artifacts;
- release manifest/SBOM;
- checksums;
- retained symbols;
- manual real Ableton approval on both operating systems.

## 8. Version and identity policy

Use semantic versioning with explicit channel:

```text
0.1.0-alpha.1
0.1.0-alpha.2
0.2.0-beta.1
1.0.0
```

A build manifest contains:

- version;
- git commit;
- dirty flag (must be false for releases);
- dependency revisions;
- compiler/Xcode/MSVC versions;
- target OS/architecture;
- plugin processor/controller UIDs;
- project schema versions;
- build timestamp in UTC;
- signing/notarization status.

Plugin UIDs and stable parameter IDs are never regenerated during ordinary releases.

## 9. Crash diagnostics and privacy

Internal alpha builds may write local diagnostic files outside realtime threads. Diagnostics include:

- build/version;
- lifecycle transitions;
- queue overflow counters;
- project UUID/checksum, not private content;
- backend errors;
- OS/host versions;
- timing statistics.

Do not upload logs automatically in version 1. The user explicitly exports a redacted diagnostic bundle.

## 10. Release gates

No build is called release-ready until:

- Windows and macOS CI green;
- Validator green on both plugin binaries;
- real Ableton host matrix complete on Windows and Apple Silicon macOS;
- installers pass clean install/update/uninstall;
- signed/notarized distribution verified;
- two-hour combined Ableton/video/DMX soak test passes;
- project correction and rollback drill passes;
- hardware matrix contains the actual show node/interface and fixture profiles.

## 11. Primary references

- Ableton Live 12 system requirements: https://help.ableton.com/hc/en-us/articles/115001663530-Live-Minimum-System-Requirements
- VST3 locations: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html
- VST3 format: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BFormat.html
- VST3 Validator: https://steinbergmedia.github.io/vst3_dev_portal/pages/What%2Bis%2Bthe%2BVST%2B3%2BSDK/Validator.html
- Apple signing: https://developer.apple.com/documentation/xcode/creating-distribution-signed-code-for-the-mac/
- Apple notarization: https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution
- Apple Hardened Runtime: https://developer.apple.com/documentation/security/hardened-runtime
