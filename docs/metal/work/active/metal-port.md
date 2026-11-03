# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `e213e9ac01`.

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
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners and color-grading LUT are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Media, terrain, water, parcel, generated sky faces, legacy sky cube, minimap layers, font atlases and color-grading LUT own no GL texture on Darwin; baked tone curve is next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native color-grading LUT

Commit `e213e9ac01` makes the Darwin color-grading LUT a native-only resource.
Backend-neutral loaded state covers both 16-bit `.cube` and 8-bit image-strip
inputs. A shared uploader preserves RGB/RGBA and 8/16-bit unorm semantics,
expands RGB to opaque RGBA and maps volume slices to Metal array layers. The
retained GL post safely disables its absent sampler; non-Darwin retains the GL
3D texture. No viewer launch or comparison was performed; color-LUT appearance
is queued for owner-attended review.
[Exact evidence](../../state/archive/CURRENT-native-color-lut-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Release build `26.4.0d68147` passed at
  `e213e9ac01d24efbdb5b587e818b24b845a456cb`; the RelWithDebInfo appearance
  utility also builds at that source.
- The full native Metal binary passes 52,272 assertions across 53 cases with
  API validation enabled. The added 51-assertion physical case proves exact
  RGB8 expansion, RGBA16 precision/layer order and invalid-input rejection.
- Physical fresh-context renderer validation passes 2,243 assertions across 94
  cases with API validation enabled.
- CPU-only renderer validation passes 1,513 assertions across 203 cases; 202
  pass and one established font-palette case skips.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `72bb699014c3e2b8eaf8dd882ccdd2a41ce5d9deb7cdcb83b6f93aa308704727`.
  Appearance utility:
  `3449cb8fb684d944cfb7f813f6379b6280b2060635ee6715ef452e01a1d951cd`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory is 2,408 files / 3,259 findings, expected exit 1. Precommit
  and exact report hashes are respectively
  `a8e681c2607c41595dc41da1329020ca29514676c8140daed419d3b73c6d9df9`
  and `26f65a0e9a325d87dbf44ecac5b12d12cef4fc0ec3e5a475b1d204538adc0ab6`;
  counts are neutral but source snippets differ. Both OpenGL and Metal remain
  linked.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—Darwin color grading now publishes one native
   Metal LUT authority for both supported source encodings, with exact physical
   precision/layer proof and fail-closed loaded state.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** the baked tone-curve LUT is the next bounded
   owner. Darwin already creates `mMetalToneCurveLut` for native grading but
   still allocates, updates, gates activation on and samples the parallel GL
   `mToneCurveLut`.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Make tone-curve state independent of the GL texture name. On Darwin, allocate
and update only `mMetalToneCurveLut`; retain non-Darwin RGBA16 GL behavior and
make the retained Darwin GL post bypass the unavailable LUT safely. Prove exact
RGBA16 samples, state transitions, identity handling and failure behavior.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
