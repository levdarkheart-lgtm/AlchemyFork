# Remaining general-viewer corrections — 2026-11-13

Scope: PPD041 general-viewer correctness, the independently applicable PPD042 builtin-list lifetime correction, and PPD050 test portability. Isolated worktree `/private/tmp/alchemy-upstream-remaining-viewer-20261113`, branch `codex/upstream-remaining-viewer-20261113`, base `b17a8c5aad`. Apple M1 Max, arm64, macOS 26.5.2. No viewer/account/scene operations, pushes, canonical source/build-directory writes, or broad renderer/refactor changes.

## Local commits

1. `7ebd42843401979f791c2dd335cb76053176e3bb` — inventory/social/navigation/UI/audio and strict XMLRPC/SLURL handling.
2. `1130f257cbb3609b63e774e30f6f9c9e22392245` — Script Studio corrections, websocket coroutine publication lifetime, immutable builtin-list snapshots.
3. `daa888ad3764fdf156219860180c866ec570326a` — coherent object roster, parent, cache identity and crossing/kill guards.
4. `23cea92614f0363ce4a4025d22af98c35b47e954` — PPD050 test-only correction: use active control text metrics and skin margins instead of assuming every one-line notice is exactly 28 pixels. Minimum 28, one line, wrapping, three-line clamp, tooltip and shrink-to-original checks retained. Production notice layout unchanged.

74 source/test/skin paths changed from base; `git diff b17a8c5aad HEAD --check` exits 0 and worktree status clean at `23cea92614`.

## Provenance and disposition

All listed upstream revisions were inspected locally, not inferred from PR titles. This is a selected correctness backport, not an upstream merge.

- #613 category LLSD ID: `da4de3d339`.
- #620 strict XMLRPC number/text handling and finite-value tail: `d6a40a151b`, `83916bb472`, `1334fb9f04`. Existing HTTPS behavior retained.
- SLURL/grid/URI matching and parsing: `7c7c76edb4`, `51eea1b8a5`, `59eea7d4e4`, `8e79f5003b`; adjacent `@` URL handling `7832a4d50f`; LLSD message URI byte size `0c3405f940`; application URL query, unblock, centered teleport and RLVa false-reading fixes `9c1699dd2e`, `d44be85c66`, `39677191ad`, `d8bacaba30`.
- Local teleport timeout/reset `647ad881a7`; opt-in draw-distance stepping `bc5cddb248` (default false). Native viewer display path retained.
- Friends sender/state/completion corrections (#642/#644): `a90ebcecdc`, `2b59bf3b36`, `7390c0a665`, `e48999cfd9`, `5ede75feca`, `e016b24ff6`, `e0dc19d16a`.
- IM/inventory correctness (#644/#645): `5da4e91940`, `58bff905ec`, `79691e43d0`, `f10b26d637`, `201e3da57a`, `9c975594de`, `6a930c095d`, `cb2068bfd1`, `47b2379dc1`. `9063` login-ready behavior already existed and was preserved rather than reapplied.
- Notecard/group/library (#646/#647): `78e83828ec`, `fd9f32b578`, `e7e6a97214`.
- Teleport progress/maturity (#650): `908cb97f3e`, `0d6989ee58`.
- Always Run notices, mirror/other graphics preset keys, and right-button camera steering (#655/#660/#665/#667): `e91d1befc4`, `dc03d93fd3`, `c7b5f32d4b`, `63c5ae3e63`.
- #599 FAudio spatial correctness `7ad29afafb`: 2048m inverse attenuation curve and filter/reverb setup; no claim of an audible test.
- #593 complete applicable Script Studio family (`046b5ea783` merged changes): converter NULL_KEY/list-to-key defaults, Luau event hover parameters, explorer master prims, compiled/source comparisons, notices, UpdateFile handling and skin strings. Existing tests and actual changed application sources compile.
- #606 `710e57aef5` coroutine lifetime family: standard mutex for async websocket handlers, one weak lock, consuming override and release of last server reference on main with diagnostics. PPD042 only the independently necessary immutable `builtinNames` shared snapshot and `LLCorosMutex` hunk from `35fc15e182` in `allslservice.cpp`; no public API change or broad lock sweep.
- #635 object/parent/cache identity family: `8c556a4fbf`, `5e44788d2e`, `68c995d9bb`, `8ddc5190db`, `fe96b4924c`, `dca7ab3b19`, `d37cb2f32d`. Added ALObjectUpdate and ALObjectOrphans helpers, server type/local-ID/UUID/cache-entry/kill/loop/crossing/handshake checks with all matching callsites and tests. No later prerequisite tail found through audited upstream `408fd6c014`. Wind/protocol pieces already existed from First batch.

Explicit omissions: optional F-key/MU features; development capture/oracle; Windows-only process publication and platform behavior; broad units/locks/scheduler/retirement changes. Old lifetime/dead-object counter behavior retained outside the selected roster correction. Existing commented-out orphan diagnostic block remains commented out. #633 face/TE retention and #640 unknown-region override behavior already existed in base via `863c6bf1df` and remain present; an initial verbal absence claim was incorrect and was corrected before evidence publication.

Preservation source checks: `llvovolume.cpp`, `llvovolume.h`, `llviewerobject.h`, `llworld.cpp` remain byte-identical to base. In `llviewerobject.cpp`, the suffix starting with `bool LLViewerObject::isAnimatedObject() const` remains byte-identical to base, including later PBR/material/boost methods. Existing `albakeproxies` integration retained. No excluded native renderer paths edited. First-batch parser bounds, cache-removal lifetime and bounded message/TLS/startup guards retained.

## Required build registration (root owns all CMake)

Root has already integrated registration in canonical `7de38c3a00e50508b3f4568c91f55fe6dcd551b4`; this worktree does not modify CMake.

- llprimitive production PRIVATE `alobjectupdate.cpp`, HEADERS `alobjectupdate.h`, `alobjectorphans.h`; primitive tests SOURCES `tests/alobjectupdate_test.cpp`, `tests/alobjectorphans_test.cpp`.
- llinventory tests SOURCES `tests/llnotecard_test.cpp`.
- viewer HEADERS `almaturitypreference.h`, `alteleportprogress.h`; newview tests SOURCES `tests/almaturitypreference_test.cpp`, `tests/alteleportprogress_test.cpp`.
- Existing alapplinks source/test and all modified test units retain their existing registrations. Standalone SLURL test must retain its own fake/stub link boundary, rather than joining main newview tests.

## Validation

Scratch scripts and all command argument arrays/exit statuses are stored outside the worktree:

- `/private/tmp/alchemy-remaining-viewer-probes-20261113.py`: reads compile metadata from canonical `build-Darwin-ninja-os-arm64-followup`, substitutes this worktree source/includes, removes precompiled-header coupling and emits only scratch objects. `compile-receipt.json`: all **58** production/test compile variants exit 0 (57 changed C++ files plus LL_TEST cache variant). This includes real agent/display/region/object/list/websocket/editor/FAudio callsites, not merely helper tests.
- `/private/tmp/alchemy-remaining-viewer-link-20261113.py`: scratch links; 8 suite executables link successfully against canonical Release dependencies read-only. Corrected newview link also includes the changed noticebar object and current unmodified datapacker implementation; `newview-matching-link-receipt.json` records the final matching link.
- `/private/tmp/alchemy-remaining-viewer-run-20261113.py`: initial helper suites, isolated scratch working directories; default hidden tests excluded. `test-receipt.json` retains the initial failed mixed-version newview attempt rather than overwriting it with a green summary.
- `/private/tmp/alchemy-remaining-viewer-lldb-20261113.py`: all retests after the initial fault use `process launch --shell-expand-args false`, inspect child status, capture all thread stacks and explicitly kill if stopped on a fault (`-k` crash commands). A fault is never continued. `newview-ui.lldb.log`, `newview-cache.lldb.log` record final matching child exit 0.

Final passing default coverage (disjoint selected units): **7,950 assertions in 321 cases**:

| Suite | Assertions | Cases | Result |
|---|---:|---:|---|
| inventory/category/notecard | 559 | 32 | exit 0 |
| XML node/XMLRPC number parsing | 191 | 16 | exit 0 |
| message reader URI | 33 | 22 | exit 0 |
| object-update/orphan helpers | 318 | 18 | exit 0 |
| URL registry | 16 | 3 | exit 0 |
| LSL converter/Luau service, default solver | 2,210 | 165 | exit 0 |
| standalone SLURL | 107 | 8 | exit 0 |
| app links/maturity/progress/Script Studio masters/orphans | 518 | 38 | LLDB child exit 0 |
| cache identity/lifetime/disk tests | 3,998 | 19 | LLDB child exit 0 |

Additional Luau new-solver rerun: `AL_TEST_LUAU_SOLVER=new`, 2,205 assertions/165 cases, exit 0; overlaps default solver cases and is not added to the total.

### Failures retained and explained

1. Initial scratch newview omitted updated `alscriptnoticebar.cpp` while using its new header/test; old support implementation produced zero line counts. Matching implementation removes those failures. One newly imported exact-height assertion then failed 30 vs 28 under active Mac fonts, while wrapping/clamping/shrinking worked. PPD050 test-only commit corrects the expectation using actual control metrics; final matching UI subset passes 518/38.
2. Initial scratch cache read jumped into libc++abi type-info vtable: `llvocache_test.cpp:793`, `read_back->unpackUUID`. Original OS report `/Users/dekstroza/Library/Logs/DiagnosticReports/newview-2026-11-13-095808.ips` (09:57:51 UTC, SIGBUS). Saved stale-datapacker executable/control reproduced under LLDB; full stack in `newview-cache-stale-datapacker-control.lldb.log`, explicitly killed. Root static llmessage archive predates the First-batch datapacker virtual API; injecting current unmodified `lldatapacker.cpp` into otherwise matching newview link removes the fault and gives 3998/19 exit 0. This is demonstrated scratch dependency mismatch, not evidence of a viewer production regression. No production workaround was added.
3. Linker warnings only: duplicate static libraries bz2/freetype/harfbuzz/png/z, ignored. No compiler errors.

## Independent PPD045 verification requested by root

Canonical newview tests omit production `altextureboost.cpp` UNIT, while viewer production retains it. `/private/tmp/alchemy-ppd045-no-boost-link-20261113.py` compiles canonical `altextureboost_test.cpp` with real Debug defines and explicit `-O0 -fno-lto`, then links without any dead-strip option, boost implementation object or newview implementation library. `nm -u` finds no LLViewerTexture/addBoostHold/removeBoostHold/ALTextureBoost RAII dependency. LLDB launch passes **1088 assertions/6 cases**, child exit 0. `ppd045-receipt.json`, `ppd045-lldb-receipt.json`, `ppd045.*.log` capture full commands and results. This separate check is not added to general-viewer totals.

## Limits and next action

Root owns combined app build, bundle verification and canonical documentation/register integration. Scratch suites prove selected source contracts and linkability, not authenticated UI/server behavior, websocket shutdown in a real session, live crossings, social/inventory network behavior, audible FAudio parity, or performance. No TSan claim for immutable snapshots. All changes remain local pending owner testing and explicit push approval. Meaningful next acceptance is root's complete app build/tests followed by owner-controlled real viewer scenarios.

## Follow-up PPD051: whole-asset HTTP class

Root registered PPD051 before implementation and explicitly assigned this follow-up. Commit **`13118aa2bed827b63c9c37460755d322f323ef80`** imports upstream **`05c6ab903212a46e7fe4e7f94e76ca4131ac6b8b`** in exactly four CPU/settings paths: `llappcorehttp.h`, `llappcorehttp.cpp`, `llviewerassetstorage.cpp`, `app_settings/settings_alchemy.xml`. It restores AP_ASSET after AP_TEXTURE, its own pipelined policy row (default 8, clamp 1–16), and persistent `AssetFetchConcurrency` U32 default 0 meaning default concurrency. The coroutine route intentionally adapts upstream: it obtains the actual created policy identifier through `LLAppViewer::instance()->getAppCoreHttp().getPolicy(LLAppCoreHttp::AP_ASSET)`, with explicit `llappviewer.h`, rather than treating the enum index as an HTTP policy ID. No Metal dependency, CMake change or broad queue/scheduler change. Texture and mesh policy routes remain untouched. AP_COUNT becomes 11, below library policy-class limit 32; AssetStorage coroutine pool remains 12.

Validation:

- Both actual production translation units compile with the same read-only canonical Release dependencies, output only scratch. `ppd051-compile-receipt.json`: both exit 0.
- `/private/tmp/alchemy-ppd051-policy-probe-20261113.py`: structural checks of the actual source/header enum and table order; removing the new asset row yields byte-identical old table source; actual asset route uses `getPolicy`, texture remains AP_TEXTURE, meshes remain AP_MESH2/AP_LARGE_MESH; XML parses with unique setting keys, default/type/persistence verified; existing AssetStorage pool12 verified.
- Scratch C++ probe includes the actual policy header, extracted actual source table and actual `refreshSettings` setting-selection block. It checks zero/default, missing setting, lower/upper bounds and overflow input. It creates a real LLCore HTTP service without starting a thread or issuing any requests, reserves an earlier policy, allocates all viewer classes, verifies distinct IDs, proves asset enum differs from its actual created ID, and verifies independent actual asset/texture connection and per-host options. Service destroyed normally. **37 assertions/2 cases pass, LLDB observed child exit 0.** Full source/compile/link/contained-launch receipts in `ppd051-policy*`.
- Initial standalone probe compile lacked a precompiled-header-provided `boost::signals2::connection` declaration; adding the explicit prerequisite include to the scratch probe resolves it. Log retained as `ppd051-policy.compile.initial.log`. Production sources compiled successfully before this probe setup correction; no production header workaround added.
- No live fetch throughput/latency claim. Owner acceptance still needs real mixed asset/texture fetch behavior; root owns full application build and bundle staging.

Additional cache mismatch evidence: `nm -C` on the saved stale control has only the old pointer-based `LLDataPackerBinaryBuffer::unpackBinaryData` overload. The matching executable additionally contains the First-batch `boost::container::small_vector_base` virtual overload. Symbol logs saved as `newview-stale-datapacker-control.datapacker-symbols.log` and `newview.datapacker-symbols.log`, consistent with the demonstrated vtable ABI mismatch and green matching-source test.

Final worktree HEAD `13118aa2bed827b63c9c37460755d322f323ef80`; clean status and full base-to-HEAD `git diff --check` exit 0. General-viewer default totals above and separate PPD045/PPD051 counts remain separate. All five local commits are ready for root integration; no push.
