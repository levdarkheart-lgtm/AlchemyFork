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
| M-008 | Advanced views/effects and compute | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 15 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Continue reachable debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The explicit 15-mode render-debug world-output set stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `b76c52576d` captures and replays the active Develop **Reflection
Probes** neighbor links natively. Only relevant probes contribute. Each neighbor
is retained exactly once in source order: orange if either endpoint lacks a
viewer object, yellow if both own viewer objects. The older node-volume block is
compiled out and is not part of the active contract.

The bridge pre-counts links with overflow protection and streams exactly two
compact position/color vertices per link plus one transform. Capture requires
the inherited enabled blend, RGB-only color mask, one-pixel line width and
forward/reverse read/write depth state. The native opaque-post pass draws the
links after spatial Points/Agent Target traversal and before manual reflection-
probe volumes. Invalid count/storage, allocation failure, non-finite input,
invalid homogeneous position or any non-orange/yellow color fails closed;
empty/off input is a no-op.

Physical GPU tests prove exact orange/yellow output, RGB-only alpha, forward
occlusion and depth writes, reverse-Z depth writes, invalid storage/data
rejection and no-owner identity. The focused RelWithDebInfo reflection-link/
world case passes 2,162 assertions under Metal API/GPU/load-store/shader
validation. Complete RelWithDebInfo, Debug and Release suites each pass 45,818
assertions/52 cases under the same validators in 5.95, 6.22 and 5.90 seconds.

Warnings-as-errors RelWithDebInfo production integration compiles, links,
stages and signs. The neutral gate passes 484/484 in 35.39 seconds. Source-exact
Release Development build `26.4.0d68059` at `b76c52576d` stages and signs. Deep
strict signing and 349/349 ARM64 inspection pass. Executable SHA-256 is
`8460b7356784d39d0c1163a4b157e9f9bc25faf85a53c31cc84aeba99dec53e1`;
packaged/source MSL is
`d05669c80b490283e9376fb797a48715bb079bc1928022884193130149643a21`;
compiled metallib is
`994eaa49f2c573361c5951c443165ce63c6919b44c032fb6d4e027358dbbcd1b`.
The executable still links OpenGL during migration, so strict no-GL is open.

The render-debug guard now contains exactly the 15 modes that still emit
uncaptured world pixels or alter captured geometry. Reflection-probe neighbor
links join Composition, Points, Agent Target, Wind Vectors, wireframe,
Highlight Transparent, update blips, reflection-probe volumes and the far-
plane overlay as a native advanced-output owner. Nine text-only and four unused
masks remain native-eligible. The 15 visual modes and the
latent pathfinding guard remain fail-closed; the current OSS pathfinding implementation
is a hard-disabled stub and does not block normal authority.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Develop Reflection Probes neighbor links can retain
   native world authority; all other uncaptured world-output modes remain
   protected by the explicit fail-closed mask.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live wireframe/material/editing parity; full stability/feature
   matrices; strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** implement `RENDER_DEBUG_TEXTURE_ANIM` next. Its
   sole active output is a flat translucent-yellow replay of visible draw-info
   triangles with a non-null texture matrix; it adds no separate generated
   primitive family and can reuse bounded world geometry capture.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Capture and replay Develop **Texture Anim** flat triangle overlays only for
visible draw infos with a non-null texture matrix. Prove exact yellow/0.5-alpha
source semantics, static and rigged geometry, traversal/transform, inherited
state, depth interaction and empty/off controls on the physical M1 Max before
removing only `RENDER_DEBUG_TEXTURE_ANIM` from the visual-output mask. Retain
the other 14 visual debug bits and pathfinding guard.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
