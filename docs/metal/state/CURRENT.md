# Current engineering state

**Observed:** 2026-11-04 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `0789086de1`

**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`0789086de1` replaces the viewer-facing GL-name-gated destruction wrapper with
an explicit GPU-residency lifecycle. Native-only sculpt, fetched-result and
ordinary fetched eviction now release their Metal texture, reset discard/state
metadata and reconcile budget accounting. Brightness/darkness bump eviction
uses the same contract; the non-Darwin media recreation call retains equivalent
GL behavior. The low-level explicit GL detach primitive remains at its owner.

The preceding scheduler checkpoint remains active. Native-authoritative Metal
images participate in redundant-load, post-create, shutdown-precache and queued
scale-down decisions without a GL name or GL scratch state. Transitional
GL/dual-backed entries retain their fail-closed path and queue order.

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
  `0789086de1`; the cold warnings-as-errors Xcode Release build reported
  `BUILD SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68241`
  (`26.4.0.68241`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `ce2bf076c43f68e5ad0218602ba775d921652d3aea4f8a701e88ee6a37a4a28a`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  an XML query confirms the packaged Metal-presentation default is `1`.
- The changed `llrender_gl_fresh_tests` target compiled warnings-as-errors.
  Its focused physical Metal slice passed 1,972 assertions across 20 cases;
  the complete binary passed 2,683 assertions across 100 cases. The new case
  proves native wrapper destruction releases Metal storage, metadata and bytes.
- A cold Ninja Release `llrender_gl_fresh_tests alchemy-bin` build also passed
  warnings-as-errors and linked/packaged the complete viewer.
- The preceding source-exact physical baselines remain `almetal_tests` at
  52,526/56, `llappearance_gl_tests` at 13,208/3, `alrender_tests` at
  557,301/136 and `newview_tests` at 98,978/744 with the established CRT skip.
- Harness structure validation passes for 32 required files; all 29 harness/tool
  tests and `git diff --check` pass.
- Strict inventory is 2,406 files / 3,220 findings / 2,909 unique
  path/kinds/snippet records, expected exit 1. No rule or suppression changed;
  this informational count is not used as feature evidence. Report SHA-256:
  `0fd3f423a7552de947b09fcad4685f9e6ac650a2715c1965c96675e97af3d9b5`.
- Both OpenGL and Metal remain linked. The static, linked and runtime no-GL
  release gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Native-authoritative allocation, pressure scale-down and eviction are Metal-aware, process-wide budgeted and no longer require GL scratch state or a GL name at converted lifecycle sites. Exact world/UI presentation remains Metal-first and fail-closed. Live GL context creation/shutdown remains. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Native legacy BLEND/stained-glass implementation and deterministic coverage exist; owner-attended visual and broad material parity remain open. |
| M-008 advanced views/effects | doing | Color grading, native PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible non-HDR legacy glow/presentation have Metal implementations at their documented boundaries. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,220 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The color-grade volume/tone-curve and PBR BRDF GL owners and the dynamic-preview/avatar-composite framework are retired after native replacement. Live initialization, broad traversal/resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Native-authoritative images now retain
eviction and pressure-management behavior through Metal allocation, scaling
and destruction. Converted owners release real Metal storage and accounting;
this is retained lifecycle behavior, not OpenGL-count-only deletion.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Move native downscale draining ahead of the global GL-disabled return so
   already-Metal work remains pressure-manageable without a live GL manager.
   Keep upload and GL/dual-backed branches fail-closed until fetched upload and
   their remaining consumers have native replacements; add deterministic tests.
2. In the next owner-attended session, collect the default-path publication
   census without adding an independent viewer-comparison run. Use measured
   failure states to repair blockers, then retire GL recovery only when complete
   native behavior is durable.
