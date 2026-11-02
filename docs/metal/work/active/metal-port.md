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
| M-008 | Advanced views/effects and compute | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 12 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Continue reachable debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The explicit 12-mode render-debug world-output set stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `27c0bec87c` captures and replays Develop **Batch Size** natively. Each
valid draw info retains its existing Metal position/index buffers, byte range,
current transform, stable hash-derived color and traversal order. The source
polygon offset `(factor=-1, units=+1)` is translated into forward/reverse Metal
depth bias. Static and object-skinned avatar variants retain the source
`4 + joint_count*12` float matrix palette, 110-joint ceiling and `TYPE_WEIGHT4`
weights.

Capture requires complete mirrored buffers, triangle cardinality, aligned
in-range offsets, exact normalized hash color and alpha byte 200, source-alpha
RGB blend, disabled culling, polygon offset and inherited forward/reverse
`LEQUAL` read/write depth. The native opaque-post pass merges Batch Size with
Points, Agent Target, Texture Anim and Update Type by source order. Invalid
storage, color, palette, transform, state, order or allocation fails closed;
off/empty input is a no-op. HUD-only output uses explicit untextured static and
rigged debug shader families and semantic depth bias; ordinary UI draws reset
bias to zero.

Physical GPU tests prove hash color, alpha preservation, no-cull reverse winding,
forward/reverse depth bias, palette-driven rigged movement, both order directions
against Update Type, collision rejection, invalid cases and no-owner identity.
The focused RelWithDebInfo world and UI cases pass 2,330 and 521 assertions.
Complete RelWithDebInfo, Debug and Release suites each pass 46,028 assertions in
52 cases under Metal API/GPU/load-store/shader validation, with warm elapsed
times of 1.21, 1.39 and 1.13 seconds.

Warnings-as-errors RelWithDebInfo production integration compiles, links, stages
and signs. The neutral gate passes 484/484 in 35.40 seconds. Source-exact Release
Development build `26.4.0d68065` at `27c0bec87c` passes after 1,094 cacheable
compilation tasks. Deep strict signing and 349/349 ARM64 inspection pass.
Executable SHA-256 is
`ad71189d24bfb9e250eecf31485aad44ff0f11ca7e1100ca70e4039b196a82e6`;
packaged/source MSL is
`4331aad8f440c6d92473a5d998bf60121521868ba47186982b87524a2cfc0764`;
compiled metallib is
`4d2140cf789ba313ba6cd5aff1bda7f39ea12e46c661f88379b01c4fc99c4475`.
The executable still links OpenGL during migration, so strict no-GL is open.

The render-debug guard now contains exactly 12 modes that still emit uncaptured
world pixels or alter captured geometry: BBOXES, OCTREE, OCCLUSION, LIGHTS,
RAYCAST, SHADOW_FRUSTA, AVATAR_VOLUME, AVATAR_JOINTS, PHYSICS_SHAPES, NORMALS,
TEXEL_DENSITY and IMPOSTORS. The latent pathfinding guard remains separately
fail-closed; the current OSS implementation is a hard-disabled stub.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Develop Batch Size can retain native world authority;
   all other uncaptured world-output modes remain protected by the explicit
   fail-closed mask.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live wireframe/material/editing parity; full stability/feature
   matrices; strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** implement `RENDER_DEBUG_LIGHTS` next. Its bounded
   source owner is light-only face triangles followed by two generated box
   outlines with exact color/depth differences; it is narrower than the other
   remaining advanced-output paths.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Capture and replay Develop **Lights** face triangles plus white spatial-extents
and yellow light-radius outlines with exact source transform, color, depth,
blend and traversal order. Prove triangle/range/order, both boxes,
forward/reverse depth, empty/non-light identity and fail-closed invalid input on
the physical M1 Max before removing only `RENDER_DEBUG_LIGHTS` from the visual
output mask. Retain the other 11 visual debug bits and pathfinding guard.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
