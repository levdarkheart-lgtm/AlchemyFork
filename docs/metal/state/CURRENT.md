# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `857493de1a` (`refactor(gl): remove dead
framebuffer readback wrapper`), on native raw-snapshot commit `d1df906f44`.
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `d1df906f44`
removes the last two `gGL.readPixels` snapshot fallbacks from
`LLViewerWindow::rawSnapshot`. Ordinary/clean-plate color, scalar depth, packed
depth24, isolated RGBA cutouts and tiled output now require the exact native
producer. Request or tile failure returns the complete snapshot as failed, so
callers cannot publish a partial image or silently rerender through GL.

With all production consumers gone, commit `857493de1a` removes the synchronous
`LLRender::readPixels`/`glReadPixels` wrapper. Unrelated GL test coverage keeps
its test-only direct reads, while the obsolete wrapper-specific test is gone.
There are now zero production `gGL.readPixels` callers. Remaining direct
production `glReadPixels` consumers are the avatar morph-mask path in
`llappearance/lltexlayer.cpp` and the separate appearance-baking utility.

Strict inventory is now 2,408 files / 3,265 findings, one fewer than the prior
3,266 checkpoint. Strict audit still exits 1, and `otool -L` still reports both
OpenGL and Metal in the main executable. Exact evidence is
[archived](archive/CURRENT-native-snapshot-readback-cutover-checkpoint-2026-11-02.md).

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
- The required warnings-as-errors RelWithDebInfo wrapper passed the bounded
  snapshot rebuild, then completed all 1,330 header-invalidated actions at
  source-exact commit `857493de1afda0a08147c2fdc6cee3b14ef6803a`, producing
  Development build `26.4.0d68119` (`26.4.0.68119`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Executable SHA-256 is
  `cd0cb8a1c24d3cf57de9579b973342c1c8d4218a7695352ed2e1338383b292ad`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, load-action validation and physical Metal
  execution, the complete source-exact RelWithDebInfo suite passed 52,209
  assertions across 52 cases.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- Focused `alrenderstate_gl` coverage passed 194 assertions across 12 cases
  after the dead wrapper-specific case was removed.
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
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,265 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Pipeline/debug and all Snapshot readbacks are native-only; zero production `gGL.readPixels` callers remain. Direct avatar bake/morph-mask GL reads are next. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The complete saved-output Snapshot
family no longer imports GL framebuffer pixels, including ordinary, no-post,
depth/depth24, cutout, tiled and 360 paths. The now-unreachable synchronous
renderer readback wrapper is deleted and strict inventory falls by one.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal, resources and avatar bake/morph-mask readbacks remain; complete
feature/stability, strict audit, profiling/performance, independent review and
release gates are not met. Prolonged stability remains open;
independent build/review evidence remains open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 on the remaining direct production
`glReadPixels` owners. Trace `LLTexLayer::readBackAlpha` and
`LLBakingTexLayerSetBuffer::midRenderTexLayerSet`, distinguish the main viewer
from the appearance utility, and provide exact native color/alpha ownership
before removing either retained feature.

## Exact next actions

1. Map the avatar morph-mask and appearance-utility bake readbacks to their
   render targets, dimensions, row orientation, alpha/color formats and process
   ownership; identify or add exact native textures/readback producers.
2. Remove each direct GL read only after its cache/bake consumer remains
   feature-preserving and fail-closed; do not conflate the main viewer with the
   separate appearance utility.
3. Refresh strict audit after each cutover and retain exact build, physical GPU,
   architecture/link and failure evidence. Add visual-only questions to the
   daily queue instead of launching or driving viewers.
