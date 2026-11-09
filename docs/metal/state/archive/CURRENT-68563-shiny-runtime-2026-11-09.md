# 68563 shiny runtime and incomplete probe warmup —2026-11-09

These are real fixed-protocol samples, **not settled steady-state A/B evidence**.
Both viewers were still initializing ordinary probes after the five-minute
warmup. No scene change is assumed or required to explain that growth.

## Artifact and controlled session

M1Max64GiB/macOS26.5.2/Xcode26.6. Source1863aab70002894b26976f822a9255f82fdf8a4f,
shadercheckpointd1288129991e72b425c7dd60b2932f9fb1214f7b. Release68563:
build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app.
Executable569079724f700563c3a08642ca0d1c620d87ed6686f1525413f82247a3936ddc;
metallib8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943.
Doctor, configure/build, verify_app349ARM64 and deepstrictsignature exit0.
Source recordsOFF; capture/API/shader validation unset during timing. Full app
preserved /private/tmp/alchemy-metal-68563-measured-baseline.app.

Same location, fullscreen beforelogin, Ultra6,3456x2168, Midday/Preset123,
28 live controls.120frames+300seconds warmup,300 consecutive exact-native
submitted rows, zeroGL,frames2684–2983. Main AND frame-end view packs exactly
match corrected68559, same Midday/sun, zero camera changes, all hero/HUD
restorations true. Mean encoded draws54340.177→54300.857(-0.072%).
This is setup comparability; it is not a complete geometry/material census.

| Fixed-protocol sample | Mean ms | p50 ms | p95 ms |1000/mean FPS |
| --- | --- | --- | --- | --- |
| Corrected68559 |123.136859|114.624521|171.057208|8.121045|
|68563 shiny optimization |110.755471|106.643730|143.567583|9.028899|

Observed mean time-10.05%,throughput+11.18%,p50-6.96%,p95-16.07%.
Product target20%/three scenes is NOTmet. No steady-state or sole-shader causal
claim follows from these windows. Stale-space68558 is not a quality baseline.

CSV/JSONL artifacts/metal/private-68563-shiny-20261109-01.*; QA run
artifacts/metal/qa-runs/candidate-68563-shiny-20261109-01. PID3507 gracefully
exited; environment/persistence restored, private68563startupplan cleanupDONE.
CUA login/fullscreen and quit completion summaries sent exactly once perphase.
Quit AXread timedout15sec, but PIDexit and Goodbye/statusstopped verified.

## Separate profile evidence

CPU30sec fullyexited BEFOREGPU30sec started; GPUrecorder fullyexited0 at05:50UTC
BEFOREexports/builds. Never treated a reached-time-limit message as completion.
GPUfinalization took several minutes and remained activeCPU work. All exports/
parsers exit0; metrics /private/tmp/alchemy-metal-68563-metrics-20261109.

Main initial opaque-post per-encoder GPUActive union19.232316→0.661060ms/present
(-96.56%), oneencoder/present each. Both sampled maininitial counts103 and shiny
2draws/600indices. This strongly supports the intended fullhero overwrite skip.
Mainbase16.576614→17.250524,mainatmospheric/alpha16.940104→17.083767 remain costly.
Heroabove-water aggregate12.234914→20.109735 andherohaze6.78863→8.34119 grew;
above-water includes subsequent alpha draws and is NOT an isolated water cost.
Nonhero shadows5.60081→7.52570. Labels overlap; do not sum them into frame time.
Viewer-PID-only disjoint Activeunion110.517256→104.162474ms/present(-5.75%).
Profiled presents7.572→7.471/s are separate instrumented windows, not formalFPS.

CPU mainRunning27.211/30.692sec→27.684/30.852sec(~88.66→89.73% of ONEcore).
New disjoint hero13.466sec48.64%, ordinary1.747sec6.31%, remainder12.471sec45.05%.
Overlapping families:shadowcapture/replay8.788sec31.74%,GBuffer4.597sec16.61%,
cull4.686sec16.93%,nativedraw3.920sec14.16%,texture/binding1.825sec6.59%,
arena/data1.686sec6.09%. These are sampled running activity, not disjoint frame
costs. Do not infer per-frame CPU improvement from fixed-duration sample counts.

## Confirmed warmup confound

Old main counts f808=4,1570=19,2332=33,2713=38,2840=40. New main f811=9,
1573=37,2335=53,2589=58,2716=61,2843=62. Neither warmup reached a plateau.
Within current sampled hero FACE2, f2796→2923 count142→150 while ALL family
batch/index counts match. Old sampled heroFACE5count82 is not a face-matched
comparison. Old13 sampler packs have unique physicalcubeIDs; new run didnot
request a sampler, so new ID-level uniqueness is unobserved.

Sampled main posttail644→661: alphamaterial+3/+33indices,alphaPBR-1/-3,
alphasimple+6/+18,glow+9/+51. Main initial103/shiny600 unchanged. These changes
can confound inclusive unchanged-pass comparisons without proving a scene change.

Manager/registration/scheduler source is unchanged7a→1863. A probe needs an
assigned cube and12 successful scheduled faces before mComplete. Routine refresh
shares the queue and preserves ever-complete state; failures retry. Therefore
wall/frame warmup alone does not establish equal initialized populations.
No source evidence of a new registration/generation duplication leak was found.

## Next acceptance boundary

Measurement-only readiness gate is being implemented in native and external
reference observers: no pending assigned initialization/fades/reset/layout work,
current native pairs,120 stable member frames alongside existing elapsed/frame
warmup. Permit routine refresh, never wait on GPU or change cadence. Sampled
membership/readiness changes fail the run. V2 post-timing material/hero samples
will establish actual candidate coverage; new private base tap masks remain
experimental.20FPS, original-calibration and product parity/release gates open.

FINDINGS.md SHA74b1bfc2473d6ea02f88f91229783b1bb7b7f952c25b6e4f43b7678c22c81219;
analysis-summary.json SHAb4f8562cc581be9d15b37b9ee6bbdb0e6030635958a7a39caf0225fd9f56a68b;
probe-count-series JSON SHA7f5639c782925e2880d5d11d21c89b609a7ee740e9ef63959042a9cd22503021.
