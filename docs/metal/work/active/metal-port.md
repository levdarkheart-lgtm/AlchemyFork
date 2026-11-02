# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | ARM64 build/package/runtime identity is evidenced; controlled fixed-view reference benchmark remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | Informational scan covers 2,408 viewer-owned files and 3,303 potential findings including macros/comments/duplicates; strict release audit is not green. |
| M-003 | Native drawable, presentation and lifecycle | doing | Authenticated exact publication and clean lifecycle are observed; preserve while advanced families are validated. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core typed resource paths have physical coverage; remaining advanced/media matrices stay open. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native login/world UI and CEF IOSurface ingress work; Highlight Transparent and Batch Size have ordered static/rigged HUD replay. Moving CEF/VLC reference parity remains open. |
| M-007 | World scene and material correctness | doing | Bounded active transparency/material and reflection-probe audits found no valid-production divergence. Broader live parity remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | The established native advanced-output set now includes Raycast and Occlusion. Four explicit visual modes remain fail-closed: OCTREE, PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY. OSS pathfinding is a nonfunctional stub. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The four-mode render-debug set stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `ae691b02da` captures and replays Develop **Occlusion** natively. Each
non-null group vertex buffer is captured immediately before its x-ray GL draw;
the later selected-group loop captures its filled magenta bounds. Records retain
zero-copy external vertex/index mirrors, exact MVP and global source order.

The Metal x-ray path uses back-face culling, indexed triangle line fill, the
source line-polygon offset, standard alpha blend into RGB only, and complementary
read-only forward/reverse depth. Selected bounds use filled procedural boxes,
back-face culling and ordinary read-only forward/reverse depth. Validation rejects
invalid storage/index/range/count/offset/transform/extent/kind/order and cross-
owner collisions.

RelWithDebInfo, Debug and Release physical Metal suites each pass 50,687
assertions in 52 cases under Metal API/load-action validation. Warnings-as-errors
production integration passes, and the neutral non-manual/non-GL gate passes
484/484 in 37.43 seconds. The prior unfiltered 574-test inventory was not rerun;
its six GL-only failures and five skips remain recorded without a green claim.
Harness validation covers all 31 required files and all 24 tooling tests pass.

Source-exact Release Development build `26.4.0d68081` (`26.4.0.68081`) at
`ae691b02da` passes after a 161-step rebuild. Deep strict signing and 349/349
ARM64 inspection pass. Executable SHA-256 is
`cb22d7020ea6f2663c53d362847c8a7031bbc3487129b8003f06e22907cb351c`;
packaged/source MSL is
`12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`;
compiled metallib is
`c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.
The executable still links OpenGL, so strict no-GL remains open.

The render-debug guard now contains exactly four world-output modes: OCTREE,
PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY. The latent pathfinding guard remains
separately fail-closed.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Develop Occlusion can retain native authority for
   group x-ray geometry and selected filled bounds; every uncaptured world-output
   mode remains fail-closed.
2. **Deferred critical work:** four remaining non-RLVa advanced outputs, broader
   post/shadow parity, controlled live comparisons, stability/feature matrices,
   strict no-GL, independent review and qualifying M1 Max performance evidence.
3. **Priority reassessment:** audit and implement `RENDER_DEBUG_OCTREE` next as
   one coherent owner, including flash lifetime, rigged faces, palettes, blend
   and wide-outline state. The other three modes own broader derived geometry.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Inventory every Octree draw and side effect at its exact source sites, then
capture/replay the complete owner without changing rebuild behavior or GL output.
Preserve bridge/model transforms, rigged palettes, indexed ranges, group bounds,
flash lifetime, additive blend, depth/cull/raster state and global order. Prove
forward/reverse, static/rigged, flash/no-flash and invalid/no-owner cases on the
physical M1 Max before removing only `RENDER_DEBUG_OCTREE`.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
