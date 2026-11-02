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
| M-008 | Advanced views/effects and compute | doing | The established native advanced-output set now includes Raycast, Occlusion, Octree, Physics Shapes and Normals. TEXEL_DENSITY is the sole explicit visual veto. OSS pathfinding is a nonfunctional stub. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The final explicit render-debug owner stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `08973b83dc` captures and replays both reachable Develop **Normals**
paths. Volume faces retain exact CPU-expanded yellow normal and cyan tangent
line pairs with inverse-scale normalization and live transforms. Non-volume
drawables retain zero-copy Metal position/normal/tangent and u16/u32 index
buffers; four native vertex variants expand one normal and optional tangent
line per triangle-index occurrence like the source GL geometry shader.

Replay preserves view-space direction normalization, one-pixel raster,
disabled culling, source-over blend, RGB-only writes and read-only LEQUAL depth
under forward/reverse Z. Existing volume HUD lines stay in native ordered UI
capture; unexpected non-volume HUD use fails closed. Validation rejects invalid
storage/data/kind/color/ranges/buffers/index width/transforms/draw length/
tangent contract/state/order and all cross-owner collisions.

RelWithDebInfo, Debug and Release physical Metal suites each pass 51,416
assertions in 52 cases under Metal API/load-action validation. Warnings-as-errors
production integration passes, and the neutral non-manual/non-GL gate passes
484/484 in 35.58 seconds. The prior unfiltered 574-test inventory was not rerun;
its six GL-only failures and five skips remain recorded without a green claim.
Harness validation covers all 31 required files and all 24 tooling tests pass.

Source-exact Release Development build `26.4.0d68087` (`26.4.0.68087`) at
`08973b83dccad57d39fa0386c4f8463385761d2a` passes after a 161-step rebuild. Deep strict signing and 349/349
ARM64 inspection pass. Executable SHA-256 is
`039fbe6e2af93d5c9fc63d8c94b79a574323aa908a4a27a08c7a16c6807f445d`;
packaged/source MSL is
`73955209f611038a5e4fe0305642614fefee30766dcd26fd03b7df7d3933df20`;
compiled metallib is
`fd64dfedceb9056e85090916b7e0dab2d8e17c553cc4ffe63ea1b5e67647c297`.
The executable still links OpenGL, so strict no-GL remains open.

The render-debug guard now contains exactly one world-output mode:
TEXEL_DENSITY. The latent pathfinding guard remains separately fail-closed.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Develop Normals can retain native authority for its
   CPU volume-line and indexed non-volume paths; every uncaptured world-output
   mode remains fail-closed.
2. **Deferred critical work:** the final non-RLVa advanced output, broader
   post/shadow parity, controlled live comparisons, stability/feature matrices,
   strict no-GL, independent review and qualifying M1 Max performance evidence.
3. **Priority reassessment:** audit `RENDER_DEBUG_TEXEL_DENSITY` next. Its full
   producer/shader/resource/state surface must remain one coherent owner before
   the explicit visual veto can become empty.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Inventory every Texel Density draw at its exact source site, then capture/replay
the complete owner without changing source geometry or GL output. Preserve all
producer selection, vertex/index/texture inputs, transforms, face filters,
colors, depth/cull/raster/blend state, side effects and global order. Prove the
production paths, forward/reverse depth and invalid/no-owner cases on the
physical M1 Max before removing only `RENDER_DEBUG_TEXEL_DENSITY`.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
