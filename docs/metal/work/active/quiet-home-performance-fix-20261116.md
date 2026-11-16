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
