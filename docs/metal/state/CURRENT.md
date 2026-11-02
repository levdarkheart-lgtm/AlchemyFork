# Current engineering state

**Observed:** 2026-11-02 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `35082663e5` (`refactor(metal): remove base-light GL pixel bridge`)
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. After the native SSR
cutover, commits `bdefc6b2d7`, `60c9ee55f2`, `bc79f48aca` and `35082663e5`
remove four more legacy-pixel ingress families from Metal consumers:
avatar-impostor attachment/PBO fallback, directional/projector shadow-depth
downloads, hero-radiance framebuffer readback/upload, and the blurred RG
base-light-map bridge.

Each consumer now accepts only its identity- and format-checked native Metal
producer. Missing or incomplete native generations fail the Metal transaction
closed; they never import legacy pixels. Existing retained native generations
remain usable where that was already the owner contract. The legacy display
path is not globally suppressed.

The strict inventory fell from 3,337 to 3,279 findings, a further 58 removed
findings. `pipeline.cpp` now has zero `gl_function` findings in that audit, but
it still contains GL shader/state/traversal dependencies. Strict audit still
exits 1, the main executable still links OpenGL, and context/traversal ownership
remain open release gates. Exact evidence is
[archived](archive/CURRENT-pipeline-pixel-ingress-cutover-checkpoint-2026-11-02.md).

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
  passed at `35082663e5`, producing Development build `26.4.0d68108`
  (`26.4.0.68108`) with warnings as errors.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Executable SHA-256 is
  `10086bb6ef963c3168658a5d83a2bb208bae1728bfe3d30fd32dcf02189e47bc`.
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
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,279 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | SSR, impostor publication, shadow depth, hero radiance and base-light pixels are native-only; broader traversal/context removal remains. |
| M-012 Computer Use bridge | complete | Semantic operation and fail-closed process identity are verified; viewer comparison driving is now owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Four additional GL-to-Metal pixel
crossings and 58 audited findings are gone. `pipeline.cpp` has no remaining
direct GL-function finding, and all affected native owners fail closed.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal, resource and readback owners remain; complete feature/stability,
strict audit, profiling/performance, independent review and release gates are
not met. Prolonged stability and independent build/review evidence remain open;
strict no-GL is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 outside `pipeline.cpp`, starting
with the two ordinary reflection-probe PBO readbacks.

## Exact next actions

1. Trace the two `llreflectionmapmanager.cpp` GL/PBO face readbacks against the
   existing native reflection-probe face/radiance owners and remove the next
   proven duplicate crossing.
2. Refresh the strict audit after each cutover and retain exact build, GPU-test,
   link/runtime and failure evidence; do not use broad draw suppression.
3. Add visual-only questions to the daily queue and wait for the owner-attended
   review window instead of launching or driving either viewer.
