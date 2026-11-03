# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `a33f57df9c`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `a33f57df9c`
cuts Darwin accelerated CEF media ingress off OpenGL. Its IOSurface is imported
directly as a shared Metal texture and row-flipped into the private native media
mirror; no GL rectangle texture, CGL IOSurface binding or framebuffer blit is
created on the macOS path.

The native source replaces the prior source only after a successful import.
Copy/adoption failure discards the old mirror and does not publish playing
state. Missing Metal capability rejects accelerated paint so the CPU path stays
available. Windows/Linux interop is unchanged in this bounded cutover.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,244 findings, down five at this exact
checkpoint, and still exits 1. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency. Exact evidence is
[archived](archive/CURRENT-native-cef-ingress-cutover-checkpoint-2026-11-03.md).

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
  source `a33f57df9c70ce4cff5434f622437f78b54a801f`, producing Development
  build `26.4.0d68127` (`26.4.0.68127`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `970a08af785d28f1ca111188c2a41beca3aedc9adc1ebcb93cc6aafc5c04decb`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,221
  assertions across 52 cases with API validation enabled. Its focused real
  IOSurface import/flip case passed 16 assertions.
- The source-exact renderer suite passed 1,216 assertions across 85 cases with
  Metal API validation enabled.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- Strict inventory is 2,408 files / 3,244 findings, expected exit 1. Both
  OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Accelerated Darwin CEF ingress is native-only; ordinary CPU media upload, complete parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,244 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Zero direct production framebuffer or texture-image readbacks remain; live duplicate GL resource and traversal owners are next. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. A live moving-media ingress path now
imports and publishes its IOSurface entirely through Metal on Darwin, removing
its duplicate GL resource, CGL binding and GL framebuffer copy while preserving
fail-closed publication.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 within the same media ownership
boundary. Ordinary CPU-painted CEF/VLC frames still allocate and upload both a
GL texture and its Metal mirror on Darwin. Give that path an explicit native
allocation/partial-update/publication contract before removing its GL side.

## Exact next actions

1. Add an exact Darwin native-only `LLImageGL` media upload boundary that
   preserves format, dirty rectangles, content stamps and async publication.
2. Route ordinary CPU-painted CEF/VLC updates through it, prove the byte layouts
   and ordering physically, and keep moving-media appearance in the existing
   daily owner-attended queue.
