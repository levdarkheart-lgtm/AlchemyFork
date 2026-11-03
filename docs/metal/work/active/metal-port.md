# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `8914212806`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,259 findings; strict exits 1. BRDF cutover changes no cross-platform static count. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned reads, RGBA/typed uploads and production native mip generation are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and grayscale/color font-atlas pages are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners, color grading, tone curve and BRDF lookup are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Media, terrain, water, parcel, generated sky faces, legacy sky cube, minimap layers, font atlases, color grading, tone curve and pipeline BRDF LUT own no GL texture on Darwin; SMAA lookup resources are next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native BRDF lookup

Commit `8914212806` removes the redundant Darwin pipeline GL BRDF target,
generator and bind. Native world and PBR-preview paths retain their independent
Metal RG16F tables. Retained Darwin GLSL returns deterministic zero instead of
sampling absent state; non-Darwin keeps the original GL implementation. The
production native preview kernel is physically checked at six edge/interior
texels against an independent 1,024-sample CPU GGX oracle. No viewer launch or
comparison was performed; BRDF appearance is queued for owner-attended review.
[Exact evidence](../../state/archive/CURRENT-native-brdf-lut-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Release build `26.4.0d68151` passed at
  `8914212806ff815379092343c54d0b9c5764379a`; the RelWithDebInfo appearance
  utility also builds at that source.
- The full native Metal binary passes 52,345 assertions across 55 cases with
  API validation enabled. The added 56-assertion physical case proves the exact
  512x512 RG16F descriptor, production kernel and independent reference values.
- The full `alrender_tests` suite passes 557,292 assertions across 137 cases,
  including Darwin/non-Darwin BRDF resource-plan coverage.
- Physical fresh-context renderer validation passes 2,243 assertions across 94
  cases. CPU-only renderer validation passes 1,513 assertions across 203 cases;
  202 pass and one established font-palette case skips.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `9776041d3b9344892d90458fb62c9a8f61ed79515e1ad279006226c5ad94e9bd`.
  Appearance utility:
  `514308e32e9afd6e17c72c5545061749b8bc1d8bb35205ccca471a11623c864a`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory is 2,408 files / 3,259 findings, expected exit 1. Precommit
  and exact reports are byte-identical at
  `ae1a31c0bca1242a4950118e029a09685fd2f79764f58c0f879808376a905862`.
  Both OpenGL and Metal remain linked; the viewer contains no legacy BRDF
  generator names.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—Darwin no longer owns the duplicate pipeline
   GL BRDF target/generator/bind, while native tables retain exact physical and
   deterministic coverage.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict binary/runtime audit;
   measured M1 Max performance; independent review and release proof.
3. **Priority reassessment:** the SMAA lookup maps are the next bounded owner.
   Darwin still creates three GL names while native SMAA already owns and
   consumes private area/search tables with physical oracle coverage.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Cut duplicate Darwin GL ownership of SMAA area/search/sample lookup textures.
Preserve non-Darwin GL behavior and fail-closed retained-presentation semantics,
then prove exact table orientation/content, lifecycle and allocation failure
before pruning any retained SMAA shader or traversal dependency.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
