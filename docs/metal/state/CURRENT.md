# Current engineering state

**Observed:** 2026-11-01 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `b328cba4b6` (`feat(metal): replay object update blips natively`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008 remains `doing`; M-012 is complete. Commit `b328cba4b6` replaces the
reachable developer **Show Updates to Objects** fallback with native Metal
replay. `LLPipeline::renderDebug()` captures each surviving blip after its age
and Z-motion update. The final native world pass replays the retained positions
and colors as eight-pixel points after Highlight Transparent and before display
post, matching the GL owner's RGB-only color mask, always-pass depth test,
depth write and white-texture behavior. Separate HDR/non-HDR pipelines preserve
the attachment formats. Invalid or unrepresentable capture data fails native
authority closed; the migration GL draw remains intact.

The same source audit corrected an earlier reachability assumption. This OSS
ARM64 viewer links only `llphysicsextensionsos`: `LLPathingLib::isFunctional()`
is hard-false, its stub initialization returns `LLPL_NOT_IMPLEMENTED`, and its
singleton remains null. Therefore the checked-in pathfinding render bodies do
not produce retained-Alchemy output in this product build. Rebuilding the
missing proprietary implementation is not an OpenGL-removal task. The latent
eligibility guard from `daa19101a9` remains harmless if a future functional
extension is supplied, but pathfinding is not a release-critical next owner.
The render-debug, wireframe, reflection-probe-volume and far-plane boundaries
remain unchanged and are the bounded candidates for the next source-led audit.

Prior commit `8361addd0f` closes a
reachable native-output omission in the viewer's **Highlight Transparent**
diagnostic. `LLDrawPoolAlpha::renderDebugAlpha()` rendered static and rigged
alpha classifications after the retained native world replay, and it also
rendered HUD attachments inside the ordered UI transaction. Native publication
could nevertheless remain eligible without either overlay, so a published
Metal frame could silently omit a core editing/diagnostic feature.

The exact draw boundary now captures the smoke texture, transform, red/blue/
green classification and avatar skin palette. World replay selects static or
rigged Metal pipelines and uses an RGB-only write mask so diagnostic blending
does not corrupt retained glow/coverage alpha. HUD replay uses the ordered UI
renderer with the live GL blend, depth, cull, sampler, viewport and color-mask
state. Missing geometry, textures or palettes fail the corresponding native
transaction closed while the migration GL draw remains intact.

This is a core native-output repair, not exact RLVa work. RLVa can gate whether
the viewer exposes Highlight Transparent, but exact RLVa behavior remains the
owner-deferred M-013 nice-to-have. Minimum RLVa support is required only where
needed to prevent Metal authority or viewer-owned OpenGL removal from blocking.

The preceding bounded transparency/material production audit is closed without
a speculative shader edit. Active legacy material and PBR alpha equations,
cutoffs, sampler/color-space use, specular AA, shadows, probes/SSR/hero inputs,
glare-derived coverage, fog, local lights, custom blend state and glow replay
showed no reachable divergence for valid viewer-produced data. The concrete
Highlight Transparent omission was the next missing output owner.

Any candidate/reference shadow comparison is admissible only when shadows are
enabled in Graphics Preferences in **both** viewers before capture. A both-off
control can isolate a shadow residual, but it cannot prove shadow parity.

The persistent M-013 inventory is [the deferred RLVa backlog](../work/deferred/rlva-parity.md).
It records the mandatory comparison prerequisite: enable RLV in Preferences in
both viewers, save, and restart both viewers before testing.

## Build and test evidence

- Focused physical RelWithDebInfo world replay passed 1,385 assertions under
  Metal API/GPU/load-store/shader validation. It covers asymmetric red/green
  blips, exact eight-pixel footprints, RGB-only alpha preservation, 0.5 depth
  writes, unchanged background, missing-storage rejection and nonfinite-input
  rejection in addition to the retained world corpus.
- Complete RelWithDebInfo, Debug and Release physical Metal suites each passed
  45,035 assertions across 52 cases under the same validation layers. The
  six-assertion policy case proves each still-uncaptured overlay class rejects
  exact publication while native blips remain eligible.
- Warnings-as-errors RelWithDebInfo objects for the Metal renderer, built-in
  MSL, GPU test and `pipeline.cpp` compiled and linked; the updated objects also
  compile in Debug and Release.
- `ctest --test-dir build-Darwin-ninja-os-arm64 -C RelWithDebInfo -LE
  'manual|gl' --output-on-failure` passed 484/484 in 244.24 seconds.
- Source-exact warnings-as-errors Release Development build `26.4.0d68040`
  linked, staged and signed at committed code head `b328cba4b6`.
- Executable SHA-256:
  `fcba555e47abed64eedafa6f60f5b577104ffeb2a8c0f1692f7e813082e82d32`.
  Packaged MSL SHA-256:
  `ab73e9f26014d8ec13d7e02bf78cf5e23a0f24c75337779d8db3ddd6b9d57146`;
  compiled metallib SHA-256:
  `7ca21604f3fe91e3755df3e034266c1cd2b4ce766b2bd31c86d2d90ba3905b30`.
- Deep strict signing passed; `tools/metal/verify_app.sh` verified all 349
  Mach-O slices as ARM64, and the main executable reports arm64.
- `tools/metal/doctor.sh` passed on Apple M1 Max with Apple7-family Metal and
  unified memory. Harness validation and tooling tests pass after this
  documentation checkpoint.
- The main executable still links both Metal and OpenGL during migration.
  Strict no-GL remains an open release gate; no native-only claim is made.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native world/UI/resource owners and physical regressions exist; complete parity remains open. |
| M-007 world/material | doing | Bounded active transparency/material shader audit found no valid-production divergence; broader live alpha/material parity remains open. |
| M-008 advanced views/effects | doing | Highlight Transparent and object-update blips are native. OSS pathfinding is a nonfunctional stub, not a reachable output owner. Broader reachable debug/shadows/post/output parity remains open; exact RLVa parity is M-013. |
| M-009 parity/stability/no-GL | todo | Full feature matrix, prolonged stability and strict owned-OpenGL elimination are not satisfied. Remaining reachable render-debug, wireframe, reflection-probe-volume and far-plane owners stay fail-closed pending audit/porting. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max performance claim exists. |
| M-011 GL authority removal | deferred | Keep fail-closed GL traversal until each owner has parity and side-effect evidence. |
| M-012 Computer Use bridge | complete | Exact-path semantic login/in-world/control/logout operation is verified and used for acceptance. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have. The persistent backlog records exact remaining work and the two-viewer restart prerequisite. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Native publication now reproduces
transient object-update blips with their authoritative point footprint, color,
alpha mask and depth behavior. Highlight Transparent remains natively covered
for static/rigged world and HUD geometry. The audit also removes a false
release target: OSS pathfinding is stubbed and cannot contribute output.

**Deferred critical work:** remaining non-RLVa advanced-view/post/output
owners; controlled live transparency/material and editing comparisons; full
feature/stability matrices; viewer-owned OpenGL removal; and paired quality-
equivalent M1 Max measurements. Independent build/review evidence and
reproducible distinct release proof also remain open. Exact RLVa behavior/image
parity is separately deferred as non-critical M-013.

**Evidence-based priority:** continue the bounded production-call audit across
the still-reachable fail-closed owners: general render-debug modes, wireframe,
reflection-probe-volume visualization and the far-plane overlay. Select the
smallest owner that actually emits uncaptured output, preserve its exact state
and ordering, and avoid speculative ports for inactive or already-captured
paths.

## Exact next actions

1. Trace the production call sites and contribution conditions for general
   render-debug, wireframe, reflection-probe-volume and far-plane output.
2. Implement the smallest confirmed reachable missing owner with asymmetric
   physical state/output proof; leave every uncovered owner fail-closed.
3. Continue strict no-GL work and controlled live parity; keep exact RLVa
   parity in M-013 and require shadows enabled in both viewers for shadow pairs.
