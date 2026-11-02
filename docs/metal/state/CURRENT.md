# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `e84d5e660b` (`feat(metal): replay avatar joint debug output`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `e84d5e660b` captures and
replays Develop **Joints** from the exact spatial-world `renderBones()` owner.
An outer transaction keeps model-preview calls independent and counts every
non-null skeleton joint before traversal. The existing shared
`render_sphere_and_line()` hook routes records to Collision Skeleton or Joints
only while the corresponding owner transaction is active.

Each joint-segment record retains the exact line MVP, post-scale sphere MVP,
endpoint, visible/occluded colors, source scale and traversal order. Native
replay reuses the immutable LLRenderSphere-compatible mesh and exact
line-plus-sphere visible/occluded encoder. It preserves the ordinary 0.001
sphere scale, visible gray, and red/cyan/green occluded color classes; the
selected 0.016 yellow/gray source grammar is also validated. Visible output
writes depth and occluded output is complementary and read-only in forward and
reverse Z.

The same owner captures yellow joint-space rigged bounds and cyan world-space
rigged attachment-face bounds at their exact source sites. These records retain
the live MVP, center, half extent and order, then replay through the proven
24-vertex box-outline pipeline with writable depth, no cull and RGB-only output.
Validation rejects missing storage, count mismatch, non-finite data, invalid
kind/scale/color/extent, owner-order collision and allocation failure. Joints
participates in the established exact global debug-order merge. Destination
alpha remains unchanged; no new MSL entry point was required.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay, manual reflection-probe volumes, Collision
Skeleton and Joints. The current OSS ARM64 viewer links only the nonfunctional
`llphysicsextensionsos` pathfinding stub, so pathfinding visualization is not a
reachable retained-Alchemy output or release-critical port target. Nine
text-only and four unused render-debug masks remain native-eligible. The
explicit visual veto now contains these 7 modes:

- BBOXES
- OCTREE
- OCCLUSION
- RAYCAST
- PHYSICS_SHAPES
- NORMALS
- TEXEL_DENSITY

The separate latent pathfinding guard also remains fail-closed.

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
off/on pair is invalid; a both-off control can isolate a residual but cannot prove
shadow parity.

## Build and test evidence

- Focused physical RelWithDebInfo world-owner case passed 4,339 assertions under
  Metal API/load-action validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  48,107 assertions across 52 cases under the same validation layers. Warm
  elapsed times were 1.21, 1.44 and 1.20 seconds respectively. Offline MSL
  compiled in all three configurations.
- Tests prove null/non-finite/invalid-kind/scale/color/extent rejection, explicit
  Joints-vs-Agent-Target source ordering with distinct pixels, independent bounds
  rasterization, alpha preservation, and forward/reverse visible writable and
  occluded read-only depth.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed across 127 cacheable tasks. Debug and Release Metal
  renderer, built-in MSL and GPU tests compiled and linked.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8` passed 484/484 in 35.56 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at committed code head `e84d5e660b` with
  0 hits across 127 cacheable compilation tasks and produced Development build
  `26.4.0d68075` (`26.4.0.68075`).
- Executable SHA-256:
  `6234111e7e6f7a688389cc6e20de3d4ebb60ce6691d224fc0105a76d4e1acb80`.
  Packaged/source MSL SHA-256:
  `3c918952dd66d3122d683d478020e13d99e0485508e95ce9d0245dc0f13bf393`;
  compiled metallib SHA-256:
  `5e56cdd100abf8e6c6a40d666a9d80a9bcc5f3ae4b6a204a6f5d0eeba1c156c1`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified all 349
  Mach-O slices as ARM64, and the main executable reports arm64.
- `tools/metal/doctor.sh` passed on Apple M1 Max with 64 GiB unified memory and
  Apple7-family Metal support. Harness validation and tooling tests pass after
  this documentation checkpoint.
- The main executable still links both Metal and OpenGL during migration.
  Strict no-GL remains an open release gate; no native-only claim is made.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-007 world/material | doing | Bounded active transparency/material and reflection-probe audits found no valid-production divergence; broader live parity remains open. |
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 7 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 7-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Joints no longer requires GL
world presentation, while every uncaptured visual debug setting remains
fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Independent build/review evidence and reproducible distinct
release proof also remain open. Exact RLVa behavior/image parity is separately
deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_BBOXES` next. Bounded source
comparison of all seven remaining owners shows it is the smallest: deterministic
spatial-group, drawable and per-face extents routed through one existing box-
outline primitive plus finite color classes. Raycast also requires volume-octree
triangle traversal and hit-orientation markers; Octree/Occlusion add time-varying
or geometry-buffer replay; Physics Shapes, Normals and Texel Density are broader.

## Exact next actions

1. Capture and replay spatial-group, drawable and per-face box outlines at their
   exact traversal sites, preserving bridge/root/object color classes, transforms
   and active-object line width.
2. Prove bridge transforms, empty controls, forward/reverse depth, exact source
   order, alpha preservation and invalid-input rejection on the physical M1 Max
   before removing only `RENDER_DEBUG_BBOXES` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
