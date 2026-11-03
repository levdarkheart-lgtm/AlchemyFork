# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `1830ac9d0b`

**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`b3f6124664` preserves the complete model-upload preview feature set while
making Darwin `LLModelPreview` targetless and Metal-authoritative. This is a
replacement-first cutover, not removal of preview functionality to reduce the
OpenGL inventory.

After that feature checkpoint passed, commit `1830ac9d0b` deleted the 1,019-line
retained/non-Darwin model-preview draw and five additional orphaned helper,
declaration and constant lines. The remaining `render()` route can only call
the Metal producer on Darwin; targetless scheduling and publication are the
only supported model-preview path. No feature behavior was removed by this
cleanup.

The Metal producer covers static textured/material-colored faces, optional
edges, physics meshes, convex-decomposition hulls and explode transforms. It
also covers rigged/skinned models, joint overrides, collision volumes, bones,
joint bounds, the ground plane and the former degenerate-triangle red wide-line
and point diagnostics. CPU camera state replaces the retained GL matrix
transaction. Clear-only output, UI-state updates, continuous rigged refresh,
demand-driven static/physics refresh, authoritative publication and fail-closed
retry behavior are preserved.

The shared Metal lit-preview renderer now supports clear-only transactions,
unindexed flat wide lines and points, always-write depth, screen-pixel line
expansion and point sizing. `LLVertexBuffer::ScopedMetalOnlyUpload` lets this
native producer populate Metal vertex/index stores without creating or binding
GL names. Source census confirms every known `LLViewerDynamicTexture` subclass
now implements targetless Darwin scheduling; `LLModelPreview` was the last.

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
- `tools/metal/build.sh` exited 0 at exact source `1830ac9d0b`. The default
  warnings-as-errors Xcode Release build linked, signed, validated and staged
  Alchemy Metal Development `26.4.0d68217` (`26.4.0.68217`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `c7e0c724c0dfeb55136a1e623fe14216bdb74b827795e1f76842aa1e57d21513`.
- Source and packaged MSL are byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  metallib SHA-256 is
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja warnings-as-errors tree reconfigured at build 68217 and linked
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
  `newview_tests` self-reported build 68217 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- Harness structure validation passes for 32 required files; all 27 harness/tool
  tests and `git diff --check` pass.
- Strict inventory is 2,408 files / 3,242 findings / 2,931 unique
  path/kinds/snippet records, expected exit 1. Retained model-preview source
  removal accounts for the five-finding/three-unique reduction; no audit rule
  or suppression changed. Report SHA-256:
  `82107e21c11b1654fc74517ae29df7cfafb5869d2dec8e8dde7c54665348e010`.
- Exact binary inspection finds `LLModelPreview::renderMetal()`, targetless
  scheduling and no-target post handling. Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | All known dynamic-texture subclasses are targetless on Darwin; model/animation/image previews, visual hints, avatar composites, legacy bumps and established media/terrain/water/parcel/sky/minimap/font/edit-grid resources have native owners. Live GL context creation/shutdown remains. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are Metal-owned at their documented boundaries. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,242 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Dynamic preview producers are Metal-authoritative on Darwin and retained model-preview source is deleted; live initialization, broad traversal/resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The model-upload floater keeps its
static, physics and rigged modes, materials/textures, edges/hulls/explode,
skinning and joint/bone/collision/degenerate diagnostics through native Metal.
This is not OpenGL-count-only deletion.

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
