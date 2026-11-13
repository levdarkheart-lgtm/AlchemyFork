# Coherent upstream sky port — 2026-11-13

Status: ready_for_owner. Owner explicitly requests sky after completed geometry delivery.
Base2144fa7040; canonical codex/apple-silicon-metal. Geometry sourceb16fa27efa /
Alchemy Metal Geometry26.4.0.68952 is the preceding candidate; final sky identity below.
Pinned upstream408fd6c014e6e3b2b90dfe61265f34df0628f450, including #584 and all
landed sky/star/water/depth/exposure/cube tails through that snapshot. PPD-066
tracks the authorized coherent sky replacement. No push without owner test,
agreement and explicit scoped approval. Five unrelated untracked research notes
remain untouched. All runtime/negative/native GPU tests run under LLDB, parse
actual child exit/status, backtrace+kill fault-stopped children; never resume a
fault or change CrashReporter preferences.

## Original behavior and native boundary

Final upstream moves dome, bodies, bloom, clouds, meteors and aurora out of
viewer-object/drawpool/G-buffer raster layers. Pure ALStarField/ALSkyLayers/
ALSkyCube supply deterministic generation, visibility/runs, texture frames,
view-layer admission, body geometry and cache policy. Only stars raster into one
cleared attachment; one far-plane ray composite writes the linear lit screen.
Water evaluates the same dome, exposure uses depth plus water-over-sky
provenance, and probes-off reflections consume a GPU-baked environment cube.
Important tails ba9da2e58b/95362a1041/d464042e3d move with this ownership change.

At task start the native viewer retained LLVOSky/LLVOWLSky/LLDrawPoolWLSky CPU
owners and raster sky buffers; these are now retired. Source/external reference revision is explicit;
old native implementation is not the new behavioral oracle. Preserve native
HDR/additive safety, depth convention, HDRI/split-screen/environment/underwater/
probe/mirror differences, star brightness and twinkle, sun/moon textures and
blend, bloom toggle, cloud controls, aurora, meteors, rainbows/halos, beacons,
settings/UI behavior, shutdown/device/resource safety and geometry batch fixes.
No OpenGL path, GL pool import, pass omission or broad dependency update.

## Ownership

Root owns canonical docs/register/CMake, pipeline/llviewershadermgr and shared
viewer callsite/lifecycle integration. Independent new clean worktrees at base:
- sky CPU: /private/tmp/alchemy-sky-cpu-20261113, codex/sky-cpu-20261113.
- native sky: /private/tmp/alchemy-sky-shader-20261113, codex/sky-shader-20261113.
- native/reference tests: /private/tmp/alchemy-sky-tests-20261113,
  codex/sky-tests-20261113.
Initial agents audit read-only. Root assigns exclusive implementation paths and
agreed typed scene/uniform/resource contracts before concurrent source edits;
only assigned agent edits each shared shader/render-owner file.

## Acceptance and next executable work

Implement final CPU helpers/ALSky ownership and native star/ray-composite/cube
passes, shared water dome and depth-aware exposure; update consumers and retire
displaced objects/pools/decodes without losing retained features. Verify actual
upstream producers/consumers and compare production algorithms, not a detached
lookalike. Add focused CPU tests and real Metal readback cases across layer/view/
depth/water/HDRI/split/body/star/cloud/meteor/aurora/cube semantics; preserve old
and malformed controls/failures with bounded allocations and LLDB containment.

Then doctor/configure/Release build, native/regression tests, verify exact app/
source/ARM64/deep signature/import/retained features, strict source audit and
harness. Keep exact SHA/commands/exits/logs/limits. Scene visual parity, physical
OOM, runtime foreign-context absence and measured M1 FPS are independent gates;
do not infer them from CPU or offscreen shader checks. The initial implementation and final canonical checks are complete; owner
scene acceptance and full product release gates remain separate.

## Observed implementation checkpoint

Integrated local source throughefc3dc17d0 (base2144fa7040): CPUhelpers714c6c,
nativef54b499/ee1db205/a000be8/484367, owner322616/2c056c, lensccb127,
GPUtests42f881, rootviewer972cda4 andpublicinclude8b77dfc5; efc3dc17 adapts the haze test contract. OldLLSky/LLVOSky/
LLVOWLSky/sky pools, fakeobjects/cullpaths, rastercapturemethods/fields and
SkyBehind native target are retired. Native cubeprovider creates/bakes outside
view batches andonlyacknowledges submitted independent work; captured NativeView
owns image/VB references. Main sky admission frozen separately fromoptional bits.
Bothmeter routes use completed post-water depth/stencil. HDRI waterexclusion,
empty async/count0stars, single selectedpreopaque clear and layer0dome retained.
Sunbloomtoggle/editor/savestate andactualdrawnbodylenssize are integrated.

PPD-037 reopened/qualified retainedHDRI404finitegreen: hardwarepacking didnot
reproduce allegednonfiniteoverflow. 067inactiveoptics oldactualnewshader red1
check/47, fixedgreen; 068disabledquad old64failed/206checks, fixed206/4green;
069emptydraw,070fixedmeter,071CPU816/MSL1008fault,072layer0dome,
073cubebakeclocksideeffects,074old16BkernelABI,075HDRIwater,
076pendingneutralpublication,077includevisibility,078obsoletehazeassertion and
079QA Boolean declarations registered before correction.
Actual071stoppedtest PID84613backtraced/killed withoutresume; correctedoffline
layoutprobe19/1pass before any resumedraw. Viewer smoke followed after final
canonical ABI admission; details below.

Focused CPU582838/30,settings63/12,owner206/4,dome236/3 andactualprovider/view
controls77/6 pass underLLDB; negativecontrolsfail normallychild42. Native
productionofflineMetallib242875assertions/13cases pass withAPIvalidation,
includingwater/depth/format/Z/layers/star/cube coverage. Lens83/4pass.
[CPU](../../research/upstream-sky-cpu-validation-2026-11-13.md),
[native](../../research/upstream-sky-shader-validation-2026-11-13.md),
[tests](../../research/upstream-sky-tests-validation-2026-11-13.md).
Rawreceipts/scripts/logs inartifacts/metal/upstream-sky-20261113/.

Root13liveTUscompileafterretirement; initialpipelinecompilemissingoldheader,
thenobsolete.inl declaration caught/fixed (retained preflight receipts).
Combined972build stopped1 late at2testTUs: alsky.h bare Metal include required
privateviewer include paths. PPD077 fixed8b77; full build then passed.
First canonical8b77 CPU run stopped normally child42 at alfarplane1/364checks:
obsolete assertion forbids any eye_to_sky in haze, despite the correct new water
ray branch. PPD078 fixedefc3dc: exact geometry and dome contracts separated,
full actual alrender1,146,808assertions/145cases pass underLLDB.

Final sourceefc3dc17d0acdb069c88786fd30a06ea60b482ba, Alchemy Metal Sky
26.4.0.68965 Release/LTOoff/testsON: doctor/configure/build all exit0, canonical
productionABI19/1 passes before viewer or other GPU work. Final broad validation all passes, exact commands/exit0 receipts retained:
206contained CPU commands/3,940,900assertions/2,812cases(13existing skips),
10CTest registration listings,24containedUI commands/22,407assertions/224cases,
512,781GPUassertions/53cases(includes modern242,875/13 and ABI gate19/1).
237native children all exit0/state10, no final fault stops. verify_app checks
346ARM64 Mach-O files/0invalid slices; deep strict signature and embedded
sourceSHA pass. Owned viewer/CEF/VLC hosts have no forbidden graphics imports.
Compiled native music23/WebRTC477/OpenHTJ2K61/OpenJPEG1 defined symbol markers
retained (counts are linkage evidence, not full runtime feature tests).
Strict source scans3,539files,0blocking/0errors,898informational compatibility
names. Tools116tests pass; final harness33requiredfiles passes.
Raw validation-state/summary/app-identity/build-state and each command/log/json
are under artifacts/metal/upstream-sky-20261113/.

## Candidate-only in-world smoke

Binary: build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Sky.app/
Contents/MacOS/Alchemy Metal Sky; SHA256
3f629d75890f1bed93f933f6f3f54e685e4c80a47d114b45d419808fc45ba777.
Explicit destination secondlife://Spare%20Time/105/21/24 uses previously
independently verified global pin; no reference comparison inferred. Saved
LevDarkheart graphics XML source hash77392c89488b7541baad09255da824eb5867a3f96f375c3eeef1d037715ea217.
Task-local LLDB wrapper leaves installed visual skill unchanged and captures
actual native PID/stops/exits; API validation enabled. First PID97037 logged in
at global261481/241429/24.3098 but capture gate blocked because CLI-only bool
controls lacked session Type declarations; actual LLSD1 versus expected true.
PPD079 corrected by deriving local profile from saved XML plus actual source
Boolean declarations. No invented observations, original profile or installed
skill changes. First viewer deliberately quit normally child0/state10; owner
asked whether it died and was informed it was intentional, not a crash.

Reopened same exact binary PID97692, run private-qa-candidate-sky-68965-20261113-02.
Unchanged skill observe/verify/gate all0 at13:03UTC: logged_in true,
global261481/241429/24.3098125, local105/21/24.3098125. Critical live controls:
farclip112,shadow2,SSRtrue,tonemap1,exposure.5; voicefalse,HDR1,emissive1,
reflections1,dynamicexposure1. Session Boolean declarations account for LLSD1.
CUA confirms exact68965 window and locationbar Spare Time(105,21,24), sky/clouds,
world/UI visible. Avatar still initially loading; no settled-avatar or paired
pixel/sky-layer parity claim. Screenshot observed in task transcript; no private
image uploaded or committed. Viewer left running for owner testing as requested;
no further GUI manipulation planned. LLDB watcher remains attached to own PID.

Root read-only final audit confirms sky preclear/stars/lighting/composite order,
optional-mask0 admission, independent dome cube publication/retry, captured
resource lifetime, water gate and both completed-depth meters against408; no
additional integration findings. Automated/offscreen coverage does not prove
all in-world environments, physical OOM, foreign-context absence or faster FPS.
Implementation and local validation are complete; changes/evidence committed
locally with source identity above. Owner tests day/sunset/night, water/horizon, bloom/body
editing, HDRI/reflections/exposure; retain all faults/failed controls and do not
close records or push without owner verification/agreement/explicit approval.

Owner follow-up during testing: no EXR initially; recommended optional free
Kloppenheim06(Pure Sky), https://polyhaven.com/a/kloppenheim_06_puresky,
2K EXR, after checking the actual published download page. No download/load or
in-world HDRI acceptance claimed. Actual UI is Develop > Render Tests > HDRI
Preview; Advanced > Show Debug Settings RenderHDRISplitScreen0/1/.5 changes
ordinary/HDRI/split main-view background; loaded HDRI may still affect probes.
