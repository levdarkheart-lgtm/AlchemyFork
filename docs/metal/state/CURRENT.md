# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `9bac86a3e4`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `9bac86a3e4`
makes native Metal the only Darwin Depth of Field authority. Configured DoF no
longer depends on retained GLSL readiness, the retained gather/combine call and
body compile out, and the six DoF program objects are absent from the Darwin
shader-manager lifecycle and packaged viewer. Native failure withholds the
complete display transaction. Every retained non-Darwin DoF path remains
unchanged.

The prior native-only FXAA/SMAA/CAS checkpoint remains in force. Darwin
constructs, executes and exports no retained post-AA/CAS/sharpen program, owns
none of the SMAA resources and always uses the ordinary unsharpened GL final
blit behind the exact Metal presentation transaction.

`RenderSMAAUseSample` remains functional. `SamplePath` is decoded and uploaded
as a private RGBA8 Metal texture with path, size, modification time and device
identity. One- through four-channel inputs preserve the retained GL base-format
conversion. The sample feeds only edge detection; output dimensions and
neighborhood blending still use the actual display source. Missing, changed or
invalid input makes native publication fail closed and retries periodically
without exposing stale or partial AA.

Metal pipeline creation and the complete native FXAA/SMAA/CAS transactions
remain fail-closed authority. Physical regressions compare edge detection,
lookup-driven weights, linear-light neighborhood blending and CAS output
against independent CPU references. Split-source coverage proves the SMAA
sample changes edge/weight discovery while final colors come from the display
frame. Exact resource-plan tests cover no-AA, FXAA, normal/sample SMAA and
stencil combinations. Static symbol inspection proves the six retained
post-AA/sharpen and six DoF program objects, `LLPipeline::renderDoF` and its
GLSL-readiness helper are absent from the packaged Darwin viewer.
Strict inventory is 2,408 files / 3,262 findings: the preprocessor-unaware
scanner now counts both the Darwin ordinary-blit line and the non-Darwin
conditional-blit line, so the one-finding increase does not represent a new
live GL owner. Exact evidence is
[archived](archive/CURRENT-native-dof-owner-checkpoint-2026-11-03.md).

Prior advanced-output, world/material, shadow, UI and presentation owners remain
as recorded in the archive. Opaque fullbright and the six-face cube convention
have physical regressions; one controlled dry authored-mirror sample is
owner-confirmed. Legacy BLEND/stained glass, broader mirrors,
projector-visible shadows, moving media and complete parity remain open.

## Visual review cadence

Per owner direction, Codex does not independently drive reference/candidate
viewers or take comparison snapshots. Visual cases accumulate in the
[owner-attended daily review queue](../work/active/visual-review-queue.md) and
are reviewed together at most once per day. Source inspection, builds, physical
GPU tests, static/link audits and profiling continue between reviews. No queued
case is a parity pass until the owner-attended review records that result.

## Build and test evidence

- `tools/metal/doctor.sh` passed on the M1 Max host with 64 GiB unified memory,
  Apple7-family support and the required Metal capabilities.
- The required default warnings-as-errors Release wrapper completed at exact
  source `9bac86a3e47ae2aaf272df6bc4a98fe254efb667`, producing Development
  build `26.4.0d68163` (`26.4.0.68163`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `2ff05074d265a2d17717513b38f4657616b9564e09cfc1a87f50ca32388b0b9a`.
  Appearance-utility SHA-256 is
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1` and shader validation, the complete native Metal
  suite passed 52,381 assertions across 55 cases. Focused all-variant DoF
  gather/combine coverage passed 2,493 assertions. Established CAS and complete
  final-presentation coverage remains included in the complete suite.
- The full `alrender_tests` suite passed 557,312 assertions across 137 cases.
  The focused resource-plan matrix passed 3,376 assertions across 37 cases.
- The source-exact fresh-context renderer suite passed 2,243 assertions across
  94 cases. The CPU-only renderer suite passed 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,262 findings, expected exit 1. The exact
  report SHA-256 is
  `d17b875fcc3daa97f616b06d455d22c6c2760eb703310909acb33ef5db085db1`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility;
  neither has an undefined `CGLTexImageIOSurface2D` dependency.
- `nm` finds no packaged-viewer symbol for the six retained
  FXAA/SMAA/CAS/sharpen globals, the six retained DoF globals,
  `LLPipeline::renderDoF` or `depthOfFieldProgramsReady`.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color grading, tone curve, BRDF, post FXAA/SMAA/CAS and DoF are native-only on Darwin; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,262 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The established resource list plus pipeline BRDF, complete post-AA/CAS and DoF transactions own no GL name or execution path on Darwin; bloom and live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Darwin DoF no longer depends on legacy
program completeness or executes a parallel GL gather/combine transaction.
Native Metal owns all near/far, shaped/unshaped, HDR-format and depth variants,
with algorithm and failure behavior physically regressed. Its unused retained
shader objects and GL methods are no longer part of the Darwin binary.

**Still incomplete:** the executable links OpenGL; HDR bloom and cross-filter
still consult retained GLSL readiness and execute parallel GL paths, and
application-owned GL context, traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** cut HDR bloom and cross-filter over at the adjacent
world-post boundary. Make native demand depend on configured effect/resources,
not the retained bloom/cross GLSL programs; compile the Darwin GL pyramid and
streak passes out and remove their unused program lifecycle. Preserve native
fail-closed publication, halation/streak variants and non-Darwin behavior.

## Exact next actions

1. Trace `bloomHDRReady`, `generateBloomHDR`, cross-filter target ownership and
   color-correct composite reachability end to end.
2. Make Darwin native bloom/cross demand independent of retained shader
   completeness, compile out the retained pyramid/streak transaction and remove
   only the now-unused Darwin bloom/cross shader lifecycle.
3. Run the physical bloom/halation/cross-filter oracles and repeat the complete
   source-exact release checkpoint.
