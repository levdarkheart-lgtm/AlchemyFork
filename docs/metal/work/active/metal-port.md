# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `ec69e977e7`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,233 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; generated bump render textures can become native sole authority with validated metadata and readback. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | World capture consumes native legacy bumps; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established advanced owners, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are native-only. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Legacy bumps, previews and edit grid are Metal-only; live GL initialization, traversal, other dynamic textures and shutdown remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native legacy bump maps

Commit `ec69e977e7` preserves standard and brightness/darkness legacy bump
behavior while moving Darwin storage, GPU generation, mip generation,
publication, caching and world consumption to Metal. The former live GLSL
producer and GL render target are deleted only after native replacement.
Physical readback proves authoritative publication retires predecessor GL
backing. Commit `0fafef5f97` separately removes an intersection method whose
entire shipping body was already disabled; the live edit grid remains.
[Exact evidence](../../state/archive/CURRENT-native-legacy-bump-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Xcode Release build `26.4.0d68205` passed at
  `ec69e977e7`; the exact Ninja Release tree configured and linked at 68205.
- Two stale deleted shader copies found in the incremental package were removed
  from the exact regenerable Release output. A second wrapper build re-signed
  successfully, package search finds none and deep strict signing passes.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `86fb1fc0426edd0dc60f112ee424e883d23c15ee2ddbed62568140f9935e4645`.
  Source/packaged MSL:
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`.
  Metallib:
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- With Metal API/GPU, shader and load-action validation,
  `almetal_tests` passes 52,414 assertions across 55 cases and
  `llrender_gl_fresh_tests` passes 2,612 across 96.
- `alrender_tests` passes 557,312 assertions across 137 cases.
- `newview_tests` self-reports build 68205 and passes 98,973 assertions across
  743 cases; one established platform CRT case skips.
- Strict inventory is 2,408 files / 3,233 findings, expected exit 1. Exact
  report SHA-256:
  `5599cf095b7d7d2a6b92c1455327dae18648e57196753703f3ca3c9a799ca33e`.
  Both OpenGL and Metal remain linked.
- Exact symbol inspection finds the Metal normal generator and authoritative
  publication, with no bump `restoreGL`, retained bump target or normal-map
  GLSL program. Deleted shader assets are absent from the signed package.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the live legacy bump feature now has native
   producers, mip lifecycle, sole backing, cache and world consumption.
2. **Dead-code boundary:** only the already-disabled intersection contract was
   removed without replacement. Live behavior is not traded for audit count.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   dynamic textures, feature/stability matrices, visual queue, strict
   binary/runtime audit, measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Source-map `LLViewerTexLayerSetBuffer`, the live avatar composite/bake
dynamic-texture owner, then implement its Metal render/publication/readback and
retry behavior before deleting its retained GL target.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
