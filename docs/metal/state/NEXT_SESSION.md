# Next session — live68559, active20 FPS goal

Read AGENTS.md, CURRENT, the ledger and relevant skills. The goal is already
ACTIVE; do not create another. Automatic build/quit/login/measurement is
allowed. Stay at the current location/Preset123; no avatar travel. Prefer Mac
shortcuts. Preserve original quality, cadence, views and casters. Architectural
changes require correctness evidence.20 FPS is not met; no release/upload approval.

## Current checkpoint — all viewers closed

Release68559/source7a2639ea4d2237f0ed9b935ea4fee983f418347d is built/verified,
app build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app.
Executable219e7df316edf16f9f6ef3665e43c90bfc6d6ba45085c192389c25d0c0a5f98e;
metallib5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1 unchanged.
All viewers/profilers CLOSED, user environment/persistence cleanupDONE in
artifacts/metal/private-68559-startup-setup-plan-20261109.json. Historical60797
is exited. Current app EXCLUDES uncommitted matrix memo integration.

68559 run complete,300exact-native frames at same28controls/fullscreen/Ultra6/
Midday/Preset123. Mean123.136859ms/8.121045FPS,p50114.624521,p95171.057208;
68558mean99.366256ms/10.063779FPS. Correctness repair RETAINED: now13distinct
spatial packs, matched probe centers align across faces; stale data is not a win.
GPUunion110.517ms/p vs86.393; initial opaque-post19.232ms (103records unchanged),
hero water12.235ms vs6.118. CPU probe refresh only89ms over30.4s; no full causal
attribution.48slowframes≥150ms clustered; physical draws+0.387%. See latest
[record](archive/CURRENT-68559-probe-view-runtime-2026-11-09.md).
Raw/exported metrics /private/tmp/alchemy-metal-68559-metrics-20261109.
CSV/provenance artifacts/metal/private-68559-startup-20261109-01.*;
workload13records artifacts/metal/private-68559-probe-workload-20261109.
All exports/analysis completed; no active measurement window.

The matrix memo candidate was REJECTED: Release252/4 correctness tests pass,
hidden640/1 benchmark pass, but repeated/null100k MVP455.5→705us,inverse757.75→
1064.08us; all-unique inverse769→1911.29us. Root removed only the uncommitted
pipeline/CMake integration, keeping7a intact. Prototype and SHAmanifest:
/private/tmp/alchemy-metal-matrix-memo-prototype-20261109. Logs
/private/tmp/alchemy-metal-matrix-memo-{final-release-build,release-unit,release-cpu-benchmark}-20261109.log.
No RWDI/product acceptance work is needed for that rejected experiment. Do not
restore it without new evidence. Current app has no matrix memo.

Next diagnostic: trace_recheck implements NEW almetalframecapture.h/.mm and
 tests/almetal_frame_capture_test.mm ONLY; root owns CMake/llviewerdisplay.cpp.
shadow_contract independently reviews on freeze. Default off, explicit existing
owned0700 directory ALCHEMY_METAL_FRAME_CAPTURE_DIRECTORY. Capture all queues of
window device, arm after timing true→false and60eligible normal main frames.
Root eligible flag ALSO requires successful recorder Result::complete, startup
ready and normal main (not nested/cube/snapshot). Helper must observe timing
while eligiblefalse so it can arm before success. Stop at NEXT ELIGIBLE entry,
BEFORE commands; includes display_end_frame/ordinary probes/idle uploads. Only
commands created after start and committed before stop are captured. No waits
or submission changes; fail isolated/noexcept/only stop own capture.

MTL_CAPTURE_ENABLED=1 must precede device creation and has diagnostic overhead;
use separate CAPTURE-ONLY session, not formal FPS comparison. Root adds default-
off shader-source embedding CMake option(-frecord-sources) for Xcode. No helper
build/capture success yet. Latest Xcode26.6 lacks gpucapture/gpudebug/metal-dsymutil;
inspect .gputrace in Xcode via CUA, same GPU/OS. Stop profiler finalization before
new recordings. CUA completion summaries already sent; rewriteDocumentation()
after compaction before new UI. All user startup state remains restored.

## Confirmed correctness repair

All13 different-camera68558 dumps shared identical49152B spatial probe data,
while env_matrix varied. Independent exact1eb audit CONFIRMED the native face
return skipped original per-view updateUniforms. The existing CPU routine
transforms centers/boxes, sorts eye-depth, reindexes neighbors and buckets, and
applies snapshot ambiance/radiance scales.7a2639ea4d restores both original
CPU update calls under deferred level>1 before capture. No shader, quality or
cadence change. The later main branch refreshes main data as before.
Counts need not stay67: original snapshot views exempt main occlusion. Verify
fresh per-face data and selection before interpreting performance; the previous
face workload used stale coordinates. Do not treat it as a correct-quality baseline.

## Latest accepted68558 result and reusable tools

Source5a99ceb9ec/productd0424ea42d: mean99.366256ms/10.063779FPS,
p5098.375104,p95114.118750. Previous68556:100.613020ms/9.939072FPS.
Observed throughput+1.2547%,p95 unchanged; not entirely source-isolated.
Constants reuse47.1109%. CPU94.1085% of one core, hero50.07% of main samples.
Viewer GPU union86.3931ms/present, essentially unchanged.
Only metal-system-clean.trace is accepted; two earlier GPU attempts overlapped
profiler finalization and are INVALID. Wait for the RECORD PROCESS to actually
exit before starting another trace; time-limit reached is insufficient.
Finalization can use several CPU cores for minutes. Never move an active trace.
68558 viewers/profilers are closed and user state restored. Complete apps:
/private/tmp/alchemy-metal-{68558,68556}-measured-baseline.app, plus68554 baseline.
[Full runtime record](archive/CURRENT-68558-constants-and-probe-view-2026-11-09.md).
Metrics: /private/tmp/alchemy-metal-68558-metrics-20261109. Reusable private
export_metal_trace.py, compare_cpu.py, compare_gpu.py, gpu_detail.py and CSV parser.

Ordered initial lists are NOT enabled. Main initial search is only2.285
candidates versus29.281 neighbors (92.76% of examined candidates). Lists save
0.321 initial checks but add4.696 cursor steps and2.31MB/view, with38.72 IDs/tile.
Dense prototype regressions therefore matter.13 records pass ABI/model checks;
offscreen figures are stale-data-sensitive. Analyzer:
/private/tmp/alchemy-metal-probe-workload-analysis-20261109.SMuSRjoH/analyze_probe_workload.py.
Run only AFTER recording windows end with the new directory and
--source-sha 7a2639ea4d. It refuses overwrites. Wake trace_recheck with
followup_task for analysis after release; shadow_contract handles comparisons/review.

## Rejected candidate evidence

Matrix source/review/execution results are above; no live app contains it.
Lazy terrain/palette scratch remains a source-only audit:
/private/tmp/alchemy-metal-cpu-preparation-audit-20261109.md. Full material-pointer
caches are unsafe for streaming/media/cutoff changes. Root owns all shared files.

5a99 source passes BOTH full API/GPU suites121201/85, benchmark644/16 and
focused sampler53910/4. Doctor M1 Max64GiB/macOS26.5.2/Xcode26.6 passes.
7a changes only the pipeline producer; product compilation/review pass, live
validation is complete. Exact original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45,
external OpenGL67607/validation734afd3179; not /Applications/Alchemy Beta.
Occlusion/Ultra4x anisotropy are already fixed. Preserve bounded lights, direct
R32 shadows, borrowed depth, projector sharing, arenas and retired geometry.
Texture-pressure separation7cd1993d8e stays reverted; discard bias4 observed.
Continue the goal after this capture; no20 FPS, parity, GL-strict or release claim.
