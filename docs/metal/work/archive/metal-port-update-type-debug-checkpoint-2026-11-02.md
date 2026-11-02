# Metal-port execution archive — Update Type

**Date:** 2026-11-02  
**Code commit:** `0ac23c10b1` (`feat(metal): replay update type debug overlay`)

## Implemented boundary

1. Audited the active owner and recorded the five exact mappings: full green,
   terse-improved cyan, compressed cached red, compressed uncached yellow and
   full-cached blue, each at alpha `0.5`.
2. Preserved the source position-only, non-rigged contract over each face's
   existing Metal position/index buffers, byte range and current transform.
3. Added exact-color and storage validation with fail-closed invalid input and
   no CPU geometry copy, texture dependency or speculative skinning.
4. Merged Update Type with Points, Agent Target and Texture Anim by original
   traversal order and rejected duplicate/non-monotonic order.
5. Retained HUD-only output through the uniform-color capture transaction and
   left the original GL draw intact during migration.
6. Reused the validated LDR/HDR flat-overlay pipeline: source-alpha RGB blend,
   RGB-only writes, disabled culling and forward/reverse read/write depth.
7. Added physical tests for every color, alpha preservation, reverse winding,
   forward/reverse depth, cross-owner order, invalid cases and no-owner identity.
8. Removed only `RENDER_DEBUG_UPDATE_TYPE` from the explicit presentation veto;
   the other 13 visual modes and latent pathfinding guard remain fail-closed.

## Evidence

- Focused physical RelWithDebInfo world case: 2,293 assertions, pass under
  Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release Metal suites: 45,951 assertions in
  52 cases each, pass under the same validators; 5.94/6.18/5.94 seconds.
- Warnings-as-errors RelWithDebInfo production integration: pass.
- Neutral non-manual/non-GL gate: 484/484 in 35.57 seconds.
- Source-exact Release Development `26.4.0d68063`: pass at `0ac23c10b1`.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `2cbd61d22685d650f66861d9e2cdefc22d30156bdf2caf7345454f8631525ab6`.
- Source/packaged MSL SHA-256:
  `fc55ef6f5161d51eb6ce8b8ee8bfb6a9316c9752f5262592b8254fc3b06701fa`.
- Metallib SHA-256:
  `4e9ab857f3a312a7ddf8b5fd13982cd4c4037b6525e8631e331edf323a14d7e1`.

## Owner constraints retained

- Exact RLVa parity is M-013 nice-to-have. Exact comparisons require enabling
  RLV in both viewers, saving and restarting both; mandatory work is only the
  minimum needed for Metal/no-GL cutover.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers; off/on is invalid and both-off cannot close parity.
- The executable still links OpenGL, so strict no-GL and product completion are
  explicitly open.

## Next boundary

Port `RENDER_DEBUG_BATCH_SIZE` with its exact debug color, polygon offset and
static/rigged matrix-palette contracts before removing only that eligibility
bit.
