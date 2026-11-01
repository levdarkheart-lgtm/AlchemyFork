# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `93dbcc131f` (`feat(metal): replay drawable centre points natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `93dbcc131f` captures and
replays Develop **Points** in the native opaque-post scene. It retains the
original one-pixel opaque-white point at each valid `LLFace::mCenterLocal`,
with each region-partition or visible-spatial-bridge model-view-projection
transform preserved. The original GL path remains during migration.

Capture is bounded around the original `renderPoints(LLDrawable*)` owner and
coalesces only consecutive batches with the same transform. Publication
requires the inherited blend, RGB-only color-mask, one-pixel point-size and
disabled depth-read/write state. Missing or mismatched storage, invalid ranges,
overflow, allocation failure, duplicate/non-monotonic ordering, a collision
with Agent Target ordering or non-finite input invalidates native publication
instead of silently dropping the overlay. Empty/no-owner input is an exact
no-op. Native encoding merges Points batches and Agent Target by captured
traversal order before reflection-probe-volume visualization, matching their
observed cross-owner order.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD **Highlight Transparent**, **Show Updates to
Objects** blips, Wind Vectors, Agent Target, Points, the Develop far-plane
projection overlay and manual reflection-probe volumes. The current OSS ARM64 viewer links
only the nonfunctional
`llphysicsextensionsos` pathfinding stub, so pathfinding visualization is not a
reachable retained-Alchemy output or a release-critical port target. The
render-debug guard now contains the 17 modes that still emit uncaptured world
pixels or alter captured world geometry. Nine text-only and four unused masks
remain eligible for native publication; the remaining 17 visual modes and the
latent pathfinding guard remain fail-closed.

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

- Focused physical RelWithDebInfo Points/world replay passed 2,053
  assertions under Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,709 assertions across 52 cases under the same validation layers.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed. Debug and Release Metal renderer, built-in MSL and
  GPU tests compiled and linked.
- The Points change passed a warnings-as-errors RelWithDebInfo production
  viewer build. `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8` passed 484/484 in
  35.42 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68055`
  linked, staged and signed at committed code head `93dbcc131f`.
- Executable SHA-256:
  `13da93dd728e38b406a3714c040ae32f32547aba875344044916c9f8de79a3ca`.
  Packaged/source MSL SHA-256:
  `a33ec7160965a1c6805ad5b2a2f69d95da8bff630cf2b05c346a57deb13e2033`;
  compiled metallib SHA-256:
  `537685a0c488cded37fc64b7a532f57610771deeaf9f17d6ae9c1772787549b1`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 17 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 17-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Points no longer requires GL
world presentation, while every other uncaptured visual debug setting remains
fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full
feature and stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** implement Develop **Composition** next. Its sole
production owner emits one fixed 261-by-261 point grid from the current region,
blue for coordinates 0..255 and red for the five-coordinate border, with height
`composition * 5 + 50`. It is bounded compared with remaining per-object
geometry, variable-width-line and multi-primitive debug owners.

## Exact next actions

1. Capture and replay Develop **Composition** as its fixed 261-by-261 current-
   region point grid; retain the other 16 visual debug bits and pathfinding
   fail-closed.
2. Prove coordinate bounds, red/blue partitioning, exact height transform,
   point size/state, no-region and off controls on the physical M1 Max before
   removing `RENDER_DEBUG_COMPOSITION` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
