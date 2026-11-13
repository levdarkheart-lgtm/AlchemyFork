# PPD-056 bounded LLFace slot admission control — 2026-11-13

No repository files edited. Parent requested read-only, private source-extracted validation of adjacent face slots. Worktree remains clean at test-only commit e82b5511083ef0ee589336ac1d4f13a198dd1ec5. All generated control sources and registration are outside repository under /private/tmp.

## Exact source extraction

`/private/tmp/alchemy-slot-admission-generate.py` reads committed LLFace source via git show, locates `LLFace::getGeometryVolume`, and extracts its complete balanced `if (mVertexBuffer.notNull())` guard without modification:

| Guard | Commit | First line | SHA256 of extracted block |
|---|---|---:|---|
| Old aggregate bounds |0c68a66e2189d79ccf221352ecea1fa905d3ca79|1139|c362d9f803c6be714cf77904ce75923d481c557268da6ebe90001f8e04cdf4c6|
| Current local plus aggregate bounds |3ee7786e670aaf4fcecba0db91d30f2cf659e7ec|1107|fa0ae4e9bc5cca577794ac24d1d81c9105abe3649018f8d91a8154128a767d78|

Exact includes `/private/tmp/alchemy-slot-admission-20261113/{old_guard,fixed_guard}.inc`; provenance `/private/tmp/alchemy-slot-admission-20261113/source.json`.

Only private collaborator context surrounds the exact blocks. It uses the actual LLFace field types from llface.h: U16 mGeomIndex/mGeomCount, U32 mIndicesIndex/mIndicesCount/mPoolType; actual LLPointer<LLVertexBuffer>; actual allocated LLVertexBuffer getNumVerts/getNumIndices. Logging/assert collaborators are real. gDebugGL is scoped false and restored; no_debug_assert=true. It does not instantiate LLFace or claim its other method branches are executed.

## Physical layout and bounded writes

Real LLVertexBuffer allocated MAP_VERTEX with12 vertices and18 U16 indices: three adjacent slots, each4 vertices/6 indices. The tested slot is the middle: vertex offset4, index offset6. Vertex stride is actual sizeof(LLVector4a)=16, index stride sizeof(U16)=2. The 4-vertex slot is grounded in LLFace::setSize(...align=true), whose production alignment is `(num_vertices+3)&~3`; a3-vertex mesh is therefore allowed in a4-vertex reserved slot.

Each case resets valid original vertex values and three sets of triangle indices. After admission, a representative fill uses actual LLVertexBuffer getVertexStrider/getIndexStrider and production `alsimd::offset_indices_u16`. Before every write, independent REQUIRE checks guarantee requested spans stay inside the physical buffer. The old guard can overwrite the adjacent logical face slot, but never any address outside the real allocation. No deliberate crash, global allocator manipulation, invalid owned pointer, product hook, or unsafe OOB occurs.

Real Metal device and native backing allocation are initialized, but this control examines CPU shared-buffer stores and never submits a rendered geometry draw. It is an admission/slot integrity control, not a GPU pixel, whole-LLFace, batching, retry-queue, scene, or performance test.

Eight inputs:

1. Exact4 vertices/6 indices: accepted.
2. Padded3 vertices/6 indices: accepted, unused fourth vertex remains untouched.
3. Fewer4 vertices/3 indices: rejected by new exact index-count rule, preventing old reserved tail from remaining drawn.
4. More4 vertices/9 indices: rejected; old whole-buffer check accepts and overwrites next-slot indices.
5. Larger5 vertices/6 indices: rejected; old whole-buffer check accepts and overwrites next-slot vertex.
6. 8 vertices/12 indices ending exactly at aggregate bounds: rejected locally; old accepts and changes both next-slot arrays.
7. 9 vertices/6 indices exceeding aggregate vertex bound: rejected by both.
8. 4 vertices/13 indices exceeding aggregate index bound: rejected by both.

For every expected rejection, the whole CPU vertex/index arrays and content stamp must remain exactly unchanged. Prefix and neighboring slots are checked independently in all cases. Accepted paths verify actual offset indices; padded path verifies untouched reserved padding.

## Build and contained runtime

Private source `/private/tmp/alchemy-slot-admission-20261113/slot_admission.cpp` is compiled using existing scratch CMake flags/libraries with an external-only test registration:

```cmake
al_add_test_binary(slot_admission_tests PROJECT llrender
 SOURCES "/private/tmp/alchemy-slot-admission-20261113/slot_admission.cpp"
 LIBRARIES llrender almetal SUPPORT llrender_test_checks llcommon_test_checks)
```

Commands:

```sh
python3 /private/tmp/alchemy-slot-admission-generate.py
cmake --build /private/tmp/alchemy-geometry-infra-build-20261113 --config Release --target slot_admission_tests -j 6
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/slot_admission_tests '[old-slot-contract]' --reporter compact
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/slot_admission_tests '[fixed-slot-contract]' --reporter compact
```

Build exit0, `/private/tmp/alchemy-slot-admission-build.log` (existing duplicate-library linker warnings only). Both runtime commands used mandatory LLDB containment with MTL_DEBUG_LAYER=1; command file prints actual child exit/state, backtraces and kills if stopped. Both logs report Metal API Validation Enabled. No process stopped or signaled. LLDB wrapper status0 is distinct from actual child status.

| Control | PID | Actual child | Assertions | Log |
|---|---:|---|---|---|
| Exact old guard |57240|42, state10|84 pass/19 fail;1 case fails|/private/tmp/alchemy-slot-admission-red.log|
| Exact current guard |57235|0, state10|**87 assertions,1 case pass**|/private/tmp/alchemy-slot-admission-green.log|

The red failures include admitted changed index count/larger local vertex spans and observed neighbor corruption inside the real aggregate allocation. Green rejection leaves arrays/stamps unchanged; exact and padded cases succeed.

Source SHA256:78f5b6d850a24eca5e7be129f00a4b0267134d5ff74af525344fe10e54dc9fa8.
Executable SHA256:8c50ec53333d27439d811e03bf9df6d0fb61ef5f6fd5e9706740d971fc1fa6dd.

Host is physical ARM64 macOS26.5.2/build25F84, as recorded in earlier avatar/clone evidence. No canonical source/build mutation, push, viewer scene operation or completion/performance claim. This closes only the bounded guard-specific evidence gap requested by parent; whole-method/native scene validation remains separate.
