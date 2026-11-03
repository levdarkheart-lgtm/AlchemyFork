# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `56c78b9134`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `56c78b9134`
removes Darwin OpenGL ownership from the baked tone-curve LUT. Availability is
backend-neutral; Darwin lazily publishes only the private shader-read Metal
RGBA16 texture, while non-Darwin retains the GL texture for later platform
pruning. Identity curves release native storage, and failed publication leaves
the curve dirty and unavailable for a later retry. The retained Darwin GL post
bypasses the tone curve when no GL object exists; native grading consumes the
Metal texture.

The shared native 2D uploader accepts 8- or 16-bit RGB/RGBA unorm data, expands
RGB to opaque RGBA, validates dimensions and byte-count overflow and publishes
only after upload commit. Physical validation proves the exact production-size
512x1 RGBA16 tone row, descriptor semantics, every sample and invalid-input
rejection.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,259 findings and exits 1. The tone-curve
cutover changes an owned Darwin path without changing the cross-platform static
inventory total. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency.
Exact evidence is
[archived](archive/CURRENT-native-tone-curve-checkpoint-2026-11-03.md).

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
  source `56c78b9134fafdaad4e60e487aaddf892bd568c8`, producing Development
  build `26.4.0d68149` (`26.4.0.68149`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `67465065ff88a70c05e225d5c0f7c3f79b14b75efb5d0de6ab851aa1853545bf`.
  Appearance-utility SHA-256 is
  `514308e32e9afd6e17c72c5545061749b8bc1d8bb35205ccca471a11623c864a`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,289
  assertions across 54 cases with API validation enabled. Its new physical case
  contributes 17 assertions proving the exact production-size tone row,
  descriptor semantics and invalid-input rejection.
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
  precommit and source-exact reports are byte-identical at
  `05c8780b14f7e52d6b49565c03076839279d7522bf96977d5e9770c2edb2f44c`.
  Both OpenGL and
  Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color-grading LUT and tone-curve LUT are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,259 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Media, terrain, water, parcel, generated sky faces, legacy sky cube, minimap layers, font atlases, color-grading LUT and tone curve own no GL texture on Darwin; live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Darwin tone-curve grading now has one
GPU authority: the native Metal RGBA16 texture. Identity, successful
publication and failure/retry states no longer depend on a parallel GL name.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-008/M-011 at the BRDF LUT. Source
inspection confirms the pipeline unconditionally allocates and rasterizes a
512x512 GL RG16F target on Darwin, while the native world renderer and native
PBR preview each already generate and consume their own Metal RG16F table.

## Exact next actions

1. Remove the Darwin pipeline GL BRDF target and generator ownership without
   weakening the native world or native PBR-preview table publication gates.
2. Preserve non-Darwin GL behavior and make retained Darwin GL callers safe
   when no pipeline BRDF target exists; add deterministic/physical coverage for
   generation, readiness and failure semantics.
