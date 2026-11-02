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
| M-008 | Advanced views/effects and compute | doing | The established native advanced-output set now includes Raycast, Occlusion, Octree, Physics Shapes, Normals and Texel Density. No reachable render-debug world-geometry bit remains in the explicit visual veto. OSS pathfinding is a nonfunctional stub. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, prolonged stability and strict no-GL release evidence remain open. Run a fresh strict GL/authority inventory and controlled live parity now that the render-debug veto is empty. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-02

Commit `b0e0973898` captures and replays every reachable Develop **Texel
Density** face draw. Its current/desired/full modes still perform their exact
texture lookup, checkerboard bind and texture-matrix side effects in source.
The bound production `gDebugProgram`, however, consumes only position and the
current uniform color, so native presentation replays that actual visible
zero-copy indexed overlay without inventing a texture/UV shader path.

Replay preserves live transforms, arbitrary finite current color, u16/u32
indices, disabled culling, fill raster, source-over blend, RGB-only writes and
writable LEQUAL depth under forward/reverse Z. HUD faces remain in native
ordered indexed UI capture. Validation rejects invalid storage, buffers,
ranges, offsets, index width, transforms, values, state/order and all cross-
owner collisions.

RelWithDebInfo, Debug and Release physical Metal suites each pass 51,784
assertions in 52 cases under Metal API/load-action validation. Warnings-as-errors
production integration passes, and the neutral non-manual/non-GL gate passes
484/484 in 37.42 seconds. The prior unfiltered 574-test inventory was not rerun;
its six GL-only failures and five skips remain recorded without a green claim.
Harness validation covers all 31 required files and all 24 tooling tests pass.

Source-exact Release Development build `26.4.0d68089` (`26.4.0.68089`) at
`b0e0973898da8e3a275b8a29ed72e7ce7f4e9db5` passes after a 161-step rebuild.
Deep strict signing and 349/349 ARM64 inspection pass. Executable SHA-256 is
`f5fa05484b57fd77f108838e6d5b82ffde0e8a41697116db742b39b9e9b083f7`;
packaged/source MSL is
`73955209f611038a5e4fe0305642614fefee30766dcd26fd03b7df7d3933df20`;
compiled metallib is
`fd64dfedceb9056e85090916b7e0dab2d8e17c553cc4ffe63ea1b5e67647c297`.
The executable still links OpenGL, so strict no-GL remains open.

The explicit render-debug world-output guard is now empty. The latent
pathfinding guard remains separately fail-closed. The main executable still
links OpenGL and Metal, so this is not a native-only claim.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** Develop Texel Density can retain native authority,
   and no reachable render-debug world-geometry bit remains in the explicit
   visual veto. Other uncaptured owner families remain fail-closed.
2. **Deferred critical work:** broader post/shadow parity, controlled live
   comparisons, stability/feature matrices, strict no-GL, independent review
   and qualifying M1 Max performance evidence.
3. **Priority reassessment:** move into M-009 with a fresh strict viewer-owned
   GL and presentation-authority inventory, then select the next concrete
   reachable release blocker from source evidence.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Run informational and strict GL inventories against the committed source and
classify each remaining application-owned finding by actual reachability and
presentation authority. Select the highest-value concrete M-009 blocker,
retain its exact production behavior with physical proof, and continue
controlled parity/stability work. Keep shadow pairs preference-qualified and
exact RLVa work deferred in M-013.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
