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
| M-008 | Advanced views/effects and compute | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; six visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Continue reachable debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The explicit six-mode render-debug world-output set stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `a22cc91fc4` captures and replays Develop **Bounding Boxes** natively.
The transaction is scoped to world spatial debug
traversal; it cannot admit the shadow helper's uncolored boxes, and HUD/cutout/
impostor ownership is source-audited.

Records retain exact MVP, center, half extent, palette color, one-to-five-pixel
line width and global order for group, face and active/nonactive drawable boxes.
Thin boxes reuse the 24-vertex outline. Wide boxes use a new 72-vertex screen-
space quad expansion pipeline with forward/reverse writable depth, no cull and
RGB-only output. Validation rejects missing storage, non-finite data, negative
extent, invalid color/width/order and allocation failure. The exact global debug
merge includes BBOX.

Complete RelWithDebInfo, Debug and Release physical Metal suites each pass
49,644 assertions in 52 cases. Coverage includes invalid-input and no-owner
rejection, thin/wide rasterization, width growth, forward/reverse writable depth,
alpha retention and BBOX-vs-Points order. Warnings-as-errors production
integration passes. The neutral gate passes 484/484 in 35.42 seconds.

The unfiltered 574-test inventory has six reproducible GL-only failures: three
primary tests (`alframefences_gl_test`, `llrendertarget_gl_test`,
`aldof_gl_test`) and their three aggregate/benchmark entries; five tests skip.
This full inventory is not claimed green and does not implicate the BBOX owner.

Source-exact Release Development build `26.4.0d68077` (`26.4.0.68077`) at
`a22cc91fc4` passes after a 1,397-edge rebuild. Deep strict signing and 349/349
ARM64 inspection pass. Executable SHA-256 is
`328024070349f48b9cb3b42f1bee2345c54dfd90c84e02d0fa79218bb81b8b37`;
packaged/source MSL is
`fa3181abe44fbaec3ffab41248d23e377ec5608faf00188f776f475404ab4d92`;
compiled metallib is
`d9370caf0b49a92972fc33b8de8e3f7451d7f21295bd5d2182ce62c0c2185959`.
The executable still links OpenGL during migration, so strict no-GL is open.

The render-debug guard now contains exactly six world-output modes: OCTREE,
OCCLUSION, RAYCAST, PHYSICS_SHAPES, NORMALS and TEXEL_DENSITY. The latent
pathfinding guard remains separately fail-closed; the current OSS implementation
is a hard-disabled stub.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Develop Bounding Boxes can retain native world
   authority, including active-object wide outlines; all uncaptured world-output
   modes remain protected by the explicit fail-closed mask.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live wireframe/material/editing parity; full stability/feature
   matrices; strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** implement `RENDER_DEBUG_RAYCAST` next. It is
   selected-hit bounded and can reuse established primitives, but its volume-
   octree triangle batches and all hit-marker owners must be captured explicitly.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Capture Raycast branch boxes and two-pass triangle batches, object hit orientation
and bounds, selected-avatar collision output, and the post-deferred particle
crosshair at exact source sites. Preserve palette, depth/color grammar, transforms
and order. Prove empty/no-hit controls, forward/reverse depth, alpha preservation
and invalid-input rejection on the physical M1 Max before removing only
`RENDER_DEBUG_RAYCAST`. Retain the other five bits and pathfinding guard.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
