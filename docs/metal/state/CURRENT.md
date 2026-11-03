# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `e213e9ac01`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `e213e9ac01`
removes Darwin OpenGL ownership from the pipeline color-grading LUT. Loaded
state is backend-neutral; Darwin publishes only the native `texture_2d_array`,
while non-Darwin retains the `ALTexture3D` implementation for later platform
pruning. The retained GL post path disables the LUT sampler when no GL object
exists instead of dereferencing absent storage; native world grading continues
to consume the Metal resource.

The shared native uploader accepts 8- or 16-bit RGB/RGBA unorm volumes, expands
RGB to opaque RGBA, maps each Z slice to one Metal array layer and publishes
only after upload commit. Both 16-bit `.cube` and 8-bit image-strip inputs use
that path on Darwin. Physical validation proves exact RGB8 expansion and every
voxel/layer, exact RGBA16 precision and ordering, descriptors and invalid-input
rejection. Failed native allocation leaves the grade unavailable rather than
publishing loaded state.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,259 findings and exits 1. The color-LUT
cutover changes an owned Darwin path without changing the cross-platform static
inventory total. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency.
Exact evidence is
[archived](archive/CURRENT-native-color-lut-checkpoint-2026-11-03.md).

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
  source `e213e9ac01d24efbdb5b587e818b24b845a456cb`, producing Development
  build `26.4.0d68147` (`26.4.0.68147`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `72bb699014c3e2b8eaf8dd882ccdd2a41ce5d9deb7cdcb83b6f93aa308704727`.
  Appearance-utility SHA-256 is
  `3449cb8fb684d944cfb7f813f6379b6280b2060635ee6715ef452e01a1d951cd`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,272
  assertions across 53 cases with API validation enabled. Its new physical case
  contributes 51 assertions proving both native LUT precision paths and invalid
  input rejection.
- The source-exact fresh-context renderer suite passed 2,243 assertions across
  94 cases with Metal API validation enabled.
- The CPU-only renderer suite passed 1,513 assertions across 203 cases: 202
  passed and the established single-palette font case skipped as expected.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,259 findings, expected exit 1. The
  precommit report is
  `a8e681c2607c41595dc41da1329020ca29514676c8140daed419d3b73c6d9df9`;
  the source-exact report is
  `26f65a0e9a325d87dbf44ecac5b12d12cef4fc0ec3e5a475b1d204538adc0ab6`.
  Counts are neutral while matching source snippets differ. Both OpenGL and
  Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners and the color-grading LUT are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,259 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Media, terrain, water, parcel, generated sky faces, legacy sky cube, minimap layers, font atlases and the color-grading LUT own no GL texture on Darwin; live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Darwin color grading now has one GPU
authority: the native Metal volume-array texture. Both supported input formats
preserve their established precision and slice convention, and failed
publication cannot advertise a loaded LUT.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-008/M-011 at the baked tone-curve LUT.
Source inspection confirms Darwin still allocates and updates the GL
`mToneCurveLut` as RGBA16 alongside `mMetalToneCurveLut`, while the native world
path already consumes the Metal texture. Activation is still coupled to the GL
name, and the retained GL post samples that name.

## Exact next actions

1. Make baked tone-curve availability backend-neutral and allocate/update only
   `mMetalToneCurveLut` on Darwin, preserving non-Darwin GL behavior.
2. Keep the retained Darwin GL post safe when the GL LUT is absent, then prove
   exact RGBA16 samples, state transitions, identity handling and failure state.
