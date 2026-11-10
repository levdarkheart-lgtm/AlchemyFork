# Bounded native stage timing and controlled68608 run

Observed2026-11-10 on physical AppleM1Max/64GiB/macOS26.5.2/Xcode26.6.
Branch codex/apple-silicon-metal, measured source
6b80fa8b3fbe999bcedc44a00a2eff4b9b9f437e. Goal20FPS Ultra remainsACTIVE/unmet;
no demonstrated FPSimprovement from this diagnostic implementation.

## Source, build and native validation

6b80 adds `almetalstagetiming.h/.mm` plus focusedtest/CMake, nativeencoderhook,
main-display controller and main/hero/ordinary/filter scopes. Default-off has
no counter allocations. Bounded windowsmax256passes,4freshindices perphysical
pass and independent samplebuffers percommand. Completionleases ownbuffers,
not commandbuffers/windows; externalpolling decodes only successful Completed
commands. No extra wait/commit. Optionalfactory failureabandons preparedslots
before originalpassretry. Unsubmitted, unencoded, failed and invalid samples
remain unavailable; no zero-cost substitution. Metal sampleTimestamps CPU values
are already nanoseconds; calibration uses CPU_nsDelta/GPUtickDelta exactlyonce.

Native Release focused41331assertions/2cases, fullRelease andRelWithDebInfo
476793/104 EACH pass actualAPI+GPUvalidation. Commands:
`cmake --build build-Darwin-ninja-os-arm64 --config Release --target almetal_tests -j4`,
corresponding RelWithDebInfo build, `[stage-timing]` and
`[almetal]~[frame-capture-gpu]` testselectors. Actualall0 in
/private/tmp/alchemy-metal-stage-window-final-validation-20261110.4vuwa0na.
Testsranprecommitaf101 with explicit perfilehashmaps; actualappsource6b80.
Four exactviewerCPPsyntaxchecks0; normalbuild recoveredoldCASPCHwithoutmanual
cachedeletion. Normaldoctor/configure/build/verify/deepstrictsignature EACH0,
349ARM64/0bad. Packet:
/private/tmp/alchemy-metal-stage-window-app-build-20261110.b2y1owqh.
Independentapp/actualreferencedcompilerflags review f51779e6 approvedO3/NDEBUG/
C++23/arm64/macOS26.0 andMetal3.1/fastmath/source-recordingOFF.

Normal68608 canonical build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app.
Executable d8f4cc0a397889ec0479dcf6c136b4c194d90a28be2a841c6dc69ddd0d241f51;
metallib3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2.
Preserved /private/tmp/alchemy-metal-68608-stage-window-preserved.app, signature0.

## Controlled actual session

ExactPID57546 launched0 via private-stage-timing-candidate-launch script8148eaef.
Scrubbed Metal/capture environment, stageframes3; formalrequireexact/ready/
provenance, warmup300s/120frames then300consecutiveframes. Fullscreen BEFORElogin,
MacMidday shortcut andPreset123 applied viaCUA, camera floaterclosed. Actual
43control/start identity/globalposition verify+gate0 saved start-*; endfresh
observe/verify/gate0 saved end-*. One initially sandboxedverify returnedEPERM,
then authorizedprocesscheck0. An initialendgate stale52.4s was correctlyblocked;
freshobserve/verify/gate immediatelypassed. No scenecomparison screenshot was
made under the blockedgate. No build/profiler/GPUtest duringformal/stagetiming.

Run artifacts/metal/qa-runs/candidate-stage-timing-20261110-01;
CSV artifacts/metal/private-68608-stage-timing-20261110-01.csv andprovenancejsonl.
Measured300frames5008–5307:

| Metric | Proper68601control02 |68608 diagnostic normal frames |
| --- | ---: | ---: |
| FPS frommean |9.410236877|9.069479205|
| Meanms |106.267250553|110.259914307|
| p50ms |106.547583|109.6861035|
| p95ms nearest-rank |116.651958|120.610834|
| Mean draws |49622.7167|51019.15|

`python3 tools/metal/perf_compare.py --baseline artifacts/metal/private-68601-material-tables-20261110-02.csv --candidate artifacts/metal/private-68608-stage-timing-20261110-01.csv --enforce --require-provenance --output artifacts/metal/private-68608-stage-timing-20261110-01-comparison.json`
actual1: provenancepasses, noqualifyingperformancewin and3missingscenes. Candidate
mean+3.757%/draws+2.814%; no isolated code-regression or gain claim. Bothall300
nativeexact/nofallback/Release3456×2168/quality6/bias1, all256ready/stable120,
mainvisible98/zeroincomplete/fade/unpublished/queues.43controls matchacrossruns.
Camera stablewithineachwindow, main/endmatch andhero/HUDrestorations true. EEP/
sunexactstable. Crossruncamera tinyresiduals remain: matrixmax0.00019693,
positiondelta≤3.624e−5,basisdelta≤1.848e−6; projectionunchanged. Do notclaim
exactcrossruncameraidentity. Independentredactedaudit f29cac5c plus761f418e
finite/projectionaudit in /private/tmp/alchemy-metal-stage-log-analysis-20261110/.

## New GPU attribution

AfterformalCSVclosed at07:46:33UTC and60eligiblecooldown, three intervals tagged
5368–5370 collected335physicalpassreports:334complete,1counter_unavailable,
9rejectedprepares; actualfinished07:46:40UTC. Missing recordordinaryface4 initial
opaque postpass remainsunavailable. Strictparser61b56e2c and twopeerreviews0;
actualparse0, no extractedcoverageissue. Rawreport stage-analysis-68608.json
insideprivateanalysispacket. Samplecounts do not mean completeGPUcoverage.

Mainbase-lightfragmentelapsed17.343–17.462ms; main atmospheric-hazeencoder
13.770–18.009ms (containslateralpha), mainprojector5.499–5.615ms. Hero face5
above-water13.168–15.057ms withvertex0.012–0.020ms. Actualnextpass74→75 endswater
before atmosphericmarker, isolating thisface's expensiveencoder towaterdraws.
Do notadd theseelapsedintervals: stages/passes overlap and queuedGPUexecution
can cross sourceframeboundaries. They are neither shaderexclusivecost norGPU
busy/utilization norformalFPS. Compute/blit/present/pre-frame++work unsampled.

Exact1eb andnative source show hero face5 is mirrorclipped: scene.mirror_clip
producer andactualperiodicpostflags3623/bit4 agree. Therefore a !mirror no-kill
watercandidate would NOT fix thishotspot and was notimplemented. Furtherwater
single-tap specialization is beingevaluated; no benefitclaim.

## Cleanup and next implementation

Exact57546SIGTERM/Goodbye/PIDabsence0; exitreceipt
artifacts/metal/private-68608-stage-timing-exit-20261110.json. OWNenvironmentplan
private-stage-timing-environment-plan-20261110-01.json restored0 at07:48:55UTC,
originalenvironment/persistencebooleans true. TranslatorstillSTOPPED andXcode
replaySTOPPED. No newCPUtrace wasneeded for thisunchangedbaseline epoch.

Rootapplied reviewed default-off narrowpostwaterSIMPLEearlytests core54b8c29d
andextendedfixture dc0e9dfc (70plannedstrictRGBA/depth/stencilpairs,28cachechecks,
avatar/cloth exclusion). ActualReleasecore+fixturebuild0/offlineMSL3.1.
FirstactualAPI+GPUfocusedtest FAILED7971assertions/7970passed: fixturezeroedits
waterplane normal; productionvalidation correctlyrejected it. Failureqk_k76gd
retained. Rootcorrectedfixture water_plane[2]=1 (geometryeyeZ0) withoutchanging
productionvalidation/math. Correctedrebuild/retest pending; no vieweractivation,
imageparity/performanceclaim orFPSgain. Testcompletecorrectness then quiet
actualrenderer occlusion/no-occlusion costs before admitting aviewertrial.

## Early-depth outcome — rejected, source restored

Corrected actual focusedGPUvalidation11598assertions/1case PASS0 in0bx1j4pc.
Six1024² fullRGBA16/rawdepth/stencilpairs alsoPASS,11753assertions/1case API+GPU;
visible336200pixels/partial168100/fullyhidden0 in both main andprobe modes.
Quietseparateprocess (noAPI/GPUvalidation/capture/profiler) PASS14676/1 with
224counterbalancedrows/7blocks/8cases, allthermalNominal. Actualscript+rawJSONL+
perfile/executable/offline-library hashes in _eqi_0p_ packet. R4 wholeCBdurations
allbelow8ms targetband, explicitlyretained; no highprecisionuniversalclaim.

| Case | OriginalCBms | EarlyCBms | Meanpairedblock delta | Blockrange% |
| --- | ---: | ---: | ---: | ---: |
| Main visible |2.3652|2.5049|+6.11%|−6.51..+20.12|
| Main partial |1.4208|1.5123|+6.88%|−7.25..+22.75|
| Main hidden |0.3167|0.3811|+20.89%|−4.91..+116.38|
| Probe visible |1.3931|1.4237|+2.30%|−5.83..+15.95|
| Probe partial |0.8377|0.7928|−5.08%|−12.95..+1.79|
| Probe hidden |0.3562|0.3558|−0.11%|−11.69..+17.07|
| SAME mainvisible |2.4478|2.5236|+3.54%|−7.95..+26.43|
| SAME probepartial |0.8152|0.8288|+2.05%|−10.91..+8.35|

No convincinggeneral benefit. Originalfullyhidden pathalreadycheap, consistent
withdriverautomaticrejection; invocationcounts remainunavailable. Apparent
partialprobe benefit overlapscontrolvariation; visiblecases show no win. Root
rejectedadmission, no R32retuning/fullviewertrial/fullsuiteexpansion. This is
notproofeverypossibleworkload isneutral/regressed. Do notrevivewithoutnewmechanism.

Fulltested5-filepatch624a611dbb6ae895e6d293c96a172ad5aa3a77b9fcb6c0f54049cb5f45c29d93
frozenin84qciz9s, reverseapplycheck/apply0; all5files byteequal6b80 afterward.
Canonical68608 remainsunchanged. Ninjaartifacts stillcontainrejectedcandidate:
rebuildbeforefutureuse. Source-onlywaterandbaseaddresscandidates are separate.

Rootpreservedrawlogs/JSONL/sourcepatch/scripts/hashmanifests andredactedstage/
formalaudits under ignored artifacts/metal/private-stage-and-early-depth-20261110/;
28files originallycopied withhashverification (consultpreservation.json foractual
list/count). Compiledtestbundlesnotcopied, hashesareinexitreceipts. Actualviewer
rawlog/provenance alreadyin originalignoredrun. Noprivateuploads.
