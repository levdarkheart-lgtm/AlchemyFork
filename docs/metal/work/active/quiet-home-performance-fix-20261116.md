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
