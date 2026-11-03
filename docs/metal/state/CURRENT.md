# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `218b60d8b6`

**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`218b60d8b6` preserves the animation-upload preview while making its Darwin
result targetless and Metal-authoritative. CPU camera matrices replace the
retained render transaction. The native producer advances the preview avatar,
prepares its current LOD/mesh data and draws the same visible eye, head, upper,
lower and conditional eyelash meshes with their rigid or scalar-skinned
transforms, default avatar texture, one-light preview shading, alpha cutoff and
camera controls.

The animation path also implements the avatar-clothing deformation used by the
former shader, including the lit cloth variant missing from the earlier Metal
mirror. Its sRGB sampling descriptor renders through a compatible linear
attachment view, preserving encoded RGB bytes; surviving fragments publish
opaque alpha, matching the former logical RGB target. A complete texture
becomes sole authoritative backing. Missing geometry, texture, device,
renderer or publication preserves the previous valid image and retries. The
old Darwin background/avatar GL draws, camera/matrix transaction and
framebuffer-copy publication are deleted from this producer.

Prior native image-upload previews, legacy-bump, material-preview, dynamic
resource, post FXAA/SMAA/CAS, DoF, world-bloom, advanced-output,
world/material, shadow, UI and presentation checkpoints remain in force as
recorded in the archive. Legacy BLEND/stained glass, broader mirrors,
projector-visible shadows, moving media, the model-upload preview and complete
parity remain open.

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
  `218b60d8b6`, producing signed, validated and staged Alchemy Metal Development
  `26.4.0d68213` (`26.4.0.68213`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `0b8c099db9044e89df69bb713872f558ad7e0d49fbfb5c8ccd312c762a54ef6f`.
- Source and packaged MSL are byte-identical at
  `516ef84aa7123130e6d72cd22e07c52e0f57758085be9c265f50f4fdd89623fb`;
  metallib SHA-256 is
  `1a85d0dcb0c05ea07001f668ae9cf1b4ddb2aa9ea8a8187d27daed825485e149`.
- The exact Ninja warnings-as-errors tree reconfigured at build 68213 and linked
  `llappearance_gl_tests`, `almetal_tests`, `llrender_gl_fresh_tests`,
  `alrender_tests`, `newview_tests` and `alchemy-bin` warnings-as-errors.
- With Metal API, GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,454 assertions across 55 cases; its focused lit
  preview case passed 264 assertions and covers the new lit-cloth path;
  `llappearance_gl_tests` passed 13,207 across 3; and
  `llrender_gl_fresh_tests` passed 2,612 across 96.
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `newview_tests` self-reported build 68213 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- Harness structure validation and all 27 harness/tool tests pass.
- Strict inventory is 2,408 files / 3,244 findings, expected exit 1. The 2,933
  unique path/kinds/snippet record count is unchanged from the prior checkpoint;
  no audit rule or suppression changed. Report SHA-256:
  `8b2a1c99316a632f58ce177ac5281bc0f8eade85144e8d51a49790feb4597079`.
- Exact binary inspection finds targetless animation-preview production,
  native camera setup and authoritative Metal publication. Both OpenGL and
  Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Animation/image-upload previews, visual hints, avatar composites, legacy bump generation and established media, terrain, water, parcel, sky, minimap, fonts and edit grid have native owners. Live GL context creation/shutdown and the model-upload preview remain. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Animation/image-upload previews, visual hints, avatar composites, established advanced-output owners, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are native-owned. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,244 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Animation/image-upload previews, visual hints, avatar composites, legacy bumps, material previews and edit grid are Metal-authoritative/native-only at their documented boundaries; live initialization, traversal, the model-upload preview and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Animation-upload previews retain
playback, camera interaction, the former visible avatar mesh set, rigid and
skinned transforms, cloth deformation, default texture, preview lighting,
alpha cutoff, background, publication and retry behavior through Metal. This
is not OpenGL-count-only deletion.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Replace Darwin `LLModelPreview` retained-target production with a targetless
   authoritative Metal result.
2. Preserve static and skinned model modes, textures/material colors, edge and
   physics overlays, convex hulls, joint/bone/collision diagnostics, camera,
   UI-state updates, publication and retry semantics. Close native gaps before
   deleting its retained producer.
