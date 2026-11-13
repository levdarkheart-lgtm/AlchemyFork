# PPD-037 native HDR finite boundaries — isolated evidence, 2026-11-13

Status: local implementation, NOT owner verified, NOT approved for push. Root integrator owns canonical PPD/state/ledger and whole-viewer build. Source base `b04d9bf6ef86d275a747f54618090afbea7354d9`; worktree `/private/tmp/alchemy-upstream-remaining-hdr-20261113`, branch `codex/upstream-remaining-hdr-20261113`. No viewer launch, login, scene operation or push.

## Scope and source mapping

Read root AGENTS.md; agent-continuity, original-viewer-reference, metal-port, gpu-validation SKILL.md; relevant PRODUCT/ARCHITECTURE/VALIDATION/APPLE_SILICON/PERFORMANCE/OPERATING_MODEL/scenarios docs, CURRENT/active ledger and existing PPD-037. Original reference is local upstream `408fd6c014`, exact corrections:

- `07094f82fe6097f6e3b34d099dd7552f2f742624` and final `977e51afe94e80045ad75140e5d4248147fb74ff`: `class3/deferred/{pointLightF,multiPointLightF,spotLightF,softenLightF}.glsl`, `class1/deferred/deferredUtil.glsl`. Bound the intensity-weighted punctual contribution AND final additive output; base/soften32752, additive512 under upstream63-draw envelope, complete-scene65504.
- `74589add5f1ff53f61dae01b9bd59abf0e95d706`: `class1/effects/bloomExtractF.glsl`, sanitize source before firefly scaling.
- `9c451dc4a23d5af3793fcbea51197bf1d519c89b`: `class1/deferred/{luminanceF,exposureF}.glsl`, NaN meters black; nonfinite history EV resets to target.
- `b929675d9cb5521a5aa11ceb61a889aaef00f666`: `class1/interface/{reflectionmipF,shPackUtilF}.glsl` and `class3/deferred/reflectionProbeF.glsl`; sanitize downsample, signed half SH clamp, return ambient before SH reads at nonpositive irradiance scale.

Native owners: `indra/almetal/shaders/almetal_builtin.metal` corresponding local/projector/base geometry/sky fragments, shared bloom helper, exposure meter/history kernels, probe downsample/SH reduce and SH evaluation helper. Private CPU constants and public scene bound in `almetalworld.{cpp,h}`. Root owns live pipeline producer assignments. No GL path, pass drop, sampler/alpha/gamma/depth-policy change introduced.

## Native count / format audit and budget

`ALMetalWorldGBufferRenderer::accumulateLocalLights` accepts<=65535 lights. Camera-outside/bounded lights each issue one draw; remaining inside lights batch16. Projector owner independently accepts<=65535 and issues one draw/light. Both accumulate in RGBA16Float target. Live viewer collectors inspect the same prefix of `mNearbyLights` limited by RenderLocalLightCount (default256), with mutually exclusive spotlight classification; root verified and owns assigning common prefix-bound to both scene `std::size_t additive_pass_count` fields. One prefix candidate contributes at most one family draw; batching only reduces it. Thus upstream63-pass assumption is insufficient natively.

Private local config.z and projector config.w carry the shared upper bound. Isolated API default0 reserves twice own actual draw count; count multiplication is safe because own count<=65535; provided bounds are maxed with own draw count and capped at UINT32_MAX. Conservative dyadic shader budget:512 through63 draws,256 through126,128 through252,64 through504 etc. Division rounds count upward rather than multiplying coverage (avoids integer overflow). No count-dependent ceiling changes ordinary values below that ceiling.

FP16 bound: for ceiling>=32, each ceiling is a power-of-two multiple of the half spacing32 above32768; rounding a nonnegative addition<=ceiling cannot advance a stored half by more than the ceiling. Starting base32752, initial transition is bounded by32768; at most63*512 extra gives65024. For ceiling<=16,65024 is an even representable half and65024+16 ties downward; lower values cannot round past65024. Hardware physical blending was exercised, not merely CPU arithmetic.

Actual native reflection face downsample is RG11B10Float, source Screen RGBA16Float, SH partial rows RGBA16Float, packed destination RGBA32Uint. Upstream uses65024 downsample ceiling. Blue has fewer mantissa bits and nominal maximum64512. Final LLDB run7 exercised positive infinity separately in R/G/B and together, plus finite65024 in all channels. Native RG11B10 writes produced finite R/G<=65024 and B<=64512. The implementation retains upstream65024 hue-preserving pre-store ceiling; the misleading common-maximum comment was corrected.

MSL `-ffast-math` / runtime MTLMathModeFast on M1 Max required a native adaptation: upstream clamp to3e38 followed by ceiling/peak produced BLACK at extreme values because reciprocal1/peak flushed to zero. Exact2^-64 prescaling only when peak>1e30 fixes normalization while leaving ordinary range untouched. Physical case covers positive infinity probe input and1e38 point/projector intensity. Existing NaN/negative sanitization preserved upstream intended semantics.

## Host and build

`tools/metal/doctor.sh` exit0; log `/private/tmp/alchemy-remaining-hdr-doctor.log`: macOS26.5.2, arm64, Apple M1 Max,64GiB, Xcode26.6 build17F113, CMake4.4.4, Metal toolchain present; unified memory=true, recommendedMaxWorkingSetSize62277025792, Apple7=true/Apple8=false/Apple9=false. Hardware-identifying serial fields are not included here.

Scratch configure exact command:

```
cmake -S indra --preset ninja-os-arm64 -B /private/tmp/alchemy-upstream-remaining-hdr-build-20261113 -DAL_CHANNEL:STRING='Alchemy Test' -DAL_BUILD_VIEWER:BOOL=OFF -DAL_BUILD_TESTS:BOOL=ON -DAL_TEST_PER_CASE:BOOL=OFF -DAL_VCPKG_INSTALL:BOOL=OFF -DVCPKG_INSTALLED_DIR:PATH=/Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-xcode-os-arm64/vcpkg_installed -DCMAKE_TOOLCHAIN_FILE:FILEPATH=/Users/dekstroza/Programming/AI/Secondlife/Alchemy/vcpkg/scripts/buildsystems/vcpkg.cmake
```

Exit0; log `/private/tmp/alchemy-remaining-hdr-configure.log`. Canonical installed dependency tree used as read-only inputs. Worktree dullahan copied locally (rsync --exclude .git) from canonical submodule. Initial configure before AL_VCPKG_INSTALL=OFF was supplied ran vcpkg's already-installed check, then failed because worktree dullahan was unpopulated; no package installation was needed. No canonical build outputs used for new tests.

Build exact command repeated after fixture corrections:

```
cmake --build /private/tmp/alchemy-upstream-remaining-hdr-build-20261113 --config Release --target almetal_tests --parallel 4
```

Latest exit0, `/private/tmp/alchemy-remaining-hdr-build-8.log`. Actual offline Metal source compiled `-std=metal3.1 -ffast-math`, production almetal library and focused test registered in almetal CMake. Intermediate build logs1–8 retained. Failures were test-only: unused helper before fixture completion, Catch2 OR needing parentheses, and INFO ternary needing parentheses. Whole app wrapper build / verify_app delegated to root; not claimed here.

## Physical GPU executions and two test fixture crashes

Exact command for each numbered run (log suffix below):

```
env MTL_DEBUG_LAYER=1 /private/tmp/alchemy-upstream-remaining-hdr-build-20261113/almetal/Release/almetal_tests.app/Contents/MacOS/almetal_tests '[hdr-finite]' --reporter compact
```

1. 09:47:45 PID6697, exit139; `/private/tmp/alchemy-remaining-hdr-gpu-red-green-1.log`. **Unintended test fixture SIGSEGV**, not an expected control, not a viewer crash. INFO streamed nil NSError.localizedDescription.UTF8String into Catch2 -> strlen(nullptr). Report `/Users/dekstroza/Library/Logs/DiagnosticReports/almetal_tests-2026-11-13-094754.ips`; inspected fault stack `_platform_strlen -> std::ostream<char const*> -> Catch::MessageBuilder -> Fixture::render`. Fixed null-safe logging before next run.
2. 09:48:32 PID6906, exit134; log `...-2.log`. **Unintended test fixture Metal API-validation SIGABRT**, not expected control. Raw test projector function still required function-constant specialization. Report `/Users/dekstroza/Library/Logs/DiagnosticReports/almetal_tests-2026-11-13-094837.ips`; inspected `validateWithDevice -> MTLReportFailure -> __assert_rtn -> abort`. Log explicitly says `almetal_world_projector_light_fragment cannot be used ... Use newFunctionWithName:constantValues`. Fixed by specializing with empty MTLFunctionConstantValues exactly as production ALMetalRenderPipeline does. No CrashReporter preferences modified.
3. 09:48:59 PID6986, exit42 clean Catch2 assertions, log `...-3.log`:1073assertions,1057passed/16failed. Found the actual extreme-float reciprocal flush described above; a supposed ordinary PBR input had .05 roughness and really produced55311 at intensity1, so changed the ordinary control to .5 roughness. All-inf SH fixture generated NaN which Metal max masked to zero; changed to L0-only infinity to expose zero-weight leak. Native RG11B10 mixed finite result quantized blue16128, not initially assumed16256; corrected physical format expectation. Fixed512*64 did not produce infinity on this hardware; the regression checks exceeding the conservative65024 envelope instead of falsely claiming native infinity.
4. 09:51:15 PID7443. Main shader red/green case PASSED all1073assertions. A second scene-owner ABI test was starting native renderer creation when root instructed all almetal_tests executions to stop after owner reported two crash dialogs. Interrupted with SIGINT; exit130; log `...-4.log` reports one passed/one interrupted,1074passed/one SIGINT failure. No subsequent almetal_tests process or new crash reports at pause snapshot. **This is NOT an overall passing suite result.**

Control design: built-in production metallib supplies actual local/projector/base geometry/sky and both bloom fragments, exposure meter/history, probe downsample and SH reduce entry points. Frozen pre-fix boundary expressions are restored in a complete production MSL module for red controls, compiled MTLMathModeFast. Binding-only appended kernels call the actual SH evaluation/radiance-budget helpers; no CPU shader-equivalent implementation. True RGBA16Float and RG11B10Float inputs/stores, signed SH half packing and repeated per-draw RGBA16Float additive target are read back from M1 Max.

Main case executed ordinary/extreme legacy and PBR point/projector lighting (intensity1,1e10,1e38), base32752 and sky65504, bloom infinity/NaN and ordinary input, NaN/inf meter, NaN/+inf/-inf EV history and finite history, probe downsample invalid/mixed/ordinary RGB, signed SH overflow, zero/negative scale with L0-infinite SH, and repeated native additive draws at1,63,64,126,127,256,1008,1009,2016,2017,65535,131070 starting32752. These passed in run4. No performance, scene parity or owner verification claim. Final all-channel probe checks are recorded below.

## Next actions / outstanding prerequisites

Runtime execution paused by root following owner crash-dialog report. Do not restart tests until root authorizes. Preserve all sources/logs/reports. Root registering owner report; carry PPD-037 and crash-harness report IDs in canonical handoff.

1. Resolve/verify RG11B10 blue-inf boundary using actual shader and format; correct misleading common-ceiling comment if required.
2. Finish native scene-owner ABI GPU test (point shared128/256/default0 and projector63/64), which is implemented but was interrupted before result.
3. Clean final focused run with Metal API validation; appropriate existing sky/water/alpha/lighting regressions, no repeats beyond failures/new code concerns.
4. git diff --check; local coherent commit; root cherry-pick plus live pipeline common-prefix producer assignment; root combined doctor/build/verify. No canonical docs modified by this agent. No push.

## Final LLDB-contained corrective validation

Root independently reviewed both crash reports and fixture corrections, registered PPD-046, informed the owner and authorized completing tests under LLDB only. No further direct test launches occurred. The pause was lifted for this controlled scope; failed earlier runs above remain retained.

Command file `/private/tmp/alchemy-remaining-hdr-lldb.commands`:

```
settings set target.env-vars MTL_DEBUG_LAYER=1
process launch --shell-expand-args false
script p=lldb.debugger.GetSelectedTarget().GetProcess(); state=p.GetState(); print('CHILD_EXIT',p.GetExitStatus(),'STATE',state); lldb.debugger.HandleCommand('thread backtrace all') if state == lldb.eStateStopped else None; p.Kill() if state == lldb.eStateStopped else None
quit
```

Initial LLDB attempt5 never launched a child: LLDB's shell argument expander rejected `[hdr-finite]`; wrapper exit1, log `/private/tmp/alchemy-remaining-hdr-gpu-lldb-5.log`. `process launch --shell-expand-args false` fixes shell expansion while preserving crash containment.

Run6: LLDB wrapper exit0, actual child PID8119 exit42/state10 (normal exit with Catch assertions), log `/private/tmp/alchemy-remaining-hdr-gpu-lldb-6.log`. Main shader case including added blue-inf controls passed. API point budget/default case passed. Projector API fixture was rejected for its mismatched normal_hdr=false on RGBA16Unorm normal target and invalid cookieLOD0; set normal_hdr=true and LOD>=1 according to actual validateProjectorLightParameters. Error logging now captures returned error before REQUIRE. No fault/crash report occurred.

Final run7 exact command:

```
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-upstream-remaining-hdr-build-20261113/almetal/Release/almetal_tests.app/Contents/MacOS/almetal_tests '[hdr-finite]' --success --reporter compact
```

Wrapper exit0, **CHILD_EXIT0 STATE10**, PID8288, **1230 assertions /2 cases passed** with Metal API validation enabled. Full success/value log `/private/tmp/alchemy-remaining-hdr-gpu-lldb-7.log`. Latest scratch build10 exit0: `/private/tmp/alchemy-remaining-hdr-build-10.log`.

Selected observed GPU readbacks (not inferred):

- Fixed512,64 repeated additive draws, base32752: `{65504,49120,40928,0}`. The red control exceeded65024 but remained finite: do not call this an observed infinity.
- Adaptive sharedcount256,256 repeated draws, base32752: `{49120,40928,32768,0}`.
- Adaptive counts65535 and131070: `{32752,32752,32752,0}`; additions below half ULP are lost at this deliberately extreme base, an expected precision limit, not a performance/quality gain claim.
- CPU scene owner with128 bounded lights: sharedbound128 result `{49120,49120,49120,0}`, bound256 `{40928,40928,40928,0}`, isolated default0 identical to256. Thus actual CPU constants carry the intended bound.
- Projector API with sharedbound63 result `{512,512,512,0}`; bound64 `{256,256,256,0}`.
- Positive infinity in each probe RGB channel becomes finite at actual RG11B10 store, red/green max65024 and blue max64512. Negative infinity/NaN channel expectations are zero.

Exact MSL source copies used by the final test fixture saved for review:

- `/private/tmp/alchemy-remaining-hdr-current-with-adapters.metal`, SHA256 `89c8c5351c992e68bb76748b878112239aa5d84de13acbd7dc67d74e22241f43`.
- `/private/tmp/alchemy-remaining-hdr-frozen-control.metal`, SHA256 `b25e00b9adae56799d46d0d36a09dd9d3ffbe6c595d02b717168d107d74471de`. Fourteen source-boundary replacements from the test file; complete production module plus identical binding adapters.
- `/private/tmp/alchemy-remaining-hdr-fixed512-control.metal`, SHA256 `16f6cf6f2d02c702270942a62f55416befc8748cec0e477591bce917c9c35075`.
- Production MSL SHA256 `06a7aeecab50965c93c679237fe1480ef2067cc6067838bed6170fc852043eea`.
- Focused test source SHA256 `f8e2de30fe064697ff37f21c8c9cbdc7a61786e559771c1311966c3346f81b54`.

No production shader math is reproduced on the CPU. The exact-power-of-two normalization and bounded additive budget execute in actual production MSL or production-function binding adapters. The frozen controls check finite/overflow/saturation and ordinary-value parity, not an expected crash. PPD-046 covers unintended early fixture crashes, independently of PPD-037.

## Existing native regression validation and local handoff

Run8 exact LLDB-contained existing regression command:

```
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-upstream-remaining-hdr-build-20261113/almetal/Release/almetal_tests.app/Contents/MacOS/almetal_tests '[base-light-fastpath],[water-normal-filter],[water-haze-projection],[linear-screen-boundaries],[cutout-depth-pin],[cutout-alpha-resample],almetal: world simple pass writes the native four-attachment G-buffer,almetal: native world G-buffer feeds exact deferred point lights,almetal: world exposure matches the production meter and history,almetal: world bloom matches the production HDR pyramid,almetal: native ordinary probes filter radiance and project SH,almetal: PBR material preview preserves glTF texture semantics' --reporter compact
```

Log `/private/tmp/alchemy-remaining-hdr-existing-gpu-lldb-8.log`. Wrapper exit0; actual child PID8479 **CHILD_EXIT0 STATE10**; **243759 assertions /19 cases passed**, Metal API validation enabled. Covers existing production G-buffer/alpha/sky/water path, exact point lighting, exposure/history, bloom pyramid, probe filtering/SH, material preview, fullbright/emissive, water normal/haze, linear screen boundaries, and cutout depth/alpha. No new failures or crash reports. This is targeted offscreen regression evidence, not owner scene parity.

`git diff --check` exit0; local coherent commit `fd35f788768708d4959a9b072b11e004a34f1d27` **Bound native HDR lighting and sanitize postprocess probe inputs**. Only5owned paths committed. Root may cherry-pick and must assign common prefix upper bound to both pipeline scenes. Working tree clean at handoff. Whole-viewer build/verify, owner runtime confirmation and explicit push approval remain pending; no push is authorized.

Earlier Next actions above are historical recovery instructions from the pause, superseded by this completed GPU validation. Remaining integration prerequisites: root whole application build/verification, canonical PPD-037/PPD-046 state/ledger updates, owner runtime test and explicit scope approval before push. Keep both PPD IDs open according to register rules; GPU/build success alone does not close them.

Final offline production metallib SHA256 `be1ef4d89b7d0469b443861e114eb5b4c0fdb5e5705e0b2afc3ba6760242d321`; path `/private/tmp/alchemy-upstream-remaining-hdr-build-20261113/almetal/Release/shaders/almetal_builtin.metallib`. The committed production/test files are byte-identical to those used for the final successful GPU runs.
