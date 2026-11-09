# Current engineering state

**Observed:** 2026-11-09, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `7b03110f71c69455bf45da6626058ad917026998`.
**Working source:** focused replay/profile completed; viewer closed/restored, GPU idle.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal and controlled baseline

The unbudgeted20FPS Ultra goal remains ACTIVE/unmet. Autonomous quit/build/login/
measure is authorized at the fixed private location/numericPreset123 only; no
travel. Prefer Mac shortcuts. Preserve Ultra3456×2168/Midday, all six hero1024
faces/two sun cascades, PCSS/clamp/casters and functionality. No private uploads.

Latest controlled68579/source1f2cc0 completed300 consecutive frames5032–5331:
**9.608672762FPS**, mean104.072646ms, p50103.8956665, p95109.0435ms. All native/
noGL/Release/Ultra3456×2168; bias1;256 probes fully ready/stable120; main-visible95.
Fresh start/end43-control/location/exact-binary gates0; environment/sun/hero/HUD
restoration matched. Sampled camera changes0; basis/projection matched; vertical
position differed30.52 micrometres from68572/68577. Drawmean55781.49, +3.591%vs68577.
No build/test/profile/heavy parsing overlapped the formal window.

FPS +19.94%vs68577(8.011210), +42.80%vs68576(6.728847), +2.414%vs68572(9.382155).
Translator GPU work was removed for68579: this is a changed control, not pure
source attribution. Target20FPS is unmet; hardware infeasibility is unproved.
RawCSV/provenance: artifacts/metal/private-1f2cc0-family-diagnostic-20261109-01.*.
Independent comparison SHA39a27d619640cdf02a3db9d85769f145beca2323a5a41b128e70773b25bc00ce.
Images7599MiB/device9483–9510/target45985–46006/recommendation59392/system-low0.
Native query119002 spatial skip EVENTS/299 intervals, not unique groups/savedtime.

## Runtime and external service

All viewers CLOSED.68581/PID97769 exact executable verified, gracefulSIGTERM,
PIDabsence and run-specific viewer.log Goodbye verified. Newb52c1b Middayplan
RESTORED actual0, all earlier plans restored too. Shutdown checker initially
looked in wrong log directory and exited1; corrected to --logfile from run.json
BEFORE restoration. No active CUA.68579 and68581 apps preserved with exact hashes.
Canonical68579 and preserved `/private/tmp/alchemy-metal-68579-controlled-measured.app`
executable bf4cdfef2fe95cf91991f26e4f948328878fa8627871100825d54b11fe6d69f5;
metallib ffd9e1d178988b0719c516cd1e87444907d59d063630d7fd9ac2d17cafaa9347.
Preservation ditto/hash check0. Doctor/configure/build/349ARM64/signature0;
full426444 assertions/91cases EACH Release/RWDI API+GPU0. No screenshot claim.

Translator user instruction is FULL STOP, superseding prior pause/resume.
`uv run --no-sync python scripts/service.py stop` from its project exited0:
API35823/tailscaled35866 stopped; GPUworker35824 absence verified. DO NOT RESTART.
No SIGSTOP was issued. Earlier68577 GPU trace attributes33.98%wall to Python35824,
7.30%outside viewer. Stop record private-1f2cc0-translator-stop-20261109.json.

## Measured GPU attribution

Post-formal68579 diagnostic trace75165 and serial exports/parsers all0.131
presents/15.706s are instrumented8.3405requests/s, separate from formalFPS.
Viewer Active GPU interval UNION103.5406ms/p (86.3584%wall), device87.4914%,
thermalNominal. Initial forward family union2.61622ms/p: avataralpha2.16112,
fullbright.31048, glow.17212, shiny.06002. Six contiguous encoder runs preserve
draw order; added load/store boundaries alter scheduling/cost. Do not equate
these with unsplit production cost or individual shader duration.
Largest inclusive contexts: heroabove-water21.067, mainbase17.333,
mainhaze/latertransparent15.716, mainGbuffer9.108, heroHaze8.333,
heroGbuffer7.616, heroShadows6.271ms/p. Labels overlap; never add them.
Translator actor has zero Active rows. Other GPU activity still exists;
nonviewer outside union.77878%wall. Family attribution FINDINGS SHA
 eebc1718407318251aae95dd77241329fe462974e7979e6f537c1e4a757dfa13.
[Complete source/build/runtime evidence](archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Active implementation checkpoint

Root owns shared source/docs/runtime.7b03110f71 adds defaultOFF MODEmain-base to
existing capture owner: same successfulbenchmark/disarm/60cooldown gate, then
ready-for-pass. Bypass outerlightingbatchONE maininvocation; capture actualbase
freshownedCB aftersceneprep/creationthroughcommit, stopbeforepublication/local
lights. Mainkind1/noSnapshot/noactivebatch; postincrementframestamp; noexcept
cleanup and failedrender/unwind cancellation. No shader/quality/cadence change.
Final serial60712 actual0: capture2041/3 API EACH Release/RWDI (shader-validation
UNSET), fullnormal426583/93 API+GPU EACH0. Independentreviewapproved.
App68584/source7b0311 built45995/349ARM64+signature87002 all0, shader sources
recorded with unchanged compiler optimization; DIAGNOSTIC, not FPScomparison.
Exe b428b6e391358547e4263e5a87787a3761cd22832335b93cc2a680925a017624,
metallib9e4fa00fb75f63660a1790abcf4314f08e72a41d22878db015129b17a0a86f1d.
68584 fullscreen/login/start+end43control/location/binarygates0. Diagnostic300
framescomplete/ready256/camera0. Capture6422→6422,1attempt/error0/stopped;9.03GiB
toplevelresources, actualcommands/replayNOTyetverified. PID21257gracefullyquit/
Goodbye/PIDabsence;7b0311MiddayplanRESTORED73096actual0. TranslatorSTOPPED.
Owner CANattend Xcode (corrected can't); send exactnewcapture/steps WHENREADY.
Existing68581 countertables4exports all0 butZEROrows; realshadercountersunknown.
PrivateSSR strict64cases0; bracketedconfirmationcompile/72casevalidation/timing86820
all0.32miss13.37%faster12/12, robust tolocalcontrols/targets/order; controlsnear0.
No liveFPS/cubeSSRgainclaim or sharedSSRshaderedit; regressiontestpreparedprivately.
[Capture and experiment evidence](archive/CURRENT-selected-base-capture-2026-11-09.md).

## Latest candidate result — no measured improvement

68581/sourceb52c1ba969:300 consecutive5032–5331, mean105.768184723ms,
p50105.811167/p95110.90525, **9.454638960FPS**. Versus68579 mean+1.62919%,
FPS−1.60307%,p50+1.84368%,p95+1.70735%. Both TranslatorOFF, all native/noGL,
43controls/environment/bias1/all256ready/stable120 match. Camera basis/projection
match, Zdelta26.70micrometres, no within-window changes. Drawmean55752.2233 vs
55781.4867(−.05246%), mainvisible96vs95; post-sample main batches831vs858.
No competing build/GPUtest/profile/heavyparse during formal. One pair does not
establish causality; flag eligibility is not an observed FPS win.
Officialenforce/provenance exit1: provenanceverified, performance target and
three other required scenes unmet. CSV SHA384cb3f21fdb1558529f671fcbc727739e6d458276076d78f22d187bdbd0c7dc;
provenance a2e345a069f0fd5675de358cab2295eb546f01063bf06e29cb9297f540a10c47.
Independent report82095ff3b65c73157107b3ef4c7cd3b55dced86a0e77316c147b903612efdb6b.
App `/private/tmp/alchemy-metal-68581-texture-usage-measured.app`, exe
3d3977af32ff663b19bfef4a9f3dbf5c88d6721f0a105a5d0de0e10f6595fb0f; shaderffd9 unchanged.
Postformal15sGPUtrace68510 actual0; sourceDWARF warnings duringfinalization,
trace saved;8serialexports/submission/union/family/actor parsers allactual0.
ViewerGPUunion105.542vs103.541ms/p, mainbase17.331vs17.333, heroabove20.709vs
21.067, mainhaze15.909vs15.716. Initialavatar5.350vs2.161 despite same3/9180
sourcecounts; mainGb6.711vs9.108. No broad reduction; overlap/diagnosticcost and
extra glowbatch prevent shader-cause claims. PacketSHAfa94e6826f98bbdbb9d137eb6651996ad1a8d321a1335cb5468d0e1f0d3ce877.
All agents/exports/parsers quiet; no active rootGPU/build/viewer/runtime processes.

## Retained quality baseline and constraints

Preserved68572/source46d622 baseline9.382155FPS/mean106.585321/p50105.948188/
p95110.883167,bias1;68576/source0fa4e3 6.728847;68577/source30715f 8.011210.
Prior external GPU controls were unrecorded. Keep3969405 image/global scopes and
46d622 C=max(R−max(A−I,0),0), then original divisor/headroom ONCE; manual/global/
system controller unchanged. Normalbias1 is not per-texture/pixel parity proof.
Keep per-view probe correctness/shiny bypass/bindings/real occlusion/Ultra4×/
bounded lights/directR32 shadows/borroweddepth/projectors/arenas/caches/retirement.
Original67611/exact1eb+observer6.095519FPS,bias1/main104; broad parity remains open.
OldCPU68566 trace lacks matchedframecounts: no77ms CPUfloor claim. Rejected
classic/SSR/materialsplit/layered candidates remain rejected; no repeated trials.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-004 Resource ownership | doing | Image/global scopes corrected; compression eligibility candidate. |
| M-008 Advanced views/effects | doing | Cube SSAO parity fixed; main and mirror GPU costs remain. |
| M-012 Native UI automation | complete | Exact fullscreen/login verified; world AX/capture timeout limits. |
| M-014 Ultra20FPS campaign | doing | Best controlled9.61FPS; new68581 9.45FPS shows no gain. |

## Exact next actions

Focused Xcode replay/profile COMPLETED:1CB/1render/2draws,31.17ms atMEDIUM PState
(notliveframeFPS), geometry99.68%/216registers/0spill.7,176,832FSinvocations,
43.22samplercalls/FS,37.460B ALUinstructions; ALUlimiter44.54% vsTextureRead6.01%.
Per-lineprofile unavailable(missingdebugtables/deployment26.0vsdevice26.5.2);
occupancyUNKNOWN. StopGPUworkload→Finishedapp observed, CUA summarysent. AllGPU
processes/viewers closed, allMiddayplansRESTORED, TranslatorSTOPPED.
XcodeAutoRGB0..0.218/NoColorMatch stretchespreview; finalsourcecolororderingmatches
originalwithonegamma conversion. No colorfix/settingschange indicated. ActualQA
SSR=0; validatedSSRsharing hasno livecurrentraymarchbenefit. Keepitprivate.
Next: rootexecutePRIVATE SHlive-range experiment beingpreparedbyshadow_contract,
current216regs justifytest butcompilerbenefitunproven. Preserveformats/4SHreads/
FPgrouping/32cap/neighbor/boxrules; priornamedaccumulatorsrejected, notrepeat.
AllactualXcodemetrics/artifact/hash/detail in[focusedcapture record](archive/CURRENT-selected-base-capture-2026-11-09.md).
Fullfeature/pixel/multi-scene/prolonged stability/strict no-GL and
independent build/review evidence OPEN.20FPSgoalACTIVE/unmet.
