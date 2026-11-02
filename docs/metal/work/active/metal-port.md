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
| M-008 | Advanced views/effects and compute | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Raycast, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; five visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Continue reachable debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The explicit five-mode render-debug world-output set stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `e8f6613c4e` captures and replays Develop **Raycast** natively across the
complete world spatial traversal. It retains every intersected volume-octree
branch outline and expands immediate triangles to explicit edge pairs for the
source one-pixel hidden and three-pixel visible passes.

Records preserve exact MVP, center, extent, palette, line width, triangle data
and global order. Branch outlines use writable depth; hidden outline/triangle
passes use complementary read-only depth; visible triangles use ordinary
read-only depth. The selected object retains its three oriented filled axis
boxes and always-visible read-only bounds. Selected avatars reuse the proven
Collision Skeleton owner. The later particle-hit crosshair is replayed at its
separate post-deferred source point after reflection-probe diagnostics.

Validation rejects invalid storage, transform, vertex, extent, palette, kind,
range, width and order. Complete RelWithDebInfo, Debug and Release physical
Metal suites each pass 50,153 assertions in 52 cases, covering all owners,
forward/reverse depth classes, alpha retention, width growth, invalid/no-owner
controls and Raycast-vs-Points order. Warnings-as-errors production integration
passes. The neutral gate passes 484/484 in 39.45 seconds.

The unfiltered 574-test inventory has six reproducible GL-only failures: three
primary tests (`alframefences_gl_test`, `llrendertarget_gl_test`,
`aldof_gl_test`) and their three aggregate/benchmark entries; five tests skip.
This full inventory is not claimed green and does not implicate the Raycast owner.

Source-exact Release Development build `26.4.0d68079` (`26.4.0.68079`) at
`e8f6613c4e` passes after a 161-step rebuild. Deep strict signing and 349/349
ARM64 inspection pass. Executable SHA-256 is
`f113ecab21125f63f88c2f3185038eb123ced6b0a48e1f258f73168259f8ec5e`;
packaged/source MSL is
`12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`;
compiled metallib is
`c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.
The executable still links OpenGL during migration, so strict no-GL is open.

The render-debug guard now contains exactly five world-output modes: OCTREE,
OCCLUSION, PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY. The latent
pathfinding guard remains separately fail-closed; the current OSS implementation
is a hard-disabled stub.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Develop Raycast can retain native world authority
   for octree branches/triangles, selected object/avatar hits and the particle
   marker; all uncaptured world-output modes remain fail-closed.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live wireframe/material/editing parity; full stability/feature
   matrices; strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** implement `RENDER_DEBUG_OCCLUSION` next. Its
   group-buffer x-ray geometry and selected-group boxes reuse established
   indexed geometry and procedural boxes; the other four owners are broader.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Capture Occlusion group-buffer x-ray geometry and the later selected-group boxes
at exact source sites without altering visibility bookkeeping. Preserve bridge/
model transforms, indexed ranges, cull, line polygon offset, complementary read-
only depth, blend/palette and order. Prove empty, selected, forward/reverse and
invalid-input cases on the physical M1 Max before removing only
`RENDER_DEBUG_OCCLUSION`. Retain the other four bits and pathfinding guard.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
