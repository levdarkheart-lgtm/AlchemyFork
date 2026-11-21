# Production convergence candidate — 2026-11-21

Status: final candidate qualification in progress
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
recorded before each renderer/settings repair. Rejected experimental implementations are retired after dependency/selection tracing; their
historical evidence remains in docs/Git. No FPS gain is claimed from a source repair
or component result.

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

Final integrated native validation is running at c8e72de54a. Repair two adjacent
isolated texture fixtures under PPD555, rerun the CPU selection, then freeze and
build the final distinct Maintenance candidate. Retain the saturation investigation
PPD556 and the explicit residual action map below. The owner controls its live test.

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

## Retired source history

Historical PPD303/304 FrameView/table proof and prior implementations remain
retrievable in Git at83d33894c37087025616bd9aaf1d831cc574da97. For example:
`git show 83d33894c3:indra/almetal/almetalworldsourcepages.mm` and
`git show 83d33894c3:indra/almetal/almetalworldresources.mm`.
These prototypes were removed under545/550; the active ResourceSource immutable
native/registry-generation proofs remain in the maintained resource module.
Historical diagnostics are not current API descriptions or new runtime failures.

The first post-retirement harness run fails1 with four references to the deleted
historical SourcePages implementation in PPD303/304. Point those evidence fields
to this retained source index, preserve all original findings/test history, and
make the current next actions refer to the maintained replacement candidate.


## Final source integration and qualification

All source groups are integrated locally in the canonical checkout. Independent
agents have frozen their implementation worktrees and reviewed the root result.
No push, viewer launch/login, runtime settings modification or live performance
collection was performed. The accepted69417 app remains preserved separately.

| Source checkpoint | Change / evidence boundary |
| --- | --- |
| 648aa4b667 | Bounded async fast-cache reads, pinned cache→fetch completions, generation-checked ready-result delivery and accepted-decode cancellation. |
| a07d000478 | Explicit inherited fetch overrides fix the normal Werror compile failure549. Full Maintenance69438 build02 passes, before large renderer retirement. |
| 24de2b2e99 /6fc608fb9e | Retire old banks/rows/GPU world visibility/resource tables/forced rejected E6 consumers, retaining CPU routing/leases/terrain/probes/shadow/rigged output. Source modules renamed to owned inputs and CPU classifier. |
| 12e0d5d35f | Build03 exposes missing adjacent live shadow reload552; restore exact previous definition. Complete preference generator dependencies553. Build04 passes. |
| c8e72de54a | Restore shared terrain/CPU interval test inclusion and register seven actual Catch fragments554. Empty visibility wrapper no longer creates an empty scope. Full Maintenance69442 build05 passes. |

Private command/output receipts are under
`artifacts/metal/production-convergence-20261121/`. The build uses
`AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-shadow-occlusion` with
`AL_METAL_CONFIG=Release AL_METAL_JOBS=8 tools/metal/build.sh build`; configure uses
`cmake -S indra -B build-Darwin-ninja-os-arm64-shadow-occlusion
-DAL_CHANNEL:STRING="Alchemy Metal Maintenance" -DAL_BUILD_TESTS:BOOL=ON`.
The isolated Python and pinned accepted OpenHT0.27 source/archive are reused;
no decoder fallback is configured.

The final CPU selection is55 entries: worker queue/retry/pool/schedule/lifetime,
image decode/worker and actual OpenHT primary+auxiliary decode, settings roundtrip,
preferences/asset startup/recovery and diagnostic contracts. Initial cpu-tests05
exits8 before execution because --tests-from-file used a relative path interpreted
inside the build directory. The absolute-path replacement cpu-tests06 executes
53PASS/2FAIL; both failures are isolated missing cancellation-state declarations
under555, not normal viewer compilation failures. Preserve all logs and rerun the
complete selection after fixture correction.

The native selection is56 scopes with MTL_DEBUG_LAYER=1 and
MTL_SHADER_VALIDATION=1. It includes default-policy world outputs, scalar/cohort
routing and query ordering, new populated-counter reload551, retained terrain and
CPU arithmetic fragments, ResourceSource generation pins, pending-view lighting
isolation/reuse, complete generic/relevant probe consumers, shadow publication,
producer capture/lifetime and actual offline-library rigged-depth invariance.
The first run exits8 with55PASS/1FAIL/270.28s; the failure is557
(four assertions in two cases with incomplete test capability setup). Correct and
rerun; no Metal validation error or native output mismatch is established.
Cache/archive-specific assertions need a separate run
without global shader validation, which intentionally bypasses the cache.

Static app verification05 passes all346 bundled Mach-O files ARM64-only. Deep
strict codesign verification05 passes. Strict GL source audit04 exits0; this is
a conservative source gate, not runtime/context or foreign Chromium qualification.
The Maintenance69442 executable links Metal directly, with no OpenGL/Vulkan direct
framework dependency. Symbol inspection contains OpenHT decoder bodies and
OpenJPEG header parsing, with no opj_decode symbol; actual decode test identity is
separately exercised. The owner session has not been launched to claim startup-log
or first-live-decode evidence.

## Explicit remaining engineering actions

| Item | Current boundary and next executable action |
| --- | --- |
| PPD556 full schedule | Conditional million-entry self-post capacity hazard, no observed saturation. Foreground housekeeping now uses retryable tryPost, so it cannot wait before unpausing. Remaining request/retry/wake design needs one reusable request-owned deadline entry, a separate bounded callback queue and explicit nonfatal admission handling across callers; preserve4-urgent fairness. Test tiny-capacity retry/wake/admission/paused shutdown, not a larger queue or dropped work. |
| PPD532 handoff fallback / HTTP service | Cache→fetch tryPost falls back to the main list if closed/full/contended. Fast-cache and decoded-result publication remain bounded owner work. HTTP request updates are still frame-triggered/coalesced. Trace completion→fetch→collect→upload→publish age and service triggers before moving more lifecycle work. |
| PPD533 decode cancellation | Pending accepted jobs can skip before allocation/expensive decode and after permit admission; a waiter does not stop waiting immediately, and a running OpenHT call remains nonpreemptible. Consider cancellation-aware permit admission only with exact one-completion/estimate/permit accounting. |
| PPD529 General service | Invalid widths fixed and effective width/selector logged; shipped General1 retained. Measure queue age by job category, then justify separation or width from ownership and workload evidence. Saved-vs-shipped map origin is not available from current CommonControl API. |
| PPD530 geometry / coarse CPU work | Misleading unused budget removed; required priority rebuild remains owner-serial. Attribute rebuild/cull/sort/encode costs and extract immutable preparation/private outputs at a concrete ownership boundary. Preserve required publication correctness. |
| PPD537 earlier GPU waits | Live regular reply budget fixed; pacing drain retained. Correlate queue age against both required-main admission and end-pacing waits. Any added reply service must run outside an active render transaction. |
| Upload/publication/producer credits | Keep memory, age/fairness and oversized progress bounds. Collect denials, backlog age and service capacity before revising per-frame credits; no universal default or FPS attribution established. |
| Single upload owner CPU preparation | Preserve serial flush/batch ownership. Investigate immutable pixel conversion/alpha/mask preparation as an independent stage, with actual ownership/lifetime and total-cost evidence. |
| Pipeline cache concurrency | Predictable point variants now eager. Archive cache mutex still covers native creation; separate archive mutation from compilation only after first-use/startup evidence and safety design. |
| Required decoder naming | Required enabled OpenHT remains selected; OpenHTJ2KExperiment/AL_EXPERIMENTAL_OPENHTJ2K names are historical despite OFF being fatal. Follow-through production naming can migrate build receipts/CI/configuration coherently without decoder fallback. |
| Owner/runtime acceptance | Test loading/teleport/refinement, stale/cancel/reload/shutdown and graphics controls in the separate candidate. Preserve accepted baseline. Whole-viewer performance, prolonged stability, broad parity, runtime no-GL and foreign subprocess/release gates remain open. |

Default-off alone does not identify dead code. Supported quality options and
selected CPU producers, bounded main-lighting chain/shadow-view overlap, probe
candidates, small lighting tiles and active shadow ICB/visibility remain backed
by live consumers or selection evidence. Only rejected/unselected abandoned
mechanisms and their exclusive tests were removed; historical results stay in Git
and the defect register.


PPD555 adjacent fixture repair is frozen: real cancellation token ownership,
endWork normal/abort cancellation, stale callback retention and accepted-completion
reset. Both actual-body ASan/UBSan scripts pass: aux32cases/101checks, asset
diagnostics71off/92on, HTTP6off/7on (fixture-repair-receipt.json).

PPD557 keeps both missing-publication scalar fallback and completed unqualified
fail-visible CPU routing, and551 now uses a fully published cohort for actual
populated-route reload. Production eligibility and conservative output are
unchanged. Independent review pending before final native rerun.

PPD558 is an independently discovered inherited timed-schedule bug: WAITING can
loop past a caller deadline and hold the queue lock in repeated expired waits.
Repair the actual condition/deadline loop and add real deadline/earlier-insertion
checks before the last candidate build; do not build556's future owner wait on
that bug. All earlier logs remain retained.
