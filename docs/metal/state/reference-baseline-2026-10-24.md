# ARM64 open-source reference baseline — 2026-10-24

This is the same-source OpenGL reference required by `VALIDATION.md`; it is not evidence that the Metal candidate exists or is complete.

## Provenance

- Source/harness SHA: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` (upstream source parent `65c39b7bad928e165f1acdc0cbf39478e017ef1d`).
- Configure preset: `ninja-os-arm64`, channel `Alchemy Metal Development`, proprietary dependencies off, tests enabled for the final baseline build.
- Configuration: Release; AppleClang 21; Ninja Multi-Config; arm64-osx-alchemy vcpkg triplet.
- App: `build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Development.app`, version 26.4.0.67604.
- Executable SHA-256: `6e40c02653b7b4b74bbf43048a159d4b1e7cde390f9f540448faff845ed8236a`.

## Commands and results

```sh
AL_METAL_PRESET=ninja-os-arm64 tools/metal/build.sh configure
AL_METAL_PRESET=ninja-os-arm64 tools/metal/build.sh build
tools/metal/verify_app.sh 'build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Development.app'
codesign --verify --deep --strict --verbose=2 'build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Development.app'
```

- Configure and build exit 0. A repeat build reports `ninja: no work to do`.
- The complete bundle scan checks 349 Mach-O files and finds zero non-arm64 slices.
- Deep ad-hoc signature verification passes.
- `otool -L` confirms the reference viewer links the macOS OpenGL framework; this must disappear from the candidate viewer executable.
- Catch2 registered 565 entries. All 461 enabled architecture-neutral tests pass with `ctest -C Release -LE 'manual|gl'`; 15 Python/LLCoreHTTP tests are disabled by the upstream configuration. The broader non-manual run passes 507 of 510 enabled tests. The three GL-only failures expose Apple OpenGL 4.1 reference constraints: no clip-control, a normalized depth-storage format, and non-portable fence-duration timing assumptions.

The isolated smoke command used `/tmp/alchemy-metal-isolated.6ErMuQ` through `ALCHEMYNEXT_USER_DIR` and `--quitafter 8`. The process wrote its settings, logs, cache, browser profile, and data beneath that root; settings paths contained a single `user_settings` component. Redacted state evidence from the uncommitted local log:

```text
GL: 4.1 Metal - 90.5
Compute unavailable (GL 4.1, needs 4.3)
Initializing Login Screen
STATE_LOGIN_SHOW --> STATE_LOGIN_WAIT
Quitting after 8 seconds
Goodbye!
```

The process exited 0. It did not authenticate, enter a region, or produce performance samples. The raw log and profile remain unversioned because runtime logs can contain user/system data.

## Static renderer census

`python3 tools/metal/audit_gl.py --mode inventory` scanned 2,404 files and reported 3,080 findings in 135 files. Largest files by finding count are `newview/pipeline.cpp` (453), `newview/llviewershadermgr.cpp` (336), `llrender/llglslshader.cpp` (255), `llrender/llimagegl.cpp` (206), and `newview/llviewershadermgr.h` (166). The raw ignored JSON hash is recorded in `CURRENT.md`.

This baseline remains an external comparison executable. The final fork has no GL fallback.
