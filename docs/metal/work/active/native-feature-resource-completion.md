# Native feature and resource completion — 2026-11-11

**Integrated checkpoint:** `696c43ba78` (benchmark registration/cleanup complete).
**Reference:** original viewer `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`;
compare immediate native source and transition `3087308155` too.
Host M1 Max /64GiB /macOS26.5.2 /Xcode26.6, native ARM64.
No viewer/utility/window launch, menu execution or screenshot. Goal remains active.

## Source behavior and implementation

Original shader reload changes actual executables; refreshing only native pass
metadata was insufficient. Commits29e483ae2e,30d6930858,4b449a92fa,56012ea17d
extend native replacement to utility/preview owners using a revision announced
only after successful forced world/UI and metadata rebuild. Failures retain
previous resources and retry; active transactions reject replacement. Utilities
recreate used variants; preview BRDF LUT resets and regenerates. Output textures,
retained pools, target histories and old command leases survive replacement.

Original Frame Profile measures GPU execution, not CPU shader binding. Commits
fb603d27b3/d4bf657a13 use real Metal stage-boundary timestamps with calibration,
encoder labels, pipeline identities and asynchronous completion including UI.
M1 Max exposes no per-draw/dispatch/blit/tile counter sampling or statistics set.
Reports describe unavailable measurements explicitly, omit incomplete identity
claims and do not sum overlapping GPU intervals. Unlabelled descriptors exposed
a runtime nil-label assertion; six optional production setters now skip absent
labels, keeping the unlabelled test fixture.

Original vertex mapped stores and draw order establish the native contract.
Commits845ecfa1ac,3e54ec8f73,dd18f31146,a970bf0311,352100f5db remove GL
allocation/upload/draw authority while retaining aligned staging, dirty flags,
retirement, exact U16/U32 index stride, offsets and retries. Native UI draws
check per-store initialized/non-dirty publication independently. Raw getters
remain allocation accessors; changing them would break intentional retry
inspection. Direct member draws flush earlier immediate geometry, and sphere
uses generic capture once. Meaningful Metal output tests replace GL-only tests.
Volume/grass merging9773539962/141597fd0c preserves original material/order/
rigging predicates but replaces GL_MAX_ELEMENTS recommendations with actual
capacity, adjacency and subtraction-safe checks. No performance claim.

Original/current GLTF UBO helpers were write-only without any bind/read consumer.
b2ffd2309d/86499350ab remove those sinks and an unused raw transform packing
helper; preserve CPU node hierarchy, inverses, animation/skin preparation, real
material packing and native avatar palettes.3d7797206e reports actual native
device identity in About/feedback/abuse reports. cfaf16e109/b3316c032b remove
dormant GL manager/entrypoint loading and all physical llgl.cpp state/error calls;
preserve CPU state defaults/restoration, depth remapping and flush boundaries.

Developer benchmark325354e4da preserves original1024²RGBA8 sources/targets,
32 textures,64×32 target0 draws and two timed runs. Actual Metal visibility
queries count2048×2²⁰ passing samples, yielding2³³ sampled bytes; completed GPU
command timestamps supply the time. Uploads/clears are untimed. No CPU fallback
or global viewer render state mutation. Bounded waits and failed/busy/conflicting
capture paths report errors. This is sampled throughput over a dedicated GPU
command envelope, not hardware DRAM traffic or shader invocation count.
The native benchmark displaces gBenchmarkProgram and benchmark GLSL. The same
commit fixes squiggle admission from an always-zero GL name to published Metal
backing; layout/geometry/sampler/color remain unchanged.

## Exact evidence

Receipts: `artifacts/metal/feature-completion-20261111/`.
All listed completed commands exit0 unless explicitly stated.

| Command / log | Observed result |
| --- | --- |
| `cmake --build build-Darwin-ninja-os-arm64 --config Release --target almetal_tests llrender_tests --parallel 8`; utility/manager/native-draw build logs | Native targets built |
| `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/llrender/Release/llrender_tests '[native-draw],[alvertexrange],[llvertexbuffer],[shader-lifecycle],[baking-loader],[alrendercapabilities],[alrenderstate],[llglfloor],*native*'`; native-draw-llrender-tests.log | 67598 assertions/42 cases |
| Validated `almetal_tests` FrameProfile/utility/preview filter; manager-profile-utility-gpu-tests.log | 60425 assertions/12 cases |
| utility-preview-reload-tests.log | 2304 assertions/8 cases; superseded by combined filter, do not add totals |
| frame-profile-gpu-tests.log | Initial abort134: absent optional pipeline label |
| frame-profile-gpu-tests-final.log | 58121 assertions/4 cases after production optional-label repair |
| `cmake --build build-Darwin-ninja-os-arm64-appearance --config Release --target appearance-utility-bin --parallel 8`; utility-vertex-appearance-build.log | Optional appearance build0, not launched |
| Isolated benchmark GPU validation; `/private/tmp/alchemy-native-developer-benchmark-validation/validation-receipt.json`,gpu-tests-final.log | 103 assertions/2 cases; five Release TU compiles, full MSL compile, standalone link0 |
| benchmark-native-test-build.log; benchmark-profile-reload-tests.log | Registered targets and validated filter `[developer-benchmark],[frame-profile],[utility-reload],[preview-reload]` exit0;19123 assertions/10 cases |
| benchmark-native-gl-audit.json/log | Strict exit1:1324 findings/2406 files after manager/direct-draw cleanup |
| `python3 tools/metal/validate_harness.py` | Final0,32 required files; initial recovery wording omissions corrected |
| frame-profile-counter-capabilities.log | Physical M1 Max stage-boundary timestamps only |
| utility-vertex-gl-audit.json/log | Strict exit1:1427 findings/2406 files,146 call syntax before subsequent manager/draw cleanup |

Canonical app remains the older30ef2ccb69/26.4.0d68655 verified checkpoint.
See native-shader-owner-cleanup.md and shader-native-identities.json for exact
hashes, link65 failure/restoration and final0 build/import/codesign checks.
Do not attribute that app to current source. Frozen696c43ba78 doctor/configure/
build sequence is running, handle14238; feature-native-*.log receipts.

## Next boundary

Root completes registered benchmark test, displaced shader removal and canonical
build. Isolated image owner fixes readback mixing pending type/format with the
old published texture before removing audited dead GL helpers. Vertex-label
owner preserves diagnostics on real Metal allocations, removes displaced
pointer declarations and excluded code. Read-only menu/RLVa audit continues.
CURRENT pins exclusive files/worktrees. Full parity, source/runtime zero-GL,
stability, release review and multi-scene performance are still open.
