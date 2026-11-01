# Default-probe deferred-publication execution — 2026-11-01

**Work item:** M-008, still `doing`.  
**Integrated commit:** `5f64b5ac2b47b4bf4ffbe0cc7c70d8118db1c6ef`.  
**Touched production owner:** `indra/newview/llreflectionmapmanager.cpp`.

## Executed change

1. Traced native probe radiance and packed-SH publication from capture through
   the ordinary probe array and separate default/material-preview mirror.
2. Demonstrated a one-pass readiness assumption: cube radiance could be
   encoded while the matching CPU-visible SH row remained pending.
3. Added one retained private default cube with explicit generation/sequence
   identity. Promotion occurs only after matching SH completion; all stale,
   errored, recycled, device-changed and layout-changed states discard it.
4. Kept the last coherent environment on later failure, suppressed redundant
   same-pass GL/PBO default mirroring after a coherent native pair, and left GL
   traversal/normal authority unchanged.
5. Added a one-time diagnostic marker for actual delayed promotion. It did not
   fire naturally, so no direct rare-branch execution is claimed.

## Executed checks

- Warnings-as-errors Debug and RelWithDebInfo object compiles: pass.
- Final-source build 68023: pass; deep strict signing and 349/349 ARM64 pass.
- Committed-head package 68024: exact SHA-256
  `fa718e362311afa7c74bff8cd641620e7a6b2b644f0465d3fee1fea7fa0e5fd3`;
  deep strict signing and 349/349 ARM64 pass.
- Physical RelWithDebInfo Metal validation: 44,882 assertions, 50 cases, pass.
- Architecture-neutral gate: 484/484 in 259.50 seconds, pass.
- Exact-path build-68024 Computer Use: preflight, semantic login,
  `STATE_STARTED`, coherent world/UI observation, semantic logout and final
  zero process census, pass.
- The malformed positional-argument pattern shown by the owner was not reused;
  the accepted invocation used repeated `--set NAME VALUE` pairs and placed
  the single `--url` last.

## Priority decision

This is a bounded correctness fix on the product path, but continued attempts
to provoke a rare scheduling event would now add activity without stronger
release evidence. Resume the source-led M-008 graph at the next unverified
non-default probe, post or advanced-view owner. Require an asymmetric physical
control for the next demonstrated divergence and rerun focused/full Metal plus
neutral validation. Any future visual comparison remains inadmissible unless
the complete settings/view/environment/window provenance contract matches.
