# Geometry prerequisites — PPD-053 / PPD-057 — 2026-11-13

Local source commits `30fe09fd53ea2271bd7d132610199a0c6d7f0212` and test compatibility follow-up `2fbe215cdd1a1e78410a015a06eba0caa8e4935e`, branch `codex/geometry-infra-20261113`, worktree `/private/tmp/alchemy-geometry-infra-20261113`, base `0c68a66e2189d79ccf221352ecea1fa905d3ca79`. Root created the clean worktree before this assignment. No renderer, sky, pacing, CMakeLists, canonical documentation, viewer launch, login or push changes. Parent owns integration/CMake wiring/canonical dependency installation/whole-app verification and owner approval.

## Source gate and ownership

Read repository AGENTS, continuity/original-reference/Metal-port/GPU-validation skill instructions earlier in this session. Read canonical PPD-053 before implementation. Exact upstream reference pinned `408fd6c014e6e3b2b90dfe61265f34df0628f450`:

- `f8dc4f830adabdcded206a23b4d99d58e498bbfa`: initial per-thread scratch arena,16-byte alignment,4MiB capacity, LIFO scope, heap fallback and counters.
- `0daac8b0159035dbeea4cd78f309fa5fb78437d8`: release-order validation; out-of-order return leaves active blocks intact; thread-local release makes late allocations fall back to heap instead of freed storage.
- `68a39b2b32893ee4ede5f4882803f9fad4abed69`: arena remains alive until outstanding TLS-held blocks return after thread retirement. Final cpp/h and original tests imported exactly, apart from the new typed overflow guard/test and sanitizer-helper compatibility below.
- `3f9ab7013c`: final `alcompact.h` + tests. Stable one-pass compaction accepts append during predicate, copying each current item before a possible vector relocation.
- `1994334c9c4a79a4f250f1e1fd62c4f81560ea5d`: signed integer SIMD load/store/set/and/compare plus packed saturated bytes required by final geometry kernels. Imported these64header lines and final tests. **Unrelated pause/pacing primitive from c552072adb was deliberately omitted.** Existing native NEON/vector APIs retained.
- `4dd25aae45e36dca11fecbc49b399376c2a94781`: registry baseline for final custom meshoptimizer1.3#5, `ee9cfc3d9ac97195a137d8fc1a1fd1902f1858d6`.

Core peer owns llmath and allocator installation in final `LLVector4a::initClass`: `meshopt_setAllocator(ALScratchArena::allocate, ALScratchArena::deallocate)`. Uploader peer owns its meshopt API consumers. Neither API consumer or geometry algorithm changed in this infra branch.

## Preregistered PPD-057 correction

Discovered before repair: upstream `Scope::allocate<T>` multiplies count*sizeof(T) without a bound check. A count greater than SIZE_MAX/sizeof(T) can wrap to a small allocation before the raw allocator's own overflow checks run. Reported to parent; parent preregistered PPD-057, then authorized narrow correction.

The template now throws `std::bad_array_new_length` before multiplying. This derives from bad_alloc and matches core's existing failure handling. New executed regression checks no arena/heap allocation occurs on overflow, the scope remains usable, and safely demonstrates the original wrapped byte expression allocating a zero-byte block. The control performs **no out-of-bounds write** and is not intended to crash. No new over-alignment restriction or unrelated allocator refactor.

## Dependency isolation / registry choices

Current application public registry baseline `e189a7d10ef4269b7fbe7f91bf667bcb139cdc4f` contains newer unrelated ports than upstream geometry's baseline. It and all existing package mappings remain unchanged. Added a separate public git-registry entry mapping **only meshoptimizer** to ee9cfc3. Existing private-registry mapping remains byte-for-byte unchanged and was not accessed for this operation. Manifest asks meshoptimizer version>=1.3#5. No bulk baseline rollback/upgrade and no private credentials.

There is no Meshoptimizer*.cmake in current or pinned upstream source. Actual import is `indra/cmake/Dependencies.cmake:84`, `al_import(ll::meshoptimizer PACKAGE meshoptimizer CONFIG TARGETS meshoptimizer::meshoptimizer)`. It correctly imports this package; no new module or shared dependency edit was necessary. Root explicitly agreed to leave it unchanged absent a concrete gap.

Isolated package prefix:

`/private/tmp/alchemy-geometry-meshoptimizer-20261113/installed/arm64-osx-alchemy`

- Header: `include/meshoptimizer.h`
- Release static archive: `lib/libmeshoptimizer.a`
- Debug static archive: `debug/lib/libmeshoptimizer.a`
- CMake config directory: `share/meshoptimizer`
- MIT/full bundled notices: `share/meshoptimizer/copyright`
- SPDX: `share/meshoptimizer/vcpkg.spdx.json`

Peers can configure `-Dmeshoptimizer_DIR:PATH=/private/tmp/alchemy-geometry-meshoptimizer-20261113/installed/arm64-osx-alchemy/share/meshoptimizer` alongside their ordinary installed dependencies. Paths sent directly to both peers and parent immediately after build.

## Actual ARM64 package build and provenance

Minimal validation manifest `/private/tmp/alchemy-geometry-meshoptimizer-20261113/manifest/vcpkg.json` has only meshoptimizer>=1.3#5 plus its two CMake host dependencies; builtin baseline da2be01c400dd3ed102c1198752ef44c76aabe37. Registry config maps only meshoptimizer to ee9. Reused a copied public v1.3 source archive in isolated downloads, not a changed canonical installed library.

Exact actual install (cwd isolated manifest):

```
env VCPKG_FORCE_SYSTEM_BINARIES=1 /Users/dekstroza/Programming/AI/Secondlife/Alchemy/vcpkg/vcpkg install --triplet arm64-osx-alchemy --host-triplet arm64-osx --x-install-root=/private/tmp/alchemy-geometry-meshoptimizer-20261113/installed --x-buildtrees-root=/private/tmp/alchemy-geometry-meshoptimizer-20261113/buildtrees --x-packages-root=/private/tmp/alchemy-geometry-meshoptimizer-20261113/packages --downloads-root=/private/tmp/alchemy-geometry-meshoptimizer-20261113/downloads --binarysource=clear --no-downloads
```

Exit0 `/private/tmp/alchemy-geometry-meshopt-install.log`. Only meshoptimizer1.3#5 and vcpkg-cmake/vcpkg-cmake-config host packages installed into this new prefix. Binary caching disabled for actual source build. Both Debug and Release built from patched source at `/private/tmp/alchemy-geometry-meshoptimizer-20261113/buildtrees/meshoptimizer/src/v1.3-4c0faa8cbc.clean`. Port Git tree `a538edc6ae1b0801ec11c5514f0a0e4bac578a2f`, package ABI `b88f5a166dfef2a130f3fae8b6137480d84dafb8b8231ab605874ca8bb5c578c`.

The source tarball SHA512 is `eef5c17afbdd551c44bc3f55d42099563d1370392f2bcc56e5e1099d37abf7ece439e6bfdee6be3adff3960c1d510956dd1cd5ddf4b1ef7c7625f0671c466ad2`. Port applied dependencies.diff, compatible-tangents-match-mikktspace.patch and vertex-remap-probe-budget.patch. Installed vcpkg_abi_info records compatible tangent patch hash `162dc578f0fbf34cdc6bd7ead08f28c9b4c7e3cecc02e17e8de165a0c148f4c2` and probe-budget patch `bb161ac5cd34b66349dca70da46ab3f91d928ad6165cb61e97a586590077cc16`.

Actual `lipo -archs` on both archives returned **arm64 only**. `nm -gU` verified exported C symbols generateTangents, generateNormals, generatePositionRemap, simplifyWithAttributes, simplifySloppy, simplifyPrune, setAllocator. Installed header byte-compares equal to actual patched source header. SPDX names1.3#5 and ABI; MIT license text/notices are installed.

SHA256:

- Release archive `c39beb4038c8cbc4afddda407d9821e075c93f8c5fc6fd7240920c052fe1d132`
- Header `5f42863752082b36e0b29aaa457f2170feb8802859ef9fb9aa5789a62816b498`
- Copyright `fc89efde3f3c6fbae26bf4e3300cc5acebd2a06c4eb9cb6976ae9618420a2101`

## Canonical dependency DRY RUNS ONLY

Agent did **not** install into either existing canonical installed tree. First dry run used the older Xcode installed tree. Parent corrected that the active canonical build uses a separate real Ninja installed tree. Authoritative final dry run below uses Ninja. An initial Xcode plan without tests would remove catch2; that plan was rejected and never executed. Include `--x-feature=tests` to retain Catch2.

Exact final active-Ninja DRY RUN (cwd this worktree/indra):

```
/Users/dekstroza/Programming/AI/Secondlife/Alchemy/vcpkg/vcpkg install --triplet arm64-osx-alchemy --host-triplet arm64-osx --x-feature=tests --x-feature=tracy --x-feature=tracy-local --x-feature=tracy-on-demand --x-feature=webrtc --x-feature=faudio --overlay-ports=/private/tmp/alchemy-geometry-infra-20261113/indra/cmake/ports --x-install-root=/Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-ninja-os-arm64/vcpkg_installed --x-buildtrees-root=/private/tmp/alchemy-geometry-meshoptimizer-20261113/canonical-ninja-plan-buildtrees --x-packages-root=/private/tmp/alchemy-geometry-meshoptimizer-20261113/canonical-ninja-plan-packages --downloads-root=/private/tmp/alchemy-geometry-meshoptimizer-20261113/downloads --dry-run
```

Exit0 `/private/tmp/alchemy-geometry-canonical-ninja-meshopt-plan-with-tests.log`: **only meshoptimizer1.3#5 rebuilt; no other removal/build/change**. Root owns actual install after freezing old controls and coordinating all consumers. Never execute the historical Xcode-path suggestion as an actual canonical install. Parent received this correction and final Ninja result.

## Executed package allocator / hostile-weld assertions

Compiled the actual patched package `demo/tests.cpp` (assertions explicitly enabled by that source) plus a tiny runTests main against **the installed Release archive**, not a replacement library. Command:

```
clang++ -std=c++23 -arch arm64 -mmacosx-version-min=26.0 -O2 -g /private/tmp/alchemy-geometry-meshoptimizer-20261113/buildtrees/meshoptimizer/src/v1.3-4c0faa8cbc.clean/demo/tests.cpp /private/tmp/alchemy-geometry-meshoptimizer-20261113/test-main.cpp /private/tmp/alchemy-geometry-meshoptimizer-20261113/installed/arm64-osx-alchemy/lib/libmeshoptimizer.a -o /private/tmp/alchemy-geometry-meshoptimizer-20261113/meshoptimizer-assertions
```

Build exit0 `/private/tmp/alchemy-geometry-meshopt-assertions-build.log`. Ran under LLDB batch using `/private/tmp/alchemy-remaining-hdr-lldb.commands`, which disables shell expansion, prints actual child exit/state and backtraces/kills a stopped child without continuing a signal. Actual child PID41574 **CHILD_EXIT0 STATE10**, `/private/tmp/alchemy-geometry-meshopt-assertions-lldb.log`.

This executes the entire package assertion suite, including customAllocator, hostile `remapCollide`, compatible tangent nonmanifold/coincident/opposite/hub/pole/NaN cases, basic/degenerate normals and API algorithms. Hostile remap creates4096words duplicated into8192vertices whose hashes share low16bits; expected first-index numbering/unreferenced sentinels are checked. It asserts3allocations/3frees for generateVertexRemap and cumulative6/6 after Multi, proving the sort-fallback route and balanced ownership. No speedup claim or borrowed benchmark figures.

## Actual source tests / typed Scope guard / TLS lifetime

No repository CMakeLists edit. External `CMAKE_PROJECT_INCLUDE` + deferred temporary registration adds actual alscratcharena.cpp to llcommon and registers geometry_infra_tests from the two new source tests. Existing llcommon_tests supplies updated alsimd_test.cpp. Scratch configure uses canonical Xcode dependency tree only as read-only existing inputs plus explicit new isolated meshoptimizer_DIR; that is independent from the active canonical Ninja build.

Configure final exit0 `/private/tmp/alchemy-geometry-infra-configure-2.log`, with AL_BUILD_VIEWER=OFF, AL_BUILD_TESTS=ON, AL_VCPKG_INSTALL=OFF. An initial temporary target attempted to duplicate alsimd_test.cpp already in llcommon_tests; the harness correctly rejected it at configure. Removed that duplicate and used the existing target. No canonical CMake mutation.

Initial source-test build compiled production Scratch but failed because pinned upstream test referred to newer ll_test::SANITIZER_ALLOCATOR absent from this fork's shared test helper. Follow-up2fbe adds only test-local compiler ASAN detection OR existing THREAD_SANITIZER; no shared helper edit. This compatibility failure is retained in `/private/tmp/alchemy-geometry-infra-tests-build.log`.

Final build:

```
cmake --build /private/tmp/alchemy-geometry-infra-build-20261113 --config Release --target geometry_infra_tests llcommon_tests --parallel 4
```

Exit0 `/private/tmp/alchemy-geometry-infra-tests-build-2.log`. Under LLDB containment:

```
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/geometry_infra_tests '[alscratcharena],[alcompact]' --reporter compact
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/llcommon/Release/llcommon_tests '[alsimd]' --reporter compact
```

- Scratch/compact: **62assertions /14cases passed**, PID43358, **CHILD_EXIT0 STATE10**, `/private/tmp/alchemy-geometry-infra-scratch-lldb.log`. Exercises LIFO reuse,16-byte alignment/zero allocations, heap fallback/full arena, extreme raw request refusal, Scope unwind, out-of-order safety, held block during TLS retirement, late TLS allocations, per-thread isolation, appended compaction/shared ownership, and PPD-057 no-allocation typed overflow guard + safe frozen expression control.
- SIMD: **3682assertions /14cases passed**, PID43355, **CHILD_EXIT0 STATE10**, `/private/tmp/alchemy-geometry-infra-simd-lldb.log`. Actual ARM64 default vector/NEON layer, including unaligned integer bit preservation, signed extremes, whole-lane comparison and saturating ordered byte packing.

Both source commits are clean at handoff. No fault/crash, scene operation, private capture upload, or push. Doctor evidence for the same Apple M1 Max/macOS26.5.2/Xcode26.6 host was collected earlier this session at `/private/tmp/alchemy-remaining-hdr-doctor.log`; no hardware/performance inference is substituted for these executed CPU/package checks.

## Root CMake wiring / integration prerequisites

1. llcommon production source list: alscratcharena.cpp; public header set: alscratcharena.h and alcompact.h.
2. llcommon_tests source list: tests/alscratcharena_test.cpp and tests/alcompact_test.cpp. Updated alsimd_test.cpp is already registered. No new dependency module required.
3. Root cherry-picks30fe then2fbe and performs its coordinated actual only-meshoptimizer upgrade if its own dry run remains safe. Core/uploader peers use isolated DIR until root transition.
4. Root owns canonical whole-application build/verify, PPD-053/057 state/ledger and integration tests. Owner runtime/visual acceptance and explicit push approval remain pending. These bounded prerequisites do not close the complete geometry product or establish any performance improvement.
