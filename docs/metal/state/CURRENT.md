# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `04edf7b27d`

**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`04edf7b27d` makes the exact native Metal world/UI presentation transaction the
default path. Source and packaged `settings_alchemy.xml` declare
`ALMetalWorldPresentation=1`, and the C++ cached-control fallback agrees. This
promotes existing native feature behavior; it does not delete or approximate a
rendering feature.

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
an incomplete frame retains the full GL result. The hidden nonpersistent
setting remains only as a controlled fallback diagnostic while
application-owned GL remains.

This is an authority advance, not feature deletion and not an audit-count
exercise. The 2-file change adds 9 lines and deletes 8. The preceding native
non-HDR glow/presentation implementation and all earlier Metal feature owners
remain unchanged. The retained GL path has not yet been removed.

Prior native animation/image-upload previews, visual hints, avatar composites,
legacy bumps, material previews, dynamic resources, post effects, world/UI and
presentation checkpoints remain in force as archived. Legacy BLEND/stained
glass, broader mirrors, projector-visible shadows, moving media, retained
non-HDR GL presentation ownership, remaining application-owned GL
traversal/resources and complete parity remain open.

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
  `04edf7b27d`; the warnings-as-errors Xcode Release build reported `BUILD
  SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68231`
  (`26.4.0.68231`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `58026eb643ad39f15b0c42e3f0ea50f752762157a99a12ef26797b1ceff800d0`.
- Source and packaged MSL are byte-identical at
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`;
  metallib SHA-256 is
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source and packaged `settings_alchemy.xml` are byte-identical at
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  an XML query confirms the packaged Metal-presentation default is `1`.
- The exact Ninja warnings-as-errors tree reconfigured at build 68231 and linked
  `llappearance_gl_tests`, `almetal_tests`, `llrender_gl_fresh_tests`,
  `llrender_gl_shared_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin`.
- With Metal API, GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,526 assertions across 56 cases; the focused legacy
  glow case passed 42 assertions;
  `llappearance_gl_tests` passed 13,207/3; and
  `llrender_gl_fresh_tests` passed 2,612/96.
- The focused Metal-only vertex-upload case in `llrender_gl_shared_tests`
  passed 17/17 assertions and proves exact native vertex/index bytes, no GL
  name/binding production and nested scope semantics.
- The complete shared-state diagnostic suite is not a green gate on this host:
  it exited 42 with 8,352/8,357 assertions, 182 passed, 4 failed and 39 skipped
  across 225 cases. Four failures are legacy/headless frame-fence timing
  assertions and one is a depth-format expectation; the new upload case passes
  under the same validator environment.
- `alrender_tests` passed 557,301 assertions across 136 cases.
  `newview_tests` self-reported build 68231 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- Harness structure validation passes for 32 required files; all 27 harness/tool
  tests and `git diff --check` pass.
- Strict inventory is 2,406 files / 3,217 findings / 2,906 unique
  path/kinds/snippet records, expected exit 1. This is intentionally unchanged
  because this checkpoint promotes Metal authority without pretending its
  transitional GL recovery was removed; no audit rule or suppression changed.
  Report SHA-256:
  `eb8b00822f94cd6b3e7a2149891e6bb359a5ee04b670b5778de35c65ad7ff449`.
- Both OpenGL and Metal remain linked. The static, linked and runtime no-GL
  release gates remain open.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Exact native world/presentation/ordered-UI publication is Metal-first by default and fail-closed per frame. The dynamic-texture framework and all concrete producers are native-only on Darwin; previews, visual hints, avatar composites, color-grade volume/tone-curve and PBR BRDF LUTs, legacy bumps and established media/terrain/water/parcel/sky/minimap/font/edit-grid resources have native owners. Live GL context creation/shutdown remains. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Color grading, native PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible non-HDR legacy glow/presentation have Metal implementations at their documented boundaries. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,217 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The color-grade volume/tone-curve and PBR BRDF GL owners and the dynamic-preview/avatar-composite framework are retired after native replacement. Live initialization, broad traversal/resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The complete native Metal
world/presentation/ordered-UI transaction is now the normal authority request,
not an opt-in diagnostic. It uses the already implemented Metal feature paths
and keeps full GL only as per-frame transitional failure recovery. This is not
OpenGL-count-only deletion.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Instrument and census default Metal publication failures and remaining GL
   side effects without independently driving a viewer.
2. Continue replacing those responsibilities and retire the GL fallback owner
   only when complete native behavior is durable; do not trade functionality
   for a lower GL count.
