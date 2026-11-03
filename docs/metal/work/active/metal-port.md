# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `15ed39561c`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,253 findings; strict exits 1. Dead restore methods are deleted without changing the rule set. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; the uncalled restore lifecycle, state and unreferenced owner methods are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct owned reads, RGBA/typed uploads and production native mip generation are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and grayscale/color font-atlas pages are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners, color grading, tone curve, BRDF, post FXAA/SMAA/CAS, DoF and world HDR bloom/cross-filter are native-only; every supported PBR preview mode owns geometry/post/publish. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Material previews remain Metal-only; obsolete restore authority and unreferenced owner methods are deleted. Live GL initialization, traversal, resources and shutdown remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — unreferenced GL restore method deletion

Commit `15ed39561c` deletes 141 lines across 20 files: every source-proven
unreferenced owner `restoreGL`/`restoreGLTexture` method after the restore
lifecycle was removed. Only live bump initialization and two lazy manipulator
grid initializers remain. The prior Metal-only preview cutover is unchanged. No
viewer was launched, driven or compared, and dead API deletion adds no visual
queue question. [Exact evidence](../../state/archive/CURRENT-unreferenced-gl-restore-method-removal-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Xcode Release build `26.4.0d68197` passed at
  `15ed39561c`; the exact Ninja Release test tree configured and linked at 68197.
- The full native Metal binary passes 52,414 assertions across 55 cases with
  API and shader validation enabled. Focused complete PBR material-preview
  coverage passes 459 assertions across two cases.
- The full `alrender_tests` suite passes 557,312 assertions across 137 cases;
  its focused resource-plan matrix passes 3,376 assertions across 37 cases.
- Physical fresh-context renderer validation passes 2,243 assertions across 94
  cases. CPU-only renderer validation passes 1,513 assertions across 203 cases;
  202 pass and one established font-palette case skips.
- The full `newview_tests` binary self-reports build 68197 and passes 98,973
  assertions across 743 cases; one platform CRT case skips as expected. A stale
  build-68158 result was rejected before regeneration and relinking.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `f935252c86ecc1514c91fd4671e570732bff36cc81468ad77fa52d3036cb3435`.
  Source/packaged MSL:
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`.
  Metallib:
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory is 2,408 files / 3,253 findings, expected exit 1. Exact
  report SHA-256:
  `f2ac606640da104c73e267b880a58ff3cbe3d3a9179be890e4f812f2dae459fc`.
  Both OpenGL and Metal remain linked.
- Static symbol inspection finds none of the retained FXAA/SMAA/CAS/sharpen or
  DoF program globals or retained DoF methods in the exact Darwin objects. The
  exact preview object additionally contains no retained preview shader/post,
  bloom/exposure/color-correction, GL batch, transient-target, framebuffer-copy,
  GL-matrix or fallback symbols; the retained implementation is gone from
  source.
- Packaged-viewer inspection finds only the three intentional live restore
  symbols: bump-image initialization and the two manipulator grid initializers.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—20 owner files no longer advertise dead
   context-recovery APIs; only three intentional initialization paths remain.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict binary/runtime audit;
   measured M1 Max performance; independent review and release proof.
3. **Priority reassessment:** delete the empty particle shutdown wrapper, then
   trace each real shutdown owner before live GL teardown removal.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Delete the empty, single-caller `LLVOPartGroup::destroyGL()` wrapper. Then trace
the real sky, manipulator, bump, font, avatar, dynamic-texture, box and
texture-list shutdown releases to their native resource lifetimes.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
