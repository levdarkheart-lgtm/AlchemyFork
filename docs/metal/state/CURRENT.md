# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `f5513b7d3f`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `f5513b7d3f`
extends the native material-preview transaction across modern non-HDR and
post-suppressed modes. It explicitly bypasses exposure/tonemapping and bloom,
applies legacy gamma when required, and reproduces static/animated non-HDR
dithering in encoded display space.

Native preview destination backing remains Metal-only with a zero GL texture
name. Classic-mode lighting is still fail-closed on retained rendering because
its compile-time atmospheric/direct-light convention is not yet represented.
Incomplete geometry, texture, probe, device or destination state and native
encode failures also retain lazy recovery.

This is a broader production-mode cut, not full no-context execution proof.
Other dynamic textures retain GL backing, classic preview fallback remains,
and global GL teardown also destroys wider pipeline state.

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
Strict inventory remains 2,408 files / 3,262 findings. Exact post-mode evidence
is [archived](archive/CURRENT-native-preview-post-modes-cutover-checkpoint-2026-11-03.md).

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
  source `f5513b7d3fc839847262d1e855a8b5af6248dfe9`, producing Development
  build `26.4.0d68185` (`26.4.0.68185`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `41eb926793007e4f5864f7dc65bb48740cebc7726a62fb60221261235ecb9a8a`.
  Appearance-utility SHA-256 is
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `177f5548a11f65cd33c6a5fcaa268a5267eb9ec83b3610a7d7ce722771967574`;
  metallib SHA-256 is
  `4cfc9ab99b6d500abfa0b8e76297d3cabed87b75a5efe11b34cf1634318662c3`.
- Under `MTL_DEBUG_LAYER=1` and shader validation, the complete native Metal
  suite passed 52,402 assertions across 55 cases. Focused lit/PBR material
  preview coverage passed 447 assertions across two cases. Established world
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
  `101f742754564f2c9c8a7b394e58df271ccf2da025eb1cb87d5da2663ef8e32c`.
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
| M-011 GL authority removal | doing | Modern HDR, non-HDR and post-suppressed previews own Metal storage/post; GL backing is lazy recovery. Classic preview lighting and other dynamic textures still own GL paths. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Modern non-HDR, clean-plate and
legacy-gamma previews no longer switch to GL for their post transaction. The
native owner now covers explicit tonemap bypass, encoded legacy gamma and
non-HDR dither while preserving Metal-only destination storage.

**Still incomplete:** the executable links OpenGL; global GL teardown also
destroys pipeline state, so this is not a runtime no-context viewer claim.
Classic preview lighting retains the GL sphere/post fallback.
Application-owned GL context, traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** implement exact classic preview lighting natively,
then distinguish failure-only recovery from production reachability before
removing retained preview programs.

## Exact next actions

1. Port classic preview atmospheric, direct-light and probe conventions with
   independent physical reference tests.
2. Separate broader native pipeline lifetime from GL teardown before claiming
   complete no-context execution.
3. Continue owner-by-owner OpenGL inventory reduction with exact tests and
   package evidence; keep visual questions queued for the daily owner session.
