# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `f013ba55de`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,241 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | The shared dynamic-texture framework and all known producers are native-only on Darwin; previews, visual hints and avatar composites publish authoritatively. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | World capture consumes native legacy bumps; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are Metal-owned. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Darwin dynamic previews, avatar composites and their shared allocation/scheduling/publication framework are native-only. Continue producer-by-producer. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native-only dynamic-texture framework

Commit `f013ba55de` removes the shared retained-OpenGL allocation, target,
viewport/camera and framebuffer-copy machinery only after every concrete
dynamic-texture producer had a physically validated native implementation.
Those producers cover material, animation, image and model previews;
appearance-editor hints/reset; and avatar composite/bake capture, readback and
publication.

`LLViewerDynamicTexture` now creates native backing on Darwin and requires every
producer to implement the targetless contract. The scheduler preserves preview,
bake and reset ordering while reporting only completed or pending work. Direct
avatar-composite updates use the same native route. Redundant per-producer
backing selectors, targetless flags and retained publication hooks are deleted.

The feature renderers, interaction, retry behavior and authoritative result
publication remain. The 14-file change adds 50 lines and deletes 509; source
and packaged-symbol inspection find no route back to the removed base GL
transaction. The
[exact evidence](../../state/archive/CURRENT-native-only-dynamic-texture-framework-checkpoint-2026-11-03.md)
is separate; existing owner-attended visual rows remain queued.

## Validation checkpoint

- Source-exact warnings-as-errors Xcode Release build `26.4.0d68221` passed at
  `f013ba55de`; the exact Ninja tree configured and linked at 68221.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `a0c68a0a0cc2bffa7e29551b4ca2d6f77627e7801b4a3e0ac34792d1c15ad009`.
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
- `alrender_tests` passes 557,312 assertions/137 cases.
  `newview_tests` self-reports build 68221 and passes 98,973 assertions/743
  cases; one established platform CRT case skips.
- Harness structure validation, all 27 harness/tool tests and diff checks pass.
- Strict inventory is 2,408 files / 3,241 findings / 2,930 unique
  path/kinds/snippet records, expected exit 1. Exact report SHA-256:
  `2e49780865d5599289a1e4fad57500fff5c5ad5e51ef61d90257a4ca26466a9b`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the shared dynamic-texture framework can no
   longer allocate GL backing, acquire retained targets or publish by
   framebuffer copy, while every concrete feature keeps its Metal producer.
2. **Replacement boundary:** shared fallback machinery was deleted only after
   the material, preview, appearance and avatar-composite producers owned their
   equivalent rendering, interaction, retry and publication behavior.
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
