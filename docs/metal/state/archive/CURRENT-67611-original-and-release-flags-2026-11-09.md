# Settled original comparison and Release compiler diagnosis

Observed2026-11-09, physical M1Max/64GiB/macOS26.5.2/Xcode26.6. Owner-selected
private location, numericPreset123, Midday, fullscreen before login, Ultra6,
3456x2168. No avatar travel or external scene change is assumed.

## Exact external original67611

Sourcef66dedb2615be7981a5c30327c8fb5fe333bebde in external checkout
/private/tmp/alchemy-reference-1eb44bb, branchcodex/reference-frame-observer.
Renderer/shader algorithms remain exact1eb44bbd1bdc76d0100c75699bad1fb337e8cb45;
default-off observer and approved accessibility additions only. No in-fork GL
fallback. Exact app path:
/private/tmp/alchemy-reference-1eb44bb/build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Development.app.
ExecutableSHA2566bee7df4d3317e6f4cb57c941accd3e84d7b0cb23c0b1d13c5496033f5591dc5.
Fullbuild/349ARM64/deepstrictsignature exit0; CPU3731assertions/18cases pass.

43live controls pass and match native68564. Four role overrides match, including
mirrors/dynamic exposure/vintage mode/persistent local environment. Original
unregistered RenderHeroReflectionProbeDetail is explicitly declared typedS32=1
in session settings before cached-control initialization; CLI--set alone ignores
unknown controls. This matches the native requested dynamic mirror detail.
SMAA type2/qualityINDEX3 is not a three-sample MSAA setting.

Actual retained G-buffer AND screen3456x2168, completeness1/1, active main pack.
The collector gates actual targets because original allocation can silently
halve internal dimensions. Window dimensions alone would not establish parity.
All256 assigned probes initialized/faded with120stable observations, routine
refresh still active; main visible104.300consecutive frames9358–9657 completed
08:47:02UTC after about25minutes of initialization. No compile/GPU/export/heavy
analysis overlapped warmup or sampling.

| Metric | Original67611 | Native68564 |
| --- | --- | --- |
| Mean frame time,ms |164.054928473 |107.873381250 |
| p50,ms |162.452146 |106.4846455 |
| p95,ms |185.230250 |118.416000 |
|1000/mean,FPS |6.095519405 |9.270127518 |
| Consecutive samples |300 |300 |
| Main visible probes |104 |97 |
| Sampled texture bias |1,all300 |Not instrumented in this run |

Raw original CSV artifacts/metal/private-67611-ready-metrics-20261109/frames.csv;
sidecar frames.csv.provenance.json. CSV SHA256808600a1a498f6fca874f7ad2ffc17aa391e5ede1a6a64e2ec14b520211d8757;
sidecar6496c27694ce797e79dc443c97a040d7d39c605e4bd3de3411eddeb9f4267cf4.
QA run artifacts/metal/qa-runs/reference-67611-ready-20261109-01.
`tools/metal/perf_compare.py --enforce --require-provenance` against native68564
exits1: this scene meets the mechanical p50 target, but three scenes are missing.
Reported p50/p95 reductions34.45%/36.07% are preliminary observed rates, not a
quality-equivalent backend-performance claim. Camera/modelview differ only
0.038mm vertically, sun matches; projection differences are only depth terms
for infinite reverse-Z versus original finite forward projection.

Quality caveats: native68564 lacks sampled-window bias; older native samples
showed4, which is not fresh evidence for68564. Main97 versus104 has no explained
equivalent owner-level result. Source audit finds identical assigned/complete/
main-occlusion selection, cube-fan geometry and query insertion order. Different
depth precision, alpha mip coverage or assigned physical owners can affect
boundary query outcomes. This is not proof of a missing-probe bug or a scene
change. The newly discovered compiler mismatch below also limits comparisons.

Separate CPU30s recorder actually exited0 before analysis. SuperQ closed the
viewer, PID56891 absent, Goodbye08:49:04; temporary environment/persistence
restored, private67611startupplan cleanupDONE. CUA phase completed exactlyonce.
InitialgetApp stalled~17minutes at login; no measurement occurred during it.

## Original CPU profile

All exports/parsers finish0 in /private/tmp/alchemy-original-67611-metrics-20261109.
MainRunning21.735/30.844830s=70.466% of ONEcore. Available-stack actors hero50.43%,
ordinary5.01%,remaining/unattributed44.56%; the last includes0.198s missing stacks.
GLdriver call-path union74.44% of Running, draw wrappers/APIs65.68%,flush15.74%,
texture-state17.28%; inclusive families overlap. Kernel trap leaves chiefly
appear under driver flushing, but Running samples do not measure all GPU waits.
334objects had stopped-DWARF analysis, including renderer/spatial/probe owners.
Complete source/inline attribution is unavailable. Culling shares differ, but
profile categories and missing inline attribution do not establish traversal
counts or a per-frame5x regression. No cross-trace per-frame normalization.

FINDINGS.md SHA8a47cd15bfd8d18cccf7bfb7a556457631d584068b9bf81d18e0da237e75111c;
analysis-summary.json91d9503fcecea6f0dae1148212aacb726ddad298e9d89ff0d6fc5bf84d1f4783;
cull-caller-details.json21a924cd0a8f181bf5535e3329d80c54a6945e19cbceb4a32ab920a65618c627.

## Confirmed native Release compiler mismatch

Native Xcode C/C++ Release cache entries were empty. Actual llmath/llrender
compiler response files use-O0/-g withoutNDEBUG. BEFOREproject census27/36targets
Release0; all36RelWithDebInfo3. Mixed Objective-C targets retain-O3 because those
language defaults remain canonical. Original fresh Ninja2199actual compile rules
across105targets use-O3/-DNDEBUG. Historical cause of empty native cache entries
is unknown. The Release label alone did not establish optimized code generation.

00-Common.cmake relies on CMake defaults for Release optimization andNDEBUG.
Commit59177ae1df35a210e83122cb49cc5cf8d80a0a3e restores standard-O3/-DNDEBUG only
for empty/whitespace enabled-language Release entries under Darwin/Xcode, updating
cache and normal binding (CMP0126NEW). Nonempty custom flags and other
configurations remain untouched. Independent review and seven private exact-block
CMake cases pass. AFTERproject metadata all36Release3, RWDI unchanged. Fresh
compiled-response verification, full build/tests and runtime remain required;
metadata alone is not evidence of a new optimized executable or speedup.

Private audit /private/tmp/alchemy-metal-release-opt-audit-20261109;
BEFOREcensusSHA434f90d75fb744a80b8db60a99cfc8d077371cf1a0ff95466d5e018d0b6bfe67,
AFTERfb86fe7024329156dbfc4ca2221f2c8fd523c285ba5199460d42134ca64d0227.
No complete pre-fix Debug metadata snapshot was collected; Debug preservation is
covered by the source guard/private sentinels, with current metadata allDebug0.

## Diagnostic artifact and rejected experiment

Commit943cb1a373d84153768233d61be1b709d47df31d adds opt-in sampled-row texture-bias
counts/minmax outside frame/camera/readiness identity; unchanged CSV and policy.
BOTHCPUconfigs2904/29pass, doctorhost0, app68565fullbuild/349/sign0. App not
launched, preserved /private/tmp/alchemy-metal-68565-unoptimized-telemetry-baseline.app.
Exe b1ef71f84d0b0e66c64facc6a4a36ca624afa9f618d63c0ee079eb5ffc8c4473;
metallib8bbfe4 unchanged from68564. Restricted doctor failedsysctl; host rerun0.

Private SH predecode /private/tmp/alchemy-metal-base-sh-predecode-20261109.YZxKNI
passes280base/70ambient/70coefficient+2rawnonfinite comparisons onGPU. Production
stdmetal3.1/ffastmath/min26.0 used. Quiet1024²10cases/7counterbalancedblocks/
14samplesvariant finish0/thermal0: nine medians slower~1–8%; paired ranges cross
zero. No general win; REJECTED/private. Decode~0.005ms/generation kept separate.
Timing-summarySHA10f7f34308e252ff06c2701bee7d19dbe7a266d06387918e8ed969e7cee9d576.
Private avatar palette storage pool CPU374844/4passes/source-reviewed; no
integration or FPS claim. Root prioritizes the confirmed Release build defect.

Apple documents [resource grouping](https://developer.apple.com/documentation/metal/managing-groups-of-resources-with-argument-buffers),
[reusable commands](https://developer.apple.com/documentation/metal/encoding-indirect-command-buffers-on-the-cpu)
and [tile-memory lifetimes](https://developer.apple.com/documentation/metal/tailor-your-apps-for-apple-gpus-and-tile-based-deferred-rendering).
These motivate measured architectural experiments, not automatic correctness or
performance conclusions. Currently sampled intermediates cannot simply become
memoryless.20FPS, broad parity and all product release gates remain open.
