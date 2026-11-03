# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `38ff186e4f`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,242 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | All known Darwin dynamic textures now schedule targetlessly; model/animation/image previews, visual hints and avatar composites publish authoritatively. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | World capture consumes native legacy bumps; opaque fullbright is corrected. Legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF and HDR bloom/cross-filter are Metal-owned. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Darwin dynamic preview producers are native-authoritative; retained model-preview source and animation-preview fallback routing are deleted. Continue producer-by-producer. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — dynamic-preview source retirement

Commit `b3f6124664` moves Darwin `LLModelPreview` to targetless Metal production
and authoritative publication. It preserves static textured/material-colored
faces, optional edges, physics meshes, convex hulls and explode transforms;
rigged/skinned faces; joint overrides, collision volumes, bones, joint bounds
and ground plane; and the red wide-line/point degenerate diagnostics.

CPU camera state replaces the retained matrix transaction. Rigged previews
continue continuously while static/physics previews remain demand-driven.
Complete output becomes sole authoritative backing; an incomplete attempt
preserves the prior image and retries. A scoped native upload mode fills exact
Metal vertex/index stores without creating GL names. The shared lit-preview
renderer adds clear-only transactions, always-write depth and screen-pixel wide
lines/points for the preserved diagnostics.

The Darwin GL producer and framebuffer-copy publication are bypassed only after
the native implementation owns these behaviors. Source census confirms that
all known `LLViewerDynamicTexture` subclasses now have targetless Darwin paths.
[Exact evidence](../../state/archive/CURRENT-native-model-preview-checkpoint-2026-11-03.md).

Commit `1830ac9d0b` then deletes the retained/non-Darwin model-preview draw and
its orphaned GL-only helpers. The cleanup follows, and does not substitute for,
the validated feature replacement. Its
[exact evidence](../../state/archive/CURRENT-retained-model-preview-source-removal-checkpoint-2026-11-03.md)
is separate.

Commit `38ff186e4f` removes the remaining retained-target routing from
`LLPreviewAnimation` only after commit `218b60d8b6` established and physically
validated complete Metal production. Native backing and targetless scheduling
are now unconditional, retained fallback cannot be selected and base
framebuffer-copy publication cannot run. Playback, camera, skinning, lighting,
alpha and cloth remain in the Metal renderer. The
[exact evidence](../../state/archive/CURRENT-retained-animation-preview-routing-removal-checkpoint-2026-11-03.md)
is separate; visual row 25 remains queued.

## Validation checkpoint

- Source-exact warnings-as-errors Xcode Release build `26.4.0d68219` passed at
  `38ff186e4f`; the exact Ninja tree configured and linked at 68219.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `242a84184a705d78adb73533c0342e31c92ec978feb1efade4b406945903e8c9`.
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
  `newview_tests` self-reports build 68219 and passes 98,973 assertions/743
  cases; one established platform CRT case skips.
- Harness structure validation, all 27 harness/tool tests and diff checks pass.
- Strict inventory is 2,408 files / 3,242 findings / 2,931 unique
  path/kinds/snippet records, expected exit 1. Exact report SHA-256:
  `8a2a55adadfa99fa32b007a2d154c463a56766d393e9e82cda450e25194fc9e3`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the animation preview can no longer allocate a
   retained target or restore framebuffer-copy publication, while its complete
   Metal behavior remains intact.
2. **Replacement boundary:** fallback routing was deleted only after the native
   producer owned equivalent playback, geometry, cloth, shading, interaction
   and result publication.
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
