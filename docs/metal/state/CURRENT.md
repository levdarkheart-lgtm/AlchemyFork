# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `ae691b02da` (`feat(metal): replay occlusion debug output natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `ae691b02da` captures and
replays Develop **Occlusion** across both source-owned output phases. The world
spatial traversal publishes each non-null group vertex buffer immediately before
its x-ray draw. The later selected-group loop separately publishes its filled
magenta bounds. The capture is zero-copy for external vertex/index storage and
retains the exact MVP and a single global source order.

X-ray geometry preserves back-face culling, triangle line rasterization,
line-polygon offset, standard alpha blending into RGB only, and complementary
read-only depth in forward and reverse Z. Selected bounds use the established
procedural box path with back-face culling, filled triangles, exact magenta,
RGB-only output and ordinary read-only depth. Validation rejects missing
storage, invalid index types/ranges/counts/offsets, non-finite transforms,
negative extents, invalid kinds, non-monotonic order and cross-owner collisions.
Occlusion participates in the global render-debug order merge.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay, manual reflection-probe volumes, Collision
Skeleton, Joints, Bounding Boxes, Raycast and Occlusion. The current OSS ARM64
viewer links only the nonfunctional `llphysicsextensionsos` pathfinding stub, so
pathfinding visualization is not a reachable retained-Alchemy output or a
release-critical port target. Nine text-only and four unused render-debug masks
remain native-eligible. The explicit visual veto now contains exactly:

- OCTREE
- PHYSICS_SHAPES
- NORMALS
- TEXEL_DENSITY

The separate latent pathfinding guard remains fail-closed.

The bounded reflection-probe production-call audit found no valid viewer-
produced divergence; its extra Metal bounds checks do not alter valid data. It
remains closed without a speculative edit.

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

- The complete RelWithDebInfo, Debug and Release physical Metal suites each
  passed 50,687 assertions across 52 cases under Metal API and load-action
  validation. Offline MSL compiled in all three configurations.
- Coverage proves x-ray complementary read-only depth, back-face culling,
  alpha-retaining blend, selected filled ordinary read-only bounds, forward and
  reverse Z, opposite global-order pixel outcomes, invalid/no-owner inputs and
  destination-alpha preservation.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed after a 130-step rebuild before the final eligibility-mask
  edit; the source-exact Release build below compiled the final code.
- The neutral gate, `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8`, passed 484/484 in
  37.43 seconds.
- The preceding deliberately unfiltered 574-test inventory had six reproducible
  legacy GL-only failures and five skips. It was not rerun for this bounded owner
  and is not represented as a green gate.
- Source-exact `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=Release
  AL_METAL_JOBS=8 tools/metal/build.sh build` passed at committed code head
  `ae691b02da` after a 161-step rebuild and produced Development build
  `26.4.0d68081` (`26.4.0.68081`). Generated build state embeds the full SHA.
- Executable SHA-256:
  `cb22d7020ea6f2663c53d362847c8a7031bbc3487129b8003f06e22907cb351c`.
  Packaged/source MSL SHA-256:
  `12989bd3fcbce372c0d28a680d93afbe111994cfa871ddb83df2bccd3f54e14e`;
  compiled metallib SHA-256:
  `c6f84163e2367f9dac55b55504773528360fedc38174516e8c8b0957ca14d5d5`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified all 349
  Mach-O slices as ARM64. Packaged and source MSL compare byte-identically.
- `tools/metal/doctor.sh` passed on Apple M1 Max with 64 GiB unified memory and
  Apple7-family Metal support.
- `python3 tools/metal/validate_harness.py` validates all 31 required files;
  `python3 -m unittest discover -s tools/metal/tests -v` passes 24/24.
- The main executable still links both Metal and OpenGL during migration.
  Strict no-GL remains an open release gate; no native-only claim is made.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-007 world/material | doing | Bounded active transparency/material and reflection-probe audits found no valid-production divergence; broader live parity remains open. |
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Raycast, Occlusion, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Four visual modes remain fail-closed; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit four-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Occlusion no longer requires GL
world presentation for group-buffer x-ray geometry or selected-group bounds,
while every uncaptured visual debug setting remains fail-closed.

**Deferred critical work:** the four remaining non-RLVa advanced-view owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Exact RLVa behavior/image parity is separately deferred as
non-critical M-013. Reproducible distinct release proof and independent build/review evidence also remain open.

**Evidence-based priority:** audit and implement `RENDER_DEBUG_OCTREE` next. Of
the four remaining modes it is the next bounded source owner, but it must retain
the time-varying rebuild flash, group bounds, ordinary and rigged face geometry,
matrix palettes, additive blend and wide outlines as a coherent unit. Physics
Shapes, Normals and Texel Density own broader derived or textured geometry.

## Exact next actions

1. Finish the exact Octree source/state inventory, including rebuild side
   effects, bridge transforms, rigged palettes, flash lifetime and every output
   phase.
2. Capture/replay the complete owner with global order and physical forward/
   reverse-Z proof before removing only `RENDER_DEBUG_OCTREE` from the veto.
3. Continue strict no-GL and controlled live parity; keep exact RLVa parity in
   M-013 and require shadows enabled in both viewers for shadow pairs.
