# PPD-052 sculpt correctness adaptation — 2026-11-13

Local only, no push/owner validation. Worktree `/private/tmp/alchemy-upstream-sculpt-correctness-20261113`, branch `codex/upstream-sculpt-correctness-20261113`, base `2a5af4fdaca0196f508f8f077f9b5c3de07555de`. PPD-052 registered by root before implementation. No viewer launch/login/scene operation. Root owns canonical docs/newview CMake/whole-app build.

## Original gate and actual producer/consumer proof

Inspected exact upstream85bf118da53e12575fc429e76f402e01c362cddf,04d9015143bee64ba635199ad70be3cdab27cebe,45007f232defcd25eca93dce11beecc3a8276f0e plus canonical2a5 source. Original-viewer-reference/metal-port/continuity skills already read in this session. Current canonical facts:

- LLViewerFetchedTexture::saveRawImage3037, sculpt branch3074: long512x8 map whose side exceeds expected128 becomes128x128; current `sMaxSculptRez=128` at95, not upstream's cited256. Saved copy retains raw decode discard label.
- saveRawImage's early saved-discard<=raw-discard guard rejects an improved same-level image irrespective of its actual pixels.
- createTexture1460–1488 CPU-only sculpt path setsImageMetadata then destroys/savesraw and has no postCreateTexture callback. postCreateTexture1700 alone currently marks attached sculpt volumes to rebuild.
- LLVOVolume updateTextureVirtualSize1011–1017 looks only at raw discard and only schedules on lower discard or negative current. It misses saved copies and same-level larger pixels.
- LLVOVolume::sculpt1438 chooses raw else saved; if smaller than GPU dimensions, synchronously readbackRawImage(-1) and replaces the candidate; early return at1515 checks equal discard alone. LLVolume currently records only discard, not original input pixel count.
- LLVolume::sculpt3276 samples only the original map at its mesh LOD, and any area<SCULPT_MIN_AREA or>384 switches to sphere. Coarse samples of the upstream synthetic oblong tube can all hit collapsed columns; bounded low-area averaging is corrective sampling, not an optimization.
- LLImageGL::readBackRaw2339 already accepts an explicit discard, validates published native mirror mip count/dimensions, and uses native ALMetalTexture readback.4096 guard already fixed. Therefore chosen-copy readback can remain synchronous native; GL request/collect/PBO/fence architecture is unnecessary and is not imported.

## Ownership and implementation

Root renderer agent owns llvolume.cpp/h for PPD-048 allocation/cache changes until its commit; sculpt modifications will apply after that commit to disjoint sculpt hunks, integrated PPD-048 first. Root exclusively owns newview/CMakeLists.txt. This branch edits no CMake file; requested dedicated registration:

```
al_add_test_binary(alsculptcopy_test PROJECT ${test_project}
  SOURCES tests/alsculptcopy_test.cpp LIBRARIES llimage llmath llcommon)
```

Narrow production helper `indra/newview/alsculptcopy.h` owns the actual aspect-preserving copy, halved-both-axes pixel budget/mip selection, raw-vs-saved larger-image selection, save admission, and same-level rebuild admission. LLViewerFetchedTexture and LLVOVolume call these actual functions; tests call them with actual LLImageRaw and LLVolume, not duplicated CPU shader or geometric formula.

Generic loaded-callback readbackRawImage remains intact. Sculpt-only synchronous readback asks LLImageGL for chosen budget mip; if absent reads published current mip then reduces CPU image to same size. Failure retains existing saved copy. Saved-copy publication notifies sculpt-only volumes that have no GPU upload callback. Producer and consumer both admit more input pixels at same discard. Existing scoped boost behavior/native upload publication is retained. No performance claim.

## Scratch CPU validation setup

Apple M1 Max/macOS26.5.2/Xcode26.6 already verified by doctor during HDR work; see `/private/tmp/alchemy-remaining-hdr-doctor.log`. Scratch worktree dullahan source copied read-only from canonical, vcpkg install explicitly disabled, canonical installed packages used as read-only inputs.

No source CMake mutation: external `/private/tmp/alchemy-sculpt-project-include.cmake` uses `cmake_language(DEFER ... include /private/tmp/alchemy-sculpt-test-register.cmake)` to register the same dedicated test target after root setup.

```
cmake -S indra --preset ninja-os-arm64 -B /private/tmp/alchemy-upstream-sculpt-build-20261113 -DAL_CHANNEL:STRING='Alchemy Test' -DAL_BUILD_VIEWER:BOOL=OFF -DAL_BUILD_TESTS:BOOL=ON -DAL_TEST_PER_CASE:BOOL=OFF -DAL_VCPKG_INSTALL:BOOL=OFF -DCMAKE_PROJECT_INCLUDE:FILEPATH=/private/tmp/alchemy-sculpt-project-include.cmake -DVCPKG_INSTALLED_DIR:PATH=/Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-xcode-os-arm64/vcpkg_installed -DCMAKE_TOOLCHAIN_FILE:FILEPATH=/Users/dekstroza/Programming/AI/Secondlife/Alchemy/vcpkg/scripts/buildsystems/vcpkg.cmake
```

Configure exit0 `/private/tmp/alchemy-sculpt-configure.log`. Initial llimage dependency build exit0 `/private/tmp/alchemy-sculpt-dependencies-build.log`. Core llmath/llcommon/al_test_support build was pending at this initial snapshot `/private/tmp/alchemy-sculpt-core-build.log`.

Focused tests authored for real copy+build:512x8/8x512/4096x4/4x4096 unchanged maps within128^2 budget; proportion-preserving larger map copies and desired mip; same-discard larger saved image selection+LLVolume rebuilding+duplicate rejection; upstream collapsed-ring coarse LOD tube; excessive-area noise must still use sphere; finest detail remains257 profile samples. The initial snapshot preceded ownership transfer; completed executions are recorded below.

## Independent AP_ASSET disposition sent to root

05c6ab903212a46e7fe4e7f94e76ca4131ac6b8b is independently applicable, four CPU files only: llappcorehttp.h/.cpp, settings_alchemy.xml, llviewerassetstorage.cpp. Current assetRequestCoro556 uses AP_TEXTURE; texture fetch2631 uses same class; each queue is therefore coupled to the same per-host/concurrency/pipeline limit. AP_COUNT10->11 fits HTTP_POLICY_CLASS_LIMIT32. Existing init/refresh loops create/configure the extra class and subscribe its setting. Default8, clamp1–16,0 means default. AssetStorage pool remains12; meshes keep their separate classes. Route using getAppCoreHttp().getPolicy(AP_ASSET) actual native policy ID instead of relying on enum numeric ID. No renderer or async dependency, no measured latency/FPS claim. Root registered PPD-051 separately and owns its implementation assignment.

## Completed ownership transfer / implementation

Renderer released llvolume.cpp/h after PPD-048commit `51d53a6dee`; cherry-picked as `1dc9fcec46` in this worktree before any sculpt edits to those files. Sculpt-only commit is `0b0c9c0101124ec25ecb6dab4162c60d64d4b006` **Preserve sculpt map copies and rebuild finer same-level geometry**. Root must cherry-pick only this sculpt commit after its own PPD-048integration, not the parent twice. Working tree clean. Root renderer's later PPD-043normalizer follow-up owns a disjoint createTangents method section; no sculpt edit touches that method or allocation/cache helpers.

- Added `LLVolume::mSculptPixels/getSculptPixels` tracking original supplied pixels,0 for no valid input, before the exact low-area04d901retry.
- Applied actual upstream04d901low-area averaging helper+loop to LLVolume::sculpt; only a too-small area with samples>2pixels apart retries. Excessive area still goes directly to sphere, lowest-detail area waiver remains, and ordinary valid samples/finest detail use original map unchanged.
- Copy/save/rebuild/selection helper is actual production logic. `saveRawImage` admits more pixels at same level for sculpt users, preserves every other user's old discard-only admission. Selection chooses an available larger saved map even while a smaller raw map remains.
- Sculpt CPU-only save notification repairs the absent postCreateTexture event; existing GPU postCreate notification reuses the same helper. Generic loaded-callback readbackRawImage semantics are unchanged.

## Actual validation results

Scratch dependency `llmath llcommon al_test_support` build exit0 `/private/tmp/alchemy-sculpt-core-build.log`. Focus target build:

```
cmake --build /private/tmp/alchemy-upstream-sculpt-build-20261113 --config Release --target alsculptcopy_test --parallel 4
```

Exit0 `/private/tmp/alchemy-sculpt-tests-build.log`. Actual test command:

```
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-upstream-sculpt-build-20261113/Release/alsculptcopy_test '[sculpt-copy]' --reporter compact
```

Log `/private/tmp/alchemy-sculpt-tests-lldb-1.log`, child PID17911 **CHILD_EXIT0 STATE10**, **73 assertions/5 test cases passed**. This executes LLImageRaw's real copyScaled/copy bytes, production sculpt-copy sizing/save/admission/selection functions, and real LLVolume::sculpt with the new pixel tracking/low-area loop. Covers thin maps both orientations,4096-wide/tall maps fitting the pixel budget, larger fallback copy dimensions/mip, more same-level pixels rebuilding geometry, duplicate rejection, coarse collapsed-ring oblong-map LODs, unchanged finest257profile samples and high-area noise preserving sphere. No crash or signal.

Existing volume regression build:

```
cmake --build /private/tmp/alchemy-upstream-sculpt-build-20261113 --config Release --target llmath_tests --parallel 4
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-upstream-sculpt-build-20261113/llmath/Release/llmath_tests '[llvolume]' --reporter compact
```

Build exit0 `/private/tmp/alchemy-sculpt-llmath-tests-build.log`; execution log `/private/tmp/alchemy-sculpt-volume-regression-lldb.log`, child PID18303 **CHILD_EXIT0 STATE10**.42cases:23passed/19tagged-expected-failure cases;4256assertions:4233passed/23expected failures. **Do not describe every legacy volume case as passing:** known nonfinite tangent/normal expected failures remain and are being handled separately under PPD-043. No unexpected failure appeared.

Actual changed viewer translation units compiled cleanly (not only helper tests). Configured same scratch tree with `-DAL_BUILD_VIEWER:BOOL=ON`, exit0 `/private/tmp/alchemy-sculpt-viewer-configure.log`. Direct object-target build would first build624other dependency objects; stopped it with SIGINT (exit130, no crash) rather than redundantly complete the whole app root already builds. Saved exact generated commands with `ninja -C ... -f build-Release.ninja -t commands <llviewertexture.cpp.o> <llvovolume.cpp.o>` to `/private/tmp/alchemy-sculpt-object-commands.log`; executed both actual compiler argument arrays in Python subprocesses with only generated `-include-pch` use removed, preserving the same forced source precompiled-header include and every compiler flag/include/definition. Redirected object/dependency outputs to `/private/tmp/alchemy-sculpt-{llviewertexture,llvovolume}.o{,.d}`. Both compiler exits0, no diagnostics:

- `/private/tmp/alchemy-sculpt-llviewertexture-compile.log`
- `/private/tmp/alchemy-sculpt-llvovolume-compile.log`

`git diff --check` exit0 before commit. No source CMake edits, no canonical docs edits, no performance claim, no native async readback import. No viewer launch/login/scene operation and no push.

## Integration and remaining limits

Root owns registration of dedicated test target in newview/CMakeLists.txt (snippet above), combined application build/verify and canonical PPD-052state/ledger updates. GPU readback owner already supports requested native mip and4096size; new sculpt policy requests that API synchronously. The focused tests validate actual CPU copy/volume contract and policy, not a logged-in native sculpt scene. Owner runtime comparison and explicit approval remain required before push/closure. Existing expected volume failures are not closed by this scoped change.
