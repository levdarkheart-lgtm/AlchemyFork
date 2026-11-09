# 68559 per-view probe repair — observed runtime,2026-11-09

Physical M1 Max64GiB/macOS26.5.2/Xcode26.6. Source-exact renderer/product
7a2639ea4d2237f0ed9b935ea4fee983f418347d, Release26.4.0.68559. Only the original
per-view CPU probe preparation was restored; shader math/quality/cadence unchanged.

## Build and controlled setup

Configure/build exit0; verify_app349 ARM64 slices and deep strict signature pass.
Executable219e7df316edf16f9f6ef3665e43c90bfc6d6ba45085c192389c25d0c0a5f98e.
Metallib5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1 unchanged.
Logs /private/tmp/alchemy-metal-probe-view-fix-{configure,build}-20261109.log,
/private/tmp/alchemy-metal-68559-{verify-app,codesign-verify}-20261109.log.
Source/fix independently reviewed. The underlying5a99 Metal suites remain
121201assertions/85cases in both Release/RWDI; this commit changes pipeline
preparation only, tested with source-exact build and real view-data validation.

Run artifacts/metal/qa-runs/candidate-68559-startup-20261109-01, PID60797
(now EXITED). CtrlCmdF before Return login, same location and numeric Preset123,
Midday native startup,28 live controls/location gate passes.3456x2168/Ultra6,
120frames/300seconds warmup,300exact-native/submitted frames2725–3024,zero GL
fallback. Local/effective sky and sun match68558, no camera changes, all hero/HUD
restoration checks true. Crosslaunch position approximately39micrometers,basis
max1.014e-6, modelview max1.097e-4; projection/focusTarget unchanged. No screenshot
parity claim. Temporary environment file/persistence restored after graceful exit;
artifacts/metal/private-68559-startup-setup-plan-20261109.json records cleanup.

## Unprofiled results — regression, not a speedup

| Build | Mean ms | p50 ms | p95 ms |1000/mean FPS |
| --- | ---: | ---: | ---: | ---: |
|68558, stale face data |99.366256 |98.375104 |114.118750 |10.063779 |
|68559, repaired face data |123.136859 |114.624521 |171.057208 |8.121045 |

Mean+23.922%,p50+16.518%,p95+49.894%,throughput−19.304%. The correctness repair
is retained; restoring stale coordinates to gain FPS would violate the goal.
Do not attribute all change solely to the CPU builder: shading inputs changed,
and the previous version used incorrect face-space data. Goal20FPS remains open.

Mean physical draws54130.640→54340.177 (+0.387%).48/300frames≥150ms;40 occur in
frames2875–2924, whose50frame mean is162.508ms. Other blocks111.397–120.002ms.
Old run had no frame≥130ms. New frame-time/draw-count Pearson−0.0668; geometry
counts do not explain the tail. This is not function-level causality evidence.
Main post census remains644records/103initial, identical family/index counts.

Settled16second interval: main536836bindings/250624reuses(46.6854%),probes
911699/437133(47.9471%),combined47.4795%. Pool9/292.966MiB,driver4575.33→4546.64MiB,
discard bias4. No quality/caster/view cadence reduction. perf_compare with
provenance exits0; complete release targets/multi-scene coverage remain false.
Summary /private/tmp/alchemy-metal-68559-metrics-20261109/csv-provenance-counter-summary.json,
SHA25638d53f556fa911a07c6c006bc296711babe1228e341d43fee10dd9642e2befef.

## Independent CPU and GPU windows

30s CPU record fully finalized/exited before the30s Metal System Trace started.
No builds, prototypes, exports or prior profiler finalization overlapped either
window. Post-CSV sampler had13completed manifests before recording. All viewers
and profilers now exited; exports/analysis occur afterward. Raw data stays local:
/private/tmp/alchemy-metal-68559-metrics-20261109.

CPU Running27211ms/30.431s≈89.4187% of one core (old94.1085%). Strict hero13853ms,
ordinary1678ms,remainder11680ms. Overlapping families shadow9039ms,cull4655ms,
arena/data1582ms. Reflection updateUniforms89ms (old10ms), getReflectionMaps8ms,
syncToViewerObject32ms. These categories overlap; no additive/per-frame claim.
The CPU trace does not support attributing the full frame regression to the
preparation call. It also does not directly sample the earlier CSV spike window.

Target-viewer GPU Active union86.3931→110.5173ms/present; whole-device union
88.6164→112.3660ms includes other processes. Clean trace only, not summed command
spans. Main initial opaque-post encoder approximately0.36→19.2323ms; main base
17.1649→16.5766ms; main atmospheric aggregate16.7642→16.9401ms (includesalpha/glow).
Hero labelled aggregate35.4718→44.3992ms; hero above-water surface6.1180→12.2349ms.
Encoder CPU42.9002→44.3316ms/present; submissions29.4066→29.5905. ThermalNominal,
allocation4.3914–4.4795GiB. Traced presents7.8260→7.5720/s; use unprofiled CSV for FPS.

Main initial-post0.3/20ms variability is historical even across unchanged-shader
runs. Counts match but per-draw GPU/attachment attribution is unresolved; do not
claim one shader consumes the entire labelled increase. A default-off native
Metal frame capture is under investigation for actual draw/resource inspection.
GPU union summary SHA2561b5e370e43b71a508a4cdfa08bfb4bcf752612e3da40170cade18fce20c3161e.

## Per-view correctness proof and selection workload

13 completed records (main+6hero+6ordinary) each have a distinct49152B spatial
probe pack; old13records had one identical pack. New main count44, each face88;
old all67. Counts are not pinned because original offscreen views exempt main
occlusion. Matched probes use physical cube layer, never changing logical index.

Rotate eye-space centers using captured env_matrix, then remove common camera
translation per view pair.87 shared nondefault probes across all6hero faces have
ZERO residual; ordinary↔hero max1.1444e-5,main↔hero43shared max4.1438e-5,zero radius
changes. Old cross-face residuals278–471 units (main↔hero315.757). Both versions'
within-pack depth metadata and reconstructed buckets match, so cross-view
rotation is decisive. Dumps lack absolute camera origins/modelviews; this proves
relative spatial coherence, not full image parity or absolute world origins.
Comparison SHA2566334392ee153b77be04db66d08552a196468cf988a94d80e295618e9e8092061.

CPU selection replay matches all modeled order/default/conservative decisions.
Main geometry samples unchanged2453(2027PBR/426legacy). Main initial1.945→1.782
with ordered lists, but adds4.507cursor advances; dense33.81IDs/tile. Corrected
hero lists68–87IDs/tile, initial7.133→6.537 while adding27.624cursor advances;
ordinary12.416→11.203 with16.519extra advances. Neighbors unchanged, containment
reductions only~10–12%. Six ordinary box/default suppressions preserved; no cap32
observed. Broad ordered-list admission remains rejected. These are CPU replay
estimates, not GPU instructions/texture-fetch counts/FPS predictions.

## Rejected CPU matrix memo and next boundary

The bounded matrix memo was integrated as a working-tree candidate, independently
reviewed, then measured before acceptance. Initial compile failed because the
math SIMD header prerequisites were omitted; include order was fixed. Release
unit252assertions/4cases and hidden benchmark640/1 pass, exact bytes/statuses.
However every measured workload regressed. Seven counterbalanced100k-operation
repetitions: repeated/null MV/MVP455.500→705.000us(+54.8%), with inverse757.750→
1064.084us(+40.4%); grouped inverse791.875→1013.084(+27.9%); all-unique inverse
769.000→1911.291(+148.5%). Direct SIMD computation is cheaper than key comparison,
cached copies and call overhead here. No FPS gain is claimed.

Root removed ONLY this uncommitted pipeline/CMake integration; committed7a
correctness repair remains. Source/test prototype preserved with SHAmanifest in
/private/tmp/alchemy-metal-matrix-memo-prototype-20261109. Logs
/private/tmp/alchemy-metal-matrix-memo-{final-release-build,release-unit,release-cpu-benchmark}-20261109.log.
No RWDI/product/runtime work is needed for the rejected candidate. Existing
newview_tests binary still contains that isolated experiment until next regen;
it is not the production app. Do not restore the cache without new evidence.

Next: default-off one logical display interval MTLCaptureManager capture,
inspected locally in Xcode. Directory explicit/private/no-overwrite; start only
after successful CSV completion plus60normal frames, stop before next normal
main display entry. Device scope includes all queues and ordinary probe update
outside display; nested snapshot/cubemap calls are not boundaries. Only commands
created after start and committed before stop enter the capture. No GPU waits or
submission changes. MTL_CAPTURE_ENABLED=1 at launch adds diagnostic overhead;
use a separate capture-only session, not a formal FPS result. Agent implements
new helper/test files; root owns CMake/display hooks and diagnostic shader-source
embedding option. No capture implementation/build/execution success yet.
