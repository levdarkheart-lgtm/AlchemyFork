# RLVa minimum-cutover execution — 2026-11-01

**Work items:** M-008 minimum rendering owners; M-013 exact parity deferred.  
**Integrated code:** `9c141e0887`, `79356bfd4f`.

## Executed work

1. Recorded the owner decision that exact parity for every RLVa family is a
   non-release nice-to-have, while minimum native/no-GL support remains
   required. Recorded the enable/save/restart prerequisite for both viewers.
2. Closed the bounded reflection-probe production-call audit without a
   speculative edit because no reachable valid-data divergence was found.
3. Implemented a fail-closed native `@setsphere` post owner for all five
   existing modes, sharing the GLSL policy/tween sample and retained depth.
4. Removed the active-`@setoverlay` native-composition rejection after proving
   its existing `gUIProgram` texture/tint/alpha/sampler draw is already inside
   the fail-closed ordered UI mirror.
5. Audited the remaining renderer-affecting RLVa routes. Forced textures,
   avatar silhouettes and camera restrictions feed shared pre-render state;
   other restrictions are CPU/UI policy and expose no separate Metal gate.
6. Preserved every exact-parity remainder in the dedicated M-013 table rather
   than treating the minimum cutover as strict RLVa completion.

## Executed checks

- `@setsphere`: 44 focused physical assertions; 255 assertions with the
  builtin contract.
- Ordered UI/overlay contract: 455 assertions in each of RelWithDebInfo and
  Debug with Metal API/GPU validation.
- Complete physical Metal suites: 44,947 assertions/51 cases in each build
  mode.
- Neutral CTest gate: 484/484 in 247.94 seconds.
- Source-exact warnings-as-errors Development build 68031: no-work repeat,
  deep strict signing and 349/349 ARM64 bundle verification passed.
- Executable SHA-256:
  `3f481298d69ecd8892d14271af072568996065ab10bd44c7571bf5eacae37efd`.
- Harness: 31 required files and 24 tooling tests passed.

## Priority decision

The minimum RLVa cutover audit is closed. Do not consume release-gate time on
the M-013 exact-parity column unless a new concrete Metal/no-GL blocker is
demonstrated. Resume non-RLVa work at the isolated projector/cascade-shadow
fixture, then transparency/material replay and owner-by-owner GL authority
removal. Strict no-GL, complete feature/stability and qualifying performance
evidence remain open.
