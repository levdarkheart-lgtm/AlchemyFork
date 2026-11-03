# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `a345287be3`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commits `94a3771606`
and `a345287be3` finish native ownership of Darwin post antialiasing. The
resource plan requests none of SMAA's five retained GL lookup, sample or
intermediate resources. The retained final-post branch neither acquires an AA
target nor executes FXAA/SMAA, and Darwin eligibility no longer consults any
legacy FXAA/SMAA GLSL program.

`RenderSMAAUseSample` remains functional. `SamplePath` is decoded and uploaded
as a private RGBA8 Metal texture with path, size, modification time and device
identity. One- through four-channel inputs preserve the retained GL base-format
conversion. The sample feeds only edge detection; output dimensions and
neighborhood blending still use the actual display source. Missing, changed or
invalid input makes native publication fail closed and retries periodically
without exposing stale or partial AA.

Metal pipeline creation and the complete native FXAA/SMAA transactions remain
fail-closed authority. Physical regressions compare edge detection and the
complete transaction, lookup-driven blend weights and linear-light neighborhood
blending against independent CPU references. Split-source coverage proves the
sample changes edge/weight discovery while the final colors come from the real
frame. Exact
resource-plan tests cover no-AA, FXAA, normal/sample SMAA and stencil
combinations. Strict inventory remains 2,408 files / 3,261 findings because the
textual audit is preprocessor-unaware even though the Darwin GL bodies compile
out. Exact evidence is
[archived](archive/CURRENT-native-antialiasing-owner-checkpoint-2026-11-03.md).

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
  source `a345287be3fdd2ac432afb6408da89c73a9f70b5`, producing Development
  build `26.4.0d68158` (`26.4.0.68158`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `5001ca71da5d0b27b28c250c051e806e9e5aea877ca0ac9118ebfbfa8fb779d0`.
  Appearance-utility SHA-256 is
  `c3e87e6a8f87c09c44e61aee502f6afd22e5f4bd1b82dd8c952c2fa05fba89a5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1` and shader validation, the complete native Metal
  suite passed 52,381 assertions across 55 cases. Focused SMAA cases include
  2,923 split-source edge/transaction assertions; the compact unorm upload
  case passed 46 assertions.
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
- Strict inventory is 2,408 files / 3,261 findings, expected exit 1. The exact
  report SHA-256 is
  `2114759ada7817a54821d55e9dab49ee076dcfa119b5c4caaa2bc3f1320606f8`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility;
  neither has an undefined `CGLTexImageIOSurface2D` dependency.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color grading, tone curve, BRDF and post FXAA/SMAA are native-only on Darwin; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,261 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The established resource list plus pipeline BRDF and complete post-AA transaction own no GL name or execution path on Darwin; CAS and live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Darwin FXAA and normal/sample SMAA no
longer depend on legacy program completeness or execute parallel GL AA. Native
Metal owns all post-AA resources and passes, with split-source and fail-closed
behavior physically regressed.

**Still incomplete:** the executable links OpenGL; CAS is native but still
depends on and executes parallel retained GLSL sharpening, and application-owned
GL context, traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** cut CAS over at the adjacent display boundary.
Demand native CAS whenever the configured sharpness is nonzero, compile out
Darwin's standalone GLSL pass and suppress inline GL blit sharpening so one
owner determines the result. Keep native failure fail closed and preserve
non-Darwin behavior.

## Exact next actions

1. Remove `gCASProgram` from Darwin native demand, compile the standalone GL
   CAS body out and prevent the retained final blit from selecting its inline
   sharpen variant.
2. Re-run existing physical CAS oracles and complete source-exact validation,
   then select the next live display-post owner from source reachability.
