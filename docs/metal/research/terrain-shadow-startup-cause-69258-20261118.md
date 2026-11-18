# PPD-241: terrain-shadow startup failures in 69258

The investigation finds a source-confirmed missing preparation step: the direct
native terrain-shadow producer reads the height texture without first refreshing
the terrain surface maps. The original shadow producer and the native reflection
producer both perform that refresh. This is a concrete mechanism for startup
rejections and stale shadow input, but the old compound-guard records cannot
prove it caused every one of the 42/81/15 rejected views.

## Scope and ownership

Initial source/runtime-evidence investigation on isolated branch
`codex/terrain-shadow-cause-20261118`, worktree
`/private/tmp/alchemy-ppd125-startup-cause-20261118`, based on `1438181de6`.
The exact recorded builder is `afc9278d23d7fa5e800e322a7e1fa913e21d1070`,
production `cfc20af4ba19c7df9d1cf406cc07a5eba4c7eda6`, build 69258.
The previous PPD-125 mirror readiness fix is separate and remains available on
its original branch. The initial diagnosis made no renderer changes or runtime
actions. The owner subsequently authorized the local repair below and requested
notification to the performance session for integration. No viewer launch,
settings change, GPU replay or performance measurement occurred. The concurrent
engine worktree is not modified by this work.

## What actually failed

`captureMetalShadowScene()` counts one CPU-failed view when the capture becomes
incomplete. The terrain pool calls `captureMetalSpotShadowTerrainBatch()` for
its patches. At exact-builder `pipeline.cpp:14851–14893`, a compound prerequisite
guard can reject face/patch/surface/region/buffer, native-buffer preparation,
height texture, position data, corner count/range or finite tessellation/grid
parameters. All observed failures are assigned to the pool stage; the recent
pass/pool93 is NUM_RENDER_TYPES86 + POOL_TERRAIN7. The recent source line14865
is the shared fail lambda, not the individual failed predicate. The latest
recorded failure is directional in all three runs (cascade2/2/3).

A rejected terrain batch marks the entire shadow view incomplete and clears
its collected batches. `endMetalSpotShadowCapture()` then returns without
encoding/publishing a new slice. These totals count rejected shadow views, not
individual terrain patches or GPU execution failures. All committed commands
complete in the retained checkpoint; matching abandoned commands never commit.

## Confirmed producer ordering gap

At exact builder afc9278d23:

1. `LLDrawPoolTerrain::captureMetalDirectionalShadow()` (307–323) loops the
   faces and directly calls the native batch capture. It does not call
   `ALTerrainSurfaceMaps::ensureUploaded()`.
2. `ALTerrainSurfaceMaps::getMetalHeightTexture()` (alterrainsurfacemaps.cpp:84)
   only returns an existing texture. It does not create or refresh one.
3. Creation/update is owned by `ensureUploaded()` (56), then `refresh()` and
   `upload()`. A new map starts dirty with no height image; a dirty existing map
   can still return its previous texture until refreshed. The refresh also
   requires the surface's CPU height data (`hasZData()`).
4. Sun-shadow generation runs before the lit terrain pass in both main display
   and cube display. The first shadow consumer cannot rely on the later lit
   consumer to publish its current height map.

The original source `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` does establish
this prerequisite: `LLDrawPoolTerrain::renderShadow()` (285–295) calls
`bindSurface()` before `drawLoop()`; `bindSurface()` ensures the maps are
uploaded first. Current native reflection capture also explicitly calls
`ensureUploaded()` (lldrawpoolterrain.cpp:468), documenting that the direct
producer runs before `bindSurface()`. The native shadow producer omits the
corresponding responsibility.

This establishes the missing preparation step from source. It does not prove
the height texture was the first false operand in any historical rejected view.
In particular, `LLVOSurfacePatch::dirtyGeom()` can deliberately clear a face's
vertex buffer while it awaits rebuilding. Native shadow preflight rejects a
null buffer; original `LLFace::renderIndexed()` skips one. Those states are not
distinguished in the old shadow diagnostics. The earlier PPD-125 classic-terrain
null-buffer repair does not identify the operand in this tessellated shadow path.

## New timing and retained limits

The exact native streams contain 42/81/15 abandoned main-view commands,
matching the shadow failure totals. All retained abandon events occur before
the logged STATE_STARTED second, using the recorded startup clock calibration:

| Run | Abandons | First → last abandon UTC, 2026-11-18 | STATE_STARTED logged second |
| --- | --- | --- | --- |
| uphzzchc | 42 | 03:07:42.751566 → 03:07:45.947666 | 03:07:48 |
| 77ib0k3e | 81 | 03:19:09.874730 → 03:19:12.993696 | 03:19:16 |
| sowsq5_l | 15 | 03:26:21.292261 → 03:26:22.220710 | 03:26:27 |

These command events carry no terrain reason. Matching totals are not a
per-event causal join. First events also share the printed PRECache transition
second, so their exact order against that transition is unproved. Retention is
qualified by the checkpoint's native contention drops; no lossless claim.

All 59/57/57 sampled main shadow censuses remain at42/81/15, respectively,
with probe failures0. Later shared-terrain publication is active with zero
reported shared-terrain fallback draws; aggregate upload/allocation failure
censuses are zero. Those facts support a startup readiness concern, without
proving any individual operand or blanket visual correctness. They establish
no cause for the continuing late FPS decline.

## Correct repair and verification boundary

The direct native terrain shadow producer must arrange current surface-map
publication before consuming the height texture, retaining the original lazy
dirty/no-change behavior and native resource generation ownership. Failed or
unavailable height data must remain distinguishable from successful refresh.
Record the first ordered terrain-shadow predicate and relevant observed values
before clearing the view, while evaluating native preparation once. Do not
weaken the compound guard or broadly skip terrain to remove the counter.

Qualification should cover a cold first shadow without any prior lit/probe
terrain pass, a dirty height update, failed upload/retry, geometry rebuilding,
and publication/command lifetime. If the remaining reason is null VBO, preserve
the original skip semantics together with shadow admission expected counts and
cache validity; copying the classic main/hero skip alone is insufficient.
The exact 42/81/15 historic predicates cannot be reconstructed from absent data;
future typed attribution and a controlled runtime retest are required. No source
repair or new runtime acceptance was claimed by that initial investigation.

## Local repair and contained qualification

Implementation source `aa8b05a5d6146eef0dec87217dbda0d5e6cf5942` restores the
missing publication prerequisite at the native terrain-shadow batch boundary:

- `ensureMetalHeightUploaded()` prepares the height before native vertex
  preparation and before any terrain batch is appended. A dirty update clears
  height readiness; a failed upload cannot pass merely because an old native
  handle remains resident. Missing CPU data stays retryable.
- Height success is independent of composition success: shadows can use a
  successfully committed current height while the existing lit/probe combined
  map retry remains pending. Current-height polls do no upload. Empty/outside
  dirty rectangles preserve readiness, including during composition retries;
  real new clipped height work invalidates it.
- The original pointer, native vertex, layout/range, tessellation and grid
  guards remain ordered and fail closed. Native vertex preparation runs once.
  Null buffers still reject the complete view; admission/expected counts,
  cache validity, dispatch thresholds, shader math and rendering quality are
  not relaxed. Native upload/COW/command ownership uses the existing path.
- A copied first/recent predicate record now identifies missing owners/buffer,
  height not current/missing, native vertex preparation, position/corners/range,
  and numeric inputs. It includes frame/startup state/map/direction, observed
  masks and height/geometry values. Both view-begin paths reset the first
  snapshot. Cumulative reason counts log at the existing census cadence; the
  bounded opt-in incomplete-slice diagnostic includes the current first record.

The actual production map-readiness and ordered-preflight bodies are exercised
by 508 assertions/14 CPU cases. The built project suite, including two native
Metal image-publication cases, passes 587 assertions/16 cases with observed
Metal API and GPU validation enabled. The latter uses real R32F LLImageGL
partial uploads/COW, an earlier-created consumer command, exact new/old GPU
readbacks and command completion leases after explicit CPU owners retire. It
also checks bounds-rejected upload leaves publication/content unchanged.
Existing terrain map layout tests pass 853 assertions/13 cases. These are
contained tests; they do not prove a live viewer's startup counter or visuals.

Doctor/configure/prescribed full Release build exit0 on M1 Max/64GiB,
macOS26.5.2/Xcode26.6; build37.143s from the isolated qualified cache. Strict
source scans3617 files with0blocking/scan errors. Two independent source reviews
check original guard order, exact texture lease acquisition, upload-before-shadow
queue submission, no new wait/worker, and unchanged admission semantics.
The prior69279 signed app is separately APFS-cloned and signature/hash checked.
New app26.4.0.69283 passes346ARM64/0invalid and deep strict signature checks;
executable SHA256 is
`3a02a1ebae785c6c986c68e0c383e2bdb3dc39c8e6349c8af73ff91e5d8ebc46`.
Final docs qualification is recorded in the repair receipts. No live startup/mirror/terrain/transition retest, whole-viewer
FPS acceptance or push is implied. Historical42/81/15 operands remain unknown.

The performance session receives the frozen implementation and exact receipts
after validation, with canonical state/ledger reconciliation left to its
integrator. This branch's diagnosis and previous PPD125 commits remain distinct.

## Evidence

Private root:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/terrain-shadow-cause-20261118`.

- `source-proof.json`, SHA256
  `2039f72a46019ee6591ab90d6c366b5dba15a33f85755c391ce7b1f1515c06c6`:
  exact original/builder function bodies and hashes, static producer/getter
  assertions, independently checked native UTC-bound arithmetic. Exit0.
- `native-startup-evidence.json`, SHA256
  `22c570073763e3408a712badeb26296c0ed68f98193f1df55d7499a2595cbb6f`:
  six fully hashed/line-counted original inputs, calibration, precise abandon
  bounds, all census summaries, source identity and explicit attribution limits.

Two independent read-only investigations supplied source and raw-input results;
the integrator inspected the source and verified the receipt's clock arithmetic.
An independent source peer reviewed the completed report and found no material
causal overclaim. Final documentation harness/diff checks are retained in
`qualification.json`; this is source/evidence qualification, not a runtime test.

Repair receipts are under the private root's `repair/`: `doctor.json`,
`configure-01.json`, `build-01.json`, `standalone-cpu-tests-final.json`,
`native-tests-01.json` (587/16 includes the CPU cases; do not sum),
`terrain-layout-tests.json`, `strict-source-check.json`/`strict-source.json`,
`baseline-preservation.json`, `app-verification.json` and final qualification.
The final receipt is `repair/final-qualification.json`. Pre-freeze source review
also corrected unnecessary height invalidation for empty/outside rectangles,
including when only composition remains pending. The earlier 494-assertion CPU
result and final 508-assertion extension are both retained, without conflating
them with native GPU coverage.
