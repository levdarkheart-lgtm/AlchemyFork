# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `aba5995eac` (`feat(metal): replay wind vectors natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `aba5995eac` captures and
replays Develop **Wind Vectors** in the native opaque-post scene. The exact live
16x16 simulator lattice is retained: 256 one-pixel green sample points and 256
red velocity line segments, with the original `mVelX`/`mVelY`,
`WIND_SCALE_HACK`, cell spacing, region/agent-altitude translation and current
camera transform. The native pass writes RGB only and preserves the inherited
forward/reverse-Z read/write depth behavior.

Wind capture is bounded at `LLWind::renderVectors()`. Missing storage,
allocation failure, odd line cardinality, a line-to-point ratio other than 2:1,
overflow or non-finite input invalidates native publication instead of silently
dropping the overlay. Empty/no-owner input is an exact no-op. The packed native
buffer and dedicated LDR/HDR pipelines render after reflection-probe volume
visualization and before the far-plane overlay, preserving the observed order.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD **Highlight Transparent**, **Show Updates to
Objects** blips, the Develop far-plane projection overlay and manual reflection-
probe volumes. The current OSS ARM64 viewer links only the nonfunctional
`llphysicsextensionsos` pathfinding stub, so pathfinding visualization is not a
reachable retained-Alchemy output or a release-critical port target. The
render-debug guard now contains the 19 modes that still emit uncaptured world
pixels or alter captured world geometry. Nine text-only and four unused masks
remain eligible for native publication; the remaining 19 visual modes and the
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

- Focused physical RelWithDebInfo Wind/world replay passed 1,559 assertions
  under Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,215 assertions across 52 cases under the same validation layers.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed. Debug and Release Metal renderer, built-in MSL and
  GPU tests compiled and linked.
- The Wind change passed a warnings-as-errors RelWithDebInfo production viewer
  build. `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8` passed 484/484 in
  41.46 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68051`
  linked, staged and signed at committed code head `aba5995eac`.
- Executable SHA-256:
  `9f75bdaf63e35aa5dbc63e927634aa466f841e81292a94b404c50e072d32344f`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 19 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 19-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Wind Vectors no longer requires GL
world presentation, while every other uncaptured visual debug setting remains
fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full
feature and stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** implement **Agent Target**, the next smallest
bounded visual owner. Its sole production consumer draws four fixed-size
colored crosshairs for the self avatar; Raycast remains broader because it spans
octree, object/intersection and particle-crosshair paths.

## Exact next actions

1. Capture and replay Develop **Agent Target** as the four self-avatar
   crosshairs; retain the other 18 visual debug bits and pathfinding fail-closed.
2. Prove owner filtering, transform, color, size, depth/order and empty/off
   controls on the physical M1 Max before removing
   `RENDER_DEBUG_AGENT_TARGET` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
