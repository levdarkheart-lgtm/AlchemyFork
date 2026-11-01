# Projector per-view refresh execution — 2026-11-01

**Work item:** M-008 advanced views/effects.
**Integrated code:** `d2a863186a`.

## Executed work

1. Traced production hero/reflection cube capture through the reused probe
   owner, retained spot-depth maps and per-face shadow-matrix generation.
2. Demonstrated that a same-`gFrameCount` cache hit could preserve face 1's
   derived projector constants for later cube faces even though each face has a
   different eye-space transform.
3. Reused only the valid raw depth texture on cache hit and republished all
   current-view projector constants through the established fail-closed path.
4. Added a two-view asymmetric physical control using one retained depth slice
   and matrix translations that select its shadowed and lit halves.
5. Kept exact RLVa parity in M-013; no RLVa release scope was reopened.

## Executed checks

- Focused projector case: 1,918 assertions in RelWithDebInfo and Debug under
  Metal API/GPU/load-store/shader validation.
- Complete physical Metal suites: 44,963 assertions/51 cases in each mode.
- Neutral CTest gate: 484/484 in 242.94 seconds.
- Warnings-as-errors changed-object builds: pass in both modes.
- Release Development `26.4.0d68033`: full build and compile-free repeat pass;
  deep strict signing and 349/349 ARM64 bundle verification pass.
- Executable SHA-256:
  `a2bb8510781871bb7598b992ea4b1814424f33f4efdf864ae561270d9c501785`.
- Harness validation and all tooling tests passed.

## Priority decision

The per-view projector cache divergence is repaired and covered. Continue
non-RLVa M-008/M-007 work at transparency/material replay, edit only a
demonstrated reachable divergence, and preserve fail-closed GL authority until
each owner has correctness and side-effect proof. Full feature/stability,
strict no-GL and qualifying paired performance evidence remain open.
