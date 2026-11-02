# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `6d82ef5405` (`refactor(metal): cut over appearance
readback`).
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `6d82ef5405`
removes the remaining direct framebuffer reads from avatar morph-mask generation
and the separate appearance-baking utility. Both now use an ordered native
RGBA8 capture checkpoint owned by `ALMetalUIRenderer`; a checkpoint submits the
current output, reads the requested region synchronously, retains that output as
the background and resumes on a distinct target.

`LLTexLayer::readBackAlpha` extracts alpha from the native RGBA result and
propagates failure through morph-mask gathering and bake publication. The
utility now publishes its hidden-window Metal device and scheduler, begins an
offscreen native capture, reads the full color output at the exact ordered
boundary and refuses to publish incomplete output. The headless Darwin window
owns a Metal device and scheduler without requiring a drawable surface.

There are now zero direct production `glReadPixels` calls and zero direct
readback calls in `llappearance` or `llappearanceutility`. Strict inventory is
unchanged at 2,408 files / 3,265 findings because those directories are outside
the scanner's current scope; strict mode still exits 1. Remaining direct
production texture-image reads are in `LLImageGL` fallback/debug/scale-down
paths and the cube-map-array resizing constructor. Exact evidence is
[archived](archive/CURRENT-native-appearance-readback-cutover-checkpoint-2026-11-02.md).

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
  source `6d82ef5405096cf108fa8a1419f72d16599697c0`, producing Development
  build `26.4.0d68121` (`26.4.0.68121`).
- A separate warnings-as-errors, viewer-disabled appearance-utility
  configuration compiled and linked `appearance-utility-bin`; both it and the
  viewer executable are ARM64 Mach-O binaries.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `75fda49173bf127566b0f1e0698222128c3ca8e00e72d378618188b2901e30b1`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete source-exact physical Metal suite
  passed 52,221 assertions across 52 cases. Focused hidden-window appearance
  coverage passed 449 assertions across two cases with API validation enabled.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- Content-equivalent broad `llrender_gl_shared_tests` retained its documented
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
| M-011 GL authority removal | doing | Pipeline/debug, Snapshot and appearance framebuffer readbacks are native-only; zero direct production `glReadPixels` calls remain. Texture-image readbacks are next. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Main-viewer morph-mask generation and
the out-of-process appearance baker no longer import framebuffer pixels through
OpenGL. The native ordered checkpoint preserves draw order and the previous
output across resumed draws; failure prevents mask-cache or bake publication.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; direct `glGetTexImage` owners remain; complete
feature/stability, strict audit, profiling/performance, independent review and
release gates are not met. The appearance utility has compile/link and hidden
backend coverage but no complete external bake-fixture execution at this
checkpoint. Prolonged stability and independent build/review evidence remain
open, and strict no-GL is not satisfied. Visual parity cases remain queued, not
waived.

**Evidence-based priority:** continue M-011 on direct texture-image reads.
`LLImageGL::readBackRaw` already prefers native mirrors for uncompressed byte
images but retains a GL fallback for missing/incompatible mirrors, compressed
readback and metadata queries. The cube-map-array resizing constructor performs
an all-layer GL CPU round-trip. Establish the exact required formats and native
ownership, then fail closed rather than retaining either GL path.

## Exact next actions

1. Trace all callers and format contracts for `LLImageGL::readBackRaw`, including
   saved fetched/sculpt raw images, discard levels and compressed input; make
   supported Darwin reads require the native mirror and prove failure handling.
2. Replace the cube-map-array resize constructor's `glGetTexImage`/
   `glTexSubImage3D` round-trip with an explicit native array-layer copy/scale,
   preserving six faces per probe and growth semantics.
3. Remove or replace the remaining GL-only texture-size diagnostics and
   scale-down PBO readback only after their native state owners are exact; refresh
   strict/build/physical-GPU evidence after each bounded cutover.
