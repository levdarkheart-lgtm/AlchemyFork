# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `ebd07151a3`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,406 files and 3,217 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact HDR/non-HDR world, presentation and ordered-UI Metal publication is default-on, fail-closed and machine-censused by current-frame failure boundary; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Native-authoritative mip/no-mip scale-down stays entirely in Metal, actual native storage is budgeted once, and fetched-texture pressure now schedules native scale-down without GL scratch state. Established dynamic resources, previews, visual hints and avatar composites publish authoritatively. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | World capture consumes native legacy bumps; opaque fullbright is corrected. Native main-world/impostor legacy BLEND behavior and deterministic coverage exist; owner-attended stained-glass visual and broad material parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Color grading, PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible legacy non-HDR glow/presentation are implemented in Metal. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL, linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | The residual GL color-volume, tone-curve and PBR BRDF lookup owners are deleted after Metal replacement; Darwin dynamic previews/avatar composites and their shared framework are native-only. Continue producer-by-producer. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — native fetched-texture pressure scheduler

Commit `ebd07151a3` adds narrow backend-neutral GPU-residency queries to the
texture lifecycle boundary. Shutdown precache, redundant-upload avoidance,
post-create downscale and `LLViewerLODTexture::scaleDown()` now recognize an
authoritative Metal texture even when its GL name has deliberately retired.

On Darwin, the downscale scheduler first scans its bounded current queue for
Metal-authoritative images. It invokes the established native
`LLImageGL::scaleDown()` route directly, clears the same pending lifecycle flag
and never binds the retained GL scratch target, copy shader, framebuffer,
screen triangle or blend/color state. GL/dual-backed entries are requeued in
their original relative order for the fail-closed legacy path. If the native
scan exhausts its time budget, it returns before unexamined entries could fall
through to GL. No rendering behavior was deleted.

### Preceding native residency-accounting checkpoint

Commit `3b772a630f` makes native Metal create, replacement, scale-down, discard
and teardown update the process-wide pressure budget using actual descriptor
storage. Dual-backed resources remain one logical charge through authoritative
handoff. Physical regressions cover transfer, mip/no-mip scale-down, RGB
expansion, discard and destruction.

### Preceding native scale-down checkpoint

Commit `63f8f0d528` adds a native-authoritative `LLImageGL::scaleDown()`
lifecycle before any GL allocation path. Mip-backed resources select and copy
an existing native mip, no-mip resources use the Metal bilinear resampler, and
both replace the old resource only after successful GPU command submission.
Physical regressions prove pixels, mip tails, discard/memory metadata and the
no-GL-name/no-GL-allocation invariant. The dual-backed path remains only for
textures still participating in complete per-frame GL recovery.

Source audit also corrects the former immediate action: legacy BLEND/stained
glass is not missing. Its native main-world and impostor shaders, production
capture and broad deterministic physical coverage exist; the owner-attended
visual question remains queued.

### Preceding presentation checkpoint

Commit `04edf7b27d` makes the exact native Metal
world/presentation/ordered-UI transaction the default path. Commit
`c2bcd90b04` exposes its actual current-frame outcome to the opt-in evidence
collector: disabled, not attempted, capture started/failed, Metal surface
unavailable, fused surface submission failed or submitted. Requested,
attempted, exact-native and GL-fallback flags are cross-validated rather than
inferred from the setting.

The path first earns one complete native drawable submission, then unlocks
continuous publication. Every frame independently starts transparent and only
a committed current-frame drawable suppresses its GL swap; incomplete frames
retain the complete GL result. The hidden nonpersistent override remains only
for controlled fallback diagnosis during transition. The analyzer reports
per-scenario state counts and exact/fallback rates without making them a
performance gate. The
[exact evidence](../../state/archive/CURRENT-metal-presentation-census-checkpoint-2026-11-03.md)
is separate; no real census is claimed before an owner-attended run collects
the CSV.

## Validation checkpoint

- The source-exact cold warnings-as-errors Xcode Release build
  `26.4.0d68239` passed at `ebd07151a3`.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `f2660391e4bbc98632509ad3bbff106e57b7f8b162cd85e7ae7df229722bcbe1`.
  Source/packaged MSL:
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`.
  Metallib:
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source/packaged settings:
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged Metal-presentation default: `1`.
- The changed `llrender_gl_fresh_tests` target compiles warnings-as-errors. Its
  focused physical Metal slice passes 1,960 assertions/19 cases and the
  complete binary passes 2,671/99. A cold Ninja Release build also links and
  packages `alchemy-bin` warnings-as-errors. Preceding baselines remain
  `almetal_tests` 52,526/56, `llappearance_gl_tests` 13,208/3,
  `alrender_tests` 557,301/136 and `newview_tests` 98,978/744 with the
  established CRT skip.
- The focused Metal-only upload case passes 17/17 assertions in the shared
  binary. The complete shared diagnostic suite remains non-green at
  8,352/8,357 assertions (182 pass, 4 fail, 39 skip): four legacy/headless
  frame-fence timing assertions and one depth-format expectation fail; the new
  case passes in that same environment.
- `alrender_tests` passes 557,301 assertions/136 cases.
  `newview_tests` self-reports build 68235 and passes 98,978 assertions/744
  cases; one established platform CRT case skips. The focused census slice
  passes 46 assertions/7 cases.
- Harness structure validation, all 29 harness/tool tests and diff checks pass.
- Strict inventory is 2,406 files / 3,217 findings / 2,906 unique
  path/kinds/snippet records, expected exit 1. Exact report SHA-256:
  `4b3803cc47b02651b3a6a44f246253bcba20df37f45b53aa7f4847e9fad2b6a7`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—native-authoritative fetched images now retain
   pressure-driven scale-down without reacquiring GL storage or scratch state.
2. **Replacement boundary:** native-only entries use Metal; GL/dual-backed
   entries retain the established fail-closed path until their consumers move.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   feature/stability matrices, visual queue, strict binary/runtime audit,
   measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Introduce backend-neutral destruction only at lifecycle sites whose contract
is to drop all GPU residency. Prove native-only sculpt eviction and teardown
physically; preserve explicit GL-only detach where that is the actual contract.
During the next owner-attended session, collect the default-path publication
census without adding an independent comparison run.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
