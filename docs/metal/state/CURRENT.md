# Current engineering state

**Observed:**2026-11-09, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `1f2cc0e786f6c88f5a1dee23215c17650e6e83b0`.
**Working source:**diagnostic committed/built/verified; controlled viewer RUNNING.
**Recovery:**[NEXT](NEXT_SESSION.md),[ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md),[history](archive/INDEX.md).

## Goal and latest result

The unbudgeted20FPS Ultra goal remains ACTIVE/unmet. Autonomous quit/build/login/
measure authorized at the fixed private location/numericPreset123 only; no travel.
Prefer Mac shortcuts. Preserve Ultra3456×2168/Midday, six hero1024faces/two cascades,
PCSS/clamp/casters and viewer functionality. No private uploads/releases.

Latest68577/source30715f completed300frames10487–10786:8.011210FPS,
mean124.825083ms,p50121.765792,p95154.566750. Allnative/noGL/Release/Ultra3456×2168,
256ready/stable120,bias1all300,43controls/environment/sun/restoration match,
cameraEXACTmatch68572 andsampleddelta0,main98. Versus68576/source0fa4e3,
mean148.613868/6.728847FPS:mean−16.007%,FPS+19.058%,draws+.038%. Versus68572/
source46d622mean106.585321/9.382155FPS:mean+17.113%,FPS−14.612%,draws−2.837%.
Both officialenforce/provenance performancegates1. This is observed improvement
versusrecentregression, not fullsourcecausalproof or20FPS. Goal remains unmet;
hardwareinfeasibility unproved. Do not substitute test/profilevolume forFPS.

Post-formal viewer-only15sGPUtrace42832 completed0; serialexports/parsers0.
Maininitialopaquegroup44.844→20.756ms/p, mainlightmap5.418→4.743, baseabout18.
Viewer ActiveGPUunion143.136→129.350ms/p acrossseparateinstrumentedwindows;
othergroupsvaried, labelsoverlap, nofull24mscausalclaim. Forwardshadowedalpha
useschangedhelpertoo. AdditionalGPUactor identified:python3.12/PID35824 Active33.98%wall,7.30%outside
viewer in68577trace. Userfirstauthorizedpause thenEXPLICITLYclarifiedFULLSTOP
usinguv. Rootran `uv run --no-sync python scripts/service.py stop` actual0 from
Translatorproject; API35823/tailscaled35866stopped,worker35824absentverified.
USERREQUESTNOAUTOMATICRESTART. Stoprecordprivate-1f2cc0-translator-stop-20261109.json.

CURRENT68579/PID78103 RUNNING, source1f2cc0, exactappfullscreenBEFORElogin,
fresh43control/location/binaryobserveverifygate0. TranslatorSTOPPED, allprior
viewerstateplansrestored;1f2cc0MiddayplanINSTALLED/NOTrestored. Normal300sample
first (no addedencoders); envALCHEMY_METAL_POST_FAMILY_ENCODERS=1 onlyactivates
AFTERcommandtimingtrue→falsepostCSV. Needseparate15sGPUtraceafterformal; parser
mustrecognize'Alchemy DIAGNOSTIC initial...' asfamilydiagnosticnotnormalFPS.
ExpectedCSVprivate-1f2cc0-family-diagnostic-20261109-01.csv/siblingprovenance.
AllGPUtests/builds/verifyfinished; nootherrootGPUtasksduringrun. Agentssourceonly.

## Retained original-contract repair

0fa4e3 removes incorrectly inherited SSAO+two blur passes from hero/ordinary cube
faces. Exact1eb pipeline11716 selects sunProbe/class2sunLightF, which writes AO1;
11752 excludes cube blur. Samefalse feeds native face light-map/base; MAIN SSAO
still enabled. No shadow/caster/cadence/shader math change. Two independent audits/
diff review approved; focused1992/1 API+GPU regression passes. Doctor/configure/
build/349ARM64/deep-strict signature all0. Actual pipeline compile O3/NDEBUG.

Measured68576 was at the canonical build path before the next build;
preserved/private/tmp/alchemy-metal-68576-probe-ssao-measured.app. Executable
75a0c59d7b862ba3d4668bd01cf9f9fe4ecc4fe257746386589589a6bfbb26e5;
metallib8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943 unchanged.
FormalCSV/privateprovenance inartifacts/metal/private-0fa4e3-probe-ssao-20261109-01.*.
Fresh start/end43control/location/binary gates0. Official performance comparison
with enforce/provenance exits1 (failed performance, not fake passing result).

Previous30715f/0fa4e3startupplansRESTORED aftergracefulquit/PIDabsence; current1f2cc0planactive.
No CUA phase active; fullscreen-before-login phase summary sent. No screenshot
for this run, so no pixel-parity claim. Root owns shared source/docs/runtime.

## Specific current GPU evidence

Post-formal15s viewer-only Metal System Trace completed0; exporter/parsers0.
96present requests over15.738528s are diagnostic, not formalFPS. Viewer Active
GPU UNION143.135660ms/p,87.3082%wall,thermalNominal,9.26–9.31GiB allocation.
Main initial opaque/forward group44.843684ms/p is largest inclusive context;
main base17.632984,haze17.090225,heroabove-water20.167778 overlap. Never sum
these or call individual shader costs. Main opaque CPU encoding0.199080ms/p.
Trace confirms ZEROhero/ordinary SSAOblurs, main97horizontal+97vertical retained.

Historical68566/bias4 mainopaque4.945755ms/p is uncontrolled context; no68572/
bias1GPUtrace exists. Source snapshotsinitial103families match68566 counts:
44fullbright/2shiny/3avatar-alpha/54glow.68572had109with6extra glow. Snapshots
precede trace; physicalperencoder drawcounts unavailable. Exact costly draw/
texture/resource cause remains unresolved. Currentfull-detail working set differs.
[Complete68576/source/build/runtime evidence](archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Active implementation checkpoint

Exact original PCSS uses native2×2gather; native expanded each to4clampedreads.
Counts8blocker/12filter and math already match. Root staged productionhelper
replacement preserving integerfootprint then gatheratblockcenter, clampedge,
xyzw(0,1),(1,1),(1,0),(0,0). AppleMSLprimaryspec/order reviewed. No4×bandwidthclaim.
Privateu34k2G full5dea prefix+namedhelperchain, two independent reviews, compile0.
After viewerclose/restoration, nativeAPI+GPU72157 actual0:15680rawqueries/
62720components EXACT;96full lightmaps EXACT (48RG32F+48RG8), nonvacuouscontrols.

Quiet timing18272actual0:6cases/168samples,7ABBAblocks14variant,8–20msCB,
thermal0, actualmain3456×2168/2048shadowmaps. MainPCSSmedians22.4–28.5%lower,
all7blocks faster,1.00–1.93ms perfixturepass. SAMEactualPSO−.51%; unchanged
Gaussian+3.15%(.0228ms),probeSingle+10.98%(.0163ms) explicit. No no-regression
or FPS forecast; source/compiler/layout versus drift remains unknown.

ExtendedRAW compile0/API+GPUrun0:23480queries exact includingproduction/nonPOT/
16kedges andIEEEclasses.30715f integrates helper plus permanentproductiontest:
focused34626/1 and fullRelease426213/91 API+GPU0. Independentreviewapproved.
FullRWDI71370 and verify_app9881 ACTUALexit0:426213/91 and349ARM64; Xcodeconfigure/build0,
deep-strict signature0. Measuredcanonical68577 now CLOSED/preserved, exe712bee665eefd7b6ca0255460070ba9fd023e0d4b3774310279bab27902d0bba,
shaderffd9e1d178988b0719c516cd1e87444907d59d063630d7fd9ac2d17cafaa9347.
Launcher/config30715f dryrun0; exactappfullscreenbeforelogin/start+end43control/
location/binary gates0. CSVprivate-30715f-shadow-gather-20261109-01.csv complete300.

## Retained quality baseline and earlier proven work

Preserved/private/tmp/alchemy-metal-68572-quality-baseline.app, source46d622,
exe84e49c91677697abf7527c93b21567950c5d525111b85ed3f3b0d189a1bbd155,
shader8bb.3969405separates image/global pressure;46d622derives C=max(R−max(A−I,0),0)
then originaldivisor/headroom ONCE, manual/global/system/controller unchanged.
Reserve includes padding/retired/unpublished bytes; estimate, not exact census.
159/21pressure+29/3capability checks eachconfig0. Normalbias1 restores texture
policy/detail; not exact per-texture/pixel parity. Latestformalimages7629MiB,
target45996–46006,device9514–9526,R59392,syslow0; pressure not current explanation.
[Budget evidence](archive/CURRENT-resource-aware-texture-budget-2026-11-09.md).

74ee5dfragment-boolAPI full391587/90checks EACH optimizedconfig API+GPU0; both
NinjaGPUbinaries currentAPI but must rebuild after working shader/test changes.
Prior classic/SSR/materialsplit candidates NOTadmitted: smallmodern benefit,
precision failures or mixed-material regression. [Evidence](archive/CURRENT-base-specialization-experiments-2026-11-09.md).

59177fixed27/36Release targetsO0;CPU91.5→72.8%ONEcore without establishedFPSgain.
No matched frame count in CPUtrace, so no77ms CPUframe or20FPSCPUfloor claim.
Existing occlusion consumed120828spatial skip EVENTS/299intervals, notuniquegroups.
Original67611/exact1eb+observer6.095519FPS/bias1/main104; fullparity stillopen.
Keep per-view probe correctness, shiny bypass, bindings, realocclusion/Ultra4×,
boundedlights/directR32shadows/borroweddepth/projectors/arenas/caches/retiredgeometry.
Deferred alpha local-light count0 guard is real but not this UltraFPS cause.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-004 Resource ownership | doing | Image/global scopes corrected; cache/lifecycle policy open. |
| M-008 Advanced views/effects | doing | Cube SSAO parity fixed; main opaque GPU cost and broad parity open. |
| M-012 Native UI automation | complete | Fullscreen/login verified; world AX/capture timeout limits. |
| M-014 Ultra20FPS campaign | doing | Latest6.73FPSregression; gather tested privately, target unmet. |

## Exact next actions

CubeSSAO parity is fixed; netFPS regressed, mainopaque GPU cost newly localized.
Gather is a measured general filter candidate with initial exact proof, not a
viewer result. Extendedraw/prodtest/build/GPUreview andresidentcomparison finished. Next:finish activecontrolledsample/postCSVfamilytrace, thenquit/restore.
1f2cc0doctor/configure/build/349ARM64/deep-strict signature all0; focused9149/1
and full426444/91 API+GPUchecks EACH Release/RWDI0. App68579executable
bf4cdfef2fe95cf91991f26e4f948328878fa8627871100825d54b11fe6d69f5,shaderffd9unchanged. Firstfixturefailed8922assertions(1invalid
water-exclusionorder), sourceonlytestorderfixed/productionvalidationretained;
retry0. DiagnosticdefaultOFF/main/postCSVonly, exactcolor/depth/stencil/counter
proof. Independentreviewsapproved. NeveruseaddedpasscostasproductionFPS. D32arrayfirstgate
actualAPI+GPU98353 passed8960queries+96threewaymaps; opaqueFSnilcaster/clamp/alpha
parity/migrationstillunproven. NootherGPU/heavyrootprocessesduringrun, agentsCPUsourceonly. Fullfeature/pixelparity,multi-sceneperformance, prolonged stability,
strict no-GL and independent build/review evidence remain OPEN. No hardware-
infeasibility, release or product-complete claim.
