# Avatar CPU geometry evidence — 2026-11-13

## Frozen source and ownership

- Worktree `/private/tmp/alchemy-geometry-infra-20261113`, branch `codex/geometry-infra-20261113`.
- Avatar-only commit **`0df1da4b8467f9a36ed1866f18175c3a69853612`**, parent `2fbe215cdd1a1e78410a015a06eba0caa8e4935e` (already integrated infra ancestors must not be cherry-picked again).
- Exactly four files: `indra/llappearance/llavatarappearance.cpp`, `llpolymesh.cpp`, `llpolymesh.h`, `tests/llpolymesh_test.cpp`.
- Parent supplied and authorized upstream patch `/private/tmp/alchemy-geometry-avatar-root.patch`; integrated upstream `26b656639dd5f96b5fab84e5457178857928bd2a` (load-time triangle cache ordering) and `4a118a46eb13e4bf6ed71f33d1e3a2a5b24908d2` (hair/skirt/eyelash authored-order exceptions). Corresponding final behavior inspected in local upstream ref `408fd6c014`.
- Parent owns canonical PPD register, state/ledger, all CMakeLists, integration, whole app build and owner runtime gate. No edits to those files here. No push, login, scene operation or viewer launch.

## Production behavior and IDs

1. **PPD-048 reopened:** cache ordering is optional. All output U32 indices are staged in a vector; only a successful optimizer call publishes them to original S32 faces. A `std::bad_alloc` from staging or meshoptimizer retains all original faces and continues normal loading. Both positive and negative out-of-range indices bypass the optimizer via unsigned range check. Vertex indices within triangles and their winding remain unchanged. Vertex/morph/UV/weight arrays retain their indexing. `LLAvatarAppearance::loadMeshNodes` explicitly sets keep-order for `MESH_ID_HAIR`, `MESH_ID_SKIRT`, `MESH_ID_EYELASH` in both reference and standalone load paths. This is load-time CPU ordering, with no measured performance claim.
2. **PPD-060:** the loader read `hasDetailTexCoords` but did not assign `mHasDetailTexCoords`, so a valid detail stream was skipped and subsequent file sections were misread. The new assignment occurs after a successful flag read, only for non-LOD meshes, matching the existing weights boundary. `setupLOD` continues inheriting the reference's flag and UV pointer. Parent registered the defect before production edits.
3. **PPD-061:** `initializeForMorph` copied `mBaseNormals` into both instance binormal arrays. A fixture with base normals `(0,0,1)` and base binormals `(0,1,0)` reproduced instance binormals `(0,0,1)` while base binormals were correct. Parent registered the finding before repair. The only repair changes the two copy sources to `mBaseBinormals`. The LOD constructor aliases the reference arrays and remains unchanged.

Actual PPD-061 consumers: `LLPolyMorphTarget::apply` in `llpolymorph.cpp` obtains `getScaledBinormals` and `getWritableBinormals`, passes them to `alsimd::morph_apply`; `alsimdkernels.inl` accumulates binormal deltas, cross-products against the normal and normalizes the result. `applyMask` subtracts masked binormal deltas from the scaled array. The two initialized arrays therefore must start with binormals, not normals. This is CPU correctness evidence; no avatar visual outcome is claimed.

## Real loader tests

`llpolymesh_test.cpp` uses actual public `LLPolyMesh::getMesh/getFaces`, generated valid little-endian Linden Binary Mesh 1.0 files and the production loader/meshoptimizer. It creates a temporary read-only-data directory, scopes a minimal `LLDir` subclass into `gDirUtilp`, and restores it. Instances die before `freeAllMeshes`, then temporary files are removed. No user profile directory is initialized and no product test hooks were added.

The six cases verify:

- Opaque faces are actually reordered; sorted complete ordered triplets match exactly (including winding). All positions, normals, base/instance/scaled binormals, UVs, weights and skin joint names remain correct. Repeated cached loading shares shared data; `genIndices(17)` reflects optimized faces correctly.
- Hair, skirt and eyelash requests keep authored faces exactly and call no optional allocator.
- Real meshoptimizer allocator call count is measured (6 for this fixture/package). Every allocation position 1 through 6 is made to throw in a separate unique mesh load. Every load succeeds, retains exact authored faces and all attributes. Global callbacks are saved/restored in scope; successful calls delegate to the prior allocator. The bounded loop avoids hardcoding the implementation's allocation count.
- An out-of-range face skips optional optimization and remains unchanged. The test does not dereference or render that index.
- LOD faces own their order while coordinates, weights and both instance binormal arrays alias the reference. Attributes remain correct.
- A valid detail-UV flag1 file consumes its exact finite detail data and faces; a LOD with file flag0 inherits reference flag1 and pointer identity. The detail fixture embeds a benign End Morphs sentinel in the finite UV byte payload, so the old reader safely returns an incorrect mesh instead of allocating arbitrary morph data.

A read-only header scan of the bundled app's `Contents/Resources/character/*.llm` found **29 files, 27 `(weights=1,detail=0)` and 2 `(weights=0,detail=0)`**; none sets the detail flag. This bounds bundled-asset incidence but does not invalidate format support.

## Build, dependency isolation and registration

Host observed `uname -m=arm64`, macOS 26.5.2 build25F84. Own source, own llappearance/math/common/etc archives in `/private/tmp/alchemy-geometry-infra-build-20261113`; no stale canonical class ABI was linked. Canonical Xcode installed third-party inputs remain read-only. Meshoptimizer explicitly uses the isolated final custom 1.3#5 package:

`/private/tmp/alchemy-geometry-meshoptimizer-20261113/installed/arm64-osx-alchemy`

Final llappearance and test compile commands put this package's include directory first among system include paths; final link uses its `lib/libmeshoptimizer.a`. Earlier initial control compilation saw a transitive Xcode header, whose only diff is a tangent API comment (all signatures/layouts identical). Final receipts explicitly prioritize the isolated header. The isolated archive SHA256 is `c39beb4038c8cbc4afddda407d9821e075c93f8c5fc6fd7240920c052fe1d132`.

No repository CMake edits. Scratch uses external deferred registration `/private/tmp/alchemy-geometry-infra-include.cmake` and `/private/tmp/alchemy-geometry-infra-register.cmake`. Parent required registration after `llappearance_test_fakes`:

```cmake
al_add_test_binary(llpolymesh_test PROJECT llappearance
    SOURCES tests/llpolymesh_test.cpp
    LIBRARIES llappearance
    SUPPORT llappearance_test_fakes)
```

Commands (all build/configure commands exit0; ordinary duplicate-static-library linker warnings only):

```sh
cmake -S indra --preset ninja-os-arm64 -B /private/tmp/alchemy-geometry-infra-build-20261113 -DAL_BUILD_TESTS:BOOL=ON -DAL_VCPKG_INSTALL:BOOL=OFF -Dmeshoptimizer_DIR:PATH=/private/tmp/alchemy-geometry-meshoptimizer-20261113/installed/arm64-osx-alchemy/share/meshoptimizer
cmake --build /private/tmp/alchemy-geometry-infra-build-20261113 --config Release --target llpolymesh_test -j 6
```

Scratch existing cache has AL_BUILD_VIEWER=OFF and the external CMAKE_PROJECT_INCLUDE registration. Logs: `/private/tmp/alchemy-avatar-configure.log`, `alchemy-avatar-build.log`, `alchemy-avatar-green-build.log`, `alchemy-avatar-final-build.log`, `alchemy-avatar-verified-build.log`, `alchemy-avatar-ppd061-build.log` (all `/private/tmp/`). Last production build includes changed `llavatarappearance.cpp`, so the blend-category integration compiled with the real class.

## LLDB containment and results

Every runtime test ran through LLDB; none launched directly. `/private/tmp/alchemy-remaining-hdr-lldb.commands`:

```text
settings set target.env-vars MTL_DEBUG_LAYER=1
process launch --shell-expand-args false
script p=lldb.debugger.GetSelectedTarget().GetProcess(); state=p.GetState(); print('CHILD_EXIT',p.GetExitStatus(),'STATE',state); lldb.debugger.HandleCommand('thread backtrace all') if state == lldb.eStateStopped else None; p.Kill() if state == lldb.eStateStopped else None
quit
```

All listed processes exited normally (state10), without a stop or signal. LLDB wrapper exit0 does not imply child success; actual child statuses were parsed.

| Phase | PID | Actual child | Result | Log under `/private/tmp/` |
|---|---:|---:|---|---|
| PPD060 red, ignored detail flag unchanged |47367|42|2 assertions pass,1 fails,1 case|`alchemy-avatar-detail-red.log`|
| Initial full green after detail fix, original required attrs |47459|0|6350 assertions,6 cases pass|`alchemy-avatar-green.log`|
| Newly strengthened instance-binormal expectation BEFORE PPD061 fix |47737|42|11129 assertions pass,2106 fail,6 cases fail|`alchemy-avatar-final-green.log` (name predates result; **this is RED**) |
| Exact upstream optional in-place ordering control |47840|42|6 assertions pass,1 fails due escaped bad_alloc,1 case|`alchemy-avatar-allocation-red.log`|
| Expanded all-allocation tests, base-binormal coverage before PPD061 fix |48024|0|13235 assertions,6 cases pass; verbose output records allocation_count6|`alchemy-avatar-verified-green.log`|
| FINAL PPD048+060+061 source and permanent tests |48148|0|**19555 assertions,6 cases pass**|`alchemy-avatar-ppd061-green.log`|

Exact commands:

```sh
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/llpolymesh_test '[llpolymesh-detail]' --reporter compact
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/llpolymesh_test '[llpolymesh]' --reporter compact
python3 /private/tmp/alchemy-avatar-old-control.py
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-avatar-ordering-old-control '[llpolymesh-allocation]' --reporter compact
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/llpolymesh_test '[llpolymesh]' --reporter compact --success
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/llpolymesh_test '[llpolymesh]' --reporter compact
```

Old optional-ordering control: Python takes the current source before PPD061 repair, replaces ONLY `if(in_range)` staging/catch block with exact local pinned408 upstream block, compiles with own compile_commands, and explicitly links that object ahead of own llappearance archive. It retains the detail fix and everything else. Files: `/private/tmp/alchemy-avatar-ordering-old-control.cpp`, `.o`, executable without extension; build log `alchemy-avatar-old-control-build.log`. No production tree was temporarily reverted.

Final test executable SHA256 `ffe2fb83330b018c7db726d176f7b75d37688efb1e1475d2261325498ce9f199`.
Old control executable SHA256 `c75b9ea5d068cafe96f6be252c1336d3e2181d7d8304cc9709905952d58c07fc`.
Old control source SHA256 `d9660ed363d77fa2b22f97f2de06df20a4ca547717c14b72e9603689fd94b0b1`.

`git diff --check` exit0 and commit worktree clean after commit. Parent can integrate this avatar-only commit immediately. Whole app rebuild, native app verification, scene visual validation and owner runtime acceptance remain parent/owner prerequisites. No performance improvement, scene result or release completeness is claimed.
