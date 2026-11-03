# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `dc368e4b80`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,254 findings; strict exits 1. The dead restore lifecycle removed one finding after the preview renderer's seven, without changing the rule set. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; the uncalled same-process context-restore lifecycle and permanent-false restore state are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned reads, RGBA/typed uploads and production native mip generation are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and grayscale/color font-atlas pages are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners, color grading, tone curve, BRDF, post FXAA/SMAA/CAS, DoF and world HDR bloom/cross-filter are native-only; every supported PBR preview mode owns geometry/post/publish. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Material previews remain Metal-only; obsolete viewer/pipeline restore authority is deleted. Live GL initialization, traversal, resources and shutdown remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — dead GL restore lifecycle deletion

Commits `69d7a19f9a` and `dc368e4b80` delete the uncalled viewer/pipeline GL
context-restore path, its unreachable shutdown branch and the permanent-false
`gRestoreGL` progress/render state. Pipeline shutdown now explicitly owns
`releaseAllRenderResources()`; no same-process context rebuild contract remains.
The prior Metal-only preview cutover is unchanged. No viewer was launched,
driven or compared, and this lifecycle-only checkpoint adds no visual queue
question. [Exact evidence](../../state/archive/CURRENT-dead-gl-restore-lifecycle-removal-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Xcode Release build `26.4.0d68195` passed at
  `dc368e4b80`; the exact Ninja Release test tree configured and linked at 68195.
- The full native Metal binary passes 52,414 assertions across 55 cases with
  API and shader validation enabled. Focused complete PBR material-preview
  coverage passes 459 assertions across two cases.
- The full `alrender_tests` suite passes 557,312 assertions across 137 cases;
  its focused resource-plan matrix passes 3,376 assertions across 37 cases.
- Physical fresh-context renderer validation passes 2,243 assertions across 94
  cases. CPU-only renderer validation passes 1,513 assertions across 203 cases;
  202 pass and one established font-palette case skips.
- The full `newview_tests` binary self-reports build 68195 and passes 98,973
  assertions across 743 cases; one platform CRT case skips as expected. A stale
  build-68158 result was rejected before regeneration and relinking.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `54fa795049b81c80007ad2736e851fdc355a67c6d8b3c3273b28d9861d21ba20`.
  Source/packaged MSL:
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`.
  Metallib:
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory is 2,408 files / 3,254 findings, expected exit 1. Exact
  report SHA-256:
  `0cd0dae0e3c15ee36848b3c27695c2699e1f88d2a0a508324d41767de197a8f3`.
  Both OpenGL and Metal remain linked.
- Static symbol inspection finds none of the retained FXAA/SMAA/CAS/sharpen or
  DoF program globals or retained DoF methods in the exact Darwin objects. The
  exact preview object additionally contains no retained preview shader/post,
  bloom/exposure/color-correction, GL batch, transient-target, framebuffer-copy,
  GL-matrix or fallback symbols; the retained implementation is gone from
  source.
- Packaged-viewer inspection finds no `gRestoreGL`, viewer/pipeline `restoreGL`,
  pipeline `destroyGL` or `releaseAllGLResources` symbol. The exact binary
  retains `LLPipeline::releaseAllRenderResources()` as the shutdown owner.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the uncalled context-recovery authority,
   unreachable branch and dead render-suppression state are deleted, leaving an
   explicit process-shutdown resource lifetime.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict binary/runtime audit;
   measured M1 Max performance; independent review and release proof.
3. **Priority reassessment:** delete the source-proven unreferenced restore-only
   owner methods while preserving live bump-map/manipulator initialization,
   then return to live traversal/resource cutover.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Delete the unreferenced sky/spatial/dynamic-texture/avatar/particle restore-only
method cluster with source-exact call-graph proof. Preserve the live bump-map
and manipulator initialization paths, then rebuild and audit before touching
live GL teardown.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
