# Active goal — complete the Apple-first renderer architecture implementation

Status: complete for combined experiment implementation/evaluation/build/staged delivery;
owner runtime, in-world performance and full product gates remain open.
Owner authorization: 2026-11-13, continue original architecture
through all remaining goals, with one combined viewer test afterward. This
supersedes the previous per-goal owner-runtime stopping points. No agent viewer
or native-window/audio-device launch, login or new interactive profile/capture.
No push, publication or silent feature/quality reduction. Target next morning;
do not substitute that target for actual completion evidence.

Branch `codex/apple-first-architecture-g2-g5`, isolated worktree
`/private/tmp/alchemy-apple-first-architecture-g2-g5`. Baseline
`5836d41753fc3e09533d638748ac9756f89ac357` includes G1 immutable attachment
records and the confirmed `fef1e756fd` exception-ABI repair. Owner reports the
crash gone and another chat is preparing a five-minute G1 profile comparison.
Its checkout `/private/tmp/alchemy-apple-shadow-records-g1` is read-only here.
Preserve that session's files, profiling app, captures and baseline receipts.

## Deliverable and real execution boundaries

Implement the [remaining architecture route](../../research/apple-first-implementation-route.md)
in actual viewer producers/consumers, not test-only scaffolding or an alternate
engine. All application-owned rendering stays direct native Metal, ARM64/macOS
only. Retain Second Life scene semantics, main/probe/hero views, rigged bodies
and attachments, PBR/alpha/terrain/trees, queries, UI, media/voice and asset work.

| Goal | State | Concrete acceptance |
|---|---|---|
| G2 retained native source inputs and single per-view admission | implemented/tested | Ordered admitted LLDrawInfo list, generation-qualified immutable source/coverage/native leases and bounded scan-resistant cache integrated. Native correctness/cache stress pass; large-family mechanism gate accepted. Live key/admission/unique-source cost remains unmeasured. |
| G3 bounded detached CPU preparation | implemented/gated | Immutable compact instances/context, at most two synchronous Dispatch tasks and epoch cancellation integrated. Fixed preparation/source gates accept8192-family threshold; small families retain owner fast path. |
| G4 native visibility/indirect and useful Metal4 | implemented/evaluated | Actual GPU command emission, conservative visibility, ordered multi-tile/query runs and native Metal4 paths pass tested correctness/lifetime but lose net-cost gates and stay off. Only selective CPU-encoded opaque ICB>=2048 native records passes mixed controls and is enabled. Full GPU-owned scene discovery remains future work. |
| G5 UMA publication and frame/pass scheduling | implemented/tested | Real wrapper generation leases, changed-range safe shared writes/COW, media generation repair, memoryless transient depth and bounded logical shadow groups integrated. Cancellation/commit/query/lifetime/probe tests pass. Streamed views retained after whole-batch wall regression; no process-memory/FPS/zero-copy claim. |
| Integrated delivery | complete | Exact6bae/26.4.0.68995 Release app separately staged; required build/doctor, final contained tests, source/static/ABI/signature and full-copy inventories pass. No launch/push. Owner runtime/full parity/performance gates remain open. |

Success requires source-path integration and measured bounded correctness/cost,
not nominal CPU/GPU utilization or a new API object merely being constructible.
Existing direct Metal specializations remain when hardware/API constraints
require them; no fallback graphics backend, Vulkan/translation layer or generic
cross-platform renderer is introduced. Existing rejected parallel-encode,
matrix-memo and draw-joining trials remain evidence against repeating them.

## Ownership, status and evidence

Root owns `pipeline.cpp`, shared CMake, source admission/integration, canonical
state/ledger/register and final build. Subagents receive disjoint helpers/tests
and explicit interfaces before editing shared classes or shaders. Begin with
source/API audits, then implementation immediately; preserve independent work.

Evidence root:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-architecture-20261113/`.
Record commands, exit codes, clean code SHA or exact diff hashes, compiler flags,
input parameters, output parity, counters and raw timing data. Validation-enabled
correctness and validation-disabled timing are separate. No headless sample is
a measured viewer frame or proof of in-world FPS, latency or full scene parity.

PPD-089 single-use cost remains open for actual admission/reuse policy work.
PPD-090/091 repair is retained; owner now confirms the crash is gone. Record that
confirmation here without modifying the other session's checkout. Register any
new defect before repair, preserve failed tests/reverts and do not infer push
permission from owner runtime confirmation.

Next action: owner manually assesses the staged68995 app at the same club/settings as repaired G1. Implementation/comparison/contained validation and separate app delivery are complete; continue from actual runtime findings, retaining thresholds, rejected variants and unverified full-product gates. Do not launch or push automatically.

## Implementation checkpoint

- G2 owner admission/source cache is integrated in pipeline.cpp and LLDrawInfo:
  one scene-map admission, exact producer tuple comparison before native source
  construction, fallback texture generations, current pose and separate matrices,
  bounded weak scene handles/unique native working-set accounting, command-owned
  packet lifetime. Independent review corrected ordered repeated texture slots
  (PPD-096); actual pipeline syntax passes. Runtime caller tests remain pending.
- G3 helper serial/Dispatch parity and lifetime cases pass the first native suite.
  First complete-cost candidate makes Dispatch9–25%slower across active sizes;
  every block regresses. Candidate is rejected/defaultserial. A changed ownership
  implementation now removes per-output shared refcount work; same fixed cases
  will be rerun only after new correctness passes. No speedup claimed.
- G4 extended native CPUICB parity passes. Initial GPU copy-command compaction
  fails (PPD-095); real GPU render_command generation corrects R32 parity299/2
  with actual Metal4 execution. GPU-only input-table mode now removes redundant
  CPUICBsetup; combined borrowed-view correctness/complete-cost gate still next.
- G5 native leases/COW/range publication pass351assertions/7cases, including
  PPD-094 last-lane repair.32small writes copy2,048B versus2,621,440B under forced
  leased full orphans; this is a byte-count result, not CPU/FPS. IOSurface writer
  participates in COW (PPD-092). Shadow-only D32 is memoryless. Frame-tagged
  admission policy passes205assertions/6cases; native transaction/publication
  integration and command-count proof are still required (PPD-097).
- Initial suite14/15file entries passed; the sole GPU copy-command failure and
  all later repair attempts remain in canonical ignored evidence. Metal4 failed
  pre-submission slot reclamation (PPD-093) corrected; native retest pending.

Exclusive ownership now: root pipeline/sourcecache/media integration/CMake/docs
and resource visibility-abandonment/byte-estimate API; CPU agent world.h/native
prepare helper/tests; GPU agent world.cpp/indirect/material/visibility helpers/MSL
and extendedtests; evidence agent LLVB/LLImageGL/resourcelease tests (stable) and
new pure frameadmission helper/tests. GPU runs/builds are serialized by evidence
agent with explicit handoffs; root performs no simultaneous compiler work.

## Shadow transaction integration checkpoint

Root integrated one frame-tagged command for each ordinary sun/projector
invocation, borrowing existing probe/hero owners. Capture destruction cancels
before borrowed callback state is destroyed. Exactly-once commit/cancel
observers defer persistent slice validity, spot-cache keys and query attachment.
Per-slice revisions reject stale callbacks. Separate pending completion receipts
allow a completed CPU invocation's same-command receiver passes to consume
encoded maps without publishing them to other commands. Early return/cancel
keeps CPU invocation incomplete and discards publications; abandoned visibility
results now fail via native completion tokens. This repairs the source-confirmed
PPD-097 mechanism; compiled/native tests are next, not yet a passing claim.

Static unjoined opaque records retain exact source certificates; joined index
ranges deliberately return to ordinary validation. Resource cache accounting
counts each retained native wrapper once, with count/age and runtime-budget
limits; distinct wrappers aliasing the same native backing may conservatively
count twice. This is estimated retained allocation, not measured process RSS or
an incremental-memory saving.

Latest documentation harness: `python3 tools/metal/validate_harness.py`, exit0
(33 required files). `git diff --check`, exit0. Source remains WIP and no viewer
launch, owner combined acceptance or push has occurred.

## Coherent native validation and bounded preparation decision

`narrow-coherent-transaction1-build.json`: Release almetal/llrender narrow build
exit0,71.49s. `correctness-coherent-transaction1.json` plus exact binary/source
manifest and raw CTest log:18/18 selected entries pass0,61.29s, Metal API/shader
validation ON. Covers borrowed source/palette/coverage ownership, ordered2/8
texture aliases, native generation leases/ranges, old source capture/indirect/
materials, pure frame admission205/6, actual admission/reload/cancel/settlement/
observer exceptions, abandoned queries25/2 and retained publication behavior.
Actual viewer `pipeline.cpp` syntax0 in6.823s (`pipeline-transaction-syntax.*`).
These tests do not execute live LLDrawInfo admission, media playback or in-world
visual parity. PPD092–097 retain owner-pending status and failed-attempt history.

Fixed ownership trial1 is accepted for bounded preparation activation:
`shadow-prepare-cost/ownership-trial-1/{receipt.json,analysis.json,benchmark.log}`.
1600exact samples,200per-arm/case,10ABBA/BAAB blocks; sameRelease binary5dcb00aa,
validationOFF, source hashes unchanged. Includes source seals, input/output
construction, preparation and same-arm cleanup, excludes live scene/native
render encode.256records: median equal, p95+9.493%(reject).2048: median−7.148%,
p95−4.355%,10/10positive block medians.8192: median−22.126%,p95−19.988%,10/10
positive.32control executes serial in both arms. Production now permits at most
two synchronous Dispatch chunks only when one admitted family has>=2048records;
smaller families stay local. No new frame queue or scene/global access on workers.
The previous owned-output trial regressed and stays rejected; all16original
receipts/scripts/logs, including sandbox failure, are copied with hash proof.
This is CPU mechanism evidence, not observed viewer frame time/FPS.

Next: actual six-slice/one-command parity/query proof, native source and GPU
full-cost comparisons, supported mode selection, then full app build/stage and
static/source/exception/native-feature verification. No agent launch or push.

## Full native-cost rejection checkpoint

`narrow-cost-proof1-build.json`: Release build0,58.20s; current test binary
`9b667a0a46dbc4fbe6696646f5a5d7f8846a03a64864226da5c4005aad22ad50`.
`correctness-cost-proof1.json`: native API/shader validation0,1,861assertions/
8cases,48.12s, including actual six R32 slices with queries and6commits versus
one transaction, borrowed owner/cancel and old per-light preflight isolation.
Both new full-cost fixtures pass native output parity. These are headless
mechanism proofs, not viewer acceptance.

G2 fixed600-sample trial is NEGATIVE (PPD-099): retained-certified versus current
same-binary COW raw-owned API regresses every block in all three cases. Median
CPU+11.421% light1v32,+5.847% reused4v256,+5.799% reused6v1024; p95+8.033%,
+6.566%,+5.456%. `shadow-source-cost` canonical copy will retain receipt, raw
rows and strict parser. This raw arm already shares current COW coverage; it is
not untouched repaired-G1 live capture. Do not reinterpret it as an improvement.
CPU agent now audits historical producer differences and isolates source/input/
preparation/certification/retirement cost before a substantive ownership repair.
Production G2 candidate remains WIP and is not deliverable while slower.

G4 fixed9,000-row/90group baseline1 is also NEGATIVE (PPD-100): GPU-generated
commands preserve pixels but per-run serial GPU emission causes severe consumer
completion regressions. `shadow-generation-cost/baseline-1/{receipt.json,
analysis.json,benchmark.log}` retains source/binary hashes, actual independent
Metal4 GPU timing feedback and all balanced100sample-arm data. Static256unknown
bounds wall median+784.87%classic/+811.36%Metal4;2048+1503.99%/+1513.22%; even
25%rejection fails. Native CPU ICB saves~19%CPU at static2048 but wallp95+6.38%
is not a passing activation gate. All new GPU modes remain OFF in production.
GPU agent next implements parallel stable command emission rather than tuning
thresholds to hide the regression; add measured6standalone-vs1batch scheduling
cost and fixed completion-owned slab experiment. Preserve all baseline evidence.

PPD-098 exceptional cleanup source finding is repaired: settlement retains moved
tickets across allocation/observer failures and discards all on failure, with
allocation-free invalid/empty cleanup. Actual pipeline syntax0,7.336s
(`pipeline-ppd098-syntax.*`) and independent source review pass; private live
pipeline/OOM injection not executed. PPD092–100 remain owner-pending; next101.

Current source ownership unchanged: root pipeline/resource query/CMake/docs;
CPU helper/world.h/source benchmark; GPU backend/indirect/visibility/MSL/extended
and generation benchmark; evidence native resource files (stable), build/test
coordination. All quiet timing windows ended. Next coherent narrow build follows
CPU/GPU repaired snapshots; validate before reusing the exact declared trial
parameters in NEW evidence directories. Full `tools/metal/build.sh build` remains
required after passing mode choices. Goal is ACTIVE; do not stop for another
owner prompt or claim an app/performance win. No viewer/window/audio launch,
login, new interactive profiler/capture or push.

## Compact CPU / parallel GPU repair candidate (not yet measured)

PPD-099 V2 now integrates a compact raw instance (source/current pose/model), one
copied view/projection context per synchronous preparation call, and a distinct
source/pose lease once per actual native command across all six views. Production
uses authoritative bounded LRU records directly on hits, removing repeated weak
locks; key revalidation and actual per-view model/current pose resolution remain.
Source/pose deduplication precedes shared ownership copying, with rollback if its
owner-vector allocation fails. Native completion owns the aggregate; settlement
releases the capture's matching-token lease on commit/cancel. Scratch instances
and contexts are not GPU-retained. All joined sources stay pinned even where
joined ranges lose individual certification. Independent source review and
actual pipeline syntax0,6.857s (`pipeline-compact-v2-syntax.*`) pass.

The V2 source fixture strengthens BOTH arms: explicit raw model snapshot/shared
view context replaces the old raw temporary; candidate matches commandaggregate
construction/acquisition/dedup/destruction. Same fixed3cases/600samples/order,
prior failure retained. Historical5836 constructed inline640-byte covered arrays
and resolved materials per draw/view; the faster current COW raw control does
not reproduce that cost. No historical/live-pipeline speedup is inferred.
Compact Dispatch has its own fixed1,600-sample trial; production is temporarily
serial until that new overload's threshold is measured. The earlier accepted
owned-input trial is historical evidence, not the compact activation gate.

PPD-100 V2 replaces serial command setters with a256-lane stable SIMD/threadgroup
prefix, tiled carry and parallel command emission. GPU-only slots stop allocating
unused CPU template ICB/proxies/resource-set reservations. Source order remains
within original state/query runs; new1,027-input/257+511+259-run actual GPU proof
covers multiple/partial tiles and final-source ordering. OfflineMSL/native C++
syntax pass; coordinated coherent narrow build and native tests now running.
G4/G5 fixtures retain the same legacy-owned preparation in both arms to isolate
GPU mode/submission effects; they do not claim compact production producer cost.

Next fixed windows after correctness: sourceV2600rows, compactprep1,600rows,
parallelgeneration9,000rows and6standalone-vs1transaction1,200rows, exact old
parameters/new evidence directories and validationOFF. Do not rerun unchanged
candidates or turn on slower paths. Then any justified stable-slab work, final
source/mode freeze, full build, broad safe regressions and separate app staging.

## Overnight streamed-owner candidate checkpoint (2026-11-14)

Latest native correctness: `correctness-compact-parallel-v2.*` passes80,364
assertions/27cases in11.83s with Metal API/shader validation; native resource
retest351/7 passes. Exact test binary240364e9a9d5e044c17bcb1ec6d25457f6136285ab8ba4bb11e3b2852a75ec38.

The compact V2 source trial still does not beat the strengthened raw control:
lightCPUmedian+9.699%,p95+6.323%;4v256+0.582%/+0.539%;6v1024+0.935%/−0.209%.
The new compact preparation trial supports Dispatch ONLY at8192: median−17.719%,
p95−14.879%,10/10positive blocks;2048regresses2.733%/4.961% and256regresses
5.380%/19.029%. Exact600/1600 followup receipts remain separate from old results.

Parallel GPU generation repairs serial catastrophe but still fails total-cost
activation: `shadow-generation-cost/parallel-1/` has9000validrows/90groups.
Static2048 all-visible classic CPU−15.42% but completionwall+39.71%/p95+53.91%;
Metal4 CPU−6.16% but wall+20.89%/+23.80%; CPUICB wall+10.88%/+16.42%. All
modes remain disabled in production. PPD-102 fixture epoch aging finding may
inflate PBR table rebuild cost; corrected lifecycle/material counters will be
used in final declared slab test, without relabeling earlier failures.

G5 `shadow-frame-batch-cost/baseline-1/` passes1200exactrows/native6vs1counts
but REJECTS whole-invocation batching: busy logical completion+cleanup wall
regresses27–35% across all families. Root preserves streamed per-view commands,
so GPU can overlap next view CPU work. Each independently owned view now commits
immediately after renderSpotShadow succeeds; query/validity publication occurs
only after actual commit, diagnostics retain a local source lease, failure
cancels only its owner. Borrowed outer transactions remain deferred/owned by
caller. Root pipeline source syntax0,6.726s before the subsequent group-ID fix.

PPD-103 source audit corrected a false assumption: ordinary probe faces have2/4
sun cascades, share gFrameCount and do NOT all have an outer batch (hero does).
Capture now uses monotonic logical shadow invocation groups, max6 streamed
commands/group and2pendinggroups; each cube face gets its own group. This is NOT
'a two-display-frame buffer'. Simulation/presentation/buffer read/completion
frame accounting remains gFrameCount. Renderer/capture swaps and reload preserve
the appropriate serial/policy. Independent source review finds no blocker;
actual private viewer execution remains untested. Next defect104.

G2 V3 uses cached immutable producer scalar tuples directly on the owner below
8192, borrows frozen coverage/current pose under its actual command aggregate,
uses existing LLMatrix4a MVP and ordinary native validation (no public batch
certificate comparison). Only one>=8192family uses compact snapshots+two jobs.
No compact input/output staging pass for small families. Four fixed V3 cases:
original3mixed cases plus one8192riggedPBRfamily/view,16sources/8slots,100perarm,
800rows. Both arms stream per-view commands, include cold source seals/aggregate
pins/destruction and report CPU-side elapsed, summed GPU envelope, logical span
and wall. Live LLDrawInfo key/LRU/admission and unique-source workload mix remain
excluded/unknown. Header/helper/source benchmark stable and syntax pass.

GPU agent is completing final stable-data experiment: bounded completion-owned
write-combined shared slab, fixed constant/palette bindings, reuse only after
actual completion/abandon, cap32slots/128MiB with same-ABI arena allocation path
at capacity, reload transfer and counters. Trial includes ordinary bulk direct
with stable data, as ICB GPU overhead has not passed. Direct body/cloth/terrain
specializations stay exact. Safety/parity tests and corrected lifecycle cost
snapshot pending; no default flag activation until net gate passes.

Evidence agent has NOT executed final full build yet. Ready runners:
`run-full-release-build.py LABEL --expected-source-sha FULLSHA` requires clean
compiled source/current configured SHA/all30 retained flags/pinnedOpenHTJ2K;
`run-broad-contained-regressions.py LABEL [--with-private-corpus]` runs59explicit
CTest entries+10exact source-vetted headless specialization cases. Whole grouped
GPU/window/capture/audio-device tests excluded. After final cost/source choices,
commit local source, reconfigure, run required full wrapper build, regressions,
static/native/ABI/ARM64/signature/source checks and separate app staging. Goal
remains ACTIVE; no app delivery/launch/push/owner combined acceptance claimed.

## Final slab fixture validation hold

`narrow-stable-data-v3-build.*` passes0,58.83s; actual pipeline including logical
probe groups syntax0,6.905s (`pipeline-group-v3-syntax.*`). First new slab native
suite fails29/30cases/3assertions (db90ed binary); strengthened gate2 test-only
rebuild0,2.28s/412d77 binary still fails29/30/7assertions. All failures preserved,
NO sourceV3 or stable-data timing executed on these snapshots.

PPD-104 explains the fixture failure, not a proved slab allocator failure. The
first gate was unrelated GPU work; a tracked identical position write fixes that
oracle, but requiring reader-pending AFTER reload creates a self-wait because
reload create() synchronously commitAndWaits lookup/fallback uploads on the same
device queue (world.cpp6453/6486/6520/6566/6595/6665). Five-second watchdog
releases the gate, making actual completion/slot reuse/packet expiry legitimate.
Test-only split is approved: keep every original three-slot/source lifetime/
cancel/retry/renderer-destruction assertion while actual readers are gated;
completed reload pool-transfer/content coverage already exists separately.
No timeout increase or production lifetime change. Next fast test rebuild +
native validation is required before fixed800-row sourceV3 and9600-row slab
trials. Both final cost fixtures stream per-view commands, matching production.
Next defect105; active goal and final app build/stage are still incomplete.

## Final source freeze and combined build

LOCAL source/CMake/test commit `d5ae05f2ffec7c243667d0414b5fc003a9164fb4`
contains36files/7,237insertions/209deletions. No push. Final actual pipeline
syntax0,6.686s (`pipeline-final-activation-syntax.*`). Documentation remains
uncommitted while the source/configuration pins are frozen for the app build.

Coherent native correctness81,815assertions/39cases +resource351/7 passes with
API/shader validation (`correctness-opaque-cache-final.*`). Mixed400cost on
2339db44 binary passes exact source/counters/native6commands and all10CPU/wall
blocks: allvisible CPU−17.20%/p95−11.24%,wall−13.72%/−7.68%;25%offscreen CPU
−18.02%/−14.67%,wall−14.42%/−8.78%. Production now sets ONLY
scene.opaque_indirect_draws=true, selecting native CPU-encoded opaque runs>=2048;
covered/direct/body/terrain/fragmented/oversized behavior preserved. Other full
indirect/GPUgeneration/Metal4/global-stable flags remain off. Correctness includes
16k+tail, rawpose, small32 andquery-fragmentation preservation.

PPD-106 actual helper stress is repaired: baseline old LRU0hits/8193admissions
on each subsequent8193-source scan; protected admission8192hits/0admissions on
all5scans, constant8192entries/262unique logical wrapper bytes. Final native
cache tests154assertions/7cases pass. New helper used by pipeline, with exact-key
weak overflow reuse only while actual source ownership remains alive. No capacity
increase. Source cardinality/live key/discovery costs remain stated limitations.

PPD-104 repaired oracle/native blocked-reader/cancel/destruction and separate
completed-reload tests pass; both failed prior snapshots preserved. PPD-105
analysis-only repair accepts the exact existing800 native rows without rerun;
overlapping GPU envelopes do not order against logical span.8192 native route
CPU−8.501%/p95−8.678%,wall−7.551%/p95−6.891%,all10blocks positive; smallcases
show no consistent net improvement. All129sourceV3 evidence files copied and
hash-verified. Existing club data does not record per-run/family distributions;
threshold activation in that club is UNKNOWN, not inferred from cumulative means.

Final commands delegated to evidence coordinator: reconfigure exact retained
channel/30feature flags/pinnedOpenHTJ2K for frozen source, then mandatory
`tools/metal/doctor.sh` and `tools/metal/build.sh build` via guarded Release8job
runner; broad60CTest+10exact source-vetted headless specializations, private codec
corpus/realvoiceABI and staticARM64/import/source/signature checks follow. Native
static verifier/staging agent is ready. No agent viewer/audio/window launch,
interactive profiling/capture or push. Goal stillACTIVE until verified separate
combined app is actually staged; don't equate source freeze with delivery.


## Combined Release build and broad-regression hold

Frozen d5ae05f2 full Release configuration and doctor exit0; required build wrapper
exit0 in933.97s. App Alchemy Metal Alpha G2G5,26.4.0.68994/CFBundle26.4.0d68994,
main SHA256d311cd3cc1e3aef47e37aaf27c356269653862031ea4388bb529ec8dd652059b.
Final static verifier exit0 includes required verify_app.sh346MachO/0invalidARM64,
strict deep signatures, source/cache/channel/30retained flags, exact pinned
OpenHTJ2K/license/nativeownedimports/music/voice/codecs/newarchitecture/system
exceptionRTTI0private. Receipts final-release-{configure,build}-1.json and
bundle-static-verification-final-1.json under the combined evidence root.

First final broad pass is NOT accepted:57/60CTest entries,9/10exactheadless
specializations pass; actual voice/codec exceptionABI and private240request corpus
pass. PPD107 records LLVB completed-store/label assertions,108 records archive
file/hit assertions with shader instrumentation1/1,109 records exact PBR terrain
selection exit5/emptylog. All failure receipts remain final-broad-contained-1.*.
No production bug is inferred from assertion failures before attribution.
Evidence agent owns buffer fixture/source triage and selector runner, GPU agent
independently audits archive policy, CPU agent independently reviews safe-current
vs retired-generation/label contract. Root registered all reports before repairs.
Do not weaken meaningful byte/lifetime/label/coverage checks; distinguish correct
new policy from actual regression. Separate private candidate staging can finish
but delivery stays held until fixes/retest and any refreshed source pin/build.
No viewer/window/audio session launch, capture or push. Goal remainsACTIVE.


## Corrective fixture pin

Independent root/CPU source review confirms PPD107 stale mandatory retired-first
expectations, not a production byte/label defect. Reviewed two-fixture repair
adds both completed/unleased current reuse and forced current-lease retired
reuse/index orphan, retaining exact old GPU-copy bytes and label immutability.
Narrow build0/2.12s and API/shader573assertions/11cases pass. Local final test pin
`6baeb0340a1ddeb89aa265a1a64321ad1e0cd57a` differs from d5ae only in those two
fixtures; production/CMake/tools unchanged. Required wrapper will refresh app
source/version after archive/PBR probes. HEAD frozen, docs-only dirt.

PPD108 source proof confirms existing disk/instrumented/selective archive tests
need distinct environments; production bypass preserves actual instrumented PSOs.
Corrected external runner keeps59CTest groups plus10exact specializations and
three archive policy processes, with mandatory real disk hits and actual native
validation bypass/selective behavior. PPD109 metadata reproduces quoted-comma
selectionexit5; escaped-comma literal selects exactly one registered PBR case.
Native archive/PBR probes are next; final corrected broad/source/static/stage
remain pending. Failed final-broad-contained-1 receipts are retained.

Strict production sourceGL audit0:3548firstparty files/0blockers/0scanerrors,
901informational legacy-name rows. Receipt source-gl-audit-d5a-receipt.json
records d5aHEAD/test-only diff at execution; tests excluded and production code
is unchanged at6bae. No ignored raw log/profile scanning or app launch.


Corrective native probes all pass without skips: archive disk5905assertions/6cases
with real persistence/hits; actual instrumented bypass882/1; selective native
label-validation98/1; escaped exact PBR terrain128/1. Total7013/9, unchanged
almetal binary before/after. Receipt ppd108-109-native-probes-1.json. No production
or archive/terrain test source changed. PPD107bufferfixtures573/11also pass.
Final exact6bae configuration/doctor/requiredwrapper incremental app refresh and
corrected59CTest+10exact+3archivepolicy+privatecorpus are now executing. Goal
stillACTIVE until final app/static/stage evidence completes; no agent launch/push.


## Final combined delivery — observed2026-11-14UTC

Final compiled code/test source `6baeb0340a1ddeb89aa265a1a64321ad1e0cd57a`,
renderer implementation d5ae05f2. Release26.4.0.68995/CFBundle26.4.0d68995,
main SHA25696dcc743c9bfdd471984dd7843639137eeb1c4c3eb77dfd722e232ce6124e2ae.
Final configure0/4.15s,doctor0/.54s,requiredwrapperbuild0/25.90s; full prior
build933.97s. Required commands/environments/source/cache/library hashes are in
final-release-configure-2.json and final-release-build-2.json.

Final corrected broad59/59CTest groups,10/10exactnative specializations,3/3
archive-policy processes and boundedprivate corpus all exit0:15/15commandstages,
563,974assertions/301cases,240submitted/240success/noescapedexception/timeout.
Actualvoice/codec exceptionABI passes. Source/binary manifest, exactargv and logs
are final-broad-contained-2.*, totals final-regression-summary.json. Original
failed broad1 and all earlier adverse/oracle/parser receipts remain. No tool
source changes. PPD107–109 corrective attribution and verification retained.

Built and separately staged app static checks pass:346MachOallARM64,strictdeep
signatures, exactsource/version/channel/cache/30retainedfeatureflags/OpenHT
archive+receipt+license, nativeownedviewer/CEF/VLCimports, retainedmusic/voice/
codecs/newarchitecture, systemexceptionRTTI/0private. Receipts
bundle-static-verification-final-6bae.json and staging-final-6bae-static.json.
StrictproductionGL source3548files/0blocking/0scanerrors,901info;
source-gl-audit-6bae-receipt.json. Foreign/runtime checks remain separate.

Staged app:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-architecture-20261113/staged/Alchemy Metal Alpha G2G5.app`.
Ditto0,fullsourcebefore/staged/sourceafter inventories equal for7839files,2links,
432directories/795,773,875filebytes,mode/link/hash/path; sourceunchanged. Tree
inventorySHA2562f32f721d45a6cf3cedb9958abb23e510543e98dab849f62c3257f5050d98cf2.
Receipt staging-final-6bae.json plus threeinventories. Otherapps/G1 checkout
untouched. All processes terminal and metadata captured at6bae; docs-only
followup does not change compiledsourcepin.

[Final results](../../research/apple-first-architecture-g2-g5-results.md) describes
accepted defaults, rejected GPU/Metal4/global-slab/whole-batch modes, exact
headless limits and remaining complete-architecture path. This completes the
tracked combined experimental delivery, not the product release gates or whole
viewer threading rewrite. Next owner sameclub/camera/quality/warm-cache check,
including movingattachments/PBRalpha/probes/media/voice; no agent viewer/window/
audio launch, login, newprofile/capture or push occurred. Runtime activation,
FPS/latency, full feature parity and long stability remain unverified.


## Owner-authorized five-minute runtime profile —2026-11-14

Owner manually started staged68995, arrived at the busy club and explicitly
requested five-minute profiling. This supersedes the prior no-new-profile
constraint for this one attach capture; no agent launch/settings/camera change.
Verified livePID19459/path/main96dcc743/compiled6bae before attachment. Exact
Game Performance Overview5m command is in newevidence recording.json; actual
run1TOC08:00:41.550–08:05:43.895UTC/302.345784s/time-limit/recorderexit0.
Finalization498.799s totalwall includesDWARF processing, notextra measurement.
Owneraskedcanclose and was toldyes afterthetime-limit; subsequentnormalquit
is not a crash. Trace and302s numeric tables preserved locally/private.

[Profile assessment](../../research/g2g5-busy-club-profile-2026-11-14.md),
PPD110: wholeFPS17.773vsG120.364(−12.72%), associateddisplay56.385vs49.135ms,
associatedGPUspan87.220vs85.394ms(+2.14%)/notGPUbusy, CB/frame32.842vs23.882
(+37.52%). Last60FPS17.925vs20.113(−10.88%). Nominalthermal302.346s; empty
individual-frame tables mean noframep95.420savedgraphicssettingsmatchG1 and
currentbefore/after, but livecamera/population/cache/probe/viewportunmatched.
This capture doesnotdemonstrate thedesiredviewerspeedup or isolateacause.
CPUattribution/333DWARFwarningcoverage is beingfinalized fromsaveddata only.
No newimplementationchange/capture/push. Original completed deliverygoal remains
complete forimplementation/build/stage; runtimeperformanceassessment isseparate.

Newevidence root artifacts/metal/g2g5-busy-club-profile-20261114-080031/:
recording.json,toc.xml,capture-verification.json,originaltrace,sevenGPUexports/
analysisreceipts,gpu-summary.md/gpu-g1-comparison.json,numericlog-metrics.json
and420controlgraphicsreceipts. CPU firstsandboxcachefailure−6 ispreserved; root
unrestrictedofflineexportpasses0/31.917s,271,352,656B XML, no duplicatecapture.


## Whole-engine analysis finalized — owner steering retained

[Final profile assessment](../../research/g2g5-busy-club-profile-2026-11-14.md)
now covers the actual frame and input/resource economy. Owner explicitly
rejects treating liveworld variation as grounds to dismiss poorFPS or treating
anisolated shadowpass/helper win asengine success. No quality/light/projector
reduction is proposed. Frame-time/pacing/texture response andwork scaling across
variableonline load areacceptance, alongsidecorrectness.

Samepublished3456×2168/420savedcontrols match. Main CPU49.30vs33.97ms/proxy
present; sourcecapture7.34vs2.22(+231%) whilemain admittedsources/frame+29.54%,
probe+25.26%; Gbufferbinds+57.81%. Key/cache/aggregate leases1.40/1.05/1.36ms
separately, sealing.641CPU-s, matrixprep.495CPU-s. Decode629CPU-s islegitimate
parallelenginework, notframe latency andnotanexcuse:56,104vs453completions,
backloglimit6/waiting2508/oldest4.64s; maintextureupdate4.39vs.99ms/present.
Textureallocations16.22vs6.97/frame; buffercreation/s+2.39%only. Pressure
system-low17/30,bias1–3.029vs1; targetpool+200.074MiB/noevictions atsame
publishedsize. Countersdonotprove COW/redecodefeedback cause.

SuccessfulopaqueICBexecutionisabsent fromloggedmonotonicgenericcounters for
bothmain/probe. Source tracing verifies selectedopaqueexecute increments those
samepoolcounters; notmerely samplerfunctionabsence. Eligibility/fallback/cache
hits/8192Dispatch/uploadbytes/producer-commandlabels remainunknown. Lowering
thresholds isnotsupported bypreviousadversesmallcasegates. Commonqueue app/
probe/upload sourcegraph demonstrates separatelyboundedproducerinteraction;
estimatedCB/s+19.91% notjustFPSdenominator. Exactextrapassoriginunproved.

CPUonepass0/26.03s,990,368Runningrows; target990.368CPU-s,main264.914CPU-s
(87.62%onecore),decoder628.601. Mainaddress-only1.410s(.532%),missing.002s;
333DWARFwarningsretained. Boundedanalysiscompleted,noextrasymbolicationloop.
Allagentprocessesterminal. Newreceipts/census/CPU-GPUreportsarelocalonly.
PPD110remainsinvestigating/noowneracceptance/nopush.

Nextengineeringboundary: publishchangedsource/material/resourcegenerationsat
realproducers once, compactper-viewreadreferences; correlate/admitstreaming
publication/residency/COWbytes underinteractiveframebudget; coordinateexisting
main/shadow/probe/upload completion/admission without giantbatching orfeature
loss. Evaluatewholeframes/p95/throughput+quality acrosssettled/streaming/traversal/
probe-avatarload, notonlyfixed16-source/2k-runsyntheticmechanisms. Current
implementationdeliverywascomplete, architecturalperformanceobjectiveisunmet.
Analysisrequestdoesnotitselfstartanothercaptureorimplementationcycle.
