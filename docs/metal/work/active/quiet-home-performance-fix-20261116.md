# Quiet Home performance repair

Owner goal2026-11-16: implement and qualify fixes for measured quietHomeGPU
workload/spilling and assess additional Apple-native techniques. Original
broad architecture remains deferred until this Home problem is addressed.
Next ownercheckpoint HOME ONLY, samegraphics/lighting/camera. Owner chooses
next steps afterward; no clubteleport in this checkpoint. No push authorized.

Baseline fd5e8e088e1224e150e7ae71a17f827faa18249e /69200 preserved, exactsource
and actual CPU/GPU/capture evidence at quiet-terminal-analysis-20261116T172257Z
and quiet-frame-analysis-20261116T174649Z. Currentintegration5d367deb04 clean
beforethisrepair; independentcheckworktreecodex/quiet-probe-checks.

PPD-327 pre-fix mapping: pinned1eb44class3/reflectionProbeF.glsl ProbeSums/
probeTap/probeMix named automatic/manual scalarpairs, conditionalweighted
accumulation, orderedcandidate/neighborwalk/manualboxprecedence/defaultzero.
Native MSL6293–6480 andwater8500–8610 have same algorithm but five2element
thread arrays andruntimefamily indexing. Actualreloadsimple/PBR/material
spills304/80/32B. Scalarstoragecausality is unproved until thepairedcheck.

First change: named scalarpairs/explicitfamilybranch in actualsharedMSL,
includingwaterreferences. Keep sampleflags/math, selection/order/limits,
normalization/mixing, precision, textures, graphics, CPUownership/overlap and
resourceadmission unchanged. Do not reapplydefault-onlydiagnosticshader.
Root owns MSL/CMake/docs/build; architecture_audit owns NEWtestfiles only in
independentworktree; gpu_tool_audit read-onlysemanticreview; evidence_audit
read-onlyApple/source-backedfuturelevers. No concurrentuncoordinatedshaderedits.

Qualification: baseline/candidateactualGPUcolors for auto/manualspheres/boxes/
fade/neighbor/cap/default/classic/legacy/PBR/water/multiview; actualpairedraw
costs andreg/spill/counter observations onM1Max;doctor/build/verify/static and
relevantnativechecks. Rejectmeasuredregressionsandkeepalladverseobservations.
Stageaseparatecombinedviewer onlyafter containedchecks. FPS/fixacceptance
requiresownerHomecheckpoint; no autonomousviewerlogin/profile. Apple-native
followups mustremoveidentifiedcost, beM1-supported, andpreserveSLbehavior.


## Contained results and rejection

Evidence: private `artifacts/metal/quiet-home-fix-20261116T183158Z`, baseline
fd5 MSL SHA256468692614a883ccf812e14e7a8b0719a0fb72c8f9e1454a4968d3547adf9f716.
M1Max/macOS26.5.2/Xcode26.6. All shader sources, exact optimized Metal3.1/
fast-math/macOS26 source-recording commands, AIR/libraries, compile/link0
receipts, unchanged test binary hashes and raw alternating pairs retained.
Independent actual helper and production base/simple-alpha PSO fixture is
integrated in3d3c07e2ba, CMake registration285c85e7df.

| Variant | GPU parity | Contained cost at39 probes | Decision |
| --- | --- | --- | --- |
| c7 scalar storage |6081 assertions/429 images; max component error1.1920929e-7 |simple-alpha paired+1.06%, ambient-base+3.56%; no convincing gain |reject |
| original arrays+narrow mix |6081 assertions/429 images; exact pixels |simple-alpha+0.04%, ambient-base+0.79%, PBR-base-0.01% |neutral; reject |
| scalar+narrow mix |6081 assertions/429 images; max error1.1920929e-7 |ambient-base+3.33%, PBR-base+3.20%, both5/5 adverse |reject |

Each variant cost fixture passes1131 assertions with160 raw observations,
16 cells, five alternating pairs,512x288/one draw per command. API/GPU
validation ON for parity and OFF for cost. Absolute baselines varied across
processes, so compare within-cell pairs only. This is a small deterministic
workload, not Home FPS or a generated register/spill measurement. Captured
245GPR/304B simple-alpha source still needs codegen qualification. Baseline
MSL restored exactly in285c85e7df; preserve failed/neutral sources and all
adverse evidence. Next target SH evaluation/live-state cost, not a quality or
probe-count reduction. Original architecture rollout remains deferred.

PPD-325 tools integratedbae0170c91 fromd1962f7d1d. Independent exact-final
179tool tests/harness/diff PASS; private quiet-home-tool-qualification-
20261116T183912Z. Cachedl9 now PASS valid retained GPU coverage (700positive+
24zero rows per family,max actual gap1.05648s). qy correctly still FAILS its
actual5.524927s startgap; do not relax5s guard or reclassify failed evidence.
Future recipe must explicitly select writer ALCHEMY_METAL_DIAGNOSTIC_MAX_MIB=256,
collector --diagnostic-cap-mib256 and summary --max-bytes268435456. Defaults64
unchanged. Actual controller/built-viewer retention check pending.

PPD-328 duplicate material-alpha SSR is source-confirmed only; no repair or
runtime cost assigned yet. Original1eb44 legacyProbeFinish samples once,
blends both lobes; native calls helper twice with same ray inputs when both
lobes enabled. Material-alpha1–2% of replay is not the dominant Home shader.


Actual controller byte-budget target compiled0 at285c85e7df;18 fresh-process
checks pass (unset,empty,64,00064,65,72,128,255,256; invalid63,257,negative,
plus/whitespace/trailing junk,uint32overflow/long overflow). Actual output
header/activation asserted with API/GPU validation1. Private controller-budget/
{build,checks}.json pins source/binary/commands/logs and retained headers.
No viewer executed; startup parse is not a renderer performance change.

Further fixture source audit: the first production alpha check was unshadowed,
while the dominant Home PSO is simple_shadow_fragment. Count39 all-overlap stress
accepts32 taps; actual saved late Home configured20, accepted distribution unknown.
Therefore prior cost results reject those variants for that tested workload only;
a matched shadowed PSO/count20 qualification is being added before final selection.
Home log confirms classic_lighting=0, so ambient SH actually executes; do not
optimize a classic/all-lobes-off path as if it explains this capture.


Owner steering: verify actual compiler options and retain scalar/constant-index/
shorter-lifetime techniques as legitimate measured candidates. Current Release
Ninja graph1641 C/C++ compile commands all-O3/arm64, none-O0/default; this is
current generated command evidence, not a claim1641objects rebuilt today.
Production offlineMSL3.1/fastmath/source-recording, targetmacOS26.0. Installed
Metal32023.883 default tiny-probe optimizedIR matches explicit-O2/-O3;
-O0 hasoptnone anddifferentIR. Fullproduction compiler audit pending. No
compiler/math/precision option changed to influence the comparison. Exact
commands, options stamp and IR in privatecompiler-audit/.

Next isolated PPD-327 sources are streamed-sh, noinline-sh andstreamed-noinline-sh,
all preserving fullprobe counts/math/precision. StreamedSH evaluates the original
L0/L1 sum before decoding L2, keeping only packedremainingwords across scopes;
addition tree andfourtexture reads remain. SH-onlynoinline tests compiler-lifetime
boundaries without outlining thewholeprobe helper or inhibiting constantlobe
elimination atsimplealpha. These are codegen hypotheses; call ABI may instead
spill and cost more. Allprivate source/library/command receipts retained. No
engine shader selection until actualshadowedPSO correctness/cost qualification;
ownerHome acceptance remains required.


Fullproduction compiler audit complete: all367function definitions andinstruction/
attribute/debug metadata are byte-identical default versus explicit-O2; only
recorded commandmetadata differs. 93940lines/8.54MiB outputs and normalizedhash
receipts at compiler-audit/full-source/. The scalar frontend DOES change five
runtime accumulator GEPs to constantfield accesses; finalbackend may further
inline/scalarize. Thus it is a legitimate optimization candidate whose measured
cost, not source appearance or frontend alone, determines selection.

New actualshadowPSO fixture integrated7a724de0eb and emptyoptionalconstant loader
ae16761fee; first APIvalidationabort preserved before loaderrepair. Scalar
repaired run7556assertions/2cases parity PASS;488costassertions PASS. Count20
paired mediansautomatic-0.78%/mixed+0.50%;39automatic+4.99%/mixed+4.47%, so no
usefulshadowedscalar gain established. Remaining variants use newhash-pinned
binary; no precedingbinary substitutes for changedsource evidence.

PPD-329 original mapping: pinned1eb44 CLASSIC_MODE compile-time macros in
class2/alphaF43–47/reflectionProbeF47–55, separate suffixedfeature compilation
llviewershadermgr922–940. Native simplelighting uniformbranch is source-confirmed
loss of that compiler visibility. Private simple-classic-specialization source
adds optionalboolconstant2, originalfallbackifundefined. Actual Homefalse PSO
canremoveunreachableclassic blocks without removing activeSH/shadowmath. If
measured useful, finite24→48 prebuilt simplePSOs andvalidatedscene.alpha_lighting
parameters[3] lookup; no new draw-time compilation or avatar/custom cache changes.
No benefit claim or actualengineactivation yet.


## Selected proposed repair after actual recording

[Recorded-scene results](../../research/quiet-home-shadow-specialization-20261116.md)
distinguish fixture and actual owner capture. High2 seven-layer fixture: 36 exact
images, 60/60 favorable pairs, 11–13% median GPU reduction. Actual saved late
Home source A/B/A: 142.99→119.21→140.90 ms at Medium/Overlapping. Raw observations
and source-edit hashes: private recording-replay/observations.json. B specializes
all shared filter consumers; the proposed viewer uses optional constant 3 only
when the exact bound pack is High2. No hardcoded-quality shader ships.

Root MSL now matches private High2 source SHA256
f05d83a1b4a3f96159d7a1750c035afab67db423f0d239c1cd496c1e1143f33d.
Exclusive world.cpp integration prepares 37 High2 PSOs for light map, projector,
simple/material/PBR alpha; other consumers retain generic behavior. Build,
engine qualification, staging and owner Home acceptance remain pending.
Simple-alpha 304-byte spill persists. Growing probe work remains a measured
limiter. The actual live FPS effect and original architecture acceptance remain
partial. GPU replay stopped, source edits discarded, input audit pending aliases.


## First combined integration: retained failures and correction

060942a945 / 69221 builds0 (344 seconds), staged8272nodes exactly match source;
verify_app ARM64/signature and strict source checks pass. Qualified Python
179tests pass; initial defaultPython run failed for missingPyYAML and is retained.
Full native global API/GPU-validation invocation: 464cases,457pass,6fail,1skip;
2380033assertions/9fail. Do not deliver this candidate.

PPD332: five failures were the wrong matrix for native archive persistence.
Its deliberate shader-validation bypass worked. Fresh API-only/no shader-env
[pipeline-cache-disk] now passes5905assertions/6cases. Ordinary correctness and
selective instrumentation will use separate documented modes.

PPD333: the odd31x31 High2/reverse-Z/HDR probe fixture fails with GPU validation
bothON andOFF, so it is a real integration parity issue, not instrumentation.
Separate lightmap usesHigh2 while fused tile producer remainedgeneric. Failed
arrays show21center-axis pixels/63RGB half-components differ (maxHDR0.03515625).
No equality relaxation, tile disable or architecture rollback is selected.
Independent reviewed world-only repair819f98d71c adds2prebuilt High2 tile-lightmap
PSOs, selecting fromthe exactboundpack; base/sky/ROGformats remain intact.
Rebuild and focused strict parity are pending; preserve first69221 stage.

PPD331: explicit older-library contract preserved through actual declaration
query atconstruction; absentoptional3 clonesgeneric withcompatlabel. Real PSO
creation errors stillpropagate. Hidden fd5reload/5declared-consumer assertions
pending. New bundledcandidate mustprove3/assetidentity; oldgenericcompat isnot
performance evidence.

Original and preserved capture content audit completed: all22105logical paths/
bytes/hashes match,17254regular files+4851aliases, no followed symlink. Symlink
permission modes differ source/copy and oldmanifest lacks linkmetadata; no
historical permission-equality or attribution claim. Strict failures retained;
finalcontent-verdict0 under post-home-fix-replay-input-verification-20261116T192819Z.


## Verified combined candidate and authorized live checkpoint

Final runtime source819f98d71ca479441f635fb619e8dccb5d26e6a8 /26.4.0.69224,
local experiment branch; build0 in48.98s after bounded tile fix. Corrected native
matrix allpass: renderer457cases/2375887assertions API+GPUvalidation; archive
API-only6/5905; selective1/98; focusedprobe3/1526; oldprivate-library1/80;
productionhelper/shadowparity2/7580. Total2391076assertion-executions in470case-
executions (focusedcases repeat ordinary coverage). Budget18freshprocesses pass,
all9acceptedactualheaders pin819source; tools179/harness/strict/app/signature pass.
Newstage candidate-26.4.0.69224 exactlymatches8272internal sourcebundle nodes.
ExecutableSHA89f27569f7c9d3dfeca5b5a4805128e72d3c24d988758b06a06986bae9e2d604;
metallibSHAe8198019580a4559c213204b2bd01423c384f8a07aa108d492f26f67014a2cc9.
First69221failedcandidate andallfailedchecks remainpreserved/unaccepted.

Owner explicitly authorized launchwhenready, then another rununderMidday.
Sameverified69224/exact19overrides/fourTRUE/cameraandgraphicsowner-controlled;
validation/capturelayersoff, nativebudget256MiB, Overview180s startsbeforelogin.
Regionenv /Users/dekstroza/Alchemy-home-upwlt_pz PID13541: recordstart20:11:03Z,
worldSTATE_STARTED20:11:35Z, recordstop20:14:11Z, complete20:15:02Z/exit0.
Midday /Users/dekstroza/Alchemy-home-dwgbpexo PID16215: recordstart20:18:50Z,
recordstop20:21:57Z, complete20:22:21Z/exit0. UserconfirmedMidday viaexplicit
marker andreported27→brief17→15FPS; previous17wasnotfinalsettlement. Bothviewers
closedobserved; nofurtherimplementation/profile/launchbeforefindingdiscussion.

Preserve/analyzeboth independently underprivate home-checkpoint-69224-
20261116T202910Z. Native136/152MBfinal files extendbeyondOverview; matchedphases
mustrespectactualoverlap andeligibility. Differentlighting runsare notpairedFPS
controls. PPD319remainsopen; replayGPUgain doesnotestablishliveFPSacceptance.
Originalbroadarchitecturepaused; no club/push. Next action: completecoverage,
source/settings/CPU-GPU/native/warning audits anddiscussresidualquietdecline.
