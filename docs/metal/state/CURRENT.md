# Current engineering state

**Observed:** 2026-11-09, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `5e1ecb76459de8a0bc90dc16da56aec75f1fc02d`.
**Working source:** normal68601 built/verified; covered-shadow live benefit unmeasured.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal, authority and runtime

Unbudgeted20FPS Ultra goal ACTIVE/unmet. Autonomous quit/build/login/measure
is authorized at the same private place/numericPreset123 ONLY; no travel.
Prefer Mac shortcuts/fullscreen before login. Preserve Ultra3456×2168/Midday,
six hero1024 faces/two cascades, PCSS, reverse-Z/clamp/casters/viewer features.
No private uploads/releases. Root owns shared source/docs/runtime/GPU; agents
prepare/review private source only. User explicitly handed Xcode back to ROOT.

All viewers CLOSED; exact50766 shutdown/Goodbye/absence29248=0, all setup
plans RESTORED. Translator FULLY STOPPED/NEVERrestart; XcodeGPUreplay STOPPED.
Root owns shared source/runtime/UI. New68601/source77977290c9bd2e3712d547f7b8e75b20c4191eca
exe f277ea10... built; doctor/configOFF/build/verify91206 each0. Independent
source/header/library/signature review passed; root349ARM64/0bad. No launch yet.
Normal68597/source6f620 preserved at
/private/tmp/alchemy-metal-68597-normal-preserved.app, executable47670056...
verified before/after copy; source-exact comparison baseline is available.
Committed5e1ecb: adjacent static covered-shadow triangles with identical full
shader/buffer/texture/cull state merge in order. Full source validation stays
before emission; avatar/rigged/terrain/ICB untouched. No shader/math changes.
Both optimized native API+GPU suites428946/95 PASS; final native draw/commit
counter tests1747/1 PASS each (test-only96af75). Counters separate covered/opaque.
Actual live merge fraction and FPS benefit UNKNOWN. Pinned QAconfig/dryrun0;
no setup plan installed. Next quiet M3/M4 benchmark then live covered census.
Apple research found no matching published driver bug in bounded official
sources. Documented binding/residency CPU cost matches profile; argument buffers
and Metal4 command ownership are concrete candidates (links in archive).
PRIVATE M1 Metal3/4 prototype compiled0 and native API+GPU0: same currentMSL3.1
library61c5.../samePSO,12single-texture coverage/Z pairs +cutoff positivecontrol,
bitwise fullR32/D32 parity. Explicit residency/events/owners/completion retained.
No timing,8/32-texture,terrain,rigging or full-viewer Metal4 claim. Sourcecb19ab4f.
Packet/private/tmp/alchemy-metal3-metal4-covered-shadow-20261109.25axBX.
Separate privateCPUbenchmark xitJjX source5a36d4e0 prepared, parity-only/timing
modes, no compilation/GPU/timing yet. Review/freeze manifest before execution.
Continuous12minCPU evidence:920867samples, main~77ms/frame flat asFPS11.8→10.75;
mainactive92→79%, Pcores; Nominalthermal720.726s, clocksunknown. GPUgrowth
attribution remains open. Startup6000allnative1/no fallback; no formal gain.
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

Public regression suites and prototype evidence are in the
[archive](archive/CURRENT-shader-registers-and-neighbor64-2026-11-09.md).

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
Covered-shadow5e1ecb validated; build and measure actual merge population.
Then private quiet Metal3/4 CPU command benchmark; no concurrent viewer/GPU work. Corrected64-parent68588 control remains prepared
but UNLAUNCHED; prior best68579 has31-neighbor behavior, so not a pure A/B.
