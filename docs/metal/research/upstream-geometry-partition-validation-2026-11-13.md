# PPD-065 base partition allocation retry control — 2026-11-13

Parent registered inherited base-partition allocation(false) loss before production repair. This subtask made no product, repository CMakeLists, canonical build or documentation changes. Private files only under /private/tmp; own worktree remains clean at e82b5511083ef0ee589336ac1d4f13a198dd1ec5.

## Exact functions

The generator `/private/tmp/alchemy-base-partition-generate.py` reads complete balanced `LLSpatialPartition::rebuildGeom(LLSpatialGroup*)` bodies from committed source using git show. No statements of the baseline or final methods are rewritten in the exercised includes.

- Baseline3ee7786e670aaf4fcecba0db91d30f2cf659e7ec, `indra/newview/llspatialpartition.cpp:358`: `/private/tmp/alchemy-base-partition-20261113/old_function.inc`, SHA25601cdde713ac0fc6d8de139af969d00d0b04ba1dec64169d9096bcd8fa80d8f3f.
- Parent's final production commit **b16fa27efab2c88a60793bcc938e5876d4fa9a9f**, same method: `/private/tmp/alchemy-base-partition-20261113/final_function.inc`, SHA256fa7508a677d66cbee638f145bb2e44a8ee0fe3583f816d63228f0d3e59385708.
- Final method differs only by `group->mBuilt=0.f; return;` after failure's buffer/map withdrawal, with parent's inline comment. It is compared against the proposed two-line adaptation before testing. The standalone proposed include (without inline comment) has SHA256bb7d0e90ca2dc9b50dbc795eae42cf637f76b8aabbce469bb98aaf1fbe9ab7f5.

Metadata `/private/tmp/alchemy-base-partition-20261113/source.json`. Generation is idempotent and reproduces the final source's exact block/hash. Baseline already contains PPD064's correct getNumIndices reuse comparison; this control does not modify it.

## Collaborators and limits

These exact function definitions compile in separate private namespaces against small buffer/group/partition input collaborators. The deterministic allocation collaborator returns false once, then true, and records requested counts like actual LLVertexBuffer; no allocation failure or OOM is caused. Buffer objects use actual LLRefCount and LLPointer, with destruction counters. The group models actual state bits DEAD0x8 and GEOM_DIRTY0x10; F32 built/distance, actual LLVector4a view-angle fields, F64 last-update time; vector-owned buffer refs and draw entries expose the clear/withdraw behavior. Counting/fill collaborators record whether getGeometry was reached. Actual logging/profile machinery compiles unchanged. Frame time input is fixed42.

This proves the exact base function's admission/withdrawal/retry control flow with explicit collaborators. It does not run the entire production LLSpatialGroup/octree or actual LLVertexBuffer allocator, priority queue, grass/terrain generation, native resources, GPU render or scene. There are no product hooks, fake owned addresses, real OOM, signals, GPU/performance claims or whole-method environment equivalence claims beyond the extracted function.

## Assertions

Failure case starts with an old4-vertex/6-index buffer held by both group and buffer map, requests8/12, then injects allocation(false).

- Failed new object and prior mapped buffer both release through real LLPointer ownership (two destruction increments).
- Group vertex buffer null, buffer map empty, draw map cleared, zero getGeometry calls.
- GEOM_DIRTY retained, mBuilt0, last-completed update time unchanged17.
- LOD distance/view-angle updates remain intact.
- A caller retries only if GEOM_DIRTY remains set (the prerequisite consumed by the separate priority queue). On retry allocation succeeds8/12, getGeometry runs exactly once, mBuilt1, completed time42, dirty clears and a draw entry is produced.

Ordinary controls run for BOTH baseline and final function:

- Existing8-vertex/12-index buffer is reused with identical pointer and no new allocation/construction; getGeometry runs once, mBuilt1, dirty clears, time42.
- Calling an unchanged clean group again does no count/fill/clear work.
- Empty geometry removes the buffer and map/draw entries, releases prior owned buffer, performs no allocation/fill, and clears dirty normally.
- Dead dirty group does no counting/cleanup/fill.

## Build and LLDB results

Private external-only registration (not product CMake):

```cmake
al_add_test_binary(base_partition_tests PROJECT llrender
 SOURCES "/private/tmp/alchemy-base-partition-20261113/base_partition.cpp"
 LIBRARIES llmath llcommon)
```

Commands:

```sh
python3 /private/tmp/alchemy-base-partition-generate.py
cmake --build /private/tmp/alchemy-geometry-infra-build-20261113 --config Release --target base_partition_tests -j 6
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/base_partition_tests '[base-partition-old]' --reporter compact
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/base_partition_tests '[base-partition-fixed],[base-partition-ordinary]' --reporter compact
```

Final compile/link exit0, log `/private/tmp/alchemy-base-partition-verified-build.log`.

All actual test executions use mandatory LLDB containment. The command file enables MTL_DEBUG_LAYER=1, runs, prints actual child exit/state, and would backtrace/kill a stopped process. Neither final process stopped or signaled. LLDB wrapper status0 is not used as child success evidence.

| Exact method | PID | Child exit/state | Result | Log |
|---|---:|---|---|---|
| Baseline3ee failure contract |66154|42/state10|12 assertions pass,7 fail,1 case fails|/private/tmp/alchemy-base-partition-red.log|
| Finalb16 failure/retry plus BOTH ordinary versions |66156|0/state10|**67 assertions in2 cases pass**|/private/tmp/alchemy-base-partition-green.log|

Baseline failures show GEOM_DIRTY incorrectly cleared, mBuilt incorrectly1, update timestamp treated as completed, and consequently no retry/fill/new buffer. Withdrawal/no-fill itself already succeeds on baseline and remains correct after final repair.

Private fixture preparation had a compile-only missing SIMD umbrella include when upgrading view-angle fields from scalar collaborators to actual LLVector4a; corrected include to llsimdmath.h. An earlier scalar-view-angle binary was run under LLDB during that iteration (PID62634 child42, PID62632 child0), but these preliminary logs are explicitly retained as `alchemy-base-partition-{red,green}-preliminary.log` and excluded from final evidence. The final receipt uses exact committed final function and actual LLVector4a fields after successful rebuild. No crash occurred.

Final private source SHA2560e899d10f7f7836fcf342597a9ab3d95ca21d8153ac833ac3348d50775a0651a.
Final executable SHA2562c54826387e757645439ac2ea29e003b720c62d9cd3a7f95a2145f9b30c5bacc.

Generator validation rerun reproduced source/hash identically. No further source changes or commits needed for this subtask. Parent owns final canonical incremental build/app verification and register/state integration. No push.
