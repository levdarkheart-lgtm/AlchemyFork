# Native sky modernization GPU and lens evidence — 2026-11-13

## Ownership and source

Parent owns canonical documentation, PPD register, shared CMake, pipeline integration, whole viewer build and owner QA. This agent owns only new `indra/almetal/tests/almetal_sky_modernization_test.mm`, new fixture header, and the separately assigned lens `.inl`/policy test. No pushes, viewer login, scene operations, performance claim, or canonical dependency mutation.

Worktree `/private/tmp/alchemy-sky-tests-20261113`, branch `codex/sky-tests-20261113`, initial base `2144fa7040384d40f0ea97d4ca54a98ddc2471be`.

Two independently cherry-pickable deliverables:
- `8e17561f551ed5faf639786995b04fb0fe32ade4`: lens radius consumes actual drawn `mSky.getSun()/getMoon().angularHalfSize()`, removes old gSky fixture macro. 2 files only.
- `798ec032c45c97551b7391b281170dfbbad2ddf8`: new native GPU test + fixture, 2 files/573 lines only. Its intermediate renderer ancestors duplicate peer-owned commits and must not be reimported.

Matching native renderer sources in this worktree:
- Peer `27a948adc9e32f2b2a7e336c082b8c9e166f7441` imported as `4fc410275a`.
- PPD071 peer `f55c53c589d67f134acf973d5dadcb0187d969f5` imported as `5e045cdde3`.
- PPD067 peer `975b6cec178e6354c4c4afe50c5987f31ee3d5df` imported as `3bb232c728`.
- PPD074 peer `2f2129bd8fc487522c7d9bbe3538a4b956572615` imported as `8999828008`.
- Public contract `/private/tmp/alchemy-sky-native-contract-20261113.md`.

Pinned final upstream reference `408fd6c014`, source paths:
- `indra/alrender/tests/alskycomposite_gl_test.cpp`: final layer order/opaque cloud short circuit, HDRI negative/full-range65024 before classic gain65504, classic alpha0, no stale stars when bit absent, split.
- `indra/alrender/tests/alskydome_gl_test.cpp`: zero-strength no-fetch and water weight floor/continuation.
- `indra/alrender/tests/alskycube_gl_test.cpp` + `alskycube_test.cpp`: 6 face basis, actual ray dome half-display-encoded cube, mip averages and initial64/255 RGB alpha1.
- `indra/alrender/tests/luminance_gl_test.cpp`: completed depth/water-over-sky provenance, original normals RGB10A2/RGBA16UNORM, missing water mask classifies water as ground.
- Upstream95362a1041 actual drawn angular size, ba9da2e58b depth meter, d464042e3d unclamped water convergence.

Tests call production native methods and decode real readback formats. They do not implement a second sky shader. Cube/water references call the same shipping main compositor on independent authored rays; the CPU oracle only performs transfer encoding, byte unpacking, scalar expected boundaries and independent face bases/mip means.

## Host and isolated build

`tools/metal/doctor.sh > /private/tmp/alchemy-sky-tests-doctor.log 2>&1` exit0: macOS26.5.2, ARM64 MacBookPro18,2, Apple M1 Max,68,719,476,736 RAM bytes, unified memory, Xcode26.6 build17F113, Apple7 true, Apple8/9 false.

Build root `/private/tmp/alchemy-sky-tests-build-20261113`, Release ARM64 Ninja Multi-Config, AL_BUILD_VIEWER=OFF, AL_BUILD_TESTS=ON, AL_VCPKG_INSTALL=OFF; existing canonical Ninja dependencies read-only at `/Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-ninja-os-arm64/vcpkg_installed`.

Configure command (exit0 after local missing-submodule/toolchain remedies):
```sh
cmake -S indra --preset ninja-os-arm64 -B /private/tmp/alchemy-sky-tests-build-20261113 -DAL_BUILD_VIEWER:BOOL=OFF -DAL_BUILD_TESTS:BOOL=ON -DAL_VCPKG_INSTALL:BOOL=OFF -DCMAKE_TOOLCHAIN_FILE:FILEPATH=/Users/dekstroza/Programming/AI/Secondlife/Alchemy/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR:PATH=/Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-ninja-os-arm64/vcpkg_installed -DCMAKE_PROJECT_INCLUDE:FILEPATH=/private/tmp/alchemy-sky-tests-include.cmake
cmake --build /private/tmp/alchemy-sky-tests-build-20261113 --config Release --target sky_modern_tests -j 6
```
External CMake includes `/private/tmp/alchemy-sky-tests-register.cmake`; no repository CMake edits. Needed target registration for parent: add `tests/almetal_sky_modernization_test.mm` to existing almetal_tests and set SKIP_PRECOMPILE_HEADERS ON. Header uses relative local include. Existing bundle offline-metallib registration suffices.

Initial configure failures were absent local vcpkg toolchain/submodule. Explicit canonical readonly toolchain plus locally shared-cloned dullahan at exact gitlink ba893db7aa6fc3257d5ee37379d04962c1ead164 resolved them. No credential/private network access. Two fixture compile issues (nativeDevice belongs command, no OpaquePostDeferredScene.normal_hdr field) corrected before affected runtime.

Final compile log `/private/tmp/alchemy-sky-permanent-build.log`, exit0. Exe ARM64 only:
`/private/tmp/alchemy-sky-tests-build-20261113/Release/sky_modern_tests.app/Contents/MacOS/sky_modern_tests`
SHA256 `7a221eb36b9cb6392e4a1f51b2e502061f4d1cb71985250bab4a862144bb659a`.
Offline bundled metallib SHA256 `eaf4d81b06866515d2d0c0dac180f490384dc5a0bf0d5b2be9de614fcfb18fc1`.
Offline flags exact CMake stamp: `-std=metal3.1 -ffast-math -mmacosx-version-min=26.0`.

## Runtime containment

Every launch used LLDB batch with `/private/tmp/alchemy-remaining-hdr-lldb.commands`:
```text
settings set target.env-vars MTL_DEBUG_LAYER=1
process launch --shell-expand-args false
script p=lldb.debugger.GetSelectedTarget().GetProcess(); state=p.GetState(); print('CHILD_EXIT',p.GetExitStatus(),'STATE',state); lldb.debugger.HandleCommand('thread backtrace all') if state == lldb.eStateStopped else None; p.Kill() if state == lldb.eStateStopped else None
quit
```
Stopped processes are backtraced and killed, never continued or passed fatal signals. Wrapper exit0 is not a child pass. No global CrashReporter preferences modified.

## PPD071 real source defect and repair gate

First ordinary API probe reached `compositeModernSky` and was stopped by Metal API validation: C++ setBytes816 but nested MSL raster-wrapper layout1008. This was a normal valid public API input, not an intentional failure test. `/private/tmp/alchemy-sky-modern-27a.log` preserves exact failure/backtrace. PID84613, state5, child status unavailable(-1) while stopped; LLDB killed process status9 without resume. Frozen app preserved `/private/tmp/alchemy-sky-modern-27a-frozen.app`. Parent registered PPD071 before peer repair.

Peer identified embedded old288/336-byte raster wrappers instead of bare192/240-byte payloads. First repaired source-compiled actual builtin gate: PID85385 child0/state10,19 assertions/1case, `/private/tmp/alchemy-sky-layout-green.log`. Private external bundle resource registration was then corrected before further draws.

Repeated OFFLINE-metallib gate: PID85599 child0/state10,19 assertions/1case, `/private/tmp/alchemy-sky-layout-offline-green.log`. Actual production layout kernel checks sizes192/240/816 and offsets112,304,544,592,640,688,704,720,768,784,800 plus exact bit patterns across nested payloads (camera.w12345.5,cloud.parameters.z0.3125,body.y0.625,transfer.w1.125,config.w37). No further draws until both gate and parent/peer review.

## PPD067 inactive optics red/green

On fixed-ABI27a+f55 but before975, exact production dome with strengths0 and NaN RGBA16F rainbow/halo maps differs from ordinary positive dome (poisoned output becomes black after finite sanitization). Log `/private/tmp/alchemy-sky-optics-red.log`, PID85700 child42/state10,46assertions pass/1fail; no signal/validation fault. This is not a CPU equivalent. After975 exact no-fetch guards, same permanent case passes, included final full green below.

## Final permanent production GPU suite

Command:
```sh
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-sky-tests-build-20261113/Release/sky_modern_tests.app/Contents/MacOS/sky_modern_tests '[modern-sky]' --reporter compact > /private/tmp/alchemy-sky-modern-permanent.log 2>&1
```
PID86489, actual child0/state10, **242875 assertions/13cases**, Metal validation enabled. No validation errors or signals. Cases:
1. Actual GPU layout probe (above).
2. Far-depth HDRI only modifies lit sky; real opaque left geometry preserved; forward/reverse Z; normal RGB10A2 and RGBA16UNORM. All4Gbuffer stores byte-exact plus depth and nonzero stencil0x5a retained.
3. STARS bit with selected Gbuffer0/3 preopaque black clear and intentionally no star draw succeeds/composites ordinary dome; all unrelated stores/depth/stencil and geometry retained. Both emissive/raw-sRGB albedo, HDR/nonHDR, forward/reverse.
4. Production packed float3+RGBA8 point and expanded6vertex quad stars, true depth occlusion of opaque half-view, both selected attachment formats and reverseZ. Opaque moon-quad texture masks visible stars. Positive signal required; other Gbuffer stores/depth/stencil exact.
5. Actual HDRI RGBA16F output: negative,ordinary0.25,65504,NAN,+INF texels; exposure1000 forbright; gain1/1.1;65024 pre-gain and65504 post-gain boundaries; allfinite/nonnegative, alpha0.
6. Inactive optics NaN no-fetch (red above).
7. Neutral64x64cube7mips, all6faces/alltexels RGB64 alpha255.
8. Baked6cube faces9center/corner sample rays each against real main GPU dome evaluation, within RGBA8/half quantization; every mip's2x2 parent mean within1byte.
9. Body/cloud/HDRI precedence: sun overwrites stars/bloom, moon overwrites earlierlayers, nextsun blend endpoint, bitclear stale star map ignored, HDRI supersedes nightlayers, opaquecloud suppresses allunderlayers, transparentcloud resumes them.
10. REAL production procedural water (not synthetic water-mask shader): half-view water spans opaqueground and clearsky. Original normal stores/depth preserved, finalpost-water depth readable with waves/SSR off. Meter's skyshare compared at16interiorgridlocations with/without water_stencil provenance. All4fixturegroups required nonempty: dryground,drysky,waterground,watersky. Both normal formats and reverseZ.
11. REAL production water-surface then atmospheric-haze sequence. Water sky convergence matches actual mainGPUray dome at independent reconstructed rays with HDRdisplayvalue>1 retained. Offwater byte colors unchanged. Fixture first test used only bright upperambient but lowerhorizonambient remained0.1; setting both authored upper/lowerambients20 creates intended bright positive control; no product fix.
12. HDRI split0/0.5/1 selects whole-dome/HDRI references pixel-exact on opposite sides, scale3/classic1.1 retained.
13. Packed meteor count64 positive/finite and count0 baseline; procedural aurora time13 intensity1 produces positive finite layer vs0.

Intermediate status logs `/private/tmp/alchemy-sky-modern-abi-green.log`132523/6cases child0, `/private/tmp/alchemy-sky-modern-first-green.log`166736/9cases child0, `/private/tmp/alchemy-sky-modern-expanded.log`189275/13cases child0. First incomplete fixture failed3base-lighting admission assertions before modern draw (missing required legacyenvironment and hdr profile mismatch), fixed via actual neutralcube/base-lighting contract; no product conclusion. All final inputs reviewed through actual public admission API.

## PPD037 bounded full-production-MSL negative control

Earlier CURRENT OLD `compositeHDRISky` RG11B10 attachment probe with exposure1,2,1e6 stayed finite; 404assertions/1case, PID81484 child0. Log `/private/tmp/alchemy-sky-hdri-baseline-red.log` is named red but is **GREEN**, do not claim oldoverflow reproduced.

After final permanent source froze, built private FULL current MSL copy with only2HDRI boundary expressions removed. Source `/private/tmp/alchemy-sky-hdri-unbounded.metal`, exact diff `/private/tmp/alchemy-sky-hdri-unbounded.patch`. All other production shader code, constantlayout, public renderer API and test exe identical. Compiled actual Metal3.1 fast offline library to copied private app `/private/tmp/alchemy-sky-hdri-unbounded.app`, never changed repository or normal tested bundle.

Source SHA256 `de462c98a28acba07c607e509925c5986e0f6c252b3f7d79bac3b1201550d5b2`; private metallib SHA256 `6d66c5125a57458868872b62ba8c2d5a0eaa6d00cc5edb35165db243ca72fd12`.

Compile commands exit0:
```sh
xcrun --sdk macosx metal -std=metal3.1 -ffast-math -mmacosx-version-min=26.0 -c /private/tmp/alchemy-sky-hdri-unbounded.metal -o /private/tmp/alchemy-sky-hdri-unbounded.air
xcrun --sdk macosx metallib /private/tmp/alchemy-sky-hdri-unbounded.air -o /private/tmp/alchemy-sky-hdri-unbounded.app/Contents/Resources/almetal_builtin.metallib
```
Run sameLLDBcommand against copiedapp withfilter`[modern-sky-hdr] --reporter compact --abortx 12300`, log `/private/tmp/alchemy-sky-hdri-unbounded-red.log`. PID86788 child42/state10,41105assertionspass/12300fail before bounded earlyabort. Actual negative output -1 and -1.099609375; ordinary0.25 controls bothgains pass. Actual huge exposure **saturates to65504** onthisGPU,480above65024 expected boundary; no observedInf. This is a valid realGPUred fornegative/above65024 semantics. Do not claim every unguarded half-float store produces Inf.

## Lens test

`8e17561f55` actual `.inl` remains included by policy test using real LLControlGroup/shader lifecycle; skyowner body inputs stubbed. No fake oldgSky macro. Tests preserve allold settings/epoch/hold/fade/zoom semantics, verify smallsize below oldfloor, unchanged cacheddrawnquad under directionchange, zoom, unready0, moonselection. `/private/tmp/alchemy-sky-lens-green.log`:83assertions/4cases, PID83961 child0/state10. Private external copy changes include locations only to avoid duplicate source registration with canonical newview_tests. No visual flare claim from CPU policy test.

## Limits / parent next action

Parent must register both committedtestfiles as indicated, integrate matching native renderer/pipeline/CPUowner sources, run whole canonical build+existing default7coverage+source/harness/noGL/app verification, and owner scene QA. These fixture results do not prove viewer visuals, user acceptance, release completion, or performance. Owner approval remains prerequisite for push.
