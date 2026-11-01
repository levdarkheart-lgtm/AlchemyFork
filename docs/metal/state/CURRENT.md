# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `b76c52576d` (`feat(metal): replay reflection probe neighbor links`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `b76c52576d` captures and
replays the active Develop **Reflection Probes** neighbor links in the native
opaque-post scene. Only relevant probes emit links. Each neighbor appears
exactly once: orange when either endpoint lacks a viewer object and yellow when
both endpoints own viewer objects. The older node-volume branch is compiled
out and is not replayed.

Capture is bounded to two compact position/color vertices per source link,
retains the current transform and source order, and requires the inherited
blend, RGB-only mask, one-pixel line width and forward/reverse read/write depth
state. Count overflow, missing or odd storage, allocation failure, non-finite
data/transform, invalid homogeneous position or any color outside the exact
orange/yellow contract invalidates native publication. An empty/off setting is
an exact no-op. Native encoding follows spatial Points/Agent Target traversal
and precedes the reflection-probe-volume display, matching the active GL owner
order.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD **Highlight Transparent**, **Show Updates to
Objects** blips, Wind Vectors, Agent Target, Points, Composition, reflection-
probe neighbor links, the Develop far-plane projection overlay and manual
reflection-probe volumes. The current OSS ARM64 viewer links only the nonfunctional
`llphysicsextensionsos` pathfinding stub, so pathfinding visualization is not a
reachable retained-Alchemy output or a release-critical port target. The
render-debug guard now contains the 15 modes that still emit uncaptured world
pixels or alter captured world geometry. Nine text-only and four unused masks
remain eligible for native publication; the remaining 15 visual modes and the
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

- Focused physical RelWithDebInfo reflection-link/world replay passed 2,162
  assertions under Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,818 assertions across 52 cases under the same validation layers. Measured
  elapsed times were 5.95, 6.22 and 5.90 seconds respectively.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed. Debug and Release Metal renderer, built-in MSL and
  GPU tests compiled and linked.
- The reflection-link change passed a warnings-as-errors RelWithDebInfo production
  viewer build. `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8` passed 484/484 in
  35.39 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68059`
  linked, staged and signed at committed code head `b76c52576d`.
- Executable SHA-256:
  `8460b7356784d39d0c1163a4b157e9f9bc25faf85a53c31cc84aeba99dec53e1`.
  Packaged/source MSL SHA-256:
  `d05669c80b490283e9376fb797a48715bb079bc1928022884193130149643a21`;
  compiled metallib SHA-256:
  `994eaa49f2c573361c5951c443165ce63c6919b44c032fb6d4e027358dbbcd1b`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 15 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 15-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Reflection Probes neighbor
links no longer require GL world presentation, while every other uncaptured
visual debug setting remains fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full
feature and stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_TEXTURE_ANIM` next. Its
sole active output is a flat translucent-yellow replay of visible draw-info
triangles whose texture matrix is non-null. Unlike the remaining box, octree,
raycast, avatar, physics, normals, texel-density and variable-width owners, it
adds no separate generated primitive family and can reuse the bounded world
geometry capture contract.

## Exact next actions

1. Capture and replay Develop **Texture Anim** flat triangle overlays only for
   visible draw infos with a non-null texture matrix; retain the other 14
   visual debug bits and pathfinding fail-closed.
2. Prove exact yellow/0.5-alpha source semantics, static and rigged geometry,
   traversal/transform, inherited state, depth interaction and empty/off
   controls on the physical M1 Max before removing
   `RENDER_DEBUG_TEXTURE_ANIM` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
