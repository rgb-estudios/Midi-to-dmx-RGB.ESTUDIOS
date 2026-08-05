# Third-party dependency plan

No third-party source code is vendored in the current foundation or PR #9.

Candidate revisions are recorded for proof only in `dependencies/proof-candidates.json`. They are not approved production dependencies until Issue #10 passes Windows/macOS build, Validator and real Ableton gates.

| Dependency | Purpose | Licence | Candidate | Policy |
|---|---|---|---|---|
| iPlug2 | VST3 and standalone application framework/UI | zlib-like permissive | `584df5a3306f3a9a62b5ebb803d3fb58134abdcf` | Proof candidate; preserve license/notices; pin only after four-target host proof |
| Steinberg VST3 SDK | VST3 API, helper classes and Validator | MIT | tag `v3.8.0_build_66`, commit `9fad9770f2ae8542ab1a548a68c1ad1ac690abe0` | Proof candidate; preserve MIT notice and follow VST trademark guidelines if branding is used |
| Media Foundation | Windows video decode | Windows SDK / OS component | not selected | Windows-only media adapter; shared media API remains platform-neutral |
| macOS AVFoundation or approved decoder | macOS video decode | Apple SDK / selected library terms | not selected | Must provide deterministic parity with Windows media behavior |
| JSON library | project/profile manifests | not selected | not selected | Exact version/license and fuzz history required |
| ZIP/package library | `.aeylashow` packages | not selected | not selected | Exact version/license, path traversal protection and size limits required |
| FTDI D2XX or supported serial API | Open DMX transport | Vendor terms | not selected | Optional named USB backend only |

Before adding a dependency, document:

- exact tag and full commit;
- source/archive checksum;
- license and notices;
- binary-distribution implications;
- supported platforms/toolchains;
- local patches;
- update and rollback strategy;
- fallback/replacement boundary;
- automated and real-host validation evidence.

No dependency may be fetched from a floating branch in a release build.
