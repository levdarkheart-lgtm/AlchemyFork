# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `7f3bbc58a0`

**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`7f3bbc58a0` preserves avatar and sculpt image-upload previews while making their
Darwin results targetless and Metal-authoritative. CPU camera matrices replace
the retained render transaction. The avatar producer keeps the chosen body or
clothing mesh, real preview texture, joint skinning, lighting, tint and alpha
cut; the sculpt producer keeps the generated geometry, scale and three-light
solid preview.

Both image-preview producers retain sRGB sampling descriptors while rendering
through compatible linear attachment views, preserving the encoded bytes that
the former RGB retained targets produced. Surviving avatar fragments publish
opaque alpha after the existing cutoff, matching those logical RGB targets.
Complete textures become sole authoritative backing; missing geometry,
resources or publication preserves the previous valid image and retries. The
old Darwin background, avatar/sculpt GL draws, camera/matrix transaction and
framebuffer-copy publication are deleted from these producers.

Prior native legacy-bump, material-preview, dynamic resource, post
FXAA/SMAA/CAS, DoF, world-bloom, advanced-output, world/material, shadow, UI and
presentation checkpoints remain in force as recorded in the archive. Legacy
BLEND/stained glass, broader mirrors, projector-visible shadows, moving media,
remaining dynamic textures and complete parity remain open.

## Visual review cadence

Per owner direction, Codex does not independently drive reference/candidate
viewers or take comparison snapshots. Visual cases accumulate in the
[owner-attended daily review queue](../work/active/visual-review-queue.md) and
are reviewed together at most once per day. Source inspection, builds, physical
GPU tests, static/link audits and profiling continue between reviews. No queued
case is a parity pass until the owner-attended review records that result.

## Build and test evidence

- `tools/metal/doctor.sh` passed on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- The default warnings-as-errors Xcode Release wrapper completed at exact source
  `7f3bbc58a0`, producing signed, validated and staged Alchemy Metal Development
  `26.4.0d68211` (`26.4.0.68211`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `dbe8db4893d4c5cd647fc72b8b439dd3d55a79a5b4ab432a59b90c0ef93a0b64`.
- Source and packaged MSL are byte-identical at
  `de17600b1fec7059e4b6417737d58dc6c47757b9ddee1f9d244096caa920c4a1`;
  metallib SHA-256 is
  `f0832637526fad05fa96fe8aed5aa3216e2c88b5b09f9f74e71d8f352b13cda7`.
- The exact Ninja warnings-as-errors tree reconfigured at build 68211 and linked
  `llappearance_gl_tests`, `almetal_tests`, `llrender_gl_fresh_tests`,
  `alrender_tests`, `newview_tests` and `alchemy-bin` warnings-as-errors.
- With Metal API, GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,447 assertions across 55 cases;
  `llappearance_gl_tests` passed 13,207 across 3; and
  `llrender_gl_fresh_tests` passed 2,612 across 96.
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `newview_tests` self-reported build 68211 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- Harness structure validation and all 27 harness/tool tests pass.
- Strict inventory is 2,408 files / 3,244 findings, expected exit 1. The 2,933
  unique path/kinds/snippet record count is unchanged from build 68209; one
  wrapper snippet changed with the refactor, and no audit rule or suppression
  changed. Report SHA-256:
  `07b221b3fccbab07c30cb11c086711b9a4b519c1db67ab4271b13e52b47f3eaf`.
- Exact binary inspection finds targetless avatar/sculpt image-preview methods,
  native camera setup and authoritative Metal publication. Both OpenGL and
  Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Image-upload previews, visual hints, avatar composites, legacy bump generation and established media, terrain, water, parcel, sky, minimap, fonts and edit grid have native owners. Live GL context creation/shutdown and other dynamic textures remain. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Image-upload previews, visual hints, avatar composites, established advanced-output owners, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are native-owned. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,244 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Image-upload previews, visual hints, avatar composites, legacy bumps, material previews and edit grid are Metal-authoritative/native-only at their documented boundaries; live initialization, traversal, other resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Avatar/sculpt image-upload previews
retain camera interaction, selected geometry/texture, skinning, preview
lighting, tint, alpha cutoff, background, publication and retry behavior
through Metal. This is not OpenGL-count-only deletion.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Replace Darwin `LLPreviewAnimation` retained-target production with a
   targetless authoritative Metal result.
2. Preserve animation playback, camera interaction, system-avatar rigid and
   skinned meshes, cloth/alpha behavior, default texture, publication and retry
   semantics; add physical GPU evidence before deleting its retained producer.
