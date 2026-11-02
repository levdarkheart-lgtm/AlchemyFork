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
| M-006 | UI, fonts, HUD and media ingress | doing | Native login/world UI and CEF IOSurface ingress work; Highlight Transparent replays through ordered static/rigged HUD capture. Moving CEF/VLC reference parity remains open. |
| M-007 | World scene and material correctness | doing | Bounded active transparency/material and reflection-probe audits found no valid-production divergence. Broader live parity remains incomplete. |
| M-008 | Advanced views/effects and compute | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 13 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Continue reachable debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The explicit 13-mode render-debug world-output set stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `0ac23c10b1` captures and replays Develop **Update Type** natively.
Drawables with a known last simulator update class contribute each valid face's
position/index buffers, range, transform, exact fixed color and traversal
order. Full is green, terse-improved cyan, compressed cached red, compressed
uncached yellow and full-cached blue, all at alpha `0.5`. The source keeps the
position-only, non-rigged `gDebugProgram`; no texture or skin palette is part of
this owner. HUD-only output also enters the established uniform-color capture.

Capture requires complete mirrored buffers, triangle cardinality, aligned
in-range offsets, an exact allowed color, enabled blending, disabled culling,
RGB-only writes and inherited forward/reverse `LEQUAL` read/write depth. The
native opaque-post pass merges Update Type with Points, Agent Target and Texture
Anim by source order. Invalid storage, color, transform, order or allocation
fails closed; unknown/off/no-face input is a no-op.

Physical GPU tests prove all five colors, RGB-only half-alpha blending,
no-cull reverse winding, forward occlusion/depth writes, reverse-Z depth writes,
both order directions against Texture Anim, duplicate-order and invalid-input
rejection, and no-owner identity. The focused RelWithDebInfo world case passes
2,293 assertions under Metal API/GPU/load-store/shader validation. Complete
RelWithDebInfo, Debug and Release suites each pass 45,951 assertions/52 cases
under the same validators in 5.94, 6.18 and 5.94 seconds.

Warnings-as-errors RelWithDebInfo production integration compiles, links,
stages and signs. The neutral gate passes 484/484 in 35.57 seconds. Source-exact
Release Development build `26.4.0d68063` at `0ac23c10b1` stages and signs. Deep
strict signing and 349/349 ARM64 inspection pass. Executable SHA-256 is
`2cbd61d22685d650f66861d9e2cdefc22d30156bdf2caf7345454f8631525ab6`;
packaged/source MSL is
`fc55ef6f5161d51eb6ce8b8ee8bfb6a9316c9752f5262592b8254fc3b06701fa`;
compiled metallib is
`4e9ab857f3a312a7ddf8b5fd13982cd4c4037b6525e8631e331edf323a14d7e1`.
The executable still links OpenGL during migration, so strict no-GL is open.

The render-debug guard now contains exactly the 13 modes that still emit
uncaptured world pixels or alter captured geometry. Update Type, Texture Anim and reflection-
probe neighbor links join Composition, Points, Agent Target, Wind Vectors, wireframe,
Highlight Transparent, update blips, reflection-probe volumes and the far-
plane overlay as a native advanced-output owner. Nine text-only and four unused
masks remain native-eligible. The 13 visual modes and the
latent pathfinding guard remain fail-closed; the current OSS pathfinding implementation
is a hard-disabled stub and does not block normal authority.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Develop Update Type can retain
   native world authority; all other uncaptured world-output modes remain
   protected by the explicit fail-closed mask.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live wireframe/material/editing parity; full stability/feature
   matrices; strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** implement `RENDER_DEBUG_BATCH_SIZE` next. It is
   the next direct draw-info triangle replay, with debug color, polygon offset
   and static/rigged palette variants that must be retained explicitly.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Capture and replay Develop **Batch Size** draw-info triangles with the source
debug color, polygon offset, static/rigged transform and matrix-palette
contract. Prove both geometry families, range/order, forward/reverse depth plus
offset, empty/off identity and fail-closed invalid input on the physical M1 Max
before removing only `RENDER_DEBUG_BATCH_SIZE` from the visual-output mask.
Retain the other 12 visual debug bits and pathfinding guard afterward.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
