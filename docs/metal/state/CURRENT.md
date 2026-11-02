# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `2a32759e7c` (`refactor(metal): require native
diagnostic readback`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `2a32759e7c`
removes two more diagnostic GL authorities from `pipeline.cpp`. Scene-pixel
probes now require the current-frame, dimension-matched native opaque post
target and use the native float-pixel readback. Scopes now require the exact
native display-post target, point-decimate it on Metal and collect through the
bounded Metal readback ring. Missing, stale or failed native producers skip or
drop the diagnostic sample instead of importing a GL result.

This deletes the synchronous scene `gGL.readPixels` fallback plus the scope GL
render target, PBO ring, blit and pixel readback. The preceding probe,
impostor, shadow, hero-radiance and base-light cutovers remain intact.

The strict inventory remains 2,408 files / 3,269 findings because this commit
removes callers hidden behind the shared `ALGPUReadback::requestPixels` helper,
not the helper's audited GL implementation. Production use of that helper is
now confined to `llviewerwindow.cpp`'s `DebugShowColor` readout. The main
executable still links OpenGL and Metal, and strict audit exits 1. Exact evidence
is [archived](archive/CURRENT-native-diagnostic-readback-cutover-checkpoint-2026-11-02.md).

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
- Source-exact RelWithDebInfo command `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64
  AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=8 tools/metal/build.sh build`
  passed at `2a32759e7c`, producing Development build `26.4.0d68112`
  (`26.4.0.68112`) with warnings as errors.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Executable SHA-256 is
  `e9a1ab921ddac8ce088ebab03c5ff7c5ad65cc228ea174036c17b4adb888d390`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, load-action validation and physical Metal
  execution, the complete RelWithDebInfo suite passed 52,190 assertions across
  52 cases.
- The prior unfiltered 574-entry CTest inventory remains 568 passes, six GL-host
  failures and five skips. Focused reruns identified three unique failures in
  untouched legacy GL tests: frame-fence pacing, depth-format expectation and
  DoF clip-control availability. This run is not represented as green.
- The prior source-exact Release neutral gate passed 484/484; it was not rerun
  for this Darwin-only reflection-manager change.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native resource, world/UI and exact-presentation owners exist; complete parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is source-correct and physically covered; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,269 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Pipeline pixel ingress, scopes, scene-pixel probes and default/ordinary probe publication are native-only; broader readback/traversal/context removal remains. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Scopes and scene-pixel probes no longer
depend on GL copies, PBOs or synchronous GL readback. The native exactness gates
remain fail-closed, and 114 fallback-heavy lines were removed.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal, resource and readback owners remain; complete feature/stability,
strict audit, profiling/performance, independent review and release gates are
not met. Prolonged stability and independent build/review evidence remain open;
strict no-GL is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 with the last production
`ALGPUReadback::requestPixels` owner, `DebugShowColor` in `llviewerwindow.cpp`,
after proving its exact presentation-source and coordinate semantics.

## Exact next actions

1. Trace `llviewerwindow.cpp`'s `DebugShowColor` source, timing, coordinate and
   UI-inclusion contract against the exact native presentation/world-UI owners;
   remove its GL/PBO fallback only after the native producer is proven exact.
2. Refresh strict audit after each cutover and retain exact build, GPU-test,
   link/runtime and failure evidence; do not use broad draw suppression.
3. Add visual-only questions to the daily queue and wait for the owner-attended
   review window instead of launching or driving either viewer.
