# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `ad3970530d` (`feat(metal): replay avatar collision volumes`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `ad3970530d` captures and
replays Develop **Collision Skeleton** from the exact centralized avatar
collision-volume owner. An outer transaction limits the shared
`render_sphere_and_line()` helper to this owner and requires exactly two
line-plus-sphere pairs per collision volume. Each pair retains the live line
MVP, exact post-scale sphere MVP, end vector, visible/occluded colors, source
scale and traversal order. Capture validates RGB-only alpha blend, no cull,
visible `LEQUAL` writable depth, occluded `GREATER` read-only depth and rejects
partial or count-mismatched traversal.

The native owner builds one immutable LLRenderSphere-compatible 20x30 mesh
(651 float4 vertices and 3,600 uint16 indices) and reuses the established
constant-colour pipeline. For each source pair it encodes the visible line and
sphere with writable depth, then the occluded line and sphere with complementary
read-only depth; reverse-Z uses `GEQUAL` and `LESS`. Validation enforces the
1.0/0.05 shell/dot alternation, ordinary blue/pastel-blue or control-avatar
red/pastel-red shell colors, white dot colors, shared pair transforms and exact
cross-owner source ordering. Destination alpha remains unchanged.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay, manual reflection-probe volumes and
Collision Skeleton. The
current OSS ARM64 viewer links
only the nonfunctional `llphysicsextensionsos` pathfinding stub, so pathfinding
visualization is not a reachable retained-Alchemy output or release-critical port
target. Nine text-only and four unused render-debug masks remain native-eligible.
The explicit visual veto now contains these 8 modes:

- BBOXES
- OCTREE
- OCCLUSION
- RAYCAST
- AVATAR_JOINTS
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

- Focused physical RelWithDebInfo world-owner case passed 3,907 assertions under
  Metal API/load-action validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  47,675 assertions across 52 cases under the same validation layers. Warm
  elapsed times were 1.17, 1.40 and 1.19 seconds respectively. Offline MSL
  compiled in all three configurations.
- Tests prove exact shell/dot scale and colors, pair transform agreement, alpha
  preservation, forward/reverse visible writable depth, forward/reverse occluded
  read-only depth, invalid input rejection and explicit cross-owner source order.
  No new MSL entry point was required.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed across 127 cacheable tasks. Debug and Release Metal
  renderer, built-in MSL and GPU tests compiled and linked.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8` passed 484/484 in 41.47 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at committed code head `ad3970530d` with
  0 hits across 127 cacheable compilation tasks and produced Development build
  `26.4.0d68073` (`26.4.0.68073`).
- Executable SHA-256:
  `f14c2d56eb3e3bb82d055c1fabdce935832c32d606b64bf21be97ef493b3ca15`.
  Packaged/source MSL SHA-256:
  `3c918952dd66d3122d683d478020e13d99e0485508e95ce9d0245dc0f13bf393`;
  compiled metallib SHA-256:
  `5e56cdd100abf8e6c6a40d666a9d80a9bcc5f3ae4b6a204a6f5d0eeba1c156c1`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 8 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 8-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Collision Skeleton no longer requires GL
world presentation, while every other uncaptured visual debug setting remains
fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Independent build/review evidence and reproducible distinct
release proof also remain open. Exact RLVa behavior/image parity is separately
deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_AVATAR_JOINTS` next. Bounded
source inspection shows one world call to `renderBones()`. Its skeleton segment
loop reuses the now-native line-plus-sphere primitive; the remaining finite
output is yellow joint-space rigged bounds and cyan world-space rigged attachment
face bounds. The world transaction must stay separate from model-preview calls.
This is narrower and has more proven reusable infrastructure than the remaining
octree, occlusion, bounding-box, raycast, physics, normal and texel-density
owners.

## Exact next actions

1. Capture and replay Develop **Joints** only around the spatial
   `renderBones()` call, preserving skeleton transforms, override/rigged/default
   colors, 0.001 sphere scale and visible/occluded depth order.
2. Capture the yellow joint-space rigged bounds and cyan world-space rigged
   attachment-face bounds; prove empty controls, forward/reverse depth, ordering
   and invalid-input rejection on the physical M1 Max before removing only
   `RENDER_DEBUG_AVATAR_JOINTS` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
