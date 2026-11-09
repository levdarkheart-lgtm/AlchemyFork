# 68558 constants runtime and probe-view defect —2026-11-09

Physical M1 Max64GiB/macOS26.5.2/Xcode26.6. Renderer source5a99ceb9ece582728339920658f1f0e75e6041e4,
product HEADd0424ea42d (docs-only descendant), Release26.4.0.68558.

## Build and validated setup

Doctor/configure/build/verify_app all exit0, deep strict signature passes349
ARM64 slices. Executable SHA256d4cb86d91b2b0c8c4b8914056572c187a4bd5265bef4ea36395dba144c439a33.
Production metallib5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1
is byte-identical to68556/68554/68550. No production shader change.
Build logs /private/tmp/alchemy-metal-5a99-product-{configure,build}-20261109.log;
verification /private/tmp/alchemy-metal-68558-{verify-app,codesign-verify}-20261109.log.
Both full Release/RWDI API/GPU suites121201assertions/85cases, both benchmark
644/16; focused sampler53910/4 pass. Source/integration independent review passes.

Exact full bundle preserved /private/tmp/alchemy-metal-68558-measured-baseline.app;
68556 also preserved /private/tmp/alchemy-metal-68556-measured-baseline.app.
Running testPID43625 is now EXITED. Temporary native environment file and
persistence preference restored after verified graceful exit; private plan
artifacts/metal/private-68558-startup-setup-plan-20261109.json.

Run artifacts/metal/qa-runs/candidate-68558-startup-20261109-01 passes28 live
binary/location/settings controls. CtrlCmdF before login then Return; native
startup loader applies numeric Preset123 and Midday. All sample snapshots show
selected_environment1, local/effective sky UUIDc46226b4-0e43-5a56-9708-d27ca1df3292,
finite sunlight,3456x2168/Ultra6. Intentional infinite reverse-Z far is explicitly
valid.120frames+300seconds warmup,300exact-native/submitted rows,zero GL fallback.
No avatar movement or quality/view/caster/cadence reduction. No screenshot parity
claim: prior CUA world timeouts remain; this run verifies startup/runtime state.

## Unprofiled comparison

| Build | Mean ms | p50 ms | p95 ms |1000/mean FPS |
| --- | ---: | ---: | ---: | ---: |
|68554 |102.678943 |101.875709 |119.073875 |9.739095 |
|68556 |100.613020 |98.968667 |114.090125 |9.939072 |
|68558 |99.366256 |98.375104 |114.118750 |10.063779 |

68556→58 throughput+1.2547%,mean−1.2392%,p50−0.5997%,p95+0.0251%.
This is a small observed change, not source-isolated proof or20FPS attainment.
Both sidecars28records,zero camera changes,allhero/HUD restorationtrue. Across
launches only38.1micrometers cameraZ/focus drift; direction/projection/focusTarget
exact. All28 asserted settings match68556;21common settings match68554.

perf_compare.py --require-provenance exits0 for both comparisons; the overall
release target remainsfalse with3missing workloads. Raw68556 scenario differs;
a derived alias changes ONLY scenario to historicalpbr-interior with hashes,
raw originals retained. Files/summary/parser under metrics directory:
/private/tmp/alchemy-metal-68558-metrics-20261109.
CSV/provenance/counter summary SHA256ff4c6d1a7b78868299db25538c84385e83d9b926d8825150aa1081979763329e.

Two settled intervals02:13:23–47UTC inside CSV: main1079192binds/497656reuses
(46.1138%); probes1863689/888761(47.6883%); combined47.1109%. This avoids1386417
400B publications,554566800raw bytes of repeated publication input. Slot4 STILL
binds on reuse. These are counts, not bytes of actual system-memory traffic.
Driver4562.62→4560.06MiB,pool9/292.966MiB,discard bias4.

## Separate traces and limits

30s CPU recording, then clean30s Metal System Trace; no build/GPU experiments
in the timed windows. Two earlier GPU attempts overlapped preceding profiler
finalization and are INVALID, retained with invalid-gpu-attempts.json. Agent
started a later recording before confirming prior process termination; this
was corrected by verifying all profiler exits, then recording metal-system-clean.trace.
Only that clean trace supports GPU comparisons. Instruments finalization took
longer than recording and emitted DWARF warnings; CPU symbol attribution is
statistical/incomplete. Valid CSV timing precedes every trace and dump.

Main CPU93.9617→94.1085%of ONEcore; strict hero14378→14422ms of fixed30.6s
sample windows, ordinary1730→1709ms,remainder12664→12672ms. Overlapping families:
shadow capture/finish/replay9675→9337ms,cull5115→5185ms,arena/data1882→1685ms.
Do not sum overlapping categories or infer per-frame savings from fixed windows.

Clean GPU: target-viewer active interval union86.7047→86.3931ms/present, essentially
unchanged. Whole-device union88.7509→88.6164ms includes other processes.
Main base17.2182→17.1649ms,main atmospheric aggregate17.1388→16.7642ms (includes
alpha/glow). Hero labelled aggregate35.5471→35.4718ms. Encoder CPU43.9326→42.9002
ms/present; pre-encoder preparation excluded. Submissions29.3386→29.4066.
Thermal Nominal; candidate allocation4.3545–4.4831GiB. Traced presents8.2910→7.8260/s;
profiling changes pacing, use unprofiled CSV for FPS. CSV metal_gpu_ms sums
command spans and is not GPU wall duration. Channel/encoder durations overlap.

## Actual probe workload and architectural decision

Opt-in sampler remained armed/no dumps until CSV ended;13completed private
records,main+6hero+6ordinary. Sparse64x40 depth/normal classifications and exact
probe constants, not fullimages. ABI/order/conservative model checks pass;
10/15844geometry samples near floating boundaries. CPU replay is an estimate,
not hardware instruction counts, exact texture-fetch counts or FPS forecast.

Main geometry2453points: original initial2.285candidates and29.281neighbor entries
per point; ordered-list model1.964initial andsame neighbors. Neighbor traversal
is92.76%of examined candidates. Containment31.567→27.346(−13.37%), but initial
savings0.321 comes with4.696cursor advances and~2.31MB extra per-view data.
Lists average38.72IDs/tile,p9546,max47;23camera-plane fail-open spheres+4boxes.
No observed cap32/box acceptance in this main sample. Dense-case prototype
regressions apply; do NOT broadly enable ordered initial lists.
Private interpretation SHA256dfcb0460b230a3d6aee639a828b77f873af93e1cb03c4621016aa78ae8677a4a.

## Confirmed per-face correctness discrepancy and repair

All13records shared BYTE-IDENTICAL first49152probe bytes (spatial/order/neighbor/
bucket pack), while current-view env_matrix varied. Source and independent review
confirm native renderGeomDeferred earlyreturn skipped original per-view updateUniforms.
Exact1eb pipeline5117–5118 calls reflection and hero updates per view. Current
manager2147+ is CPU preparation: getReflectionMaps sorts current-eye depth,
reindexes neighbors;2241 transforms spheres;getBox usesinverse(object*CURRENTview);
2261 applies snapshot ambiance/radiance scales. Nativefill1140 merelycopies
old spatial pack and constructs freshcurrent-view env_matrix. Metal reconstructs
position in CURRENTeye space and directly compares to sphere/box coordinates.
This is a genuine mismatch, not equivalent coordinate spaces.

Separate correctness commit7a2639ea4d restores BOTH existing update calls under
original deferred-level>1 guard before native-face capture. Entirepack rebuilt,
not just spheres. No shader math/quality/cadence change, no new GL authority.
Main later branch rebuilds mainpack asbefore. Independent11line review approved.
Source-exact product build/post-fix runtime pending. Counts can legitimately
change because original snapshot path exempts main-view occlusion; don't pin67.
Earlier face performance/workload is stale-data-sensitive. Verify fresh per-face
spatial packs and original transforms/order/scaling before interpreting speed.
