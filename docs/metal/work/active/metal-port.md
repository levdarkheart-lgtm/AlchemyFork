# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `aaf3bbcd2a`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,407 files and 3,224 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact HDR/non-HDR world, presentation and ordered-UI Metal publication is default-on, fail-closed and machine-censused by current-frame failure boundary; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Report-screenshot/local-bitmap/HDRI/fetched publication, sculpt metadata, pressure scale-down and eviction work natively at converted sites; authoritative per-texture memory matches actual Metal storage. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Local terrain-paint baking owns full geometry/weight/mip production in Metal. World capture consumes native bumps; native legacy BLEND exists, while owner-attended visual and broad parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Color grading, PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible legacy non-HDR glow/presentation are implemented in Metal. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL (3,224 findings), linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Native report-screenshot, local-bitmap and HDRI publication own no GL storage; prior color/tone/BRDF and dynamic-preview owners were retired after replacement. Continue producer-by-producer. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — Metal-authoritative terrain-paint baking

Commit `aaf3bbcd2a` builds full-resolution terrain geometry into direct Metal
buffers, evaluates the existing alpha-ramp/precision shader, generates mips and
atomically publishes the result. The duplicate GL shader, scratch target and
framebuffer copy are gone; local terrain creation/deletion behavior remains.
Preceding report-screenshot and local-bitmap publication remain native.

Preceding commit `5985911fab` retains full/split HDRI sky through native
RGBA16F/mip publication with descriptor-exact accounting.

### Preceding GL-independent sculpt metadata checkpoint

Commit `c3d0a6c7ed` preserves sculpt geometry, discard, teardown and LOD
behavior without allocating a GPU resource after GL shutdown.

### Preceding GL-disabled pressure checkpoint

Commit `66581835e8` drains already-native downscale work before the global
GL-disabled return. Metal-only fetched textures keep native scale-down and
budget behavior without GL scratch state. The queue policy preserves retained
and unexamined order across a frame-budget timeout.

### Preceding backend-neutral texture eviction checkpoint

Commit `0789086de1` replaces `LLGLTexture`'s GL-name-gated destruction wrapper
with `destroyGPUTexture()`. Native sculpt, fetched-result, ordinary fetched and
brightness/darkness bump eviction release actual Metal storage and accounting;
explicit low-level GL detach remains at its owner.

### Preceding native pressure-scheduler checkpoint

Commit `ebd07151a3` makes shutdown precache, redundant-upload avoidance,
post-create downscale and queued scale-down recognize authoritative Metal.
Darwin drains native entries through `LLImageGL::scaleDown()` without GL
scratch state and preserves the GL/dual-backed fail-closed queue order.

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

- The source-exact warnings-as-errors Xcode Release build
  `26.4.0d68255` passed at renderer source `aaf3bbcd2a`.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `3090d9ea9eff67d705a30826c2f78ef5603b4b6d5537b5a7ba21998f241a8ef9`.
  Source/packaged MSL:
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`.
  Metallib:
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source/packaged settings:
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged Metal-presentation default: `1`.
- Changed targets compile warnings-as-errors. The terrain-bake physical case
  passes 289/1; `almetal_tests` passes 52,526/56 and `newview_tests` passes
  98,990/747 with the established skip. Ninja and Xcode both
  link/package `alchemy-bin`; preceding broad baselines remain green.
- The focused Metal-only upload case passes 17/17 assertions in the shared
  binary. The complete shared diagnostic suite remains non-green at
  8,352/8,357 assertions (182 pass, 4 fail, 39 skip): four legacy/headless
  frame-fence timing assertions and one depth-format expectation fail; the new
  case passes in that same environment.
- `alrender_tests` passes 557,301 assertions/136 cases. The focused census slice
  passes 46 assertions/7 cases.
- Harness structure validation, all 30 harness/tool tests and diff checks pass;
  the documented `audit_gl.py --strict` alias is tested.
- Strict inventory is 2,407 files / 3,220 findings / 2,909 unique records,
  expected exit 1; no rule or suppression changed. Exact report SHA-256:
  `36dd346df62e10ec6d19eeecee4b2e31f56e01b2cc59fd27ae23ac952dad2f9a`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—local terrain paint retains full-resolution
   geometry, alpha-ramp weights, configured precision, mips and UI behavior.
2. **Replacement boundary:** native production is now authoritative; the GL
   bake/copy owner was retired only after physical shader coverage existed.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   feature/stability matrices, visual queue, strict binary/runtime audit,
   measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Trace the default/fetched texture bootstrap and every fallback consumer, then
make the next bounded Darwin producer Metal-authoritative without deleting its
user-visible behavior. Collect the census only in an owner-attended session.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
