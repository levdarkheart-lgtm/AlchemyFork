# Current engineering state

**Observed:** 2026-11-09, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `4e833fa29aadabee3a66023fb67da0975f6b6c7f`.
**Working source:** normal68597 live; actual selected probe specialization confirmed.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal, authority and runtime

Unbudgeted20FPS Ultra goal ACTIVE/unmet. Autonomous quit/build/login/measure
is authorized at the same private place/numericPreset123 ONLY; no travel.
Prefer Mac shortcuts/fullscreen before login. Preserve Ultra3456×2168/Midday,
six hero1024 faces/two cascades, PCSS, reverse-Z/clamp/casters/viewer features.
No private uploads/releases. Root owns shared source/docs/runtime/GPU; agents
prepare/review private source only. User explicitly handed Xcode back to ROOT.

All viewers CLOSED; exact50766 path/hash/SIGTERM/Goodbye/absence29248=0,
continuous-startup plan RESTORED0/allolderplans restored. Translator FULLY
STOPPED/NEVERrestart; Xcode GPU replay STOPPED. Root owns source/runtime/UI.
Normal68597/source6f6207698720bc4455c43f68e007715135643074, renderer4e833,
exe47670056.../metallib61c5dcb3... verified; doctor/config/build/verify25454 all0,
independent349ARM64/deepstrictidentity0. Currentapp preserved; no shaderchange.
User-requested continuous TimeProfiler14305 actual0,12m from BEFORElogin;
920867CPU-active samples/596968main. TOC33140/export16452/thermal60378 each0.
Main sampled activity92%→79% ofonecore; world onPcores. Approx aligned CPU/frame
77.56→76.46ms as actualFPS11.78→10.75 (30s bins; UTC/flush alignment~1s).
CPU work did not ramp upward. Thermal Nominal ALL720.726s; fans louder is not
proof of throttling. Clock sampler unavailable(requiresroot/sudo-n1); clocks
unknown. GPU completed-CB spans grow with probe packs; sums overlap, notactive
GPUtime/criticalpath proof. CPU serialcost still needs reduction for20FPS.
6000rawframes ALLnative1/fallback0 complete (527.46s), freshstart/end43gates0.
Raw logging stops before12minCPUtrace; laterbins usewidget/process/logdata.
Mean11.375FPS; first10s12.35/43.5kmedian draws, later10.71/~52.4k. DIAGNOSTIC,
no formal gain. StartupreadyobserverOFF, do notinterpretfalsefieldsasmissing.
Prior68597 ready300DIAG8.159FPS is separate; no causal samebuildcomparison.
Both optimized API+GPU428004/95PASS. Live selected eligibleheroPSOs true;
staticbulkexecute only2.22%ofmainCPU =>defer bulk-only baseInstance/parallelwork.
Rootcorrelation report4e46f5c4/CPUfullanalysise50ce5df inpacket3a9on15l (NEXT).
Startup6000 raw frames complete:12.258→11.038FPS first10/next10s, median draws
44,631→50,697, early camera unchanged. Later main probe pack2→98/hero up to256
while several fixedfaces retain post family/index counts. Correlation, not
causal GPUtime. Bias1/system_low0. Widget observed32→11 then11/12/later10;
owner clarified start~38 this time, starting value varies. Do not assume160.
NativeCPU sample exit0 but only2main-thread observations/58DWARF warnings:
unusable for attribution. Explicit Mach-unit correctione0656051 preserves raw
counters; no font hotspot/normal CPU-time claim. Viewer UI recovered afterward.
Committed selected-PSO census4e833; both optimized API+GPU suites428004/95 PASS.
First sandbox test failed42/no Metal device; native rerun0, distinct logs kept.
No shader/quality changes; diagnostics armed only by existing command timing.
Continuous recording captured startup decline; CPU no upward ramp, per-frame
CPU cost remains~77ms. GPU causal attribution and actual CPU repairs remain.

## Controlled viewer baselines

New68594/sourceb299 formal300 frames7949–8248: **9.635581285FPS**,
mean103.782010697ms/p50103.661375/p95nearest108.9555. Best68579 **9.608672762FPS**:
mean104.072646/p50103.8956665/p95109.0435; newmean−0.2793%/FPS+0.2800%, effectively
FLAT. Latest68581 **9.454638960FPS**/mean105.768185. Single pairs not causal;
physical draws differ~3.8%, olderbaselines lack64neighbor repair. Newformal
all300 consecutive/native1/Ultra3456×2168/bias1/256ready/stable120; start/end43
freshgates0. Four camera changes occurred ONLYwarmup; samplecamera/basis/EEP
fixed, same-view2898→3197. Full endshutdown/restore verified, no claimedlivegain.
[Independent report](/private/tmp/alchemy-metal-68594-formal-independent-20261109.p5xjUcY1/independent-formal-summary.json).

Earlier68577 8.011210/68576 6.728847 had Translator GPU contention; do not
attribute their difference to code. Original67611/exact1eb+observer6.095519FPS;
full parity remains open. [History](archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Actual GPU diagnosis

Diagnostic GPU union105.542ms/p; inclusive mainbase17.33/heroabove20.71/
mainhaze15.91 overlap: DO NOT ADD. Old CPU trace has no matched frame counts;
no77ms CPU floor. Focused mainbase216 registers/0spill,7.177M fragments,
37.46B ALU, ALUlim44.54%; mainbase occupancy still unknown.

New0d5d49 hero face1 capture7506:1CB/4render/4blit/833draws/1024²RGBA16F,
nonempty raw-cube preview. Profile15.74ms MEDIUM is one face, NOTliveFPS.
Simple-shadow alpha58.02%/245registers/304spill bytes; water20.39%/254/96;
PBR alpha15.22%/254/80, legacy alpha1.21%/254/32. Haze FS only1.79%/64/0;
589-draw haze ENCODER77.867% includes later alpha. Encoder occupancy4.19%,
1.6607MFS, overdraw1.58, PreZfails55.29%,1.4197BALU; high registers/spill
correlate with low occupancy but causality requires A/B. No DRAM saturation
or late-Z count claim. Static spill bytes are NOT per-fragment memory traffic.
Call Stack attributes71.05% to base-probe sampling,28.93% to holds; optimized
source/inclusive percentages overlap and are not literal call edges. Branch
5525 shows11.50%; declaration97.32% is not a single-instruction cost.
Private profile JSON/capture paths and full counters are in NEXT/archive.

## Active implementation checkpoint

Committed4b23 repairs exact-original64 candidate/32 ACCEPTED neighbor traversal
in three walkers (old31 truncated valid later probes). Red late32/64 failures;
green9161 assertions, both optimized API+GPU suites427464/98, independent12-case
all-walker oracle pass. Normal68588 built/verified/preserved; no FPS gain claim.
Committed0d5d49 adds defaultOFF selected hero-forward capture ownership. BOTH
capture suites3059/3 and normal API+GPU427476/95 pass. Diagnostic68590 ran300
frames/all256 ready/stable120/camera0/fresh43 gates0 and captured face1.
479ff diagnostic compiler/dependency repair retains optimization and source-line
metadata; full history/verified hashes in the archive. Normal builds recordOFF.

Committed735a: optional Metal bool function constant0 folds the EXISTING single-tap
shadow branch; main/default shaders retain runtime behavior. Stock simple-alpha
PSOs now none/runtime/probe, custom blend cache includes mode. Selection checks
ACTUAL validated bound shadow bit0x4 plus actual probe count>1; no view-label
assumption. Constants cleared before other families; avatars remain runtime.
Source independently approved twice. No sampling/caster/quality/math change.
Production MSL920d2e2d927b94a43cd396478bfa3776bc2edccaf5e5dd3ef8eb10bbbfcb1fa1.

Actual735a function-constant fixture:200 strict color/rawD32 pairs PASS;
168 quiet7-block timings show heavy23.2–31.8%lower, control/default ranges cross0.
TinyactualXcode capture1CB/4draws:245→224registers, BOTH304spill bytes. No spill
elimination/liveFPS claim. Bothoptimized publictests pass; full paths/archive.

Public renderer regression patch applied after source/peer review: local
checkerboard raw shadows, complete RGBA16/rawdepth count1vs2 equality,
rigid/skinned and standard/custom blend, main→probe→main isolation with actual
PCSS-vs-single-tap positive controls PASS in BOTH optimized suites. Next small
normal68594 formal result is flat as recorded above.
[Full evidence/paths](archive/CURRENT-shader-registers-and-neighbor64-2026-11-09.md).

Ambient-only sum rewrite REJECTED:200 strict pairs pass but168 timing samples
show no consistent benefit. Prior SH lifetimes/predecode/named accumulators/
mainbase specialization/materialsplit/layered/scalarcursor/simpletilemask stay
rejected; do not repeat unchanged. Live Ultra SSR=0, so private SSR miss-path
win does not address this scene. Ordinary SCENE batch proposal remains private;
commit BEFORE separate face publication, preserve generation/failure ordering.
Agents may prepare independent PBR/water specialization fixtures source-only;
do not combine before this simple variant's controlled viewer comparison.

## Exact next actions

Retain image/global memory accounting3969405+46d622(bias1), binding cache,
per-view probes, shiny bypass, real main occlusion/Ultra4× anisotropy, bounded
lights/direct R32 shadows/borrowed targets/depth/arenas/retirement/cube SSAO.
NativeD32 differs from raw-depth reference: no migration/epsilon waiver. Cube
occlusion needs per-face current depth and conservative ownership/invalidation.
Xcode Color0 Auto expands intermediate linear HDR; original final order/one
gamma remains, no proven look defect/settings change from that observation.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-004 Resource ownership | doing | Image/global pressure repaired; broader lifecycle open. |
| M-008 Advanced views/effects | doing | Probe shader specialization validated; live result flat, mirror cost open. |
| M-012 Native UI automation | complete | Fullscreen/login verified; root controls Xcode again. |
| M-014 Ultra20FPS campaign | doing | Best controlled9.64FPS; specialization comparison flat, attribution open. |

Full pixel/feature parity, multi-scene performance, prolonged stability,
strict no-GL and independent build/review evidence remain OPEN.20FPS unmet;
infeasibility unproved. Next: attribute startup growth/actual selected PSOs;
Selected-PSO diagnostics4e833 validated; fullstartupCPU evidence captured.
Next GPU growth attribution and scalar covered-shadow draw reduction prototype. Corrected64-parent68588 control remains prepared
but UNLAUNCHED; prior best68579 has31-neighbor behavior, so not a pure A/B.
