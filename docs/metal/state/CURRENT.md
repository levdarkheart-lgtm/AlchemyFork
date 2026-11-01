# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `9e28d64932` (`fix(metal): narrow debug fallback to world output`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `23ce2f7190` makes the global
viewer `gUseWireframe` state a native Metal world-presentation owner instead of
an eligibility veto. `LLPipeline::MetalWorldSimpleCapture` snapshots the live
state for main and reflection-probe captures; impostor baking remains explicitly
filled, matching the distinct upstream ownership boundary.

The Metal encoder now exposes triangle fill/line raster state. Wireframe applies
to the same geometry families as the GL excursion: deferred G-buffer draw-pool
geometry, sky geometry, water surfaces and patches, alpha/forward geometry,
glow, and opaque/fullbright post-deferred geometry. Full-screen water and
atmospheric haze, coverage seeding, water exclusion, Highlight Transparent,
object-update blips, reflection-probe-volume composite and far-plane overlay
remain filled. Fill state is explicitly restored before those later owners so
wireframe cannot leak across their ordering boundaries.

The wireframe G-buffer preserves GL's raw gray clear. Attachment zero is sRGB,
so Metal uses linear `0.21586050011389926`, the sRGB decode of byte 128/255,
while the linear attachments use 0.5. A physical focused test rejected the
initial mathematical-0.5 decode because finite conversion stored byte 127; the
corrected path stores byte 128 exactly. The `gUseWireframe` veto was removed only
after line-only coverage, reduced coverage, clear depth, exact clear bytes,
post-deferred line raster and filled diagnostic ownership passed on the M1 Max.

Prior native advanced-output owners remain covered: static/rigged world and HUD
**Highlight Transparent**, **Show Updates to Objects** blips, the Develop
far-plane projection overlay and manual reflection-probe volumes. The current
OSS ARM64 viewer links only the nonfunctional `llphysicsextensionsos`
pathfinding stub, so pathfinding visualization is not a reachable retained-
Alchemy output or a release-critical port target. The render-debug audit at
`9e28d64932` replaces the broad `mRenderDebugMask != 0` veto with an explicit
mask of the 20 modes that still emit uncaptured world pixels or alter captured
world geometry. Nine modes that only feed LLHUD/debug text after native world
publication and four unused masks no longer force a GL world fallback. The 20
visual modes remain fail-closed; the latent pathfinding guard also remains.

The preceding reflection-probe production-call audit found no valid viewer-
produced divergence; its extra Metal bounds checks do not alter valid data. That
bounded audit remains closed without a speculative edit.

Exact RLVa parity is owner-deferred M-013 nice-to-have. Implement only enough
RLVa behavior to avoid blocking Metal authority and viewer-owned OpenGL removal.
The persistent inventory is [the deferred RLVa backlog](../work/deferred/rlva-parity.md).
Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both viewers before testing.

Any candidate/reference shadow comparison is admissible only when shadows are
enabled in Graphics Preferences in **both** viewers before capture. A mismatched
off/on pair is invalid; a both-off control can isolate a residual but cannot
prove shadow parity.

## Build and test evidence

- Focused physical RelWithDebInfo G-buffer replay passed 1,491 assertions under
  Metal API/GPU/load-store/shader validation. Focused exact deferred/sky replay
  passed 1,927 assertions under the same validators.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,147 assertions across 52 cases under the same validation layers.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed. Debug and Release Metal renderer, built-in MSL and
  GPU tests compiled and linked.
- The render-debug eligibility change passed a warnings-as-errors RelWithDebInfo
  production viewer build. `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8` passed 484/484 in
  35.39 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68049`
  linked, staged and signed at committed code head `9e28d64932`.
- Executable SHA-256:
  `f8dd26047e89afd0aca67de34de3eb64a2bc2ae986caad4f7d189288c14c7691`.
  Packaged/source MSL SHA-256:
  `dd51886cc8cc33a8b19ac69206fed5045421c8905bb51c7d596fdeaefb467214`;
  compiled metallib SHA-256:
  `b5ccb104ec22fade3cf1822595924c751128b6ca14e0ab188276890339357e58`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified all 349
  Mach-O slices as ARM64, and the main executable reports arm64.
- `tools/metal/doctor.sh` passed on Apple M1 Max with unified memory and Apple7-
  family Metal support. Harness validation and tooling tests pass after this
  documentation checkpoint.
- The main executable still links both Metal and OpenGL during migration.
  Strict no-GL remains an open release gate; no native-only claim is made.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-007 world/material | doing | Bounded active transparency/material and reflection-probe audits found no valid-production divergence; broader live parity remains open. |
| M-008 advanced views/effects | doing | Wireframe, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 20 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 20-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Native authority is no longer rejected
for debug settings that cannot change the world surface, while every setting
that can change world pixels remains fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full
feature and stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** implement **Wind Vectors**, the smallest bounded
remaining visual owner: one fixed 16x16 lattice, with 256 one-pixel green points
and 256 red line segments sourced from the live simulator wind field. Preserve
its camera/depth/order state and remove only that bit after asymmetric proof.

## Exact next actions

1. Capture and replay Develop **Wind Vectors** as bounded Metal point/line
   primitives; retain the other 19 visual debug bits and pathfinding fail-closed.
2. Prove count, transform, color, depth/order and empty/off controls on the
   physical M1 Max before removing `RENDER_DEBUG_WIND_VECTORS` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
