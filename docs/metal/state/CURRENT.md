# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `4b6093028e` (`refactor(metal): remove probe GL
readback mirrors`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `4b6093028e`
removes the default- and ordinary-reflection-probe GL/PBO mirrors after the
native face, filter, SH and publication owners were traced end to end. Metal
consumers now receive radiance cubes and packed SH rows only from the ordered
native six-face producer. Slot generation, scheduled/realtime lane identity and
coherent radiance/SH publication remain fail-closed.

This deletes the GL readback rings, every filtered face/mip `glReadPixels`,
CPU staging and Metal re-upload transaction. The legacy renderer's own probe
array still updates; it is no longer imported into Metal. The preceding
impostor, shadow, hero-radiance and base-light cutovers remain intact.

The strict inventory fell from 3,279 to 3,269 findings. The reflection manager
now has zero `gl_function` findings, though it retains 29 GL
shader/state/traversal findings. The main executable still links OpenGL and
Metal, and strict audit exits 1. Exact evidence is
[archived](archive/CURRENT-reflection-probe-native-cutover-checkpoint-2026-11-02.md).

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
  passed at `4b6093028e`, producing Development build `26.4.0d68110`
  (`26.4.0.68110`) with warnings as errors.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Executable SHA-256 is
  `cb331c2789776603c99b59f8dbb578977270355512550e7334d229b6e101ea91`.
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
| M-011 GL authority removal | doing | Pipeline pixel ingress plus default/ordinary probe radiance and SH are native-only; broader readback/traversal/context removal remains. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A second full probe authority is gone:
Metal no longer waits for GL, stages probe pixels through CPU memory or uploads
them back to the GPU. Ten audited findings and 722 fallback lines were removed.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal, resource and readback owners remain; complete feature/stability,
strict audit, profiling/performance, independent review and release gates are
not met. Prolonged stability and independent build/review evidence remain open;
strict no-GL is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 with the already-native scope sample
and one-pixel scene-readback owners in `pipeline.cpp`.

## Exact next actions

1. Trace native scope sampling and scene-pixel readback against their GL/PBO and
   synchronous GL fallbacks; remove the next proven duplicate authority without
   weakening exactness.
2. Refresh strict audit after each cutover and retain exact build, GPU-test,
   link/runtime and failure evidence; do not use broad draw suppression.
3. Add visual-only questions to the daily queue and wait for the owner-attended
   review window instead of launching or driving either viewer.
