# Next session — build and measure the validated PBR candidate

Read AGENTS.md and CURRENT.md completely. Goal20FPS Ultra ACTIVE/unmet;
infeasibility unproved. Concrete progress: PBR code committed and both full
optimized native suites pass; isolated affected GPU workload improves14–18%.
No new viewer FPS result. The measured retained water build remains9.785FPS.

## Source and exact next action

Branch codex/apple-silicon-metal. Inspect HEAD/status; renderer candidate is
765c305b5ac4c7b897d6581ea2990de49d1e09f6. Later docs commits may be HEAD.
Five implementation files are committed: worldcpp/h/diagnosticsh/GPUtest/pipeline.
Normal viewer candidate has NOT yet been built. All builders/native tests/
profilers/viewers are STOPPED/terminal; no active handle. Root owns shared
source/docs/runtime/GPU; agents prepare private source and independent reviews.

NEXT: exact normal source build with tools/metal/doctor.sh, build.sh configure,
build.sh build, verify_app.sh and deep strict signature. Adapt the proven builder
/private/tmp/alchemy-metal-water-app-build-20261110.qyziw4c0/build.py into a NEW
private directory: replace expected_source with actual clean HEAD, output path,
and preserved baseline identity (68610/dc30 below). Never inherit old build0.
Check actual O3/NDEBUG/C++23/ARM64 flags and source-recordingOFF. Preserve the
completed log and actual response files. Independently verify app identity and
source/hash/compiler metadata before live use. Keep HEAD stable during builder.

Canonical build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app
is still68610/source8eea2c6895c1c907ce513a5c83872a631874a3bc, executable
 dc30d261ddbef780c6ffb459c2c87f9d08500785a062e1e834d5a4c49b0f2909;
metallib3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2.
Preserved /private/tmp/alchemy-metal-68610-water-specialization-preserved.app.
Verify that copy/exe/signature before overwriting canonical. Normal build may
have a different version number after docs commits; inspect actual Info.plist.

After build admission, repeat controlled fullscreen BEFORElogin, Midday
Super+Shift+Y, Preset123. Use installedalchemy-viewer-visual-qa launcher and
fresh43start/end observe→verify→gate proofs. Existing old launch templates:
/private/tmp/alchemy-metal-water-specialization-candidate-launch-20261110.py
and /private/tmp/alchemy-metal-water-control-launch-20261110.py. Adapt exact
paths/source/binary hashes/new run directories/OWN environment-plan names.
Use /Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/private-ultra-water-specialization-qa-20261110.json
only as a verified private target/profile template. No guessed region, camera,
quality, binary, source or plan. Warmup300s/256ready stable120 then300frames.
No stage/family/workload/capture env during formal. New PBR12-view receipts
report encoded actual truePSOs only; views without PBR need no receipt. Compare
with preserved68610, report probe/draw drift. Quit exact PID/Goodbye/absence then
restore only that run's OWN environment plan. All old plans already restored.

## Authority and runtime constraints

Owner permits autonomous quit/build/login/measure at SAME private location and
Preset123 ONLY. No travel/avatar movement. Keep Ultra3456×2168/Midday, all six
1024hero faces/two cascades/PCSS/rawR32/reverseZ/clamp/casters/features. Prefer
Mac shortcuts; leave fullscreen GameMode unchanged. NEVER CUA-bind ControlCenter
(prior7654s stall). All previous CUA phases have completion_summary.
Translator FULLY STOPPED: NEVERrestart. Xcode replay stopped. No private capture
uploads, quality cuts/nativeD32/epsilon waiver/fallback backend.

## PBR implementation and verified gates

Core privateb809d9e0 applied and retained in765c305. Exact original1eb PBRalpha
uses sampleDirectionalShadowGrad; shadowUtil cube_snapshot branch is single-tap.
Same current native helper/shader/library/math; only optionalFC0 is specialized
for actual shadowed PBR color, nonavatar/nondepth, validated probe count>1 and
actual bound shadow bit4. Standard8lazy HDR/coverage/rigid-skinned clones restore
ownedlibrary; customcache shadow-mode distinct; unqualified keys unchanged.
Directscene flag defaultsfalse, genericproducer true. Main/default/no-shadow/
DoF stay original. Actual descriptor census is timingopt-in/whole-success only,
13 independent view tuples,12bins. Producer logs at most12eligible views with
matching currenttuple. No ordinary unarmed getter/logging. All peerreviewpassed.

Integration /private/tmp/alchemy-metal-pbr-probe-fc0-integration-20261110.4savz8k7:
- measuredfixture9a4dd85e: actual API+GPU25379/1,68small+6large pairs0;
- quiet1 actual28122/1,168samples/R2/all7blocks/nominal0;
- dense rigid GPU5.519929→4.771719ms (mean−13.555%), skinned5.595585→4.602552
  (−17.747%), both7/7 faster beyond SAME controls; no CPU/FPS claim;
- all28out-of8–20ms rows are count1guard, retained; eligible dense/SAME inband;
- independent auditda5b8bb6/findings0f27e9d2 actual0;
- extra16nonHDR+4protectedDoF pairs: expandedAPI+GPU27593/1 passed0;
- full measuredsource/patch and expanded test are frozen BEFORE stripping.

Initial testbuild1 was Catch OR-parentheses. Preflight142 used invalidcutoff1.
Intermediate.99 compiled but NEVERGPU-run: source permits fixed originalcutoffs,
not numericrange. Correct edge is.004+zero_alpha_diffuse, acceptedprewaterphase
with depthwrites, and exactbackground/farD32 oracle. No tolerance relaxation.
Rawfailed attempts kept. Core/shader never changed for these fixture corrections.

Private benchmark31a41fa5/sevenimports removed, onecommentrenamed. Entirepermanent
strict source7ccb823c7313e0a64ed5d2d3fba6254f953fc0295442306957f39699c35aa2e3
reconstructed byte-exactly by root+peer; all88pairs/census/input guards retained.
Admission /private/tmp/alchemy-metal-pbr-fc0-admission-20261110.ln3y_bxk/run.py
build-release/full-release/build-rwdi/full-rwdi EACH0. BOTHoptimized actualAPI+
GPU494211assertions/104cases each0; all15finalfilehashes rechecked before765commit.
Library39320181 unchanged. No further native tests needed absent new changes.
Full details archive/CURRENT-pbr-probe-specialization-2026-11-10.md.

## Baseline and closed leads

Waterf549 normal68610 source8eea: build/verify/signature0,349ARM64, independent
identityfc35, bothfullnative482529/104pass0. Isolated eligible GPU−15.42..−17.17%.
Live300frames:68608before9.069479;68610water9.785333;68608after9.257293FPS.
Observed+5.704%versusfresh. Main-visible probes98/95/100 and drawcounts vary,
so no isolatedcode-onlypercentage. All43controls/camera/EEP/native/bias1/256ready
match, exactexits/ownplanrestorations0. Water archive has full run paths.

Probe whole-bufferconstant→device was implemented and rejected: denseGPU+5–6%,
all7slower, no CPUbenefit. Fourfiles restored before PBR; no viewertrial. Early
fragmenttests/materialABcache/parallelshadow/SH/predecode/register/scalar/mask/
ambient/SSR/immutableUBO remainclosed. Haze localcopy/math deletion ungrounded;
aggregateencoder includeslateralpha. Buffer offset-only native setters ALREADY
implemented/invalidationcorrect; do not reopen that lead or oldbulktexturelead.
Coupled baseline6b80 trace is finalized, no exactcriticalpath/hardwareceiling.
Retain64examined/32accepted/perviewpacks/bindings/arenas/mainocclusion/aniso4/
boundedlights/directR32/borrowedtargets/image-globalbudgets/cubeSSAO.
Full pixel/featureparity/multiscene/stability/strictnoGL/freshbuildreview OPEN.

## GitHub owner request

Stable7772 includingwaterf549 successfully pushed. CLI/browserunauthenticated,
connectorcannotdispatch. Owner chose manualexistingTag a Build fromanotherdevice:
branchcodex/apple-silicon-metal,channelProject,projectMetal Alpha3,tagoverride
26.4.0.68611-alpha3. Harness42799555537passed; NO packagingrun observed at11:28
check. New765PBR is LOCAL/unmeasured; do not claim it is in that pushedalpha.
ExistingactionpublishesARM64tar.xz/checksum+ZIP-wrappedartifact. Owner authorized
thatalphapackage; no broaderreleasegatewaiver/privatecaptureupload.
