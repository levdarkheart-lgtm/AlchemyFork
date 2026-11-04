# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../archive/INDEX.md).
**Renderer checkpoint:** Commit `31fa27392e`.

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
| M-006 | UI, fonts, HUD and media ingress | doing | Font pages retain synchronized GL+Metal storage while incomplete GL world frames own visible recovery UI; owner accepted readable UI text. The Metal Snapshot floater currently has no preview, Refresh effect or Save-to-Disk action. Ordered native capture samples the Metal copy and complete fused Metal world/UI publication remains the cutover boundary. |
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

### Owner-verified transition UI and native atlas ownership

Owner-attended run `font-cache-replay-fix-20261104-01` logged in but was rejected:
menu, location-bar and toolbar text were absent, and the changing FPS field
flashed glyph rectangles. The rejected executable SHA-256 is
`49d5cf47f17e9e740dab38aaee48ef76ec5dd540afb7b9bc83f26522b62695e6`;
its owner-supplied rejection image is retained in the run directory.

The follow-up run `font-metal-overlay-20261104-01` is also rejected. It encoded
509,986 production Metal UI batches with zero eligible fallback batches but no
viewer UI appeared over the GL world. This isolates the failure to transparent
child-layer composition above SDL's OpenGL drawable rather than Metal glyph or
batch coverage. Its retained owner image SHA-256 is
`423d3a34cb43ab9b1f264f04d176b96fa738a6cb5d46299e9b260650b7009c30`.

Commit `31fa27392e` keeps complete GL UI visible while an incomplete GL world
frame owns presentation, with the same font page synchronized into Metal for
ordered native capture. Direct transparent replacement is disabled after world
startup. A complete fused Metal world/UI frame remains the only boundary that
may make the native surface opaque and suppress GL swap. No UI feature was
deleted; the transition copy is removed only with complete native authority.

The rebuilt `26.4.0.68285` Release viewer is pinned at
`c9bf95b8a8d6c9f36c437c16b913feb5d3f1e9bcd77e9b5c69965992af2527a8`;
349/349 inspected Mach-O slices are ARM64 and deep strict code-sign verification
passes. Metal-validation runs pass 3,684/110 for
`llrender_gl_fresh_tests`, 36/2 for the dual-atlas cases and 56/1 for the
real-FreeType native UI path. Owner-attended run
`font-dual-recovery-20261104-01` accepted readable menu, location, toolbar and
stable numeric FPS text. Broader font-atlas validation remains queued.

Commit `b27df920fc` completes direct ordinary/control-avatar probe production,
including transparent multipass state, while full GL remains fail-closed
fallback. Indexed masks and remaining pass owners still block removal of the
GL resource gate, `display_cube_face` and cube array.

Earlier terrain, water, startup/local/fetched resources, pressure, eviction,
accounting and scale-down checkpoints remain active. Native-authoritative
resources avoid GL names; dual backing remains only where complete per-frame GL
recovery is still required. Commits `04edf7b27d` and `c2bcd90b04` keep exact
ordered world/UI Metal presentation default-on, measurable and fail closed:
only a committed current-frame fused drawable suppresses its GL swap. Exact
prior evidence remains in the [archive index](../../state/archive/INDEX.md).

## Validation checkpoint

- Release `26.4.0.68285` builds and packages warnings-as-errors. Viewer SHA-256
  is `c9bf95b8a8d6c9f36c437c16b913feb5d3f1e9bcd77e9b5c69965992af2527a8`.
- `verify_app.sh`: 349/349 ARM64 Mach-O slices; deep strict signing passes.
- Metal-validation: fresh renderer 3,684/110, dual atlas 36/2, real glyph/native
  UI 56/1. Shared baseline retains 5 known Apple GL failures/4 cases.
- Strict inventory remains 3,214 findings and exits 1; both APIs remain linked.

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

Font/UI recovery is owner-accepted. The clean Midday/Lightbox pair forced and
verified identical exposure/graphics controls and final position. Metal image
`4aa3fb673ca3c0e650288cf36daf41d78e7fa0d3c08aaf94b175e097c501fc9e`
remains washed out with clipped highlights and a black mirror; reference image
`9330a018b41a00cf55b9056c7c6a8716129db3d34cf6f30765ccea533b6d79e1`
has retained tonal range and a live reflection. The owner also found Snapshot
preview, Refresh and Save to Disk nonfunctional in Metal. Pause; after explicit
resume repair these three paths before continuing indexed pass removal.

## Non-negotiable open gates

- Do not globally suppress GL traversal before each retained side effect has a
  native owner or an explicit removal proof.
- Do not mark feature parity from build/test success alone. An unreviewed queue
  row is not evidence.
- Do not claim performance improvement without paired, provenance-complete,
  quality-equivalent M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Do not weaken fail-closed native authority, snapshot, appearance-readback or
  presentation gates.
