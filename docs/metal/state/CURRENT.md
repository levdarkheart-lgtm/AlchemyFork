# Current engineering state

**Observed:** 2026-11-04 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `ebd07151a3`

**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`ebd07151a3` routes fetched-texture pressure decisions through actual GPU
residency. Native-authoritative Metal images now participate in redundant-load,
post-create, shutdown-precache and queued scale-down decisions. The scheduler
drains those images through `LLImageGL::scaleDown()` without a GL name, scratch
target, copy shader, framebuffer binding or blend/color state. Transitional
GL/dual-backed images retain their established fail-closed path and relative
queue order; no feature was deleted for an inventory reduction.

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

The native path preserves the 512-wide reduced glow target, power-of-two
height, RGBA8/RGBA16F precision choice, luminance/warmth/alpha extraction,
static true-noise input, alternating eight-tap blur and additive combine. It
also extends FXAA, SMAA and CAS to RGBA8 and preserves the production order:
display mapping and first-boundary dither, glow extraction, AA, CAS, glow
combine, RLVa sphere, scopes/reference and final presentation. Non-HDR avoids
double dithering and no longer depends on HDR-only exposure, flare, dirt,
bloom or cross-filter readiness.

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
- The exact `tools/metal/build.sh` invocation exited 0 at committed source
  `ebd07151a3`; the cold warnings-as-errors Xcode Release build reported
  `BUILD SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68239`
  (`26.4.0.68239`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `f2660391e4bbc98632509ad3bbff106e57b7f8b162cd85e7ae7df229722bcbe1`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  an XML query confirms the packaged Metal-presentation default is `1`.
- The changed `llrender_gl_fresh_tests` target compiled warnings-as-errors.
  Its focused physical Metal slice passed 1,960 assertions across 19 cases;
  the complete binary passed 2,671 assertions across 99 cases. The two new
  assertions prove a Metal-only wrapper is GPU-resident but not GL-resident.
- A cold Ninja Release `llrender_gl_fresh_tests alchemy-bin` build also passed
  warnings-as-errors and linked/packaged the complete viewer.
- The preceding source-exact physical baselines remain `almetal_tests` at
  52,526/56, `llappearance_gl_tests` at 13,208/3, `alrender_tests` at
  557,301/136 and `newview_tests` at 98,978/744 with the established CRT skip.
- Harness structure validation passes for 32 required files; all 29 harness/tool
  tests and `git diff --check` pass.
- Strict inventory is 2,406 files / 3,217 findings / 2,906 unique
  path/kinds/snippet records, expected exit 1. This is intentionally unchanged
  because this checkpoint promotes Metal authority without pretending its
  transitional GL recovery was removed; no audit rule or suppression changed.
  Report SHA-256:
  `4b3803cc47b02651b3a6a44f246253bcba20df37f45b53aa7f4847e9fad2b6a7`.
- Both OpenGL and Metal remain linked. The static, linked and runtime no-GL
  release gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Exact native world/presentation/ordered-UI publication is Metal-first by default and fail-closed. Native-authoritative scale-down is Metal-only, process-wide budgeted and now directly scheduled from fetched-texture pressure without GL scratch state. Dynamic resources and established media/terrain/water/parcel/sky/minimap/font/edit-grid owners remain native. Live GL context creation/shutdown remains. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Native legacy BLEND/stained-glass implementation and deterministic coverage exist; owner-attended visual and broad material parity remain open. |
| M-008 advanced views/effects | doing | Color grading, native PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible non-HDR legacy glow/presentation have Metal implementations at their documented boundaries. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,217 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The color-grade volume/tone-curve and PBR BRDF GL owners and the dynamic-preview/avatar-composite framework are retired after native replacement. Live initialization, broad traversal/resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Native-authoritative fetched images now
retain the viewer's existing pressure-management behavior, including queued
scale-down, without reacquiring GL storage or binding GL scratch state. This is
a Metal implementation of retained behavior, not OpenGL-count-only deletion.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Make texture destruction semantics backend-neutral at lifecycle sites that
   mean "drop GPU residency." In particular, native-only sculpt storage must be
   evictable without pretending it has a GL name, while call sites that truly
   mean GL-only detach retain explicit GL semantics. Add physical lifecycle
   regression before removing any superseded GL owner.
2. In the next owner-attended session, collect the default-path publication
   census without adding an independent viewer-comparison run. Use measured
   failure states to repair blockers, then retire GL recovery only when complete
   native behavior is durable.
