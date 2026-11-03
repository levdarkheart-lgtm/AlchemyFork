# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `bceb839ea5`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `bceb839ea5`
makes retained material-preview resources demand-driven on Darwin. Merely
creating an eligible preview no longer plans the retained probe pack, post
targets or bloom. A successful native preview completes without acquiring or
planning those GL resources.

The contract remains fail closed. A failed native attempt raises a counted
retained-fallback demand. Ready resources still permit same-frame fallback;
otherwise the preview stays pending while the next reconcile builds them,
without repeating the identical native attempt each waiting frame. Successful
fallback releases demand so a later reconcile can remove preview-only retained
resources. Multiple pending previews are reference-counted; non-Darwin keeps
live-preview planning.

Classic, non-HDR, post-suppressed or incomplete cases still retain the GL
callback. The dynamic-texture base still owns a GL texture, the global
GL-disabled scheduler guard still blocks targetless work, and the shared target
remains necessary for fallback and other dynamic textures.

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
Strict inventory remains 2,408 files / 3,262 findings. Exact retained-demand
evidence is [archived](archive/CURRENT-native-material-preview-demand-checkpoint-2026-11-03.md).

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
  source `bceb839ea5a9b55bb9d8d1917b2136d1218626b0`, producing Development
  build `26.4.0d68179` (`26.4.0.68179`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `eeec153f9092884ad9a9ee35ba88d03f18dcf3de583b3bd8e5787572965cc2a0`.
  Appearance-utility SHA-256 is
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1` and shader validation, the complete native Metal
  suite passed 52,381 assertions across 55 cases. Focused lit/PBR material
  preview coverage passed 426 assertions across two cases. Established world
  bloom, DoF, CAS and final-presentation coverage remains in the complete suite.
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
  `e0c8e680275632fd2f47f3b6df95e010e95ff252d7d5c291aa41a6aa8fa16603`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility;
  neither has an undefined `CGLTexImageIOSurface2D` dependency.
- `nm` finds no packaged-viewer symbol for the six retained
  FXAA/SMAA/CAS/sharpen globals, the six retained DoF globals,
  `LLPipeline::renderDoF` or `depthOfFieldProgramsReady`.
- Exact object inspection finds the retained bloom program objects and
  `generateBloomHDR` still present because unsupported material-preview modes
  retain that path. Native-eligible previews bypass it at source.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color grading, tone curve, BRDF, post FXAA/SMAA/CAS, DoF and world-view HDR bloom/cross-filter are native-only; complete native-eligible PBR previews own geometry/post/publish. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,262 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Successful eligible PBR previews neither require nor plan retained preview resources; failures demand them explicitly. Unsupported preview modes and dynamic textures still own GL paths. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A successful eligible PBR preview now
runs without retained auxiliary-screen/post resource allocation and owns camera,
material shading, exposure, bloom, tone mapping and publication. Preview-only
retained post/bloom resources are created only after explicit fallback demand
and released after a successful fallback; ready resources still support
same-frame recovery.

**Still incomplete:** the executable links OpenGL; unsupported native preview
eligibility modes retain the GL sphere/post fallback.
Application-owned GL context, traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** allow targetless native work to proceed when GL is
disabled while keeping retained items safely pending, then remove the preview's
dual GL texture ownership and extend unsupported preview-mode ownership.

## Exact next actions

1. Split the global GL-disabled scheduler guard so targetless Darwin work can run
   while retained-target items remain safely pending.
2. Remove dual GL texture ownership from native-only preview destinations without
   weakening the explicit fallback transaction.
3. Extend native ownership to classic/non-HDR/post-suppressed modes, then remove
   GL preview resources/programs only after reachability proves dead.
