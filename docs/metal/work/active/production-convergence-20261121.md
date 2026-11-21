# Production convergence candidate — 2026-11-21

Status: doing
Owner: root integrator, canonical codex/apple-silicon-metal worktree.
Base: 83d33894c37087025616bd9aaf1d831cc574da97 / indra05091a15a7.
Owner authorizes proper scheduling/ownership repairs, coherent Apple-first infrastructure,
graphics preference cleanup, and a separate candidate viewer for manual testing.
All proposed fixes stay local; no push/release or automatic login authorized.

## Ownership and acceptance

| Work | Owner / exclusive paths | Acceptance | State |
| --- | --- | --- | --- |
| PPD528/532/533 cache priority, asset handoffs, ready decode and cancellation | asset agent: texturecache/fetch/viewertexture/list, llimageworker and dedicated tests | stale/cancel/shutdown/fairness tests, real decoder identity, full build | doing |
| PPD529/531 pool validation and nonblocking retry infrastructure | scheduler agent: llcommon threadpool/queuedthread/workqueue/schedule and dedicated tests | invalid widths, timed retries, wake/abort/delete/generation, idle update accounting | doing |
| PPD527/536 graphics preferences | preferences agent: settings/UI/feature tables, llfloaterpreference and dedicated tests; no pipeline writes | consumer census, preset parity, no dead controls or misleading GL help | doing |
| PPD530/534/535/537/538/539 geometry contract, resources, lighting, replies and LFS integration | root: shared renderer files, llappviewer, canonical CMake/docs | actual resource lifetime/GPU tests, queue integration, complete Release candidate | doing |
| Candidate | root alone owns configure/build/test/staging | doctor, Release build, verify_app, strict GL, relevant tests, immutable source/app hashes | todo |

## Preserved contracts

OpenHTJ2K remains the sole pixel decoder. Upload keeps its one batch/flush owner.
Scene mutation and result publication stay on explicit owners; memory/retirement,
request generations, cancellation, quality and retained Alchemy features remain.
Scheduling changes use existing queues and ownership boundaries, without a new
engine-wide framework or speculative user tuning knobs. Original mappings are
recorded before each renderer/settings repair. Rejected small-job experiments stay
retained; no FPS gain is claimed from a source repair or component result.

## Evidence and integration

Initial working changes are the preceding audit's documentation, owned by root.
Existing untracked native-im worktree is untouched. Independent code changes use
separate worktrees and local commits, integrated serially; root owns shared
pipeline.cpp, almetalworld.cpp, llappviewer.cpp and canonical documentation.
Exact commands, failures, source identities and test outcomes are appended here.

## Remaining work must survive handoff

The audit action map remains [source assessment](../../research/scheduling-source-assessment-20261121.md).
Every item will end with implemented evidence or an explicit residual contract,
reason and next executable step. Large scene preparation/parallel encoding,
capacity tuning, deep geometry ownership changes and pipeline compilation lock
splitting require demonstrated need and appropriate correctness/cost qualification;
they must not disappear merely because a candidate builds. Existing PPD319 GPU
completion/performance, parity and product release gates remain open.

## Next executable action

Implement independent worker/asset/preferences repairs, review and integrate them,
then build and qualify one separate candidate. The owner controls its live test.

## Root pre-fix ownership mapping

PPD530 uses original upstream408fd6c priority rebuild semantics: full required
priority traversal plus moved bridge updates, distinct from time-sliced object
creation. Remove the misleading budget interface, name the required phase
explicitly, and keep future immutable geometry decomposition as residual work.
Do not insert unsafe timer breaks in live scene mutation.

PPD534 is a new native resource registry: exact allocation identity/generation,
per-command access escalation, submission and completion retirement remain
unchanged. Construct the owning map value only on insertion; existing actual
resource-plan tests cover repeated hits, generations and every terminal path.

PPD535 uses original per-view lighting constants/list contents and bindings.
Native ALMetalWorldFrameData pages already retain command completion tokens and
reuse only after all readers complete. Publish immutable lighting slices on the
exact command after validation/acquisition; preserve offsets and independent
view contents. No shader math, material, pass order or depth policy changes.
Standalone renderer callers without externally supplied frame data need a real
owned arena too, rather than an inline-byte fallback for large probe constants.

PPD539 mirrors existing eager main-projector PSO construction: build the finite
HDR/LDR depth-bearing point-light descriptors alongside their ordinary variants,
with the same library generation and error handling. Archive locking remains
owned and serialized; no speculative compilation framework.

PPD537 preserves the original main-owned complete callback model and existing
safe pacing reply consumer. Use a live cached setting with finite bounded
conversion. PPD538 removes foreground queue-count sleeps once worker service
progress/no-op coalescing is integrated; actual LFS operations remain intact.

## Owner maintenance extension: retire abandoned experiments

Owner additionally requests removal of dead, disabled experiments from source and
exclusive tests where coherent. PPD543 retires the rejected main-light overlap,
retaining normal point values and active lighting chain. PPD544 retires the
unselected world-preparation scheduler/prepared route while preserving immutable
owned scalar packets. Inventory other controls by actual consumers and selection
history; default-off quality features and active probe candidates are not deleted
by name/default alone. Root handles all Git writes centrally because child agents
retain the prior read-only .git sandbox; do not leave approval prompts waiting.

## Integrated progress before experiment cleanup

Root native component build: cmake --build build-Darwin-ninja-os-arm64-shadow-occlusion
--config Release --parallel6 --target almetal_tests, exit0. Nine focused CTests under
MTL_DEBUG_LAYER=1/MTL_SHADER_VALIDATION=1 pass176.92s. Logs native-build-01.log and
native-tests-01.log in private production evidence root. These precede later
cleanup and cannot qualify final candidate source by themselves.

Root core5d7dc12078; saved-profile retirementb9304951d8; scheduler source2c9628e07a
integrated60932652bf; preferences source96d937b816 integrated10dc03df11; rejected
main-overlap removal9bb0f4e289; owned-scalar caller cleanup5ce8f94aba. All local.
Root handles Git centrally after child .git index writes were denied; no owner
permission is needed in root's current unrestricted session.

The large stale recovery/active notes were archived with relocated links, replaced
by a concise truthful current snapshot. Prior release/acceptance/failed evidence
is retained; current build/cleanup work is no longer mixed with old completed tasks.

PPD547 retains the first compact recovery rewrite harness failure (six invariant
coverage messages); explicit current gate wording is being restored without
moving history back into live snapshots or weakening the gate.
