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
| M-008 | Advanced views/effects and compute | doing | Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. OSS pathfinding is a nonfunctional stub. Continue reachable render-debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. General render-debug and wireframe owners stay fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `28c4dca74b` replaces the persisted Develop Render Reflection Probe
Volumes fallback with native Metal replay. Capture occurs at the authoritative
GL draw boundary and retains the live probe-selection constants plus the
current inverse projection. A constants-only producer intentionally avoids
radiance/irradiance-generation dependencies because this visualization samples
only manual probe shapes and the retained scene depth.

The full-screen Metal pass reconstructs eye-space position under forward or
reverse Z, then replays the GLSL sphere and box intersection rules for manual
probes only. Front intersections accumulate yellow; hidden intersections
accumulate distance-faded blue. Source-alpha RGB blending and RGB-only writes
preserve destination alpha and depth. HDR and non-HDR pipelines match the
destination formats, and the pass is ordered after object-update blips and
before the native far-plane overlay exactly as the observable GL owners are
ordered when the remaining general-debug guard is clear.

Invalid probe counts, nonfinite inverse-projection or consumed shape data,
negative sphere radii, unreadable depth, allocation failure or duplicate
capture rejects native authority. The migration GL owner remains unchanged.
Only after asymmetric physical proof was the reflection-probe-volume eligibility
veto removed.

Prior native owners remain integrated: the far-plane projection overlay,
object-update blips and static/rigged world/HUD Highlight Transparent. The
linked OSS pathfinding implementation is a hard-disabled stub and cannot emit
retained output. Remaining reachable fail-closed advanced owners are general
render-debug output and wireframe; the latent pathfinding guard remains. The
bounded reflection-probe production-call audit is separately closed without a
speculative edit because its extra Metal bounds checks do not alter valid
viewer-produced data.

Focused physical RelWithDebInfo replay passes 1,470 assertions under Metal API/
GPU/load-store/shader validation. Complete RelWithDebInfo, Debug and Release
suites each pass 45,118 assertions/52 cases under the same validators. The
neutral gate passes 484/484 in 40.46 seconds. Warnings-as-errors RelWithDebInfo
production viewer integration and the full source-exact Release wrapper build
both link and stage.

Source-exact Development `26.4.0d68044` at `28c4dca74b` stages and signs. Deep
strict signing and 349/349 ARM64 inspection pass. Executable SHA-256 is
`4306dea5dedc006eab0017f40404985d323f224d0b40c864b54bc61f2fcc45e0`;
packaged/source MSL is
`dd51886cc8cc33a8b19ac69206fed5045421c8905bb51c7d596fdeaefb467214`;
compiled metallib is
`b5ccb104ec22fade3cf1822595924c751128b6ca14e0ab188276890339357e58`.
The executable still links OpenGL during migration, so strict no-GL is open.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** a reachable persisted advanced-output owner now
   owns its manual sphere/box intersections, depth reconstruction, blending and
   output side effects in Metal.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live material/editing parity; full stability/feature matrices;
   strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** audit wireframe and general render-debug output,
   then implement the smallest confirmed missing owner without broad replay or
   speculative edits.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace production contribution and capture state for wireframe and general
render-debug output. Select the smallest confirmed reachable omission,
implement its exact native state/order with asymmetric physical controls, retain
the latent pathfinding guard, and leave every other uncovered owner fail-closed.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
