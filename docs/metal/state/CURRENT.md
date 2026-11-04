# Current engineering state

**Observed:** 2026-11-04 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `3984366670`

**Historical evidence:** [archive index](archive/INDEX.md)

This concise recovery snapshot delegates superseded detail to the indexed archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commits
`8a4c9aee97` and `3984366670` make system/default/local raw textures and decoded
in-memory fetched textures publish directly to Metal when GL is disabled. They
retain exact pixels, dimensions, mips, categories, texture-list behavior and
GPU accounting without a GL name. While GL remains active, the established
dual-backed path is deliberately retained because fail-closed fallback still
needs the exact black/default/checker and decoded image pixels. This is native
replacement work, not feature deletion or premature recovery removal.

The preceding terrain checkpoint remains active. Commit `aaf3bbcd2a` makes
local terrain-paint baking Metal-authoritative while retaining full-resolution
region geometry, alpha-ramp composition, 1–8-bit weight precision, mips and
local material behavior without its GL bake/copy pass. Preceding
report-preview, local-bitmap, HDRI and fetched work remains active.
The preceding pressure checkpoint remains active. Metal-only queue entries
retain native scale-down behavior without a GL name, scratch target, copy
shader or live GL manager; timeout/resume preserves retained work order.

The preceding eviction checkpoint remains active. The viewer-facing
GPU-residency lifecycle releases native-only sculpt, fetched-result, ordinary
fetched and brightness/darkness bump Metal storage, resets metadata and
reconciles budget accounting. Explicit low-level GL detach remains at its owner.

The preceding accounting checkpoint remains active. Native Metal allocation,
replacement, scale-down, discard and teardown update the same process-wide
budget signal consumed by pressure logic, using actual descriptor storage. A
GL/Metal mirror remains one logical allocation through authoritative handoff.

The preceding native scale-down checkpoint remains active. Commit
`63f8f0d528` keeps native-authoritative texture scale-down entirely in Metal.
Mip-backed images copy the selected native mip and regenerate their tail;
single-level images use the native bilinear resampler. Both publish only after
successful allocation, encoding and command-buffer submission, retain the old
resource on failure, update lifecycle metadata on success and never create or
account a GL texture name. The legacy dual-backed route remains where complete
per-frame GL recovery is still required.

The preceding presentation checkpoint remains active. Commit `04edf7b27d`
makes the exact native Metal world/UI presentation transaction the
default path. Commit `c2bcd90b04` makes that cutover measurable per frame: the
collector distinguishes disabled, not-attempted, capture-failed,
surface-unavailable, surface-submit-failed and submitted states and records
whether requested Metal authority retained its GL swap. Source, package and
C++ fallback all enable it by default without deleting a rendering feature.

The Metal-first transaction remains fail closed during transition. It first
earns one complete world/presentation/ordered-UI/drawable submission, then
unlocks continuous publication. Every frame starts transparent, and only a
successfully committed current-frame drawable suppresses its matching GL swap;
an incomplete frame retains the full GL result. Contradictory census flags
invalidate evidence, and the analyzer reports exact/fallback rates without
turning them into a performance pass. The hidden nonpersistent
setting remains only as a controlled fallback diagnostic while
application-owned GL remains.

Prior native animation/image-upload previews, visual hints, avatar composites,
legacy bumps, material previews, dynamic resources, post effects, world/UI and
presentation checkpoints remain in force as archived. Native main-world and
impostor legacy BLEND/stained-glass behavior and broad deterministic physical
coverage already exist; its owner-attended visual parity remains open. Broader
mirrors, projector-visible shadows, moving media, retained non-HDR GL
presentation ownership, remaining application-owned GL traversal/resources
and complete parity remain open.

## Visual review cadence

Per owner direction, Codex does not independently drive reference/candidate
viewers or take comparison snapshots. Visual cases accumulate in the
[owner-attended daily review queue](../work/active/visual-review-queue.md) and
are reviewed together at most once per day. No queued case is a parity pass
until the owner-attended review records that result.

## Build and test evidence

- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- The exact `tools/metal/build.sh` invocation exited 0 at committed renderer
  source `3984366670`; the warnings-as-errors Xcode Release build reported
  `BUILD SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68258`
  (`26.4.0.68258`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `4466b1a298175e3ebc6e4624b72e14ffab77b79037835af00333545d11f657c4`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  an XML query confirms the packaged Metal-presentation default is `1`.
- The changed production targets compiled and linked warnings-as-errors. The
  focused physical system-texture case passed 53 assertions and proves exact
  null/black/default/checker pixels, mips, native residency, zero GL backing
  and teardown. `llrender_gl_fresh_tests` passed 3,363/105 under Metal API
  validation.
- `almetal_tests` passed 52,526/56 under Metal API validation and
  `newview_tests` passed 98,990/747 with the established CRT skip. Ninja and
  Xcode both linked and packaged the complete viewer warnings-as-errors.
- Harness structure validation passes for 32 required files; all 30 harness/tool
  tests and `git diff --check` pass. The documented strict-audit alias works.
- Strict inventory is 2,407 files / 3,220 findings / 2,909 unique records,
  expected exit 1; no rule or suppression changed. Report SHA-256:
  `65c7ca71d1e23307df6e031bf38932e309c52c331ec246d71b70a8ac9de70124`.
- Both OpenGL and Metal remain linked. The static, linked and runtime no-GL
  release gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | System/default/local raw and decoded-memory fetched textures publish natively when GL is disabled; report screenshots, local previews, HDRI, fetched publication, sculpt metadata, pressure scale-down and eviction are backend-correct at converted sites. Exact presentation remains Metal-first/fail-closed. Live GL context creation/shutdown remains. |
| M-007 world/material | doing | Local terrain-paint baking is Metal-authoritative with full geometry/weights/mips. Metal world capture consumes native bump backing; native legacy BLEND exists, while owner-attended visual and broad material parity remain open. |
| M-008 advanced views/effects | doing | Color grading, native PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible non-HDR legacy glow/presentation have Metal implementations at their documented boundaries. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,220 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Replaced GL owners are retired only after native coverage; system/local and in-memory factories are native-capable after GL disable, while active fail-closed recovery is intentionally retained. Live initialization, broad traversal/resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Exact system/default/local and decoded
in-memory textures now survive a disabled GL manager through native Metal
publication, preserving their pixels, mips and lifecycle. Active recovery was
retained where removing it would degrade fallback output. No feature was
deleted for the cutover.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Trace startup local-file preload readiness (`white.tga`, missing asset, flat
   normal, default irradiance and the UI image list) and the transition from
   fetched-queue `setGLTextureCreated(true)` state to native-only publication.
2. Convert the next bounded producer only after every consumer and failure path
   has a native owner. Queue any visual question for the owner-attended daily
   review; do not add an independent viewer-comparison run.
