# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `0971551338`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `0971551338`
removes the three duplicate Darwin GL SMAA lookup names from ordinary SMAA.
The Darwin resource plan omits area, search and sample textures in normal mode,
the retained GL post does not run its parallel SMAA transaction, and native
display publication owns the private Metal area/search tables and complete
edge/weight/neighborhood sequence.

The explicit `RenderSMAAUseSample` feature is preserved rather than silently
deleted. Enabling it restores all three retained GL textures, runs the original
GL SMAA path and rejects native world publication because native SMAA does not
yet consume `SamplePath`. A normal native SMAA failure similarly rejects native
publication and leaves the retained full-color GL source authoritative, without
publishing a partial native AA result.

Physical Metal regressions compare edge detection and the complete transaction,
lookup-driven blend weights and linear-light neighborhood blending against
independent CPU references. Exact resource-plan tests cover no-AA, FXAA, normal
SMAA, sample-image SMAA and stencil combinations. Strict inventory remains
2,408 files / 3,259 findings because retained sample/non-Darwin GL source is
still present. Exact evidence is
[archived](archive/CURRENT-native-smaa-lookups-checkpoint-2026-11-03.md).

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
  source `0971551338534843ae13516f98183bb781c230f3`, producing Development
  build `26.4.0d68153` (`26.4.0.68153`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `a78f67bfcfa9da7ed2334ece2de2ad1098236560c1c4cb1124ff16017cd67191`.
  Appearance-utility SHA-256 is
  `514308e32e9afd6e17c72c5545061749b8bc1d8bb35205ccca471a11623c864a`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,345
  assertions across 55 cases with API validation enabled. Focused SMAA cases
  passed 2,916 edge/transaction, 16,421 lookup-weight and 419 neighborhood
  assertions.
- The full `alrender_tests` suite passed 557,320 assertions across 137 cases.
  The focused SMAA resource-plan case passed 60 assertions.
- The source-exact fresh-context renderer suite passed 2,243 assertions across
  94 cases. The CPU-only renderer suite passed 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,259 findings, expected exit 1. The
  precommit and source-exact reports are byte-identical at
  `daaee20b8acabf2bafe4391b09c5fe5a60082703bbefcefca5a948ca439eeca9`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility;
  neither has an undefined `CGLTexImageIOSurface2D` dependency.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color grading, tone curve, BRDF and ordinary SMAA lookups are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,259 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The established resource list plus pipeline BRDF and normal SMAA lookup textures own no GL texture on Darwin; sample-image SMAA and live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Ordinary Darwin SMAA no longer creates
or consumes three parallel GL lookup names. Native Metal owns the lookup tables
and transaction, while the unsupported sample-image variant remains exact and
explicitly GL-authoritative instead of being deleted or misrepresented.

**Still incomplete:** the executable links OpenGL; sample-image SMAA still owns
GL tables and application-owned GL context, traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** finish the SMAA owner. Add native `SamplePath`
ingress and a distinct edge-detection source while preserving neighborhood
blending against the actual display source. Then remove the sample exception,
the three Darwin GL lookup names and the retained Darwin GL SMAA transaction.

## Exact next actions

1. Decode and publish the SMAA sample image as an exact native texture, with
   source identity, component/size validation, failure/retry state and physical
   orientation/content coverage.
2. Extend native SMAA so edge detection may read that sample while neighborhood
   blending reads the display source; keep native publication fail-closed, then
   eliminate the last Darwin SMAA lookup GL exception.
