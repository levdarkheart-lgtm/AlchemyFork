# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `53de84c79c` (`refactor(metal): remove GL SSR bootstrap`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `53de84c79c`
removes Metal's remaining GL framebuffer/PBO bootstrap for paired screen-space
reflection history. The native main-world renderer now owns both production and
consumption of the coherent previous-frame color/depth pair.

Startup and resize have no predecessor. The first complete Metal frame renders
without SSR feedback, publishes its native R11G11B10F-or-RGBA8 color plus R32F
depth pair, and is barred from ordered UI composition, snapshots and drawable
presentation. The next frame consumes that pair. A publication failure retains
the prior native generation; when no generation exists the offscreen warm-up
repeats. No GL framebuffer readback, CPU staging, PBO queue or Metal re-upload
remains in this path. Readiness is checked against current Metal capture
dimensions and native texture formats, not GL target dimensions.

The strict source inventory fell from 3,351 to 3,337 findings: 14 owned GL calls
were removed. Strict audit still exits 1, the main executable still links
OpenGL, and GL traversal/context ownership remain open release gates. This is a
real owner cutover, not a native-only claim. Exact implementation and validation
evidence is [archived](archive/CURRENT-native-ssr-cutover-checkpoint-2026-11-02.md).

Prior advanced-output, world/material, shadow, probe, UI and presentation
owners remain as recorded in the archive. Opaque fullbright and the six-face
cube convention have physical regressions; one controlled dry authored-mirror
sample is owner-confirmed. Legacy BLEND/stained glass, broader mirrors,
projector-visible shadows, moving media and complete parity remain open.

## Visual review cadence

Per owner direction, Codex no longer independently drives reference/candidate
viewers or takes comparison snapshots. Visual cases are accumulated in the
[owner-attended daily review queue](../work/active/visual-review-queue.md) and
reviewed together at most once per day with the owner. Source inspection,
builds, physical GPU tests, static/link audits and profiling continue between
reviews. No queued case is a parity pass until the owner-attended review records
that result.

## Build and test evidence

- `tools/metal/doctor.sh` passed on the M1 Max host with 64 GiB unified memory,
  Apple7-family support and the required Metal capabilities.
- Source-exact RelWithDebInfo command `AL_METAL_PRESET=ninja-os-arm64
  AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64
  AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=8 tools/metal/build.sh build`
  passed at `53de84c79c`, producing Development build `26.4.0d68103`
  (`26.4.0.68103`) with warnings as errors.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Executable SHA-256 is
  `12fcfbe8780f4bae179f3d1e1f77f99fb50b405d7ff4331542b9eefa43f909c8`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 remains
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, load-action validation and physical Metal
  execution, the complete RelWithDebInfo suite passed 52,190 assertions across
  52 cases. Existing coverage proves null-history rendering, paired native
  history publication/reuse, formats, row convention and SSR consumers.
- The unfiltered 574-entry CTest inventory ran for 1,268.61 seconds: 568 entries
  passed and six GL-host entries failed. Focused reruns reproduced three unique
  legacy GL defects in untouched code: frame-fence timing (4 assertions),
  depth-format expectation (1), and unavailable clip control in DoF (2).
  Combined targets and the DoF benchmark account for the duplicate entries.
  Five unsupported GL cases skipped. This run is not represented as green.
- The prior source-exact Release neutral gate passed 484/484; it was not rerun
  for this Darwin pipeline-only change.
- `python3 tools/metal/validate_harness.py` and all harness unit tests pass after
  the review queue was added to the mandatory live-document set.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native resource, world/UI and exact-presentation owners exist; complete parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is source-correct and physically covered; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; remaining review cases are queued rather than autonomously captured. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,337 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Native SSR history no longer reads GL; continue owner-by-owner cutover without broad traversal suppression. |
| M-012 Computer Use bridge | complete | Semantic operation and fail-closed process identity are verified; viewer comparison driving is now owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. One real GL-to-Metal frame dependency
and 14 owned GL calls are gone. Native history now has a fail-closed warm-up
boundary that cannot reach the drawable.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal, resource and readback owners remain; complete feature/stability,
strict audit, profiling/performance, independent review and release gates are
not met. Prolonged stability and independent build/review evidence remain open;
strict no-GL is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 from remaining synchronous GL/PBO
ingress and context ownership, starting with the avatar-impostor bake boundary.

## Exact next actions

1. Trace the avatar-impostor bake's remaining GL attachment/PBO ingress against
   its existing native renderer and remove the next proven duplicate owner.
2. Refresh the strict audit after each cutover and retain exact build, GPU-test,
   link/runtime and failure evidence; do not use broad draw suppression.
3. Add visual-only questions to the daily queue and wait for the owner-attended
   review window instead of launching or driving either viewer.
