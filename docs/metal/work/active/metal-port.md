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
| M-008 | Advanced views/effects and compute | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 11 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Continue reachable debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The explicit 11-mode render-debug world-output set stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `4b755ffc95` captures and replays Develop **Lights** natively. Each light
face retains its Metal position/index buffers, byte range, transform and source
order. Face geometry is translucent cyan with source-alpha RGB blending,
destination alpha retained, disabled culling and inherited forward/reverse
`LEQUAL` read/write depth. A procedural white spatial-extents outline follows
with always-pass/write depth, then a yellow light-radius outline with ordinary
forward/reverse `LEQUAL` read/write depth.

Capture validates exact blend factors/equations, RGB-only color mask, no cull,
per-kind depth state and unit line width. The MSL outline emits the same 24 box
edge vertices as `drawBoxOutline`. The native opaque-post pass merges Lights with
Points, Agent Target, Texture Anim, Update Type and Batch Size by source order.
Invalid storage, color, transform, range, state, order or allocation fails
closed; off/empty/non-light input is a no-op. HUD faces use the explicit
untextured debug shader, while HUD immediate box lines stay in ordered generic
UI capture.

Physical GPU tests prove cyan blend and alpha preservation, no-cull reverse
winding, triangle depth writes, white always-pass depth writes, yellow
forward/reverse `LEQUAL` visibility/depth writes, both order directions,
collision rejection, invalid cases, no-owner identity and the offline shader
entry point. The focused RelWithDebInfo world case passes 2,380 assertions.
Complete RelWithDebInfo, Debug and Release suites each pass 46,079 assertions in
52 cases under Metal API/GPU/load-store/shader validation, with warm elapsed
times of 1.22, 1.38 and 1.16 seconds.

Warnings-as-errors RelWithDebInfo production integration compiles, links, stages
and signs. The neutral gate passes 484/484 in 35.40 seconds. Source-exact Release
Development build `26.4.0d68067` at `4b755ffc95` passes after 127 cacheable
compilation tasks. Deep strict signing and 349/349 ARM64 inspection pass.
Executable SHA-256 is
`8f2815a0848ae5028aad7f460625d3c3f3228d74ba29f97253bd02913fa1d5d6`;
packaged/source MSL is
`ad52fd989f6a3442eaf0fe38348b9968c8721d68d663cb468418af12ed5bda32`;
compiled metallib is
`b0af557be051b32b1fa07d85ec4d0bc98c883b319e05f7bd6d5c3098da18eaca`.
The executable still links OpenGL during migration, so strict no-GL is open.

The render-debug guard now contains exactly 11 modes that still emit uncaptured
world pixels or alter captured geometry: BBOXES, OCTREE, OCCLUSION,
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

1. **Product advancement:** Develop Lights can retain native world authority;
   all other uncaptured world-output modes remain protected by the explicit
   fail-closed mask.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live wireframe/material/editing parity; full stability/feature
   matrices; strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** implement `RENDER_DEBUG_IMPOSTORS` next. Its bounded
   source owner is one four-edge additive white outline around an already native-
   captured billboard, with time-decaying line width and inherited depth.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Capture and replay Develop **Impostors** as the four exact billboard edges with
additive white blend, inherited depth, dynamic 5-to-1 pixel thickness and source
order. Prove basis/edge placement, thickness decay, forward/reverse depth,
off/no-impostor identity and fail-closed invalid input on the physical M1 Max
before removing only `RENDER_DEBUG_IMPOSTORS` from the visual output mask.
Retain the other 10 visual debug bits and pathfinding guard.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
