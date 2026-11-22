# Current viewer performance investigation —2026-11-22

Status: investigation complete; implementation proposals only. Explicit overnight goal: diagnose current Home/club frame time and
produce ranked engineering proposals. Owner authorizes autonomous launch/login,
teleport, camera, Xcode/Instruments/recordings/logs. No broad implementation,
commits, pushes or publication. Minor112m vs120m draw-distance differences are
immaterial; preserve consistent quality/resolution and concentrate on the limiting
sequence. No owner assistance is expected.

## Verified starting identity

- Canonical codex/apple-silicon-metal HEADd219d538ab; tracked tree initially clean,
  unrelated worktrees/ retained. Rendering/scheduling code960bd3644a, indra
  7d8c095d360bd3f77ac64d1b69e23c21840e0011.
- Intended owner-accepted Scheduling26.4.0.69450 at the separately staged app,
  executable SHA256df28cec46e833d8eba36c38e45385a4ed469964cc2ae84a753a78a86ccd47335.
  No viewer process was running at preflight. The binary is arm64, UUID
  C505BD31-BD99-3274-8134-DE7ED4BD796A; matching dSYM UUID is verified.
- Actual Release TU flags include-O3/-g. CMAKE_BUILD_TYPE=RelWithDebInfo is not
  selected Ninja multi-config evidence; selected Release commands are retained.
- Shader-source metadata is OFF in the current accepted metallib. Function/pass
  attribution remains available; line-level profiling may require a separately
  labelled optimized metadata-only diagnostic build after the initial baseline.
- Real host M1Max64GiB/macOS26.5.2/Xcode26.6; doctor/private receipts retained.
  Storage available5.8TiB. No active root-controller/launchd diagnostic env keys.
- AlchemyNext/user_settings copied privately before launch. Saved setting values
  are context only; live verification will establish actual values/activation.
  Exact Home/club SLURLs come from the private verified destination map.

## Plan and ownership

Root exclusively owns canonical docs, runtime/session configuration, UI, recording,
exports and final diagnosis. Peers audit tool qualification, current setting
consumers and source/capture maps read-only; no heavy parallel work during measured
intervals. All raw logs/settings/recordings stay private under ignored
artifacts/metal/performance-investigation-20261122. No private account/region data
belongs in the versioned report.

1. Preserve as-used config; establish exact PID/args/relevant env/live settings and
   real drawable/3D dimensions. Use saved credentials; no plaintext credential CLI.
2. Preserve a brief as-used observation, then qualify a20sOverview pilot: target,
   CPU symbols, positive GPU families, actual first/last/gaps, clocks/native tail.
   Assess capture overhead with nearby capture-off intervals.
3. Capture separate120–180sHome/club Overview intervals, same executable/quality/
   resolution, labelled loading/early/late/stationary/camera/focus transitions.
   Automatic GPU capture stays off. Validate retained data, not recorder exit alone.
4. Resolve limiting uncertainties with<=5sthread-state/Metal System Trace detail
   and complete-device early/late GPU frames where justified. Preserve captures;
   replay working copies, profile actual passes/pipelines/shaders in Xcode.
5. Analyze current exclusive CPU and actual GPU work/dependencies against source;
   separate frame statistics, GPU envelopes, replay and scanout. Rank proposals
   with measured potential, risks, smallest experiment and acceptance criteria.

## Known collector boundaries

Installed Overview template SHAacb55044577f59565776ae995a12b6b7007f202587b5467e02914b78ae989938
has windowLimit0 and CPU/Overview/thermal instruments. Detailed Game Performance
has a5srolling window; it is not the continuous Overview. Use current
quiet_capture.py directly, not historical hardcoded controllers/19retired overrides.
Native telemetry is startup-only,8192ring/600s, default64MiB/max256MiB; a reader cap
never extends writer retention. Pilot growth/tail and loss/footer must be checked.
Current helper exports CPU+GPU family coverage but does not qualify useful symbols,
low overhead or phase attribution automatically. No privileged sampler has been used. Owner subsequently permits local operator
authorization if needed; GPU MHz remains unknown unless actually exposed.

## Observed findings and failures

No current frame-time diagnosis yet. Historical Home probe-dependent GPU growth,
club mixed CPU/GPU limitation and earlier-wait/slack effects are hypotheses to retest.
ResourcePlan hit ownership and recurring lighting allocations are already repaired;
removed experiments must not be diagnosed as active paths. Actual native AA types
are none/FXAA/SMAA; quality0–3 is not MSAA sample count. MaxFPS explicitly declares
itself obsolete/unused and has no active limiter consumer; do not report it as a cap.

## Immediate executable action

Finish matching symbols and file-triggered read-only live-settings helper; launch
exact69450 at verified Home with preserved settings. Collect as-used/pilot evidence,
then validated Home and club recordings. Keep this request investigational; no
broad changes or commits/pushes. Record new defects before diagnostic repairs.

## First runtime observations

Exact69450 PID33661 started at00:20:24UTC with no graphics/session-preset override,
no native telemetry or GPU capture and idle-triggered LEAP. Runtime log confirms
OpenHTJ2K0.27.0 pixel decode, General1/ImageDecode6/LLImageGL1 workers. CPU dSYM
UUID matches the exact executable. Initial UI pixel size3456x1916 was windowed;
owner then explicitly requires all runs fullscreen, and native full-screen entered
before measured collection. No windowed run is performance evidence.

PPD-566: actual observer query retains four invalid_type rows because LLSD storage
is numeric-string despite numeric declared controls. Preserve original artifacts
and repair only the private observer to label raw vs parsed values before next
launch. Fullscreen observed UI then identifies the actual club region, not Home,
after login-page pointer interactions; label the current as-used/pilot Club, never
Home. No recording had yet started. Actual UI statistics approximately17–18FPS
and61msrolling mean are a point observation, not a timestamped raw-frame sample.

Pilot-club01 exits0. Retained CPU20.646s/27575target samples (maxstartgap12.0ms);
both positive GPU families cover20.011s (maxbucketgap1.025s), no zero/invalid rows.
Trace0:24:59.633–0:25:20.531UTC; actual recorder/export/size receipts retained.
Matching dSYM UUID passes. Current UI full-screen3456x2168, not initialwindowed
3456x1916. Capture-off rolling UI observation after pilot15.9FPS/65.5msmean vs
earlier17.4/61.3 is confounded by loading age/crowd; no isolated overhead claim.
Native telemetry was off, so this pilot provides no calibrated raw app-frame rows.
Initial session quit normally via CmdQ before the bounded-telemetry main run.

PPD566 private helper repaired with raw numeric-string storage retained and parsed
view separate; original helper/snapshot preserved. Syntax/import0; real next-launch
query remains required before treating repaired values as established. Fullscreen
owner instruction is mandatory for all measured runs; Mac FullScreen setting is
ignored by init, so use the native full-screen window action and verify pixels.

## Baseline completion checkpoint —01:04UTC

Both main recordings now qualify actual continuous target CPU and positive GPU
families, with automatic GPU capture off and full-screen3456x2168. Home exact
PID36864 trace00:40:34.031–00:43:35.778UTC retains181.175sCPU/229498samples,
181.009sGPU/maxbucketgap1.004s. Club exactPID38547 trace00:53:31.682–00:56:33.303
retains181.010sCPU/438088samples,180.987sGPU/maxbucketgap1.066s. Each includes
pre-arrival, loading and stationary late work; phase windows must exclude the
pre-login portion. Club population and parcel landing movement are uncontrolled.

Home off/on/off control completed on the same process without requested camera or
quality changes. Use actual20sOverview bounds for on, and exclude recorder startup
and export/finalization from off windows. Final native Home footer has264
contention drops, no full/budget/io losses; command lifecycle retires completely.
Club telemetry reached its explicit600s duration cap after the measured trace
ended;298contention drops,15committed commands pending at that later cutoff.
Neither stream is lossless; assess FRAME continuity and interval relevance before
quantiles or command accounting. Preserve all raw footers and failed pre-login
recordings; they are not Home evidence.

PPD566 repair now passes complete live pre-login and in-world requests: raw numeric
strings retained with separately labelled parsed values. Existing LLPanelLogin
event API solved saved-account submission without credentials in scripts.
Previous manual/autologin attempts remained at login and are explicitly excluded.
Native full-screen action is required: the Mac initialization ignores saved
FullScreen. Quit observations must not call CUA getApp/getAX after process exit,
because getApp can relaunch it.

Next: parallel private analysis while viewer is closed, then narrowly targeted
wait/submission and full-device GPU captures. CPU matching symbols useful; accepted
optimized shader source metadata remains off. No causal FPS diagnosis yet.

## Targeted follow-up checkpoint —01:15UTC

Baseline native FRAME analysis confirms a Home age trend: mean app/display periods
35.234ms at0–10s,47.993ms at30–60s,57.198ms at60–120s and61.528ms at120–180s.
These are uninterrupted eligible application periods, not scanout quantiles. Late
Home main on-CPU share falls to36.5% while late Club remains98.7%; GPU Overview
associated spans overlap and cannot be added to CPU or converted to hardware busy.
Club late native periods52.065ms and no paired explicit wait wall in that phase.
Precise sample windows, losses and estimates remain in private reductions.

Home GPU diagnostic exactPID49606 preserves all66 live controls, observed native
full-screen3456x2168 and independently verified world destination. Fresh startup
enables only telemetry/capture controls, no graphics override. Early2s and late240s
complete-device display-interval captures both stop successfully, include ordinary
probes/uploads, and remain separate from capture-off baseline FPS. Stop/save itself
takes seconds and is excluded from performance interpretation. Accepted optimized
shader metadata remains off; inspect named PSOs/resources/counters first.

First3sMetal System Trace+Thread State Trace attempt exceeds its observed2GiBraw
limit during finalization at2.398GB; marked failed, recorder-only cleanup, partial
evidence retained. It supplies no timing conclusion. Retry is2swith measured-size
justified8GiBobserved cap, same exact target,90sfinalization bound and45sexport
bound. This is diagnostic retention adjustment, not a renderer change.

## Collection completion —01:27UTC

Four current exact-build full-device captures stop successfully: Home early/240s,
Club early/60s. Fresh66control snapshots and actual process argv/filtered diagnostic
environment retained for each variant; allnativefullscreen3456x2168. Capture-enabled
variants are diagnostic only, with visible asset-arrival differences. Originals
remain private, separate APFS working copies opened inXcode; no native framebuffer
quality changes or embedded-source rebuild.

HomeMST02 retry saves0 with2.062GBobservedraw/77.707MBfinal. ClubMST01 fails90s
finalization while copies/analysis also run; preserve it. ClubMST02 repeats same2s
request alone,180sfinalization bound, saves0 with4.355GBobservedraw/179.386MBfinal.
TOCs exported; actualtargettablecoverage/clock joins still require qualification.
No raw/control exit alone supplies a performance conclusion. Lastclubviewer closed
normally; allcapturework andfinalization finished beforeparallelanalysisresumed.

Next: complete native/Overview overhead and phase analyses, boundedMST exports and
command/thread/display joins. Root alone controlsXcodecurrentGPUworkingcopies;
inspectSummary/PSOs then profile with otherheavyjobsidle. HomeLate replays without
profile:21commandbuffers,45render/5compute/56blitencoders,7147drawcalls,5dispatches;
3456x2168BGRA8. NoGPUperformanceattribution fromreplay hasyetbeenestablished.

## Source and replay checkpoint —02:02UTC

Private reports now cover native cadence, exact aligned CPU stacks, configuration
provenance, positional GPU counter CSV and qualified MST/native command joins.
Home baseline late required-main CPU17.588ms/admission wait38.586ms; Club49.974ms/0.
Home late aligned main CPU22.096ms per associated displayed frame versus Club51.213ms;
scopes differ. Home Overview off/on/off means65.683/66.170/66.672ms, no isolated
large Overview penalty. Telemetry remains on in that bracket; net overhead unknown.
All baseline FRAME iteration IDs are contiguous despite other contention losses.

Actual GPU captured main-frame probe counts are Home early1/late39 and Club late13,
not the corresponding later baseline sample count. Same main-base FS invocations
7,520,256 in early/late, raw GPUTime ratio8.136. Haze scope contains later alpha/PBR
work; its pixel/geometry mix changes. Shader profiling confirms late Home dominant
BASE candidates26.80% and simple-alpha candidates26.25%; PBR6.15% is smaller. Missing
candidate variants are not the main explanation. Existing prepared masks use16px
tiles/eight32-bit words; this is distinct from32px native lighting hardware tiles.
Per-probe accepted SH/radiance versus rejected traversal remains unpartitioned.

Xcode Overlapping/Medium replay: Home early64.55ms, late135.45 then134.39ms, Club81.37ms.
These are replay diagnostic results, never live FPS. Encoder CSVs and shader/UI
receipts retain duplicate rows/columns and unknown blank spill fields. One malformed
Club CSV record is preserved uninterpreted. GPUTime CSV absolute unit is still not
independently confirmed; dimensionless ratios remain valid. Selecting Counters starts
unneeded draw-counter replay; it was explicitly stopped, no results used. A prior
Summary insight selection during profiling saturated Xcode AppKit layout; sample
retained, only root-opened Xcode trace session force-quit via native dialog and
restarted. No raw capture or source was modified by that recovery.

Home short MST common1.798s:46substantive older-main lighting/post waits878.976ms,
median20.826ms;98.75%overlap waited command GPU envelopes, callback-tail median0.248ms.
Waited commands are two display tags older. Exact create/commit-bracket joins map
421/423Home and536/538Club submissions,0ambiguity. Club~789ms MST hole is missing
data: native15frames/268commits/269timed commands continue; exclude it, no idle/CPU
interpolation. Two larger instrumented Home blocks require immediate mutex/capture
stack attribution, not a baseline cost assumption. Runtime queue IDs unavailable.

New PPD567 tracks repeated immutable49,216B forward-probe hashing (Club2.167ms
exclusive sampled cost affected, not promised savings);568 retains unattributed
profiled owner block;569 retains early missing-binding replay insight. Retired owner
release already occurs outside registry lock, contrary to a generic deferred-release
proposal. No production fix authorized/implemented. PP319/408 histories retained.

Specific next measurement: source-line/accepted-tap versus traversal attribution on
an optimized metadata-only copy. Fresh private CMake configure and shader-only target
complete0; flags differ only by-gline-tables-only/-frecord-sources; shader text identical.
Normal required tools/metal/build.sh incremental Release check also0. Canonical cache
metadata remainsOFF; owner-accepted app untouched. Root will run private guarded
clone/metallib-only replacement/signature+CPU-section/inventory verification, then
exact full-screen Home capture with this separately labelled diagnostic variant.
Do not reuse current frames as if they contain missing metadata. No commits/pushes.

Pending root sessions: Home final syscall/display export27876; clone helper under
EV/helpers being authored by worker agent, not executed. Other agents finish MST
classification and private review; pause heavy work before the next capture. Xcode
currently stopped at HomeLate Replay landing, no timed profile/recording active.

MST syscall correction: the157.0465ms captured Home block is __ulock_wait2 through
Apple IOGPUMemoryInfo/AGXBuffer/CaptureMTLDevice into ALMetalBuffer::create. Nearby
graphUse sampling was not its blocked stack. PPD568 updated without erasing history.
Normal older-main admission waits separately show MTL waitUntilCompleted; no generic
registry/deferred-release repair follows from the long instrumented allocation.

## Metadata-stage checkpoint —02:16UTC

Diagnostic compiler/metallib build succeeds0 at02:00 with normal Metal math/default
optimization and only source/line metadata flags added. New library SHA
eb6f8d8e78cafcd838b0753e0c7108c505ef352cc0afe90bdad022d8977a205d.
No new app has yet been published/launched. First guarded stage fails beforecopy
on absent Darwin Python os.listxattr (PPD570); corrected bounded libSystem reader
passes actual native binary/empty/symlink xattr fixture0. Secondstage fails beforecopy
on commented implicit ad-hoc designated requirement (PPD571); actual zero stored
requirements and self-CDHash must be distinguished from explicit requirements.
Worker exclusively corrects private helper under optimized-shader-source-diagnostic/
helpers; both failures/original helper variants retained. Root executes staging,
strict/deep signature and346MachO checks after guarded success. Existing accepted
app and canonical shader metadataOFF cache untouched. Required normal build.sh0.

Xcode currently closed (processabsence verified) to release induced profilingstate
before nextviewer. Fullscreen newHome metadata run remains planned, notexecuted.
Private launch_session.py now supports --identity inside EV and verifies a supplied
metallibhash aswellasEXEhash; create new metadata identity from successfulstaging
receipt before launch. ParentacceptedCPU source checkpoint/dSYMremainunchanged
if full non-signature MachO/UUID proof passes; signing maychangewholeEXEhash.

Unit reconciliation nowestablished: rawGPUTime/1e6 sum matches independentXcode ms
UI aggregates within rounding for all4CSV exports. SelectedearlyGBuffer raw
2,975,261.58ns rounds toUI2.98ms withmatched1,739,384vertices/1095UIdraws. Thus units
ns, but UIheadline is SUMofindividualencoder costs, not concurrentreplay makespan.
PositionaloriginalCSV/duplicates/malformedClubrow retained; no additiveframeclaim.

PPD569 exact warning read withoutprofiling: genericGBuffer diffuse_textures fragment
texturebindingindex3; sourceunused-slot/selectorbounds review pending. Noillegalread
proved. Sourceaudit ischeckingcaptured early/late SSR/input-feature differences before
causally attributing the8.136x same-fragment base cost toprobe countalone. Dominant
latefunctions alreadyusecandidatevariants; peracceptedtap/rejection splitunmeasured.

MST finalreportscomplete: syscalldriverallocationattribution resolves157msHome and
212msClub capture-enabled blocks; noResourcePlanmutexcause. Club789msmissingdatahole
strictlyexcluded. RuntimequeueIDs/scanoutlatency andone58msHomeblock unknown; no new
recordingneededforbroaddiagnosis. Private recommendationdraftfinished; rootfinal
readablereport/configrestoration/harness stillpendingaftermetadatafollowup.

## Final collection, attribution and handoff —2026-11-22

The [readable investigation report](../../research/performance-investigation-20261122.md)
is authoritative for final matched statistics and engineering decisions; earlier
checkpoints above retain their then-current unknowns, not competing final results.
No production code/quality change, commit or push occurred.

Guarded metadata stage03 succeeds0. Independent root deep/strict signature and
verify_app both0(346ARM64). CPU sections/data/dyld andUUID remain equal under
documented signing-field normalization; whole executable hash changes with signing,
so this is not whole-file or GPU-native-code equivalence. All6entitlements/runtime
flags preserved. Accepted app/canonical metadataOFF cache untouched.

New exact source-metadata app PID66557 starts02:21:15UTC, native fullscreen before
login, STATE_STARTED02:22:20 at independently verified Home. Early192/193 and late
4268/4269 complete-device captures stop. Actual main packs1/55; SSR1/25/4,hero0,
3456x2168,all66controls match normal reference across prelogin/arrival/end. Last
viewer quits normally02:31:17, native footer requested with143899created/committed/
completed,217contentiondrops,0full/budget/io and0pending. No stage-timing control,
graphics reduction or requested camera move. Source diagnostic is excluded from
FPS comparison with the accepted library.

Xcode source metadata is actually visible, not inferred from flags. Overlapping/
Medium profile: BASE root37.73ms/29.70%aggregate, sample_base_probes66.40%root,
tap27.78%,SH11.77%inside tap,holds10.04%; SSR1.91%. Simple-shadow root30.39ms/
23.92%aggregate, samplebase57.51%,tap23.03%,SH13.52%,holds7.75%. All inclusive
percentages overlap; no exclusive traversal subtraction or reject/accept counts.
127.06msUI is aggregate isolated encoder counter cost, not replaymakespan/FPS.
Originals immutable; separate Xcode export with Embedperformance data completes,
770148040byte gpuprofiler_raw SHA256
19fb1f866aacd7d20c5eaf4f3dff20cf83e61155fcbbec7fd4e10059a8099d5b.
Viewer andXcode processes absent after normal quit; no administrator prompt used.

Strict matched Overview-late native reductions supersede full120–180s rows for
baseline comparison. Home725periods mean60.586882/p9573.151159/p9978.846294ms;
Club847 mean51.868873/p9558.044333/p9961.116734.727/848Overview associations are
different measurements over the same43.989swindows. Earlier full180sphase tail
includes~15s outsideOverview; original rows retained/labelled, not silently reused.
Private analysis-native-matched receipts include exactMach bounds and±33ms
alignment sensitivity. No native FRAME gaps within qualified baseline windows.

Final preservation audit:66graphics/thread/scheduling keys,18stored/48absent both,
0value/type differences. Whole settingsfile changes normally outside the audited
whitelist; no blanket restore. Accepted8273-path inventory including modes,
symlinks/xattrs remains equal; executable/metallib/canonical metadataOFFcache equal.
Private analysis-configuration-preservation retains audit/helper/inputhashes and
current consumer map. No credential output or owner assistance required.

Ranked first patch proposalPPD567 retains invocation-local lazy deduplicated slice,
bothslot1/3bindings andcompletionpages.2.167msClub/0.424Home exclusive sampledhash
work affected, not promise. GPUfirststep is<=1024receiverBASE-only bounded census
with fixed indexed outputs/noatomics; oldPPD353 predecoded coefficients and rejected
scalar/tileSH experiments are contraryevidence, not restored fixes. Preserve
manual/order/accepted32/neighbor64/defaultcontribution afternonzeroaccepts.

PPD568driver allocationattribution resolved, ordinaryimpact unknown; separate58ms
block unknown. PPD569generic32-slot sourceguards support unusedsuffixcount<=3;
inspect exactwarneddraw/count/binding before diagnosis. Allissues remainregistered.
Normalbuild/doctor/appchecks0; final harness and independent reportreview retained
at handoff. Product release gates and the implementation backlog remain open.

Final review: three independent source, MST and native/configuration reviews
complete. Corrected captured Club129 lights versus the separate128 baseline,
recorded-pressure denominators, GPU-end-to-wait-return terminology, command-buffer
capture boundaries, guarded32-slot warning status, sortShadow query/cull/state-sort
scope and unmeasured material/SSR confounds. No inclusive-cost sum or FPS promise.
`python3 tools/metal/validate_harness.py` exits0 (33required files), and
`git diff --check` exits0. Final logs/receipt are under EV. No tracked source/tool
changes, commits or pushes.
