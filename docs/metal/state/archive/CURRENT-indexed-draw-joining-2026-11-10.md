# Native adjacent indexed draws — geometry census and opt-in prototype

Observed2026-11-10, physical M1Max/64GiB/macOS26.5.2/Xcode26.6.
No new FPS improvement or actual joined-draw count is established.

## Original contract and bounded opportunity

Exact1eb44bb LLRenderPass::pushBatch applies current state and emits the original
index range in order. Original/current LLVolumeGeometryManager::registerFace
already combines many compatible faces. Shadow coalescing therefore does not
prove additional G-buffer opportunity. Native world G-buffer/forward ordinary
batch loops still emit one indexed command per logical batch.

Diagnostic1ffdef5d531e3574dbb8274463e38490dfd134ee adds only default-off
ALCHEMY_METAL_DRAW_JOIN_CENSUS=1 CPU logging after successful phase encoding.
Each phase has13independent view tuples, at most once/120frames. Necessary
same-wrapper VB/IB, contiguous indexbytes/type, complete triangles and uint32
pair-count fit are counted. Matrices/materials/PSO/attribute bindings are NOT
compared; these are upper bounds, not actual safe joins or CPU time. Excluded
logical records remain barriers. Whole query-bearing G-buffers are explicitly
query_gated/unmeasured. Forward scope is only static ordinary fullbright/shiny,
not alpha, water/haze/glow/emboss. No draws/resources/states/cadence change.

## Actual normal build and completed settled census

Normal68616/source1ffdef5d actual doctor/configure/build/verify/deepstrict0,
349ARM64/0bad. Exe9814f07a6baabe2326fcc4c08f8b4d6333129b17b0c0d76c61640a6d483f5eea,
normal metallib39320181. Build packet
/private/tmp/alchemy-metal-draw-join-census-build-20261110.gcbekdvk.
Independent identity074c00d6 checks generated/plist/embedded stamp, actual
pipeline/llversioninfo O3/NDEBUG/C++23/ARM64 recipes and referenced response,
Metal3.1/fastmath/sourceRecordingOFF, top-executable ARM64 and deepstrict0;
349slice root proof checked, not independently rescanned.

Run artifacts/metal/qa-runs/candidate-draw-join-census-20261110-01,
CSVprivate-68616-draw-join-census-20261110-01.csv+siblingprovenance. Config904c4d59,
source pinned, same private location, fullscreen BEFORElogin/Midday/Preset123
actually applied with CUA. All43start/endcontrols and freshgates0; complete300
ready frames4957–5256, native1/fallback0/bias1/256stable120/cameradelta0/restores
true, exact camera/EEP/sun match prior68614. All CUA phases completed.

Independent formal-only audit actual0:78unique rows, all26phase/view tuples
exactly3samples/120frame spacing. Stable hero G-buffer upperboundpairs:

| Hero face | Total source batches mean | Static scope mean | Adjacent pairs upper bound |
| --- | ---: | ---: | ---: |
|0|1832|1276|779|
|1|3483.33|2926.33|1791–1792|
|2|1409|929|470|
|3|1879|1519|812|
|4|272|272|191|
|5|111|109|54|

Main G-buffer is query-gated/unmeasured644–654queries; its logged0 is NOT observed
zero opportunity. Main restricted forward20edges/667batches, hero1 34/839;
others0–18. Ordinary-face samples can refer to different probe owners; do not sum
13view means into frame savings. Positive geometry counts justify testing exact
native-state joining, not a claimed speedup. Main source forward830→667 and mean
draws−2.2994% versus prior68614 demonstrate workload variation. This instrumented
CSV is NOT an unmodified performance baseline; no FPS ratio is credited.

Audit /private/tmp/alchemy-metal-draw-join-census-audit-20261110:
JSONc254f61ca0a139d0833cea72cba10b4d1c33fd51d37afa78bfba2e3171b63338,
findings ab021da1ccdb726c5613166e4bd5e406b3a7e6b87c3d8a5573617543f5efe887.
ExactPID37648 executable check→SIGTERM→Goodbye/PIDabsence0; OSexitcodeunknown.
OWNprivate-draw-join-census-environment-plan-20261110-01.json restored0 AFTERexit.
No active plan/viewer/profiler/GPUtest. Private evidence copied under ignored
artifacts/metal/private-draw-join-census-20261110/; never upload private logs.

## Prototype under correctness gate — NOT enabled in production

Root applied private wrapper208c9c7c: default-false setIndexedTriangleJoining,
strongly retained pending native IBO, adjacent complete triangle/instance1/range/
type/count validation; every actual native mutation/nonindexed/patch/query/end/
destructor flushes first, cache hits may retain pending work. Native interop,
cache invalidation and prevalidated counters flush AND disable joining.

World scene.join_adjacent_static_draws defaultsfalse. Whole query-bearing,
wireframe and opaque-coverage-stamp scenes remain direct. Every special batch
explicitly disables before its branch; only static ordinary simple/mask/bump/
material/PBR G-buffer can opt in. Source geometry and immutable FrameData ranges
remain frozen; no primitive_id/device writes/atomics/ROG in eligible shaders.
No indices, triangle order, shader/library/math, shadows/casters, material ABI,
quality or pass boundaries change. All29nativecommand sites and wrapper/world
integration have independent source approval; executed proof follows.
Generic pipeline producer does NOT enable the flag.

Committed c2804f88df2923a9cbc172bc551fad5cd38243eb contains seven source/test/
CMakefiles: wrapper/world headers+implementations, CMake, wrappertest and expanded
productionGPUtest. Sourcefiles match both fullsuite recorded hashes exactly. Initial Release Ninja core compile completed actual0
in /private/tmp/alchemy-metal-indexed-join-integration-20261110.8ani3l5u.
Existing fullRelease suite with opt-inDEFAULTFALSE passed actualAPI+GPU
494211assertions/104cases0; bothvalidationpreambles confirmed. That default suite
alone was not opt-in correctness proof; focused results follow.
Root integrated source-reviewed wrapperfixture1ab3 and productionfixtureee089.
Actual focused API+GPU pass0: wrapper7969/1(58invocations) andproduction2120/1
(67strictpairs plus2specialremovalcontrols). Both actual validationpreambles
confirmed. Core still unchanged; original GPUtest byte-exactly preserved by
removing unique insertion72287180. Pipeline/query positiveoracles were strengthened
BEFORE first GPU run; an unused countingproposal was retained but not adopted.
Production fixture initially assumed coverage stamped stencil; peer source review
corrected it to actualcoloralpha/S8zero BEFORE execution. No tolerance relaxation.
Updated completeRelease and RelWithDebInfo EACHactualAPI+GPU504300/106 passed0.
Both build steps0, both actual validationpreambles confirmed, same39320181library.
All16 recordedsourcehashes rechecked before coherent c280commit. No hardwarejob
is live. PrivatequietcompleteCPU/GPU cost remains; root alone builds/runs GPU. No performance admission/FPS trial until then. Full product gates and
20FPS goal remain OPEN/ACTIVE; infeasibility unproved.

Privatecostpacket /private/tmp/alchemy-metal-gbuffer-join-cost-20261110 is
source-reviewed, UNEXECUTED: patch813fa308ce031602730d69852865c55d3c7ed55b597c469465ca5a6c6b77aa14,
benchmark0fab8d6f/candidate4024883b. It preserves strictsourceee089 after removing
exactbenchmark+7privateimports. Sixcases/168samples/sevenABBA/BAAB, controlled
physical1024target, actual fullrenderer CPUprecommit+commit separateGPUCB/wait/
callback/diagnosticdrain. Capturedarrays/resourcecreation excluded; syntheticrun
frequency/retirement/fixedR/outband limitations remain. CatchOR parentheses
corrected BEFOREcompile; no benchmark build/execution yet. Do not claim gain.
