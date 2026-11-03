# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `afda818d2d`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `afda818d2d`
removes Darwin OpenGL ownership from the `LLNetMap` CPU-painted object and
parcel layers. Dynamically sized logical textures now begin without GL storage,
publish complete linear RGBA8 images through Metal and refresh the native image
in place. Existing scale, redraw cadence and native UI sampled-texture consumers
are preserved.

Publication failure is non-destructive: the prior complete native texture and
its matching global center remain visible, while the freshly painted CPU layer
is retried on the next draw. Resize still replaces the logical texture exactly
where the prior implementation did. Existing physical RGBA creation/update and
no-GL-name regressions remain green.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,259 findings and exits 1. The one-finding
delta is the native `LLImageGL*` handle line classified heuristically as a
shader symbol, not a GL call. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency. The minimap owner adds no strict finding.
Exact evidence is
[archived](archive/CURRENT-native-minimap-layers-checkpoint-2026-11-03.md).

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
  source `afda818d2d428c3b8372a53837bb74a85a3917da`, producing Development
  build `26.4.0d68141` (`26.4.0.68141`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `96187c4eb4f7969ff64b917839064dbacdad5ab9d116841db0e1632a475d6d1c`.
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
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky and minimap layers are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,259 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Media, terrain, water, parcel, generated sky faces and minimap layers own no GL texture on Darwin; live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The live minimap object and parcel
layers now own only linear Metal storage on Darwin and retain their established
dynamic sizing, update cadence and UI sampling behavior. Failed publication
cannot pair stale pixels with a newly advanced map center.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-006/M-011 at `LLFontBitmapCache` and
`LLFontFreetype`. Native UI already consumes the atlas `LLImageGL` identity,
but each sheet still allocates from a raw image through GL and every new glyph
uses GL `setSubImage`.

## Exact next actions

1. Convert Darwin grayscale and color font-atlas sheet creation to native-only
   storage while preserving luminance-alpha swizzle and BGRA semantics.
2. Convert dirty glyph-rectangle publication to native subimage upload while
   preserving page reuse/eviction, retry behavior and CPU-only test mode.
