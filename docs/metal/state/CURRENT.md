# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `3269c260f7`

**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`3269c260f7` retires the residual GL-only color-grading volume owner after the
supported Darwin product already had a validated Metal LUT resource and native
display-post consumer.

The `.cube` decoder, image-strip decoder, component validation, grade controls
and failure behavior remain. `LLPipeline` still uploads the decoded volume to
`mMetalCGLut` through `alMetalCreateUnormVolumeArray`; native display post still
configures and samples that resource. The deleted `ALTexture3D` unit and its
unused `ALImageSlot`/`LLGLSLShader` overloads had no remaining supported-platform
construction or execution path.

This is replacement-first source retirement, not feature deletion. The
11-file change adds 15 lines and deletes 330, including the 216-line obsolete
GL texture unit. Deterministic Metal tests retain exact volume upload and real
display-post LUT sampling/configuration coverage. Source inspection finds no
`ALTexture3D`, `mCGLut` or `altexture3d` reference other than the
backend-neutral LUT-size parameter.

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
- The first `tools/metal/build.sh` invocation exited 65 because the existing
  generated Xcode project tried to scan the just-deleted `altexture3d.cpp`
  before its `ZERO_CHECK` regeneration completed. That invocation regenerated
  the project. The second exact invocation exited 0 at committed source
  `3269c260f7`; the warnings-as-errors Xcode Release build reported `BUILD
  SUCCEEDED` and staged Alchemy Metal Development `26.4.0d68223`
  (`26.4.0.68223`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `7b57f992a4adcf1dcb14eb8dcbc6c5e2738c72f16ff2e4a20cc246fa5b8dce18`.
- Source and packaged MSL are byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  metallib SHA-256 is
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja warnings-as-errors tree reconfigured at build 68223 and linked
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
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `newview_tests` self-reported build 68223 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- Harness structure validation passes for 32 required files; all 27 harness/tool
  tests and `git diff --check` pass.
- Strict inventory is 2,406 files / 3,231 findings / 2,920 unique
  path/kinds/snippet records, expected exit 1. This is two files, ten findings
  and ten unique records below the prior checkpoint; no audit rule or
  suppression changed. Report SHA-256:
  `bc06ed5b7171d9ce54b4c68cccb1d8708420b2b8b3f8464403be033b120c3184`.
- Exact source inspection finds no residual GL LUT-volume owner or unused
  binding overload. Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | The dynamic-texture framework and all concrete producers are native-only on Darwin; previews, visual hints, avatar composites, color-grade LUTs, legacy bumps and established media/terrain/water/parcel/sky/minimap/font/edit-grid resources have native owners. Live GL context creation/shutdown remains. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Color grading, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are Metal-owned at their documented boundaries. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,231 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | The color-grade GL volume owner and the dynamic-preview/avatar-composite framework are retired after native replacement. Live initialization, broad traversal/resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Color grading retains its decoders,
controls, Metal volume upload and native display-post sampling while the dead
GL volume-texture implementation and unused binding API are gone. This is not
OpenGL-count-only deletion.

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
