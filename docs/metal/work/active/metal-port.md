# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `e4d5c187a9`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,244 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; avatar composites support authoritative publication and mid-capture readback/resume. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | World capture consumes native legacy bumps; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Avatar composites and established advanced owners, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are Metal-owned. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Avatar composites, legacy bumps, previews and edit grid have native authority; live GL initialization, traversal, other dynamic textures and shutdown remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native avatar composite/bake

Commit `e4d5c187a9` moves Darwin avatar layer composites to targetless Metal
capture and authoritative native publication. The retained feature includes
layer geometry/textures/blending, minimum-alpha behavior, static and processed
alpha textures, live composite readback for morph masks, texture publication
and failure retry. The old GL side effects are suppressed only inside this
native capture boundary; a prior valid result survives a failed update.
[Exact evidence](../../state/archive/CURRENT-native-avatar-composite-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Xcode Release build `26.4.0d68207` passed at
  `e4d5c187a9`; the exact Ninja Release tree configured and linked at 68207.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `b66c59b1da4aff162c16cef62ca4b7d483f5ef5b3c1f4808da3827b6003f3357`.
  Source/packaged MSL:
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`.
  Metallib:
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- With Metal API/GPU, shader and load-action validation,
  `almetal_tests` passes 52,414 assertions/55 cases,
  `llappearance_gl_tests` passes 13,207/3, and
  `llrender_gl_fresh_tests` passes 2,612/96.
- `alrender_tests` passes 557,312 assertions/137 cases.
  `newview_tests` self-reports build 68207 and passes 98,973 assertions/743
  cases; one established platform CRT case skips.
- Harness structure validation and all 27 harness/tool tests pass.
- Strict inventory is 2,408 files / 3,244 findings, expected exit 1. Its +11
  net delta is capture resource/state vocabulary, not new GL drawing. Exact
  report SHA-256:
  `52bf624a5d7bb582a4705bc35a2d354a00cdf8c55b8265a4f05bd81e04956b27`.
  Both OpenGL and Metal remain linked.
- Exact symbol inspection finds targetless composite rendering,
  authoritative Metal publication and mid-capture Metal readback.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the live avatar composite/bake feature retains
   rendering, alpha/morph processing, publication and recovery with Metal
   authority.
2. **Replacement boundary:** Darwin GL effects are suppressed only after the
   native capture owns equivalent inputs and result publication.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   dynamic textures, feature/stability matrices, visual queue, strict
   binary/runtime audit, measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Replace the Darwin `LLVisualParamHint`/`LLVisualParamReset` retained-target and
GL impostor producer with targetless state preparation plus an authoritative
Metal lit-preview result. Preserve camera, body/clothing/attachment,
alpha/material, publication and retry semantics, then validate physically
before deleting the old Darwin path.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
