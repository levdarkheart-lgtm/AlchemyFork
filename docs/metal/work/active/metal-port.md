# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `26801e499f`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,254 findings; strict exits 1. Six added matches are `LLImageGL`/native-binding symbols, not new GL calls. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned framebuffer and texture-image reads are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media ingress/upload and initial placeholder allocation are native-only; owner-attended moving-media parity remains open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Zero direct production framebuffer/texture-image reads remain; media owns no GL placeholder, and terrain surface maps are next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native media placeholder

Commit `26801e499f` removes the Darwin media texture's initial GL allocation.
`LLImageGL`/`LLGLTexture` create populated Metal-only images with no GL name.
`ALTextureSlot` preserves their logical native identity while retained GL
traversal binds the established fallback. Raw/wrapped images and slow/fast binds
are physically regressed; Windows/Linux paths remain unchanged. No viewer
launch or comparison was performed.
[Exact evidence](../../state/archive/CURRENT-native-media-placeholder-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Release build `26.4.0d68131` passed at
  `26801e499ffdf5f895daae4b6659156bfb2e6588`; the RelWithDebInfo appearance
  utility also builds at that source.
- The full native Metal binary passes 52,221 assertions across 52 cases with
  API validation enabled; the focused IOSurface import/flip case passes 16.
- Physical renderer validation passes 1,527 assertions across 90 cases with
  API validation enabled.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `ad62e9b12d9c929e213481015ff24cc5610d60e1c9a4342e81d7f5c42d493fd5`.
  Appearance utility:
  `4c0a61cfff3e1ddbd8f3307fb75da958b9cc89ea83e62e087fc4400297b4e9fb`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory is 2,408 files / 3,254 findings, expected exit 1. Its six
  added matches are `LLImageGL`/native-binding symbols, not new GL calls; two
  existing GL-binding snippets were only equivalently reworded. Both OpenGL and
  Metal remain linked.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—both Darwin media update modes and their initial
   placeholder now own Metal storage only; retained traversal uses the existing
   fallback without duplicating media pixels in GL.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** `ALTerrainSurfaceMaps` is a bounded next owner. It
   already exposes native height/composition textures, but still creates GL
   storage and cannot update a Metal-only image through generic `setSubImage`.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Confirm every live consumer of `ALTerrainSurfaceMaps`, then give its height and
composition textures an explicit native full/region allocation/update contract.
Remove their Darwin duplicate GL authority and physically prove formats, bytes
and partial updates. Keep visual parity in the owner-attended daily queue.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
