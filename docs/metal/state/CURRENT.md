# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `a22cc91fc4` (`feat(metal): replay bounding box debug output`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `a22cc91fc4` captures and
replays Develop **Bounding Boxes** from the exact spatial-world traversal owner.
The capture transaction encloses only that traversal, so the shadow helper's
`set_color=false` boxes cannot enter the native batch. HUD attachments use the
separate post-deferred owner, while impostor/cutout traversals suppress this
debug owner; the bounded transaction therefore covers reachable BBOX output.

Each record retains the live MVP, center, half extent, diffuse color, source
line width and global order. Hooks cover spatial-group boxes, per-face bridge
boxes, active and nonactive drawable boxes, with the original palette and
one-to-five-pixel active-object pulse. Thin boxes reuse the proven generated
24-vertex outline. Wide boxes use a new 72-vertex screen-space quad expansion
pipeline with no cull, RGB-only output and writable forward/reverse depth. Its
clip-space guard rejects non-finite input and handles zero `w` without division.

Validation rejects missing/null storage, non-finite transforms or geometry,
negative extents, colors outside the exact source palette, invalid widths,
owner-order collisions and allocation failure. BBOX participates in the exact
global debug-order merge. Destination alpha remains unchanged.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay, manual reflection-probe volumes, Collision
Skeleton, Joints and Bounding Boxes. The current OSS ARM64 viewer links only the
nonfunctional `llphysicsextensionsos` pathfinding stub, so pathfinding
visualization is not a reachable retained-Alchemy output or release-critical port
target. Nine text-only and four unused render-debug masks remain native-eligible.
The explicit visual veto now contains these six modes:

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

- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  49,644 assertions across 52 cases under Metal API and load-action validation.
  Offline MSL compiled in all three configurations.
- Coverage proves missing/null storage, non-finite matrix, negative extent,
  invalid palette/width, order collision and no-owner rejection; thin and wide
  forward/reverse raster/depth behavior; destination-alpha preservation; width-5
  coverage growth; and explicit BBOX-vs-Points source ordering.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed after a 1,097-edge rebuild.
- The intended neutral gate, `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8`, passed 484/484 in
  35.42 seconds.
- The deliberately unfiltered 574-test inventory had 568 non-failing entries,
  including five skips, and six failures after 1,286.64 seconds. The three
  primary failures reproduce alone and require unavailable/legacy GL behavior:
  `alframefences_gl_test`, `llrendertarget_gl_test` and `aldof_gl_test`. The
  other three are their combined/benchmark aggregate entries. This is not a
  BBOX Metal-owner regression and is not represented as a passing full gate.
- Source-exact `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=Release
  AL_METAL_JOBS=8 tools/metal/build.sh build` passed at committed code head
  `a22cc91fc4` after a 1,397-edge rebuild and produced Development build
  `26.4.0d68077` (`26.4.0.68077`).
- Executable SHA-256:
  `328024070349f48b9cb3b42f1bee2345c54dfd90c84e02d0fa79218bb81b8b37`.
  Packaged/source MSL SHA-256:
  `fa3181abe44fbaec3ffab41248d23e377ec5608faf00188f776f475404ab4d92`;
  compiled metallib SHA-256:
  `d9370caf0b49a92972fc33b8de8e3f7451d7f21295bd5d2182ce62c0c2185959`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; six visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit six-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Bounding Boxes no longer requires
GL world presentation, including the wide active-object pulse, while every
uncaptured visual debug setting remains fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Independent build/review evidence and reproducible distinct
release proof also remain open. Exact RLVa behavior/image parity is separately
deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_RAYCAST` next. Its output is
bounded to a selected hit: volume-octree branch boxes and triangles, object hit
orientation and bounds, selected-avatar collision volumes, and a particle-hit
crosshair. It can reuse established outline/line/sphere primitives but requires
an explicit triangle-batch owner. Persistent Octree/Occlusion views and the
Physics Shapes, Normals and Texel Density geometry-wide owners are broader.

## Exact next actions

1. Capture the Raycast volume-octree traversal and selected object/avatar hit
   markers at their exact source sites, plus the post-deferred particle crosshair.
2. Preserve branch palette, two-pass triangle depth/color grammar, hit orientation,
   object bounds, traversal order and particle marker; prove empty/no-hit controls,
   forward/reverse depth, alpha preservation and invalid-input rejection on M1 Max.
3. Remove only `RENDER_DEBUG_RAYCAST` from the veto after that proof. Continue
   strict no-GL and controlled live parity; keep exact RLVa parity in M-013 and
   require shadows enabled in both viewers for shadow pairs.
