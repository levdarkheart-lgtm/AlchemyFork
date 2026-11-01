# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `0d8b5ccdda` (`feat(metal): replay agent target natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `0d8b5ccdda` captures and
replays Develop **Agent Target** in the native opaque-post scene. The sole
production owner remains self-avatar-only and retains the four original 0.2 m
crosshairs: agent position red, drawable position green, root position white
and pelvis position blue, each with alpha 0.8. The native LDR/HDR pipelines use
source-alpha RGB blending without writing alpha and preserve the inherited
forward/reverse-Z depth reads and writes.

Capture is bounded around the four original draws at `renderAgentTarget()` and
accepts exactly 24 line vertices with one transform. Duplicate or partial
traversal, missing storage, overflow, allocation failure, invalid inherited GL
state or non-finite input invalidates native publication instead of silently
dropping the overlay. Empty/no-owner input is an exact no-op. Native encoding
runs after object-update blips and before reflection-probe-volume visualization,
matching observed GL order.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD **Highlight Transparent**, **Show Updates to
Objects** blips, Wind Vectors, Agent Target, the Develop far-plane projection
overlay and manual reflection-probe volumes. The current OSS ARM64 viewer links
only the nonfunctional
`llphysicsextensionsos` pathfinding stub, so pathfinding visualization is not a
reachable retained-Alchemy output or a release-critical port target. The
render-debug guard now contains the 18 modes that still emit uncaptured world
pixels or alter captured world geometry. Nine text-only and four unused masks
remain eligible for native publication; the remaining 18 visual modes and the
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

- Focused physical RelWithDebInfo Agent Target/world replay passed 1,998
  assertions under Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,654 assertions across 52 cases under the same validation layers.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed. Debug and Release Metal renderer, built-in MSL and
  GPU tests compiled and linked.
- The Agent Target change passed a warnings-as-errors RelWithDebInfo production
  viewer build. `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8` passed 484/484 in
  35.57 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68053`
  linked, staged and signed at committed code head `0d8b5ccdda`.
- Executable SHA-256:
  `ac11e94f7bd088c26fc1df2d2f81ca37b004af471c5021922c8f89b3fff8879f`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 18 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 18-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Agent Target no longer requires GL
world presentation, while every other uncaptured visual debug setting remains
fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full
feature and stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** implement Develop **Points** next. Its single
production drawing function emits one white point at each drawable face's local
center with depth explicitly disabled. It traverses region partitions and
visible spatial bridges, so capture must preserve each traversal transform;
Raycast remains broader because it spans octree, object/intersection and
particle-crosshair paths.

## Exact next actions

1. Capture and replay Develop **Points** as one white point per valid drawable
   face center across region partitions and visible spatial bridges; retain the
   other 17 visual debug bits and pathfinding fail-closed.
2. Prove face filtering, per-traversal transform, color, point size, disabled
   depth behavior and empty/off controls on the physical M1 Max before removing
   `RENDER_DEBUG_POINTS` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
