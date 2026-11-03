# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `88f1677884`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `88f1677884`
cuts the complete native-eligible Darwin PBR material preview off retained GL
geometry, post processing and framebuffer publication. Darwin attempts the
native scene first; once it produces an exact target, the preview does not bind
the auxiliary GL screen, select the GL PBR shader, issue the sphere draw,
acquire a GL post target, run retained post work or copy the retained
framebuffer. Non-Darwin behavior is unchanged.

The eligible path also skips global local-light changes, retained default-probe
uniform updates, hardware-light setup and GL light-zero mutation; the fallback
callback alone performs and restores them.

This eligibility-bounded cut requires HDR PBR mode, active post, complete
geometry/textures, default-probe state and a compatible destination. Classic,
non-HDR, post-suppressed or incomplete/failed-native cases invoke the GL
draw/post callback. Shared pre-render/camera/matrix/clear setup still precedes
native submission, so retained bloom remains live and must not yet be removed.

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
[archived](archive/CURRENT-native-material-preview-light-state-checkpoint-2026-11-03.md).

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
  source `88f1677884181e0b8b3524175dab76c545cb2183`, producing Development
  build `26.4.0d68171` (`26.4.0.68171`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `fd10040bf4fc8c110f9cbbfc8b45d8f569256da5ba8f3c126baa9ddf8435ab5c`.
  Appearance-utility SHA-256 is
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1` and shader validation, the complete native Metal
  suite passed 52,381 assertions across 55 cases. Focused complete PBR material
  preview coverage passed 201 assertions. Established world bloom, DoF, CAS and
  final-presentation coverage remains in the complete suite.
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
  `589b98fbc15cee351f34a264f75341e80b762d9bdd071958f469bdfc67967019`.
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
| M-011 GL authority removal | doing | The established resources plus pipeline BRDF, post-AA/CAS, DoF and world bloom own no GL path; eligible PBR previews skip GL shader/geometry/light/probe/post/publish, while unsupported-mode fallback and shared GL viewport/matrix/clear setup remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A complete native PBR preview now runs
before and instead of the retained GL shader/sphere draw, and no longer waits
for retained GL post success or performs a redundant framebuffer copy. Metal
owns its material shading, exposure, HDR bloom, halation, cross filter, tone
mapping and publication, with those semantics physically regressed.

**Still incomplete:** the executable links OpenGL; unsupported native preview
eligibility modes retain the GL sphere/post fallback, shared preview
pre-render/viewport/matrix/clear setup still mutates GL state, and
application-owned GL context, traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** derive the native preview camera and matrices on
the CPU and defer dynamic-texture GL pre-render/clear/state setup into the
fallback, then extend native coverage toward classic/non-HDR/post-suppressed
configurations. Preserve unsupported-mode behavior until physically covered.

## Exact next actions

1. Derive native camera/modelview/projection on the CPU without changing preview framing or material semantics.
2. Defer dynamic-texture GL pre-render, clear/mask/state setup and matrix loads
   into the retained callback; keep it for unported or failed-native modes.
3. Extend the native scene contract to classic/non-HDR/post-suppressed modes,
   then remove GL preview resources/programs once reachability proves dead.
