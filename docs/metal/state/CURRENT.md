# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `ae2b436442` (`fix(metal): preserve opaque fullbright draws`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 and M-009 are `doing`; M-012 is complete. Commit `47d253a91c` captures
and replays every reachable Develop **Texel Density** face draw. Source audit
proved that all three current/desired/full modes bind the checkerboard and
calculate a texture matrix, but the bound production `gDebugProgram` consumes
only vertex position and the current uniform color. Texture, UV and the matrix
are not shader inputs. Native replay therefore retains the exact visible
zero-copy indexed uniform-color overlay without inventing a sampled-texture
path; unchanged source traversal retains the mode-dependent texture lookup,
binding and matrix side effects.

Replay preserves the live model-view-projection matrix, 16- and 32-bit index
buffers, arbitrary finite current color, disabled culling, fill raster,
standard alpha blend, RGB-only writes and writable LEQUAL world depth under
forward and reverse Z. HUD faces remain covered by ordered native indexed UI
capture. Validation rejects missing storage/buffers, invalid ranges, offsets,
index width, non-finite values, invalid state, non-monotonic order and every
cross-owner collision.

Prior native advanced-output owners remain covered: global world wireframe,
static/rigged world and HUD Highlight Transparent, Show Updates to Objects blips,
Wind Vectors, Agent Target, Points, Composition, reflection-probe neighbor links,
Texture Anim, Update Type, Batch Size, Lights, Impostors, Shadow Frusta, the
Develop far-plane projection overlay, manual reflection-probe volumes, Collision
Skeleton, Joints, Bounding Boxes, Raycast, Occlusion, Octree, Physics Shapes,
Normals and Texel Density. The current OSS ARM64
viewer links only the nonfunctional `llphysicsextensionsos` pathfinding stub, so
pathfinding visualization is not a reachable retained-Alchemy output or a
release-critical port target. Nine text-only and four unused render-debug masks
remain native-eligible. No reachable render-debug world-geometry bit remains in
the explicit visual veto. The separate latent pathfinding guard remains
fail-closed.

Commit `ed9e64736d` makes the scoped native authored-mirror Snapshot diagnostic
reachable in optimized Development builds; compiled public Release channels
remain ineligible, and mirror rendering/live authority are unchanged.

Exact RLVa parity is owner-deferred M-013 nice-to-have. Implement only enough
RLVa behavior to avoid blocking Metal authority and viewer-owned OpenGL removal.
The persistent inventory is [the deferred RLVa backlog](../work/deferred/rlva-parity.md).
Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both viewers before testing.

Any candidate/reference shadow comparison is admissible only when shadows are
enabled and saved through Graphics Preferences in **both** viewers before
capture. Shadows do not require a restart; restart applies only to RLVa state
changes. A mismatched off/on pair is invalid; a both-off control is diagnostic.

The 2026-11-02 fixed-Midday Grignano replay satisfies that prerequisite: both
profiles and restarted panels reported `RenderShadowDetail=2`, all 416 visual
settings matched, and candidate logs proved continuous native publication plus
direct Metal directional/projector owners. It corroborates directional shadows
only; projector-visible, alpha/avatar-caster, cascade and PCSS/quality coverage
remain open. [Exact evidence is archived](archive/CURRENT-shadow-preference-qualified-checkpoint-2026-11-02.md).

A source-derived physical regression now proves that GL cube-camera rays,
native face publication and Metal cube sampling agree for every texel of all
six asymmetric faces. Direct same-slice copy is correct; no production
transform edit was justified. [Exact evidence is archived](archive/CURRENT-hero-cube-camera-orientation-checkpoint-2026-11-02.md).
Commit `ae2b436442` corrects the native ordinary/masked fullbright pipeline to
match its GL owner: blending disabled and RGB-only writes. Source proves that
vertex alpha is shininess outside the alpha pool, not coverage; blending by it
could erase zero-shiny posters/screens. A physical regression now proves full
texel replacement with destination-alpha preservation. [Exact evidence is
archived](archive/CURRENT-opaque-fullbright-parity-checkpoint-2026-11-02.md).
The dry fixture now has one owner-confirmed visual parity sample. Reference
`67607` and native candidate `68099` used byte-identical 175-control sessions,
identical seated coordinates and 12 matching live controls. Both 2048x1476
images contain the avatar, horizon and stairs with equivalent exposure/tone;
native readback, six-face publication and clean exits are logged. Metrics remain
descriptive because camera/cloud/foliage phase differs. Legacy alpha/stained
glass and broader mirrors remain open. [Exact evidence](archive/CURRENT-authored-mirror-owner-confirmed-checkpoint-2026-11-02.md).
## Build and test evidence

- The complete RelWithDebInfo, Debug and Release physical Metal suites each
  passed 52,190 assertions across 52 cases under Metal API and load-action
  validation. Offline MSL compiled in all three configurations.
- Coverage additionally proves opaque fullbright texel replacement despite
  vertex shininess alpha, RGB-only destination-alpha preservation and the exact
  six-face GL camera-to-Metal convention across 96 asymmetric texels.
- Warnings-as-errors RelWithDebInfo production integration and the source-exact
  Release build below compiled, linked, staged and signed the final code.
- The source-exact neutral gate, `ctest --test-dir
  build-Darwin-ninja-os-arm64 -C Release -LE 'manual|gl'
  --output-on-failure -j 8`, passed 484/484 in 36.60 seconds.
- The preceding deliberately unfiltered 574-test inventory had six reproducible
  legacy GL-only failures and five skips. It was not rerun for this bounded owner
  and is not represented as a green gate.
- Source-exact `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=Release
  AL_METAL_JOBS=8 tools/metal/build.sh build` passed at
  `ae2b43644211a73802d4197efa39a361ad5614bb`, producing Development build
  `26.4.0d68099` (`26.4.0.68099`) with the Git revision embedded.
- Executable SHA-256:
  `ea063150c286acf7498a3c6e065cbe367e140b401ef3682536d1771d357f602a`.
  Packaged/source MSL SHA-256:
  `d263315d1ea0c2c834e9e5a5d37109e9ec5d56325153220dedc1b2a9ab2ffac9`;
  compiled metallib SHA-256:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified all 349
  Mach-O slices as ARM64. Packaged and source MSL compare byte-identically.
- `tools/metal/doctor.sh` passed on Apple M1 Max with 64 GiB unified memory and
  Apple7-family Metal support.
- `python3 tools/metal/validate_harness.py` validates all 31 required files;
  all 27 tooling tests pass, including kernel-relative launches and Firestorm exclusion.
- The [preference-qualified shadow archive](archive/CURRENT-shadow-preference-qualified-checkpoint-2026-11-02.md)
  retains effective-setting artifacts, screenshot hashes and exact metrics.
- The main executable still links both Metal and OpenGL during migration.
  Strict no-GL remains an open release gate; no native-only claim is made.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-007 world/material | doing | Ordinary/masked fullbright now match GL opaque/RGB-only semantics with a physical regression; legacy BLEND/stained-glass and broader live parity remain open. |
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Texel Density, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Raycast, Occlusion, Octree, Physics Shapes, Normals, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. One controlled authored-mirror sample is owner-confirmed; the broader probe/mirror matrix stays open. No reachable render-debug world-geometry bit remains in the explicit veto; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | doing | Full matrix, stability and strict owned-OpenGL elimination are open; 3,351 findings still exit 1. Directional shadows, hero cube orientation and one dry authored-mirror sample are bounded/proven. Opaque fullbright is corrected; legacy alpha/stained-glass and broader mirror parity remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; preflight now accepts kernel-proven relative launches and rejects Firestorm siblings. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Opaque fullbright is source-correct,
the cube convention is physically protected and one dry mirror sample is
owner-confirmed; legacy BLEND/stained glass and broader mirrors remain open.

**Deferred critical work:** controlled live wireframe, material and editing
comparisons; projector-visible, alpha/avatar-caster, cascade and quality shadow
coverage; full feature and prolonged stability matrices; owned OpenGL removal;
and paired quality-equivalent M1 Max measurements. Exact RLVa
behavior/image parity is separately deferred as non-critical M-013.
Reproducible distinct release proof and independent build/review evidence also
remain open.

**Evidence-based priority:** continue M-009 from the strict GL inventory; use
the empty debug veto and directional-shadow result only as bounded facts while
resolving the highest-value presentation-authority gate.

## Exact next actions

1. Reconcile the refreshed 3,351-finding strict GL inventory against actual
   reachability and presentation authority; the strict exit remains 1.
2. Move past the accepted dry mirror sample and isolate legacy alpha-pool
   BLEND/stained glass with a controlled fixture.
3. Continue controlled parity/stability; keep remaining shadow coverage open
   unless Preferences/save-qualified, and exact RLVa deferred in M-013.
