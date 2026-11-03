# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `4a005b8ef9`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `4a005b8ef9`
removes Darwin OpenGL ownership from the legacy sky `LLCubeMap`. Darwin now
prepares the historical transformed CPU faces and publishes only the existing
native `texture_cube`; `initGL()` is inert, no GL face wrapper or texture name
is created, and retained GL traversal clears an older cube binding instead of
sampling stale storage.

The native cube remains linear `rgba8_unorm`, 64x64, one mip, with the
established `-X,+X,-Y,+Y,-Z,+Z` transformations and GL-to-Metal face/slice
mapping. A generation prepared without a Metal device is unavailable rather
than exposing the prior cube, then publishes exactly when the device returns.
Physical validation reads every slice back byte-for-byte and proves descriptor,
mapping, retry and zero-GL-name behavior. Non-Darwin GL behavior is retained for
later platform pruning.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,259 findings and exits 1. The legacy-cube
cutover changes an owned Darwin path without changing the cross-platform static
inventory total. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency.
Exact evidence is
[archived](archive/CURRENT-native-legacy-sky-cube-checkpoint-2026-11-03.md).

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
  source `4a005b8ef950f1872c0ad9a86cfdc575dff2030c`, producing Development
  build `26.4.0d68145` (`26.4.0.68145`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `9277b87848d3723b04baa3c90dd123a23cd6faca1c991122dfb4bd35bc0870c6`.
  Appearance-utility SHA-256 is
  `bf70df75140711cd0d034d5562d3ec567d9814b252f617d1202e4f3833e0cf10`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,221
  assertions across 52 cases with API validation enabled.
- The source-exact fresh-context renderer suite passed 2,243 assertions across
  94 cases with Metal API validation enabled. It includes descriptor, all-six-
  slice transform/mapping, exact-byte retry and zero-GL-name proof for the
  native legacy sky cube.
- The CPU-only renderer suite passed 1,513 assertions across 203 cases: 202
  passed and the established single-palette font case skipped as expected.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,259 findings, expected exit 1. Precommit
  and exact reports are byte-identical at
  `a8e681c2607c41595dc41da1329020ca29514676c8140daed419d3b73c6d9df9`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,259 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Media, terrain, water, parcel, generated sky faces, legacy sky cube, minimap layers and font atlases own no GL texture on Darwin; live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The live Darwin legacy environment cube
now owns only native Metal storage while preserving every established face
transform and slice convention. Failed publication cannot expose an older sky
generation, and retained traversal cannot reuse a stale GL cube binding.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-008/M-011 at the color-grading LUT.
Source inspection finds the pipeline's `mCGLut` as the only live production
`ALTexture3D` owner. Darwin currently uploads the same decoded LUT into a GL 3D
texture and a native 2D-array texture, while the Metal world path already
consumes the native resource.

## Exact next actions

1. Make decoded color-LUT availability backend-neutral and allocate only
   `mMetalCGLut` on Darwin, preserving non-Darwin `ALTexture3D` behavior.
2. Add physical descriptor/slice/precision and failure-state regressions for
   both 16-bit `.cube` data and 8-bit image-strip LUT data.
