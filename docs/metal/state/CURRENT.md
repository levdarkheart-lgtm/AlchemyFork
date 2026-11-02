# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `5110b2b9fc` (`feat(metal): replay physics shapes natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `5110b2b9fc` captures and
replays Develop **Physics Shapes** across the source's exact three-pass
transaction. Depth fill, translucent color fill and color wire records preserve
box, sphere, cylinder, primitive-volume mesh, decomposition/base hull and
generated convex-hull production, including bridge/model transforms and the
existing asynchronous hull-build/fetch side effects.

Fill replay uses original triangle topology, back-face culling, LEQUAL depth and
the source RGB-only color masks. Triangle wire replay retains triangle topology
and culling, applies the exact polygon-offset sign for forward/reverse depth and
reconstructs the source three-pixel line footprint with nine one-pixel
screen-space instances. Cyan/magenta/yellow fallback boxes use the established
three-pixel wide-line path and remain uncullable line primitives. Validation
rejects missing storage, non-finite vertices/transforms/colors, invalid pass,
ranges/topology/state/order and every cross-owner collision. Physics Shapes
participates in the global render-debug order merge without changing physics
shape generation or fallback behavior.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay, manual reflection-probe volumes, Collision
Skeleton, Joints, Bounding Boxes, Raycast, Occlusion, Octree and Physics Shapes. The current OSS ARM64
viewer links only the nonfunctional `llphysicsextensionsos` pathfinding stub, so
pathfinding visualization is not a reachable retained-Alchemy output or a
release-critical port target. Nine text-only and four unused render-debug masks
remain native-eligible. The explicit visual veto now contains exactly:

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
  passed 51,308 assertions across 52 cases under Metal API and load-action
  validation. Offline MSL compiled in all three configurations.
- Coverage proves depth-only color preservation, all three passes, triangle-wire
  and explicit fallback-wide-line production, forward/reverse depth, exact RGB-
  only destination-alpha preservation, invalid/no-owner inputs and cross-owner
  order rejection.
- Warnings-as-errors RelWithDebInfo production integration and the source-exact
  Release build below compiled, linked, staged and signed the final code.
- The neutral gate, `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8`, passed 484/484 in
  40.43 seconds.
- The preceding deliberately unfiltered 574-test inventory had six reproducible
  legacy GL-only failures and five skips. It was not rerun for this bounded owner
  and is not represented as a green gate.
- Source-exact `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=Release
  AL_METAL_JOBS=8 tools/metal/build.sh build` passed at committed code head
  `5110b2b9fc21e300f7cdf724aa70ceefb8624435` after a 161-step rebuild and
  produced Development build `26.4.0d68085` (`26.4.0.68085`). Generated build
  state embeds the full SHA.
- Executable SHA-256:
  `791597f39f48a34c005891fd8216371dddba769993957b434185df48661363f8`.
  Packaged/source MSL SHA-256:
  `16d904f070eb3000210b7581f42312bc88bfa5f257d6ff1593c13646752f51b9`;
  compiled metallib SHA-256:
  `f6266b2331e1a410967fb79902d33446c44647e3d73706f1e6fdf13f69f1b7ae`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Raycast, Occlusion, Octree, Physics Shapes, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Two visual modes remain fail-closed; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit two-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Physics Shapes no longer
requires GL world presentation for its depth, fill or wire passes across every
reachable shape family, while every uncaptured visual debug setting remains
fail-closed.

**Deferred critical work:** the two remaining non-RLVa advanced-view owners;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Exact RLVa behavior/image parity is separately deferred as
non-critical M-013. Reproducible distinct release proof and independent build/review evidence also remain open.

**Evidence-based priority:** audit `RENDER_DEBUG_NORMALS` next. It contains both
CPU immediate normal/tangent line generation and drawable-buffer geometry-
derived output, so its exact source paths, transforms, filtering and state must
be reconciled before changing authority. Texel Density remains the final
explicit textured-geometry owner.

## Exact next actions

1. Inventory every Normals source path, transform, face filter and normal/tangent
   construction rule before changing presentation ownership.
2. Capture/replay the complete owner with physical CPU-generated, drawable-
   derived and forward/reverse-Z proof before removing only
   `RENDER_DEBUG_NORMALS` from the veto.
3. Continue strict no-GL and controlled live parity; keep exact RLVa parity in
   M-013 and require shadows enabled in both viewers for shadow pairs.
