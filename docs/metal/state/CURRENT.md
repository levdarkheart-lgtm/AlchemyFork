# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `15ed39561c`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`15ed39561c` deletes 141 lines across 20 files: every source-proven unreferenced
class-level `restoreGL`/`restoreGLTexture` method after the viewer/pipeline
restore lifecycle was removed. Sky, spatial, dynamic-texture, avatar and
particle owners no longer advertise dead context recovery. Live initial bump
setup and the two lazy manipulator initializers remain; exact binary inspection
finds only those three restore symbols. The prior explicit process-shutdown
resource lifetime remains unchanged.

The prior material-preview cutover remains in force. The targetless scheduler
keeps texture/probe readiness, device/geometry/encode failure and native
publication failure on the Metal retry path. Resource planning never requests
the preview probe/post pack, and the preview manager contains no retained draw,
framebuffer-post or fallback implementation.

Native preview destination backing remains Metal-only with a zero GL texture
name. Every supported HDR, non-HDR, post-suppressed, legacy-gamma and classic
lighting configuration selects the native owner. Readiness and all failure
classes retry targetlessly; none can acquire a shared GL target, generate GL
backing or run the retained sphere/post chain on Darwin.

This is a broader production-mode cut, not full no-context execution proof.
Other dynamic textures retain GL backing, and global GL teardown also destroys
wider pipeline state.

Prior native-only FXAA/SMAA/CAS, DoF and world-bloom checkpoints remain in
force. SMAA `SamplePath` ingress is private Metal storage and affects only edge
discovery; final colors come from the display source. Native creation and
execution remain fail closed, independent CPU/physical regressions pass, and
the retained post-AA/sharpen and DoF programs remain absent from Darwin objects.
Strict inventory is now 2,408 files / 3,253 findings, without an audit-rule or
suppression change. Exact restore-method-removal evidence is
[archived](archive/CURRENT-unreferenced-gl-restore-method-removal-checkpoint-2026-11-03.md).

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
- The required default warnings-as-errors Xcode Release wrapper completed at
  exact source `15ed39561c`, producing Development build `26.4.0d68197`
  (`26.4.0.68197`). The exact Ninja Release test tree also configured and
  linked warnings-as-errors at build 68197.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64.
  Viewer executable SHA-256 is
  `f935252c86ecc1514c91fd4671e570732bff36cc81468ad77fa52d3036cb3435`.
- Source and packaged MSL are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- Under `MTL_DEBUG_LAYER=1` and shader validation, the complete native Metal
  suite passed 52,414 assertions across 55 cases. Focused lit/PBR material
  preview coverage passed 459 assertions across two cases. Established world
  bloom, DoF, CAS and final-presentation coverage remains in the complete suite.
- The full `alrender_tests` suite passed 557,312 assertions across 137 cases.
  The focused resource-plan matrix passed 3,376 assertions across 37 cases.
- The source-exact fresh-context renderer suite passed 2,243 assertions across
  94 cases. The CPU-only renderer suite passed 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- The complete source-exact `newview_tests` binary self-reported build 68197 and
  passed 98,973 assertions across 743 cases; 742 passed and one platform CRT
  case skipped as expected. A stale build-68158 binary was rejected before the
  Ninja tree was regenerated and relinked.
- The broad retained-GL suite passed 8,335/8,340 assertions and 181/224 cases;
  its four established frame-fence/depth-format cases failed and 39 skipped.
  The run is recorded as failed, not promoted to green.
- Strict inventory is 2,408 files / 3,253 findings, expected exit 1. The exact
  report SHA-256 is
  `f2ac606640da104c73e267b880a58ff3cbe3d3a9179be890e4f812f2dae459fc`.
  Both OpenGL and Metal remain linked in the viewer and appearance utility;
  neither has an undefined `CGLTexImageIOSurface2D` dependency.
- `nm` finds no packaged-viewer symbol for the six retained
  FXAA/SMAA/CAS/sharpen globals, the six retained DoF globals,
  `LLPipeline::renderDoF` or `depthOfFieldProgramsReady`.
- Exact Release-object inspection finds no retained preview shader/post globals,
  bloom/exposure/color-correction calls, GL batch submission, transient target
  acquisition, framebuffer copy, GL-matrix or retained-readiness/fallback
  symbols. Those implementations are deleted rather than source-guarded; the
  Apple-only preview transaction remains Metal-native.
- Exact packaged-viewer symbol inspection finds only
  `LLBumpImageList::restoreGL()`, `LLManipTranslate::restoreGL()` and
  `FSManipTranslateJoint::restoreGL()`; no deleted restore method remains.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | The dead context-restore lifecycle and unreferenced owner methods are deleted; Darwin media, terrain, water, parcel, CPU sky faces, legacy sky cube, minimap and font-atlas resources are native-only. Live GL context creation/shutdown and queued parity remain open. |
| M-007 world/material | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, color grading, tone curve, BRDF, post FXAA/SMAA/CAS, DoF and world-view HDR bloom/cross-filter are native-only; all supported PBR preview modes own geometry/post/publish. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,253 and exits 1; full parity/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Material previews are Metal-only; obsolete restore authority and unreferenced owner methods are deleted. Live initialization, traversal, resource and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Twenty owner files no longer carry dead
context-recovery APIs after recovery authority was deleted. This narrows the
surface before live shutdown/resource cutover while preserving the prior
Metal-only preview cutover and the three intentional initialization paths.

**Still incomplete:** the executable links OpenGL and still creates and shuts
down a live GL context. Application-owned GL traversal and resources remain.
Complete feature/stability, profiling/performance and release gates are not met.
Prolonged stability and independent build/review evidence remain open; strict no-GL
is not satisfied. Visual parity cases remain queued, not waived.

**Evidence-based priority:** remove the empty particle shutdown wrapper, then
trace the remaining shutdown-only `destroyGL`/`cleanupGL` owners to their native
resource lifetimes before removing live GL teardown.

## Exact next actions

1. Delete the empty `LLVOPartGroup::destroyGL()` shutdown wrapper and caller.
2. Trace the remaining real sky, manipulator, bump, font, avatar,
   dynamic-texture, box and texture-list shutdown releases owner by owner; keep
   visual questions queued for the daily owner session.
