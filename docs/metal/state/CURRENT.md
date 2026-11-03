# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `026a97804b`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit `026a97804b`
cuts ordinary CPU-painted CEF/VLC frame upload off OpenGL on Darwin. The media
worker wraps only the plugin's valid `data_width x data_height` bytes, uploads
that complete frame into a pending private Metal mirror and retains any larger
power-of-two destination padding. It no longer allocates or modifies a GL
texture, calls `setSubImage`, or publishes a GL name/fence for each macOS frame.

The plugin provides a complete valid CPU frame, so this path deliberately
uploads the full valid extent rather than using its dirty rectangle. A native
mirror becomes current only after its shared Metal batch commits successfully
and main-thread publication runs. Failed encoding, posting or commit cannot
publish playing state. The accelerated IOSurface path remains native-only;
Windows/Linux media upload and interop remain unchanged in this bounded cutover.

There are zero direct production framebuffer or texture-image readback calls.
Strict inventory is 2,408 files / 3,248 findings and still exits 1. The four
added findings are scanner bookkeeping for three new `LLImageGL` method
definitions and one typed use, not GL calls. The packaged viewer has no
`CGLTexImageIOSurface2D` dependency. Exact evidence is
[archived](archive/CURRENT-native-cpu-media-upload-checkpoint-2026-11-03.md).

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
- The required default warnings-as-errors Release wrapper completed at exact
  source `026a97804bb361c173e2b473e7a61f2e6bf02d82`, producing Development
  build `26.4.0d68129` (`26.4.0.68129`). The separate warnings-as-errors
  RelWithDebInfo appearance-utility build also completed at that source.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `1de4c5c6cdf278c44949b6b409593b62e4c3928f042c68749eab38c7ac693dc7`.
  Appearance-utility SHA-256 is
  `a53d96036859d90452b133820a11ba1038312c1dd0eefdcc031af3694e824000`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Under `MTL_DEBUG_LAYER=1`, the complete native Metal suite passed 52,221
  assertions across 52 cases with API validation enabled.
- The source-exact renderer suite passed 1,499 assertions across 87 cases with
  Metal API validation enabled, including the new valid-frame/padding and
  deferred worker-publication regressions.
- The complete source-exact `newview_tests` binary passed 98,973 assertions
  across 743 cases; 742 passed and one platform CRT case skipped as expected.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,248 findings, expected exit 1. Both
  OpenGL and Metal remain linked in the viewer and appearance utility.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Accelerated and ordinary Darwin CEF/VLC ingress/upload are native-only; moving-media parity and GL context removal remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners are native; visual-only validation is batched in the daily queue. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,248 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Zero direct production framebuffer or texture-image readbacks remain; live duplicate GL resource and traversal owners are next. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Both live Darwin media update modes now
publish through Metal: accelerated CEF imports IOSurfaces natively, while
ordinary CEF/VLC CPU frames upload into private Metal mirrors without per-frame
GL allocation, mutation or GL synchronization. Publication remains fail closed.

**Still incomplete:** the executable links OpenGL; application-owned GL context,
traversal and resources remain; complete feature/stability, strict audit,
profiling/performance, independent review and release gates are not met.
Prolonged stability remains open; independent build/review evidence remains
open; strict no-GL is not satisfied.
Visual parity cases remain queued, not waived.

**Evidence-based priority:** continue M-011 at the general texture ownership
boundary. `LLImageGL` still contains 43 direct GL-call findings, and production
callers of `createGLTexture` still allocate duplicate storage. Trace the
highest-frequency fetched-texture owner through its native consumers before
removing its Darwin GL allocation/upload authority.

## Exact next actions

1. Inventory the Darwin production callers of `LLImageGL::createGLTexture` and
   `setImage`, beginning with `LLViewerFetchedTexture::createTexture`, and mark
   each retained GL consumer versus native sampling consumer from source.
2. Give the first fully native-consumed owner an explicit allocation/upload/
   publication contract, remove its duplicate Darwin GL authority, and add
   byte/ordering regressions. Keep visual questions in the daily owner-attended
   queue rather than launching a viewer.
