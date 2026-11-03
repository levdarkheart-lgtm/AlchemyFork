# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `56c78b9134`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,259 findings; strict exits 1. Color-LUT cutover changes no cross-platform static count. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned reads, RGBA/typed uploads and production native mip generation are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and grayscale/color font-atlas pages are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners, color-grading LUT and tone-curve LUT are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Media, terrain, water, parcel, generated sky faces, legacy sky cube, minimap layers, font atlases, color-grading LUT and tone curve own no GL texture on Darwin; pipeline BRDF LUT is next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native tone-curve LUT

Commit `56c78b9134` makes the Darwin baked tone curve a native-only RGBA16
resource with backend-neutral publication state. Identity releases native
storage; failed publication remains unavailable and dirty for retry. A generic
native 2D uploader preserves 8/16-bit RGB/RGBA unorm data and physically proves
the exact production-size 512x1 row. The retained Darwin GL post safely bypasses
the absent texture; non-Darwin retains GL behavior. No viewer launch or
comparison was performed; tone-curve appearance is queued for owner-attended
review. [Exact evidence](../../state/archive/CURRENT-native-tone-curve-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Release build `26.4.0d68149` passed at
  `56c78b9134fafdaad4e60e487aaddf892bd568c8`; the RelWithDebInfo appearance
  utility also builds at that source.
- The full native Metal binary passes 52,289 assertions across 54 cases with
  API validation enabled. The added 17-assertion physical case proves the exact
  512x1 RGBA16 row, descriptor semantics and invalid-input rejection.
- Physical fresh-context renderer validation passes 2,243 assertions across 94
  cases with API validation enabled.
- CPU-only renderer validation passes 1,513 assertions across 203 cases; 202
  pass and one established font-palette case skips.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `67465065ff88a70c05e225d5c0f7c3f79b14b75efb5d0de6ab851aa1853545bf`.
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
  `05c8780b14f7e52d6b49565c03076839279d7522bf96977d5e9770c2edb2f44c`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—Darwin tone-curve grading now publishes one
   native RGBA16 authority, with exact physical sample proof and fail-closed
   identity/publication/retry state.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** the pipeline BRDF LUT is the next bounded owner.
   Darwin still allocates and draws a 512x512 GL RG16F target, while both native
   world rendering and native PBR preview already generate and consume native
   Metal RG16F tables.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Remove the Darwin pipeline GL BRDF target/generator owner. Preserve non-Darwin
GL behavior, keep retained Darwin GL callers safe with no pipeline table and
prove native generation, one-time readiness, exact table semantics and failure
handling through deterministic and physical tests.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
