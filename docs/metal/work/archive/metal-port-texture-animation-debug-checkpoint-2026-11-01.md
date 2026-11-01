# Metal-port execution archive — Texture Anim

**Date:** 2026-11-01  
**Code commit:** `5001b10d20` (`feat(metal): replay texture animation debug overlay`)

## Implemented boundary

1. Audited the active GL owner and corrected the working assumption: a non-null
   texture matrix selects the draw info, but `gDebugProgram` consumes positions
   only and does not skin draw infos carrying avatar metadata.
2. Added a bounded scene record over existing Metal position/index buffers,
   byte ranges, index type, transform and source traversal order; no CPU
   geometry copy or texture dependency was introduced.
3. Added LDR/HDR native position-only pipelines with exact translucent-yellow
   source color, source-alpha RGB blending, RGB-only writes, disabled culling
   and forward/reverse read/write depth.
4. Merged Texture Anim with Points and Agent Target by the existing source order
   and rejected duplicate/non-monotonic order.
5. Retained HUD output through the uniform-color capture transaction and left
   the original GL draw intact during migration.
6. Added fail-closed validation for storage, triangle cardinality, offsets,
   ranges, transforms, state and allocation.
7. Added physical tests for exact color/alpha, reverse winding, forward and
   reverse depth, ordered interaction, invalid cases and no-owner identity, and
   added both MSL entry points to the offline production-function inventory.
8. Removed only `RENDER_DEBUG_TEXTURE_ANIM` from the explicit presentation veto;
   the other 14 visual modes and latent pathfinding guard remain fail-closed.

## Evidence

- Focused physical RelWithDebInfo world case: 2,214 assertions, pass under
  Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release Metal suites: 45,872 assertions in
  52 cases each, pass under the same validators; 5.99/8.05/7.84 seconds.
- Warnings-as-errors RelWithDebInfo production integration: pass.
- Neutral non-manual/non-GL gate: 484/484 in 35.43 seconds.
- Source-exact Release Development `26.4.0d68061`: pass at `5001b10d20`.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `eac2133bf873934f01c63753864d477804b3f0b838ef6e2960ab31c260e68a1c`.
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

Port `RENDER_DEBUG_UPDATE_TYPE` as the next source-backed flat overlay. Prove
its five fixed update classifications and face-buffer/state/order/depth
contract before removing only that eligibility bit.
