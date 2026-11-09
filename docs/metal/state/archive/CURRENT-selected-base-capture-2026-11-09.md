# Focused base-light capture and bounded SSR experiment — 2026-11-09

Physical M1Max64GiB/macOS26.5.2/Xcode26.6. Active20FPS Ultra goal remains unmet.
Previous goal turn made progress by executing a controlled candidate and ruling
out an FPS gain; it did not achieve20FPS. Latest actual formal68581 is9.454639FPS,
best controlled68579 is9.608673. Translator remains FULLY STOPPED by owner request.

## Why a new capture boundary is necessary

Existing68561 whole-display4.2GB/51,754draw GPU capture loads and replays in Xcode,
but repeated profiling attempts stalled85%. It is historical source, notcurrent
performance. The current68581 trace's gpu-counter-info/value/intervals and shader
profiler sample tables were serially exported: all4 actualexit0, ZEROdata rows.
Schema presence did not prove counters. Private counterinspection summary SHA
ada719623f863b28bd6deaa777c4ae866121303b57a915bbb05c93e52e7e9648.
Installed gpucapture/gpudebug/metalperftrace tools unavailable(exit72); Apple
identifies those as newer macOS27 tools. Do not invent a CLI profiler recipe.

## Implemented default-off diagnostic

Code `7b03110f71c69455bf45da6626058ad917026998` extends the existing capture owner
with MODE=main-base. Same successful benchmark/disarm/60eligible-interval/private
path/no-overwrite gate; then ready_for_pass instead of automatic full-display
start. The normal main lighting batch is omitted on that ONEselected invocation.
The actual base call starts capture after scene preparation, only mainkind1,
no snapshot, and no enclosing command batch; its fresh owned command is CREATED
and COMMITTED inside capture. Stop precedes result publication and local lights.
RAII/unwind/rendererfailure cancels; exception-safe wrappers isolate diagnostics.
Same-frame eligibility is stamped AFTER display's real gFrameCount increment.
Default directory-unset never touches capturemanager or changes normal batching.
No shader arithmetic/quality/cadence/resource-format changes in this commit.
Device scope may include concurrent uploads; inspect actual trace command counts.
A file/valid in-process consumer is not proof of successful Xcode replay/profile.

Exact original1eb pipeline11716–11873 keeps lightmap/blur before geometry-only
base-light/cheap sky, then local lights. Native acquireCommandBuffer20748 and
finishCommandBuffer20904 retain the same ordered inputs/output and commit inside
this diagnostic boundary. Original has no equivalent capture mechanism; the
nearest producer/consumer contract is preserved, not copied GL mechanics.

## Actual validation

Release and RWDI final builds62451 actual0. Serial final validation60712 actual0:
EACH optimized configuration actual capture2041assertions/3cases with API
validation+MTL_CAPTURE_ENABLED1, shader-validationUNSET; normal full renderer
426583assertions/93cases EACH with API+GPU validation/captureOFF. Fixtures cover
precapture input production withoutCPUwait, freshcaptured consumer/nonzero output,
rearm through actualcapturecompletion, external ownership/collisions/cancellation,
failed selected work and defaultdisplay/manual-boundary isolation. Independent
source/fixture reviews approved. App68584build45995/verify87002/signature actual0,349ARM64; realcapture/profilepending.
Logs /private/tmp/alchemy-metal-selected-capture-*20261109.log.

Failures preserved: first newfixture139 crashed in Catch INFO(nullNSError) strlen,
not renderer; crashreport165211.ips confirms null-string stack. Null-safe testfix
firstneeded macro-parentheses compilefix(actual1); corrected. Additionalrearm
metadata assertion expected9 but actual8 (rearmdoesnotrepeatinitialwaitingrecord),
actual42. Corrected expectation without weakening capture/output/state checks;
retained original8recordbound. Final4serialsuites allpassed as above.

## Separate private SSR candidate — not integrated

Exact original legacyProbeFinish evaluates SSR once and shares it between glossy
and legacy results; native legacyforwardmaterial had two identical samples.
Private _aexcrf5 fixture froze complete current shader e6ea8ce7 and appendedonly
candidatefragment. Compile01 failed on diagnostic variable namedkernel (reserved
MSLword); rename ONLYdiagnostic to normal_kernel. Compile02 all3steps actual0;
API+GPUvalidation38864 actual0:64strictcolor+D32 comparisons/16cases×4formats,
actualpositiveSSRhit/miss controls, rigged/material/hero/outputgate coverage.
CombinedSHAad3ad2d4e767cc85ab73f42cc9c7b68ee78bcacc1f2bdf74adebf7cb195aa73f.

Quiet1024timing83182 actual0,224samples/8cases/7ABBAblocks, thermal0. Missboth
8.57233349→7.34356279ms(−14.334%,7/7faster); sameactualPSO6.539125→6.424271
(−1.756%,4/7faster, blockrange−14.65..+6.17%). Controloutlieroverlapsmissmagnitude.
Hitboth−3.842%/off−2.631%/hero−3.584%/skin−2.914% have variable signs. No control
subtraction/generalwin/FPSforecast; cubeSSRoff meansno hero-raymarch saving.
Independent timing review SHA81f734fe68873d2dde0d7573247bd559d173324603edf217acf5c3351e70d83d.

Private bracketed confirmation el8dshgm preserves shader bytes and uses cached
fixtures, before/after identicalPSOcontrols PERblock, independent physicaltarget
swaps, balanced caseorder and three miss histories/iterationcounts. Source-only
independent review approved; compile43544 all3steps actual0, API+GPUvalidation64956 actual0(72invocations), timing86820 actual0; independentanalysispending.
No shared SSR shader edit. Do not admit based solely on duplicate-source calls.

## Next execution

Finish exactsource-recording appbuild45995 (configure12089/53437 actual0), verify
app/signature/identity, then freshprivateMiddayplan/QA/run. CaptureenabledCSV is
DIAGNOSTIC, not a formal performance comparison. SameUltra/Midday/Preset123 and
256fullyready/stable probes before collection. MODEmain-base aftersuccessfulCSV.
Owner offered to attend Xcode (explicitly corrected can't→can); send exact new
capture file/profilingsteps WHENREADY. Serialize all GPU work with that handoff.
Allpreviousviewersclosed/plansrestored. No newviewer launched yet. Translator
STAYSSTOPPED. Broadparity/stability/strictnoGL/multi-scene gates remainopen.


### Exact live capture-only session

App68584 source7b0311, exe
b428b6e391358547e4263e5a87787a3761cd22832335b93cc2a680925a017624,
metallib9e4fa00fb75f63660a1790abcf4314f08e72a41d22878db015129b17a0a86f1d
(2082855B, source recordsON). Doctor previously0/samehost; configure12089+
sourceconfigure53437/build45995/verify87002/signature all0,349ARM64.
Newlauncher dryrun0; after alltiming/build/testprocesses ACTUALterminal, installed
fresh7b0311Middayplan/source andlaunched0. PID21257, full-screenCtrlSuperF BEFORE
savedcredentialReturn viaCUA; actualLEAPloggedintrue andfresh43control/location/
exactbinary observe/verify/gates0. CUAcompletedsummary. Currentwarming samePID,
frame1360/incomplete250/fading250/samples0/camera0/3456×2168/source7b0311.
Benchmarkcollector is onlyarming capture: DIAGNOSTICNOTPERFORMANCECOMPARISON.
Parentartifacts/metal/private-7b0311-main-base-capture-20261109, mode0700;
childframe-capturemetadataexists/waitingfortiming. No actualnewcapturefile yet.
UserCANattendXcode; waitfileandquit/restore beforemanualGPUhandoff. Translator
remainsSTOPPED. Newstartupplanactive/NOTrestored untilverifiedquit.


### Bracketed SSR confirmation completed

Compile43544/API+GPUvalidation64956/timing86820 allACTUAL0, thermal0.
Independent boundedCPUparse verified864samples/72blocks andnormal+swappedstrict
outputs/hitcensus (72+72validation comparisons; timingresolution12+12preflights).
32iterationmiss8.231479→7.131083ms,−13.3681%/1.100396ms,12/12rawfaster; bracket
normalized12/12negative,11/12beatsbothadjacentcontrols. Bothphysicaltargets/orders
shownegative medians. SameactualPSOmedian−.1244%,SSRoff−.1450%; perblocknoise
remains. Miss8−6.82%,miss64−19.77%,hit−2.74% have weaker/mixedblockevidence.
Normalization is descriptive and assumeslocalmultiplicativedrift, notconfidence
intervalorcontrolsubtraction. Supportsactualrendererregression/livecandidate,
notviewerFPS/hero gain. CubeSSRdisabled. No productionSSRshaderedit yet.
Report `/private/tmp/alchemy-metal-forward-ssr-confirmation-review-20261109.ia45wgjz/confirmation-independent-summary.json`
SHA8379fbf0ae36df2a547bd9603eda48e9c8cf3357b4214aa7c86f644d092a8f5d;
rawconfirmation0b3e0f061ad66eaa26d29cfd58e3b088ad0c1aaad465309c28d0b95be89abaaa.
Allanalysisfinishedbeforeformalarming; agentsquiet/sourceonly. Rootcontinuesto
currentlive68584capturebeforealteringworkspaceMSLsourceforprofileclarity.


### Live capture complete, Xcode opening requested

All256probesready/current/faded/stable120 and300DIAGNOSTICcollectorframes complete,
within-windowcamera0; start/end43controls/location/exactbinarygates0. Selected
capturemetadataSTOPPED,start=stop6422,attempts1,error0,deviceallqueues/selectedbase
interval. Fileexists with21574toplevelentries/9695800649resourcefilebytes(~9.03GiB).
Residentpoolstillstored; actualcommandcount andXcodereplay/profile NOTyetverified.
Initialresourceinventory command accidentally printedallfilenames; outputtruncated,
subsequentinventoryboundedcounts-only. No capturecontentuploaded/committed.

Root21257exactexeverified, gracefulSIGTERM→PIDabsence/run-specificGoodbye; restore
73096ACTUAL0. Allviewersclosed/plansrestored, TranslatorSTOPPED. UserCANattend;
asyncquestionasksopenexactnewfileinXcodeandreportLoaded/Error beforeReplay/Profile.
AllrootGPU/build/test/recordingprocessesterminal; GPUexclusivityhandedtoUIphase.
Outcomeprivate-7b0311-capture-outcome-20261109.json; metadataSHA
1c7a304848292d4367ff9086483e1fd375efdfb84749745f6175c6474a764b51.
PrivateactualSSRrendererregressionpatch125lines FINAL59895c511bd130a418ac0eaed0d5837e8aef8d32ba9fafce9398e5dca5bac367
reviewedbytwoagents, unapplied/uncompiled untilcurrentcaptureinspectioncomplete.


### Actual Xcode replay, profile and counters completed

OwneransweredLoaded; rootCUAfoundold68561/03:28window, explicitlyopenednew7b0311/
17:19filewithCmdO/GoToPathTextField. ReplaywithProfileafterReplayOFF completed;
actualsummary1CB/1render/0compute/2draws/0dispatch/6vertices,3456×2168RGBA16F.
SceneTexturepreviewnonempty, raworientationupsideDown, notfinalpixelparityproof.
Profilecompletedin~20seconds:31.17ms atMEDIUMPState. This is a diagnosticreplay
value, notliveframe/groupFPS. Geometry99.684%commandcost, sky.315%; FS99.62%
shadercost. Geometryallocated/highregisters216,spilled0; sky23regs/0spill.
Geometry7,176,832FSinvocations,overdraw.95,43.22sampler/FS,37,460,254,720ALU
instructions. F32composition32.58%,F16 0,intconditional50.11%,intcomplex8.33%,
inefficiency8.96%; L1texture7.04GiB(CACHEtraffic, notDRAMbandwidth).
EncoderscopeALUlimiter44.54%/util41.65%,textureReadlim6.01%,bufferRead2.74%,
LLC7.04%,MMU1.27%. Scopesnotinterchangeable. OccupancycounterUNKNOWN.
PerlineUI explicitlyunavailable(missingdebuglineinfo/deploymentOSmismatch).
RootStopGPUworkload→FinishedrunningmacOSApp, Replaylanding/checkboxOFF; CUA
completion_summarysent. NoactiveGPUwork. Rawobservedmetrics transcribedto
artifacts/metal/private-7b0311-xcode-profile-observed-20261109.json SHA
fdd3f7b583912f7a6777dd40b9446f93b5b792ac0bbbe4ef6763892a86d8e48f.

InstalledMetalCompiler.xcspec maps include-source toBOTH -gline-tables-only and
-frecord-sources; currentCMakeoptiononlylatter. Futuredebugcapture needslineflag,
nooptimizationreduction. ApplealsorequiresmatchingdeploymentOS; min26.0 vsSDK26.5/
host26.5.2 remainsfactor. DocumentedsamecaptureShaderedit→ReloadShadersworkflow
exists, currenteditorinput/reloadnotyetverified. No furtherbuild/GUItrialclaimed.

### Color0 preview observation

OwnernotedColor0richer/brighterthanviewer. ActualXcodeFiltersAutoRGB0..0.218,
DefaultNoColorMatch, EDRoff,RGBcombined,alphaoff,premultAlpha1; automaticpreview
rangeexpands~4.59×. No settingchanged. CapturedColor0linearHDRbaseaccumulation,
thenvieweraddsorderedlighting/haze/transparency,bloom,exposure,tonemap/grade,
one linear→sRGB, AA/presentationSDRsRGB(noEDR). Boundedsourceauditmatchesexact1eb
ordering/ACES_Hill; no demonstratedextra/missinggamma. ActualQAexposure1/dynamic
false/ACES_Hill1/registeredmix.7; effectiveskymixandgradingnotfullycaptured.
This is notsame-stage/calibrateddisplaycomparison, noqualityfixinferred.

ActualQA RenderScreenSpaceReflections=0; XcodeboundinactiveSSRfallbackconfirms.
PrivateSSRshare13.37%missfixturegain is GENERAL, notcurrentliveUltraFPSfix.
HolditprivatewhiletargetingmeasuredALU/registercost. ShadowpreparesSHlive-range
onlyprototype; SHlayout/halfpolynomialexactlyoriginal, noextra256probe/threadarray.
Prior namedaccums remainrejected. Rootmusttestnewhypothesis, notclaimsourcecode
livenessor216regsautomatically proveslowoccupancy or anoptimizationgain.
