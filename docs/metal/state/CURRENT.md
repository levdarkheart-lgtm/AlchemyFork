# Current engineering state

**Observed:** 2026-11-10, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`. **Renderer code checkpoint:** `5e1ecb76459de8a0bc90dc16da56aec75f1fc02d`.
**Working source:** tested material tables + flat preparation; viewer trial pending.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal, authority and runtime

Unbudgeted20FPS Ultra goal ACTIVE/unmet. Autonomous quit/build/login/measure
is authorized at the same private place/numericPreset123 ONLY; no travel.
Prefer Mac shortcuts/fullscreen before login. Preserve Ultra3456×2168/Midday,
six hero1024 faces/two cascades, PCSS, reverse-Z/clamp/casters/viewer features.
No private uploads/releases. Root owns shared source/docs/runtime/GPU; agents
prepare/review private source only. User explicitly handed Xcode back to ROOT.

All viewers CLOSED. Exact68601 PID73462/source77977290/exe f277ea10... ran
fullscreen before login; freshstart/end43controls/location/binary gates0.
300readyframes captured, SIGTERM/Goodbye/absence+Middayplanrestore0; allplans
RESTORED. Media subprocesses exited before privateGPUwork. Translator FULLY
STOPPED/NEVERrestart; XcodeGPUreplay STOPPED. Root owns runtime/UI.
Doctor/configOFF/build/verify91206 each0; independentidentity/signaturepass,
root349ARM64/0bad. No scene-image parity claim or concurrentGPU/profile/build.
Normal68597/source6f620 preserved at
/private/tmp/alchemy-metal-68597-normal-preserved.app, executable47670056...
verified before/after copy; source-exact comparison baseline is available.
Committed5e1ecb: adjacent static covered-shadow triangles with identical full
shader/buffer/texture/cull state merge in order. Full source validation stays
before emission; avatar/rigged/terrain/ICB untouched. No shader/math changes.
Both optimized native API+GPU suites428946/95 PASS; final native draw/commit
counter tests1747/1 PASS each (test-only96af75). Counters separate covered/opaque.
Independent39interval audit VUhsf14y/8fc7f266 verifies120frames each; latest
main1833/probe2465.86 commands removed/frame (16.40%/18.41%family inputs).
Combined4298.86/frame is API-command reduction, NOT triangles or FPS.
QA candidate-covered-shadow-20261109-01, CSV private-covered-shadow-20261109-01.csv:
300frames9507–9806, mean101.690106ms/p50101.502709/p95106.397708,9.833798FPS.
Allnative1/fallback0/256ready/bias1. About2% above priorbest9.63558FPS; earlier
68597census8.15930 is a cross-session comparison, not an isolated causal gain.
Apple research found no matching published driver bug in bounded official
sources. Documented binding/residency CPU cost matches profile; argument buffers
and Metal4 command ownership are concrete candidates (links in archive).
PRIVATE M3/M4 same-oldPSO/61c5 proof compiled0/API+GPU0/12strictdepthpairs.
FixedtextureCPUcaseNOwin (M4+4–6%); changing-material stress frBX0a passed
24strictpairs/168timings,8/32slot CPUencode−59.6%/−83.5%. These price stable
prebuilt tables, notrealviewer materialmix/FPS. Fullpaths/hashes in archive.
No productionMetal4 switch. M3argumentBmrgf1 actuallycompiled0/API+GPU0,
24strictpairs/168quietCPUtimings0;8slots−59.93%/32slots−87.18%,all7pairedwins.
Hf64pq full2048²GPUgate85strictpairs0,2×252quiettimings0: modestprobable32slot
GPUcost amidcontrolnoise (repeat+4.28%median/~.004ms/pass); NOT GPU-nonregression.
Rootintegrated sharedFrameData materialcache/nativehelper/new8/32resourceABI,
sampleropt-in/directoracle/lifetime tests. No shippingFPSgain claim. Release
finalnativeBOTHconfigs434648assertions/101casesPASS inclactualworkbounds.
InitialfullCPU1960hot8−37%/32−70%, cold512+117%/cap4096+1194%rejected.
Missfixmetadatareuse/capacitylatches applied; exactretest1960nowhot8−39.54%/
32−67.80%, cold5126.506→5.014ms(−22.93%), cap4096still6.621→14.833(+124%).
All7pairedcapacityregress; no viewerbuild/promotion. Source/library/exe preserved.
NEXT privatecapacitypolicy: whenNEWunique8/32keys exceedmaxslots ANDallonce,
skipNEWtableallocation/use direct, keepcachedhits. Validateallnativeobjects
onceperstrongunion beforecachewrites; per-entryrefs onlyforadmittedtables.
Pressurepolicy/testsAPPLIED; BOTHconfigs435459/102PASS (44174/71862 each0).
Flat scratch75dc APPLIED; BOTHfull435459/102PASS,collisionCPU0/nativepairs10/10.
Actual196retest0:hot8−40.21%/32−69.60%,cold512−31.10%,but4096once-only
6.414→7.869ms(+22.68%/+1.455ms),all7regress. ControlledviewerTRIAL next,
not universalnon-regression/releasepromotion; no newFPSclaim.
Fresh Apple research found GameMode launch/opt-in uncertainties; see
[research](archive/CURRENT-apple-metal-research-2026-11-10.md). Actual state unknown.
Continuous12minCPU evidence:920867samples, main~77ms/frame flat asFPS11.8→10.75;
mainactive92→79%, Pcores; Nominalthermal720.726s, clocksunknown. GPUgrowth
attribution remains open. Startup6000allnative1/no fallback; no formal gain.
Startup probe packs grew2→98/main and256/hero while draw counts plateaued;
GPUcompleted-CB sums overlap, so no causal shader-ms attribution. Widget peak41,
later12/11/10; initial value varies. Earlier2-sampleCPUtrace unusable; corrected
Mach units e0656051 retained. SelectedPSOcensus4e833 passed428004/95 bothconfigs
(firstsandbox42/noGPU, separatenativererun0). Full evidence in archive.

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

Oldhero face1:1CB/4render/4blit/833draws/1024²;15.74msMEDIUM is NOTliveFPS.
Simplealpha58%/245regs/304spill,water20%/254/96,PBR15%/254/80;occupancy4.19%.
Haze encoder77.9% includeslateralpha; hazeFSonly1.79%. CallStack71/29%probe/
holds is inclusive/optimized; staticspillbytes nottraffic. Fullcounters/archive.

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

Committed735a folds EXISTING probe single-tap branch with boolconstant0;
actualvalidatedshadowbit4/probecount>1 selects PSO, runtime main/avatar preserved.
Actualfixture200strict color/rawD32 pairs PASS;168timings heavy23.2–31.8%lower,
controls cross0. Xcode1CB/4draws245→224registers/BOTH304spill: no spill removal.
Public optimized tests pass; actual viewer result flat, not synthetic gain.

Material evidence: [archive](archive/CURRENT-material-argument-buffers-2026-11-10.md).

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
| M-014 Ultra20FPS campaign | doing | Latest9.83FPS; binding-stress CPU win needs safe integration/live proof. |

Full pixel/feature parity, multi-scene performance, prolonged stability,
strict no-GL and independent build/review evidence remain OPEN.20FPS unmet;
infeasibility unproved. Next: commit tested material candidate, normalbuild/verify,
then controlled viewer trial against preserved68601; actual materialmix unknown.
Record GameMode status separately before any launch/opt-in change.
Whole-shadowparallel source-only; fresh68597control stillneeded forcausalFPS.
Corrected64-parent68588 UNLAUNCHED; oldbest68579 has31neighbors, notpureA/B.
