# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `31eb491c27`

**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`31eb491c27` retires the residual GL-only PBR BRDF lookup owner after native
world rendering and native PBR material previews already generated and
consumed validated Metal RG16F tables.

The GGX split-sum integration, 512x512 RG16F precision, bilinear-clamp
sampling, world base/local/projector/pre-water consumers, PBR-preview consumer
and command-batch readiness/retry behavior remain. Only the duplicate GL
resource-plan entry/target, fullscreen generator program and GLSL sources,
shader reload lifecycle, sampler binding and reserved uniform were deleted.

This is replacement-first source retirement, not feature deletion. The
12-file change adds 8 lines and deletes 370. Physical Metal validation checks
the generated table against an independent six-point GGX oracle and the full
suite exercises real world and PBR-preview consumers. Source inspection finds
no residual `gDeferredGenBrdfLutProgram`, `mPbrBrdfLut`, `Id::BrdfLut`,
`DEFERRED_BRDF_LUT`, `brdfLut` sampler or `genbrdflut` GLSL owner.

Prior native animation/image-upload previews, visual hints, avatar composites,
legacy bumps, material previews, dynamic resources, post effects, world/UI and
presentation checkpoints remain in force as archived. Legacy BLEND/stained
glass, broader mirrors, projector-visible shadows, moving media, remaining
application-owned GL traversal/resources and complete parity remain open.

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
  `31eb491c27`; the warnings-as-errors Xcode Release build reported `BUILD
  SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68227`
  (`26.4.0.68227`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `1d55cd4654a54209b6f3fd56ea3c647d007f6091aaa9633f0b8edbc82119973e`.
- Source and packaged MSL are byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  metallib SHA-256 is
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja warnings-as-errors tree reconfigured at build 68227 and linked
  `llappearance_gl_tests`, `almetal_tests`, `llrender_gl_fresh_tests`,
  `llrender_gl_shared_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin`.
- With Metal API, GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,468 assertions across 55 cases;
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
  `newview_tests` self-reported build 68227 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- Harness structure validation passes for 32 required files; all 27 harness/tool
  tests and `git diff --check` pass.
- Strict inventory is 2,406 files / 3,217 findings / 2,906 unique
  path/kinds/snippet records, expected exit 1. This is seven findings
  and seven unique records below the prior checkpoint; no audit rule or
  suppression changed. Report SHA-256:
  `cb28d7ae17d4ee6555cf86c9132f189c3bfb221bba03c1fdf1e8cf26705e2938`.
- Exact source inspection finds no residual GL BRDF lookup owner. Both OpenGL
  and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | The dynamic-texture framework and all concrete producers are native-only on Darwin; previews, visual hints, avatar composites, color-grade volume/tone-curve and PBR BRDF LUTs, legacy bumps and established media/terrain/water/parcel/sky/minimap/font/edit-grid resources have native owners. Live GL context creation/shutdown remains. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Color grading, native PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are Metal-owned at their documented boundaries. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,217 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The color-grade volume/tone-curve and PBR BRDF GL owners and the dynamic-preview/avatar-composite framework are retired after native replacement. Live initialization, broad traversal/resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. PBR retains native Metal GGX lookup
generation and its world/preview consumers while the duplicate GL target,
generator and sampler lifecycle are gone. This is not OpenGL-count-only
deletion.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Census the next application-owned Darwin GL producer and its user-visible
   responsibilities.
2. Implement and validate its full feature behavior in Metal before retiring
   that producer; do not trade functionality for a lower GL count.
