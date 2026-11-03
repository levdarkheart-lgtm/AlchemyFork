# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `f3f341e0be`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `f3f341e0be`
finishes native ownership of normal and sample-image SMAA on Darwin. The
resource plan never requests the three GL lookup/sample names, the retained GL
post never runs its parallel SMAA transaction, and native display publication
owns the private area/search tables plus the complete edge/weight/neighborhood
sequence.

`RenderSMAAUseSample` remains functional. `SamplePath` is decoded and uploaded
as a private RGBA8 Metal texture with path, size, modification time and device
identity. One- through four-channel inputs preserve the retained GL base-format
conversion. The sample feeds only edge detection; output dimensions and
neighborhood blending still use the actual display source. Missing, changed or
invalid input makes native publication fail closed and retries periodically
without exposing stale or partial AA.

Physical Metal regressions compare edge detection and the complete transaction,
lookup-driven blend weights and linear-light neighborhood blending against
independent CPU references. Split-source coverage proves the sample changes
edge/weight discovery while the final colors come from the real frame. Exact
resource-plan tests cover no-AA, FXAA, normal/sample SMAA and stencil
combinations. Strict inventory is 2,408 files / 3,261 findings: the textual,
preprocessor-unaware audit adds two native device references while the Darwin
GL bodies are compiled out. Exact evidence is
[archived](archive/CURRENT-native-smaa-sample-checkpoint-2026-11-03.md).

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
  source `f3f341e0bec64574e8d37ee310168e118c492ddd`, producing Development
  build `26.4.0d68155` (`26.4.0.68155`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `ed52efe8a51a52d88a5c17cbbab59593b4c4c9ee1c834e639a4311da80515d19`.
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
- The full `alrender_tests` suite passed 557,320 assertions across 137 cases.
  The focused resource-plan matrix passed 3,384 assertions across 37 cases.
- The source-exact fresh-context renderer suite passed 2,243 assertions across
  94 cases. The CPU-only renderer suite passed 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,261 findings, expected exit 1. The
  precommit and source-exact reports are byte-identical at
  `5d7d1c6ee0d83f602a2055b1f884c4e570262fddbf3d7515acdf6d7ee182d904`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility;
  neither has an undefined `CGLTexImageIOSurface2D` dependency.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color grading, tone curve, BRDF and normal/sample SMAA are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,261 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The established resource list plus pipeline BRDF and all SMAA textures/transactions own no GL name on Darwin; legacy SMAA GLSL eligibility and live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Darwin normal and sample-image SMAA no
longer create or consume the parallel GL lookup/sample names or execute the GL
transaction. Native Metal owns SamplePath ingress and every SMAA pass, with
split-source and fail-closed behavior physically regressed.

**Still incomplete:** the executable links OpenGL; SMAA eligibility still
consults legacy GLSL program completeness, and application-owned GL context,
traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** remove SMAA's last Darwin dependency on legacy
GLSL program completeness and omit its dead GL transient descriptions from the
Darwin plan. Native pipeline creation and transaction failure remain the exact
fail-closed authority. Then reassess the next live GL owner from source and
strict inventory rather than deleting fallback behavior speculatively.

## Exact next actions

1. On Darwin, admit SMAA from native resource readiness rather than
   `gSMAA*Program.isComplete()` and omit `SMAAEdges`/`SMAABlend` GL transient
   descriptions while preserving non-Darwin behavior.
2. Add exact resource-plan/eligibility regressions, rebuild and re-run physical
   Metal validation, then select the next live GL owner from measured source
   reachability.
