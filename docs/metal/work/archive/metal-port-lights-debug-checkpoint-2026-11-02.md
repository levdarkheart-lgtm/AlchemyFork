# Metal-port execution archive — Lights

**Date:** 2026-11-02
**Code commit:** `4b755ffc95` (`feat(metal): replay lights debug overlay`)

## Implemented boundary

1. Audited `renderLights` and retained the source order: translucent-cyan face
   triangles, white spatial-extents outline, then yellow light-radius outline.
2. Reused each face's existing Metal position/index buffers, exact range,
   transform and original traversal order without a CPU geometry copy.
3. Added procedural MSL box-outline generation for the exact 24 vertices emitted
   by `drawBoxOutline`.
4. Added source-alpha RGB-only face blending with destination alpha retained,
   disabled culling and forward/reverse `LEQUAL` read/write depth.
5. Added always-pass/write depth for white spatial bounds and forward/reverse
   `LEQUAL` read/write depth for yellow radius bounds.
6. Merged Lights with Points, Agent Target, Texture Anim, Update Type and Batch
   Size by source order and rejected collisions or non-monotonic order.
7. Added explicit untextured HUD face publication while retaining immediate-mode
   HUD outline geometry in ordered generic UI capture.
8. Failed closed for invalid storage, color, transform, range, state, order or
   allocation; off/empty/non-light input remains a no-op.
9. Removed only `RENDER_DEBUG_LIGHTS` from the visual eligibility veto; 11 visual
   modes and the latent pathfinding guard remain fail-closed.

## Evidence

- Focused physical RelWithDebInfo world case: 2,380 assertions, pass under Metal
  API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release suites: 46,079 assertions in 52
  cases each, pass; 1.22/1.38/1.16 warm seconds.
- Warnings-as-errors exact viewer translation units and full RelWithDebInfo
  production integration: pass.
- Neutral non-manual/non-GL gate: 484/484 in 35.40 seconds.
- Source-exact Release Development `26.4.0d68067`: pass at `4b755ffc95` after
  127 cacheable tasks.
- Deep strict signing: pass; ARM64 inspection: 349/349 Mach-O slices.
- Executable SHA-256:
  `8f2815a0848ae5028aad7f460625d3c3f3228d74ba29f97253bd02913fa1d5d6`.
- Source/packaged MSL SHA-256:
  `ad52fd989f6a3442eaf0fe38348b9968c8721d68d663cb468418af12ed5bda32`.
- Metallib SHA-256:
  `b0af557be051b32b1fa07d85ec4d0bc98c883b319e05f7bd6d5c3098da18eaca`.

## Owner constraints retained

- Exact RLVa parity is M-013 nice-to-have. Exact comparisons require enabling
  RLV in both viewers, saving and restarting both; mandatory work is only the
  minimum needed for Metal/no-GL cutover.
- Shadow comparisons require shadows enabled in Graphics Preferences in both
  viewers before capture; off/on is invalid and both-off cannot close parity.
- The executable still links OpenGL, so strict no-GL and product completion are
  explicitly open.

## Next boundary

Port `RENDER_DEBUG_IMPOSTORS`: the four-edge additive white billboard outline
using the already captured center/left/up basis, with inherited depth, source
order and exact time-decaying 5-to-1 pixel thickness. Remove only that eligibility
bit after physical proof.
