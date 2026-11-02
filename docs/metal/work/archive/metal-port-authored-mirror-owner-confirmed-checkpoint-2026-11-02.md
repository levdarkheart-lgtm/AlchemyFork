# Metal-port checkpoint — owner-confirmed authored mirror

**Date:** 2026-11-02  
**Repository observation HEAD:** `3cd2aa0d977edf064418b6e8141862a7a10be69e`  
**Renderer binary source:** `ae2b43644211a73802d4197efa39a361ad5614bb`

## Completed work

1. Replaced the rejected authored-mirror screenshots with a fresh sequential
   reference/candidate pair from the owner-controlled private dry fixture.
2. Pinned reference `67607` and candidate `68099` by exact executable paths and
   SHA-256, used identical merged Graphics/Looks session files, and observed
   identical seated coordinates plus 12 matching common live controls.
3. Corrected local QA profile installation: Graphics `Codex Metal Mirror Q`
   and Lightbox `CodexMetalMirrorQ` now exist under the active viewer profile
   with their original hashes and remain distinct presets.
4. Proved the candidate save used the scoped native authored-mirror diagnostic
   and native 2048x1476 readback; both sessions then shut down normally.
5. Presented the exact latest files to the owner. The owner explicitly accepted
   their visual equivalence and directed the engineering work to move on.

## Decision

Accept this one controlled authored-mirror sample. Reject every earlier image
that used GL fallback, lacked the named Lightbox profile, contained different
mirror content or was otherwise not the final owner-reviewed pair. Do not use
unregistered SSIM from the sequential live images as a release threshold.

This is not a broad mirror/probe completion claim. Keep normal live authority
fail-closed pending its separate integration decision, and continue with the
next source-evidenced release blocker: legacy alpha-pool BLEND/stained glass,
then strict owned-OpenGL inventory and the remaining validation matrix.

Exact hashes, live settings, logs, metrics and shutdown evidence are in
`docs/metal/state/archive/CURRENT-authored-mirror-owner-confirmed-checkpoint-2026-11-02.md`.
