# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `08973b83dc` (`feat(metal): replay debug normals natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `08973b83dc` captures and
replays both reachable Develop **Normals** source paths. Volume faces retain the
exact CPU yellow normal and cyan tangent line construction, including inverse-
scale normalization, draw length and live transform. Non-volume drawables
retain their live Metal position/normal/tangent and 16- or 32-bit index buffers;
native vertex variants expand one normal and optional tangent line per triangle-
index occurrence without a per-frame geometry copy.

Replay preserves the GL vertex/geometry shader's view-space direction
normalization, one-pixel lines, disabled culling, standard alpha blend,
RGB-only writes and read-only LEQUAL world depth under forward and reverse Z.
Volume HUD normals remain covered by ordered native immediate-mode UI capture;
an unexpected non-volume HUD path invalidates the transaction. Validation
rejects missing storage, non-finite data, invalid kinds/colors/ranges/index
widths/buffer offsets/transforms/draw lengths/tangent contracts/state/order and
every cross-owner collision.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay, manual reflection-probe volumes, Collision
Skeleton, Joints, Bounding Boxes, Raycast, Occlusion, Octree, Physics Shapes and
Normals. The current OSS ARM64
viewer links only the nonfunctional `llphysicsextensionsos` pathfinding stub, so
pathfinding visualization is not a reachable retained-Alchemy output or a
release-critical port target. Nine text-only and four unused render-debug masks
remain native-eligible. The explicit visual veto now contains exactly:

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
  passed 51,416 assertions across 52 cases under Metal API and load-action
  validation. Offline MSL compiled in all three configurations.
- Coverage proves explicit CPU normal/tangent lines, zero-copy u16/u32 indexed
  normal and optional tangent expansion, forward/reverse read-only depth, exact
  RGB-only destination-alpha preservation, invalid/no-owner inputs and cross-
  owner order rejection.
- Warnings-as-errors RelWithDebInfo production integration and the source-exact
  Release build below compiled, linked, staged and signed the final code.
- The neutral gate, `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8`, passed 484/484 in
  35.58 seconds.
- The preceding deliberately unfiltered 574-test inventory had six reproducible
  legacy GL-only failures and five skips. It was not rerun for this bounded owner
  and is not represented as a green gate.
- Source-exact `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=Release
  AL_METAL_JOBS=8 tools/metal/build.sh build` passed at committed code head
  `08973b83dccad57d39fa0386c4f8463385761d2a` after a 161-step rebuild and
  produced Development build `26.4.0d68087` (`26.4.0.68087`). Generated build
  state embeds the full SHA.
- Executable SHA-256:
  `039fbe6e2af93d5c9fc63d8c94b79a574323aa908a4a27a08c7a16c6807f445d`.
  Packaged/source MSL SHA-256:
  `73955209f611038a5e4fe0305642614fefee30766dcd26fd03b7df7d3933df20`;
  compiled metallib SHA-256:
  `fd64dfedceb9056e85090916b7e0dab2d8e17c553cc4ffe63ea1b5e67647c297`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Raycast, Occlusion, Octree, Physics Shapes, Normals, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Texel Density is the sole explicit visual veto; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The final explicit render-debug world-output owner remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Normals no longer requires GL
world presentation for volume CPU lines or non-volume indexed normal/tangent
expansion, while every uncaptured visual debug setting remains fail-closed.

**Deferred critical work:** the final remaining non-RLVa advanced-view owner;
controlled live wireframe, material, editing and shadow comparisons; full feature
and stability matrices; viewer-owned OpenGL removal; and paired quality-equivalent
M1 Max measurements. Exact RLVa behavior/image parity is separately deferred as
non-critical M-013. Reproducible distinct release proof and independent build/review evidence also remain open.

**Evidence-based priority:** audit `RENDER_DEBUG_TEXEL_DENSITY` next. It is the
final explicit textured-geometry owner and must retain every reachable producer,
shader input, texture/state dependency, transform and side effect before the
world-output veto can become empty.

## Exact next actions

1. Inventory every Texel Density producer, shader, buffer/texture dependency,
   transform, filter, state transition and side effect before changing
   presentation ownership.
2. Capture/replay the complete owner with physical textured-geometry and
   forward/reverse-Z proof before removing only `RENDER_DEBUG_TEXEL_DENSITY`
   from the veto.
3. Continue strict no-GL and controlled live parity; keep exact RLVa parity in
   M-013 and require shadows enabled in both viewers for shadow pairs.
