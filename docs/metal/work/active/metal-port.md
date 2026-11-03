# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `db43ee336d`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,408 files and 3,251 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact Metal publication exists; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Core paths are physical-GPU covered; direct reads, typed/generated uploads and complete caller-authored mip publication are native-only. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; add only source/evidence-driven variants. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and grayscale/color font-atlas pages are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Opaque fullbright is corrected; legacy BLEND/stained glass and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Established debug/advanced owners, color grading, tone curve, BRDF, post FXAA/SMAA/CAS, DoF and world HDR bloom/cross-filter are native-only; every supported PBR preview mode owns geometry/post/publish. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Material previews and the live edit grid are Metal-only; obsolete restore authority/methods and redundant shutdown hooks are deleted. Live GL initialization, traversal, resources and shutdown remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — Metal edit grid with caller-authored mip publication

Commit `db43ee336d` adds a validated native authored-mip API, publishes the live
edit grid's exact 512-to-1 RGBA8 chain into private Metal storage, and samples
the texture object with trilinear wrapping. Physical readback preserves every
level of a representative complete chain after source release. The joint
subclass's duplicate producer was removed only after proving it was never
sampled. No viewer was launched or compared; the presentation question is in
the daily queue.
[Exact evidence](../../state/archive/CURRENT-manipulator-grid-metal-authored-mip-checkpoint-2026-11-03.md).

## Validation checkpoint

- Source-exact warnings-as-errors Xcode Release build `26.4.0d68202` passed at
  `db43ee336d`; the exact Ninja Release test tree configured and linked at 68202.
- The full native Metal binary passes 52,414 assertions across 55 cases with
  API and shader validation enabled. Focused complete PBR material-preview
  coverage passes 459 assertions across two cases.
- The full `alrender_tests` suite passes 557,312 assertions across 137 cases;
  its focused resource-plan matrix passes 3,376 assertions across 37 cases.
- Physical fresh-context renderer validation passes 2,595 assertions across 95
  cases. CPU-only renderer validation passes 1,513 assertions across 203 cases;
  202 pass and one established font-palette case skips.
- The full `newview_tests` binary self-reports build 68202 and passes 98,973
  assertions across 743 cases; one platform CRT case skips as expected. A stale
  build-68158 result was rejected before regeneration and relinking.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices.
- Viewer executable SHA-256:
  `a886120e86917481f7eff068df4a7e028f24595c53c91454075b169cc893b311`.
  Source/packaged MSL:
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`.
  Metallib:
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- The broad retained-GL suite remains at 8,335/8,340 assertions and 181/224
  cases, with its four established failures and 39 skips.
- Strict inventory is 2,408 files / 3,251 findings, expected exit 1. Exact
  report SHA-256:
  `27b00c663c6a70c570f8ff667e045230dafa5892095d209c8d17824ea3a45299`.
  Both OpenGL and Metal remain linked.
- Static symbol inspection finds none of the retained FXAA/SMAA/CAS/sharpen or
  DoF program globals or retained DoF methods in the exact Darwin objects. The
  exact preview object additionally contains no retained preview shader/post,
  bloom/exposure/color-correction, GL batch, transient-target, framebuffer-copy,
  GL-matrix or fallback symbols; the retained implementation is gone from
  source.
- Packaged-viewer inspection finds only bump-image initialization from the old
  restore set. The grid exposes native `getGridTexture`/`releaseGridTexture`;
  neither manipulator restore symbol nor a joint grid producer remains.
- No viewer executable was launched, driven or compared for this checkpoint.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—the live edit grid retains its exact authored
   mip algorithm while storage/upload/sampling are Metal-native.
2. **Deferred critical work:** remaining GL context/traversal/resources;
   feature/stability matrices; visual queue; strict binary/runtime audit;
   measured M1 Max performance; independent review and release proof.
3. **Priority reassessment:** remove the permanently compiled-out manipulator
   intersection no-op, then port the next live reachable render feature; do not
   treat GL deletion alone as product completion.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Delete the permanently disabled `highlightIntersection()` no-op contract, then
select the next live GL-backed feature by reachability and implement its actual
Metal resource/pass owner with deterministic and physical evidence.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
