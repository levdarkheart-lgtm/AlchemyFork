# PPD-063 native vertex clone tests — 2026-11-13

## Source and ownership

Test-only commits in `/private/tmp/alchemy-geometry-infra-20261113`, branch `codex/geometry-infra-20261113`:

- `12ad48ba428be6d476d5db2bac0e5febad9fd72c`: adds ONLY `indra/llrender/tests/alvertex_clone_metal_test.cpp`.
- `e82b5511083ef0ee589336ac1d4f13a198dd1ec5`: fixes that test's ambiguous LLPointer/raw-pointer conditional construction after compiler diagnosis. Apply after 12ad. Parent/avatar/infra ancestors already integrated separately must not be repeated.

No production source, CMakeLists, canonical build inputs or canonical docs edited by this agent. Root owns PPD-063 repair/registration and PPD-064 base-partition fix. Source worktree clean after commits.

Root froze canonical LLVertexBuffer production files; copies in `/private/tmp/alchemy-clone-root-snapshot-20261113` were hash-verified before use:

- `llvertexbuffer.cpp`: SHA256 `71385033529fac4922616505e00b072f6b7feaa9297feb3fe6a749e4549cd2fc`.
- `llvertexbuffer.h`: SHA256 `04ac8a11fc6a89862f8d9f99923aa522c9302820a58fad83471726af341cc20a`.

The snapshot cpp is an explicit executable object, so its actual production class implementation overrides the old source object in the own-worktree llrender static archive. Snapshot header is preincluded via a tiny external `clone_prelude.h` (linden_common, then llvertexbuffer). PCH disabled for these two executable sources. All other class headers and libraries are from the same own worktree. Only the clone return contract changed; class layout is unchanged. Final isolated meshoptimizer archive and canonical read-only Xcode third-party inputs are the same as the prior avatar receipt.

## Review findings and repair contract

Review identified ignored allocateBuffer(false) in old void clone, same-count layout changes not recalculating offsets, and U32 physical-capacity truncation. Root preregistered PPD-063 before repair.

Root's frozen implementation captures layout change before updating target layout, calls allocation when layout/counts/physical index size differ, allocates physical `mIndicesSize/sizeof(U16)` units (allocateBuffer's actual contract), restores source logical index count, propagates allocation failure, and rejects missing CPU stores for nonzero counts. Matching valid warm layouts avoid allocation. Root separately guards LLFace selection before the buffer swap.

U32 producer evidence: `setIndexData(U32*)` changes stride to4 and halves mNumIndices, retaining the U16-sized original physical allocation. Thus allocate(3,6) becomes logical3 indices occupying12 bytes; naive clone allocate(3,3) would only reserve6 bytes.

LLVertexBuffer is final; no protected test subclass, product hook, forged owned pointers or intentional fault was introduced. Root owns a separate allocation-failure collaborator control. These permanent tests verify actual positive native behavior and safely bound the old layout/capacity failures.

## Four permanent GPU cases

1. Empty source/target with different layouts: clone true; zero counts/sizes; no CPU/GPU stores needed.
2. Matching U16 source/target: target published CPU pointers, native handles and content stamp remain unchanged after clone. Existing target content remains published and draws green through the real indexed LLVertexBuffer draw path.
3. Same vertex/index counts but changed mask: source adds NORMAL, target either has the smaller base layout or equal-byte TANGENT layout. Recompute offsets for position, normal, UV and color; equal-byte case specifically catches the old same-size stale-offset bug. Fill intended attributes, publish and draw.
4. U32 source allocate(3,6), setIndexData(U32[3]) -> logical3, physical12, stride4. Clone to both an empty target and existing U16 target. Assert physical12 BEFORE any U32 write. Publish and read actual native index buffer `{0,1,2}`; issue an actual uint32 indexed draw and check rendered pixels.

All five actual draws (one warm U16, two layout changes, two U32 targets) use a real Apple Metal device, real LLVertexBuffer CPU/native stores and existing NativeFontUI native capture. A full-coverage triangle renders to16x16 RGBA8; all4 channels of every pixel must equal green. No equivalent CPU renderer is used. No viewer login, avatar scene, visual parity or performance claim.

## Build and runtime

External scratch registration only in `/private/tmp/alchemy-geometry-infra-register.cmake`:

```cmake
al_add_test_binary(vertex_clone_tests PROJECT llrender
  SOURCES "${INDRA_SOURCE_DIR}/llrender/tests/alvertex_clone_metal_test.cpp"
          "/private/tmp/alchemy-clone-root-snapshot-20261113/llvertexbuffer.cpp"
  LIBRARIES llrender ll::icu almetal
  SUPPORT llrender_test_checks llcommon_test_checks)
```

Canonical root should add only `tests/alvertex_clone_metal_test.cpp` to existing llrender native test sources, using its repaired real llrender library. No snapshot wiring belongs in product CMake.

```sh
cmake --build /private/tmp/alchemy-geometry-infra-build-20261113 --config Release --target vertex_clone_tests -j 6
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-geometry-infra-build-20261113/Release/vertex_clone_tests '[native-clone]' --reporter compact
python3 /private/tmp/alchemy-clone-old-control.py
lldb --batch --source /private/tmp/alchemy-remaining-hdr-lldb.commands -- /private/tmp/alchemy-clone-old-control '[native-clone]' --reporter compact
```

First compile failed on the test-only conditional between LLPointer and raw pointer (exit1, `/private/tmp/alchemy-clone-build.log`). The dedicated follow-up commit fixes it. Final compile/link exit0, `/private/tmp/alchemy-clone-fixed-build.log`; only existing duplicate-library linker warnings.

Both runtime commands use the mandatory LLDB command file that enables MTL_DEBUG_LAYER=1, runs, prints actual CHILD_EXIT/state, and backtraces/kills a stopped child rather than continuing. LLDB wrapper returns0 for ordinary child42; actual child statuses are authoritative. Neither process stopped or signaled.

| Source | PID | Child status | Assertions/cases | Log |
|---|---:|---:|---|---|
| Exact frozen root production cpp/header |50881|0, state10|**5265 assertions,4 cases pass**|`/private/tmp/alchemy-clone-green.log`|
| Old clone allocation/layout body with bool adapter |51418|42, state10|3176 pass/8 fail;2 cases pass/2 fail|`/private/tmp/alchemy-clone-red.log`|

Both logs explicitly report Metal API Validation Enabled on ARM64 host. The same host was observed macOS26.5.2/build25F84 in avatar evidence.

The old control replaces ONLY the frozen source's clone method with the own-worktree old body. Because new tests require bool, its signature is adapted to bool and appends returntrue; allocation/layout logic is otherwise exact old code. Other production methods stay exact frozen root code. This is explicit contract adaptation, not a claim that old source had bool. The resulting failures: smaller layout96vs144, stale NORMAL/UV/COLOR offsets for both unequal/equal physical byte footprints, and U32 physical6vs12. The U32 REQUIRE halts before writes; no out-of-bounds access or deliberately crashing control.

Old-control reproduction script, source, object and executable are `/private/tmp/alchemy-clone-old-control.py`, `.cpp`, `.o`, and extensionless executable; compile/link command log `/private/tmp/alchemy-clone-old-control-build.log`.

SHA256:

- Fixed executable `/private/tmp/alchemy-geometry-infra-build-20261113/Release/vertex_clone_tests`: `b18e3d2c025099534b09510cd3dcfd0906b0861282499731e6dd162af52c1964`.
- Old-control executable: `ebd2b4361fcaa3a89b26ee326cf95db992dcbb32fb830286103f68d9ab8a64d7`.
- Old-control source: `114ff44183049e2953f21a58206c29186d7eca60e717d17d64b765c36b812500`.

`git diff --check` exit0. Parent will run the registered tests against canonical build as well. Whole-app build/verification and owner runtime approval remain independent gates. No push.
