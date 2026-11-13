# Final geometry core evidence — 2026-11-13

## Source identity and scope

Read-only final report; source is frozen. Worktree `/private/tmp/alchemy-geometry-core-20261113`, branch `codex/geometry-core-20261113`, base `0c68a66e2189d79ccf221352ecea1fa905d3ca79`, final local commit `e622a133f1d1011900f3c9189490e2f7e222ddda`. Working tree was clean at final handoff. Pinned reference `408fd6c014e6e3b2b90dfe61265f34df0628f450`, coherent geometry/generator/tangent/planar/SIMD/volume-cache stack through #567/#571/#572/#573/#575/#576/#577. Explicit owned source deltas were inspected; this was not a whole-tree replacement. Prior local fork changes were retained where needed. Audit artifacts: `/private/tmp/alchemy-geometry-core-prior-fork-delta-20261113.patch` and pinned copies `/private/tmp/alchemy-geometry-core-upstream-20261113/`.

Local coherent commits:

- `20e4a274e9`: final SIMD/vector kernels and strict planar implementation/tests; root integrated as `010b...`.
- `d81f565dff`: final geometry core, generators, normal/tangent/remap algorithms, planar/frame and volume-cache APIs, allocation-safe adapters and regression tests; root integrated as `e0f...`.
- `8d98d6a8dd`: actual glTF primitive tangent/topology migration and dedicated production-source regression; retired `llmeshoptimizer.cpp/.h` after all canonical callers had been migrated; root integrated as `2876762102`.
- `e622a133f1`: PPD-062 stale tangent owner reuse through Arena staging; root integrated as `84844a3646`.
- Prerequisite ancestor `846da5f8c9` is the cherry-pick of infra's `30fe09fd53ea2271bd7d132610199a0c6d7f0212`; root already integrated original, so do not cherry-pick this duplicate.

Root owns canonical docs/register/CMake and native consumers. Uploader agent owns ALMeshSimplifier/LLModel/preview CPU changes. Infra owns llcommon Scratch/SIMD and matching dependency. No CMake or canonical build-directory writes by this agent. No pushes, viewer launches, visual acceptance or performance measurements.

## Production behavior and integration boundaries

`llvolume.cpp/.h` imports the final normal/tangent generation, flexi partial rebuild, sculpt/prim generation, planar caches/frames and decoded-face optimization behavior. Previous bounds, zero-normal, index admission, octree ownership, sculpt pixel/aspect and low-area averaging safeguards were retained. `llmath.h`, quaternion and octree source fixes were left intact. `transform_directions_keep_w` is retained for native skinning.

Public contracts communicated before integration: bool `LLVolumeFace::createTangents()`, `LLVolume::genTangents(face)`, `LLVolumeFace::remap()` and `LLVolumeFace::generateNormals(crease_angle, smoothing)`; noexcept full `LLVolumeFace::swap` and `LLJointRiggingInfoTab::swap`; final planar/frame and VolumeProbe/decode APIs. Root guards actual native callers; uploader propagates failures instead of publishing partial results.

PPD-048 was reopened by root BEFORE repairs for incoming generateNormals/remap/createTangents, optional ordering and constructor/copy/assignment allocation hazards. Adapters stage output, preserve original owners on failure, return false where required and catch bad_alloc. Optional cache ordering retains its earlier true/original-face semantics when only optional optimization cannot allocate; malformed input still fails admission. Copy construction/strong assignment check aligned allocations, preserve extents/center adjacency and do not delete old octree state before successful publication. Full face swap includes private caches and owning rigging-table state. No physical OOM was attempted.

PPD-053 keeps the original nineteen failing geometry contracts/history. They are now actual positive cases, not merely stripped shouldfail tags. Eighteen retain their exact names. The zero-area-only vertex contract is strengthened from finite-normal to taking a neighbour's normal. The exact 19-name old/new mapping is `/private/tmp/alchemy-geometry-core-checks-20261113/formerly-failing-case-map.json`. It covers cylinder/sculpt/torus normals, S/T seam wrapping, degenerate caps and tangents, unit generation normals, square holes, flexi partial rebuilds and retained buffers, seam normals, twisted cap UVs/sphere caps, placeholder sphere normals and zero-area neighbour normals. Root independently owns the strict old-source nineteen-case negative control; this report does not invent its outcome or log path.

PPD-062 was registered BEFORE repairing our d81 implementation: regenerating stale tangents had replaced an existing aligned tangent owner every time. Final code stages with `ALScratchArena::Scope`, then copies completed output into the existing owner. Initial absent output still uses aligned RAII ownership. Actual flexible-volume regression verifies six owners stay identical, zero heap fallback blocks after warm-up and Arena staging is used. This is allocation/pointer evidence, not an FPS claim.

PPD-059 glTF production implementation ports `7a7296abfc` compatible `meshopt_generateTangents(..., meshopt_TangentCompatible)` with glTF V-sign adjustment, and `2bb25308c2` attribute count plus JOINTS/WEIGHTS pairing checks. Live mTangents, tangent weld stream, MAP_TANGENT/VBO upload and ray-picking consumers are preserved; later upstream removal of unused tangents was not imported. Shared actual-runtime triangle corner mapping now governs strip/fan unroll and picking, validates surface counts before unsigned count arithmetic, corrects fan 0/i+1/i+2 and strip winding, handles direct unindexed corners, and uses runtime mGLMode while retaining authored mMode for serialization. Points/lines still prepare and have no triangle picking tree. Wrapper/Mikk retirement occurred only after canonical callers were migrated. Root owns external/CMake/attribution cleanup.

New source `alsimdplanar.cpp` requires strict FP (`-ffp-contract=off -fno-associative-math -fno-reciprocal-math`) and no PCH; same strict options apply to alsimdkernels test TU. `LLVector4a::initClass` installs Arena allocation callbacks before meshoptimizer scratch consumers. Matching dependency is custom meshoptimizer 1.3#5 at `/private/tmp/alchemy-geometry-meshoptimizer-20261113/installed/arm64-osx-alchemy`; infra separately validated its patches/allocators. No dependency ABI inferred from an old archive.

Root was given dedicated CPU test registration: `al_add_test_binary(newview_gltf_primitive_tests PROJECT ${test_project} SOURCES tests/gltf_primitive_test.cpp UNIT gltf/primitive.cpp LIBRARIES llui ${test_libs} INCLUDES ${llui_headers} ${CMAKE_CURRENT_SOURCE_DIR}/gltf)`. Root verified the current include variable is llui_headers and used that actual variable; the child initially suggested newview_includes, which is not defined here. No viewer executable or viewer settings/fakes are needed. The fixture defines the Asset clock and exercises actual Primitive.cpp.

## Exact build/probe commands and raw evidence

All raw evidence is under `/private/tmp/alchemy-geometry-core-checks-20261113/`. Commands were executed as argument arrays, without shell expansion. Full exact compile/link arrays are in `all-compile-commands.json`, `geometry-tests-link.json`, `geometry-sanitized-link.json`, `gltf-primitive-compile.json`, `gltf-tests-link.json`; each run has a corresponding `*-lldb-run.json` containing its exact debugger arguments and LLDB exit status.

Build drivers:

- `python3 /private/tmp/alchemy-geometry-core-build-tests-20261113.py` built every then-current math production TU freshly, plus planar/scratch and four focused test TUs; compile/link exit 0 after correcting an initial file-selection filter before runtime. The final archived command list excludes retired llmeshoptimizer. Old leftover scratch object files are not proof of archive membership.
- `python3 /private/tmp/alchemy-geometry-core-sanitizers-20261113.py` rebuilt the complete focused math production/test set with `-fsanitize=address,undefined -fno-omit-frame-pointer`, then linked fresh sanitized archive. Exit 0.
- `python3 /private/tmp/alchemy-geometry-final-readonly-checks-20261113.py` rebuilt changed final llvolume and its test TU, reconstructed sanitized archive from the final explicit member list, and relinked. Exit 0. Logs `indra_llmath_llvolume.cpp.o.final-san.compile.log`, `indra_llmath_tests_llvolume_test.cpp.o.final-san.compile.log`, `final-sanitized-link.log`.
- glTF actual Primitive.cpp compiled with the recorded `gltf-primitive-compile.json`; `python3 /private/tmp/alchemy-geometry-gltf-test-build-20261113.py` compiled the fixture and linked against actual fresh math/common/support. Final compile/link exit 0. One initial private fixture compile needed its local gltf include directory; corrected before runtime.
- `python3 /private/tmp/alchemy-geometry-source-controls-20261113.py` built exact old glTF and pinned-upstream generateNormals controls. Compile/link exit 0. Control code and logs are retained beside the binaries.

All runtime commands used `python3 /private/tmp/alchemy-geometry-core-run-contained-20261113.py BINARY FILTER LABEL`. That runner invokes LLDB batch with `settings set target.disable-aslr false`, `process launch -X false`, then inspects actual process state/exit. A fault stop would backtrace all threads and kill the child, never continue or pass signals. All observed runs below exited normally; there were no fault stops, signal resumes or CrashReporter launches. All LLDB wrapper exit statuses were 0; negative controls' actual child status was 42 and was not treated as a passing test.

| Actual run / filter | Observed result | Raw log |
| --- | --- | --- |
| geometry_tests `[llvolume]` initial d81 | 243811 assertions; 103 passed + 1 explicit corpus skip / 104 cases; child 0 | volume-first-lldb-run.log |
| geometry_tests `[alsimdkernels]` | 90412 assertions / 19 passed; child 0 | kernels-first-lldb-run.log |
| geometry_tests `[LLVector4a]` | 608 assertions / 14 passed; child 0 | vector-first-lldb-run.log |
| geometry_tests exact former19 names | 38 assertions / 19 passed; child 0 | former-19-positive-lldb-run.log |
| geometry_san_tests combined initial d81 | 334831 assertions; 136 passed + 1 explicit corpus skip / 137 cases; child 0 | sanitized-core-lldb-run.log |
| **geometry_san_tests combined FINAL e622** `[llvolume],[alsimdkernels],[LLVector4a]` | **334840 assertions; 137 passed + 1 explicit corpus skip / 138 cases; child 0; no ASan/UBSan diagnostics** | **final-sanitized-core-lldb-run.log** |
| geometry_tests `[mesh-stale-tangent]` final | 9 assertions / 1 passed; child 0 | stale-positive-lldb-run.log |
| exact d81 archive plus same new stale test | 7 failed / 9 assertions, 1 failed case; child 42 | stale-d81-negative-lldb-run.log |
| gltf_primitive_tests `[gltf-primitive]` | 192 assertions / 5 passed; child 0 | gltf-positive-lldb-run.log |
| exact base0c68 glTF body, same fixture | 49 failed / 70 assertions; 4 failed + 1 passed / 5 cases; child 42 | gltf-old-negative-lldb-run.log |
| exact pinned408 generateNormals body, final typed surrounding source, `[mesh-normal-allocation]` | 1 failed / 4 assertions; escaping bad_alloc; child 42 | upstream-normal-negative-lldb-run.log |

The last normal control replaces ONLY the upstream generateNormals method body in the final typed source; it proves the incoming nonfatal failure contract was not sufficient. Permanent new normal/remap/required/optional cache allocation failure regressions are in actual almesh_decode_safety_test.cpp and passed in the combined sanitized run. This is deterministic allocation injection, not evidence of real OOM frequency.

For the old glTF control, only two relative include paths were redirected to the private build. A PRIVATE `gltf_old_allocation_guard.cpp` throws before operator-new requests over 8 MiB; it prevents old short-count underflows from requesting huge physical allocations. It is not a production allocation policy. Those ordinary exceptions appear as test failures. No giant physical allocation or crash was induced. The 192-assertion positive fixture covers authored/generated strip/fan tangents and picking, seven short attribute families, JOINTS/WEIGHTS mismatch, 0/1/2 surface counts, incomplete triangles, points/lines, and direct unindexed fan picking. Actual GPU upload was not executed.

## Limits and handoff

The final sanitizer proof identifies our e622 source plus its prerequisite/core stack, not the later whole canonical viewer source. Root's frozen combined canonical source is `3ee7786e670aaf4fcecba0db91d30f2cf659e7ec`; root owns its full build, native buffer/consumer tests, PPD-063 native clone/layout and PPD-064 native reuse-count corrections, and final integration evidence. This child did not rerun a whole viewer build or claim release readiness.

AL_MESH_CORPUS_DIR was unset, so one real-world mesh corpus case explicitly skipped. Generated procedural, pathological/fault-injection and SIMD cases ran. The full planar diagnostic examined 26152 generated faces: worst flat-plane drift 2.38e-7, zero past1e-4; this is a CPU correctness diagnostic, not scene acceptance or speed measurement. External dependency libraries were not sanitizer-rebuilt by this agent; actual math source/test TUs were. No FPS, GPU, visual, owner acceptance or whole-dc0f allocation-equivalence claim follows from these tests. No further source changes were made after source freeze; final evidence only.
