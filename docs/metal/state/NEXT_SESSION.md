# Next session — stage-timestamp validation and bounded GPU attribution

Read AGENTS.md and CURRENT.md completely. Goal20FPS Ultra is ACTIVE/unmet;
infeasibility is unproved. This turn made concrete progress: committed/rebuilt
the direct default; compiled/GPU-tested whole-shadow parallel encoding; measured
and rejected two complete-cost revisions; restored the exact baseline source.
Do not create a duplicate goal or mark complete/paused/blocked.

## Ownership and runtime

Branch codex/apple-silicon-metal. Renderer checkpoint
06d16eb4cb20f4dfe47b4ce71e86e1420eb9e060. ALL indra source equals this commit.
There are no uncommitted renderer changes. No viewer/GPU/profiler/build
running. Prior three viewer sessions ended with exact exits/Goodbye/absence and
all their environment plans restored. Translator FULLY STOPPED/NEVERrestart;
Xcode replay STOPPED. Root owns shared source/docs/runtime/GPU. Agents use
private source/read-only work; never concurrently edit shared renderer files.

Owner authorizes autonomous quit/build/login/measure at SAME private location
and Preset123 only; no travel/avatar motion. Preserve Ultra3456×2168/Midday,
six1024hero faces/two cascades/PCSS/rawR32/reverseZ/clamp/allcasters/features.
Use Mac shortcuts and fullscreen BEFORE login. No private uploads/releases.
Owner sees fullscreen GameMode activation banner: leave mode unchanged.
NEVER bind CUA ControlCenter/com.apple.controlcenter (7654s unsupported-target
stall despite60s timeout). Exact listed Alchemy app works; AX world queries
can take30–42s. All prior CUA phases have completion_summary already.

## Ready baseline app68606

Canonical /Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app.
Preserved /private/tmp/alchemy-metal-68606-direct-default-preserved.app.
Actual source06d16, version26.4.0.68606, executable
ac719b12a34964c6d34ac7850f24a0e5dc2a70a626216420ad7cac1b221615ec,
metallib3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2.
Doctor/configure/build/verify/deepstrictsignature ALL0,39343terminal0;
349ARM64/0bad. Packet ys8mhyv4, independent identity review zukzcud7/e614f1e3.
Source recordingOFF/C++23/O3/NDEBUG/arm64/min26.0; actual45responsefiles reviewed.
Preservation42077terminal0/deepstrictsignature0. No new live FPS run of68606.

Direct source default material_buffers=false; ordinarydiffusesamplerABfalse;
experimental material sampler remains lazy, quality-identical and safely reset
on anisotropy change. BOTH baseline optimized API+GPU suites435462/102 PASS;
all eleven per-file source hashes still match after prototype removal.
NINJA TEST ARTIFACTS STILL CONTAIN REJECTED PROTOTYPE. REBUILD before reuse.
Normal68606 app is correct and unchanged; do not confuse the two build trees.

## Exact next executable action

The standalone stage-timestamp fixture is COMPLETE: root compile/nativevalidation
both0, actualAPI+GPUenabledlogs,8resolvedsamples/twopasses, exactfullRED/BLUE
pixels/cancel, CPU_ns/Machclockproof. Source926da7bdeb9fbe2a3c8dc9bc81cb89d5e0ad1b0760800ca0d3a2394f132a1a36,
packet /private/tmp/alchemy-metal-stage-timestamp-validation-run-20261110.veybvjku.
Result3833ed66/binary424df499. No viewer hook or live timings yet. Do not rerun
capability/standalone gates without a new defect or source change.

/root/shadow_contract now owns a NEW PRIVATE minimal explicit
ALMetalStageTimingWindow helper+GPUtest, based on06d16 (no parallel prototype).
/root/trace_recheck reviews it. No shared edits/build/hardware by agents. Obtain
its delivered source packet/manifest, review and integrate helper/CMake/tests.
Root owns backend hook in ALMetalRenderEncoder::begin plus display/view gates.
Use a caller-owned bounded window,4freshindices/physicalpass, copieddescriptor,
buffer-only completion owners(noCB/token cycles), external completion polling,
strict decode/calibration, and unavailable unsubmitted/cancelled records.
Failing diagnostics must leave ordinary rendering valid; default off and no
extra wait/commit. This is next implementation, not a general profiler.

Current query packet /private/tmp/alchemy-metal-counter-capability-run-20261110.j3oo8ios
compiled/ran0: M1Max only timestamp/GPUTimestamp, stage_boundary=true; all draw/
dispatch/tile/blit points false, no statistic/utilization sets. It created no
GPU commands. This REVALIDATES November8/archiveCURRENT-ultra-goal-experiments,
which already allocated four shared samples. No renderer counter hooks exist.

Important calibration correctness: Apple's sampleTimestamps CPU values are
ALREADY nanoseconds. Archived real CPU values agree with raw Mach ticks only
AFTER125/3 conversion. Do NOT multiply Metal CPU samples by the Mach timebase
again (would inflate timings41.67×). Use Mach brackets only to verify units;
GPU duration = GPUrawDelta * CPU_nsCalibrationSpan / GPUCalibrationSpan.
Reject zero/error samples, invalid spans and out-of-bounds resolve data.
Do not infer global pass order or sum overlapping vertex/fragment/pass spans
into frame GPU busy time. Actualstageutilization/invocation counters unavailable.

The standalone fixture now passes; implement a bounded
DEFAULT-OFF diagnostic hook in existing native encoder/pass ownership. Preserve
all attachments/shaders/cadence. Sample main+six hero views with actual frame/
view/face/pass labels and command completion owners; no waits/extra submissions.
Follow the existing formal-ready gate so measurements follow300formal frames.
Do not turn this into a general profiling framework or enable unsupported APIs.
Then rebuild/verify an exact app and measure currentGPUstage intervals to select
an actual GPU optimization. The next goal remains FPS improvement, not tooling.

## Closed whole-shadow parallel trial

Full core/source-preserving rebase B9lJuH/5780aa70, two-view test PcHZll/0570116c,
benchmark67b4kh32/35506dcf, optional validation-slotcache nVgDPI/30eeb373.
All sources independently reviewed; actual Release API+GPU full suites435842/
103 PASS for both revisions. Bridge100/1, focused rawshadow2210/1 include
worker failure/retry, ordering, invalidsource/cancel, all representativefamilies
and two distinct views before one commit. No candidate viewer built/launched;
RelWithDebInfo notrun because performance admission failed first.

Initial280quiet samples: serial→two CPU+19.20%640/+13.39%4096, all7slower.
Slotcache revision280:640+9.71% all7slower;4096 apparent−4.45% range−6.12..+9.32,
SAMEparallel−5.24% range−11.32..+9.43. No general win; REJECT. Eachrunpassed
6268assertions/8strictR32pairs/thermal0/schedule/provenance.140short640rowskept.
Synthetic12family32×32 streams; generation-per-call differs from viewer cadence.
Genericcallbackretirementunproven, GPUcontrolsswing: no FPS/GPUbenefitclaim.
Do NOT rerun these variants or activate a viewer trial.

Root froze full final patch63636129baa76c4fcff046ac196800e93078eb4064cacf349ffb005bc8d7c2c7,
reverse-applycheck/apply0 and source-map comparison0;13-file prototype changes
removed (ten existing files restored and three prototype files deleted).
Initialfulltestedpatch2ce2c74d833855a34604c92c8f633da3223b3eb5e79eecb061ba10e41c867d12.
ALL ten raw packets SHA-verified and preserved in ignored
artifacts/metal/private-whole-shadow-encoding-20261110/; preservation.json maps
original paths. See indexed CURRENT-parallel-shadow-encoding-2026-11-10.md.

## Latest live evidence and limits

Earlier68601/source779 ready300:9.833798FPS historical. Proper fresh control02:
9.410236877FPS, mean106.267250553/p50106.547583/p95116.651958ms. Experimental
material-cache68605:9.066674534FPS, mean110.294021947/p50110.364396/p95119.195042.
Bothstart/end43gates0/cameraEEPexact/native1/fallback0/bias1/256ready120.
Candidate draws+2.53%; no isolated code-regression inference, no gain shown.
Reference01 exploratory9.408707 lacked startgate; never fabricate one.
Post-formal10s CPU traces passed; hero~50%active main, shadowparent2.459/2.381s
inclusive/overlapping, not exactCPUms/frame. Narrow symbols differ with inlining.
GPUcompleted-CBsumsoverlap/lag, notcriticalpath. Allsixhero publication audited;
no persistent incomplete capture found. Original67611/exact1eb6.095519FPS remains
historical; full parity and multi-scene release-performance proof OPEN.

Retain all existing native repairs, especially64examined/32accepted probe
neighbors, per-view coordinate packs, bound lights, main occlusion, Ultra4xaniso,
rawR32directslices, arenas/resourceborrowing/image-global budgets/bindingcache.
Immutable regularprobeUBO1 passed200strict/168timings but no repeatable win:
closed/no rerun. SH/predecode/mask/ambient/scalarcursor/register variants remain
closed. No quality reduction/nativeD32 migration/epsilon waiver/fallback backend.
Fullpixel/featureparity/multisceneperformance/prolongedstability/strictnoGL/
independentbuild-review release gates remain OPEN. Keep20FPS goal ACTIVE.
