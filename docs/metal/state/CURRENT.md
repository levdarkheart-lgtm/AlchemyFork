# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `b79cb6a6ca`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `b79cb6a6ca`
removes Darwin OpenGL ownership from grayscale and color font-atlas pages.
Darwin viewer startup explicitly selects the Metal backend; retained GL tests
and non-Darwin production retain GL, while CPU-only tests select neither.
Grayscale pages use native `rg8_unorm` storage with the established RRRG
swizzle, color pages use `rgba8_unorm_srgb`, and each rasterized glyph publishes
only its exact dirty rectangle.

Glyph metadata is committed only after its CPU pixels reach the selected GPU
backend. A failed native subimage update retries a complete page publication;
if that also fails, the glyph is not cached and the caller may retry instead of
sampling unpublished atlas coordinates. Page reuse, eviction and logical image
identity remain intact. Physical tests read both native page types back and
prove exact dirty-rectangle bytes plus no GL texture name.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,259 findings and exits 1. The font-atlas
cutover replaces structural `LLImageGL` matches without adding a GL call, so
the total remains unchanged. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency.
Exact evidence is
[archived](archive/CURRENT-native-font-atlas-checkpoint-2026-11-03.md).

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
  source `b79cb6a6ca3ef3f4b879ca5e8d62cd5ff00abb80`, producing Development
  build `26.4.0d68143` (`26.4.0.68143`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `8e14e74c38ff6de8dfbbf4ab889cc55a24b478fce69da92d2262ffc003e8e723`.
  Appearance-utility SHA-256 is
  `89908a96bfef57e5f62ef17cf515b96d067f7e381c01c887fffe0ce5b2708dd5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,221
  assertions across 52 cases with API validation enabled.
- The source-exact fresh-context renderer suite passed 2,216 assertions across
  93 cases with Metal API validation enabled. It includes native-only font-page
  format/swizzle proof and exact grayscale/color dirty-rectangle readback.
- The CPU-only renderer suite passed 1,513 assertions across 203 cases: 202
  passed and the established single-palette font case skipped as expected.
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
| M-003..M-006 platform/resources/UI | doing | Darwin media, terrain, water, parcel, CPU sky, minimap and font-atlas resources are native-only; queued parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,259 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Media, terrain, water, parcel, generated sky faces, minimap layers and font atlases own no GL texture on Darwin; live resource/traversal owners remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Live Darwin text and emoji atlases now
own only native Metal storage, preserve their established swizzle/color-space
semantics and update exact glyph rectangles. Failed publication cannot expose
glyph metadata whose pixels never reached the GPU.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-006/M-011 at the legacy sky
`LLCubeMap`. Source inspection shows Darwin still allocates and updates a six-
face GL cube, then separately uploads the same transformed CPU faces into the
native cube already consumed by Metal world lighting.

## Exact next actions

1. Split legacy sky-cube CPU face preparation from GL allocation so Darwin can
   publish the existing native cube without creating the parallel GL object.
2. Add physical cube descriptor, face-order and exact-byte regressions; retain
   the non-Darwin GL path until the fork's platform pruning removes it.
