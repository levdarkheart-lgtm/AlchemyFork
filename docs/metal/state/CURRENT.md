# Current engineering state

**Observed:** 2026-11-09, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `735a407bb7da33a95bf14f7cbae995e1d9a91d4b`.
**Working source:** committed735a in normal68594; formal test flat, startup observation complete.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal, authority and runtime

Unbudgeted20FPS Ultra goal ACTIVE/unmet. Autonomous quit/build/login/measure
is authorized at the same private place/numericPreset123 ONLY; no travel.
Prefer Mac shortcuts/fullscreen before login. Preserve Ultra3456×2168/Midday,
six hero1024 faces/two cascades, PCSS, reverse-Z/clamp/casters/viewer features.
No private uploads/releases. Root owns shared source/docs/runtime/GPU; agents
prepare/review private source only. User explicitly handed Xcode back to ROOT.

LIVE68594/sourceb299d5c99a/renderer735a, exact PID17022/path/hash verified.
Latest owner requested restart and observe top-right FPS. Startup DIAGNOSTIC:
normal flags/quality, warmup0/ready-gateOFF/exact-gateOFF,6000 raw frames COMPLETE.
Readiness false fields are DISABLED observations, not missing-probe evidence.
Same43-control/location/binary observe/verify/gate each0. Cua fullscreen before
login, then exact displayed FPSText read via official read-only LLWindow.getInfo;
not a new scene screenshot/visual parity claim. Widget32→11/22.4s, later10;
160 not captured, polling gaps up8.8s permit a missed transient.240/0 are UI
initialization values. First10s actual12.258FPS/44,631 median draws; next10s
11.038FPS/50,697 with unchanged camera. Real workload growth accompanies UI lag.
Viewer LEFT RUNNING; NEW startup-readout Midday plan ACTIVE until verified exit.
Prior normal PID14587 shutdown/Goodbye/absence75320=0; prior b299plan RESTORED0.
All other plans restored. Translator FULLY STOPPED/NEVER restart/resume.
Xcode replay STOPPED. No competing build/GPU/profile; agents source-only/quiet.
Original full-world Cua screenshot timedout/reset; later login/UItext worked.
Cua completion Confirmed live FPS display changes recorded. Root controls UI.
BOTH optimized API+GPU427529/95 pass. Normal doctor/configOFF/build13489 all0,
verify87922=0, independent349ARM64/deepstrictsignature96763=0; old68590 preserved.

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

Best controlled normal68579/source1f2cc0:300 frames5032–5331,
**9.608672762FPS**, mean104.072646ms/p50103.8956665/p95109.0435.
Latest normal68581/sourceb52c1b:300 frames, **9.454638960FPS**,
mean105.768185ms/p50105.811167/p95110.90525. One pair not causal. Both matched
43 controls/location/binary/camera/Ultra3456×2168/bias1/256 ready/stable120,
TranslatorOFF, no heavy work during samples. Exact apps preserved privately.
Earlier68577 8.011210/68576 6.728847 had external Translator GPU contention.
Original67611/exact1eb+observer6.095519FPS; full parity remains open.
[Raw comparison/hashes](archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

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

Initial forced-clone strict200 pairs pass;168 timings show heavy cases22–32%
lower/all7 blocks, default-only+5.34%. Density followup168 cases with2/8 active
probes/manual boxes also improve6.4–32.8%; smaller effect has variable control.
Actual function-constant source compile31725=0/0/0, API+GPU10157=0:200 bitwise
full color/rawD32 pairs50×4, including main/default fallbacks; thermal0.
Actual guarded timing33685=0:168 samples/7ABBA blocks, heavy cases23.2–31.8%
lower/all7/both targets. Default-only−2.08% range crosses0; samePSO control−2%
also crosses0. No isolated default gain, viewer FPS or spill-reduction proof.
Independent raw-log/parser/source-hash checks pass. No other GPU/profile/build
competed. Tiny Xcode capture ACTUAL1CB/4render/4draws/1024²RGBA16F, profile29.09ms
MEDIUM (notliveFPS). Baseline245→candidate224 registers; BOTH304spill bytes.
Costs55.69/44.22% for two draws each; repeated quiet timings are stronger proof.
No spill elimination/occupancy-causality claim. Replay STOPPED/Finished app observed
and CUA completion recorded before other tools.

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
close exact17022/restoreONLYstartup-readout plan before any build/GPU test.
Private selected-PSO census patch1bc731 is reviewed/unapplied; parent68588 control
prepared but UNLAUNCHED (latest owner prioritized current-viewer observation).
