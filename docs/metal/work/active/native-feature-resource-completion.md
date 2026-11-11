# Native feature and resource completion — 2026-11-11

**Integrated checkpoint:** `815416675e`; latest verified app696c43ba78.
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

Frozen696c43ba78 canonical doctor/configure/build exits0; version26.4.0d68677.
verify_app0:349Mach-O/0invalidARM64; deepstrictcodesign0; viewer imports have no
OpenGL/Vulkan/MoltenVK/ANGLE framework or GL/CGL/SDL_GL undefined symbols.
feature-native-identities.json pins executable SHA256
`a222d0dfffe8612f9fb5adcd7a7ecb948882726a65f66c3905886809182c6984`
and metallibSHA256
`b8da56415b3546dedc50882109b20568ab4a8ec6f6bf8d3885e6165ea18e3f89`.
feature-native-doctor.log/configure.log/app-build.log/verify-app.log/codesign.log
and feature-native-viewer-imports.log retain exact command results. Earlier
30ef2ccb69/link65/restoration receipts remain in native-shader-owner-cleanup.md.
The current815416675e source still requires a new canonical build.

## Integrated image, label, profiler and context follow-up

b91d739a46 aligns readBackRaw's type, compressed allocation and BC row pitch
with the already-published native owner while a replacement upload is pending.
The original viewer's publication boundary and immediate native producer code
were checked separately; producer getStorageInternalFormat stays live-build.
Baseline325354e4da fails both real staged-generation cases (RGBA8-to-float
rejects old byte readback; BC1-to-BC3 incorrectly allocates64 instead of32bytes).
Fixed no-layout implementation passes83assertions/2cases under Metal validation,
including cancel/publish, old/new texture identity, exact bytes and accounting.
Evidence `/private/tmp/alchemy-image-publication-validation/` baseline/fixed
GPU logs and Release compile/link receipts.31db370a3d then removes13physical
GL calls, dead name/deferred-delete pools, accounting map and allocation/helper
graph.29 native publication/worker/rollback/alpha/mip/readback methods compare
unchanged;12 Release TU syntax checks0. Disabled-manager destruction coverage
is preservation evidence, not a newly discovered baseline accounting bug.
The changed image class layout requires rebuilt dependent GPU tests.

2e2b542800 labels actual Metal vertex/index resources across publication/reuse,
removes always-zero GL fields, nine displaced pointer pairs, old GL label macro
and exact excluded face block. The original pass-name switch is retained.
Seven Release syntax checks0; physical label/lifetime test awaits root execution.
815416675e registers new image/label/primitive regressions and removes the last
seven image pointer pairs after their production consumers disappeared.

d0f6fcf6f0 removes the disconnected GL per-shader query profiling engine after
actual native Frame Profile and benchmark replacement.27 retained native shader/
uniform methods compare unchanged;10 Release TU syntax checks0. d397b548ca
checks actual primitive query support before plotting, preserving supported
empty-workload zero semantics without fabricating unsupported zero measurements.
The actual profiler-enabled owner fails two baseline assertions (exit42), then
passes7assertions/1case after repair (exit0); primitive-profile-baseline/fixed
logs and consumer census are retained under `/private/tmp/`.

e4d33e2278 removes uncalled SDL shared WGL/EGL context APIs and reset callbacks;
original1eb44 workers needed shared GPU namespaces, while current native uploads
share Metal resources and no callers remain. Release syntax check0 in
window-tail-syntax.log. b17ba5d131 removes unused application/plugin OpenGL
package/link declarations after production GL entrypoint census.007defc0cf
enforces native macOS/ARM64 and rejects the optional dependency OpenGL demo.
Actual configure/build validation for this newer checkpoint is pending.

`image-label-profile-native-test-build.log` is running(handle79692) to rebuild
all header dependents before image/layout and label execution. Latest strict
audit(image-label-native-gl-audit.json/log) exits1:1257findings/2406files,
36call-syntax matches. Remaining physical matches are unsupported Win32 source;
CPU coordinate helpers, glTF strings and native compatibility class names also
match. Do not equate this with the strict release gate passing.

## Next boundary

Root completes rebuilt image/label/profiler/draw regressions, integrates bounded
dead generic render wrappers/water helpers, then freezes a new canonical build.
Read-only exhaustive menu audit and independent label/water diagnostic review
continue. Unsupported Win32 source, native-name inventory false positives and
excluded legacy test fixtures still require original-mapped scope resolution.
CURRENT pins exclusive files/worktrees. Full parity, source/runtime zero-GL,
stability, release review and multi-scene performance are still open.
