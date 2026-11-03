# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `0fb0d391d1`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `0fb0d391d1`
makes `LLImageGL::readBackRaw` require the published Metal mirror on Darwin. It
removes the GL texture read and metadata-query fallback while preserving
byte-exact one-, two-, three- and four-component readback, published-state
ordering during asynchronous upload and explicit compatible compressed block
reads. Missing or incompatible native storage fails closed.

The only production caller passes `compressed_ok=false`; production
explicit-format inspection found no compressed BC/DXT owner on that decoded
fetched-image path. The hidden test context now publishes and refreshes its
Metal device for physical native image tests.

There are zero direct production `glReadPixels` calls. Exact source search now
leaves two production `glGetTexImage` calls: the `LLImageGL` scale-down path and
`LLCubeMapArray` growth. Two GL texture-size debug queries also remain. Strict
inventory is unchanged at 2,408 files / 3,265 findings because scanner
identifier/comment/string false positives offset the removed calls; strict mode
still exits 1. Exact evidence is
[archived](archive/CURRENT-native-image-readback-cutover-checkpoint-2026-11-03.md).

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
- The required warnings-as-errors RelWithDebInfo wrapper completed at exact
  source `0fb0d391d1e0bdaac0ba30ba554eb92cafb3c6b1`, producing Development
  build `26.4.0d68123` (`26.4.0.68123`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `8616f6803def8f4b0e57f2866eda99ed39e12a6eb2e5d4ea72e3634a625498d8`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the source-exact native image suite passed 1,216
  assertions across 85 cases with API validation enabled.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- Source-exact broad `llrender_gl_shared_tests` retained its documented
  legacy baseline: 8,335/8,340 assertions and 181/224 cases passed, with four
  frame-fence/depth-format failures and 39 skips. It is not represented as
  green.
- Strict inventory remains 2,408 files / 3,265 findings, expected exit 1. Both
  OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native resource, world/UI and exact-presentation owners exist; complete parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,265 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | `readBackRaw` is native-only and zero direct production `glReadPixels` calls remain; scale-down and cube-array growth are the last direct texture-image reads. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Retained fetched/sculpt saved-raw and
related consumers no longer import texture bytes through OpenGL. Native mirror
publication is authoritative, byte layouts are explicit and missing or
incompatible native storage fails closed.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; two direct `glGetTexImage` owners remain;
complete feature/stability, strict audit, profiling/performance, independent
review and release gates are not met. Prolonged stability remains open;
independent build/review evidence remains open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 on the two remaining direct
texture-image reads. The cube-map-array resizing constructor performs an
all-layer GL CPU round trip; the `LLImageGL` scale-down path reads through a PBO.
Give each exact native ownership before removing its GL implementation.

## Exact next actions

1. Replace the cube-map-array resize constructor's `glGetTexImage`/
   `glTexSubImage3D` round-trip with an explicit native array-layer copy/scale,
   preserving six faces per probe and growth semantics.
2. Remove or replace the remaining GL-only texture-size diagnostics and
   scale-down PBO readback only after their native state owners are exact; refresh
   strict/build/physical-GPU evidence after each bounded cutover.
