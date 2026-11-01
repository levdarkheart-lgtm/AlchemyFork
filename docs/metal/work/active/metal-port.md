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
| M-008 | Advanced views/effects and compute | doing | Wireframe, Wind Vectors, Agent Target, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 18 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Continue reachable debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. The explicit 18-mode render-debug world-output set stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `0d8b5ccdda` captures and replays Develop **Agent Target** natively at
the exact self-avatar-only `renderAgentTarget()` owner. It retains four 0.2 m
crosshairs: red agent position, green drawable position, white root world
position and blue pelvis world position, each with alpha 0.8.

The native opaque-post pass accepts exactly 24 line vertices and one transform,
then renders after object-update blips and before reflection-probe-volume
visualization. Dedicated LDR/HDR pipelines use source-alpha blending for RGB,
leave alpha untouched and retain forward/reverse-Z depth reads and writes.
Duplicate or partial traversal, missing storage, allocation failure, overflow,
non-finite input or mismatched inherited GL state invalidates native
publication; empty/no-owner input is a no-op.

Physical GPU tests prove all four exact blend equations, RGB-only alpha
preservation, forward/reverse-Z pass/rejection and writes, cardinality and
non-finite rejection, and output/depth identity without an owner. The focused
RelWithDebInfo Agent Target/world case passes 1,998 assertions under Metal
API/GPU/load-store/shader validation. Complete RelWithDebInfo, Debug and Release
suites each pass 45,654 assertions/52 cases under the same validators.

Warnings-as-errors RelWithDebInfo production integration compiles, links,
stages and signs. The neutral gate passes 484/484 in 35.57 seconds. Source-exact
Release Development build `26.4.0d68053` at `0d8b5ccdda` stages and signs. Deep
strict signing and 349/349 ARM64 inspection pass. Executable SHA-256 is
`ac11e94f7bd088c26fc1df2d2f81ca37b004af471c5021922c8f89b3fff8879f`;
packaged/source MSL is
`a33ec7160965a1c6805ad5b2a2f69d95da8bff630cf2b05c346a57deb13e2033`;
compiled metallib is
`537685a0c488cded37fc64b7a532f57610771deeaf9f17d6ae9c1772787549b1`.
The executable still links OpenGL during migration, so strict no-GL is open.

The render-debug guard now contains exactly the 18 modes that still emit
uncaptured world pixels or alter captured geometry. Agent Target joins Wind
Vectors, wireframe, Highlight Transparent, update blips, reflection-probe
volumes and the far-plane overlay as a native advanced-output owner. Nine text-
only and four unused masks remain native-eligible. The 18 visual modes and the
latent pathfinding guard remain fail-closed; the current OSS pathfinding implementation
is a hard-disabled stub and does not block normal authority.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Agent Target can retain native world authority; all
   other uncaptured world-output modes remain protected by the explicit fail-
   closed mask.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live wireframe/material/editing parity; full stability/feature
   matrices; strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** implement Develop Points next. Source inspection
   shows one drawing function that emits one white point at each valid drawable
   face center with depth disabled. It spans region partitions and visible
   spatial bridges, requiring per-traversal transform retention; Raycast is
   broader because it spans octree, object/intersection and particle outputs.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Capture and replay Develop **Points** as one white point per valid drawable face
center. Prove face filtering, region/bridge transforms, color, point size,
disabled-depth behavior and off/empty controls on the physical M1 Max before
removing only `RENDER_DEBUG_POINTS` from the visual-output mask. Retain the
other 17 visual debug bits and the latent pathfinding guard.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
