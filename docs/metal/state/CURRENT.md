# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `26801e499f`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `26801e499f`
removes the Darwin media texture's initial GL placeholder. Its main-thread
creation path now allocates and populates only Metal storage, with no GL name.
Both accelerated IOSurface ingress and ordinary CPU-painted CEF/VLC publication
therefore begin and remain native-only on Darwin.

`ALTextureSlot` keeps that Metal-only image as the native logical binding. While
retained GL traversal still executes, its physical GL slot uses the established
white/default placeholder instead of allocating a duplicate media texture.
Changing native logical identity flushes queued geometry first. Raw images,
wrapped textures, slow bind and fast bind are covered by physical regressions;
Windows/Linux media paths remain unchanged in this bounded cutover.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,254 findings and still exits 1. Two existing
GL-binding snippets were equivalently reworded; the net increase of six consists
of `LLImageGL`/native-binding symbol matches, not new GL calls. The packaged
viewer has no `CGLTexImageIOSurface2D` dependency. Exact evidence is
[archived](archive/CURRENT-native-media-placeholder-checkpoint-2026-11-03.md).

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
  source `26801e499ffdf5f895daae4b6659156bfb2e6588`, producing Development
  build `26.4.0d68131` (`26.4.0.68131`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `ad62e9b12d9c929e213481015ff24cc5610d60e1c9a4342e81d7f5c42d493fd5`.
  Appearance-utility SHA-256 is
  `4c0a61cfff3e1ddbd8f3307fb75da958b9cc89ea83e62e087fc4400297b4e9fb`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,221
  assertions across 52 cases with API validation enabled.
- The source-exact renderer suite passed 1,527 assertions across 90 cases with
  Metal API validation enabled, including native allocation and raw/wrapped
  slow/fast sampled-binding regressions.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,254 findings, expected exit 1. Both
  OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media ingress/upload and initial placeholder allocation are native-only; moving-media parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,254 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Zero direct production framebuffer or texture-image readbacks remain; live duplicate GL resource and traversal owners are next. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Both live Darwin media update modes and
their initial texture allocation now own only Metal storage. Retained GL
traversal sees the established fallback rather than a duplicate media copy, and
native logical identity remains authoritative. Publication remains fail closed.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 at the bounded terrain-map ownership
boundary. `ALTerrainSurfaceMaps` already exposes native height/composition
textures, but its upload path still creates GL storage and generic `setSubImage`
rejects Metal-only images before its Darwin update branch.

## Exact next actions

1. Confirm every live consumer of `ALTerrainSurfaceMaps` height and composition
   textures and preserve the existing native sampling contract.
2. Add an explicit native full/region allocation and update path, remove the
   Darwin duplicate GL allocation/upload, and physically prove byte formats and
   partial updates. Keep visual questions in the daily owner-attended queue.
