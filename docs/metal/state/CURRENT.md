# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `0464f5466e`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `0464f5466e`
removes Darwin OpenGL ownership from the CPU-generated sky and shiny face pairs.
Their explicit linear RGBA8 storage now creates and refreshes through Metal,
preserves current/next double buffering and survives GL cleanup; restore
republishes both native buffers.

Physical regressions distinguish explicit linear `rgba8_unorm` sky storage from
sRGB parcel storage, retain exact all-channel/outside-region RGBA update proof
and confirm no GL name exists. Failed in-place sky/parcel updates retain the
prior complete native texture for retry.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,259 findings and exits 1. The one-finding
delta is the native `LLImageGL*` handle line classified heuristically as a
shader symbol, not a GL call. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency. Exact evidence is
[archived](archive/CURRENT-native-sky-faces-checkpoint-2026-11-03.md).

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
  source `0464f5466ec53c94fb9b9e736e26420afe363020`, producing Development
  build `26.4.0d68139` (`26.4.0.68139`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `02401ca36b16f8ee4622cf50c8414906b69c1e38be77edca628672c6db0ca20a`.
  Appearance-utility SHA-256 is
  `5769d937be848ee0b6edf9c153eefb0b0107898f90bc42fb377399e2ff7b337e`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,221
  assertions across 52 cases with API validation enabled.
- The source-exact renderer suite passed 2,191 assertions across 92 cases with
  Metal API validation enabled, including explicit linear RGBA8 descriptor
  proof plus exact RGBA update and prior typed/mip regressions.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,259 findings, expected exit 1. Both
  OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel and CPU sky resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,259 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Media, terrain, water, parcel and generated sky faces own no GL texture on Darwin; live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. CPU-generated sky/shiny faces now own
only linear Metal storage on Darwin, preserve double-buffer/cubemap consumers
and no longer disappear during GL cleanup. Restore deterministically
republishes both buffers.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 at `LLNetMap`. Its CPU-painted
object and parcel layers allocate from raw images and refresh through GL even
though native UI capture consumes the same logical textures.

## Exact next actions

1. Convert `LLNetMap` object/parcel image creation and refresh to native-only
   RGBA while preserving dynamic sizing, scale and update cadence.
2. Keep both logical textures stable for native UI capture and retry failed
   publication without publishing incomplete CPU layers. Queue visual review
   only after deterministic implementation evidence exists.
