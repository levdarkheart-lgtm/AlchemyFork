# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `7d3af86e6c`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `7d3af86e6c`
removes every direct production `glGetTexImage` and
`glGetCompressedTexImage` call. `LLImageGL::scaleDown` no longer reads a mip
through a scratch PBO: its retained GL side effect is a GPU-to-GPU exact mip
blit, while its authoritative Metal mirror continues through native mip-copy or
native bilinear-resample paths.

The removed cube-array copy constructor had no executable caller; the live
reflection resize path already recreates the array and invalidates every probe.
Texture-size debug validation now uses the immutable allocation record instead
of GL width/height queries. Two debug internal-format queries remain in
`llgl.cpp`; loader declarations remain until the GL loader is retired.

There are zero direct production `glReadPixels`, `glGetTexImage` or
`glGetCompressedTexImage` calls. Strict inventory is 2,408 files / 3,249
findings, down 16 at this exact checkpoint, and still exits 1. Exact evidence is
[archived](archive/CURRENT-texture-image-readback-elimination-checkpoint-2026-11-03.md).

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
  source `7d3af86e6c88b6b4003b2c0ba52f30ccbe0e9ef3`, producing Development
  build `26.4.0d68125` (`26.4.0.68125`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `3226822da171f02d69b496bd4e628012635053fcb0bb1471666fb0963eb481fd`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the source-exact renderer suite passed 1,216
  assertions across 85 cases with API validation enabled, including exact
  source-mip blit and native scale-down coverage.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- Source-exact broad `llrender_gl_shared_tests` retained its documented
  legacy baseline: 8,335/8,340 assertions and 181/224 cases passed, with four
  frame-fence/depth-format failures and 39 skips. It is not represented as
  green.
- Strict inventory is 2,408 files / 3,249 findings, expected exit 1. Both
  OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Broad native resource, world/UI and exact-presentation owners exist; complete parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,249 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Zero direct production framebuffer or texture-image readbacks remain; live duplicate GL resource and traversal owners are next. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Owned framebuffer and texture-image
readback paths no longer import pixels through OpenGL. Scale-down no longer
round-trips through the CPU, and a dead overflow-prone cube-array copy path is
gone without changing the live invalidation behavior.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open;
independent build/review evidence remains open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 on live duplicate resource owners.
Accelerated macOS CEF ingress currently imports each IOSurface to both a GL
rectangle texture and a Metal texture even though the native destination copy
already exists. Make the Darwin path publish only after the Metal copy succeeds
and remove its GL import/blit side effect; keep other platforms unchanged.

## Exact next actions

1. Cut the Darwin accelerated CEF media path over to its existing IOSurface
   Metal import/resample alone and fail closed before publishing a stale frame.
2. Preserve Windows/Linux interop, add source-level and native resource
   regression coverage where practical, then refresh strict/build/physical-GPU
   evidence without autonomously launching a viewer.
