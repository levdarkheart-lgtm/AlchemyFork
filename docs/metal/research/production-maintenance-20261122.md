# Production maintenance consolidation —2026-11-22

The owner requested the production fixes and contradiction reconciliation on
`codex/apple-silicon-metal`, with experiments, one-off tools and new instrumentation
excluded. This is a selective integration from production base `d191bb062f`, not a
merge of the review branch. Home FPS remains unresolved; no rendering optimization
or product/release completion is claimed.

## Retained fixes

| ID | Maintained behavior |
| --- | --- |
| PPD582 | Existing transparency suite tests real above-water fragment slot3 and restoration to alpha slot1 across pending immutable packs. Corrects the old claim that simple-alpha used slot3. |
| PPD585 | Camera API description states request-time/non-atomic values, radians and possibly infinite projection far clip; behavior unchanged. |
| PPD586 | Octree diagnostic uses live child count, rejects a ninth child before mutation and compiles with its diagnostic macro enabled. |
| PPD587 | Reply publication wakes the existing texture-fetch owner independently of the frame loop. Turns are bounded by16callbacks/1000µs with at least one progress item, and unfinished/exceptional work re-arms the owner. Notification clearing synchronizes with teardown. |
| PPD588 | Nonthreaded fetch executes housekeeping once through the existing virtual owner dispatch. |
| PPD589 | Response callbacks retain exception-safe ownership; request identity/cancellation and FIFO behavior remain. |
| PPD590 | Existing harness excludes external evidence URLs from local path/command checks while preserving local labels, adjacent links and executable validation. |
| PPD584 | Current register/task fields reflect source and accepted history. Original IDs/failures/history remain; completed accepted removals are retired from active implementation lists. |

## Excluded scope

The immutable [review checkpoint](https://github.com/levdarkheart-lgtm/AlchemyFork/tree/a6089d691c113b9e6bc66a09f6b65b020e70960f)
retains all research evidence and failed attempts. Its newly added native frame,
command, input, presentation, memory and admission diagnostics, extended quiet
capture limits, HTTP age histogram/timestamps, metric manifest and Python analysis
modules/fixtures are not included in production. Existing supported diagnostics are
unchanged. No new tuning preference, shader variant, worker width, queue-depth or
adoption-order experiment is introduced. Research-only scripts are not future tasks
on the production queue.

The [audit](register-truth-audit-20261122.md) preserves original reconciliation counts
and individual evidence gaps. The [full review report](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/a6089d691c113b9e6bc66a09f6b65b020e70960f/docs/metal/research/review-home-admission-20261122.md)
is evidence, not the current production feature map. The actual HTTP comparisons
show lower publication→handler age under unequal live populations; they do not
establish whole-loading/FPS improvement. Increasing the main-group limit worsened
tails and shifted blocking; the normal policy stays unchanged.

## Validation

Combined harness passes33required files; full maintained tool suite passes188tests
with1optional skip. Its existing requirements include NumPy and PyYAML. The first
focused invocation preceded audit-document integration and correctly rejected the
missing note; that failed log is retained. Three new existing-suite cases qualify
the external-evidence fix; no new tool/module is introduced. Independent source
review confirms17intended C++/Objective-C++ paths, baseline-identical instrumentation
paths, and bounded HTTP wake/re-arm/teardown/response ownership. Clean-scope build
and native checks now pass as listed below. The normal and separately macro-enabled
octree tests are distinct; ordinary macro-off math tests alone do not exercise
diagnostic overflow rejection. Prior ASan/UBSan8331assertions/8cases apply to the
identical retained header/test source; no new macro-enabled run is claimed.

| Check | Current result | Private receipt |
| --- | --- | --- |
| Doctor |0 | `doctor.log` |
| Full Release build |0 | `build.log` |
| Focused native CTest |6/6pass,107.28s | `native-tests.log` |
| ARM64/signatures |0;346components,0nonARM64 | `verify-app.log` |
| Strict owned-GL source gate |0 | `strict-gl.log` |
| Tool suite |188tests/1optional skip,0 | `tools-tests.log` |
| Documentation harness |33required files,0 | `harness.log` |
| Independent source/harness reviews |No material issue | `source-freeze.json` |
| History preservation |All589prior histories retained;590records | `history-preservation.json` |

Private evidence root: `artifacts/metal/production-maintenance-20261122`.
The clean executable SHA256 is `8f8f59babd642d40f8797184cd2f34212041fc5d347575c053a294fb2b1fe99f`;
UUID `7C472792-D5A5-388D-8071-C9FE2B0914EE`. The app was not launched.
Source hashes and exact app identity are in `source-freeze.json`/`app-identity.json`.

Commands (all exit0 after complete integration):

```sh
tools/metal/doctor.sh
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion AL_METAL_CONFIG=Release AL_METAL_JOBS=8 tools/metal/build.sh build
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 ctest --test-dir build-Darwin-ninja-os-arm64-shadow-occlusion -C Release --output-on-failure -R 'almetal_tests/almetal_probe_candidate_transparency_test$|llcommon_tests/llqueuedthread_test$|llcorehttp_tests/(httpoperation_test|httprequest_test|httprequestqueue_test)$|llmath_tests/lloctree_test$'
tools/metal/verify_app.sh 'build-Darwin-ninja-os-arm64-shadow-occlusion/newview/Release/Alchemy Metal Scheduling.app'
python3 tools/metal/audit_gl.py --mode strict
PYTHONPATH=/Users/dekstroza/.venvs/alchemy-qa/lib/python3.14/site-packages python3 -m unittest discover -s tools/metal/tests -v
python3 tools/metal/validate_harness.py
```

## Runtime and authorization

The previous instrumented review candidate was closed normally at owner request;
PID98258 is absent. Keep the viewer closed. No owner-driven test is required for
this requested publication. Owner visual/performance acceptance and broader release
gates remain separate. Raw captures, account data and private worktrees are excluded
from the commit and push.
