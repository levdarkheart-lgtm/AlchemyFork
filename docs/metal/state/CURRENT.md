# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `6bc7395fe1`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `6bc7395fe1`
removes Darwin OpenGL ownership and GL-lifecycle dependence from the parcel
overlay. Its logical `LLViewerTexture` now starts without GL allocation,
publishes completed RGBA maps through Metal and advances directly from idle
processing even when the GL manager is disabled.

Physical regressions prove exact RGBA8 native creation and full-coordinate
subimage update across all four channels, preserve every texel outside the
region and confirm no GL name exists. A failed parcel publication retains the
previous complete texture, resets CPU row progress and recreates native storage
on retry instead of publishing an incomplete overlay.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory remains 2,408 files / 3,258 findings and exits 1. The exact
report is byte-identical to the prior water checkpoint, so this owner cutover
adds no static finding. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency. Exact evidence is
[archived](archive/CURRENT-native-parcel-overlay-checkpoint-2026-11-03.md).

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
  source `6bc7395fe134d09cada72f0b08d57007e031caa6`, producing Development
  build `26.4.0d68137` (`26.4.0.68137`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `1f3af01e9078e0b2f4b65cd97c0aa9639042da4d68d890677fd32e8267393070`.
  Appearance-utility SHA-256 is
  `5769d937be848ee0b6edf9c153eefb0b0107898f90bc42fb377399e2ff7b337e`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,221
  assertions across 52 cases with API validation enabled.
- The source-exact renderer suite passed 2,187 assertions across 92 cases with
  Metal API validation enabled, including exact RGBA8 native update and
  outside-region preservation plus the prior typed/mip regressions.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,258 findings, expected exit 1. Both
  OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water and parcel-overlay resources are native-only; queued visual parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,258 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Zero direct production framebuffer or texture-image readbacks remain; media, terrain, water and parcel overlays own no GL texture on Darwin, and live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The parcel overlay now owns only Metal
storage on Darwin and no longer depends on the GL manager/rebuild queue to make
progress. Its retained/native terrain consumers preserve identity, and failed
publication retries from a complete CPU rebuild.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 at `LLSkyTex`. Its double-buffered
CPU-generated sky/shiny faces feed the legacy sky and cubemap path, but each
completed RGBA face still publishes through GL on Darwin.

## Exact next actions

1. Convert `LLSkyTex` Darwin face publication to explicit RGBA8 native storage
   while preserving current/next double-buffer selection and cubemap inputs.
2. Make cleanup/restore rebuild native faces without requiring GL texture
   creation. Queue any sky appearance question only after deterministic
   implementation evidence exists.
