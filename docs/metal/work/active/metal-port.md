# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `cb59cc05a3`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,257 findings; strict exits 1. The net three-finding terrain delta is symbol-only, not new GL calls. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned reads and typed R32F/RG16F full/region uploads are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media ingress/upload and initial placeholder allocation are native-only; owner-attended moving-media parity remains open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Zero direct production framebuffer/texture-image reads remain; media and terrain maps own no GL texture on Darwin, and water resources are next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native terrain maps

Commit `cb59cc05a3` removes Darwin GL allocation and update from
`ALTerrainSurfaceMaps`. `LLImageGL` typed native creation and same-coordinate
subimage upload physically preserve exact R32F and float-to-half RG16F data,
including unchanged texels outside dirty rectangles. Failed refreshes retain
dirty state. No viewer launch or comparison was performed; terrain appearance
is queued for owner-attended review.
[Exact evidence](../../state/archive/CURRENT-native-terrain-maps-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Release build `26.4.0d68133` passed at
  `cb59cc05a368b756b86c1b525ea3a53adbc288b5`; the RelWithDebInfo appearance
  utility also builds at that source.
- The full native Metal binary passes 52,221 assertions across 52 cases with
  API validation enabled; the focused IOSurface import/flip case passes 16.
- Physical renderer validation passes 1,692 assertions across 91 cases with
  API validation enabled, including exact typed full/partial texture data.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `02b826dfacd8a42afe3b579d9a2bd7d1537f4760a1183ceae8a5bd4168150e19`.
  Appearance utility:
  `3cfba52ae7d0fff7b6cb62bab25fcc44057a1b50e05c18cd5a96b143ae451ca5`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory is 2,408 files / 3,257 findings, expected exit 1. Its five
  added and two replaced snippets are typed-native/method-signature heuristic
  matches, not new GL calls. Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—Darwin terrain height/composition maps now own
   Metal storage only and preserve exact typed full/partial upload semantics.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** `ALWaterSurface` is the next bounded owner. Its
   RG32F cell map and RG16F baked displacement maps already feed explicit
   native captures; the latter require native mip generation.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Convert the procedural water cell map to typed native allocation/update, then
extend production typed creation to generate mip chains and move baked wave
height maps to native-only RG16F storage. Physically prove base data and mip
levels. Keep visual parity in the owner-attended daily queue.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
