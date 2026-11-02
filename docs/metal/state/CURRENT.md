# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `0ac23c10b1` (`feat(metal): replay update type debug overlay`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `0ac23c10b1` captures and
replays Develop **Update Type** in the native opaque-post scene. Drawables with
a known last simulator update class replay each valid face's indexed triangles:
full is green, terse-improved cyan, compressed cached red, compressed uncached
yellow and full-cached blue, all at alpha `0.5`. The source keeps the
position-only, non-rigged `gDebugProgram` bound, so the native contract reuses
the face's existing position/index buffers and current transform without
texture or skin-palette input.

Capture requires complete mirrored buffers, triangle cardinality, aligned and
in-range offsets, one of the five exact colors, enabled blend, disabled culling,
RGB-only writes and inherited forward/reverse `LEQUAL` read/write depth.
Non-finite transforms, invalid color/storage/order and allocation failure fail
closed; unknown/off/no-face input is an exact no-op. Update Type is merged with
Points, Agent Target and Texture Anim by the original traversal order. HUD-only
output remains in the established uniform-color capture transaction while the
original GL draw stays unchanged during migration.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD **Highlight Transparent**, **Show Updates to
Objects** blips, Wind Vectors, Agent Target, Points, Composition, reflection-
probe neighbor links, Texture Anim, Update Type, the Develop far-plane projection overlay and manual
reflection-probe volumes. The current OSS ARM64 viewer links only the nonfunctional
`llphysicsextensionsos` pathfinding stub, so pathfinding visualization is not a
reachable retained-Alchemy output or a release-critical port target. The
render-debug guard now contains the 13 modes that still emit uncaptured world
pixels or alter captured world geometry. Nine text-only and four unused masks
remain eligible for native publication; the remaining 13 visual modes and the
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

- Focused physical RelWithDebInfo Update Type/world replay passed 2,293
  assertions under Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,951 assertions across 52 cases under the same validation layers. Measured
  elapsed times were 5.94, 6.18 and 5.94 seconds respectively.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed. Debug and Release Metal renderer, built-in MSL and
  GPU tests compiled and linked.
- The Update Type change passed a warnings-as-errors RelWithDebInfo production
  viewer build. `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8` passed 484/484 in
  35.57 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68063`
  linked, staged and signed at committed code head `0ac23c10b1`.
- Executable SHA-256:
  `2cbd61d22685d650f66861d9e2cdefc22d30156bdf2caf7345454f8631525ab6`.
  Packaged/source MSL SHA-256:
  `fc55ef6f5161d51eb6ce8b8ee8bfb6a9316c9752f5262592b8254fc3b06701fa`;
  compiled metallib SHA-256:
  `4e9ab857f3a312a7ddf8b5fd13982cd4c4037b6525e8631e331edf323a14d7e1`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 13 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 13-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Update Type no longer requires
GL world presentation, while every other uncaptured
visual debug setting remains fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full
feature and stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_BATCH_SIZE` next. Source
inspection shows another draw-info triangle replay over already mirrored
buffers, but its exact contract additionally includes `getDebugColor()`,
polygon offset and rigid/skinned variants with the source matrix palette. Those
differences make it the next direct-geometry owner rather than a generated-box,
octree, raycast, avatar-skeleton, physics, normals or texel-density rewrite.

## Exact next actions

1. Capture and replay Develop **Batch Size** draw-info triangles with the exact
   debug color, polygon offset, static/rigged transform and palette contract;
   retain the other 12 visual debug bits and pathfinding fail-closed afterward.
2. Prove static and skinned geometry, color/range/order, forward/reverse depth
   plus offset, empty/off identity and invalid-input rejection on the physical
   M1 Max before removing `RENDER_DEBUG_BATCH_SIZE` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
