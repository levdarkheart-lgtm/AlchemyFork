# Production convergence candidate — 2026-11-21

Status: owner-verified candidate; publication approved
Owner: root integrator, canonical codex/apple-silicon-metal worktree.
Base: 83d33894c37087025616bd9aaf1d831cc574da97 / indra05091a15a7.
Owner authorizes proper scheduling/ownership repairs, coherent Apple-first infrastructure,
graphics preference cleanup, and a separate candidate viewer for manual testing.
Publication was initially held for owner testing; the owner now explicitly approves
commit/push of Maintenance69445. No release or automatic login is authorized.

## Ownership and acceptance

| Work | Owner / exclusive paths | Acceptance | State |
| --- | --- | --- | --- |
| PPD528/532/533 cache priority, asset handoffs, ready decode and cancellation | asset agent: texturecache/fetch/viewertexture/list, llimageworker and dedicated tests | stale/cancel/shutdown/fairness tests, real decoder identity, full build | ready_for_owner |
| PPD529/531 pool validation and nonblocking retry infrastructure | scheduler agent: llcommon threadpool/queuedthread/workqueue/schedule and dedicated tests | invalid widths, timed retries, wake/abort/delete/generation, idle update accounting | ready_for_owner |
| PPD527/536 graphics preferences | preferences agent: settings/UI/feature tables, llfloaterpreference and dedicated tests; no pipeline writes | consumer census, preset parity, no dead controls or misleading GL help | ready_for_owner |
| PPD530/534/535/537/538/539 geometry contract, resources, lighting, replies and LFS integration | root: shared renderer files, llappviewer, canonical CMake/docs | actual resource lifetime/GPU tests, queue integration, complete Release candidate | ready_for_owner |
| Candidate | root alone owns configure/build/test/staging | doctor, Release build, verify_app, strict GL, relevant tests, immutable source/app hashes | ready_for_owner |

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

Record the owner's verification; normal publication of the approved Maintenance69445
scope uses origin/codex/apple-silicon-metal with no tags/release. Display the
[ordered outstanding work](metal-port.md#outstanding-work-in-priority-order).
PPD556 remains the first correctness implementation; other residuals below retain
their architecture/policy/investigation boundaries. All existing source work is frozen.

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


## Earlier source integration and qualification checkpoints

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

This table records the boundary at accepted69445. Subsequent PPD-556 implementation
and Scheduling69450 evidence are retained in the [new execution ledger](ppd556-bounded-scheduling-20261121.md).
Use the [canonical active priorities](metal-port.md#outstanding-work-in-priority-order)
for current status; the historical556row below is not a claim that its repair is
still unimplemented. Geometry scheduling and earlier GPU-wait accounting remain
open beyond the narrower accepted interface/setting fixes.

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
| Broader runtime qualification | Owner accepted Maintenance69445 and approved publication. Separate whole-viewer performance, prolonged stability, exhaustive parity, runtime no-GL and foreign subprocess/release gates remain open. Preserve accepted baselines and exact source/active-setting evidence. |

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

The pre-final PPD557 checkpoint keeps both missing-publication scalar fallback and completed unqualified
fail-visible CPU routing, and551 now uses a fully published cohort for actual
populated-route reload. Production eligibility and conservative output are
unchanged. Independent review subsequently passed at33b1b6a308; final native
execution is queued after build06.

PPD558 is an independently discovered inherited timed-schedule bug: WAITING can
loop past a caller deadline and hold the queue lock in repeated expired waits.
The actual condition/deadline repair and real checks subsequently passed in
2bba3d1e6b, before build06. Do not build556's future owner wait on the old bug.
All earlier logs remain retained.


## Final code freeze

Source `a2aaaf29421dd49821606457923f246f73f77318`,
indra tree `6e3b13c1f4076c5244bfa708ce564d8e03cd6cdd`. All agent source ownership is
frozen. Root2bba3d1e6b repairs558 plus retryable housekeeping admission under556;
33b1b6a308 completes555/557 actual fixtures; a2aaaf2942 retires559's unused decoder
selection/alternate pixel path and makes notices unconditional. OpenHT accepted
pixel body is unchanged apart from its production diagnostic; OpenJPEG metadata,
encoding and cache identity bodies are byte-identical (source-proof.json).
Focused production gate13tests0 and actual codec Release syntax0 precede fullbuild.

Configure06 exits0 and selects Maintenance26.4.0.69445, explicitly migrating oldON
cache metadata, requiring pinnedOpenHT0.27 and retaining OpenJPEG header/encoding.
Build06 is the full final source build; do not deliver prior69442 while it runs.
Doctor06 exits0 on the same M1Max host. Strictsourceaudit06 has0blocking/0scanerrors.

Independent final source review confirms557 preserves publication capability,
both scalar/CPU routes and actual distinct outputs. Schedule558 native baseline
fails10assertions; fixed actual fixture passes27assertions/4cases including
deadline/output/future-item retention, earlier-head insertion and expired/closed
semantics. These contained checks do not replace final integrated execution.

Pipelinecache05 separately passes ordinary5905assertions/6cases, global shader
validation882/1 and selective validation98/1; exact flags/selectors/logs are in
pipeline-cache-05-receipt.json. These preserve archive semantics without disabling
validation globally to make correctness tests pass.

Tooltests06 fail1 on import because unconfigured systemPython lacks PyYAML
(183discovered/1skip); preserve the exact failure. Re-run the full required suite
with the already configured isolated build-tool Python, which has PyYAML6.0.3.
No global environment modification or dependency replacement is needed.


The complete tool suite07 uses the configured isolated Python and exits0:
185discovered,184passed,1optional retained-native serializer fixture skipped
(no explicit retained stream selected),15.820s. System-Python import failure06
remains separate. No tool test failures are relabeled as product failures.


## Final candidate handoff

**Ready for owner testing:**
`/Users/dekstroza/Alchemy-candidates/Alchemy Metal Maintenance 69445.app`
(version26.4.0.69445 / bundle26.4.0d69445).
[Candidate report](../../research/maintenance-candidate-69445-20261121.md) is the
readable scope/evidence/remaining-work summary. Source a2aaaf29421dd49821606457923f246f73f77318
and indra6e3b13c1f4076c5244bfa708ce564d8e03cd6cdd are unchanged after qualification.
Only handoff documentation changes follow that code checkpoint.

| Final command/evidence | Result |
| --- | --- |
| doctor06; configure06; tools/metal/build.sh build06 | All exit0, complete1474-action Release build and signing. |
| CPU-tests07 absolute manifest, parallel4 |55/55pass,0skip,5.90s; XML/argv/source/log retained. |
| GPU-tests06 manifest, MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 |56/56pass,0skip,297.04s. Corrected routing/reload1864assertions; offline rigged765checks0failures. |
| Pipeline-cache06 three exact selectors/modes | All0; ordinary5905assertions/6cases, instrumented882/1, selective98/1. |
| Tool-tests07 configured Python |184pass,1optional retained-stream skip;185discovered/15.820s. |
| verify_app06 staged app |346Mach-O,allARM64,0invalid. |
| codesign06 --verify --deep --strict |0; valid on disk/designated requirement. |
| Strict source GL audit06 |0blocking,0scanerrors; informational legacy names retained. |
| Complete bundle inventories |7,839regularfiles/2internal symlinks/797,480,981bytes; source=stage;0external symlinks. |

Executable SHA256 `0c5de0c90878f85c7e71296c279d303adda475be39a9c31923b10a942e2b9d89`;
metallib SHA256 `6fe71e2833331703acc4912f83f21c9e6ff0a14b7ef5fad70b1f75dccd688793`.
The packaged OpenHT source licence exactly matches the pinned dependency;
app_settings/packages-info.txt retains its attribution. Actual decoder suite passes
56assertions/3cases; staged symbols contain OpenHT decoding and OpenJPEG header
parsing, without opj_decode. Main rpaths are only bundled Frameworks and system
Swift, with no build-directory path or direct OpenGL/Vulkan dependency. No live
viewer startup/decode collection or runtime no-GL claim is made.

Receipts: candidate-69445-receipt.json, candidate-69445-inventory.json,
final-test-summary-06.json and final-test-results-06.json in the private evidence
root. Compact receipt also beside the app at Maintenance-69445-receipt.json. The
first staging receipt script failed an assumed packages-info location; installed
path is Resources/app_settings/packages-info.txt. Corrected inventory/notice checks
pass without app mutation; candidate-staging-attempt-01.json preserves the failure.
Accepted69417 executable hash rechecked unchanged. No viewer process was launched,
no profile/settings modified, no live FPS result claimed and nothing pushed.

PPD527–555/557–559 move to ready_for_owner with exact final evidence and retained
histories; owner verification remains pending and push not_granted. PPD556 remains
investigating. All agents/source ownership are frozen; existing native-IM worktree
is untouched. These are bounded maintenance results, not product/release completion.

## Owner-requested launch

After candidate delivery the owner explicitly requested starting the viewer.
Opened the exact staged69445 bundle with its recorded executable SHA256, after
checking no other Alchemy viewer process was present. Process11514 was observed
running from that exact path; owner-launch-69445.json retains the observation.
No agent login, destination/preset override, screenshot or live performance
collection was performed. This is launch evidence, not owner test acceptance.


## Owner verification and publication authorization — 2026-11-21

After the exact staged viewer launch, owner states "looks good ...verified" and
"you can commit and push", then requests the outstanding-items display. This
authorizes the implemented69445 scope plus its qualification/cleanup/handoff
records. PPD527–555 and557–559 move to verified, Owner verification passed and
Push approval granted, retaining all earlier failures/history. PPD556's unresolved
request/retry contract remains investigating; no future repair is approved here.

Remote fetch observes origin/codex/apple-silicon-metal at797643859d, with0remote-only
and25local commits througha7728fbc6a. The outgoing scope contains the accepted
maintenance implementation and its source/qualification records, plus the two
preceding source-identical shoreline investigation/owner-confirmation docs. It
contains no candidate binary, private artifact/capture, unrelated worktree or
unapproved source implementation. The rendering tree stays
6e3b13c1f4076c5244bfa708ce564d8e03cd6cdd; no rebuild is needed for this approval-only
documentation update. Run the ordinary harness, commit and perform a normal push
with push.followTags=false; capture local/remote heads in owner-approval-push.json.

Remaining scope is consolidated in the active ledger's nine ordered rows. The
completed decoder migration is removed from the remaining-actions table; no
current saturated-queue reproduction, FPS improvement, exhaustive feature test,
release publication or closure of older owner incidents is invented.
