# Current engineering state

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.
**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `ec69e977e7`
**Historical evidence:** [archive index](archive/INDEX.md)

This is the concise recovery snapshot. Superseded detail remains in the indexed,
append-only archives.

## Active implementation checkpoint

M-003, M-004, M-008, M-009 and M-011 are `doing`; M-012 is complete. Commit
`ec69e977e7` preserves the live legacy bump feature while replacing its Darwin
OpenGL ownership. Standard bump assets now publish their CPU-produced normal
pixels directly into Metal. Brightness/darkness bump maps use
`ALMetalNormalMapGenerator`, generate the complete native mip chain and publish
the render texture as the image's sole authoritative backing. Cache retention
and world capture inspect Metal backing, and missing native source data retries
without constructing an empty or stale GL fallback.

`LLImageGL::adoptMetalRenderTextureAuthoritative()` validates the produced
descriptor, retires any predecessor GL name and makes native mip/memory/category
metadata authoritative. A physical regression starts with dual backing, renders
and mip-generates a Metal texture, publishes it authoritatively, proves the GL
name is zero and reads level zero back byte-for-byte.

The obsolete normal-generation GLSL program, shader assets and GL render target
are deleted only after the native producer replaced their live behavior.
Separately, commit `0fafef5f97` deletes the manipulator
`highlightIntersection()` contract because its complete shipping body was
already behind `#if 0`; it removed no runtime feature. Live edit-grid behavior
continues through the prior native authored-mip implementation.

Prior material-preview, dynamic resource, post FXAA/SMAA/CAS, DoF, world-bloom,
advanced-output, world/material, shadow, UI and presentation checkpoints remain
in force as recorded in the archive. Legacy BLEND/stained glass, broader
mirrors, projector-visible shadows, moving media and complete parity remain
open.

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
  `ec69e977e7`, rebuilding 1,091 cacheable tasks and producing Alchemy Metal
  Development `26.4.0d68205` (`26.4.0.68205`).
- Two deleted `normgen*.glsl` files were found as stale incremental package
  outputs. Only those exact regenerable Release outputs were removed; a second
  wrapper build re-signed the app, and source and package searches now find
  neither file. This was staging hygiene, not a source or audit suppression.
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O slices as ARM64,
  and deep strict code-sign verification passed. Viewer executable SHA-256 is
  `86fb1fc0426edd0dc60f112ee424e883d23c15ee2ddbed62568140f9935e4645`.
- Source and packaged MSL are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- The exact Ninja Release tree reconfigured at build 68205 and linked all four
  required test targets warnings-as-errors.
- With Metal API, GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,414 assertions across 55 cases.
- Under the same validation, `llrender_gl_fresh_tests` passed 2,612 assertions
  across 96 cases, including authoritative publication, GL retirement, native
  mip metadata and byte-exact physical readback.
- `alrender_tests` passed 557,312 assertions across 137 cases.
- `newview_tests` self-reported build 68205 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- Strict inventory is 2,408 files / 3,233 findings, expected exit 1: 18 fewer
  than the prior checkpoint with no rule or suppression change. Eight belong to
  proven-dead intersection deletion; ten belong to native bump replacement.
  Report SHA-256:
  `5599cf095b7d7d2a6b92c1455327dae18648e57196753703f3ca3c9a799ca33e`.
- Exact binary inspection finds `LLBumpImageList::restoreResources()`,
  `ALMetalNormalMapGenerator` and authoritative native publication; it finds
  no bump `restoreGL`, retained bump render target or normal-generation GLSL
  program. Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Milestone snapshot

| Area | State | Current fact |
| --- | --- | --- |
| M-003..M-006 platform/resources/UI | doing | Legacy bump generation plus established media, terrain, water, parcel, sky, minimap, fonts and edit grid are native-only. Live GL context creation/shutdown and other dynamic textures remain. |
| M-007 world/material | doing | Metal world capture consumes native bump backing; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 advanced views/effects | doing | Established advanced-output owners, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are native-only. |
| M-009 parity/stability/no-GL | doing | Strict inventory is 3,233 and exits 1; feature/stability and linked/runtime no-GL gates remain open. |
| M-010 measured performance | deferred | No qualifying paired, quality-equivalent M1 Max claim exists. |
| M-011 GL authority removal | doing | Legacy bump maps, previews and edit grid are Metal-only; live initialization, traversal, other resources and shutdown GL paths remain. |
| M-012 Computer Use bridge | complete | Semantic operation is verified; comparison driving is owner-attended only. |
| M-013 RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum Metal/no-GL behavior remains required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. The user-visible standard,
brightness-derived and darkness-derived bump feature remains present with
native production, mip generation, publication, cache lifetime and world
consumption. This is not OpenGL-count-only deletion.

**Still incomplete:** the executable links OpenGL and still creates/shuts down a
live GL context. Application-owned GL traversal and resources remain. Complete
feature/stability, profiling/performance, visual parity and release gates are
not met. Prolonged stability and independent build/review evidence remain
open. The strict no-GL gate remains open.

## Exact next actions

1. Source-map `LLViewerTexLayerSetBuffer`, the live avatar composite/bake
   dynamic-texture owner, including render inputs, publication, readback and
   retries.
2. Implement its actual native Metal feature path before removing its retained
   GL target; add deterministic/physical evidence and queue only its visual
   presentation question.
