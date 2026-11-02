# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `e8f6613c4e` (`feat(metal): replay raycast debug output natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `e8f6613c4e` captures and
replays Develop **Raycast** across the complete spatial-world traversal. Every
volume-octree branch intersected by the live cursor ray retains its ordinary
outline plus the source's hidden and visible triangle passes. Immediate source
triangles become explicit edge pairs so Metal can preserve one-pixel hidden and
three-pixel visible line widths without changing the GL diagnostic.

Records retain exact MVP, center, half extent, palette, line width, expanded
triangle vertices and global order. Ordinary branch outlines use writable depth;
hidden outlines/triangles use complementary read-only depth; visible triangles
use ordinary read-only depth. The selected object retains three oriented filled
axis boxes and its always-visible read-only bounds. A selected avatar reuses the
proven Collision Skeleton owner, and the post-deferred particle-hit crosshair is
replayed after reflection-probe diagnostics at its separate exact source point.

Validation rejects missing storage, non-finite transforms/vertices, invalid
extent/palette/kind/width/range/order, owner-order collisions and allocation
failure. Raycast participates in the global render-debug order merge; all output
keeps RGB-only destination-alpha preservation. Only its veto bit was removed.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay, manual reflection-probe volumes, Collision
Skeleton, Joints, Bounding Boxes and Raycast. The current OSS ARM64 viewer links
only the nonfunctional `llphysicsextensionsos` pathfinding stub, so pathfinding
visualization is not a reachable retained-Alchemy output or release-critical port
target. Nine text-only and four unused render-debug masks remain native-eligible.
The explicit visual veto now contains these five modes:

- OCTREE
- OCCLUSION
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

- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  50,153 assertions across 52 cases under Metal API and load-action validation.
  Offline MSL compiled in all three configurations.
- Coverage proves missing/null storage, non-finite matrix/vertex, negative
  extent, invalid kind/palette/range/order and no-owner rejection; branch,
  hidden/visible triangle, oriented-axis, hit-bounds and particle rasterization;
  forward/reverse writable and complementary/read-only depth; width growth;
  destination-alpha preservation; and explicit Raycast-vs-Points source order.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed after a 127-step rebuild.
- The intended neutral gate, `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8`, passed 484/484 in
  39.45 seconds.
- The deliberately unfiltered 574-test inventory had 568 non-failing entries,
  including five skips, and six failures after 1,286.64 seconds. The three
  primary failures reproduce alone and require unavailable/legacy GL behavior:
  `alframefences_gl_test`, `llrendertarget_gl_test` and `aldof_gl_test`. The
  other three are their combined/benchmark aggregate entries. This is not a
  Raycast Metal-owner regression and is not represented as a passing full gate.
- Source-exact `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=Release
  AL_METAL_JOBS=8 tools/metal/build.sh build` passed at committed code head
  `e8f6613c4e` after a 161-step rebuild and produced Development build
  `26.4.0d68079` (`26.4.0.68079`).
- Executable SHA-256:
  `f113ecab21125f63f88c2f3185038eb123ced6b0a48e1f258f73168259f8ec5e`.
  Packaged/source MSL SHA-256:
  `12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`;
  compiled metallib SHA-256:
  `c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified all 349
  Mach-O slices as ARM64, and the main executable reports arm64. Packaged and
  source MSL compare byte-identically.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Raycast, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; five visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit five-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Raycast no longer requires GL
world presentation across branch/triangle traversal, object/avatar hits and the
particle marker, while every uncaptured visual debug setting remains fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Independent build/review evidence and reproducible distinct
release proof also remain open. Exact RLVa behavior/image parity is separately
deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_OCCLUSION` next. Source
audit bounds it to grouped vertex-buffer x-ray geometry under complementary
read-only line depth plus selected-group filled boxes. It can reuse validated
mirrors, indexed geometry and box primitives. OCTREE adds time-varying rebuild
flashes, rigged faces and additive wide outlines; Physics Shapes, Normals and
Texel Density own broader derived or textured geometry.

## Exact next actions

1. Capture Occlusion x-ray group-buffer batches and the later selected-group
   boxes at their exact source sites without altering visibility bookkeeping.
2. Preserve bridge/model transforms, indexed ranges, cull, line polygon offset,
   complementary read-only depth, palette/blend and source order; prove empty,
   selected and forward/reverse cases plus invalid-input rejection on M1 Max.
3. Remove only `RENDER_DEBUG_OCCLUSION` from the veto after that proof. Continue
   strict no-GL and controlled live parity; keep exact RLVa parity in M-013 and
   require shadows enabled in both viewers for shadow pairs.
