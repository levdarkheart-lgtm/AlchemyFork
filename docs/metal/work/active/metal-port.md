# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `7d3af86e6c`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,249 findings; strict exits 1. Reflection manager and `pipeline.cpp` have zero direct `gl_function` findings. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; SSR warm-up is explicitly barred from presentation. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned framebuffer and texture-image reads are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Native world/UI and CEF IOSurface paths exist; moving media parity is queued for owner review. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners are native; visual-only validation is batched in the daily queue. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Zero direct production framebuffer or texture-image readback calls remain; live duplicate GL resource/traversal owners are next. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — texture-image readback elimination

Commit `7d3af86e6c` removes every direct production `glGetTexImage` and
`glGetCompressedTexImage` call. `LLImageGL::scaleDown` now performs its retained
GL exact-mip copy GPU-to-GPU while preserving the existing authoritative native
mip-copy/no-mip-resample paths. Its scratch pack/unpack PBO is gone.

The removed cube-array copy constructor was unreachable; the live resize path
already recreates storage and invalidates every probe. Texture-size diagnostics
now use immutable allocation metadata. Two debug internal-format queries remain
outside `LLImageGL`. No viewer launch or comparison was performed.
[Exact evidence](../../state/archive/CURRENT-texture-image-readback-elimination-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors RelWithDebInfo build `26.4.0d68125` passed at
  `7d3af86e6c88b6b4003b2c0ba52f30ccbe0e9ef3`.
- Physical renderer validation passes 1,216 assertions across 85 cases with API
  validation enabled, including exact source-mip and native scale-down paths.
- The full `newview_tests` binary passes 98,973 assertions across 743 cases;
  one platform CRT case skips as expected.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `3226822da171f02d69b496bd4e628012635053fcb0bb1471666fb0963eb481fd`.
  Source/packaged MSL:
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
  Metallib:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,249 findings, expected exit 1, down 16
  from the prior exact checkpoint. Both OpenGL and Metal remain linked.
- Source-exact broad `llrender_gl_shared_tests` retains the documented
  baseline of four frame-fence/depth-format failures and 39 skips; it is not
  promoted to green.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—direct production framebuffer and texture-image
   readbacks no longer use GL, and scale-down no longer round-trips via CPU/PBO.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict
   binary/runtime audit; measured M1 Max performance; independent review and
   release proof.
3. **Priority reassessment:** accelerated Darwin CEF ingress still imports and
   copies each IOSurface through GL before separately updating the existing
   Metal mirror. This is the next source-proven duplicate resource owner.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Cut over the Darwin accelerated CEF media path to its existing native IOSurface
import and Metal resample alone. Publish the playing frame only after the Metal
copy succeeds, preserve Windows/Linux behavior and keep moving-media visual
parity in the owner-attended daily queue.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
