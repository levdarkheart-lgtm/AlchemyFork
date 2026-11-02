# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `2403e40ce3` (`feat(metal): replay octree debug output natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `2403e40ce3` captures and
replays Develop **Octree** across its exact post-order spatial traversal. Three
record kinds preserve the five-pixel red rebuild-flash bounds, each recently
updated/moved static or rigged face, and the final one-pixel cyan group outline.
Geometry capture uses zero-copy external vertex/index storage, retains exact
MVP and object-skin palette ownership, and publishes all records into one global
source order.

Flash geometry preserves four live `mBuilt` colors, additive RGB-only blending,
triangle line rasterization, no culling and always-pass read-only depth. Rebuild
bounds use the established screen-space wide-box path with the same additive and
no-depth contract. Final group outlines use standard RGB-only alpha blending,
filled box triangles, no culling and ordinary writable forward/reverse depth.
Validation rejects missing storage, invalid index types/ranges/counts/offsets,
non-finite transforms, negative extents, invalid kinds, malformed skin palettes,
non-monotonic order and cross-owner collisions. Octree participates in the
global render-debug order merge without changing source rebuild side effects.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay, manual reflection-probe volumes, Collision
Skeleton, Joints, Bounding Boxes, Raycast, Occlusion and Octree. The current OSS ARM64
viewer links only the nonfunctional `llphysicsextensionsos` pathfinding stub, so
pathfinding visualization is not a reachable retained-Alchemy output or a
release-critical port target. Nine text-only and four unused render-debug masks
remain native-eligible. The explicit visual veto now contains exactly:

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
  passed 51,021 assertions across 52 cases under Metal API and load-action
  validation. Offline MSL compiled in all three configurations.
- Coverage proves wide additive rebuild bounds, static and rigged additive flash
  geometry, all four source movement/update colors, no-depth forward/reverse
  behavior, ordinary writable group bounds, opposite Octree/Occlusion order
  outcomes, invalid/no-owner inputs and destination-alpha preservation.
- Warnings-as-errors RelWithDebInfo production integration compiled, linked,
  staged and signed after a 130-step rebuild before the final eligibility-mask
  edit; the source-exact Release build below compiled the final code.
- The neutral gate, `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8`, passed 484/484 in
  37.44 seconds.
- The preceding deliberately unfiltered 574-test inventory had six reproducible
  legacy GL-only failures and five skips. It was not rerun for this bounded owner
  and is not represented as a green gate.
- Source-exact `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=Release
  AL_METAL_JOBS=8 tools/metal/build.sh build` passed at committed code head
  `2403e40ce39532262f34947ec29a124865e1556d` after a 161-step rebuild and
  produced Development build `26.4.0d68083` (`26.4.0.68083`). Generated build
  state embeds the full SHA.
- Executable SHA-256:
  `99c5b3141adc32ca7a22c5a6e33c36cd1a72dc4410324f1642f9f328e48fd58f`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Raycast, Occlusion, Octree, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Three visual modes remain fail-closed; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit three-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Octree no longer requires GL
world presentation for rebuild flash, static/rigged face geometry or group bounds,
while every uncaptured visual debug setting remains fail-closed.

**Deferred critical work:** the three remaining non-RLVa advanced-view owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Exact RLVa behavior/image parity is separately deferred as
non-critical M-013. Reproducible distinct release proof and independent build/review evidence also remain open.

**Evidence-based priority:** audit `RENDER_DEBUG_PHYSICS_SHAPES` next. It is the
remaining owner with the clearest source boundary, but spans box, sphere,
cylinder, primitive-mesh and convex-hull production, asynchronous hull fallback,
bridge transforms, color/state selection and wireframe behavior. Normals and
Texel Density remain broader derived or textured geometry.

## Exact next actions

1. Inventory every Physics Shapes source path, state transition and hull-build
   side effect before changing presentation ownership.
2. Capture/replay the complete owner with physical shape-family, bridge,
   wireframe and forward/reverse-Z proof before removing only
   `RENDER_DEBUG_PHYSICS_SHAPES` from the veto.
3. Continue strict no-GL and controlled live parity; keep exact RLVa parity in
   M-013 and require shadows enabled in both viewers for shadow pairs.
