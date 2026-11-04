# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `b27df920fc`.

This ledger contains current decisions and executable work only. Completed
checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build and reference baseline | doing | Source-exact ARM64 build/package identity passes; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | 2,407 files and 3,214 findings; strict exits 1. No rule or suppression changed. |
| M-003 | Native drawable, presentation and lifecycle | doing | Exact HDR/non-HDR world, presentation and ordered-UI Metal publication is default-on, fail-closed and machine-censused by current-frame failure boundary; dead restore authority/methods and redundant shutdown hooks are deleted. Live GL context creation/shutdown remains. |
| M-004 | Metal resources, transfers, readback and synchronization | doing | Reflection maps use backend-neutral logical slots and the manager owns backend resources; native cube/radiance/SH plus startup/local/HDRI/fetched publication, scale-down and eviction work at converted sites. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production library and established variants load; wide-line/point preview diagnostics now have explicit native pipelines. |
| M-006 | UI, fonts, HUD and media ingress | doing | Darwin media resources and font atlases are native-only; owner-attended text/emoji and moving-media parity remain open. |
| M-007 | World scene and material correctness | doing | Probe simple/material/PBR/bump opaque batches plus tree, terrain, complete ordinary avatar/impostor, HDRI/WindLight sky and water are enumerated directly from CPU scene state with GL-independent transforms/state. Remaining pass families plus broad parity remain open. |
| M-008 | Advanced views/effects and compute | doing | Color grading, PBR BRDF integration, model/animation/image previews, visual hints, avatar composites, material previews, FXAA/SMAA/CAS, DoF, HDR bloom/cross-filter and eligible legacy non-HDR glow/presentation are implemented in Metal. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Full matrix, stability, strict no-GL (3,214 findings), linked/runtime no-GL and visual review remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Reflection maps are backend-neutral and core opaque/bump, tree, terrain, ordinary avatar/impostor including transparent multipass, sky and water have a direct native producer. `LLCubeMapArray` remains pending complete probe-family production, consumption and failure proof. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation is verified; comparison driving is reserved for the owner-attended daily window. |
| M-013 | RLVa strict parity | deferred | Owner-designated non-release nice-to-have; minimum no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded work
item and never implies product completion.

## Active checkpoint — complete direct native probe avatar enumeration

Commit `b27df920fc` adds direct post-deferred skirt, eyelash and hair production
to the deferred avatar/impostor work in `6183613ef0`. High-detail three-pass and
low-detail two-pass culling, depth-write and color-write state are explicit and
GL-independent. Pool selection, active joint LOD, baked/composite textures,
fixed skin palettes and optional cloth remain aligned with retained rendering.

Successful production suppresses only later duplicate native hooks; the full GL
face remains fallback. Missing native inputs, GL-only test textures, unexpected
joints and the separate UI-dummy mode invalidate the whole native face. No
feature was deleted. Indexed masks and remaining pass owners stay open, so the
GL resource gate, `display_cube_face` and cube array remain.

The preceding water checkpoint preserves normal-map blending, phases/lattice,
environment, optics, scaling, exclusions, haze and probe flat-field semantics.

The preceding startup checkpoint remains active: direct native publication and
truthful GL-created state work after GL disable, while initialized active GL
retains fail-closed recovery until its complete replacement is proven.

### Preceding Metal-authoritative terrain-paint checkpoint

Commit `aaf3bbcd2a` builds full-resolution terrain geometry into direct Metal
buffers, evaluates the existing alpha-ramp/precision shader, generates mips and
atomically publishes the result. The duplicate GL shader, scratch target and
framebuffer copy are gone; local terrain creation/deletion behavior remains.
Preceding report-screenshot and local-bitmap publication remain native.

Preceding commit `5985911fab` retains full/split HDRI sky through native
RGBA16F/mip publication with descriptor-exact accounting.

Earlier resource-lifecycle checkpoints keep sculpt metadata, pressure
scheduling, scale-down, eviction and actual-descriptor accounting functional
after GL disable. Their exact commits and physical evidence remain in the
[archive index](../../state/archive/INDEX.md); explicit low-level GL detach
remains only at owners that still need fail-closed recovery.

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
  `26.4.0d68284` passed at renderer source `b27df920fc`.
- `tools/metal/verify_app.sh` reports 349/349 ARM64 Mach-O slices; deep strict
  signing passes.
- Viewer executable SHA-256:
  `ed66dc697f5f0da16503418f7d5e5876c78f721cc3ceb22f0dd6c7c74a35aed5`.
  Source/packaged MSL:
  `563f5db2c7d7427cbd32b0e8403a108e755bca4163a891a248ee107c0c174571`.
  Metallib:
  `e6512d7623e35437d1e3b3dd78ffa726a186e89528d16a70292440b17792848c`.
- Source/packaged settings:
  `355f811a1e9c299c01fbfe0ad22696810410fe4a6eaaeb814f2ae46c0751a4f0`;
  packaged Metal-presentation default: `1`.
- Changed targets compile warnings-as-errors. `llrender_gl_fresh_tests` passes
  3,594/106 and `almetal_tests` passes 52,526/56 with Metal API/GPU validation
  enabled. `newview_tests` self-reports build 68284 and passes 98,990/747 with
  the established skip. Ninja and Xcode both link/package `alchemy-bin`;
  preceding broad baselines remain green.
- The focused Metal-only upload case passes 17/17 assertions in the shared
  binary. The complete shared diagnostic suite remains non-green at
  8,352/8,357 assertions (182 pass, 4 fail, 39 skip): four legacy/headless
  frame-fence timing assertions and one depth-format expectation fail; the new
  case passes in that same environment.
- `alrender_tests` passes 557,301 assertions/136 cases. The focused census slice
  passes 46 assertions/7 cases.
- Harness structure validation, all 30 harness/tool tests and diff checks pass;
  the documented `audit_gl.py --strict` alias is tested.
- Strict inventory is 2,407 files / 3,214 findings / 2,903 unique
  path/rule/snippet records, expected exit 1; no rule or suppression changed.
  Exact report `artifacts/metal/audit-gl-b27df920fc.json` has SHA-256
  `f8e3c3ecd6883dadc934e686e312a5b11e6bb0167406e23e615831ae6a5a2d53`.
  Both OpenGL and Metal remain linked.
- No viewer executable was launched, driven, photographed or compared.

## Owner-attended visual review cadence

Do not autonomously launch either viewer, navigate login/world UI or capture a
comparison. Add a bounded question and prerequisites to the
[daily visual review queue](visual-review-queue.md). At most once per day, the
owner and Codex may work through ready rows together. Existing artifacts may be
summarized, but no row closes without explicit owner-attended evidence.

## Complete-product checkpoint gate

1. **Product advancement:** yes—ordinary avatar/impostor selection, rigid and
   skinned/cloth geometry plus transparent multipass state feed direct Metal;
   fallback remains.
2. **Replacement boundary:** indexed masks and remaining pool/face-owned scene
   families plus cube-array consumption remain until complete capture is proven.
3. **Deferred critical work:** remaining GL context/traversal/resources,
   feature/stability matrices, visual queue, strict binary/runtime audit,
   measured M1 Max performance and release proof.

Checkpoint count, elapsed time, commit volume and test volume are supporting
signals only; none substitutes for movement toward a release criterion.

## Immediate executable action

Paused by owner at this coherent checkpoint for an owner-attended visual backlog
review. Do not autonomously operate either viewer. After explicit resume, add
indexed masks and remaining pass inputs; only complete native face production
permits removing the GL-resource gate or cube-array owner.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
