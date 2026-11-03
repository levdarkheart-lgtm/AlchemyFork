# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `18a8b6924f`

**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`18a8b6924f` preserves appearance-editor visual-parameter previews while making
their Darwin result targetless and Metal-authoritative. CPU preview-camera setup
and pipeline visibility/state sorting replace the retained render transaction;
the native lit-preview scene keeps system-avatar meshes, cloth, rigid/rigged
attachments, attachment alpha ordering, legacy/PBR materials, fullbright/mask
behavior and the authored background.

Appearance hints retain an sRGB sampling descriptor while using a compatible
linear attachment view so already encoded preview bytes are not encoded twice.
A complete texture publishes as sole authoritative backing. Missing geometry,
resources or publication preserves the previous valid image and retries. The
old Darwin background GL draw, camera/depth/blend transaction,
`generateImpostor(..., true)` draw and framebuffer-copy publication are deleted.

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
  `18a8b6924f`, recording 175 cacheable tasks and producing signed, validated
  and staged Alchemy Metal Development `26.4.0d68209` (`26.4.0.68209`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64;
  deep strict code-sign verification passed. Viewer executable SHA-256 is
  `70723d54a7afc5ed798d0683a22a430014e5476c797a41a00a11ad38a48ccf04`.
- Source and packaged MSL are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- The exact Ninja Release tree reconfigured at build 68209 and linked
  `llappearance_gl_tests`, `almetal_tests`, `llrender_gl_fresh_tests`,
  `alrender_tests`, `newview_tests` and `alchemy-bin` warnings-as-errors.
- With Metal API, GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,427 assertions across 55 cases;
  `llappearance_gl_tests` passed 13,207 across 3; and
  `llrender_gl_fresh_tests` passed 2,612 across 96.
- `alrender_tests` passed 557,312 assertions across 137 cases.
  `newview_tests` self-reported build 68209 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- Harness structure validation and all 27 harness/tool tests pass.
- Strict inventory is 2,408 files / 3,244 findings, expected exit 1. All 2,933
  unique path/kind/snippet tuples are unchanged from build 68207; no audit rule
  or suppression changed. Report SHA-256:
  `3a91085405bf2dad63efa126dd0723aba15226fb018b277a6829c9b9b1d55e68`.
- Exact binary inspection finds targetless visual-hint rendering, native camera
  setup, avatar scene preparation and authoritative Metal publication. Both
  OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Visual hints, avatar composites, legacy bump generation and established media, terrain, water, parcel, sky, minimap, fonts and edit grid have native owners. Live GL context creation/shutdown and other dynamic textures remain. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Visual hints, avatar composites, established advanced-output owners, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are native-owned. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,244 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Visual hints, avatar composites, legacy bumps, previews and edit grid are Metal-authoritative/native-only at their documented boundaries; live initialization, traversal, other resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Appearance-editor visual hints retain
camera framing, body/clothing geometry, cloth, attachments, alpha ordering,
materials, background, publication and retry behavior through Metal. This is
not OpenGL-count-only deletion.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain open.
The strict no-GL gate remains open.

## Exact next actions

1. Replace Darwin `LLImagePreviewAvatar` and `LLImagePreviewSculpted`
   retained-target producers with targetless authoritative Metal results.
2. Preserve camera/interaction, selected mesh and texture, preview lighting,
   alpha and retry semantics; add exact physical GPU coverage before removing
   the old Darwin producers.
