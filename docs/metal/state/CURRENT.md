# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `5001b10d20` (`feat(metal): replay texture animation debug overlay`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `5001b10d20` captures and
replays Develop **Texture Anim** in the native opaque-post scene. The source
selects only visible draw infos with a non-null texture matrix, but its active
`gDebugProgram` is position-only: the matrix and texture are not sampled. The
native contract therefore reuses the source position/index buffers and current
modelview-projection with a fixed `(1,1,0,0.5)` translucent-yellow uniform. It
deliberately does not skin draw infos that merely carry avatar metadata because
the source keeps the non-rigged debug shader bound.

Capture requires complete mirrored buffers, triangle cardinality, aligned and
in-range offsets, enabled blend, disabled culling, RGB-only writes and inherited
forward/reverse `LEQUAL` read/write depth. Non-finite transforms, invalid or
duplicate traversal order, missing storage and allocation failure fail closed;
empty/off input is an exact no-op. Texture Anim, Points and Agent Target are
merged by the original traversal order. HUD-only output is retained through the
established uniform-color capture transaction while the original GL draw stays
unchanged during migration.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD **Highlight Transparent**, **Show Updates to
Objects** blips, Wind Vectors, Agent Target, Points, Composition, reflection-
probe neighbor links, Texture Anim, the Develop far-plane projection overlay and manual
reflection-probe volumes. The current OSS ARM64 viewer links only the nonfunctional
`llphysicsextensionsos` pathfinding stub, so pathfinding visualization is not a
reachable retained-Alchemy output or a release-critical port target. The
render-debug guard now contains the 14 modes that still emit uncaptured world
pixels or alter captured world geometry. Nine text-only and four unused masks
remain eligible for native publication; the remaining 14 visual modes and the
latent pathfinding guard remain fail-closed.

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
off/on pair is invalid; a both-off control can isolate a residual but cannot
prove shadow parity.

## Build and test evidence

- Focused physical RelWithDebInfo Texture Anim/world replay passed 2,214
  assertions under Metal API/GPU/load-store/shader validation.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,872 assertions across 52 cases under the same validation layers. Measured
  elapsed times were 5.99, 8.05 and 7.84 seconds respectively.
- Warnings-as-errors RelWithDebInfo production viewer integration compiled,
  linked, staged and signed. Debug and Release Metal renderer, built-in MSL and
  GPU tests compiled and linked.
- The Texture Anim change passed a warnings-as-errors RelWithDebInfo production
  viewer build. `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8` passed 484/484 in
  35.43 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68061`
  linked, staged and signed at committed code head `5001b10d20`.
- Executable SHA-256:
  `eac2133bf873934f01c63753864d477804b3f0b838ef6e2960ab31c260e68a1c`.
  Packaged/source MSL SHA-256:
  `fc55ef6f5161d51eb6ce8b8ee8bfb6a9316c9752f5262592b8254fc3b06701fa`;
  compiled metallib SHA-256:
  `4e9ab857f3a312a7ddf8b5fd13982cd4c4037b6525e8631e331edf323a14d7e1`.
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
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. Text-only/unused render-debug modes no longer veto; 14 visual modes remain fail-closed. OSS pathfinding is a nonfunctional stub. Broader debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The explicit 14-mode render-debug world-output set remains pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Texture Anim no longer requires
GL world presentation, while every other uncaptured
visual debug setting remains fail-closed.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output owners;
controlled live wireframe, material, editing and shadow comparisons; full
feature and stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** implement `RENDER_DEBUG_UPDATE_TYPE` next. Source
inspection shows a bounded flat-overlay owner: drawables with a known last
update type replay each face's indexed triangles under one of five fixed
half-alpha colors. It uses the inherited position-only debug shader, blend,
RGB-only mask and depth state, with no texture or generated primitive family.

## Exact next actions

1. Capture and replay Develop **Update Type** face triangles only for drawables
   whose last update type is known; retain the other 13 visual debug bits and
   pathfinding fail-closed after that owner passes.
2. Prove all five source color classifications, face range/transform and
   traversal order, RGB-only half-alpha blending, inherited forward/reverse
   depth interaction, unknown/off identity and invalid-input rejection on the
   physical M1 Max before removing `RENDER_DEBUG_UPDATE_TYPE` from the veto.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa parity
   in M-013 and require shadows enabled in both viewers for shadow pairs.
