# Third-party dependency plan

No third-party source code is vendored in the foundation package.

Planned production dependencies:

| Dependency | Purpose | Licence | Policy |
|---|---|---|---|
| iPlug2 | VST3 and standalone application framework | zlib-like permissive | Pin commit; preserve notices |
| Steinberg VST3 SDK | VST3 API/build | MIT | Pin release/commit |
| Media Foundation | Windows video decode | Windows SDK | OS component |
| FTDI D2XX or supported serial API | Open DMX transport | Vendor terms | Optional backend |

Before adding a dependency, document exact version, licence, binary-distribution implications, update strategy and fallback.
