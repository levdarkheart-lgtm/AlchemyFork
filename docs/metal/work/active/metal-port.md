# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `a9a0edd4e2`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,266 findings; strict exits 1. Reflection manager and `pipeline.cpp` have zero direct `gl_function` findings. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; recurring pipeline and debug-color readbacks are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native world/UI and CEF IOSurface paths exist; moving media parity is queued for owner review. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | The only remaining production `gGL.readPixels` calls are three snapshot fallbacks in `llviewerwindow.cpp`. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native ordered debug-color readback

Commit `a9a0edd4e2` replaces `DebugShowColor`'s final GL/PBO recurring pixel
sample with an exact native ordered snapshot. The active offscreen UI encoder
is split after ordinary HUD/interface drawing and before debug text, one pixel
is copied into a private texture, and the same retained pass resumes with
load/store actions. A three-slot `ALMetalTextureReadbackRing` queues after the
source transaction on the same device queue and collects without a steady-state
wait. Missing or incomplete native production publishes no new sample.

The diagnostic requests a fresh exact world/UI transaction without consuming
the bounded presentation probe. `ALGPUReadback`, its `glReadPixels`/pixel-pack
implementation and its two GL-only tests are now deleted. Strict inventory fell
from 3,269 to 3,266.
[Exact evidence](../../state/archive/CURRENT-debug-color-readback-cutover-checkpoint-2026-11-02.md).

## Validation checkpoint

- Source-exact warnings-as-errors RelWithDebInfo build `26.4.0d68114` passed at
  `a9a0edd4e225681e49ae5b06f47284af31b70594` after the full invalidated
  dependency build completed all 1,392 actions.
- Physical Metal validation passes 52,209 assertions across 52 cases with API
  and load-action validation enabled.
- Focused surviving `algpubuffer` GL coverage passes 108 assertions across nine
  cases.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Executable SHA-256:
  `fcb6a3669e822170e408ac05d8343175dc51f121ce68ce5403f3c6b62f9f733b`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,266 findings, expected exit 1. The linked
  executable still contains both OpenGL and Metal.
- The prior 574-entry broad CTest inventory remains 568 passes, six GL-host
  failures and five skips. It is recorded as failed, not promoted to green.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the last production recurring GL readback is
   native and exact, and its dead GL abstraction is removed.
2. **Deferred critical work:** remaining GL context/traversal/resources and
   snapshot fallback reads; feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** trace the three remaining production
   `gGL.readPixels` calls in `rawSnapshot`/`simpleSnapshot`. Preserve every
   supported snapshot contract while extending the existing native owners; do
   not substitute one-off viewer driving.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Build a source-derived contract matrix for `rawSnapshot` and `simpleSnapshot`:
color/depth/depth24, RGB/RGBA, cutout, UI/HUD, tiled high resolution,
single-pass and 360 faces. Map each to the existing native snapshot producer,
implement the uncovered variants, and remove GL reads only once the affected
feature remains functional and exact.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot or presentation gates.
