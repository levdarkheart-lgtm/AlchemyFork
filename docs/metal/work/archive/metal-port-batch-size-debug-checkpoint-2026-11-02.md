# Metal-port execution archive — Batch Size

**Date:** 2026-11-02
**Code commit:** `27c0bec87c` (`feat(metal): replay batch size debug overlay`)

## Implemented boundary

1. Audited the active draw-info owner and retained the exact stable hash color
   with alpha byte 200, source-alpha RGB-only blend, disabled culling and
   polygon offset `(factor=-1, units=+1)`.
2. Reused each draw info's existing Metal position/index buffers, index type,
   byte range, current transform and original traversal order without a CPU
   geometry copy.
3. Added static and object-skinned paths. The latter retains `TYPE_WEIGHT4`, the
   source `4 + joint_count*12` float matrix palette and 110-joint ceiling.
4. Added forward/reverse-Z semantic Metal depth bias and explicit static/rigged
   world pipelines with read/write `LEQUAL` depth and no culling.
5. Merged Batch Size with Points, Agent Target, Texture Anim and Update Type by
   source order and rejected duplicate/non-monotonic order.
6. Added explicit untextured static/rigged HUD debug families and reset depth
   bias on ordinary UI draws to prevent state leakage.
7. Validated actual capture state and failed closed for invalid storage, color,
   transform, palette, weights, bias, state, order or allocation.
8. Added physical world and HUD controls for color/blend, depth bias, reverse
   winding, rigged motion, order, collision, invalid input and identity.
9. Removed only `RENDER_DEBUG_BATCH_SIZE` from the explicit presentation veto;
   the other 12 visual modes and latent pathfinding guard remain fail-closed.

## Evidence

- Focused physical RelWithDebInfo world/UI cases: 2,330 and 521 assertions,
  pass under Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release suites: 46,028 assertions in 52
  cases each, pass; 1.21/1.39/1.13 warm seconds.
- Warnings-as-errors RelWithDebInfo production integration: pass.
- Neutral non-manual/non-GL gate: 484/484 in 35.40 seconds.
- Source-exact Release Development `26.4.0d68065`: pass at `27c0bec87c` after
  1,094 cacheable compilation tasks.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `ad71189d24bfb9e250eecf31485aad44ff0f11ca7e1100ca70e4039b196a82e6`.
- Source/packaged MSL SHA-256:
  `4331aad8f440c6d92473a5d998bf60121521868ba47186982b87524a2cfc0764`.
- Metallib SHA-256:
  `4d2140cf789ba313ba6cd5aff1bda7f39ea12e46c661f88379b01c4fc99c4475`.

## Owner constraints retained

- Exact RLVa parity is M-013 nice-to-have. Exact comparisons require enabling
  RLV in both viewers, saving and restarting both; mandatory work is only the
  minimum needed for Metal/no-GL cutover.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers; off/on is invalid and both-off cannot close parity.
- The executable still links OpenGL, so strict no-GL and product completion are
  explicitly open.

## Next boundary

Port `RENDER_DEBUG_LIGHTS`: light-only translucent-cyan face triangles plus the
white spatial-extents and yellow radius outlines with their exact depth and
order semantics. Remove only that eligibility bit after physical proof.
