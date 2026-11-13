# First upstream object hardening batch — 2026-11-13

- Isolated worktree: `/private/tmp/alchemy-upstream-first-objects-20261113`
- Branch: `codex/upstream-first-objects-20261113`
- Base: `bbd5193f99a3dfea2b9ac6337e3909bba86dc290`
- Local commit: `fe0ecc9321c6f34a042bc65fed3a9baafdec23ba`
- Scope: PPD-021 object input hardening, viewer half of PPD-018 local Xfer filenames, PPD-026 deferred cache-removal caller prerequisite. No push or canonical docs changes.

## Source mapping and implementation

Original base `65c39b7bad` owns CPU object-update parsing in `LLViewerObject::processUpdateMessage`, then volume/tree subclass consumers, and packs texture entries/extra parameters in `LLPrimitive`. The native fork uses the same producer/consumer data model. These changes preserve valid wire inputs and native rendering; malformed sizes/type slots now fail without reading beyond data, and refused compressed updates are killed/requested whole before subclass fields are consumed. Motion fields of unknown length leave current motion unchanged. Inline small-vector storage replaces guessed fixed buffers/unchecked wire-sized allocations while keeping normal updates in place.

Imported the owned paths of upstream merge #619, `6ba1a87017a70b68842bcc2bb126e856d88c06ec`, including:

- `62bf248bf32c3002db6a22b8f1c7570ef77abfa8`: checked particle block/fixed data and bounded skips.
- `a0235bf308d27d1f2e4af351d3aa29d6434c966a`: bounded binary-data vector overloads.
- `5ad54288eda941887f5e7cd0cf2dc68c8b32ab35`: owned name/type/value strings and initialized partial VEC3.
- `444eb52a3ee2ff4c64b61ba98626024d2d5cfd72`: full texture-entry lengths/parse-before-apply.
- `54cd028d551a9ce532ac5d916bafa1fde560ebb7`: bounded terse TextureEntry field.
- `8c29c9f3f082fc03fb96748b171a80c094838c36`, `9330f859032672dc8bddea6bff68a6985cde0641`, `1e16131a41d05822046549b260e976323b5e621b`: strict extra-parameter type slots, complete entries and exact type sizes.
- `e1501fb83bace3345720ed99d632a311911dc8ae`: `ALObjectMotion` valid-format decode and current-motion preservation on unknown length.
- `084874fd9e56d63e6526035613c31deee575d367`, `acfc131923c7b389600456086b2445fcd3f75433`, `eb5633a5e6bd7251c16bada68c367d4358a23d09`, `37fc1019d0aa49835b699cd7d6da3c670b4792b7`: bounded compressed/full data, ownership and inline stores.
- `8756e2be8013f26e37ed12921060b013de9f7ea6`, `bd9479a70006771ae70ed04e94f3c245578e702d`: material count packing and valid empty materials.

Included upstream followup `84a7c7fc7b`: shared `ALCompressedSoundAndNames` and `ALCompressedVolume`, full sound/name/volume/TE/animation read before applying a compressed tail, checked texture-animation read, and its actual helper regression tests.

Included viewer-only `4fa42289ac`: fresh viewer-owned inventory_UUID.tmp/mute_UUID.tmp local names, remote name retained separately, callback uses local path and cleans late/failed task inventory files. Protocol peer owns Xfer implementation/suffix checks.

Included ONLY the `llvovolume.cpp` hunk of `fdda86251c`: malformed cached update requests deferred removal, because the parser may still borrow data held by that cache. Integrator owns cache/world helper implementation and tests. Native residency alpha picking, buffer merge bounds and native material comments remain exactly as the base.

Resolved three-way context conflicts by omitting unrelated already-upstream context: bake-proxy include, LOD-invariant geometry test, llmodel_test CMake registration. No #633 face-retention/object-roster/geometry/sky work imported.

## Required integration prerequisites

- Foundation peer: profile-cut clamp `dfc15bd9eb` and rounding-tail `6369eef3fa` in llmath/llvolume.h/test, completing #619 outside this exclusive path set.
- Protocol peer: llmessage Xfer implementations and protocol hardening/tests.
- Integrator: PPD-026 cache helpers `fdda86251c` + `79057e0958`, including declaration of requestRemoval, deferred queue draining, safe cache entry release and pending-removal admission. This worktree alone deliberately does not compile viewer because those exclusive shared files are integrated separately.

## Commands and observed status

Apple M1 Max arm64 / macOS26.5.2. Commands run inside the worktree:

1. `tools/metal/doctor.sh` — exit0, `/private/tmp/alchemy-first-objects-doctor.log`; reports M1Max unified-memory Metal device. Does not verify app runtime.
2. Initial isolated configure without an explicit toolchain — exit1: worktree vcpkg submodule not initialized. Retry explicitly points at existing root vcpkg toolchain/installed dependency tree, with manifest installOFF (read-only reuse).
3. Second configure — exit1: uninitialized dullahan submodule is still an unconditional CMake subdirectory. `git submodule update --init --no-fetch indra/dullahan` checks out the exact existing pinned ba893db7aa6fc3257d5ee37379d04962c1ead164; no gitlink change.
4. Final configure — exit0: `cmake -S indra --preset ninja-os-arm64 -B /private/tmp/alchemy-upstream-first-objects-build-20261113 -DCMAKE_TOOLCHAIN_FILE=/Users/dekstroza/Programming/AI/Secondlife/Alchemy/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=/Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-xcode-os-arm64/vcpkg_installed -DVCPKG_MANIFEST_INSTALL=OFF -DAL_BUILD_TESTS=ON -DAL_BUILD_VIEWER=OFF -DAL_BUILD_CEF_PLUGIN=OFF -DAL_BUILD_VLC_PLUGIN=OFF -DAL_BUILD_PACKAGE=OFF -DAL_USE_LTO=OFF -DAL_USE_PRECOMPILED_HEADERS=OFF`. Log `/private/tmp/alchemy-first-objects-configure.log`.
5. `git diff --cached --check` — exit0 before commit. Worktree clean after local commit.
6. Test target build — exit0: `cmake --build /private/tmp/alchemy-upstream-first-objects-build-20261113 --config Release --target llprimitive_test llmessage_tests llprimitive_tests --parallel 3`. Log `/private/tmp/alchemy-first-objects-build.log`. Only library/test targets; no viewer build/launch.

## Verification limits and next action

Actual complete-viewer update application, callbacks and owner scenes are not tested by the library helpers. No closure/owner verification/push is inferred. Integrator should cherry-pick fe0ecc9321 with required sibling prerequisites, perform the combined native app build/verify, and run the registered parsing/primitive regressions. Terminal test/build results are retained below.


## Terminal targeted verification

- Test/library build above exits0 at exact source `fe0ecc9321c6f34a042bc65fed3a9baafdec23ba`. Linker emits only a duplicate static-library warning (bz2/freetype/harfbuzz/png/z); no compiler errors. No existing build directory or installed dependency was modified.
- `ctest --test-dir /private/tmp/alchemy-upstream-first-objects-build-20261113 -C Release -V -R '^(llmessage_tests/(lldatapacker|llnamevalue|llpartdata)_test|llprimitive_test/llprimitive_test|llprimitive_tests/(alcompressedupdate|alobjectmotion|llprimitivetemessage)_test)$'` — exit0, 7/7 CTest entries, 88 cases / 1451 assertions. `/private/tmp/alchemy-first-objects-ctest.log`.
- Per-entry counts: datapacker140/25cases; namevalue169/19; partdata56/9; compressedupdate339/6; objectmotion108/5; texture-entry28/4; standalone primitive611/20.
- Test binary SHA256: llmessage_tests `f37177dc2922d032db1a49a7fb951b61a8954f833d13925750a164da9b1dda65`; llprimitive_test `783e7c7ec3db43baa5c563d80475054ff14a72ce8ee3415ea02fb164b805de1c`; llprimitive_tests `1a44afe0af5ed6b2fe737df354c35b52daf6896678484bdb0360ed2ff8f3dfe5`.
- Source preservation probe compares the entire `llvovolume.cpp` suffix beginning `bool LLVOVolume::lineSegmentIntersect(` against HEAD^ and asserts byte equality, exit0. Native alpha-picking, batch bounds and all later geometry/native code are unchanged.
- `git diff HEAD^ HEAD --check` exit0, worktree clean. No post-test source edits or pending build jobs.
- No sanitizer run, baseline red execution, whole-app build/verify, viewer launch, malformed simulator session, or owner acceptance performed by this child task. Integrator owns combined build, cache helper integration, runtime/owner verification and canonical register updates; no push authorized.
