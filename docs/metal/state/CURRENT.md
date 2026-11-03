# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `ae9cdcd1f7`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `ae9cdcd1f7`
makes native Metal the only Darwin world-view HDR bloom, halation and
cross-filter authority. Configured native demand no longer depends on retained
GLSL readiness, and Darwin's world finalizer no longer generates or composites
the retained GL bloom pyramid. A native failure withholds the complete display
transaction rather than substituting GL. Every retained non-Darwin world path
remains unchanged.

This boundary is deliberately world-only. The material-preview dynamic texture
still generates retained GL luminance, exposure, bloom and color correction,
then adopts its separately rendered native preview only after the GL post-render
reports success. Its bloom shader programs and `generateBloomHDR` therefore
remain live on Darwin and must not be removed until preview publication no
longer depends on that retained transaction.

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
GLSL-readiness helper remain absent from the Darwin objects. Physical bloom
coverage checks the production extract/downsample/upsample pyramid, strength,
halation, cross-filter composition and malformed resource handling.
Strict inventory is 2,408 files / 3,262 findings: the preprocessor-unaware
scanner now counts both the Darwin ordinary-blit line and the non-Darwin
conditional-blit line, so the one-finding increase does not represent a new
live GL owner. Exact evidence is
[archived](archive/CURRENT-native-world-bloom-owner-checkpoint-2026-11-03.md).

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
  source `ae9cdcd1f7d2fcb4d414444abc1ac8fb27f3f806`, producing Development
  build `26.4.0d68165` (`26.4.0.68165`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `e45db46afe804be11f6577aef5e88dcc97e52fd1fe934c75427e64e493510a62`.
  Appearance-utility SHA-256 is
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1` and shader validation, the complete native Metal
  suite passed 52,381 assertions across 55 cases. Focused production-pyramid
  bloom/halation/cross-filter coverage passed 67 assertions. Established DoF,
  CAS and complete final-presentation coverage remains in the complete suite.
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
  `536fd9b2a39e6ffa41faf3ae0ad715d37e4c6f74835bc19746b47556b28a3dfd`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility;
  neither has an undefined `CGLTexImageIOSurface2D` dependency.
- `nm` finds no packaged-viewer symbol for the six retained
  FXAA/SMAA/CAS/sharpen globals, the six retained DoF globals,
  `LLPipeline::renderDoF` or `depthOfFieldProgramsReady`.
- Exact object inspection finds the retained bloom program objects and
  `generateBloomHDR` still present, with the material-preview object referencing
  the generator. This is a live preview dependency, not a world-view fallback.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color grading, tone curve, BRDF, post FXAA/SMAA/CAS, DoF and world-view HDR bloom/cross-filter are native-only on Darwin; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,262 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The established resource list plus pipeline BRDF, complete post-AA/CAS, DoF and world bloom transactions own no GL name or execution path on Darwin; material-preview bloom and live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Darwin world bloom no longer depends on
legacy program completeness or executes a parallel GL pyramid/composite.
Native Metal owns the production HDR pyramid, strength, halation and
cross-filter variants, with algorithm and failure behavior physically
regressed. Configured native failure suppresses exact frame publication.

**Still incomplete:** the executable links OpenGL; the material-preview path
still executes retained GL exposure, bloom, color correction and presentation
before it may adopt the native preview, and application-owned GL context,
traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** cut the material-preview publication lifecycle off
its retained GL post transaction. Make native preview success sufficient for
the dynamic texture update, retain fail-closed behavior, and only then remove
the now-unused Darwin bloom/exposure/color-correct program lifecycle.

## Exact next actions

1. Trace `LLGLTFPreviewTexture::render`/`postRender`, dynamic-texture success and
   `adoptMetalRenderTexture` ownership end to end.
2. Let a complete native PBR preview publish without first running retained GL
   luminance/exposure/bloom/color-correct/final-blit work; preserve explicit
   failure and refresh semantics.
3. Remove only Darwin shader/resource lifecycle proven unreachable afterward,
   then run physical preview and full source-exact release validation.
