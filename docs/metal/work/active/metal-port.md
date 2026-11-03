# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `18a8b6924f`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,244 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; visual hints and avatar composites publish authoritatively, including compatible linear storage for sRGB preview output. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | World capture consumes native legacy bumps; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Visual hints, avatar composites and established advanced owners, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are Metal-owned. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Visual hints, avatar composites, legacy bumps, previews and edit grid have native authority; live GL initialization, traversal, other dynamic textures and shutdown remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native visual-parameter hints

Commit `18a8b6924f` moves Darwin appearance-editor visual hints to targetless
Metal production and authoritative native publication. CPU camera and pipeline
state preparation retain system-avatar meshes, cloth, attachments, alpha
ordering, legacy/PBR materials and background without issuing the retained
draw. A prior valid result survives an incomplete update. The old Darwin GL
background, camera/depth/blend, avatar draw and framebuffer-copy producer are
deleted.
[Exact evidence](../../state/archive/CURRENT-native-visual-param-hint-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Xcode Release build `26.4.0d68209` passed at
  `18a8b6924f`; the exact Ninja Release tree configured and linked at 68209.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `70723d54a7afc5ed798d0683a22a430014e5476c797a41a00a11ad38a48ccf04`.
  Source/packaged MSL:
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`.
  Metallib:
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- With Metal API/GPU, shader and load-action validation,
  `almetal_tests` passes 52,427 assertions/55 cases,
  `llappearance_gl_tests` passes 13,207/3, and
  `llrender_gl_fresh_tests` passes 2,612/96.
- `alrender_tests` passes 557,312 assertions/137 cases.
  `newview_tests` self-reports build 68209 and passes 98,973 assertions/743
  cases; one established platform CRT case skips.
- Harness structure validation and all 27 harness/tool tests pass.
- Strict inventory is 2,408 files / 3,244 findings, expected exit 1. Its 2,933
  unique classified tuples are unchanged from build 68207. Exact report
  SHA-256:
  `3a91085405bf2dad63efa126dd0723aba15226fb018b277a6829c9b9b1d55e68`.
  Both OpenGL and Metal remain linked.
- Exact symbol inspection finds targetless visual-hint rendering, native camera
  and avatar-scene preparation, and authoritative Metal publication.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the live appearance-editor hint feature retains
   camera, avatar/clothing/attachment geometry, cloth, alpha/material,
   background, publication and recovery behavior with Metal authority.
2. **Replacement boundary:** Darwin GL production was deleted only after the
   native scene owned equivalent inputs and result publication.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   dynamic textures, feature/stability matrices, visual queue, strict
   binary/runtime audit, measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Replace the Darwin `LLImagePreviewAvatar` and `LLImagePreviewSculpted`
retained-target producers with targetless authoritative Metal output. Preserve
camera/interaction, selected geometry and texture, preview lighting, alpha,
publication and retry semantics, then validate physically before deleting the
old Darwin paths.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
