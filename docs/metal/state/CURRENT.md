# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `4b755ffc95` (`feat(metal): replay lights debug overlay`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `4b755ffc95` captures and
replays Develop **Lights** in the native opaque-post scene. Light faces retain
their existing position/index buffers, exact ranges, transforms and traversal
order and render translucent cyan with source-alpha RGB blending, destination
alpha retained, disabled culling and inherited forward/reverse `LEQUAL`
read/write depth. Each drawable then emits a procedural white spatial-extents
outline with always-pass/write depth followed by a yellow light-radius outline
with ordinary forward/reverse `LEQUAL` read/write depth.

Capture validates the exact blend factors/equations, RGB-only color mask,
culling, line width and per-kind depth state. The native procedural MSL outline
emits the same 24 box-edge vertices as `drawBoxOutline`. Invalid storage, color,
transform, range, state, order or allocation fails closed. Lights is merged with
Points, Agent Target, Texture Anim, Update Type and Batch Size by source order.
HUD face geometry uses the explicit untextured debug shader; immediate-mode HUD
box lines remain in the already ordered generic UI capture.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, the Develop far-plane projection
overlay and manual reflection-probe volumes. The current OSS ARM64 viewer links
only the nonfunctional `llphysicsextensionsos` pathfinding stub, so pathfinding
visualization is not a reachable retained-Alchemy output or release-critical port
target. Nine text-only and four unused render-debug masks remain native-eligible.
The explicit visual veto now contains these 11 modes:

- BBOXES
- OCTREE
- OCCLUSION
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

- Focused physical RelWithDebInfo Lights world replay passed 2,380 assertions
  under Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  46,079 assertions across 52 cases under the same validation layers. Warm
  elapsed times were 1.22, 1.38 and 1.16 seconds respectively.
- Tests prove cyan source-alpha blending and destination-alpha preservation,
  triangle depth writes and reverse winding with no cull, white always-pass
  depth writes, yellow forward/reverse `LEQUAL` visibility and depth writes,
  both order directions, collision rejection, invalid input, no-owner identity
  and the offline MSL outline entry point.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed across 1,632 cacheable tasks. Debug and Release Metal
  renderer, built-in MSL and GPU tests compiled and linked.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure -j 8` passed 484/484 in 35.40 seconds.
- Source-exact `AL_METAL_CONFIG=Release AL_METAL_JOBS=8
  tools/metal/build.sh build` passed at committed code head `4b755ffc95` after
  127 cacheable compilation tasks and produced Development build
  `26.4.0d68067`.
- Executable SHA-256:
  `8f2815a0848ae5028aad7f460625d3c3f3228d74ba29f97253bd02913fa1d5d6`.
  Packaged/source MSL SHA-256:
  `ad52fd989f6a3442eaf0fe38348b9968c8721d68d663cb468418af12ed5bda32`;
  compiled metallib SHA-256:
  `b0af557be051b32b1fa07d85ec4d0bc98c883b319e05f7bd6d5c3098da18eaca`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 11 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 11-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Lights no longer requires GL
world presentation, while every other uncaptured visual debug setting remains
fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Independent build/review evidence and reproducible distinct
release proof also remain open. Exact RLVa behavior/image parity is separately
deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_IMPOSTORS` next. Bounded
source inspection shows one four-edge white outline around an already native-
captured impostor billboard. It uses additive blending, a time-decaying 5-to-1
pixel line width, inherited depth and the existing center/left/up basis. This is
narrower than BBOXES and NORMALS, which require scene traversal, many object-
dependent colors and/or generated per-vertex geometry.

## Exact next actions

1. Capture and replay Develop **Impostors** as the exact four-edge billboard
   outline with additive blend, inherited depth, dynamic thickness and order.
2. Prove basis/edge placement, 5-to-1 pixel thickness decay, forward/reverse
   depth, order, off/no-impostor identity and invalid-input rejection on the
   physical M1 Max before removing only `RENDER_DEBUG_IMPOSTORS` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
