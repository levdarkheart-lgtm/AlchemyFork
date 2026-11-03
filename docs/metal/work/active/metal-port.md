# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `7b7d772256`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,406 files and 3,224 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | The color-grade volume and tone-curve LUTs and shared dynamic-texture framework are Metal-native on Darwin; previews, visual hints and avatar composites publish authoritatively. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | World capture consumes native legacy bumps; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Color grading including tone curves, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are Metal-owned. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | The residual GL color-volume and tone-curve owners are deleted after Metal replacement; Darwin dynamic previews/avatar composites and their shared framework are native-only. Continue producer-by-producer. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native tone-curve ownership

Commit `7b7d772256` removes the residual GL-only tone-curve owner only after the
supported Darwin product already had validated Metal RGBA16 LUT publication and
native display-post sampling. The curve editor/model, per-channel controls,
settings, CPU bake, identity/bypass and failure behavior remain. `LLPipeline`
still publishes the baked 512x1 curve as `mMetalToneCurveLut`; native display
post still configures and samples it.

The deleted boundary is the duplicate GL resource plan, raw texture name,
allocation/upload/binding route, GLSL sampler/function and reserved uniforms.
The 11-file change adds 35 lines and deletes 163. Deterministic Metal coverage
checks production-sized upload/readback, exact per-channel sampling and invalid
configuration rejection. The
[exact evidence](../../state/archive/CURRENT-native-tone-curve-gl-owner-removal-checkpoint-2026-11-03.md)
is separate; the owner-attended tone-curve visual row remains queued.

## Validation checkpoint

- The source-exact warnings-as-errors Xcode Release build `26.4.0d68225`
  passed at `7b7d772256`; the exact Ninja tree configured and linked at 68225.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `67858ab25cf0d283e125689ecd0cb72658b55152b0ada32b1318f3424fea3ac9`.
  Source/packaged MSL:
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`.
  Metallib:
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- With Metal API/GPU, shader and load-action validation,
  `almetal_tests` passes 52,468 assertions/55 cases,
  `llappearance_gl_tests` passes 13,207/3, and
  `llrender_gl_fresh_tests` passes 2,612/96.
- The focused Metal-only upload case passes 17/17 assertions in the shared
  binary. The complete shared diagnostic suite remains non-green at
  8,352/8,357 assertions (182 pass, 4 fail, 39 skip): four legacy/headless
  frame-fence timing assertions and one depth-format expectation fail; the new
  case passes in that same environment.
- `alrender_tests` passes 557,308 assertions/137 cases.
  `newview_tests` self-reports build 68225 and passes 98,973 assertions/743
  cases; one established platform CRT case skips.
- Harness structure validation, all 27 harness/tool tests and diff checks pass.
- Strict inventory is 2,406 files / 3,224 findings / 2,913 unique
  path/kinds/snippet records, expected exit 1. Exact report SHA-256:
  `5380797ccdd22f938b2969bb6b5d6c74dcfa49c78373e2a472b26f820e81fad4`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—tone curves keep their editor/model, controls,
   CPU bake, Metal RGBA16 upload and native display-post sampling while the
   superseded GL owner is gone.
2. **Replacement boundary:** the GL texture publication and GLSL sampling path
   were deleted only after native upload, channel sampling and invalid-state
   behavior had deterministic Metal coverage.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   feature/stability matrices, visual queue, strict binary/runtime audit,
   measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Census the next live application-owned Darwin GL producer and its user-visible
responsibilities. Implement and validate the complete Metal behavior before
retiring that producer; do not trade functionality for a lower GL count.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
