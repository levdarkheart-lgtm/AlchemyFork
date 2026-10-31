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

## 2026-10-31 reconstruction and authenticated ordinary-color comparison

The installed Alchemy Beta application was not used because its executable is x86_64-only. The exact base SHA above was checked out in the isolated external worktree `/tmp/alchemy-reference-1eb44bb`, its submodules were initialized and the ARM64 Release app was rebuilt. Current ld rejected the obsolete generated `-duplicate_symbols` option; removing only that option from generated Ninja link lines allowed the otherwise source-exact build to complete. The exact base remained 26.4.0.67604. Its full pre-instrumentation executable hash was not retained before the later instrumented rebuild replaced it; the exact hash below therefore identifies the artifact actually used for comparison.

The 2026-10-24 source predates isolated macOS profile routing. Its first no-authentication probe wrote ordinary profile state before it was stopped. The external comparison worktree therefore carries three explicitly non-rendering validation commits: `24106a7cac` backports `ALCHEMYNEXT_USER_DIR`, `26aa50e5e4` adds the protected development-login bridge and `8a68422d10` exposes actionable LLUI controls to Computer Use. None belongs to the Metal integration branch. The fully instrumented reference is version 26.4.0.67607 with executable SHA-256 `fac5225435df2c096d10b856f6e0f186b37e41446576398ee8f4842b884ef28d`; deep strict code-sign verification and the 349/349 ARM64 bundle scan pass.

Actual Computer Use under isolated profile `/tmp/alchemy-reference-matrix-1eb44bb` completed protected login, reached `STATE_STARTED`, opened Snapshot, selected Disk with Colors, PNG, 2048x1476, Interface/HUD off and No Filter, observed `Saved to Computer!`, then logged out cleanly through semantic Quit and `Goodbye!`. The local untracked reference image is `/private/tmp/alchemy-reference-1eb44bb-color_2026-10-31_17032401.png`, SHA-256 `fa8b710b4a44b5b97a6974a07292d05a8f889601ae32cb7bf028f078c7d2e572`. Account-bearing profile and log data remain unversioned.

The corresponding Metal build 67957 image is `/private/tmp/alchemy-metal-67957-color_2026-10-31_17265201.png`, SHA-256 `15dbc6ce42c830bac09aa22a08755ec9a6e24effe83bd73f1e2074bc3aeb7772`. It used the same snapshot settings and camera composition and logged explicit native 2048x1476 color readback. Visual inspection shows matched upright geometry, texture placement, avatar, alpha foliage and output orientation without channel swap or ghosting. SSIM 0.769382 and PSNR 18.361069 dB are descriptive only because the approximately 23-minute separation changed sky lighting and dynamic media-board content. This validates one ordinary-color snapshot path, not the remaining feature matrix or performance.

The same instrumented reference subsequently saved scalar depth as `/private/tmp/alchemy-reference-1eb44bb-depth_2026-10-31_17314401.png` (`8a58cb042a62d9fd58fa25438deebc066a9cfc27ce0a40b305cd9f3d7f5dc4b7`) and packed depth24 as `/private/tmp/alchemy-reference-1eb44bb-depth24_2026-10-31_17323901.png` (`e1337955cc64ff22a2f668a44f46f461f567f3b6e106c673f4789ef70683c715`). Current Metal build 67957 repeated both semantic modes within minutes and saved `/private/tmp/alchemy-metal-67957-depth_2026-10-31_17360101.png` (`eeb5b7e3825f9ad743a29314ddd17db1739baa44910e3275564ddc8723d11de6`) and `/private/tmp/alchemy-metal-67957-depth24_2026-10-31_17371901.png` (`ee4d65e2f6eb91389738362d16bd216609b5abd4ddf084107e3449871b2aeb51`). All are untracked 2048x1476 PNGs. The Metal log records native 2048x1476 depth readback while each mode was active.

Visual inspection finds matched upright geometry, near/far ordering and packed RGB byte structure. Scalar depth SSIM is 0.903758 with PSNR 22.970676 dB; 79.203% of gray samples are identical and the median absolute error is zero. Decoding each packed RGB triplet as a single 24-bit value gives median normalized absolute error 0.000436, p95 0.015563, mean 0.011376 and Pearson correlation 0.926407. Small avatar/scene pose changes remain because the viewers cannot be connected simultaneously with the same account. This validates one ordinary scalar/packed depth view, not tiled/supersampled, cutout, 360-degree or the broader feature matrix.

## 2026-10-31 authenticated no-post comparison

Actual Computer Use subsequently repeated the reference/candidate flow for Colors with No post-processing enabled. Both sequential sessions used Disk, PNG, 2048x1476, Interface/HUD off and No Filter; each visibly refreshed the clean-plate preview, saved, and exited through the semantic Quit route with `Goodbye!` and exit 0. The reused file picker retained an older filename stem, so the session logs, active semantic controls, timestamps and hashes identify the artifacts: reference `/private/tmp/alchemy-metal-67956-depth24_2026-10-31_17473001.png` (`6eb13d09ed51b1d9866fe6e8153f81d5405ed55ffe34124fcec59b1edd2fd2e5`) and Metal 67959 `/private/tmp/alchemy-metal-67956-depth24_2026-10-31_17495401.png` (`dd6b6b5d94f2a8eb7fee0bb65afec2461ddeea590b12ab8dac8fbe7268c34229`). Both are untracked 2048x1476 RGB PNGs.

The Metal log records native full-size and preview color readback while the no-post control was active. Side-by-side inspection shows the same upright scene, display transfer and suppression of look effects without a channel, gamma, tonemap or orientation mismatch. Descriptive SSIM is 0.874056 and PSNR is 26.924431 dB; sequential-login avatar pose, dynamic media and streamed scene changes account for localized residuals. This validates ordinary one-view clean-plate color capture, not tiled/supersampled, cutout, 360-degree or the broader feature matrix.
