# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `27c0bec87c` (`feat(metal): replay batch size debug overlay`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `27c0bec87c` captures and
replays Develop **Batch Size** in the native opaque-post scene. Each valid draw
info retains its existing position/index buffers, index range, current transform,
stable hash-derived debug color and source traversal order. The exact source
polygon offset `(factor=-1, units=+1)` is mapped to forward/reverse Metal depth
bias. Static and object-skinned avatar variants are supported; the latter retains
the current `4 + joint_count*12` float matrix palette with the source 110-joint
limit and `TYPE_WEIGHT4` weights.

Capture requires complete mirrored storage, triangle cardinality, aligned and
in-range offsets, exact normalized hash color with alpha byte 200, source-alpha
RGB blending with destination alpha retained, disabled culling, polygon offset,
and inherited forward/reverse `LEQUAL` read/write depth. Invalid storage, color,
palette, transform, state or order fails closed. Batch Size is merged with Points,
Agent Target, Texture Anim and Update Type by original traversal order; HUD-only
static and rigged output uses explicit untextured debug shader families and the
same semantic depth bias. Generic UI draws reset depth bias so state cannot leak.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, the Develop far-plane projection overlay
and manual reflection-probe volumes. The current OSS ARM64 viewer links only the
nonfunctional `llphysicsextensionsos` pathfinding stub, so pathfinding
visualization is not a reachable retained-Alchemy output or release-critical port
target. Nine text-only and four unused render-debug masks remain native-eligible.
The explicit visual veto now contains these 12 modes:

- BBOXES
- OCTREE
- OCCLUSION
- LIGHTS
- RAYCAST
- SHADOW_FRUSTA
- AVATAR_VOLUME
- AVATAR_JOINTS
- PHYSICS_SHAPES
- NORMALS
- TEXEL_DENSITY
- IMPOSTORS

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

- Focused physical RelWithDebInfo Batch Size world replay passed 2,330 assertions;
  focused UI replay passed 521 assertions under Metal API/GPU/load-store/shader
  validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  46,028 assertions across 52 cases under the same validation layers. Warm
  elapsed times were 1.21, 1.39 and 1.13 seconds respectively.
- Tests prove exact hash color and alpha blending, RGB-only alpha preservation,
  no-cull reverse winding, forward/reverse depth bias, rigged palette movement,
  both order directions against Update Type, collision rejection, invalid
  storage/color/palette/state rejection, and no-owner identity. HUD controls
  additionally prove BGRA byte order and untextured static/rigged publication.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed. Debug and Release Metal renderer, built-in MSL and
  GPU tests compiled and linked.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8` passed 484/484 in 35.40 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at committed code head `27c0bec87c` after
  1,094 cacheable compilation tasks and produced Development build
  `26.4.0d68065`.
- Executable SHA-256:
  `ad71189d24bfb9e250eecf31485aad44ff0f11ca7e1100ca70e4039b196a82e6`.
  Packaged/source MSL SHA-256:
  `4331aad8f440c6d92473a5d998bf60121521868ba47186982b87524a2cfc0764`;
  compiled metallib SHA-256:
  `4d2140cf789ba313ba6cd5aff1bda7f39ea12e46c661f88379b01c4fc99c4475`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 12 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 12-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Batch Size no longer requires GL
world presentation, while every other uncaptured visual debug setting remains
fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Independent build/review evidence and reproducible distinct
release proof also remain open. Exact RLVa behavior/image parity is separately
deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_LIGHTS` next. Bounded source
inspection shows a compact reachable owner: only light drawables emit their face
triangles in translucent cyan, followed by a white spatial-extents outline with
depth testing disabled and a yellow light-radius outline. This is narrower than
the remaining octree/occlusion, raycast, shadow-frustum, avatar-skeleton,
physics, normals, texel-density and thick-line impostor contracts.

## Exact next actions

1. Capture and replay Develop **Lights** face triangles plus the two generated
   box outlines with exact source color, depth, blend, transform and order.
2. Prove triangle/range/order, both outline extents, forward/reverse depth,
   empty/non-light identity and invalid-input rejection on the physical M1 Max
   before removing only `RENDER_DEBUG_LIGHTS` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
