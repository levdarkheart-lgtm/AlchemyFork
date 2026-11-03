# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `cb59cc05a3`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `cb59cc05a3`
removes Darwin OpenGL ownership from the terrain height and composition maps.
Their R32F and RG16F storage now allocates and uploads only through Metal, and
full or dirty-rectangle refreshes do not create or mutate a GL name. Existing
GL-owned or missing native images are rebuilt native-only.

`LLImageGL` now has an explicit typed native creation contract for formats that
`LLImageRaw` cannot represent and a same-coordinate native subimage contract.
Physical regressions prove exact R32F and float-to-half RG16F full uploads,
partial updates and unchanged texels outside the region. A failed terrain map
upload preserves dirty state for retry instead of silently dropping it.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,257 findings and still exits 1. Relative to
the prior checkpoint, five new heuristic snippets and two replaced method
signature snippets produce the net increase of three; none is a GL call. The
packaged viewer has no `CGLTexImageIOSurface2D` dependency. Exact evidence is
[archived](archive/CURRENT-native-terrain-maps-checkpoint-2026-11-03.md).

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
  source `cb59cc05a368b756b86c1b525ea3a53adbc288b5`, producing Development
  build `26.4.0d68133` (`26.4.0.68133`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `02b826dfacd8a42afe3b579d9a2bd7d1537f4760a1183ceae8a5bd4168150e19`.
  Appearance-utility SHA-256 is
  `3cfba52ae7d0fff7b6cb62bab25fcc44057a1b50e05c18cd5a96b143ae451ca5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,221
  assertions across 52 cases with API validation enabled.
- The source-exact renderer suite passed 1,692 assertions across 91 cases with
  Metal API validation enabled, including exact typed R32F/RG16F allocation,
  conversion and partial-region update regressions.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,257 findings, expected exit 1. Both
  OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Darwin media and terrain-map ownership are native-only; moving-media/terrain parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,257 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Zero direct production framebuffer or texture-image readbacks remain; media and terrain maps own no GL texture on Darwin, and live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Terrain height/composition resources now
own only Metal storage on Darwin, including exact typed full and partial upload.
Their existing native consumers remain authoritative, and failed refreshes
remain dirty for retry rather than publishing incomplete state.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 at `ALWaterSurface`. Its procedural
RG32F cell map and baked RG16F displacement maps already feed explicit native
water captures, but both still allocate through GL. The cell map can use the
typed native path directly; displacement maps require native mip generation.

## Exact next actions

1. Convert the `ALWaterSurface` RG32F cell map to typed native allocation/update
   while preserving native capture and retained sampled-binding semantics.
2. Extend typed native creation for production mip chains, cut the baked RG16F
   height maps off GL, and physically prove base data plus generated mip levels.
   Keep all visual questions in the daily owner-attended queue.
