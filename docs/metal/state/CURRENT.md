# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `a986999d95` (`refactor(metal): require native
360 snapshots`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `a986999d95`
removes the production GL RGB fallback from six-face 360 capture. Every cube
face must now complete through the exact native world-snapshot producer. A
failure on any face aborts the aggregate before encoding, JavaScript footer
publication or WebGL stitcher navigation, while common camera, world, timeout
and control-state cleanup still runs. Mixed native/GL panoramas and a second
whole-panorama GL rendering pass are no longer possible.

`LLViewerWindow::simpleSnapshot` now has one native-only contract and returns
failure when the native request or readback cannot complete. The capture caller
recreates each raw face before use so stale bytes cannot survive a failed
transaction. Existing owner-attended evidence already covers all six native
faces, cube orientation, seams and channel ordering; no new viewer comparison
was performed for this cutover.

Strict inventory remains 2,408 files / 3,266 findings because the audit counts
the shared `glReadPixels` implementation rather than every `gGL.readPixels`
call site. Strict audit still exits 1, and `otool -L` still reports both OpenGL
and Metal in the main executable. Exact evidence is
[archived](archive/CURRENT-native-360-snapshot-cutover-checkpoint-2026-11-02.md).

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
- The required warnings-as-errors RelWithDebInfo wrapper passed 221 invalidated
  actions before commit and the source-exact rebuild passed at
  `a986999d95aeb67e1ce8528945b6cadb467738a0`, producing Development build
  `26.4.0d68116` (`26.4.0.68116`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Executable SHA-256 is
  `130eb49ef0ff657933b2d317353712a5c2605c056b9c0cb036c6d3acb7af8d5f`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, load-action validation and physical Metal
  execution, the complete source-exact RelWithDebInfo suite passed 52,209
  assertions across 52 cases.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The prior unfiltered 574-entry CTest inventory remains 568 passes, six GL-host
  failures and five skips. Focused reruns identified three unique failures in
  untouched legacy GL tests: frame-fence pacing, depth-format expectation and
  DoF clip-control availability. This run is not represented as green.
- The prior source-exact Release neutral gate passed 484/484; it was not rerun
  for this bounded Darwin-only readback change.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native resource, world/UI and exact-presentation owners exist; complete parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is source-correct and physically covered; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,266 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Recurring pipeline/debug readbacks and 360 faces are native-only; the two remaining production `gGL.readPixels` calls are `rawSnapshot` row fallbacks. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Six-face 360 capture no longer imports
GL framebuffer pixels or rerenders an all-GL panorama after a native failure.
The retained feature is fail-closed and aggregate-atomic around its established
native producer.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal, resources and the ordinary `rawSnapshot` fallback reads remain; complete
feature/stability, strict audit, profiling/performance, independent review and
release gates are not met. Prolonged stability remains open;
independent build/review evidence remains open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 on the only two remaining
production `gGL.readPixels` calls in `LLViewerWindow::rawSnapshot`. Native
color/depth/cutout/tiled owners exist, but fallback removal requires tracing
every supported contract and preserving the feature rather than merely failing
it.

## Exact next actions

1. Inventory `rawSnapshot` color/depth/depth24 variants, RGB/RGBA formats,
   tiling, coordinate/row orientation and UI/HUD inclusion against the existing
   native snapshot producer; add any missing native ownership.
2. Remove GL snapshot fallback reads only when every affected supported path is
   exact or has an explicit feature-preserving native implementation; keep
   failures fail-closed and aggregate tiled captures atomically.
3. Refresh strict audit after each cutover and retain exact build, physical GPU,
   architecture/link and failure evidence. Add visual-only questions to the
   daily queue instead of launching or driving viewers.
