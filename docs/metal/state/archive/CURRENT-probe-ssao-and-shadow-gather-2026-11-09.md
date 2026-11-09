# Probe SSAO parity repair and shadow-gather candidate

Observed2026-11-09 on physical M1 Max/64GiB/macOS26.5.2/Xcode26.6;
branch `codex/apple-silicon-metal`.20FPS Ultra goal remains active and unmet.
The completed viewer window regressed; no net viewer improvement is claimed.

## Original contract and retained repair

Source `0fa4e331333f99401955bc0722929fafb6bdff22` changes only the native
probe-face branch of `LLPipeline::renderDeferredLighting`: pass `has_ssao=false`
to both native light-map and base lighting. Main calls retain the global setting.
Hero and ordinary owner entrypoints both reach this branch; lighting callees
require the corresponding cube/target context. Shadows/casters/cadence unchanged.

Exact original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` pipeline11716
selects `gDeferredSunProbeProgram` for cube captures. Shader manager2091–2102
builds it with class2/deferred/sunLightF.glsl, whose green occlusion channel is
explicitly1. Original pipeline11752 skips cube light-map blur; resource-plan
comments likewise assign SSAO to the main view. Native previously propagated
global SSAO into faces, performed8tap occlusion plus two full-face blur passes,
and consumed that occlusion in reflected lighting. Two independent source
audits and final diff review approved the narrow correction.

Existing native light-map GPU regression passes1992assertions/1case with Metal
API+GPU validation. Doctor/configure/build/verify349ARM64/deep-strict signature
all actualexit0. Initial sandboxed doctor failed on sysctl permission; host
diagnostic rerun0. Actual pipeline response file contains `-O3 -DNDEBUG`.
No shader change in this build: metallib
`8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943`.
App68576 executable
`75a0c59d7b862ba3d4668bd01cf9f9fe4ecc4fe257746386589589a6bfbb26e5`;
identity `artifacts/metal/private-0fa4e3-build-identity-20261109.json`.

Build/test logs `/private/tmp/alchemy-metal-probe-ssao-` followed by
`{doctor,configure,build,verify-app,signature,gpu-release}-20261109.log`.
Preserved measured app `/private/tmp/alchemy-metal-68576-probe-ssao-measured.app`.

## Completed resident sample — regression, not a speedup

Fullscreen shortcut before Return login; exact native binary, pinned location,
Ultra6/Midday/Preset123/3456×2168 and43live controls passed startup and final
observe/verify/gate. All256probes initialized and reached120stable frames before
300consecutive measured frames7623–7922. Initialization, not the login counter,
controlled admission. All samples exact native/zeroGL, desired discard bias1,
no missing/nonfinite bias observations, no sampled camera changes. Main-visible
97 versus baseline98; cross-run camera translation difference5.722e−6m. Controls,
sun/environment and hero/HUD restoration match. Full pixel/geometry parity is
not established. No external scene change is assumed.

| Controlled window | Mean ms | p50 ms | p95 ms | FPS |
| --- | ---: | ---: | ---: | ---: |
|68572/source46d622,bias1|106.585321|105.948188|110.883167|9.382155|
|68576/source0fa4e3,bias1|148.613868|149.572063|156.249375|6.728847|

Mean frame time increased39.43%, FPS decreased28.28%. Mean recorded draws fell
2.874% (55420.33→53827.41); this is not proof of identical pixel work. Formal
image storage7629MiB, image target about45996–46006MiB, device9514–9526MiB,
recommendation59392MiB; system_low0. Budget pressure does not explain this
window. No build, GPU test, capture or profile overlapped formal samples.
Private gather CPU compilation ended during early initialization, more than
ten minutes before formal samples. Native API/shader validation/HUD/capture
variables were unset for collection. Post-sample pmset reported no thermal or
performance warnings, not a proof of identical clock history.

CSV `artifacts/metal/private-0fa4e3-probe-ssao-20261109-01.csv`, SHA
`5678a0b3e307a11a0d4457dcbe3a0aed096ba8e5e5d3fcef31a905d64a1348a7`;
sibling `.provenance.jsonl`, SHA
`3c12eeca04bc1b765effe954ffbb138b07c9c8814f1afc97d4c42dad2abb9afe`.
QA run `candidate-0fa4e3-probe-ssao-20261109-01`. Independent report
`/private/tmp/alchemy-metal-68576-probe-ssao-comparison-20261109/formal-comparison-68572-vs-68576.json`,
SHA `44df65c9b2d5090dcf3a6faaaa6fe457a1d457fcc69c7be2938fd64cf8528c2d`;
FINDINGS SHA `3df1ff6c9f82b4cc4540b6448d231a64704cd0b2426f2ea6c6177ea016a117ac`.
Official perf_compare with `--enforce --require-provenance` actualexit1;
`artifacts/metal/private-0fa4e3-vs-46d622-perf-20261109.json` retains the failed
performance gate. Do not describe code removal as observed frame-time savings.

## Specific GPU attribution after formal collection

One `xcrun xctrace record --template 'Metal System Trace' --attach 38935
--time-limit 15s` completed actualexit0; output saved before exports/parsers.
Directory `/private/tmp/alchemy-metal-68576-regression-attribution-20261109`.
Symbol processing emitted DWARF warnings; no CPU symbol-completeness claim.
Exporter/comparison/union parser each actualexit0. Trace is separate from the
formal CSV:15.738528s,96present requests,6.099681requests/s under instrumentation.

Viewer Active GPU interval UNION143.135660ms/present,87.3082%wall; device-wide
87.8510%. Nominal thermal metadata,9.2588–9.3088GiB viewer allocation. Largest
inclusive encoder context is main initial opaque/post-deferred44.843684ms/p;
main base17.632984, haze17.090225, hero above-water20.167778. Different encoders
overlap; these numbers cannot be added or called individual shader costs.
Main opaque CPU encoding itself0.199080ms/p, not44.84ms CPU submission.

Trace confirms ZERO hero/ordinary horizontal/vertical SSAO blur encoders; main
has97of each. Thus original capture scheduling is observably restored, although
net performance regressed. Historical68566/bias4 is uncontrolled context only:
its main initial opaque group4.945755ms/p was much smaller. No68572/bias1 GPU
trace exists to establish a direct controlled cause. Existing source snapshots
show initial103batches (44fullbright,2shiny,3avatar-alpha,54glow), same category
counts as68566;68572had109with6extra glow. Snapshots precede the GPU trace and
exports lack physical draw/primitive counts. Exact costly draw/resource remains
unresolved. Current full-detail texture residency differs from historicalbias4.

Attribution FINDINGS SHA
`c640b2f9d579b240a063905424b8a93bf10b5a06159c45cbfad7b9e6c3408f46`;
viewer-union report SHA
`2d3c395f6fa1f9aa06a483f18c21af5916a4781711a9e792413c20014dc29c25`.
PID38935 subsequently quit gracefully: exact executable checked before SIGTERM,
Goodbye logged and PID absence verified.0fa4e3startup plan restored afterward.

## Raw shadow gather — tested candidate, viewer performance pending

Exact original shadowUtil.glsl272/327 uses one native textureGather per2×2
footprint. Native helper7431 used four clamped scalar reads for the same texels.
Counts8blocker/12filter and all PCSS math already matched; do not claim fourfold
bandwidth savings. Candidate preserves the original integer lower-left calculation,
then gathers at `(lower_left+1)/resolution`, clamp-to-edge, raw componentX.
Apple MSL specification section6.13 confirms xyzw order(0,1),(1,1),(1,0),(0,0).
Primary PDF/order proof retained privately; no guessed swizzle.

Private `/private/tmp/alchemy-metal-shadow-gather-20261109.u34k2G` preserves
full frozen5dea shader prefix, named helper chain only; two independent source
reviews preceded root compilation. Combined shader
`a75d5217eaa55be9263965bb4fdf26cc374c7f5c7bfd5ebac194205bd80d2f6d`;
correctness host `5e2867035a5161e149498ee726fd74c9e180bfbdbc92f02f2daac0d500da36b8`.
All three compile/link steps actualexit0. After viewer exit and state restoration,
native API+GPU validation session72157 actualexit0:15680raw queries/62720float
components EXACT;96whole light-map outputs EXACT (48RG32F,48actualRG8).
Lit/shadowed/mixed/clear/cascade controls nonvacuous; RG8 population count is not
measured by the diagnostic and must not be interpreted as zero coverage.
Correctness summary SHA
`dcd1bbbbed7396f88ea1206fdb699f8aaf5333fab89e5a80eb4ed8d4ff2b5793`.

Separately quiet timing session18272 actualexit0 after trace processing stopped.
Validation/capture/HUD unset, all thermal0. Six cases/168samples,7ABBAblocks,
14samples per variant and8–20ms common calibrated command buffers; same target
and SAME actual PSO control. Timing host
`b3dab4f6c4bf824312b451615859e9abb916dc968d0428915cdbf538fcb3af90`.

| Case | Original→gather median ms | Paired block range | Faster blocks |
| --- | ---: | ---: | ---: |
|SAME actual PSO,main resolution|8.239687→8.197791|−4.39..+4.25%|3/7|
|PCSS,no blockers,3456×2168/2048maps|3.522225→2.518292|−29.27..−26.21%|7/7|
|PCSS,blocked,same dimensions|8.345281→6.475750|−23.19..−20.24%|7/7|
|PCSS,varied UV/depth/slope|8.245542→6.317240|−24.56..−20.13%|7/7|
|Gaussian unchanged path,1024²|0.724045→0.746849|+2.05..+8.09%|0/7|
|Probe single tap unchanged path,1024²|0.148624→0.164948|+6.20..+15.90%|0/7|

Main PCSS pass medians22.4–28.5%lower; unchanged-path regressions16/23microseconds
per fixture pass remain explicit. Compiler/layout versus measurement cause is
unresolved; no general no-regression or live FPS forecast. Timing summary SHA
`a2ceea312af46faf473762fcf23a2bea2894da050ee83b0c82144afadc4e8418`.
Extended production/nonPOT/16K-edge raw proof completed with API+GPU validation:
23480queries, zero changed components/class differences, actualexit0/thermal0.
Independent review approved.30715f3cde5d6b29e2f0fe974e5d0ef72a7db94f commits the
minimal helper and permanent184line regression. Focused34626/1 and full426213/91
EACH Release/RelWithDebInfo API+GPU suites actualexit0. Initial sandboxedNinja
failed writing the compiler module cache; host retry0. Xcodeconfigure/build0,
verify349ARM64/deep-strict signature0. Build68577 executable
712bee665eefd7b6ca0255460070ba9fd023e0d4b3774310279bab27902d0bba,
metallibffd9e1d178988b0719c516cd1e87444907d59d063630d7fd9ac2d17cafaa9347.
No live FPS result yet: PID58207 launched full-screen before login, startup/end
settings checks are not yet a completed sample. Fresh live43control/location/
binary gate passed;30715fstartupplan installed/notrestored during warmup.

## Product advancement and next boundary

The cube repair closes a real original-contract discrepancy, not the performance
goal. Gather offers a measured general PCSS mechanism with initial exact output
proof. Complete extended footprint proof, integrate focused production test,
then build/GPU-test/review and run a controlled resident viewer comparison.
Investigate main initial opaque GPU cost with explicit scope; do not blame haze,
CPU alone or a specific shader from inclusive labels.20FPS, full visual/feature
parity, multi-scene performance and release gates remain open. Hardware
infeasibility has not been established.
