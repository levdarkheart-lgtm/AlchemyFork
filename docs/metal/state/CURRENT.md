# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `a9a0edd4e2` (`refactor(metal): cut over debug
color readback`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `a9a0edd4e2`
removes the last production `ALGPUReadback` consumer and the helper itself.
`DebugShowColor` now flushes the immediate stream at its exact pre-debug-text
boundary, ends the active native world/UI encoder, copies one pixel into a
private 1x1 Metal texture and resumes the retained pass with load/store actions.
After the exact transaction is submitted, a bounded three-slot native readback
ring queues the pixel on the same Metal command queue and collects it without a
steady-state CPU wait. BGRA is converted back to the historical RGBA display.

The diagnostic requests a fresh exact world/UI transaction while enabled but
does not spend or latch the bounded presentation-probe budget. Missing,
incomplete or failed native production drops the diagnostic sample instead of
importing a GL framebuffer result. The dead GL buffer/pixel readback class and
its two GL-only test cases are deleted.

Strict inventory is now 2,408 files / 3,266 findings, three fewer than the prior
3,269 checkpoint. Strict audit still exits 1, and `otool -L` still reports both
OpenGL and Metal in the main executable. Exact evidence is
[archived](archive/CURRENT-debug-color-readback-cutover-checkpoint-2026-11-02.md).

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
- The required warnings-as-errors RelWithDebInfo wrapper first completed all
  1,392 invalidated actions, then the source-exact rebuild passed at
  `a9a0edd4e225681e49ae5b06f47284af31b70594`, producing Development build
  `26.4.0d68114` (`26.4.0.68114`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Executable SHA-256 is
  `fcb6a3669e822170e408ac05d8343175dc51f121ce68ce5403f3c6b62f9f733b`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, load-action validation and physical Metal
  execution, the complete source-exact RelWithDebInfo suite passed 52,209
  assertions across 52 cases. Its ordered-pixel regression proves the snapshot
  sees the first draw and the resumed final target sees a later draw.
- Focused `llrender_gl_shared_tests/algpubuffer_gl_test` passed 108 assertions
  across nine surviving cases after the dead readback tests were removed.
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
| M-011 GL authority removal | doing | Recurring pipeline and debug-color readbacks are native-only; the three remaining production `gGL.readPixels` calls are snapshot fallbacks. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The final recurring GL pixel readback
owner is native, asynchronous and exact at its historical ordered boundary;
the now-unused GL readback abstraction and tests are removed. The strict
inventory fell by three findings.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal, resources and snapshot fallback reads remain; complete
feature/stability, strict audit, profiling/performance, independent review and
release gates are not met. Prolonged stability remains open;
independent build/review evidence remains open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 on the only three remaining
production `gGL.readPixels` calls in `LLViewerWindow::rawSnapshot` and
`simpleSnapshot`. Native color/depth/cutout/360 owners already exist for some
shapes, but fallback removal requires tracing every supported snapshot
contract and preserving the feature rather than merely failing it.

## Exact next actions

1. Inventory `rawSnapshot` and `simpleSnapshot` variants, formats, tiling,
   coordinate/row orientation, UI/HUD inclusion and 360-face behavior against
   the existing native snapshot producers; add missing native ownership.
2. Remove GL snapshot fallback reads only when every affected supported path is
   exact or has an explicit feature-preserving native implementation; keep
   failures fail-closed and aggregate tiled captures atomically.
3. Refresh strict audit after each cutover and retain exact build, physical GPU,
   architecture/link and failure evidence. Add visual-only questions to the
   daily queue instead of launching or driving viewers.
