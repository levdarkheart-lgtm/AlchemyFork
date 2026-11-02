# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `c9f600b12f` (`feat(metal): replay shadow frusta debug output`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `c9f600b12f` captures and
replays Develop **Shadow Frusta** from the exact centralized source block. The
capture retains four optional cascade point sets, each cascade's 24-vertex
extent box and 24-vertex camera outline, and the conditional 10/4/4-vertex
side/near/far volume strips in source order. It validates the live RGB-only
alpha-blend, no-cull, one-pixel line/point, read-only `LEQUAL` depth state and
fails the frame closed on state, topology, finite-data, overflow or allocation
disagreement.

The native owner reuses the established debug vertex/fragment pair, with exact
red/green/blue/magenta cascade colors, opaque points/lines, alpha-0.1 volume
strips, eight-pixel cascade points and destination-alpha preservation. It uses
forward/reverse read-only depth and encodes after reflection-probe debug output
but before Wind Vectors, matching source order. Validation requires the exact
batch grammar and conditional volume presence; no-owner output remains identical.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay and manual reflection-probe volumes. The
current OSS ARM64 viewer links
only the nonfunctional `llphysicsextensionsos` pathfinding stub, so pathfinding
visualization is not a reachable retained-Alchemy output or release-critical port
target. Nine text-only and four unused render-debug masks remain native-eligible.
The explicit visual veto now contains these 9 modes:

- BBOXES
- OCTREE
- OCCLUSION
- RAYCAST
- AVATAR_VOLUME
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

- Focused physical RelWithDebInfo world-owner case passed 3,744 assertions under
  Metal API/load-action validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  47,512 assertions across 52 cases under the same validation layers. Warm
  elapsed times were 2.49, 2.70 and 2.55 seconds respectively. Offline MSL
  compiled in all three configurations.
- Tests prove exact topology and colors, conditional volume presence, alpha
  blending with destination alpha retained, forward/reverse read-only depth,
  no-owner color/depth identity, invalid storage/data/topology rejection and the
  shadow-before-wind overwrite order. No new MSL entry point was required.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed across 127 cacheable tasks. Debug and Release Metal
  renderer, built-in MSL and GPU tests compiled and linked.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8` passed 484/484 in 35.42 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at committed code head `c9f600b12f` after
  127 cacheable compilation tasks and produced Development build
  `26.4.0d68071`.
- Executable SHA-256:
  `a32a43c92176b4873ccec625e951f68079c60eb67aeb49feddf76a29f2428c42`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 9 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 9-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Shadow Frusta no longer requires GL
world presentation, while every other uncaptured visual debug setting remains
fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Independent build/review evidence and reproducible distinct
release proof also remain open. Exact RLVa behavior/image parity is separately
deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_AVATAR_VOLUME` next. Bounded
source inspection shows a centralized finite owner: for each collision volume,
two line-plus-sphere pairs share the joint world matrix and switch between the
visible `LEQUAL` pass and occluded `GREATER` read-only pass. It is narrower than
the remaining octree, occlusion, bounding-box, raycast, avatar-joint, physics,
normal and texel-density owners.

## Exact next actions

1. Capture and replay Develop **Collision Skeleton** from the centralized avatar
   collision-volume owner, preserving joint transforms, end vectors, two sphere
   scales, control-avatar colors and visible/occluded pass order and depth state.
2. Prove empty controls, ordinary/control-avatar colors, forward/reverse visible
   and occluded depth, ordering and invalid-input rejection on the physical M1
   Max before removing only `RENDER_DEBUG_AVATAR_VOLUME` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
