# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `8914212806`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `8914212806`
removes the duplicate Darwin pipeline GL BRDF-LUT owner. The Darwin resource
plan no longer requests the 512x512 RG16F GL target, the viewer shader manager
does not build the legacy BRDF generator and pipeline creation/binding is a
no-op. Non-Darwin retains the original target, generator and binding path.

The native world renderer and native PBR preview continue to allocate, compute
and consume their own Metal RG16F split-sum tables. The retained Darwin GLSL
helper returns deterministic zero instead of sampling an absent texture; it is
not presentation authority. A physical regression dispatches the production
PBR-preview compute kernel into the exact 512x512 private texture and compares
six edge/interior texels against an independent 1,024-sample CPU GGX oracle.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,259 findings and exits 1. This owner cutover
does not change the cross-platform static total because non-Darwin source is
retained. The packaged viewer contains no legacy BRDF generator names and has
no `CGLTexImageIOSurface2D` dependency. Exact evidence is
[archived](archive/CURRENT-native-brdf-lut-checkpoint-2026-11-03.md).

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
  source `8914212806ff815379092343c54d0b9c5764379a`, producing Development
  build `26.4.0d68151` (`26.4.0.68151`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `9776041d3b9344892d90458fb62c9a8f61ed79515e1ad279006226c5ad94e9bd`.
  Appearance-utility SHA-256 is
  `514308e32e9afd6e17c72c5545061749b8bc1d8bb35205ccca471a11623c864a`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
  Source and packaged `deferredUtil.glsl` are byte-identical at
  `78002b1d66fd816dc2b16fac62a05c1136d0c9302c13b62dec5ef15ee0a2ecba`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,345
  assertions across 55 cases with API validation enabled. The new physical
  BRDF case contributes 56 assertions for the exact descriptor, production
  compute kernel and independent reference samples.
- The full `alrender_tests` suite passed 557,292 assertions across 137 cases;
  its resource-plan coverage proves Darwin does not request the legacy BRDF
  target while non-Darwin retains its exact dimensions and format.
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
  `ae1a31c0bca1242a4950118e029a09685fd2f79764f58c0f879808376a905862`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color grading, tone curve and BRDF lookup are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,259 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Media, terrain, water, parcel, generated sky faces, legacy sky cube, minimap layers, font atlases, color grading, tone curve and pipeline BRDF LUT own no GL texture on Darwin; live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Darwin no longer allocates, rasterizes
or binds a redundant pipeline GL BRDF table; native world and preview paths own
their exact Metal tables and the production kernel now has an independent
physical numerical regression.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-008/M-011 at the SMAA lookup resources.
Source inspection confirms Darwin still allocates the area, search and optional
sample textures as GL names when SMAA is selected. The native world renderer
already owns private Metal area/search tables, consumes them in the native SMAA
transaction and has physical edge, weight and neighborhood oracle coverage.

## Exact next actions

1. Remove duplicate Darwin GL ownership of the SMAA lookup textures while
   preserving non-Darwin behavior and fail-closed retained-presentation
   semantics when a demanded native SMAA transaction cannot publish.
2. Prove table orientation/contents, creation failure handling and exact
   resource-plan lifecycle through deterministic and physical tests before
   removing any retained shader or traversal dependency.
