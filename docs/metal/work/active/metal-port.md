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
| M-002 | GL/shader/pass/resource and binary inventory | doing | Informational scan covers 2,408 viewer-owned files and 3,303 potential findings (including macros/comments/duplicates); strict release audit is not green. |
| M-003 | Native drawable, presentation and lifecycle | doing | Authenticated exact publication and clean lifecycle are observed; preserve while advanced families are validated. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core typed resource paths have physical coverage; remaining advanced/media matrices stay open. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native login/world UI and CEF IOSurface ingress work; Highlight Transparent and Batch Size have ordered static/rigged HUD replay. Moving CEF/VLC reference parity remains open. |
| M-007 | World scene and material correctness | doing | Bounded active transparency/material and reflection-probe audits found no valid-production divergence. Broader live parity remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 7 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Continue reachable debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The explicit 7-mode render-debug world-output set stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `e84d5e660b` captures and replays Develop **Joints** natively. An outer
transaction scopes the shared line/sphere helper to the spatial-world
`renderBones()` call and keeps model-preview calls independent. Segment records
retain exact line and post-scale sphere MVPs, endpoint, scale, colors and order;
the owner counts every non-null skeleton joint before traversal.

Replay reuses the exact 651-vertex / 3,600-index sphere mesh and paired encoder.
It preserves ordinary 0.001 scale, visible gray, red/cyan/green occluded classes,
the source-selected 0.016 yellow/gray grammar, writable visible depth and
complementary read-only occluded depth in forward and reverse Z.

Exact hooks also retain yellow joint-space rigged bounds and cyan world-space
attachment-face bounds. These use the proven 24-vertex outline pipeline with
their live MVP, center, half extent and source order. Validation rejects invalid
storage, non-finite values, kind, scale, color, extent, owner order or allocation.
The global debug merge now includes Joints alongside Avatar Volume, Agent Target,
Points, Texture Anim, Update Type, Batch Size and Lights.

Physical tests prove invalid-input rejection, visible/occluded forward/reverse
depth, independent bounds rasterization, destination-alpha preservation and
explicit Joints-vs-Agent-Target pixel order. The focused owner case passes 4,339
assertions. Complete RelWithDebInfo, Debug and Release suites each pass 48,107
assertions in 52 cases, with warm elapsed times of 1.21, 1.44 and 1.20 seconds.

Warnings-as-errors RelWithDebInfo production integration compiles, links, stages
and signs. The neutral gate passes 484/484 in 35.56 seconds. Source-exact Release
Development build `26.4.0d68075` (`26.4.0.68075`) at `e84d5e660b` passes with
0 hits across 127 cacheable compilation tasks. Deep strict signing and 349/349
ARM64 inspection pass. Executable SHA-256 is
`6234111e7e6f7a688389cc6e20de3d4ebb60ce6691d224fc0105a76d4e1acb80`;
packaged/source MSL is
`3c918952dd66d3122d683d478020e13d99e0485508e95ce9d0245dc0f13bf393`;
compiled metallib is
`5e56cdd100abf8e6c6a40d666a9d80a9bcc5f3ae4b6a204a6f5d0eeba1c156c1`.
The executable still links OpenGL during migration, so strict no-GL is open.

The render-debug guard now contains exactly 7 modes that still emit uncaptured
world pixels or alter captured geometry: BBOXES, OCTREE, OCCLUSION, RAYCAST,
PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY. The latent pathfinding guard remains
separately fail-closed; the current OSS implementation is a hard-disabled stub.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Develop Joints can retain native world authority;
   all uncaptured world-output modes remain protected by the explicit fail-closed
   mask.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live wireframe/material/editing parity; full stability/feature
   matrices; strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** implement `RENDER_DEBUG_BBOXES` next. Its finite
   group/drawable/face extents and color classes reuse the proven outline path;
   every other remaining owner requires broader geometry or state replay.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Capture and replay BBOXES group, drawable and per-face outlines at their exact
spatial traversal sites. Preserve bridge transforms, group/drawable color
classes, active-object line width, depth behavior and source order. Prove empty
controls, forward/reverse depth, alpha preservation and invalid-input rejection
on the physical M1 Max before removing only `RENDER_DEBUG_BBOXES`. Retain the
other six visual bits and pathfinding guard.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
