# Third-party dependencies and candidates

AEYLA pins every fetched dependency to a full commit. No release build may fetch a floating branch.

| Dependency | Purpose | Licence | Revision | Status / policy |
|---|---|---|---|---|
| miniz | ZIP read/write/validation for `.aeylashow` packages | Unlicense / public-domain dedication in upstream source | release `3.1.2`, commit `77d0dce8627735138c51770d1799a1ef48f2117d` | **Approved for the bounded package slice.** Fetched as source, built statically, `MINIZ_NO_TIME` enabled for deterministic archives. AEYLA rejects ZIP64, encryption, unsupported methods, unexpected/duplicate entries, oversized archives and non-root `project.json`. Rollback boundary: remove `project_package.*` and the `aeyla_miniz` target without changing the document/runtime model. |
| iPlug2 | VST3 and standalone application framework/UI | zlib-like permissive | `584df5a3306f3a9a62b5ebb803d3fb58134abdcf` | Proof dependency; preserve licence/notices; production approval still requires Validator and real Ableton evidence on Windows/macOS. |
| Steinberg VST3 SDK | VST3 API, helper classes and Validator | MIT | tag `v3.8.0_build_66`, commit `9fad9770f2ae8542ab1a548a68c1ad1ac690abe0` | Proof dependency; preserve MIT notice and follow VST trademark guidelines if branding is used. |
| Inno Setup | Compile the internal unsigned Windows VST3 installer | Inno Setup License (commercial use permitted; upstream requests commercial users purchase a licence) | `6.7.1` | Pinned approved Chocolatey build tool only; no Inno compiler binaries are redistributed inside AEYLA. Windows public releases still require Authenticode signing. Rollback boundary: retain the portable VST3 pack and remove the `.iss`/installer job without changing AEYLA runtime code. |
| Media Foundation | Windows video decode | Windows SDK / OS component | not selected | Windows-only media adapter; shared media API remains platform-neutral. |
| macOS AVFoundation or approved decoder | macOS video decode | Apple SDK / selected library terms | not selected | Must provide deterministic parity with Windows media behavior. |
| JSON library | project/profile manifests | none | none | No external JSON dependency is used in Alpha 0.3. The repository contains a bounded project-specific parser with size/depth/node limits and malformed-input tests. |
| FTDI D2XX or supported serial API | Open DMX transport | Vendor terms | not selected | Optional named USB backend only. |

## miniz integration boundary

- Upstream source: `richgel999/miniz` release 3.1.2.
- Full commit pin: `77d0dce8627735138c51770d1799a1ef48f2117d`.
- Source retrieval: CMake `FetchContent` Git checkout; no floating tag or branch.
- Build: static `aeyla_miniz` target from `miniz.c`, `miniz_zip.c`, `miniz_tinfl.c` and `miniz_tdef.c`.
- Platforms under CI: Linux, Windows and macOS.
- Local patches: none; AEYLA supplies only the generated `miniz_export.h` expected by upstream CMake builds.
- Current use: one deterministic root `project.json` entry only.
- Deliberately blocked: assets/media until bounded streaming plus SHA-256 verification are implemented.

Before adding or promoting another dependency, document:

- exact tag and full commit;
- source/archive verification method;
- licence and notices;
- binary-distribution implications;
- supported platforms/toolchains;
- local patches;
- update and rollback strategy;
- fallback/replacement boundary;
- automated and real-host validation evidence.
