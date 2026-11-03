# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `4a005b8ef9`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,259 findings; strict exits 1. Legacy sky-cube cutover changes no cross-platform static count. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned reads, RGBA/typed uploads and production native mip generation are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and grayscale/color font-atlas pages are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Media, terrain, water, parcel, generated sky faces, legacy sky cube, minimap layers and font atlases own no GL texture on Darwin; color-grading LUT is next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native legacy sky cube

Commit `4a005b8ef9` makes the Darwin legacy sky `LLCubeMap` a native-only
resource. CPU face preparation remains independent of GPU allocation; the
historical transforms publish directly to one linear RGBA8 Metal cube, while
`initGL()` cannot recreate duplicate ownership. Missing-device publication is
fail-closed and retryable, and retained GL traversal clears stale cube state.
Non-Darwin GL behavior is retained for later platform pruning. No viewer launch
or comparison was performed; legacy sky-cube appearance is queued for
owner-attended review.
[Exact evidence](../../state/archive/CURRENT-native-legacy-sky-cube-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Release build `26.4.0d68145` passed at
  `4a005b8ef950f1872c0ad9a86cfdc575dff2030c`; the RelWithDebInfo appearance
  utility also builds at that source.
- The full native Metal binary passes 52,221 assertions across 52 cases with
  API validation enabled.
- Physical fresh-context renderer validation passes 2,243 assertions across 94
  cases with API validation enabled, including all-six-slice legacy-cube
  descriptor, transform/mapping, retry and exact-byte readback.
- CPU-only renderer validation passes 1,513 assertions across 203 cases; 202
  pass and one established font-palette case skips.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `9277b87848d3723b04baa3c90dd123a23cd6faca1c991122dfb4bd35bc0870c6`.
  Appearance utility:
  `bf70df75140711cd0d034d5562d3ec567d9814b252f617d1202e4f3833e0cf10`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory is 2,408 files / 3,259 findings, expected exit 1. The legacy
  sky-cube precommit/exact reports are byte-identical at
  `a8e681c2607c41595dc41da1329020ca29514676c8140daed419d3b73c6d9df9`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the Darwin legacy sky cube now owns only Metal
   storage while retaining exact historical face transforms and retrying failed
   publication without exposing a stale generation.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** the color-grading LUT is the next bounded owner.
   `mCGLut` is the only live production `ALTexture3D` user; Darwin already
   publishes an equivalent native 2D-array resource consumed by Metal, but
   still allocates and uploads the parallel GL 3D texture.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Make color-LUT loaded state independent of the GL object. On Darwin, publish
only `mMetalCGLut` for both 16-bit `.cube` files and 8-bit image strips; retain
the non-Darwin `ALTexture3D` path. Prove exact native format, slice ordering,
component expansion, precision and failed-publication behavior physically.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
