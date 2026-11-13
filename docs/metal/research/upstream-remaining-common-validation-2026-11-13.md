# Remaining common/data/cache corrections — 2026-11-13

Isolated worktree `/private/tmp/alchemy-upstream-remaining-common-20261113`, branch `codex/upstream-remaining-common-20261113`, base `b04d9bf6ef86d275a747f54618090afbea7354d9`, local commit **1e0090f5a283d40474749911e1bdc19ebe751686**. Clean working tree; no push, viewer operation, canonical docs edit or canonical build-directory mutation. M1 Max arm64/macOS26.5.2. Root owns combined app build/verification and owner testing.

## Scope and source mapping

The original common data producers/consumers remain the native fork's CPU contracts: URI escaping/parsing, LLSD serialization/XML, dates, string splitting, quaternion normalization and default sky construction. These are direct corrections rather than GPU translations. The native renderer and earlier scene/runtime fixes are untouched. PPD-039 intentionally corrects default sun/moon directions; actual appearance still requires owner verification.

Imported complete owned correctness sequences through upstream `408fd6c014`:

- **PPD-030 URI:** `517072b781` malformed escape preservation; `080980a07d` empty query map; `3be4f909db` leading-colon scheme; `584b2cea53` guarded immutable sorted first-call state and concurrent first-call test; `ebf44e6cf9` named existing escape sets and their tests.
- **PPD-031 dates:** `4847b8c56d` portable UTC calendar/fraction/offset logic; final `58a1362221` hundredths roundtrip. Existing calendar rollover and input-field safety preserved.
- **PPD-031 LLSD/XML:** `8000ca483b` empty/failed unzip result; `e1a49a7022` bitwise NaN text; `446d19e518` empty XML keys; `1d410c39b9` whole first-line deserialize; `25ba8e149b` malformed XML; `f878001e68` missing map value; `423f2e1496` empty DOCTYPE; `e651a3e8e2` quoted/internal subset DOCTYPE termination; `2ed88ce337` XML controls; `15fd4bac40` quoted URI notation; test tails `dbaa007f43` and `3292d4e9be`.
- **PPD-031 strings:** `0ec7fe6246` split separator handling; `5d268213f2` all carriage returns removed; `58f471334a` empty replacement target stops rather than looping.
- **PPD-032 quaternion:** complete `6a0a982f5f`, including exact `is_finite(F32)` bitwise helper in additionally assigned llmath.h and both normalization methods/tests. Valid finite magnitude behavior retained.
- **PPD-039 default sky:** `4364fdcdba` azimuth/altitude argument order and direction tests.
- **Remaining PPD-026 persistence:** `29eff897e3` extras written alongside dirty objects/when absent; `4bb9b798e6` no orphan extras when header does not retain the region. Both new disk roundtrip/error cases retained alongside existing first-batch lifetime tests.

All eight common production files and their six corresponding test files are byte-identical to reviewed upstream HEAD after these sequences. Deferred-removal and releaseAll helpers in llvocache.cpp remain byte-identical to base; llviewerregion changes only its disk save flags. Conflict resolution retained both the existing lifetime tests and new disk-pairing tests. No lock/scheduler/units/ALoracle, geometry modernizations or optional new product features imported.

## Scratch build/verification method

Read compile/link metadata and dependency archives from `/Users/dekstroza/Programming/AI/Secondlife/Alchemy/build-Darwin-ninja-os-arm64-followup`, replace source/include paths with this worktree, remove PCH reuse flags, and write every object/executable/log to `/private/tmp/alchemy-remaining-common-probes-20261113/`. No cmake configure/build command mutates existing directories. Exact compiler/linker argv and statuses are in compile/build/test/cache/quaternion JSON receipts in that scratch directory; scripts are `/private/tmp/alchemy-remaining-common-{probes,probes-retry,run,cache-retry,cache-red-retry,quat-release-lto-final}-20261113.py`.

- Seventeen production/test translation units compile with the real Release flags; four scratch binaries link. Source commit remains unchanged throughout final tests. Duplicate static-library linker warnings only.
- `llcommon --reporter compact` — **exit0, 516592 assertions / 304 cases**, six affected test files in one process. Includes malformed XML/DOCTYPE, typed/notation/quoted URI/control roundtrips, date edge cases, string and URI regressions. `llcommon-green.log`.
- `llmath '[#llquaternion_test]~[.]' --reporter compact` — **exit0, 266 assertions / 26 cases**. `llmath-green.log`.
- `llinventory '[#llsettingssky_test]~[.]' --reporter compact` — **exit0, 58 assertions / 11 cases**. `llinventory-green.log`.
- `newview '[#llvocache_test]~[.]' --reporter compact` — **exit0, 3976 assertions / 17 cases**, actual filesystem in the test's unique temporary cache directory, existing deferred-removal/release tests plus new persistence cases. `cache-green-final.log`.
- These four runs total **520892 assertions / 358 cases**; the focused runs below overlap them and are not added.
- Fresh-process exact `LLURI: escape gives every thread the same answer on their first calls at once` — **exit0, 16 assertions / 1 case**, sixteen real simultaneous first calls to the production default escape overload. `uri-first-call-green.log`. No ThreadSanitizer run is claimed.
- Cache old-source control at b04d9bf6ef linked to the same final tests: **exit42, 5 failed assertions across the two new cases; existing 15 cases pass**. Current source passes all17. `cache-red-final.log` / `cache-red-receipt.json`.
- Quaternion real Release arithmetic flags plus `-O3 -flto`: original matching header control **exit42, 10 failed assertions / 1case**; imported source **exit0, 36 assertions / 1case**. `quat-old-release-lto-final.test.log`, `quat-fixed-release-lto-final.test.log`, `quaternion-release-lto-final-receipt.json`. Old header transitive include closure is explicitly pinned in scratch; no new normalization body substitutes into that control.
- `git diff HEAD^ HEAD --check` — exit0. Source preservation check verifies releaseAll and deferred-removal helper ranges byte-for-byte against base. Clean branch at1e0090f5a2.

## Retained setup failures and limits

Initial common test compile omitted upstream test tail dbaa007f43 and failed -Wrange-loop-construct; exact upstream tail fixed it before commit. The initial stripped newview scratch link omitted unrelated support units; retaining the existing full support link set fixed it. A second cache setup picked the production source compile entry without LL_TEST, disabling its cache through undefined application settings and failing18checks; selecting the existing `newview_test_llvocache` entry restores the actual registered test configuration and all3976checks pass. The first old-source cache compile lacked its original implicit source-directory include; an explicit worktree include corrected it.

An exploratory **unsupported** `-ffast-math -flto` mode failed the quaternion case even for imported source. `indra/cmake/00-Common.cmake:234–254` deliberately excludes `-ffast-math`/`-ffinite-math-only`, because those assume nonfinite values cannot occur. This is not a result for the shipped Release arithmetic contract. The actual Release flags plus LTO pass with the baseline red control above. Earlier quaternion control setup forced a header before required math definitions, then allowed the transitive new header into the old control; neither is retained as corrective proof. Final old math-header closure resolves both and produces the expected red result.

No sanitizer run for this batch, full app build/bundle verification, live viewer or owner scene test. Actual LLViewerRegion save invocation is source-integrated and will compile in root's app build; cache tests exercise its underlying real writer/readback contract. Automated checks do not close PPD IDs or approve publishing. Integrator must retain pending owner verification and explicit push approval.
