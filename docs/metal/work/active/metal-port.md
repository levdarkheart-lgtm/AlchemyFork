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
| M-008 | Advanced views/effects and compute | doing | Wireframe, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. OSS pathfinding is a nonfunctional stub. Continue reachable render-debug/shadows/post/output parity. |
| M-009 | Feature parity, stability and native-only cleanup | todo | Full matrix, prolonged stability and strict no-GL release evidence remain open. General render-debug output stays fail-closed pending audit/porting. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements must follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | deferred | Resume only after each owner has correctness and side-effect proof; global traversal suppression remains rejected. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Exact-path semantic login/in-world/control/logout is verified and used for current acceptance. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; keep only enough native implementation to avoid blocking Metal/no-GL cutover. Exact inventory and restart-qualified prerequisite: [RLVa backlog](../deferred/rlva-parity.md). |

States describe evidence, not effort. `complete` applies only to a bounded work
item; it never implies product completion.

## Active checkpoint — 2026-11-01

Commit `23ce2f7190` replaces the global `gUseWireframe` native-presentation veto
with exact Metal line raster ownership. The live flag is retained for main and
reflection-probe capture; impostor bake is explicitly filled. The encoder maps
the new fill-mode state directly to `MTLTriangleFillModeFill/Lines`.

Line fill is scoped to GL-owned wireframe geometry: deferred draw-pool G-buffer
batches; HDRI/WindLight sky, stars, heavenly bodies, meteors, aurora and clouds;
water surfaces and patches; alpha/forward, glow, opaque and fullbright post-
deferred batches. Full-screen coverage, haze, water exclusion and diagnostic
overlays explicitly restore fill. This prevents raster state from leaking into
Highlight Transparent, update blips, reflection-probe volumes or the far-plane
overlay.

Wireframe clears reproduce GL's pre-`GL_FRAMEBUFFER_SRGB` raw 0.5 G-buffer
clear. The sRGB albedo attachment uses the linear decode of byte 128/255,
`0.21586050011389926`; linear attachments use 0.5. The initial mathematical-0.5
decode stored byte 127 and was rejected by the focused physical test before the
correction. Tests now prove exact byte-128 clear, line-only and reduced coverage,
untouched clear depth, line-rendered post-deferred geometry, filled highlight
after line geometry, and line-rendered HDRI sky with untouched interior depth.

Focused physical RelWithDebInfo cases pass 1,491 and 1,927 assertions under
Metal API/GPU/load-store/shader validation. Complete RelWithDebInfo, Debug and
Release suites each pass 45,147 assertions/52 cases under the same validators.
The neutral gate passes 484/484 in 35.58 seconds. Warnings-as-errors
RelWithDebInfo production integration and the full source-exact Release wrapper
build both link, stage and sign.

Source-exact Development `26.4.0d68047` at `23ce2f7190` stages and signs. Deep
strict signing and 349/349 ARM64 inspection pass. Executable SHA-256 is
`779d8233a2d62c05de54fe527ec70143ec20d36b6c09f8d6ab75bfc3baf89b18`;
packaged/source MSL is
`dd51886cc8cc33a8b19ac69206fed5045421c8905bb51c7d596fdeaefb467214`;
compiled metallib is
`b5ccb104ec22fade3cf1822595924c751128b6ca14e0ab188276890339357e58`.
The executable still links OpenGL during migration, so strict no-GL is open.

The only remaining reachable advanced-output eligibility guard is general
render-debug output. The current OSS pathfinding implementation is a hard-
disabled stub, so its latent guard remains but does not block normal authority.

Shadow comparison prerequisite: enable shadows in Graphics Preferences in
**both** viewers before capture. A mismatched off/on pair is invalid, and a
both-off control may isolate a residual but cannot close shadow parity.

Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both. Exact RLVa correctness is deferred M-013; implement only the
minimum behavior needed to unblock Metal/no-GL cutover.

## Complete-product checkpoint gate

1. **Product advancement:** a reachable global render mode now owns line/fill
   scope, raw-clear semantics and ordered downstream state in Metal.
2. **Deferred critical work:** remaining non-RLVa advanced output/post/shadows;
   controlled live wireframe/material/editing parity; full stability/feature
   matrices; strict no-GL; independent review; and qualifying M1 Max performance
   evidence. Exact RLVa parity remains non-critical M-013.
3. **Priority reassessment:** audit general render-debug output, then implement
   the smallest confirmed missing owner without broad replay or speculative
   edits.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace production contribution and capture state for general render-debug output.
Select the smallest confirmed reachable omission, implement its exact native
state/order with asymmetric physical controls, retain the latent pathfinding
guard, and leave every other uncovered owner fail-closed.

## Non-negotiable open gates

- Do not disable OpenGL traversal before per-owner parity and side-effect proof.
- Do not mark feature parity from build/test success alone.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority or owner-only waiver rules.
