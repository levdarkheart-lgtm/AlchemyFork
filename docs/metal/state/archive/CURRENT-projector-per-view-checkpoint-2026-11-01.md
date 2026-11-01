# Projector per-view refresh checkpoint — 2026-11-01

**Observed host:** physical Apple M1 Max.
**Branch:** `codex/apple-silicon-metal`
**Code checkpoint:** `d2a863186a` (`fix(metal): refresh projector shadows per view`).

## Reachable divergence and repair

`fillMetalWorldProjectorShadows()` previously cached both raw spot depth and
derived projector constants by `gFrameCount`. Hero/reflection cube updates can
render multiple camera faces in that frame with the same probe capture owner.
`generateSunShadow()` intentionally retains the raw spot-depth maps during a
cube snapshot, but recomputes `mSunShadowMatrix[4..5]` from every face's current
eye-space transform. Face 2 and later could therefore sample valid retained
depth with face 1's stale projector matrices.

Commit `d2a863186a` keeps the depth reuse but routes the cached texture through
the existing publication lambda on every view. That refreshes matrices, clip
planes, resolution, bias/filter, reverse-Z and unbuilt state for the current
pass. Main and probe capture owners remain isolated; no GL authority or
fail-closed boundary was weakened.

The physical regression uses one asymmetric 32x32 raw depth slice with a
shadowed left half and lit right half. Two consecutive renders change only the
projector matrix X translation (0.25 then 0.75). The first is black/shadowed;
the second matches the lit reference, proving that same-frame retained depth
does not retain the prior view's matrix.

## Validation evidence

- Warnings-as-errors `pipeline.cpp` and `almetal_gpu_test.mm` object builds
  passed in RelWithDebInfo and Debug.
- The focused production projector case passed 1,918 assertions in each build
  mode under Metal API/GPU/load-store/shader validation.
- Complete physical Metal suites passed 44,963 assertions across 51 cases in
  both RelWithDebInfo and Debug under the same validation layers.
- The neutral CTest gate passed 484/484 in 242.94 seconds.
- Release Development build `26.4.0d68033` succeeded at committed source
  `d2a863186a`. A repeat emitted no `CompileC` work; always-run packaging
  relinked, staged and re-signed successfully.
- `tools/metal/verify_app.sh` passed all 349 ARM64 Mach-O slices; the main
  executable is arm64, and deep strict signing passed.
- Executable SHA-256:
  `a2bb8510781871bb7598b992ea4b1814424f33f4efdf864ae561270d9c501785`.
  Packaged MSL SHA-256:
  `06454d17c307c6e1b9493a15edbf16e0c59bc385707f59bac327f5b4f71f006c`.
  Compiled metallib SHA-256:
  `53ac4db074d648096a2a5bb578a30b8b5406579e8bc2c4369e0c1470948e1013`.
- Harness validation and all tooling tests passed after the documentation
  checkpoint.

## Boundary and next work

This is a concrete non-RLVa M-008 correctness repair. It does not verify the
complete shadows feature row or close broader probe, material, stability,
strict no-GL or performance gates. Exact RLVa parity remains the owner-deferred
M-013 nice-to-have with its enable/save/restart comparison prerequisite.

Next release work is source-led transparency/material replay: implement only a
demonstrated reachable divergence, add asymmetric physical coverage, then
continue owner-by-owner native-authority removal.
