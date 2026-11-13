# Geometry uploader port — 2026-11-13

Worktree `/private/tmp/alchemy-geometry-uploader-20261113`, branch `codex/geometry-uploader-20261113`, base `0c68a66e2189d79ccf221352ecea1fa905d3ca79`. Apple M1 Max / 64 GiB / arm64 / macOS 26.5.2 / Xcode 26.6. PPD053 feature scope, PPD054 original-normal/identity corruption, reopened PPD048 allocation/publication safety. Root registered new findings before repair. No canonical source/build-directory mutations, CMake/docs changes, extra skins/controls, pushes, viewer/account/scene operations or performance claim.

## Local commits and dependency contract

Apply after geometry infrastructure and core:

1. `c0ff2e9fc7b1150320a7022e0ba77289e8f51d35` — final attribute-aware LOD helper, uploader integration, skin/normal tests, model-identity normal restore.
2. `fac081495969a971043ed633103cef1af766695d` — checked temporary compaction/empty-face buffers, complete-model and per-LOD staging, native-buffer retention, normal backup/restore safety, allocation regression.
3. `452566a9d186e25f4e4cb700643f7e172861e26c` — preserve standard allocation exceptions as distinct from legitimate no-triangle algorithm fallback; otherwise an OOM could be treated as an ordinary request for a lower quality replacement.

Core prerequisites supplied by peers: infra `30fe09fd53ea2271bd7d132610199a0c6d7f0212`, vector/SIMD/planar `20e4a274e9`, core `d81f565dff`. Uses `bool LLVolumeFace::generateNormals(F32,F32)`, strong/throwing face copies, `noexcept LLVolumeFace::swap`, `getVolumeFaces()` and friend `ll_test::VolumeProbe::decode` into `decodeVolumeFaces`. `LLModel::generateNormals` changes from void to bool, keeps a model's original faces unless every generated face succeeds, and catches allocation failure. Repo-wide callers are the owned preview callbacks and test; preview logs failure and preserves saved originals.

Actual tests replace the old math archive with the peer's **fresh complete 34-TU** `/private/tmp/alchemy-geometry-core-checks-20261113/libllmath_geometry_core.a` (includes new planar/scratch production units), source-pinned to core d81. They use custom meshoptimizer **1.3#5**, not the old canonical archive:
`/private/tmp/alchemy-geometry-meshoptimizer-20261113/installed/arm64-osx-alchemy/{include/meshoptimizer.h,lib/libmeshoptimizer.a}`. All other canonical dependencies are read-only. Compile metadata came from canonical `build-Darwin-ninja-os-arm64-followup`; source/include paths and the two archives were explicitly substituted. Complete argument arrays and final archive/header/source SHA256 hashes are retained below.

## Upstream/reference mapping

Final source reference `408fd6c014e6e3b2b90dfe61265f34df0628f450`, including coherent #567 (`5fe299ad73`), #571 (`3872ceeb74`), #575 (`dc247a509e`), #577 (`d835df2665`) and later uploader tails through that snapshot. Important sequence: `466cb5d487`, `caf86d08f2`, `df8fae4a26`, `1928e7a6a7`, `96631b9b5d`, `0d6a6b9492`, `3ab0f9fde0`, `7c013e56dc`, `f44158931c`, `25c09c57e8`, `815ccfe6aa`, `8f11d19fff`, `ebea7a863e`, `fe2dadfc34`, `0965df5481`, `9e7aff3594`, `ea8e64184b`, `0bf5fe1633`.

Nine owned paths only: almeshsimplifier.cpp/h, its test and almeshcorpus.h; llmodel.cpp/h and its test; llmodelpreview.cpp/h. The final helper/corpus/test files and LLModel CPU changes were imported from the actual final source, then allocation adaptations added as described here. Unused LLModel::optimizeVolumeFaces was retired consistently with final core API; no remaining callsites.

Behavior:

- Simplify in authored proportions with normal/UV attribute error. ALL/UV/NONE/SLOPPY seam modes retain material borders as their contracts specify, including double-sided shared rims, texture islands and face compaction.
- Prepare one simplifier per unchanged base model; invalidate on base replacement, normal changes/restores and material-list reorder. Auto retains the closest precise output, skips equivalent UV/NONE work, preserves the better sloppy correction and performs bounded distant-part pruning.
- Preserve source positions and all retained vertex attributes, keeping exact position-keyed skin lookup valid.
- Generate normals through final LLVolumeFace's crease-aware implementation, not the retired faceted-copy/nearest-normal uploader algorithm.
- PPD054: capture every model's original faces before rewriting shared high/base models, associate copies with their actual LLModel owners, restore into those owners and release lost-model copies. The old ordinal list and copy-after-mutation sequence could replace a newly loaded model's vertices with another model's saved faces, not just display stale normals.
- PPD048: build checked, correctly padded 64-byte position/normal/UV and 16-byte index/weight/tangent buffers in a temporary face. Compact publishes via noexcept face swap; whole-model simplification stages all faces before vector swap. Allocation exceptions leave original targets intact. Whole LOD models, scene, request metadata and native buffer invalidation publish only after a valid replacement is ready. OOM propagates through the simplifier to the preview's outer allocation handler; genuine no-triangle results retain their normal fallback semantics. Failed normal capture/restore retains model data and original backup for retry.

Original Metal ownership boundary is retained: native preview helper block, full renderMetal suffix and genBuffers block are **byte-identical to base**. `native-preservation.json` records independent hashes:

- native helpers: `f88777e707319ab0350d84bdd0b60b9cc18cbc6bff8fe0b60ce044351df5e4a9`
- native renderer: `5b701633f8385c48e3dae51d5d83522970ab85aeabc2ec98168fe38dbbbf360a`
- buffer fill: `32fc29b18adcfe3f5d112ed2c29f6eba1c25dafd77c4fcf6ed0037a55947a308`

No GL renderer restoration, sky changes, material/selection shader changes, native capture/presentation/publication rewrite or broad uploader framework.

## Build registration for root

No child CMake edits. Root adds production `llmath/almeshsimplifier.cpp`, header `almeshsimplifier.h`; llmath test SOURCES `tests/almeshsimplifier_test.cpp`; primitive test SOURCES `tests/llmodel_test.cpp`. `tests/almeshcorpus.h` is a quoted test helper header, not a test source to register as its own case. Tests link existing llmath/llprimitive dependencies with the new meshoptimizer package. The optional private corpus stays disabled unless explicitly requested; no corpus assets were read or added.

## Executed checks

`tools/metal/doctor.sh` in the isolated worktree: exit 0. It confirms hardware/tools only; no application build or performance inference.

Scripts under `/private/tmp/` and receipts/logs under `/private/tmp/alchemy-geometry-uploader-probes-20261113/`:

- `alchemy-geometry-uploader-compile-20261113.py`: all five actual production/test TUs compile, including native LLModelPreview.cpp, with Release/O3/Werror metadata, new core headers and 1.3#5. Modified helper/test recompiled after allocation-policy changes. `compile-receipt.json`, `allocation-compile.json` contain exact commands.
- `alchemy-geometry-uploader-link-20261113.py`: isolated helper/model/actual-method suites; old canonical libllmath and meshoptimizer are replaced explicitly, not linked alongside the new versions. Final links exit 0. Duplicate static-library warnings (bz2/freetype/harfbuzz/png/z) only.
- Every runtime probe uses LLDB batch containment, `process launch --shell-expand-args false`, observes actual child exit status, and has crash status/backtrace/kill hooks. No fault occurred; no stopped child was continued.

Final functional results:

| Check | Assertions / cases | Actual child |
|---|---:|---:|
| Final simplifier suite, all ordinary upstream cases plus selected every-mesh-allocation regression | 1092 / 23 | 0 |
| LLModel skin lookup and later-face failure preserves entire model | 152 / 2 | 0 |
| PPD054 old actual preview methods at base0c68 | 61 / 2, **21 failed** | 42, expected control |
| PPD054 fixed actual methods | 66 / 3 | 0 |
| PPD048 old pre-staging LOD method at c0ff | 7 / 1, **4 failed** | 42, expected control |
| PPD048 fixed allocation / actual LOD producer | 96 / 5 | 0 |
| Same selected allocation paths, own helper/model/probe ASan+UBSan | 96 / 5 | 0 |

Controls are retained as failures, not added to passing totals. Sanitizer run overlaps the ordinary allocation cases. The registered simplifier+model total is 1244 assertions/25 cases; private method probes test additional scenarios and are reported separately.

PPD054 probe copies actual generateNormals/restoreNormals/drop-lost-copy method text and uses real LLModel and LLVolumeFace data; only hosting class name and UI setting/status/container scaffolding are substituted. It proves the shared high/base original-normal failure, replacement-model vertex corruption, repeated generation/restore and lost-copy release. `preview-method-extraction.json`, generated old/new C++, compile/link/LLDB records are retained. New methods are fac081 source; old reference0c68. It is not a full UI/window execution.

PPD048 probe likewise copies actual genMeshOptimizerLODs/simplifierFor. It uses real model, scene and simplifier data with fake UI controls and buffer-map tokens. Old c0ff lets the allocation exception escape after replacing model/request and clearing buffers; staged fac081 preserves all of those. The fixed success case publishes a valid complete scene/model before clearing old buffer tokens. Helper-only aligned-allocation injection tests position, weights, tangents and index failures plus empty-face replacement; a first std-new failure verifies LLModel clone failure retains all model data.

ASan+UBSan instruments the actual helper, LLModel and private probe objects. The fresh complete core archive and custom dependency archives are **not** reinstrumented in this child run; do not call it a full sanitized viewer/core run. `ASAN_OPTIONS=abort_on_error=1:detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; no sanitizer diagnostics. No leak-detector claim.

Setup corrections retained: initial compile template referenced an unregistered old llmeshoptimizer test file, then used existing mathmisc metadata; an old-only private probe needed [[maybe_unused]] on a fixed-only assertion helper under Werror; initial scratch sanitizer link lacked sanitizer runtime flags, corrected with explicit address/undefined flags. These were build/probe setup errors, not viewer defects or successful tests. Combined logs and final receipts retain commands/results.

## Limits and next action

Child did not build or verify an app bundle, launch the viewer/uploader, load/upload user models, read private mesh corpora, capture a GPU frame or measure performance. Native source preservation plus compilation does not prove runtime preview parity. Root owns combined build/CMake/test/bundle/source verification, integration review and any native validation. Owner confirmation remains necessary before defect closure or pushing. PPD058 draw-registration/retry handling stays root-owned and is outside these files.

Final source `452566a9d186e25f4e4cb700643f7e172861e26c`, clean worktree, `git diff 0c68a66e21 HEAD --check` exits0. All work local, no push.
