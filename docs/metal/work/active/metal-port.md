# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `53de84c79c`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,337 potential findings; strict exits 1. SSR cutover removed 14 owned calls. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; remove remaining GL/PBO ingress owner by owner. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native world/UI and CEF IOSurface paths exist; moving media parity is queued for owner review. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | GL/PBO SSR bootstrap is removed; next inspect avatar-impostor bake ingress. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is now reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native SSR cutover

Commit `53de84c79c` deletes the paired GL framebuffer/PBO SSR readback path,
including queueing, CPU buffers, private texture re-upload and fallback
generation bookkeeping. Metal's completed final-color target is now the sole
producer for Metal SSR consumers.

At startup or resize, no prior native history exists. The first complete Metal
frame omits SSR feedback, publishes the paired native history and is prohibited
from UI composition, snapshot readback or drawable publication. A later frame
may consume only a complete color/depth generation. Failed publication retains
the prior native pair or repeats the offscreen warm-up. Texture validation uses
the current Metal capture dimensions plus RGBA8/R11G11B10F and R32F formats; it
does not consult GL target dimensions.

This cutover preserves the retained GL renderer for still-unported owners and
does not suppress global traversal. The strict scan fell from 3,351 to 3,337,
but the executable still links Metal and OpenGL and the release gate is open.
[Exact evidence](../../state/archive/CURRENT-native-ssr-cutover-checkpoint-2026-11-02.md).

## Validation checkpoint

- Source-exact RelWithDebInfo build `26.4.0d68103` at `53de84c79c` compiles,
  links, stages and signs with warnings as errors.
- Physical Metal validation passes 52,190 assertions across 52 cases with API
  and load-action checks enabled.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Executable SHA-256:
  `12fcfbe8780f4bae179f3d1e1f77f99fb50b405d7ff4331542b9eefa43f909c8`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The 574-entry broad CTest inventory completed with 568 passes, six GL-host
  failures and five skips in 1,268.61 seconds. Focused reruns reproduce three
  unique failures in untouched GL test code: frame-fence pacing, depth format,
  and DoF clip-control availability. This is recorded as a failed broad gate,
  not hidden or promoted to green.
- Harness validation and its unit suite pass with the daily review queue in the
  mandatory live-document inventory.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through the ready rows together. Existing artifacts may
be summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—one live GL-to-Metal data dependency and 14
   owned GL calls are removed, with a non-presentable native warm-up boundary.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict binary/runtime audit;
   measured M1 Max performance; independent review and release proof.
3. **Priority reassessment:** continue M-011 on the next synchronous GL/PBO
   ingress. Do not return to one-off viewer comparison driving.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Inspect the avatar-impostor bake attachment/PBO transfer and its existing native
bake owner. Remove the next source-proven duplicate producer or data crossing,
then rebuild, rerun the physical suite and refresh the strict count.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot or presentation gates.
