# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `ab90edefd0`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `24b50cc382`
extends the native material-preview transaction across classic skies with the
retained atmospheric color-space, white-sun 1.35 boost, direct-light
recombination and classic probe-diffuse suppression. Commit `ab90edefd0`
separates ordinary Metal texture/default-probe publication waits from actual
device, geometry, encode or publication failures.

Native preview destination backing remains Metal-only with a zero GL texture
name. Every supported HDR, non-HDR, post-suppressed, legacy-gamma and classic
lighting configuration now selects the native owner. Ordinary producer
readiness retries targetlessly and does not schedule the GL probe/post pack.
Only a real native failure requests the one-shot lazy retained recovery path.

This is a broader production-mode cut, not full no-context execution proof.
Other dynamic textures retain GL backing, preview failure recovery remains,
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
Strict inventory remains 2,408 files / 3,262 findings. Exact classic-lighting
and readiness evidence is
[archived](archive/CURRENT-native-classic-preview-and-readiness-checkpoint-2026-11-03.md).

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
  source `ab90edefd033abc24d6d535740f422ee4589ed0a`, producing Development
  build `26.4.0d68188` (`26.4.0.68188`). Exact RelWithDebInfo objects for the
  preview manager and pipeline also compile warnings-as-errors.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `e23aed414f4433469260bb024c230ff9d0badc1081800738c336e0d2a4d7ea8d`.
  Appearance-utility SHA-256 is
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- Under `MTL_DEBUG_LAYER=1` and shader validation, the complete native Metal
  suite passed 52,414 assertions across 55 cases. Focused lit/PBR material
  preview coverage passed 459 assertions across two cases. Established world
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
  `462ca6658bdddfa0a760200742e3ac75b22166b7a1ea12cd5a8096c3764b9c07`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility;
  neither has an undefined `CGLTexImageIOSurface2D` dependency.
- `nm` finds no packaged-viewer symbol for the six retained
  FXAA/SMAA/CAS/sharpen globals, the six retained DoF globals,
  `LLPipeline::renderDoF` or `depthOfFieldProgramsReady`.
- Exact source inspection finds the retained bloom program objects and
  `generateBloomHDR` still present only for explicit native-failure recovery;
  no supported preview configuration or ordinary producer wait selects them.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color grading, tone curve, BRDF, post FXAA/SMAA/CAS, DoF and world-view HDR bloom/cross-filter are native-only; all supported PBR preview modes own geometry/post/publish. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,262 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | All supported preview modes own Metal storage/post; readiness stays native and GL is failure-only recovery. Other dynamic textures and wider traversal still own GL paths. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Classic and modern preview lighting now
share the native geometry/post/publication owner. Producer readiness no longer
allocates or executes the retained probe/post pack, so all supported modes stay
Metal-only while waiting for coherent inputs.

**Still incomplete:** the executable links OpenGL; global GL teardown also
destroys pipeline state, so this is not a runtime no-context viewer claim.
Actual native failures still retain the GL sphere/post recovery transaction.
Application-owned GL context, traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** replace the now-isolated preview failure recovery
with native retry/failure reporting and remove its retained resource/program
reachability, then address broader pipeline lifetime under GL teardown.

## Exact next actions

1. Remove Darwin preview failure recovery's retained sphere/post transaction
   and its resource demand while keeping native retry fail-closed.
2. Separate broader native pipeline lifetime from GL teardown before claiming
   complete no-context execution.
3. Continue owner-by-owner OpenGL inventory reduction with exact tests and
   package evidence; keep visual questions queued for the daily owner session.
