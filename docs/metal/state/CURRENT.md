# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `b0e0973898` (`feat(metal): replay texel density overlay natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 and M-009 are `doing`; M-012 is complete. Commit `b0e0973898` captures
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

The bounded reflection-probe production-call audit found no valid viewer-
produced divergence; its extra Metal bounds checks do not alter valid data. It
remains closed without a speculative edit.

Exact RLVa parity is owner-deferred M-013 nice-to-have. Implement only enough
RLVa behavior to avoid blocking Metal authority and viewer-owned OpenGL removal.
The persistent inventory is [the deferred RLVa backlog](../work/deferred/rlva-parity.md).
Any exact RLVa comparison must enable RLV in Preferences in both viewers, save,
and restart both viewers before testing.

Any candidate/reference shadow comparison is admissible only when shadows are
enabled and saved through Graphics Preferences in **both** viewers and both
viewers have been restarted before capture. A mismatched off/on pair is invalid;
a both-off control can isolate a residual but cannot prove shadow parity.

The 2026-11-02 fixed-Midday Grignano replay satisfies that prerequisite: both
profiles and restarted panels reported `RenderShadowDetail=2`, all 416 visual
settings matched, and candidate logs proved continuous native publication plus
direct Metal directional/projector owners. It corroborates directional shadows
only; projector-visible, alpha/avatar-caster, cascade and PCSS/quality coverage
remain open. [Exact evidence is archived](archive/CURRENT-shadow-preference-qualified-checkpoint-2026-11-02.md).

A subsequent current-build authored-mirror attempt did not yield an admissible
pair. After exact native 256-square hero-radiance publication, candidate semantic
UI observation timed out; `SIGTERM` entered quitting but failed to finish, and
the watchdog identified `Pipeline:RenderGeomDeferred` before forced process
termination. The mirror guard remains fail-closed. [Failure evidence and the
next diagnostic are archived](archive/CURRENT-hero-mirror-validation-hang-checkpoint-2026-11-02.md).

## Build and test evidence

- The complete RelWithDebInfo, Debug and Release physical Metal suites each
  passed 51,784 assertions across 52 cases under Metal API and load-action
  validation. Offline MSL compiled in all three configurations.
- Coverage proves u16/u32 indexed replay, arbitrary uniform color,
  forward/reverse writable depth, exact RGB-only destination-alpha
  preservation, invalid/no-owner inputs and cross-owner order rejection.
- Warnings-as-errors RelWithDebInfo production integration and the source-exact
  Release build below compiled, linked, staged and signed the final code.
- The neutral gate, `ctest --test-dir build-Darwin-ninja-os-arm64 -C
  RelWithDebInfo -LE 'manual|gl' --output-on-failure -j 8`, passed 484/484 in
  37.42 seconds.
- The preceding deliberately unfiltered 574-test inventory had six reproducible
  legacy GL-only failures and five skips. It was not rerun for this bounded owner
  and is not represented as a green gate.
- Source-exact `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=Release
  AL_METAL_JOBS=8 tools/metal/build.sh build` passed at committed code head
  `b0e0973898da8e3a275b8a29ed72e7ce7f4e9db5` after a 161-step rebuild and
  produced Development build `26.4.0d68089` (`26.4.0.68089`). Generated build
  state embeds the full SHA.
- Executable SHA-256:
  `f5fa05484b57fd77f108838e6d5b82ffde0e8a41697116db742b39b9e9b083f7`.
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
- The [preference-qualified shadow archive](archive/CURRENT-shadow-preference-qualified-checkpoint-2026-11-02.md)
  retains effective-setting artifacts, screenshot hashes and exact metrics.
- The main executable still links both Metal and OpenGL during migration.
  Strict no-GL remains an open release gate; no native-only claim is made.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-007 world/material | doing | Bounded active transparency/material and reflection-probe audits found no valid-production divergence; broader live parity remains open. |
| M-008 advanced views/effects | doing | Wireframe, Wind Vectors, Agent Target, Points, Composition, reflection-probe links, Texture Anim, Update Type, Texel Density, Batch Size, Lights, Impostors, Shadow Frusta, Collision Skeleton, Joints, BBOXES, Raycast, Occlusion, Octree, Physics Shapes, Normals, Highlight Transparent, object-update blips, far-plane projection and reflection-probe-volume visualization are native. No reachable render-debug world-geometry bit remains in the explicit veto; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | doing | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. The refreshed scan covers 2,408 files and 3,351 findings; strict exits 1. Directional shadows are preference-qualified, but projector-visible parity remains open. A current authored-mirror diagnostic then hit a semantic-UI timeout and watchdog shutdown hang after native hero publication; its guard remains fail-closed. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Develop Texel Density no longer requires
GL world presentation, and the explicit reachable render-debug world-geometry
veto is now empty. Uncaptured owners outside that bounded family remain
fail-closed.

**Deferred critical work:** controlled live wireframe, material and editing
comparisons; projector-visible, alpha/avatar-caster, cascade and quality shadow
coverage; full feature and stability matrices; viewer-owned OpenGL removal;
and paired quality-equivalent M1 Max measurements. Exact RLVa
behavior/image parity is separately deferred as non-critical M-013.
Reproducible distinct release proof and independent build/review evidence also
remain open.

**Evidence-based priority:** continue M-009 from the refreshed strict
viewer-owned GL and presentation-authority inventory. Use the now-empty
explicit render-debug veto and preference-qualified directional-shadow result
as bounded facts, not as a native-only claim; resolve the highest-value
remaining presentation-authority gate from current source and live evidence.

## Exact next actions

1. Reconcile the refreshed 3,351-finding strict GL inventory against actual
   reachability and presentation authority; the strict exit remains 1.
2. Reproduce and sample the current hero-mirror semantic-UI/shutdown hang before
   changing its guard; do not remove the guard from an old fixture alone.
3. Continue controlled live parity and stability. Keep projector-visible and
   remaining shadow coverage open unless Preferences/save/restart-qualified,
   and keep exact RLVa parity deferred in M-013.
