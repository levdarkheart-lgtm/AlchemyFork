# Current engineering state

**Observed:** 2026-11-10, physical M1 Max/64 GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`. **Renderer code checkpoint:** `f549a796d2c734222665dd10c836e56c0df352b9`.
**Working source:** committed water specialization/strict tests/guarded producer.
Renderer source is clean. Canonical app68610/source8eea; active control68608.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal and authority

The unbudgeted20FPS Ultra goal is ACTIVE and unmet. Infeasibility is unproved.
Autonomous quit/build/login/measurement is authorized at the SAME private
location and Preset123 only. No travel or avatar movement. Prefer Mac shortcuts
and fullscreen BEFORE login. Preserve Ultra3456×2168/Midday, six1024 hero faces,
two cascades, PCSS/rawR32/reverse-Z/clamping/casters and all viewer features.
No private uploads/releases. Root owns shared source/docs/runtime/GPU; agents
prepare private source and reviews. Translator FULLY STOPPED/NEVERrestart;
Xcode replay STOPPED. Leave fullscreen GameMode unchanged. NEVER bind CUA
ControlCenter: the unsupported target previously stalled7654s.

## Runtime and current decision

All viewers are CLOSED; both new sessions exited with SIGTERM/Goodbye/PIDabsence
and OWN environment plans restored0. No profiler/build/GPU test is running.
Normal68610/source8eea is canonical and preserved; independent identityfc35passed.
Both full optimized native suites passed482529/104. Fresh baseline comparison
completed; CPU/GPU timeline exports and independent analysis are finalized.

6b80 adds bounded, default-off stage timing. Native focused41331/2 and both
optimized full API+GPU suites476793/104 passed. Normal68608 doctor/configure/
build/verify/deep strict signature all0,349ARM64/0bad. Exact app source6b80,
exe d8f4cc0a397889ec0479dcf6c136b4c194d90a28be2a841c6dc69ddd0d241f51,
metallib3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2.
Actual O3/NDEBUG/C++23/arm64/macOS26.0 and source-recording OFF independently
verified. Preserved /private/tmp/alchemy-metal-68608-stage-window-preserved.app.
[Build, validation and complete run](archive/CURRENT-stage-timing-runtime-2026-11-10.md).

## Latest controlled measurement

68608 captured300 consecutive ready frames5008–5307: **9.069479205FPS**,
mean110.259914307ms, p50109.6861035, p95120.610834. Proper68601 control02 was
9.410236877FPS, mean106.267250553/p50106.547583/p95116.651958ms. New mean draws
were2.814% higher, so neither an isolated code regression nor a gain is proved.
Both runs passed43 start/end controls, exact native/no fallback, Release
3456×2168/quality6/bias1,256 ready/stable120, main-visible98 and restorations.
EEP/sun match exactly; small cross-run camera residuals remain. No FPS gain
has been established in this turn or the preceding eight-hour status window.

After formal collection and60 eligible cooldown frames, three source intervals
5368–5370 produced335 pass reports,334 complete,1 unavailable and9 rejected
prepares. Main base-light fragment elapsed17.34–17.46ms; main haze encoder
13.77–18.01ms (includes later alpha); hero face5 above-water13.17–15.06ms.
The next pass boundary isolates that face's expensive encoder to water draws.
These elapsed intervals overlap and may cross source-frame boundaries; they
are NOT additive GPU busy time, utilization or shader-exclusive frame costs.
Compute/blit/present/pre-frame++ work was not sampled. The formal FPS sample
precedes all stage-counter overhead. No new CPU trace was needed for this run.

Raw run/CSV/provenance: artifacts/metal/qa-runs/candidate-stage-timing-20261110-01
and artifacts/metal/private-68608-stage-timing-20261110-01.csv. Redacted stage,
formal audits and early-depth raw evidence are hash-preserved under ignored
artifacts/metal/private-stage-and-early-depth-20261110/.

## Exact next actions

Early fragment tests were implemented for read-only ordinary post-water SIMPLE
alpha, excluding all depth writers/avatar/query/stencil consumers. Corrected
API+GPU checks passed11598/1, then six1024² strict attachment pairs11753/1.
Quiet224 samples/7 blocks passed14676/1 and thermal checks, but no convincing
benefit exceeded same-path drift. All R4 commands were below the8ms target.
Original fully hidden work was already cheap. No viewer trial or R32 retuning;
all five experimental source/test files were restored byte-for-byte to6b80.
Full tested patch624a611d and raw evidence remain preserved. Ninja artifacts
were subsequently rebuilt for the NEW water candidate. Do not revive early
fragment tests without a new mechanism. [Detailed rejection](archive/CURRENT-stage-timing-runtime-2026-11-10.md).

Committed waterf549 passed both full suites and source-exact normal68610 build.
Exact source8eea2c6895c1c907ce513a5c83872a631874a3bc, exe dc30d261ddbef780c6ffb459c2c87f9d08500785a062e1e834d5a4c49b0f2909,
same39320181library. Preserved /private/tmp/alchemy-metal-68610-water-specialization-preserved.app.
The repeated isolated GPU savings15.42–17.17% are verified; CPU gain is unproved.

68610 formal300 frames4977–5276:9.785333153FPS,mean102.19376125/p50101.9975625/
p95113.184916. Fresh68608 control9520–9819:9.257293251FPS,mean108.02293639/
p50107.8225625/p95118.326458. Candidate+5.704%FPS versus fresh (+7.893% before).
Exactcamera/EEP/43controls/native/bias1/256ready match. Draws+1.616% versusfresh,
main-visible probes95vs100 (98before): retain workload caveat, not code-only proof.
Independent audit129db056 recommends RETAIN;20FPS remains unmet. Both plans restored.

Candidate post-formal CPU:main8.304s/10.554s,hero~50%/shadowparent31%/GBuf21%,
overlapping with symbolization limits. Baseline post-formal coupled trace record/
eight exports0: real CPU samples and2461 matchedCPU/GPU CBs. Target Activeunion
87.60% of matched10.139s, not utilization. Two~0.48s gaps coincide with profiler
resource-metadata workers; do not optimize them as normal frame stalls. Final50977
finds mostly submitted work pending in remaining gaps; GPU probe code is next. Allraw
traces preserved under ignored private-water-viewer-comparison-20261110/.
[Complete results](archive/CURRENT-water-probe-specialization-2026-11-10.md).

A separate main-base constant→device probe-address experiment is PRIVATE only;
it preserves bytes/math/order and uses the same buffer. Mixed uniform loads
may regress. No compiler/GPU/performance proof. Root integrates candidates
serially. A no-mirror water-discard specialization was rejected BEFORE editing:
the measured hero5 really has mirror clipping enabled.

## Retained diagnosis and closed work

Hero views remain about half of sampled active main-thread CPU work. Prior
12-minute trace had main CPU/frame77.6→76.5ms while FPS11.8→10.75; this is not
a current CPU floor. Nominal thermal state and louder fans do not prove clocks
or throttling. Probe inputs grew2→98/main and256/hero, explaining workload
ramp during loading; the remaining later drift is not causally resolved.
Completed-command duration sums overlap/lag and are not frame GPU busy time.

The material cache trial68605 measured9.0667 versus proper68601 control9.4102;
default restored false06d16, normal68606/ac719b12 remains preserved. Parallel
shadow encoding passed native correctness but regressed complete CPU cost;
rejected and removed before6b80. Do not repeat those variants. SH/predecode/
register/scalar-cursor/mask/ambient/SSR/immutable-probe experiments remain closed.
Retain64 examined/32 accepted neighbors, per-view packs, binding cache/arenas,
main occlusion, Ultra4x anisotropy, bounded lights, directR32 slices, borrowed
targets, cubeSSAO and image/global memory accounting. No nativeD32 migration,
quality cuts, epsilon waiver or fallback backend. Historical original67611/
exact1eb observer6.095519FPS is not current full-parity proof.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-004 Resource ownership | doing | Existing completion-safe resources retained; no new net frame win. |
| M-008 Advanced views/effects | doing | Six faces/clipping preserved; actual water hotspot now isolated. |
| M-012 Native UI automation | complete | Exact fullscreen/login/Midday/Preset123 and live gates executed. |
| M-014 Ultra20FPS campaign | doing | Controlled data and rejected early-depth mechanism; water gate active. |

Full pixel/feature parity, representative live scenes, prolonged stability,
strict no-GL and independent build/review evidence remain OPEN.20FPS is neither
achieved nor proved infeasible. Keep the goal ACTIVE.
