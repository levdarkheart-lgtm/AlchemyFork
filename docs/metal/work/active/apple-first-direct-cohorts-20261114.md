# Retained ordinary Metal material cohorts

Status: doing. Continuation of the original whole-engine architecture goal,
not a replacement goal or a finished architecture claim. Worktree
`/private/tmp/alchemy-apple-first-native-cohort-consumption`, branch
`codex/apple-first-native-cohort-consumption`, base `941e1339ec`.

## Reason for this boundary

The combined build at `ec44584c06` (26.4.0.69044) passes required build/static
checks and contained native correctness. Its actual command-bank cost fixture
passes parity but all48 CPU comparisons regress40–122% (PPD-191); scalar GPU
source rows also regress (PPD-183). Both remain disabled. Producer publication
is already shared, but the old bridge expands retained material groups back
into304-byte references and repeated per-source preparation. This work removes
that expansion while retaining ordinary direct Metal shaders and GPU work.

## Ownership and preserved behavior

Original reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, original
`LLCullResult` ordered render maps and material draw-pool traversal, owns source
ordering, pass separation, material coverage and view matrices. Original
`LLDrawPoolBump::renderDeferred` also separates masked/unmasked material work.
The new native certificate has no original API equivalent. It certifies only
completed static Legacy/PBR opaque or mask publications; it does not change
their shader math, texture matrix, ordering, depth, query boundaries or culling.
Unsupported/animated/rigged/other passes retain their ordinary native path.

- Producer seals owned prototype/key metadata once per dirty publication, after
  checking the complete typed native resource union. Certificate owner identities
  are weak; CPU publication and native allocation tuple have separate lifetimes.
- A captured cohort contains one copied view and one CPU/native owner pair per
  ordered group. Old raw/reference order tokens describe only noncohort draws;
  logical query offsets include all cohort rows.
- Adoption checks group intervals, exact owners and current view matrices. The
  renderer consumes sealed source rows with a sequential cursor and skips
  redundant static validation. Existing ordinary Metal PSOs remain in use.
- Command completion may retain native tuples, not CPU producer graphs. CPU
  prototype metadata remains alive through encoding only. Dirty publication
  metadata counts against the existing bounded group budget.

Root exclusively owns CMake, documentation, integration and all builds/tests.
Residency owns cohort/draw facade/adopt/reclaim and certificate fixtures. Opaque
owns world renderer declarations/consumption and renderer fixtures. Architecture
owns actual pipeline publication/capture and real-owner correctness/cost fixtures.
These disjoint files are edited in the same isolated tree by explicit assignment.

## Required evidence and next action

Implementation is unbuilt and untested at this checkpoint. Freeze/review the
complete change, integrate locally, run doctor/configure/build and verify_app,
then native API/GPU validation for sealed immutability, ownership/retirement,
raw/ref/cohort ordering, fixed per-source UVs, Legacy/PBR target ABIs, depth and
queries. Measure actual publication→capture→adopt→encode→reclaim→completion
cold/warm/churn costs at small/many source counts and one/multiple views.
Cold first-view raw admission and dirty resource-flavor duplication must remain
inside the measured boundary; do not pre-seal outside the timer to hide them.

Only an end-to-end improvement with preserved behavior can justify enabling
this path in a separately staged owner candidate. No FPS gain is established by
contained timings. Existing CPU preparation, source rows and ICB experiments
remain disabled unless their own measured costs justify activation. The larger
architecture goal, all feature/stability/performance release gates and owner
whole-viewer assessment remain open. No viewer launch/login/profile or push.

## Coherent source freeze

All three source owners froze their disjoint edits; two read-only cross-reviews
found no concrete owner-lifetime, insertion-order or query-offset counterexample.
This is source review, not executed correctness. New native fixtures cover seal
isolation, exact/expired owners, mixed ordering, current view/fixed UVs, both
normal target ABIs, preparation/reclamation and pending native-only retirement.
Renderer cases also cover compact terrain factor indexing and standalone large
palettes, plus ordinary prior-library behavior. Actual-owner hidden cost tag is
`[.native-direct-cohort-cost]`,48 comparisons with12 samples plus2 retained
warmups. Root next integrates this exact freeze into the original experiment
tree and runs required build16 before executing any native/cost gate.

## Executed first direct-cohort gate

Clean `c57b22353111b548be00a850b7cb27972c74321e` required16 doctor/configure/
build exit0,167.650s. Static26.4.0.69046 passes346ARM64/signatures/imports/
source/media/voice and all seven cost-gated controlsOFF. Strict source scans
3585 files with905 informational rows,0 blockers and0 errors.

Native cohort suite4/5 cases,969/970 assertions: PPD192 negative owner alias is
the first member's identical address. Actual publisher43/44cases,9419/9421:
PPD193 still expects optional argument-table binding for the selected ordinary
cohort path. Both corrections are test-only at2180adfac9; required17/retests
follow. Renderer764/1 and prior ordinary shader286/1 pass. Banks1942/5+old84/1, budget194/14, admission311/4, sharedlibrary331/3+
old82/1, region64/5, rows7354/9, compact1990/8, resources12379/7,
oldresource4721/1, oldrows283/1 and missinglayout13/1 all pass under native
API/GPU validation. Receipts retain the two failed suites, not an all-pass claim.

Actual-direct-cohort-costs-16 passes87566assertions/48comparisons. Warm512 CPU
falls61.96–66.91%; GPU median sums are between2.79%lower and1.28%higher. But
all16cold comparisons regress7.58–153.66%CPU and all16churn regress5.57–117.52%.
Legacy512/oneview CPU warm0.761→0.289ms, cold2.200→5.580ms,
churn0.899→1.956ms. PBR512/threeviews warm2.244→0.743ms,
cold5.618→10.039ms, churn2.448→2.670ms. These are contained costs, not FPS.
Raw samples, setup rows and summary JSON remain in the private evidence root.

PPD194 blocks default activation. Next producer change reuses private static
per-source records at the existing immutable WorldVariant boundary, avoiding
whole-group prototype recopy/revalidation on partial mutation, and removes
duplicate raw/group source flavor publication. Native owner lifetimes remain
independent; idle layout metadata must not become a new GPU reader. Residency
owns cohort API/fixtures, architecture owns producer/capture/actual fixtures,
opaque provides read-only lifecycle review, root owns integration/execution.
No plain-SIMPLE expansion, shader rewrite or extra GPU cache in this repair.

Required17 at2180adfac9 passes28.527s; static69047 passes. Repaired owner
fixture971assertions/5cases passes with API/GPU validation. Combined17 passes
88/88 selected CTest entries (138.458s),10 additional native specialization
processes,3 archive-policy processes and the private decoder corpus. This
includes the actual-publication lifetime assertion repair; other native16
receipts remain evidence for unchanged production source, not new executions.
No production renderer changed between16 and17. PPD192/193 retain their
failed evidence and are not closed by these automated checks.
The separate exact-tag actual-publication retest also passes9423/44 at17;
CTest's filename selection covers9367/43 and is not misreported as that suite.

## Reused source records: measured follow-up

Clean e5fd8bd98e required18 passes162.650s; static69048 passes. Source records
now validate/copy once per completed requested source generation and are shared
by WorldVariant/cohorts; idle Layout references remain weak. Resource capability
bit14 no longer splits semantic source variants. FlagOFF baseline still creates
neither resource descriptors nor cohort records. Immutable promotion uses exact
already-accounted generations and checks original publication identity/revision.
PPD195 fault/reentrancy guard is implemented with authored deterministic cases.

Native18 cohort1080/6, renderer764/1+prior286/1 and earlier bank/budget/library/
region gates pass. Actual producer46/47cases,9787/9788 assertions: new PPD196
parity test renders a camera-independent prototype without current view matrices.
It passes the source/key/native-tuple/revision assertions before ordinary batch
validation correctly rejects that incomplete test input. Source record reuse and
PPD195 allocation/reentrancy cases pass. Test-only21efebeb18 supplies actual
fixture view matrices; required19/retests follow. Preserve failed18 receipt.

Actual cost18 passes88718/48 with exact source-record publication counters:
baseline0 records, candidate only updated rows; no whole-group revalidation is
hidden. Warm512 CPU61.88–65.27%lower. All16cold cases remain5.24–90.20%higher;
15/16churn cases remain slower, oneview512churn77.76–84.59%higher. Cold512
oneview Legacy2.197→4.178ms/PBR3.924→6.149ms. WarmGPU Legacy512/oneview
has an outlying median0.122→0.180ms (+47.7%); other512 medians span3.31%lower
to1.82%higher. Retain raw samples; do not claim GPU equivalence or whole FPS.

PPD194 remains open and activationOFF. Next consumes a successfully post-flush
sealed group in that same dirty view. Existing cull raw rows stay in place;
equal-count replacement ranges verify exact ordered pointers and skip replaced
raw entries during capture, preserving later offsets, warm-range ordering and
logical query counts. Failed seal/verification/allocation keeps raw handling.
Residency owns llspatialpartition.h/shared native range bodies; architecture
owns postSort/capture and actual owner fixtures/cost. Root integrates/builds;
there is no shader/cache or second-cull-pass expansion in this next boundary.

## Exact continuation checkpoint

Original goal remains ACTIVE, with no owner-ready architecture/performance
candidate. Root experiment tree is clean at21efebeb18cf61ab9d9ca08db86c386a61713702.
Required19 passes28.588s, static69049 passes; native cohort1080/6 and actual
producer9882/47 pass. Native19 runner completed all focused gates with exit0, including the resource/old-library
tail: `/tmp/run_source_cohort_correctness.py 19 native-direct-cohort-correctness-19`.
Receipt native-direct-cohort-correctness-19.json is authoritative for completion;
do not infer final success from the earlier passing commands.

Isolated native-cohort-consumption is based on the same21ef, with active disjoint
edits: residency owns llspatialpartition.h and llcullresult_native_ranges.inl;
architecture owns pipeline.cpp, pipeline_group_capture_native.inl and actual
producer fixture. Root owns documentation, CMake, integration and all execution.
Raw-span replacement must reject an existing warm range strictly inside its
span, allow equal-begin warm ranges before replacement and equal-end ranges
after it, and reject duplicate/overlapping replacements without mutating raw
storage. Actual fixture includes the shared production range bodies.

Next obtain the remaining source freeze, review replacement order/
failure/query invariants, commit/integrate locally and run fresh required20,
native correctness and the unchanged48-case complete cost scope with actual
post-flush same-view promotion included. Preserve18 costs and GPU outlier.
Do not enable the path or stage it as a completed architecture before evidence.
Next global defect197. No viewer operation or push; canonical owner tree and
earlier staged owner/reference applications remain untouched.

Native19 terminal receipt confirms all17 focused processes exit0. PPD196 parity
fixture and PPD195 promotion/exception/reentrancy cases pass in actual9882/47.
Residency native-range API/source is frozen; architecture postSort/capture and
actual-case/cost updates remain in progress. No app or test job is running.

## Same-frame source freeze for build20

Previous goal turn made concrete progress: immutable per-source sharing and
semantic capability promotion were integrated and native19 completed. This
continuation adds the actual post-flush consumer boundary, not another status
checkpoint. All source owners now frozen; implementation is not yet executed.

Raw replacements validate exact nonnull ordered pointers, preserve raw vectors/
logical counts and reject overlaps or interior warm insertions. Capture skips
only a verified equal-count replacement. Main/probe counters expose successful
same-view replacements. The actual-owner fixture uses the shared production
range methods and includes pending-map/per-group-vector/expected-pointer-vector
allocation/find work in its timed boundary, rather than borrowing a free span.
It retains the48-case matrix and requires accepted cohort draws in every view.
New parity cases cover warm-begin/replacement/warm-end ordering, unchanged raw
fallback, all attachments/depth and7 logical query positions. Root integrates
this freeze, then executes required20 and native/cost gates. Activation staysOFF.

## Same-frame result and remaining direct-work coverage

Clean4f52dcfec2ed77334c654cfe7f49a1ced27c7cac required20 passes138.107s;
static69052 passes346ARM64/signatures/imports/source/features/sevenOFFcontrols.
Actual native producer10198assertions/48cases passes with API/GPU validation.
almetal_tests is byte-identical to19 (binary-equivalence-19-20.json), so its
earlier native evidence is explicitly reused rather than rerun/relabelled.

Actual-direct-cohort-costs-20 passes91726/48. Warm512CPU59.24–63.72%lower;
threeview512churn26.15–28.68%lower. All16cold cases still5.65–65.32%higher;
14/16churn comparisons remain slower (oneview51231.12–34.32%higher). Large
GPU sums range2.50%lower to3.82%higher; raw18 outlier remains retained. No
whole-viewer FPS or default activation acceptance. Pending map/vector and exact
replacement bookkeeping are inside the timed boundary.

Contained CPU sample20 executed only llrender_tests, not the viewer; native
fixture and sampler exit0. It captured only41 main-thread samples dominated
by GPU waits/readback and is insufficient to attribute cold compilation. The
new hidden no-render compilation-stage fixture separates ordinary source,
resource-only promotion, record-only promotion and group-from-completed-record
CPU work. Its separate promotion copies mean those stage totals must not be
summed as a full-path result. Fixture is authored, not executed yet.

SIMPLE transport still used old per-source expansion in20. Work in this
isolated tree now extends private owner-free family0 records, region snapshots
independent of geometry, whole-cohort capture and same-view replacement to
plain opaque passes, with24 actual cold/warm/churn comparisons alongside the
unchanged48 material comparisons. Residency owns cohort API/tests, opaque owns
SIMPLE cull snapshot/ranges, architecture owns producer/capture/actual fixture.
Root owns the new compile attribution inl, documentation and all execution.

PPD197 separately removes the unused argument-resource descriptor prerequisite
from ordinary direct material compilation. Explicit table/SourceRows requests
retain their implementation. Completed producer tuples must still prove all
typed geometry/index/numeric buffers and every active texture role/control
block; optional present descriptors must match. Device qualification remains
in actual encoder bindings; private certificates are not claimed to prove it.
Source-record/native-reader lifetime remains required. No new shader/cache or
generation-owning idle SIMPLE cache is authorized by these changes.

## Plain/descriptor-free coherent freeze for build21

All three owners have frozen their disjoint source/tests. Plain family0 now
uses private owner-free records, one current-region view per group and the same
post-flush replacement transport; fresh model snapshots do not replace geometry.
Its idle strong CPU proof contains no native lease owner. Legacy/PBR direct
records allow absent resource descriptors while retaining matched material
uniforms, typed geometry/index/numeric/texture role proof and native-only GPU
completion ownership. Present optional descriptors must match; requesting one
later preserves a valid existing direct record. PPD198 std::array iteration
syntax is corrected by source inspection, not yet claimed compiled.

The new no-render CPU attribution inl is included in the actual-owner test TU.
Material48 and plain24 cost comparisons are separate hidden cases using the
same complete capture/publication/cull/encode/reclaim/completion boundary and
all pending-span bookkeeping. Both direct arms require zero unused resource
descriptors; explicit resource/table/SourceRows tests retain their own setup.
Root next commits/integrates and executes required21, focused native correctness,
separate cost cases and compilation attribution. No performance acceptance or
default activation follows from this authored freeze. Next global defect199.

## Executed descriptor-free/plain checkpoint

Required21 at3af1013468 fails63.043s at three Catch ternary assertions (PPD199),
doctor/configure0. Test-only3a3c0cd3f0 required22 passes135.660s, static69054
passes. Native cohort1763/7, direct renderer764/1+prior286/1 and earlier bank/
budget/admission/library/region gates pass. Actual producer48/50cases,
10361/10363 assertions: PPD201 fixture routes plain type0 to material admission;
PPD200 plain retry loses valid source after child cache withdrawal. All other
new descriptor-free role/ownership tests pass. The remaining22 native backend
tail (rows7354/9, compact1990/8, resources12379/7 and old/missing libraries) passes.

Material cost22 passes92880/48: all16warm CPU comparisons improve; warm512
61.78–63.22%lower. All16cold cases still2.62–40.53%higher and13/16churn
comparisons slower. Threeview512churn27.88–33.07%lower; oneview512churn
21.80–22.71%higher. PBR512/oneviewchurn GPU median0.175→0.245ms is an outlier
(+40.4%) retained with prior raw evidence; no GPU-equivalence claim.
Plain cost22 passes46440/24: warm51267.09–69.96%lower, threeview512churn
38.30%lower; oneview512cold48.26%higher andchurn13.04%higher. Sevenof8warm
comparisons improve; a tiny-source case is1.26%higher. These results do not
waive the failed transient case or establish viewer FPS.

No-render compile-stage attribution22 passes115236/1: at512 sources, ordinary
source CPU medians Legacy1.090/PBR2.793ms; resource-only promotion1.033/1.259ms;
record-only promotion0.693/0.757ms; group from completed records0.893/0.913ms.
Separate promotions copy their parent twice; do not sum these as full-path cost
or assign all record/group time to copies without further evidence.

PPD200 repair now frozen: SIMPLE's existing weak-transient source can create its
missing owner-free CPU proof at the group boundary without child publication.
Original retry assertions (no child publisher, one group residency entry, real
reader/byte/dependency ownership) remain unchanged. PPD195 exact publication/
revision guards are preserved. PPD201 selects the proper simple API, preserving
matrix/owner/discovery checks. Root next required23/retests.

Pure ALNativeCohortCompilationAdmission owner policy and five tests are also
frozen in llcommon: original pending age, once-per-frame consideration, last
admitted service priority, completed-publication reset and CPU-local credits.
It creates no scheduler and is NOT YET integrated into postSort. Caller must
warm-check, sort eligible owners deterministically and charge checked logical
work bytes; logical byte caps are not wall-clock latency guarantees. Original
whole-engine goal remains ACTIVE; source/ICB experiments and GroupSourcesOFF.

## Required23 pass and exact next implementation boundary

Main experiment source8eb7e37f17c716d8dcbc3d042f7cc9e91a04e4b9 is clean.
Required23 passes817.302s (1435 targets after shared budget header change).
Static69055 passes346ARM64/signatures/imports/source/retained features and seven
diagnostic defaultsOFF. Strict23 scans3585files,905informational,0blockers/errors.
Policy333assertions/19cases and actual producer10807/50 pass with native
validation. PPD200 transient reader retry preserves all original child/group
entry/byte/reader assertions; PPD201 route and PPD199 assertion fixes pass.

Saved pre-layout baseline23: material92880/48, plain46440/24 and compile-stage
115236/1 all pass. Large warm material CPU61.91–65.21%lower/plain67.41–68.97%lower;
singleview cold/churn remains slower. Full summaries/raw logs retain all small
cases and GPU variability. No owner FPS or completed architecture acceptance.
No viewer, profiling of viewer, launch/login or push occurred. The only sampling
target was the explicitly recorded contained llrender fixture.

Source24 is in the isolated cohort tree, based on8eb. Architecture owns
pipeline.cpp, llspatialpartition.h, new pipeline_cohort_compilation_native.inl
and actual producer fixture. Actual owner compilation credit/admission ordering
and PPD202 terrain catch are authored/frozen pending any coordinated glow change;
peer review found no blocker. Residency owns world.h SlotStorage, narrowly
MaterialSource.publish/serializers in world.cpp, pipeline_scene_publication_native.inl
CPU-only slot freezing/static source facts and material-source fixtures.
No overlapping source edits are authorized. Root owns docs/CMake/execution.

Lazy authored slots are an experiment, not yet accepted into the main tree.
Standalone exact-header ARM64/O3 comparison verifies3672→1688bytes but PPD203
unsealed PBR copy/fresh-author regressions. Native alias/copy/move/output tests
are authored. No custom pool or weakened copy semantics is permitted. The next
narrow change removes MaterialSource's whole-batch serialization staging copy
only through immutable cloned slot inputs and nonthrowing final commit. CPU-only
sealing for numeric-disabled published sources must keep required authored input
on allocation failure. PBR glow's0.004 cutoff may move into keyed source facts
only after all callers are proved equivalent; otherwise preserve the rewrite
and contain newly throwing detach. These coordinated source changes are not
frozen or executed at this checkpoint.

Next obtain both final source freezes, review immutable-slot/exception/admission
invariants, integrate a fresh24 source, run required doctor/configure/build/static,
native material/cohort/actual/policy and appropriate broader gates, then compare
ordinary AND cohort complete72-case costs against23 plus raw-authoring risk.
Keep earlier negative evidence, PPD202/203 and GroupSourcesOFF until assessed.
Current root commands are terminal; no build/test process remains running.
Next defect204; original whole-engine goal remains active and incomplete.


## Source24 integration checkpoint and owner readiness

Owner asks whether a useful architecture candidate can be tried. Latest verified
app remains26.4.0.69055 at8eb; it does not exercise disabled GroupSources, source
rows or command banks by default. A build of that state cannot establish the
performance of those mechanisms. The architecture is still incomplete: useful
small/mixed GPU command preparation and fuller pass/dependency scheduling remain
open. Do not present successful narrow costs or another compile as completion.

Source24 integrates actual frame-wide CPU cohort compilation admission, lazy
1688-byte batches with immutable CPU slot snapshots, snapshot-based numeric
serialization, and post-water glow coverage as keyed immutable source facts.
PPD202 terrain allocation containment and PPD204 shared preparation credits are
authored. Shadow dispatch acquires two CPU-local preparation credits through
the existing shared policy; pressure/busy behavior is not independently vetoed.
PPD205 preserves an exact real-reader authored transient when optional CPU
freezing is its sole missing capability and no publisher remains; semantic
revision/newer-publication checks remain intact. Actual-owner fixture pending
final freeze. No executed OOM claim. All new source remains unbuilt at this note.

Required24 script: /tmp/persistent_native_build_24.py. Root integrates the
coherent source freeze into the main experiment, then runs doctor/configure/
build/static and `python3 /tmp/run_source_cohort_correctness_24.py 24 native-correctness-24`. It adds material snapshot
and shared-shadow suites to the existing actual-owner/native gates. Broader
combined checks use /tmp/run_combined_correctness_24.py. Complete material48 +
plain24 costs follow only correctness; retain denied samples and compare the
ordinary path as well as cohort costs against23. PPD203 raw-copy tradeoff stays
open. No viewer launch/login/profile/push; no owner candidate claimed yet.


All three disjoint source owners have now frozen source24. Root read-only
review and git diff --check pass; harness33files passes. PPD205 actual-owner
fixture covers Legacy/PBR normal and thrown-allocation transient fallback,
producer revision changes, newer same-revision publications and replaced weak
reader identity. These tests are authored, not executed. Integrate this coherent
freeze and run required24 next; no source changes are authorized during build.


## Required24 pass and fixture-only retest

Integrated10498116fce3f25f14ddae97b5d3baff7b55585f required24 doctor/
configure/build exit0; build165.029s. Static69057 passes346ARM64/signatures/
source/imports/retained features/defaults. Strict24 scans3587files,905info,
0blockers/errors. Native material2513/6, shadow98414/14, cohort1763/7, renderer
764/1+old286/1, banks1942/5+old84/1, budget333/19, admission311/4, library331/3
+old82/1 and region64/5 pass with native validation. Actual producer passes
51/53cases,11773/11788assertions. PPD206 records all15 failures: the new
forward/glow tests expect zero observer calls although world() unconditionally
observes once before its cache lookup. Rendering/fallback/cutoff/freshness
assertions pass. Correct only those two expectations, then required25 and
actual-owner retest; do not change production observation behavior.

Full original architecture remains active and partial; no viewer launch, live
profile, push or owner acceptance. Reject the proposed tiny command-bank scratch
cleanup as a priority: source review identifies two allocation calls on hits,
but evidence does not show they explain the large CPU/GPU bank regressions.
Keep attention on complete usable production paths rather than nominal API use.


## Passing source25 gate and next GPU cohort boundary

Clean7b49e2ea9a3fe7097aad331681dfb06de5ac5792 required25 build29.420s;
static69058 passes. Fixture-only changes leave almetal_tests hash identical
to24; its material/shadow/cohort/renderer/resource/row evidence is retained as
24, not falsely rerun. Actual producer11788/53 and budget333/19 pass at25.
Combined25 passes88/88CTest entries165.144s,10specialization processes,
3archive policy processes and the private decoder corpus. No viewer executed.
Complete material/plain cost25 gates now follow; keep PPD194/203 acceptance open.

Next original G4 implementation is a retained GPU cohort publication, not the
rejected per-source facade or a scratch-allocation microchange. SourceCommands
alone cannot remove SourcePages per-view N-input capture/validation. Source26
therefore changes the actual boundary: once-per-generation GPU row/table/index
membership publication from an exact completed cohort, fresh bounded native-only
FrameViews with one region/global update, and cached native ICB execution split
at existing query/raster/cull runs. No per-view reference/request/key expansion.
Per-page content stamps and exact table/context proofs must detect in-place row
changes; allocation generation alone is insufficient. Idle caches must not hold
CPU source graphs, C++ generation pins or real reader tokens. Actual commands
retain native-only views/banks through completion. Default direct paths stay
unchanged; no gain or full-architecture completion claim precedes cost evidence.

Exclusive source26 ownership in the isolated cohort tree (main7b49 frozen):
residency owns SourcePages/cohort helpers+fixtures and minimal immutable native
membership interface; opaque owns SourceCommands+fixtures, prepareCohortForCommand
and executeRange; architecture owns pipeline integration, world.cpp renderer
loops only and actual-owner/renderer fixtures. Material serializers/world.h are
frozen. Agents coordinate exact helper APIs before dependent edits. Root owns
docs/CMake/integration/all executions. No builds/tests/viewer/profile/push by
agents. PPD183/191 prior regressions are retained; this is their genuinely new
measured-boundary repair attempt, not evidence of a speedup.


## Complete source25 cost outcome

Material cost25 passes94720assertions/48pairs, plain47360/24pairs; source
7b49, no viewer. All setup/sample rows retained, including the synthetic idle
frame budget and complete CPU preparation/capture/adopt/encode/reclaim/settle
boundary. There are0denials/duplicates in these one-group idle cost cases;
admission-denial behavior is correctness-tested separately, not inferred as a
performance result. Warm material wins14/16 and plain7/8; cold wins3/16 and2/8;
churn wins2/16 and2/8. Keep all small cases and GPU variability.

At512rows, Legacy1view warm0.7739→0.3221ms (−58.4%),3views2.4553→0.9966
(−59.4%); PBR1view0.7283→0.2818 (−61.3%),3views2.3694→0.8570 (−63.8%).
Plain1view0.6660→0.2160 (−67.6%),3views1.9511→0.6272 (−67.9%). But1view
cold adds24.4–41.1% and1view churn11.5–29.5%. Multi-view churn is28.9–38.5%
lower. These are contained native preparation costs, not whole-frame FPS.

Separate before23/after25 ordinary-path medians do not isolate layout causes:
large Legacy cold−4.1%/+3.7%(1/3views),PBR+3.5%/+1.3%,plain−0.8%/+0.3%.
Largest absolute ordinary512 increase is Legacy3view cold+0.139ms; large PBR
ordinary churn is+4.5–5.8%, warm−0.3/+5.9%. Small-case percentages reach84%
from tiny absolute durations; preserve raw samples and PPD203 tradeoff rather
than treating smaller sizeof as acceptance. Native suite verifies sizeof1688
and copy/alias semantics. GPU paired medians range−7.1/+10.2%material and
−8.8/+7.0%plain despite unchanged ordinary shaders; no GPU equivalence claim.

Receipts: actual-direct-{cohort,simple-cohort}-costs-25.json, their summaries,
and each -versus-...-23 comparison in the private evidence root. GroupSources
remainsOFF; rows/banksOFF after earlier losses. Source26 retains CPU cohorts and
adds a genuine optional native GPU publication boundary, with producer-provided
cold typed texture resolution from exact NativeTuple allocations. No alias of
NativeTuple's own controlblock may fabricate a typed allocation. Commands retain
no CPU source/LL graphs. Owner full-viewer acceptance remains pending.


Next exact execution after source26 freeze: inspect all helper/renderer diffs,
verify minimal shared API agreement and native-only lifetime guards, commit
coherent source locally, FF main experiment and run a fresh required26 (clone
/tmp/persistent_native_build_25.py with receipt26). Run SourcePages/cohort/bank/
renderer/actual-owner native gates with validation; new hidden
[.native-gpu-cohort-cost] compares ordinary/direct/GPU3arms at actual cold/warm/
churn boundaries, including denials and no false GPU-hit labels. No source26
implementation is built/tested yet. All root source25 build/check/cost handles
are terminal. Agents continue source26 in their exclusive files; root owns
docs and next integration. Mandatory nextPPD207.


## Source26 in-progress integration review

Previous goal turn made verified progress: required25/static69058, all native
and broader gates plus complete72costs. Main HEAD137c0bec64 is docs-only beyond
built7b49. Source26 remains unbuilt in the isolated cohort tree. No root
build/test/cost process is running; three exclusive source agents are active.

Shared header contract now includes freezeCohortForCommand, private immutable
numeric CommandMembership (ordered calls and cull runs), prepareCohortForCommand
and executeRange. Cold typed texture resolution comes only from the exact
NativeTuple resource controlblocks, with raw pointer/extent checks. The
completed cohort stores only a cold resolver function pointer and weak native
owner identity; no direct-default per-row resolver work. Actual implementation
file is almetalworldcohort.mm, not .cpp. Root prepared required26/broad26 and
/tmp/run_gpu_cohort_costs.py wrappers, none executed.

PPD207 source review registers pre-admission eviction of usable command banks.
Opaque owns staged publication repair and A/B-deny/A-hit fixtures for both old
and cohort APIs, then NEW alpipeline_gpu_cohort_cost_native.inl at the existing
actual fixture's EOF. Architecture retains the existing .cpp include plus
renderer/actual correctness; it prequalifies whole cohorts and aborts after a
later ICB range failure rather than replaying emitted draws. A default-unused
one-shot native execution seam supports deterministic coverage. Residency owns
SourcePages/cohort helpers and native-only page/table content proof. No new
shader algorithm, renderer backend or app-default activation. Next defect208.


## Coherent source26 freeze

All three exclusive owners froze their code; root bounded source reviews and
git diff --check pass. Source26 remains unbuilt/unexecuted. Added cohort-native
GPU row/table membership, weak exact ownership/page-content proofs, cached
indexed banks with query/cull range execution, and actual renderer/producer
integration. SIMPLE and unsupported cases retain ordinary direct Metal.
No app defaults activated and no new shader algorithm or backend introduced.

PPD207 stages command-bank publication before cache mutation; PPD208 bounds
unique retained root MTLTexture allocation footprint by hardware advice and
prunes expired bindings at FrameData.beginFrame even with the GPU flagOFF;
PPD209 stages cache allocation before eviction. New deterministic native cases
cover query/pixel/depth/order, reentrancy/admission/failure, pending completion/
CPU graph retirement, in-place row changes, cache pressure and failure.
Foreign-device execution remains unavailable on the default-device-only fixture;
exact native device guards are retained. No executed OOM claim.

New actual cost hidden tag[.native-gpu-cohort-cost] uses independent ordinary/
direct/GPU arms, numeric material caches, FrameData/renderers and shared actual
CPU/GPU budget policy, complete cold/warm/churn count1/4/16/512 and1/3views.
Count1/failure/denial fallbacks and acceptedGPU counters remain explicit.
CPU descriptions and native leases retire inside measured boundaries. Existing
two-mode cost tags remain unchanged. Root next: commit this freeze, FF main,
python3 /tmp/persistent_native_build_26.py; static native26, focused native
correctness then fresh cost26. Source25 results remain prior-source evidence.
Keep full architecture active/partial and all owner acceptance open. Next210.


Required26 clean e7eba56957 doctor/configure pass; build fails58.746s solely
at new cost CHECK logicalOR decomposition (PPD210). Strict26 passes3587files/
905info/0blockers/errors. Root parenthesizes the one complete expression; no
production source change. Required27 follows from the corrected pin, then
static/native/cost gates. Prior failure retained; no source26 success claim.


## Passing required27 and bounded native corrections

187f1026768d172868b334e831032b8f8a26bfeb required27 doctor/configure/build
pass (build135.221s), static69061 passes native features/346ARM64/signatures/
source/imports/defaults. Native page1547/12,material2513/6,shadow98414/14,
cohort1763/7 pass. Renderer3/4 (2216/2217) fails only mixed reference fixture
missing CPU owner (PPD211). Banks8/9 (3195/3196) fails only query fixture
missing attachCommand (PPD212); PPD207 denial/capacity/lifetime cases pass.

Root collected remaining native27 gates before one rebuild: old renderer400/1,
oldbank84/1,budget333/19,admission404/5,library331/3+old82/1,region64/5,
actual producer12397/53,rows6538/3,compact1990/8,resources12379/7,oldresources
4721/1,oldrows283/1,nolayout13/1 all pass. Actual producer GPU cohort parity,
queued COW/CPU-owner retirement and query boundaries pass. Native API/GPU
validation enabled. Keep both failed suite receipts, no all-pass27 claim.

Root repairs only fixture ownership and query-command association, with
production validation/rendering unchanged. Required28 +corrected renderer/bank
retest +full broader28 follow, then actual3arm cost. Source27 unaffected native
evidence is retained as27, not claimed as a rerun. Architecture remains partial
and controlsOFF; no viewer launch/profile/push. Next213.


Required28 fails19.320s at the root PPD212 repair: attachCommand is private
(PPD213). Root checked its implementation but missed the header restriction.
Replace only the low-level fixture store with an explicitly owned shared query
buffer/native pass attachment, following existing indexed-join native fixtures;
read real counters only after the exact producer command's commitAndWait.
Higher-level renderer tests still exercise production VisibilityResultBatch
lifecycle. No production API/validation change. Required29 follows.


## Source29 gates and coherent source30 continuation

06d8d4484d278d38c63c912f5f71a15281316ae1 required29 build28.115s and
static69063 pass. Corrected renderer2331/4 and banks3348/9 pass; unaffected
production native27 evidence remains source-pinned. Broad29 passes88/88CTest
139.435s,10native specializations,3archive processes and private decoder corpus.
PPD211–213 fixture failures/attempts are retained; production validation/API
never relaxed. No viewer launch/profile/push.

New3arm cost29 executes all144summary rows/2016raw samples but fails128 of
140088assertions, exclusively four steady-state-zero-work expectations on
warm condition sample0 (warmup). PPD215 corrects only measured-sample guard;
no pixel/query mismatch. Timings provisional, not accepted: warm512 GPU CPU
about0.060ms versus direct0.256–0.278ms(1view), but small4 GPUCPU about0.039ms
versus direct0.033–0.034ms. Full settle/GPU costs remain material and mixed;
no activation follows the failed gate or its warm CPU improvement alone.

PPD214 source audit confirms early aged CPU compilation can starve later
GPU metadata under sustained pressure. Clock audit separately confirms actual
CLOCK_THREAD_CPUTIME_ID/required_main_cpu_ns, not blocked GPU/presentation wall
time; no timing rewrite needed. Frozen fix adds fixed two-category recent
denial/service histories to existing budget, CPUAdmission selects compilation
kind, native callbacks retain their default. Idle quotas/hard caps/leases
unchanged. Five authored repeated-frame models plus read-only peer/root review;
execution pending. No task graph, request identity registry or resource pins.

Source30 also uses existing FrameData scalar arena for cold/warm GPU cohort
views: one padded288B publication, native buffer+offsets and weak exact origin
completion only. No FrameData/rawCppslice retention. Wrong/completed/abandoned
commands reject before binding; standalone null-FrameData views retain prior
immutable208B behavior. Wrapper join/cache state is flushed/invalidated before
native view-buffer writes. Three native cases cover output/allocation reuse/
pending/abandon/recycle/parent expiry. Root supplies FrameData at the single
renderer callsite and adds throttled arena counters/actual-cost assertions.
No shader/backend/default activation. Coherent source freeze is reviewed but
unbuilt; required30 (shared budget header may rebuild many targets) next, then
native/broad/static and corrected3arm costs. Next defect216.


## Source30 verified checkpoint and owner candidate boundary

Source `8691dc3430b65637219a6838fd6171826d3c9263`, version
`26.4.0.69064`: required30 doctor/configure/build all exit 0; build took
839.090 seconds. Static bundle validation passes all 346 ARM64 images,
signatures, source identity, imports, retained media/voice and feature markers.
Strict source30 passes. Evidence is under
`artifacts/metal/apple-first-persistent-native-scene-20261114/`, receipts
`frozen-source-build-30.json`, `static-native-30.json` and
`strict-source-30.json`.

All 20 contained native processes in `native-correctness-30.json` pass with
Metal API/GPU validation. In particular, shared frame admission passes 1690
assertions in 24 cases (PPD214), and SourcePages passes 1926 in 15 cases,
including the three new completion-safe arena cases. Actual producer passes
12397/53, renderer 2331/4, command banks 3348/9, and shadow 98414/14.
`combined-correctness-30.json` passes 88 CTest cases, ten native
specialization processes, three archive processes and the private decoder corpus.

The corrected three-arm cost gate passes 140840 assertions in one case:
144 summary rows and 2016 retained raw samples. PPD215's initial warm-up is
still recorded but excluded from measured steady-state assertions. The receipt
is `actual-gpu-cohort-costs-30.json`; derived comparisons are in
`actual-gpu-cohort-costs-30-summary.json`. These are controlled native-workload
observations, not viewer FPS results or a causal comparison with source25.

For one warm 512-source Legacy/PBR group, ordinary submission CPU is about
1.0 ms versus GPU cohort 0.055 ms; completion/query settlement is about
1.51–1.59 ms versus 0.61–0.64 ms. GPU time rises from 0.124–0.135 ms to
0.193–0.206 ms. Cold and churn cases remain slower. At four sources and one
view, warm CPU is nearly unchanged, settlement worsens 7–13%, and GPU time
rises roughly 82–89%. Across non-scalar cases GPU CPU wins 11/12 warm
comparisons against ordinary submission, but none of 12 cold comparisons.
All requested GPU cohort draws in those cited cases were actually accepted;
the differences are not silently measured fallback. This evidence rejects
unconditional activation, despite the large retained-group CPU win.

A new many-small-group fixture is authored but not yet built. PPD216 registers
a peer-found cost-boundary omission: its pending/candidate containers were
cheaper than production postSort. The author is correcting the timed map,
per-group vectors and full candidate copies before root includes it in the
existing actual-producer test translation unit. Source31 is intended to be
test-only; no further renderer policy or default activation is implied.

Product advancement: shared admission now has executed cross-phase progress
coverage; native cohort view constants use completion-qualified shared arena
storage; actual retained GPU consumption has measured large-group CPU gains.
Critical remaining work: small/many/changing group selection, visibility and
animated-source coverage, full pass dependency boundaries, and a separately
staged enabled combined candidate. All seven experimental controls remain off.
The original whole-engine goal and owner runtime acceptance remain partial.
No viewer launch, login, live profile, push or owner-app replacement occurred.

Source31 freeze adds only the hidden many-small fixture and its EOF include,
plus evidence documentation. PPD216 timed containers now match the production
allocation/control-copy boundary; root inspected both actual postSort and
fixture. Required31 and the hard-capped native cost execution follow. Source30
production gates remain prior-source evidence, not a claim of rerunning them
at31. Harness33files and whitespace checks pass before commit.

A bounded candidate-readiness audit corrects an overly broad interpretation of
all seven optional controls being off. Always-on production foundations already
include producer publication, owned CPU packets/native completion retirement,
shadow preparation on bounded shared credits, probe-group admission,
decoder/upload downstream limits, CPU frame planning/progress fairness and
shared-memory dirty-range/COW writes. Those do not depend on the optional
WorldPreparation flag. A candidate can therefore exercise real integrated work
without all optional paths, although it does not implement the full architecture.
A direct-cohort experiment requires GroupSources on and NativeVisibility and
DepthPrePass off. The optional GPU-bank experiment additionally enables banks;
its raw fallback can also activate the older per-source row path, so it must not
be described as exclusively the fast retained-cohort path. Prior regressions
remain relevant to candidate selection, and acceptance is still partial.


## Source31 executed and separate owner preview prepared

Source `14837e631a8e96492bb3037de24dd1c1a8030ac5`, version
`26.4.0.69065`: doctor/configure/build exit0 (29.063s build), static native
bundle checks pass all346ARM64 images, signatures/source/features/imports.
Strict source scans3587files with905informational rows,0blockers and0errors.
Source31 changes no production rendering code relative to passing source30;
its existing broad/native evidence remains explicitly source30.

`actual-many-cohort-costs-31.json` passes1871896assertions/1case in9.106s,
24summary rows and2016raw samples. All pixel/depth/query parity checks pass.
The8 scenarios mix64/256Legacy/PBR groups,2/4sources/group and1/3views;
progressive first-publication loading,64warmups and12measured frames remain
visible. This is not asset download simulation or a viewer FPS measurement.

Direct cohorts improve measured CPU21.8–76.0% and completion/query settlement
3.4–67.0% versus ordinary submission in all8scenarios. GPU time ranges−3.0 to
+2.4%. GPU-bank mode is not robust: settlement loses28.3–148.9% versus direct
cohorts in all8scenarios, with GPU time26.6–104.6%higher. Repeated cache work
and fallback are observed even after warmups; PPD217 records this next concrete
implementation boundary. Prior direct cold/churn losses remain relevant.

A separate owner preview is staged under the private evidence directory at
`owner-preview-69065/Alchemy Metal Alpha G2G5.app`. Receipt
`owner-preview-69065.json` proves8272matching bundle entries, repeated ARM64
and strict signature checks, exact source and no viewer launch. Source and
owner-passed apps are untouched. Included `Launch Architecture Preview.command`
is owner-operated and syntax-checked only: it enables retained direct groups,
forces NativeVisibility/DepthPrePass and the other optional experiments off,
and keeps command banks off. This makes the chosen experimental path concrete
without claiming an accepted default. Opening the app alone uses shipped off
defaults plus any runtime overrides. Runtime activation is not agent-observed.

This preview is meaningful for an intermediate whole-viewer assessment of the
active foundations and direct retained consumption. It is not the full original
architecture or proof of FPS improvement. Native visibility before per-source
CPU discovery, animated membership, complete pass dependencies and robust GPU
command selection remain partial. The original goal remains ACTIVE. Next work
is PPD217 useful retained working-set selection, followed by the remaining
original architecture boundaries and owner runtime assessment; no launch,
login, live profiling or push is authorized by this handoff.


## PPD217 mechanism narrowed before repair

Independent command/page/root reviews of source31 identify page-wide freshness,
not bank capacity, as the64group rebuild loop. Every source allocation or write
invalidates all cached cohorts on its128row page. Cache pruning releases native
tables and membership; weak row table proofs then expire, and bank pruning sees
expired membership. All473pagefailures in that measured case are admission
denials; cached/live banks stay8–40 and capacity failures are zero. The1044
fallback draws split946source_setup/98bank_admission. PPD218 registers the
precise invalidation defect before implementation. Next repair is row-selective
bounded cache dependencies with unchanged physical COW/readers and warm O(1)
qualification; no larger cache or small-group exclusion. Owner preview69065
is frozen and will not be overwritten. Prior turn classified progress: new
cost evidence, local commits and separately verified owner artifact delivered.


Independent next-boundary implementation is assigned in a separate worktree:
`/private/tmp/alchemy-apple-first-retained-cohort-visibility`, branch
`codex/apple-first-retained-cohort-visibility`, base8159abf7c6. Architecture agent
exclusively owns optional producer-sealed bounds, retained visibility inputs,
existing compute/renderer integration and focused source fixtures there; root
owns docs/CMake/build/test integration. No pages/command-cache changes there.
The vertical slice must avoid warm per-source visibility capture expansion,
preserve fail-visible cold/unknown behavior, exact regional/query ordering and
native completion lifetime. Defaults remain off. It advances retained visibility,
but cannot eliminate same-frame CPU bindings using the current20B indirect
arguments; that larger conditional-command boundary remains explicitly open.
No source integration or build is claimed yet. Source32 first covers only
PPD218 precise freshness and its strict64group warm regression gate.


PPD218 source32 freeze: fixed rows hold only intrusive head pointers. Each
stable cache entry owns an exact-call-count watcher array and one validity bit;
changed(slot) invalidates/prunes only dependent entries before native writes.
All node allocations precede eviction, and exact node bytes count against the
existing8MiB cache metadata cap. Clear/destruction/cache rollback unlink while
Row storage remains live; no CPU source graph or actual GPU reader is cached.
Warm views retain their existing immutable membership/tables and acquire current
physical page/bank reader leases. Old commands retain old pages independently.
Two added native cases cover unrelated-row denial/rewrite, pending old/current
pixels, withdraw/clear/destruction, and64mixed cohorts converging under the
unchanged16ticket shared budget. Root adds measured-only64group no-rebuild/
no-denial/full-bank acceptance to the existing many-small gate;256cases retain
real pressure. Root and independent source/lifetime review find no blocker.
No native execution claimed at freeze; required32 and native/cost gates follow.


## Source32 executed: precise freshness repaired, bank activation still rejected

Exact `3fa382096919c7c50f648b3e5a5a785e184a670f`, version
`26.4.0.69067`: required32 doctor/configure/build exit0 (build34.304s).
`static-native-32.json` passes346ARM64 images/signatures/source/imports/features;
`strict-source-32.json` scans3587files/905informational/0blockers/0errors.
`native-correctness-32.json` passes all20processes with native API/GPU validation,
including pages3644/17, source admission1874/6, banks3348/9, actualproducer
12397/53, renderer2331/4, frame budget1690/24 and shadow98414/14. Old-library,
query/lifetime, compact inputs and resources gates also pass. Broader CPU/tool
source30 evidence is retained as prior-source; unchanged generic tools were not
retested redundantly. Native cost process observations record no running viewer;
no viewer was launched or profiled.

`actual-gpu-cohort-costs-32.json` passes140840/1 in3.975s with all144summary
rows/2016samples. `actual-many-cohort-costs-32.json` passes1872424/1 in8.943s,
24summary rows/2016samples. Both summaries are retained beside their raw logs.
Strict64group measured assertions now prove full requested bank use, all cache
hits and zero page/bank rebuilds, evictions, admission denials or fallback.
In64groups/2sources/1view, bank CPU falls0.766479→0.187416ms, complete/query
settlement1.32296→0.932375ms, while accepted bankdraws rise492→1536of1536.
All measured rebuild/failure counters go to zero. This is contained mechanism
evidence, not an in-world FPS improvement.

The fitting64group GPU cost is still worse than direct cohorts:2sources/group
CPU+16–17%;4sources/group CPU−20–24%, but completion/query settlement loses
24–105% and GPU time roughly doubles. Across all8many-small scenarios GPU
settlement loses23.6–105.9% versus direct and GPU time rises24.5–136.1%.
True256group pressure still produces192bank builds/12frames and only25–26%
bankdraw fraction. PPD217 therefore remains open. Larger warm512source CPU
wins remain, while3arm cold/churn and tiny groups still lose. No source32 bank
activation or replacement of the owner69065 direct-preview artifact follows.

The next priority is retained per-pass visibility/command data without per-group
publication duplication and stable dirty-generation admission. This is a source
cost/ownership boundary, not permission to increase caps or hide small groups.
Independent retained-visibility work remains unbuilt in its separate worktree.
Architecture owns productionsource/actual-owner tests; opaque now exclusively
owns existing `indra/almetal/tests/almetal_world_visibility_test.mm` there.
Root owns integration/builds/tests/CMake/docs. A new native test binary, source
freeze and exact evidence are required before enabling that path. Original
whole-engine architecture/owner runtime acceptance remain partial and ACTIVE.


Independent ordered-pass publication backend is now assigned in
`/private/tmp/alchemy-apple-first-native-pass-publication`, branch
`codex/apple-first-native-pass-publication`, baseddd872c0654 (tested PPD218).
Residency exclusively owns SourcePages.h/.mm and its existing native fixtures;
no renderer/Commands/CMake/docs/build changes there. The intended API accepts
ordered homogeneous cohort spans, caches one native composition under existing
row/metadata/native-asset caps, publishes one matrix/global block and produces
one aggregate immutable membership. Warm work may scale with group count, not
per-source expansion. Native tuple closure, precise row invalidation, region
mapping ambiguity, failed staging and pending reader lifetimes remain explicit
acceptance requirements. Single-cohort behavior must remain covered. It is
unbuilt and not integrated; root must wire actual renderer/costs after the
independent retained-visibility slice freezes and passes.

Source review also observes linear owner scans inside retainUntilCompletion;
G separate FrameViews/Batches can multiply comparisons, plus repeated source
slot binding per group. The share of measured CPU cost is unproved. Aggregating
existing owners/metadata may remove those calls; no generic retainer rewrite or
GPU speedup is inferred. Query boundaries can still require Gexecute ranges,
and existing source-row shader indirection/GPU cost remains a separate limit.
The original goal stays active; the source32 cache repair is one concrete
completed engineering step, not a replacement definition of the architecture.

Pre-freeze retained-visibility review finds PPD219: bounds-only optional promotion can reject a valid current-reader transient after child-cache withdrawal. Registered before extending only the exact transient fallback guard; revision/newer replacement and195/205 protections remain required. Implementation/native execution pending; next220.

PPD220 pre-freeze retained consumer review: generic range/MVP qualification cannot distinguish a different exact cohort/native generation. Registered before a cohort-specific weak-identity qualification API and swapped-generation controls; required direct fail-visible rendering and native-only retirement remain required. Next221.

PPD221 pre-freeze native row-cap review: new bounds buffers charged logical48B/row despite larger actualMetal allocation footprints. Registered before immutable allocatedSize debt/retirement accounting and small-buffer physical-cap checks. Existing per-view65536row limit still bounds one attempted allocation. Next222.

PPD222 pre-freeze G5 integration audit confirms new cold visibility rows/dirty raw bounds bypass shared FrameData publication admission. Registered before passing optional FrameData through encode and coalescing new metadata work on the existing callback; direct fail-visible denial, zero warm tickets and exact command credit retirement are required. Raw fallback under GroupSources must avoid optional per-source bounds discovery. Next223.

Root backend review registers PPD223 before repair: new persistent multi-region composition mappings are omitted from charged publication writes. Existing-table and valid-row reuse can leave charged0 despite a new retained map allocation/copy. Add explicit persistent mapping write charge and mapping-only denial/warm/credit coverage; required transient per-view buffers remain separate. Pass backend unfreezes only for this narrow repair. Next224.


Ordered-pass backend source33 is frozen/reviewed for a separate first build,
not yet actual renderer consumption. Only SourcePages.h/.mm and existing source
page tests change: single API delegates, exact ordered composition cache,
O(G)warm qualification, one regional/global payload and aggregate command
membership with logical→bank ranges. Existing row/native/metadata/asset caps
remain; cache entries are compositions with underlying group-reference census.
Five new native cases exercise unchanged Commands.prepare/executeRange output,
regions/query boundaries,64/256group metadata, ambiguous mappings, cache OOM,
pending old/new tuples and PPD223 mapping-only shared admission/retirement.
Root/independent reviews pass; required33/native execution next. Renderer and
complete cost integration remain required after retained visibility freezes.
No Source33 performance improvement or combined candidate is claimed.

PPD224 source review registers broad optional visibility allocation catch before repair: optional metadata bad_alloc currently reaches clear-required-capture handling. Narrow local disable/fail-visible must retain raw/ref/cohort geometry and queries, with actual-body fault injection. Required geometry/source failures keep old incomplete policy. Same220 guard also distinguishes raw arguments from cohort spans in both consumer directions. Next225.


## Source33 backend gates pass before renderer consumption

Exact `f78f994b643c6c232a63b16727ff6b7eda115765`, version
`26.4.0.69073`: required33 doctor/configure/build exit0,163.914s/232targets.
Static passes346ARM64/signatures/source/imports/features; strict scans3587files,
905informational/0blockers/0errors. Native33 passes all20processes withAPI/GPU
validation, including SourcePages5364/22 (five ordered-pass cases), admission
1993/7, banks3348/9, actualproducer12397/53, renderer2331/4, shadow98414/14,
framebudget1690/24, resources and old-library controls. Receipts follow the
same `frozen-source-build-33`, `static-native-33`, `strict-source-33` and
`native-correctness-33` prefixes in the private evidence directory.

Existing-single consumers also pass3arm140840/1 (3.931s) andmany1872424/1
(8.838s), with bothraw logs/derived summaries retained. No viewer process was
observed at the cost start. Direct cohorts still win all8many-small CPU and
settlement comparisons; current single-cohort GPU banks still lose settlement
24.8–120.9% andGPU24.2–145.5% versus direct. These costs do not exercise new
renderer composition grouping, so they establish compatibility/regression
coverage, not a new aggregation speedup. Required main/probe renderer wiring
and complete cold/warm/churn/many costs remain before any bank activation.

Retained visibility remains source-owned in its separate worktree; sparse
qualifiedraw logical indices now replace optional disabled raw/ref placeholders,
while cohort spans remain O(G). PPD219–222/224 repairs and fixtures are authored
there but unbuilt/unintegrated. Root owns all native execution and renderer
composition wiring after that full slice freezes. No owner app replacement,
launch/login/live profile/push or full-goal completion occurred.

PPD225 pre-execution cost review: requested50% outside atone source actuallyselects0sources. Keep requestedlabels andreport actualoutsidecount/effectivefraction, includingrounding, before accepting evidence. Production/nativefixtures stayfrozen during this fixture-only correction. Retained visibility runtimeflag newlytoggled ontooldplaincohorts currently staysdirectuntilproducerrebuild; freshlaunchflagON publishesbounds. This limitation is retained, not a claim of fully GPU-driven discovery/CPU-binding elimination. Next226.

PPD226 final root compile-dependency review finds new visibility implementation accessing forward-only WorldSimpleBatch/FrameData declarations. Register before including almetalworld.h directly; no backend PCH is present. Required34 compilation follows; no executed compile failure is claimed. Next227.


## Retained visibility source34 freeze and verification boundary

The complete15file slice in the retained-cohort-visibility worktree is frozen,
with source/native-fixture/actual-owner/cost peer reviews. Root adds only the
PPD226 direct world.h include. New CPU optional bounds are not material-key
bits; retained views keep compact qualifiedraw logical indices and one matrix
per cohort. Existing same-command GPU arguments preserve order/query offsets;
exact current-cohort identities/MVP are required at consumption. Credits flow
through existing FrameData admission before cold writes, with physical native
row debt distinct from logical upload work. Optional metadata OOM disables only
visibility. PPD219/220/221/222/224 source guards/native cases are authored;
PPD225 reports actual offscreen geometry for integer-rounding scenarios.

New included cost tag is `[.native-cohort-visibility-cost]`, correctedfile
SHA474be95c5baf7a435a58b0cca566af07af6bd59eca47130d03f0d54a4e86bdf8.
It retains504summary rows (168configuredscenarios×3arms),12measured samples,
small2/many64warmups, full CPU capture/admission/retirement and GPU/query settle.
No pre-seal/source expansion is hidden. Nativefixture finalSHA is
f9145920588e5075ee5fc45d6661dac37313aa6bd36d3b40b69637ff234ad401.
These are unexecuted authored gates at freeze, not accepted results.

Source34 first combines the visibility slice with validated backend33, without
renderer composition wiring yet. Required34, all existing native suites plus
world-visibility, actual-owner and new cost gates follow. Source settings remain
OFF. Experimental retained visibility needs GroupSources1/NativeVisibility1/
DepthPrePass0 and banks/SourceRows/CompactInputs/resource tables/join/preparation0.
It is frustum culling, not occlusion or GPU conditional command generation; CPU
per-source direct bindings remain. Oldplain cohorts on a mid-session flagtoggle
staydirect until producer rebuild; freshlaunchON publishes required bounds.

One independent6line shader hypothesis is frozen in
`/private/tmp/alchemy-apple-first-constant-view-metadata`, basedf78f994b64:
region array/map/local reference useconstant instead ofdevice address space in
two source-row vertex shaders. No geometry/layout/reflection/math changes.
Source33 exactmetallib control is saved with receipt `source33-shader-control.json`.
Do not integrate that experiment into34: preserve34control and test separately
onlyafter visibility gates, with parity and actualsame workload GPU costs.
Source facts show ordinaryshaders also manuallyfetchgeometry; a fixed-function
vertex-fetch explanation of current regressions is unsupported. Additional
source-row pointer chains are observed, but their measured cost share isunknown.


Required34 at60a28bf449 passes doctor/configure but fails59.265s compiling
MaterialCacheScope's savedconst device pointer back into mutable pipeline state.
PPD227 registers before root changes only that saved-field type. No production
API/assertion change, no native run or successful34claim. Required35 follows;
failed34receipt/log preserved. Next228.

PPD228 registers legacy scalar SourceRows autoactivation from bank flag underGroupSources before capture-policy repair. Root exclusively owns pipeline.cpp in newordered-pass-consumption tree; architecture ownsworld.cpp/actualfixtures, opaque owns onlycohort_renderer_test.mm there. Rawfallback mustretainordinarynativegeometry without implicit scalar metadata, whileexplicit SourceRows andoldnoncohortbank control semantics remain. Next229.


First visibility35 complete cost passes5672016/1,504summary rows/14064raw
samples in68.816s. Data shows no warm uploads/expanded per-source members or
publication/fallback work, but cheap3-index geometry generally loses. PPD229
registers performance rejection before further selection/dispatch work: only
5/168CPU and4/168GPU comparisons beat direct; many warmactual50% loses34–72%
settlement/123–184%GPU. This does not measure heavier indexed meshes or viewer
FPS. Control remainsOFF; preserve data and investigate useful compute granularity
and geometry workload rather than unconditional activation.


## Source35 executed checkpoint and isolated shader hypothesis

Previous owner-status turn was no implementation progress; this continuation
resumes concrete source/testing work. Exactafe694394ba006676666d80488b531e1c39543e0,
26.4.0.69076, required35 doctor/configure/build exit0 (136.531s). Static passes
346ARM64/signatures/imports/source/features/defaults; strict scans3587files,
905informational/0blockers/0errors. All21 native correctness processes pass
with API/GPU validation, including visibility2506/9, pages5364/22,
admission2248/8, actualproducer14269/58, renderer2331/4 and shadow98414/14.
Required34 fixture failure remains retained under PPD227.

Cost35 passes unchanged3arm140840/1, many1872424/1 and new visibility5672016/1;
visibility504summary rows/14064raw samples span168configuredscenarios, with
requested versus actual offscreen counts explicit. Warm retained visibility has
zero row uploads, expanded members, new native allocations, publication tickets
or fallback, yet cheap3-index geometry generally loses; PPD229 remains open.
Heavier-mesh coverage and one batched multi-cohort dispatch are now independently
assigned; no speedup or viewer FPS claim follows correctness success.

Before any shader change, source35 test/build/viewer metallibs are verified
byte-identical and copied to private source35-shader-control with SHA receipt.
Next isolates only six constant-address-space qualifiers in the two source-row
vertex entries. Original1eb44bbd deferred/materialV.glsl consumes immutable
matrix uniforms for identical position/normal transforms; native ordinary
material vertex uses constant numeric state with device geometry. The retained
region/map mechanism is new, with no original GL equivalent. Changing only
read-only region/map address spaces must preserve layout, source IDs, geometry,
math and shader/library compatibility. This is a PPD217 performance hypothesis,
not a proved explanation; execute native parity and the same3arm/many costs
before retaining it. Renderer grouping remains separate and unintegrated.

Root owns docs/CMake/pipeline integration and all execution. Architecture owns
world.cpp and actual producer/cost fixtures in ordered-pass-consumption; opaque's
renderer fixtures there are frozen. Opaque now owns test-only heavier visibility
mesh coverage. Residency implements only batched retained-cohort dispatch in a
new isolated tree; no coarse classification or renderer edits in that slice.
Owner69065 app/launcher, canonical branch and all prior evidence remain untouched.

PPD230 records the newly authored heavier-grid sample alignment trap before
accepting execution; phase correction and non-empty depth coverage are required.
Original cheap geometry tag/data are preserved, not replaced by favorable cases.


## Source36 shader hypothesis rejected; heavier visibility coverage freeze

Source4e1daf873f,26.4.0.69077: required36 doctor/configure/build exit0
(32.505s), static346ARM64/signatures/imports/source/features/defaultsPASS,
strict3587files/905informational/0blockers/0errors. All21 native correctness
processes pass with the same assertions/cases as35. Actual3arm140840/1
(6.010s) andmany1872424/1(8.805s) pass all parity/accepted-work assertions.

Saved35-versus36 comparison shows no consistent shader benefit: across48GPU
cohort configurations medianGPU change+0.41%, range−55.33..+108.39%; warm512
one-view Legacy/PBR GPU rise108.39%/57.65% while other views differ. Many-small
medianGPU−1.18% spans−26.06..+2.78%. CPU also changes despite shader-only code,
so these sequential-run differences do not isolate shader causality. Retain
raw samples and do not claim the qualifier caused every observed difference.
Without a consistent demonstrated advantage, revert only the six qualifiers to
the exact35 shader control before subsequent renderer/visibility integration.
PPD217 stays open; compatibility passing alone is not performance acceptance.

Source37 adds only heavier visibility test coverage on the unchanged35 shader/
visibility production control. Existing504cheap3index cases remain. New hidden
mesh tag runs144arm summaries/48comparisons:1x512Legacy/PBR768uniqueindices,
64x2mixed3072,256x4mixed768;1/3views,cold/warm/churn,0/actual50%outside.
Real LLVertexBuffer ownership/publication, complete dirty bounds/capture work,
shared admission and retirement, fallback and MRT/depth/query parity remain.
Logs include actual candidate vertices/indices, payload bytes and summed native
wrapper extents explicitly distinguished from physical unique allocation.
PPD230 mesh phase/non-empty depth assertion prevents empty-output parity.
No production strategy or culling selection changes in this coverage commit.
Native mesh execution is pending. Root and independent review retain exact
frozenfile SHA3125403a560101b75c6afe90238be9123cb3c0ea11e2adf68c4851264442d50f.


## Source37 heavier baseline passes; source38 integrated freeze

Exact5c04df69e7,26.4.0.69078, required37 doctor/configure/build0(29.819s),
static346ARM64/signatures/source/imports/features/defaultsPASS. The almetal_tests
executable and builtin metallib are byte-identical to35; llcommon_tests also
matches35. native-binary-equivalence-35-37.json records hashes and scoped reuse
of35native evidence; changed llrender re-executes actualproducer14269/58 and
region64/5, bothPASS with API/GPU validation.

Original cheap504row/14064sample gate passes6068160/1(68.750s), including new
geometry-count checks. New heavier144row/6144sample gate passes5532576/1
(48.156s), including non-clear depth and full MRT/depth/stencil/query parity.
Heavy meshes do not rescue current many-group dispatch: warmactual50% loses
CPU12.29–15.73%, settlement6.20–19.19%, GPU27.48–44.98% versus direct cohorts.
64x2x1/3072indices GPU0.455875→0.629187ms;256x4x1/768 GPU1.66517→2.41412ms.
Warm row uploads/expanded members/new row allocations/tickets/fallback arezero;
this isolates remaining work, not viewer FPS. PPD229 stays open, defaultsOFF.

Source38 integrates two independently frozen mechanisms, not shader speculation:
ordered world renderer compositions and batched retained visibility. Existing
banks/visibility exclusivity remains explicit. Renderer chunks split only on
family,4096call capacity or actual same-region176B matrix disagreement. One
FrameView/membership/bank per eligible ordered chunk, O(G) warm range proof,
aggregate minimum-run, raw gaps/query/cull offsets and native-only retirement
are tested. No fallback/replay after a bank prefix; first-emission failure keeps
whole-composition ordinary native rendering. PPD228 removes implicit scalar
SourceRows under GroupSources, with mixed raw/cohort CPU/GPU-denial output tests.

Visibility G1/raw kernels remain unchanged. Multi-cohort uses one packed1D
compute dispatch over G96B descriptors/prefixes, exact old8corner predicate,
logical destinations and runtime Tier2/nonzero native GPU addresses. The same
command-owned slot retains indirect row resources and descriptor allocation;
useResourcesRead declares residency. Descriptor physical debt is separately
bounded8MiB across pending/borrowed/idle slots; existing32slots/65536records/
8MiBsource-row limits and shared cold publication admission remain intact.
Four new native cases cover64/256unequal groups/raw holes/queries, pending old/
new geometry/bounds/cameras, denial/abandon/warm0tickets, and true descriptor-cap
rejection/borrowed-reader protection/recovery with shared one-row certificates.
No source CPU expansion, readback, conditional GPU ICB or occlusion is claimed.

Root integration adds composition/mapping census and per-cost dispatch/descriptor
upload/allocation/live/cap counters, retaining original cheap and heavy matrices.
Independent renderer review found no concrete blocker; final batched peer review
and required38/native/allfourcost gates follow. All unexecuted gates arepending.
Owner69065 app/launcher remains untouched; no viewer/profile/push/full-goal claim.

PPD231 retains source38 static failure: old private verifier requires unused
single-cohort symbol dead-stripped after plural renderer consumption. Correct
only the private feature marker and add explicit composition/batched evidence,
then retest exact unchanged bundle under a fresh receipt. No production patch.


Required38 at7246b19e9659a1ded3811efe8ac8ca24a1ab09a2 builds165.919s;
static38 initially fails stale single-cohort marker(PPD231), corrected private
verifier38 additive plural/census/kernel checks pass exact unchanged69079bundle.
Strict3587files/905informational/0blockers/0errorsPASS. Native renderer5046/8,
visibility7521/13 (including packed GPU pointers and physical-cap recovery),
sourcepages5364/22, sourceadmission2392/9 and earlier scopesPASS. Actual-owner
scope fails ONLY new232fixture assumption: zero quota is normalized toone,
so first compile is admitted;14523/14531assertions and58/59cases pass.
Retain failed38 receipt/log. Source39 changes only that fixture's quota setup,
explicitly exhausting/releasing one credit and retaining every denial assertion.
Native tail did not execute after failure, so no all21PASS claim for38.

Source38 production remains frozen for allfour complete cost matrices while the
fixture-only repair is authored in the root integration tree. Do not build or
change the cost checkout until terminal results. Source39 fullnative retest will
follow; source38 cost evidence must remain labeled38, with production source
identity inspected before any reuse. No viewer launch/profile/push occurred.


All four source38 cost processes are terminalPASS:3arm144872/1(3.866s),
many1876456/1(8.553s), cheap visibility6110352/1(68.212s), heavier visibility
5551008/1(47.727s). Raw logs and all four derived summaries are retained.
Every many-small bank scenario now uses100%requestedGPUdraws with zero warm
rebuild/fallback. CPU improves10.60–51.39% versus direct cohorts acrossall8;
GPU still rises87.41–166.90% and settlement8.94–142.52%, so217 remains open.

Batched visibility counters prove one dispatch/view rather than64/256. Warm
metadata now honestly includes96B/group/view descriptor writes (64groups6144B,
256groups24576B), with zero new warm native descriptor allocations. No claim of
zero total warm uploads follows the old zero-row-upload result. Originalcheap
many warm50% still loses CPU8.22–16.73%,GPU18.31–34.88%,settlement2.60–11.20%.
Heavy many warm50% GPU ranges−21.11..+11.73%,settlement−11.41..+0.79%, whileCPU
remains+7.91..+14.55%. Geometry coverage and counters show useful improvement
from removed dispatches, but not universal activation acceptance. Retain exact
source37control for comparison; old-only per-group results are not overwritten.

Root's explicit one-credit232fixture correction is independently reviewed:
credit retirement clears in-flight debt but never refunds current-frame quota.
39changes only fixture/docs, so production source38 remains the cost pin.
Required39/fullnative and broader combined functional gates follow; no default
activation, replacement of owner preview, viewer/profile/push or goal closure.

PPD233 records an unbuilt direct-bank authoring error: scene guard inserted in
factory instead of renderInternal. Move guard before freeze;39product unaffected.

PPD234 registers the direct-bank WIP resource-policy mismatch before repair:
new gross geometry/numeric/table/texture retention must not silently inherit the
64MiB ICB/index ceiling. Preserve that cap, bound/report additional native assets
under existing hardware working-set policy, with pending debt/pruning explicit.


## Source39 verified checkpoint; direct-bound experiment ownership

Exact1630611063,26.4.0.69080: required39 doctor/configure/build0(29.260s),
corrected additive verifier38 staticPASS346ARM64/signatures/source/imports/
features/defaults; strict3587files/905informational/0blockers/0errorsPASS.
Source39 differs from38 only docs and232fixture setup. Byte-identical
almetal_tests and production-source diff prove reuse of14successful38native
scopes; seven39processes re-execute actual-owner14537/59 and all previously
unexecuted tails (GPUrows6538/3, compact1990/8, resources12379/7,
oldresources4721/1, oldrows283/1, noLayout13/1), allPASS with API/GPU validation.
This covers21scopes; it is not a claim that all21were rerun in39.

Broader combined-correctness39 completes with exit0:88/88selectedCTest groups
(145.044s),10additional exact native specializations and3non-skipped archive
policy processes pass (14processes total). Private decoder corpus was not
rerun because decoder/tool code is unchanged. Source38fourcost evidence remains
explicitly labeled38, with production-source identity recorded; no new FPS or
owner acceptance follows these contained gates. Failed static38 and actual38
fixture receipts/logs remain preserved under231/232. No live root job remains.

The next PPD217 implementation is isolated in
/private/tmp/alchemy-apple-first-direct-bound-cohort-banks,
branchcodex/apple-first-direct-bound-cohort-banks, base1630611063. Retain old
row/certificate/publication and current-bank control to isolate the hypothesis.
Direct binding mode is opt-in scene.source_direct_command_banks; root adds
nonpersistent/default0 RenderMetalSourceDirectCommandBanks, gated by bank mode,
in common main/probe capture. Owner69065 app/launcher is unchanged.

Exclusive paths: residency owns SourcePages/Commands headers/implementations
and their existing native tests; architecture owns world.h/.cpp/builtin shader
(three-file source freeze authored, unbuilt); opaque owns existing cohort
renderer/actual-owner/GPU+many cost fixtures. Root owns pipeline.cpp/settings,
CMake/docs/all integration/execution. No concurrent writes across these sets.
Native binding ABI agreed: geometryVS0/1/2/3/8/9/10,region176VS4,globals32VS5,
coldUV/policy96VS6,selectedPBRnumericVS11 andFS0,resourceTableFS1. New shaders
retain arithmetic/varyings and reuse existing fragment functions. Old libraries
missing optional VS fail direct before freeze. PBR numeric ABI must match both
stages. SourceRows diagnostic shaders remain unchanged.

Fixed native view slots are leased per command/chunk/snapshot; idle bank storage
is not a reader. Warm hits key exact immutable membership/slot/region offsets/
normalHDR and do not encodeNcommands or copyN400Bconstants. Slot exhaustion
falls direct without waits. Existing calls/banks/metadata bounds remain;233guard
placement repair is authored. PPD234 repair keeps64MiB ICB/index scope intact
and bounds extra deduplicated gross native binding assets by the exact existing
SourcePages hardware asset policy, not a duplicate formula or raised cap.
Native-only closure, pending/abandoned/borrowed slot lifetime, denial and idle
trim even while flagsOFF must be proved before performance acceptance.

Next: finish/freeze backend and4arm native/actual-cost fixtures, root review,
append direct slot/bank debt census, integrate source40, required build/static/
strict/native and new ordinary/direct/old-bank/new-bank cost gates. All new
experiment execution is pending. Original full route (GPU work before repeated
CPU discovery, animated conservative bounds, broader detached/pass/memory work)
and owner whole-engine performance remain partial/ACTIVE; no goal shrink.

PPD235 records missing strict warm-reuse assertions in fitting64group direct
mode before first build. Keep all loading/256adverse cases and require actual
zero measured rebuild/reencode/allocation/admission work in fitting warm cases.

PPD236 post-freeze review finds destructive native buffer role promotion before
cold admission. Hold source40 build; unfreeze only Commands accounting/comments
and native alias regression. Prefer per-role deduped leases, with overlap labeled
and no physical-total addition, preserving legal combined buffer support.

Source40integration has hash-checked15frozen source/settings files in root;
only236Commands accounting/test delta must be recopied after repair/review.
No40build exists yet. Root added explicit direct policy/slot/work/retained-asset
census, additive verifier40(eightOFFnonpersistentcontrols/newcompiledmarkers),
private new4arm cost wrappers and summary parser. No claimed execution follows
syntax-only preparation. Existing canonical owner/release apps remain untouched.

PPD237 pre-build consumer review requires generic inherited-bank preparation to
reject direct-binding FrameViews. Row identity alone does not certify binding
ABI; production cohort consumer is correct. Narrow guard/native rejection test
must land before source40 freeze, with proper API recovery preserved.


## Source40 direct-bound ICB freeze

All15source/settings files are frozen, hash-checked and copied to root. Root
reviewed actual old SourceRows/math/resource fragment contracts and the new
split96/176/32B ABI; opaque reviewed shader/world and architecture now reviews
final backend lifecycle/caps read-only. This native binding mechanism has no
original GL equivalent: original1eb44bbd materialV matrix/normal/UV/color and
same Legacy/PBR fragments remain the output contract. No source-row shaders,
material arithmetic, alpha/order/query/cull behavior is intentionally changed.

Direct mode uses cold explicit policy data and stable native view slots. Cache
keys exact immutable membership/control block+slot storage+normalHDR (region
layout/offsets fixed by that composition). Only real FrameView/command reader
prevents overwrite; storage cached by an idle bank is not a reader. Native
geometry/numeric/table/slot/texture closure survives actual GPU completion,
not CPU source descriptions or mutable geometry generation leases.

PPD234 separates existing64MiB ICB/index debt from gross additional binding
asset debt under exact SourcePages resolved hardware cap. PPD236 removes
permanent role promotion: allocation+immutable role leases deduplicate within
each independent scope; dual-role overlap is explicitly not physical-total RAM.
Denied cold aliases preserve older cache/debt, verified by authored native
Ageometry/Bindex shared-buffer denial/retirement/recovery tests. PPD237 blocks
generic inherited API consuming direct FrameViews; descriptors are independently
proved otherwise valid, and proper cohort API output recovery is included.

Cold command admission retains existing40B canonical indexed-call descriptor
(not the separate20B Metal indirect-argument ABI) and adds actual12Legacy/
13PBR handle+64-bit-offset logical binds,192/208B per call. Allocated GPU bytes
and gross retained asset bytes remain separate. Warm hits have zero tickets,
policy copies, Ncommand encoding and new slot allocation. PPD235 strict64group
measured checks require those counters, exact composition hits and zero failures;
loading/256pressure rows remain unfiltered. New hidden tags comparefour
independent ordinary/direct/current-bank/direct-bound arms (192/32summary rows),
while originalthree-arm tags remain available as controls. No heavy-bank mesh
extension or GPU speedup claim is included in this freeze.

Root adds defaultOFF/nonpersistent RenderMetalSourceDirectCommandBanks in common
main/probe capture and separate logical-work/slot/allocation/gross-retention
census. Additive private verifier40 requires the eighthOFFcontrol, direct slot/
trim symbols and new shader/census markers. Private4arm cost wrappers/summary
parser are authored, not executed. Next required40/build/static/strict/all21
native scopes, oldcontrols and bothnew4arm cost matrices; preserve failures.
Source39app69080/owner69065preview remain untouched until required integration;
no viewer launch/login/profile/push and no full-goal acceptance occurred.

PPD238 prevents an unexecuted fixture hang: new128slot pressure used128native
uncommitted commands, exceeding actualqueue64. Source40build mayfinish; do not
run that native scope. Rework onlyfixture to128view occurrences on3commands
plusdenial/blocker, preserving all slot/abandon/GPU protections andqueue limits.


Required40 at0a6daddaac passes doctor/configure/build170.429s. Independent
final backend review passes role accounting/caps/slot lifetime/Gwarm/bindings
and explicit admission; no source blocker found. Before native execution,
PPD238queue-cap fixture flaw is confirmed against actualqueue64 and installed
SDK. No40native scope is run; no timeout/hung process or native success claimed.
Source41 changes only that pressure fixture and docs:128view occurrences use
3command origins plusdenial/blocker, not128uncommitted buffers. Reusing storage
with a CPU reader still fails; actualGPU pending reader and exactslot recovery
remain asserted. Required41/native/static/strict/new4arm+oldcontrols follow.
No source40performance measurement exists yet. Original goal remainsACTIVE.


## Source41 native/cost gates pass; heavier-bank source42 freeze

Exactb02f81fe36,26.4.0.69083: required41 doctor/configure/build0(28.133s),
static346ARM64/signatures/source/imports/features/eightOFFcontrolsPASS,
strict3587files/905informational/0blockers/0errorsPASS. All21native processes
PASS withAPI/GPUvalidation, including pages5914/25,visibility7521/13,
renderer9394/9,commands4055/12,admission2460/10,actualowner14809/59,
shadow98414/14 andold-library/no-layout controls. New128slot fixture finishes
normally; no queue hang was executed. Broader41 alsoPASS88/88CTestgroups
(156.475s),10additional native specializations and3archive policy processes.

Old3arm controlsPASS144872/1(5.967s),many1876456/1(8.560s). New4arm actual
costPASS195980/1(5.753s),192summaryrows/2688rawsamples; many2568360/1
(11.508s),32summaryrows/2688rawsamples. Matching source41 CPU/GPU parity and
query coverage pass across allfour independent arms. Strict64 direct mode proves
100%requested bank work, exactcompositionhits and zero warm policy/slot
allocations, Ncommand encodes, admission/fallback or eviction. Warm view copies
are208B per one-region composition (176+32), not N400B input reconstruction.

Direct-bound versus oldbank improves all8many scenarios CPU8.57–14.16%,
GPU7.19–19.85%,complete/querysettlement5.18–51.56%. This supports continuing the
binding experiment, not a universal speedup. Against direct cohorts CPU is
28.77–56.58%lower, butGPU37.22–102.17%higher; complete cost wins2of8 and ranges
25.12%lower to19.62%higher. Warm512one-group cases complete6.51–42.06%lower
than direct despiteGPU80.08–189.10%higher. Cold/churn remain adverse. Source41
geometry is explicitly3indices/source, so these remain mechanism diagnostics,
not real-world FPS, frame-tail, or release performance acceptance. ControlsOFF.

Source42 is TEST-ONLY heavier-bank coverage. Shared visibilityCostMesh helper
moves before existingcost includes; old768/3072 square math/phase/AABB remain,
1536 adds a32x16 grid. Single512Legacy/PBR/1or3views/cold-warm-churn uses768
unique indices/source; all8manyconfigurations preserve8loading+64warm+12measured
frames with3072/1536/1536/768 indices perSource. FullNcolor churn, publication,
capture/adoption/reclamation, CPU/GPU admission, native/query settlement are
inside timed boundaries; asset creation is explicitly excluded. Geometry counts,
payload bytes and gross wrapper extents are logged, with non-clear depth plus
MRT/query parity. Old3/4armtags and visibility504/144arm cases remain unchanged.
Independent source review passes; no42execution yet. No production code changes.

Separate /private/tmp/alchemy-apple-first-stable-cohort-admission(baseb02) now
implements only opt-in CPU compilation stability under PPD194. Architecture owns
FrameBudget/CPUAdmission/header+tests, cohort compile inl and isolated actual
admission cases; root owns pipeline/settings/cost integration. Stamp is publisher
identity+revision, eligible only across distinct budget frames; same-frame probes
cannot mature it. Requiredraw/native source publication stays current. Cold
child subscriptions do not exist before seal, so stamp is a cheap heuristic,
not proof every child was unchanged; no Nper-view walk is allowed. No policy
activation, execution or performance claim yet. Full original goal remainsACTIVE.

PPD239 source-only stability helper review: stale/duplicate calls must reject
before notePending, including after publicationCompleted. Add regression before
freeze; existing wrapper/current42production unaffected.


## Owner69065 trial evidence imported without viewer actions

Separate owner-authorized Trial run session supplies exact69065/source14837e631a8e
PID13388 snapshot08:44:55–08:48:29UTC. Private evidence is canonical
artifacts/metal/architecture-trial-69065-20261115T084831Z/
(trial-findings.json/.txt,snapshot-receipt.json,Alchemy-trial-snapshot.log).
Root verifies642821bytes/SHA37a69080e41dea552883f663e35af3be825b37db0fd713dc97d8c2435d15b8d3.
No raw log upload, settings/app change, source edit by Trial session, profiler,
new viewer launch or FPS comparison occurred. Owner was told logout is safe.

GroupSources activation is observed in accepted797788main/91688probe cumulative
cohort logicaldraws; banks/preparation/visibility/otherexperimental flagsOFF.
Automatic process policy49152MiB,OS/process low flags0 isobserved, not memory
acceptance.148warnings/zeroERROR/FATAL; no logged Metal command/device failure,
without proof GPU validation was enabled. Source40/41bank results cannot explain
this older bank-disabled run. Source/project/app identity is exact.

Compilation39088admitted/1149230frame-limit-denied/35576published/3512failed
isprocess-cumulative main/probe work, not dropped draws/GPUerrors.3512equals
2252unsupported+1260unattributed publicationfailures(PPD240).558598capturedgroup
occurrences/889910drawoccurrences average1.59draws/group, directly supporting
actual small-run coverage and cheap bookkeeping/stability work, without an FPS
claim. CPUlayoutcache2048entries/3221rows/29682912estimatedbytes has2073evictions.
Source failed shadow views7/1 and zero detached prep (familymax2201/1293 below
8192) are separate PPD241; do not blindly lower measured threshold.

PPD125 retains27hero/mirror warnings, successfulsix-face publication08:46:32,
thenlaterabortfaces1590/2308/2350 terrain guard exact15588. Cause/predicate and
relation to prior teleport issue unknown; fail-closed guard stays. Assetfetch/
texture403/HTTP/DNS/alpha warnings do not establish architecture regression.
Next independent source audits target125/240/241 while stability43 integrates.


## Source42 observed and source43 stability freeze

Source42 ed235581673629f0c527a9d16e3b3525551ab27b /26.4.0.69084 required
build42 exits0/0/0 in30.468s; static42 passes346ARM64/signatures/source/imports.
Production unchanged from41; byte-identical almetal_tests supports explicit
reuse of all21native41 and broad88CTest/10specializations/3archive41, not a claim
of rerunning them. Changed LLRender reruns region64/5 and actual14809/59 with
API/GPU validation, actual41.081s. New mesh costs PASS151960/1(single6.362s)
and2738216/1(many17.560s). Private receipts: frozen-source-build-42,
static-native-42,native-binary-equivalence-41-42,native-correctness-42,
actual-direct-bound-cohort-mesh-costs-42 and actual-many-direct-bound-cohort-mesh-costs-42.

Heavier indexed geometry confirms CPU savings but not a universal bank gain.
Warm512 single groups: CPU73.03–77.15%lower versus directcohort, full/query
settlement1.36–7.24%lower, GPU5.95–8.10%higher. Many64/256groups×2/4draws:
CPU25.46–57.97%lower, GPU8.27–24.65%higher, complete8.80%lower to10.74%higher
(wins3/8). Cold/churn adverse. No setting activation or viewer FPS conclusion.

Source43 integrates the reviewed source41-based feature while preserving42's
shared mesh helper. New RenderMetalGroupSourceStability is defaultOFF/Persist0,
requires GroupSources, and only delays optional compilation until the same
publisher identity+revision appears in two distinct outer frames. Current raw
native capture/resource publication stays live; main/probe views in one frame
cannot mature the observation. Cache eviction preserves scalar stamp evidence;
new identity/revision restarts it. It estimates no work and consumes no budget
on deferral. Stability has its own counter, separate from budget denial/failure.
Cold child subscriptions are absent before sealing: this is a bounded owner-
stamp heuristic, not proof every child stayed unchanged. PPD239 stale/duplicate
checks precede notePending in the new helper; added common and actual-owner
cases include completed→stale, frame0, raw changed pixels during deferral,
eventual parity, cache eviction, cancellation and equal-revision new owners.

All code/cost source peer reviews pass; execution43 pending. New optional gated
cost tags cover allfour existing modes and retain measured required raw work:
[.native-stable-direct-bound-cohort-cost] and
[.native-many-stable-direct-bound-cohort-cost]. Existing tags unchanged. Root
owns source integration/docs/build; no agent launches/profiles. Next: clean
freeze43, doctor/configure/build/verify_app, strict and21native, then paired
ungated/gated cost controls if no owner viewer is running. Stage a separate
owner candidate only after observed checks; preserve69065.

Read-only exacttrial source audits narrow125 to the compound terrain preflight
before texture lookup,241 to CPU capture completeness before GPU commit, and240
to publication stages rather than ordinary CPU-layout rejects(rejections0).
Specific first predicates remain unknown; no guard/threshold/cap changes based
on these counters. Typed bounded diagnostics remain followup, not cause claims.
Full original producer/pass/visibility/memory architecture and owner FPS gates
remain incomplete; current candidate is a meaningful partial experiment.


Next-iteration isolated diagnostics: PPD240 frozen group-failure-census inl
SHAa68a5559e0c6bf087781e466784271d17fdfdaa6acc3f4d24b6c7bbc330dfb83 and
actualCPP074cc7642ac3b89d5d9d26cab32cce570857d3744b8734bbcfd25066382957b2
have independent source peer review, no execution/integration. PPD125 terrain
preflight WIP pipeline.cpp only is separately reviewed. PPD242 records its
first-snapshot hero-face freshness edge before author repair; no43/69065 impact.
Next243. Both are followup, not part of the source43 build/cost receipts.


## Source43 executed — meaningful partial trial point

Frozen6af308d1650818d5334a0e0494a2ad825955fdf0 /26.4.0.69085 required43
exits0/0/0, broad shared-header rebuild834.740s. Static43 passes346ARM64,
signatures/imports/source/media/codecs, allnine experimental controlsOFF/Persist0
and compiled stability method/census. Strict43 scans3587/905info/0blocking/0errors.
All21native43 processes pass API/GPU validation, including common5091/27,
pages5914/25,visibility7521/13,shadow98414/14,renderer9394/9,commands4055/12,
admission2460/10,actual15220/60. Remaining scope counts are retained in private
native-correctness-43.json; no88CTest rerun43 claim. No viewer was launched.

Fresh ungatedfourarm costs219988/1(5.963s),many2645160/1(11.686s); gated
210260/1(5.756s),many2638312/1(11.714s) allPASS. Each single has192summaryrows,
2688raw; eachmany32/2688. Process guard before each scope sees no owner viewer;
no simultaneous build/native validation/cost scopes. Baseline/new shader receipt
hashes and source/binary guards retained. Source43 mesh tags not rerun;42mesh
proof remains explicitly older and addresses unchanged bank implementations.

Mechanism proof: gated single cold/churn defers eachoptional seal (12measured
stability deferrals/config,0admitted,0capturedcohortdraws) while retaining every
required raw logicaldraw and MRT/query parity. Warmsingle and64/256many reach
allretained required draws,0raw,0compile,0deferrals inmeasured windows, preserving
allstrictbank cache/slot/noNencode checks. Cold CPU versus ungateddirectcohort
improves13/16 configs(range20.67%lower–21.18%higher); churn10/16(range25.97%lower–
108.68%higher). Total/querysettlement mixed(cold9/16wins,churn7/16); separate warm
runs vary too, despite no measured sealing/deferral. These12sample synthetic
separate-run timings prove neither universal gain nor causal warm GPU effects.
No300frame/inworld FPS claim or shippeddefault activation. Stability remains an
owner-trial switch; directbanks/visibility/preparation remainOFF for primary trial.

Privatecomparison: stable-direct-cohort-comparison-43.json and corresponding
fourrawreceipts/summaries. Purpose of69085 trial is incremental retained-group
admission/churn behavior versus69065, not evaluation of completed architecture.
Original animated-geometry/pass preparation, useful broader GPU submission and
whole-viewer feature/stability/FPS acceptance remain incomplete. Separate staged
copy/owner-operated launcher are being finalized; preserve69065.

Next iteration (not69085): group-failure-census tree frozeninl a68a5559e0c6...
plus actualCPP074cc7642ac3... bothsourcepeerPASS, notexecuted. Terrain-preflight-
census tree frozenpipeline.cpp d1ad9070d150... with242resetpeerPASS, notexecuted.
Root must review/integrate those disjointfiles, addexistingperiodiccensus stage/
resolvedcap logging, freeze nextsource, build and actualcapture fault-injection
checks. Do not infer125/240/241 causes or change guards/caps from old counters.


Owner preview69085 ready at canonical ignored artifacts/metal/
apple-first-persistent-native-scene-20261114/owner-preview-69085/
Alchemy Metal Alpha G2G5.app. Full source-before/staged/source-after inventories
8273entries(7839files/432directories/2links) match exactly; native/static/signature
checks PASS again on stagedcopy. ExecutableSHA0d22a4345fe703d0039d6f40da33d3888ecb5a2709d32e941f8a05ccf33d425b.
Owner-operated Launch Architecture Preview.command is syntaxchecked, notexecuted,
SHA71b8591706e731421135b734f70eebf4221ea8eeeec011e0a741c8d92e3ce4b9.
It explicitly enables GroupSources+GroupSourceStability and disables native
visibility, depthprepass, preparation, compactinputs, scalarrows, resourcetables,
joinstatic and bothbanks. Shipped settings remainOFF/nonpersistent; this is an
incremental owner experiment, not a finished architecture/performance claim.
Receipts owner-preview-69085.json/-static.json/-launcher.json preserve exactpins.
No launch/login/profile/push;69065stagedapp remains untouched. OriginalgoalACTIVE.

Next executable continuation: review frozen group-failure-census inl+actualCPP
and terrain-preflight-census pipeline.cpp (allbase6af), integrate disjointfiles
into root, wire bounded groupfailure/miss/mask/resolvedcap census without new
walks, and author meaningful actual ordered-terrain/hero-face242 fault cases.
Run harness, cleancommit/FFbuildtree, requiredbuild44+static/strict and changed
actual native scopes; do not rerun unrelated cost suites without changed costs.
Keep69085immutable for owner trial. Existing owner125/240/241 causal questions,
complete architecture, whole-engine FPS and owner acceptance remain open.


## Next architecture boundary — shared animated attachment palette identity

PPD243 records two agreeing source audits at6af: real main/probe rigged capture
already obtains and CPU-pins ALMetalAttachmentPaletteRecord but discards its
identity. ViewInputs::adopt then copies per-view and validation/publication scans/
hashes per-draw; shadows already publish that record byidentity into sharedFD.
Implement the direct identity bridge before inventing additional caches/workers.
Maintain exactpointer/count/joints qualification, actual native owner retention,
raw fallback, and view-dependent body composition. No shader or new flags.

Isolated world-palette-publication base95ed: residency owns almetalworld.h/.cpp,
worldprepare.mm and existing backendtests; architecture owns actual pipeline
buffer-publication inl, new attachment actual/cost appendix and finalinclude only.
Root owns integration, ordinary batch-stride regression costs, builds/docs/register.
Opaque separately extracts actual terrain diagnostic slices/tests for125/242;
no shared edits. Freeze/review then combined nextsource; next244.


PPD244 stops the new243lifetime fixture before execution: renderer creation after
its own GPUgate would wait on synchronous initialization uploads. Root+peer agree
from actualsingle-devicequeue/create source. Move setup beforegate, retaining
pending old/newpose andnative-onlyretirement checks. No hang executed. Next245.


## Combined source44 freeze: attachment ownership and failure attribution

This is progress toward the original engine goal, not a completed architecture.
243bridges the existing immutable attachment palette through actual capture,
owned view inputs, validation and all SimpleBatch GBuffer/forwardalpha/glow/
impostor consumers. Same sharedFrameData publisher already used by shadows;
raw/system-body composition is unchanged. Root includes manualBump/LegacyGlow
capture and rejects the new field in static-only cohort/page/group families.
No shader, graphics setting, resolution or update cadence changes. Five backend
and three actual-owner cases plus96paired complete-cost comparisons are authored.
Equal-byte distinct record identities explicitly expose identity-vs-content
publication tradeoffs. Added batch pointer cost must be checked on ordinary paths.

125/240/241 diagnostics preserve failure/cancel/admission behavior and add bounded
first terrain predicate, group exit/miss/unsupported histograms, exact configured
source byte cap and first CPU shadow failure stage/pass/map/line. Source reviewers
pass. 242hero-face diagnostic reset,244queue-gate test ordering,245owner-line
propagation and246singleton cost-host repair are retained. No affected hang ran.
New terrain tests include actual ordered body with real LLVB/MissingPublisher,
protected phase-V memory, actual failed clearDraws and copied snapshot lifetime;
full terrain packing and hero transaction are explicitly not covered by this unit.

Root reviews/integrates all disjoint frozen files; no test/build evidence44 yet.
Next cleancommit/FF, doctor/configure/build44, static/strict and relevant native
API/GPU checks (including attachment/preparation, actual captured scene, static
families and existing resource/command suites), then palette/ordinary cost scopes
with no viewer/recorder process. Owner69085/69065 staged copies stay immutable.
No new viewer launch/profile/push. Next247.

## Owner69085 full-window profile reconciled

Owner-authorized Trialrun session provides completed/exported measurement at
canonical ignored artifacts/metal/architecture-trial-69085-profile-20261115T095706Z/.
Root inspects profile-handoff.txt, profile-summary.json, cpu-analysis.json,
gpu-summary.txt/gpu-analysis.json and measurement-log-telemetry.json; source6af
and executable0d22a4345fe703d0039d6f40da33d3888ecb5a2709d32e941f8a05ccf33d425b
match immutable69085. PID40700 attached, actual301.686248s with full-window
CPU coverage; recorder/exports0. Architecture session held heavywork during the
requested window. Owner loggedout; release message plus process inspection
confirms no viewer or xctrace before next checks. No additional live measurement.

Presentation13.998462FPS across99.7064%coverage/300.800508s; reported display
interval71.4263ms and20.4801command-buffer associations/frame. Associated GPU
active121.9663ms/begin-end134.0052ms overlap frames in flight and cannot be
summed as serial costs or interpreted as ALU occupancy. Only Overview tables
exist: no per-pass/program GPU ranking or individual-frame p50/p95/p99. Thermal
Nominal; sampled peak physical footprint11.53–11.55GiB is not GPU residency.
338DWARF warnings limit line/inlined symbol fidelity; raw evidence retainedprivate.

Main sampledCPU85.117s includes disjoint ordinaryprobe28.810s and remainder
56.305s; stricthero0.002s. Inclusive overlapping scopes: shadowcapture17.761,
nativeshadow15.642, bindings12.155, draws9.897, worldcapture8.848, postSort4.052.
This points to shared publication/encoding/probe work; counted deferrals alone
are not bottleneck evidence. Interior census endpoints add1,864,334candidates,
1,799,264budgetdenials,64,853admitted,63,362published and1,491unsuccessful
(630unsupported/861unattributedfactoryfailures), only217newstabilitydeferrals.
These are not exact measurement-boundary deltas or CPU durations. No causal
before-after FPS comparison with older dynamic-world sessions is claimed.

Measured log20HTTP-policy warnings, zeroERROR/FATAL and no hero/mirror warnings;
startup20hero warnings are earlier and have renderer0/GBuffer0/line0, unlike125's
older terrain preflight. Recovery/cause still unproved. Probe source build ratios
are higher with different populations/cohort coverage; view matrices apply after
source lookup and do not alone explain rebuilds. New reason diagnostics target it.

Next independent native experiment: eligible full-resolution unblurred probe
lightmap→base lighting pass fusion, with exactRG8quantization/filter/shadow/normal/
HDR/reverseZ semantics and old native path retained for SSAO/blur/external inputs.
Opaque owns a separate fusion worktree/backend/shader/tests; rootpipeline/CMake
integration remains serialized. This is a bounded parity/cost experiment, not a
GPU-bottleneck attribution, quality reduction or predicted FPS improvement.


Required44 at38732f4b25: doctor/configure0, build1 in61.548s(PPD247newtest
LLVector4 tangent strider vs actualLLVector4a/LLVector3 overloads). No native44
execution and no successful44build claim. Root repairs only fixture tangent type;
source45 is the next clean required build. All44failure receipts remain retained.
Owner requests clearer occasional overall progress after23h; maintain a concise
implementation/remaining checklist, not just per-test updates. Next248.


Source45 5c74f34b93 /69088 required build0(141.050s), static/strict pass.
First14native scopes pass, actual67cases66pass/1fail(17756/17757assertions),
PPD248newPBRglow fixture lacksrequiredemissive stream. Production guard correctly
failsclosed; rootadds/publishesactualstream, no guard change. Native tail/newbackend
palette checks and allcosts notexecuted. Next46rebuild with explicit source/binary
unchanged-production reuse for successful45scopes where proved, rerun actual/tail.
No all-pass45 or speedup claim. Next249.


## Source46 observed, next integrated tile/pass/identity repair

Clean aa5645944d5a9be1ee3ba6587492788798b6691e /26.4.0.69089 required46
exits0/0/0 (30.828s), static/strict pass. Production unchanged from45; explicit
native-binary-equivalence-45-46.json proves samealmetal/common binaries and only
fixture change, reusing13successful45scopes. Nine scopes rerun/new46pass:
region64/5,actual17772/67,gpurows6538/3,compact1990/8,resources12379/7,
oldresource4721/1,oldrows283/1,nolayout13/1,palette+worldprepare2340/14.
All22planned scope coverage is accounted for; not an all22rerun46 claim.

Attachment complete-cost46 PASS225024/1(11.937s),192summaries/2688raw,
96pairedconditions. CPUwins85/96, all48large110jointcases; typicalsharedidentity
30/32wins. LargestCPUgain289.917microseconds/42.24%; largestloss4.2295us/4.72%
in tiny1jointdistinctequalbytecase. Total/querysettlement69/96wins with mixedGPU
results, noFPSclaim. PPD251 deliberatelyadverse equal-byte identities reveal up to
55.02times publishedlogicalbytes despiteCPUgain. This is not physical residency.
Rerun after world-only cold-record content interning, preserving O1identityhits
and establishedshadowdefault. Exact matched page, not lastpage, is cached.

Ordinary/control4arm costs46 alsoPASS219988/1(5.816s),many2645160/1(11.653s).
These are full-current-path smoke/cost controls, not isolated causal measurement
of the new8Bfield versus allotherchanges. No owner launch/profile/push/staging.

Nextsource integrates source-reviewed native probe tile lighting: original lightmap
fragment writes memorylessRG8 color1, basegeometry/sky fetches it in the same render
pass with ROG0/hardwarequantization. Rootpending owner packet defers only supported
unblurredprobe scenes, consumes synchronously and resets at every previouslightmap
lifecycleboundary; mainSSAO/oldlibrary routes retained. Encodedtile/separate counters
are not acceptedface counts. New31x31 odd-size parity case adds non-power2coverage.
Allproduction/fixture/rootwiring peers pass; nativeShader/runtime acceptance pending.

Also integrated250validatedempty-local/projector no-op owners;251world-firstidentity
content interning;240childsource miss stages (first/revision/transient vsfactory
payload/variant/promotion). Newcounters countAPIcalls, not uniqueobjects/capacity
causes. Newactual/source-miss and nativeearly-page alias/no-op/pass-fusion fixtures
are unexecuted. PPD249pool-scope,252idlebarrier,253combinedfixture-names corrected
before execution, retainedinregister. Next254. Rootnext freezes47, requiredbuild,
allrelevantnative checks andcompletecosts. Fullarchitecture/ownerFPSacceptance open.


Required47 dfcfb911b0 doctor/configure0, build1 in60.165s(PPD254newoldlibrary
fixture unique_ptr lvalue vs shared const API). No native47/cost execution. Root
uses explicitsharedconst ownership, no production/fallback changes, next48build.
All47failedreceipt evidence retained; next255.


Required48 9063dba5cb/69091 exits0/0/0 (150.069s), static/strictpass.
First22native scopesPASS includingactual17897/70,newpalette2492/16; probe tile
APIvalidation aborts nilDepthStencilState beforefirstdraw(PPD255), no subsequent
native/cost run. Rootbindsexistingalways-pass/no-write overlaydepthstate, retaining
depth/stencil andGPUvalidation. No viewercrash/newlaunch or all48pass claim.
Next49build/scopedretest; priorsuccessreuse requires explicitproductiondiffproof.
Next256. Failedreceipt/logretained.


Required49 da8c24dbca/69092 doctor/configure/build0 (37.728s),static/strictPASS.
Probe API validation no longer aborts;3/4cases and7560/7562assertions pass, only
synthetic sky nonzero assertions fail(PPD257). Exact tile/separate equality passes.
Fixture omitted actual HDRI normal alpha1; correction retains all assertions and
production source. Remaining native/cost suites did not execute. Next50 targeted
retest with exact unchanged-production22-scope reuse proof. PPD256 compact static
restoration remains independently implementing; owner whole-engine goal active.
Next258. Owner69085/69065 remain unchanged; no viewer launch/profile/push.


Source50 **05f223c34e /26.4.0.69093** required doctor/configure/build0
(build27.845s),static/strictPASS. Fourtargetednative scopesPASS: probe7562/4,
oldprobe65/1,empty465/2,deferredlocal27016/1 underAPI/GPUvalidation.
Twenty-twosuccessful48scopes184809assertions explicitlyreused via exact source
proof; these were not allrerun50. CorrectedHDRImarker fixes257; depthfix255holds.
Privatecostdriver selectedllrenderfornativeprobe, exit2/no tests(PPD259); failed
receiptretained. Correcteddriver retry1probe2448/1,empty2948/1,palette225024/1,
ordinary4arm219988/1,many4arm2645160/1 allPASS. No viewer launched/profiled.

Probe8pairedconfigurations (256/512,1/3views,with/withoutshadows),8warmsamples
perarm: encodewallduration lower8/8,median−8.31%;GPUenvelope lower8/8,median−28.39%;
completewait lower7/8,median−4.87%,one512/1view/no-shadow+3.18%. Coldsingle-sample
GPU/settlement mixed; do not generalize percentages to frameFPS. Exact1vs2render
pass,depthandpixelchecks pass; independent oldlibraryseparatepathretained.

Palette logicalbyteamplification251removed: maxrecord/raw1.0 across96cases,
originalequal110jointcase74112vs74112bytes ratherthan4077888vs74112at46.
CPU84/96wins,all48largepaletteswin(median−15.65%); total57/96wins andGPU47/96,
so broaderbenefitpartial. Existingbanks remainOFF: fresh4armcosts stillmixed/worse.
Evidence rollup: artifacts/metal/apple-first-persistent-native-scene-20261114/
architecture-batch-validation-50.json andper-suite/summaries. Owner69085unchanged.

IntegratedPPD256fivefrozenpeer-reviewedfiles: owner-freestaticrestorationtuple,
SIMPLEusesexistingproof; nativegenerations weak-locked beforeexpansion; warmchild/
prooftransportunchanged,dirtyexpiredproofexpands intoownedvariant. Truecapacity
chargedandoldadmissionfloorretained; capsunchanged. Newactual3cases/smallcomplete
costunexecuted. PPD258retained-row and260boundedcompletion-retirement fixturefixes
included; no productionlifetime shortening. Next51requiredbuild/actualpublication/
small-restorationcosts. Originalwhole-enginegoalactive; next261.


Source51 **96363dc6d2 /26.4.0.69094** doctor/configure/build0 (30.483s),
static/strictPASS. Actualpublisher18617assertions/73casesPASS in43.974s under
API/GPUvalidation, including256/258/260. Smallrestoration73578/1,ordinary4arm
219988/1,many4arm2645160/1 costsPASS. Explicit50→51sourceproof reruns actualpublisher
and reuses21successful48native scopes plus4targeted50renderer scopes; no binary
identity or all26rerun claim. Allrootexecution handles terminal, no viewer action.

Measuredsmallrestore108summary/1512rawrows,54pairedconditions: CPUwarm17/18wins
median−2.39%;cold0/18median+7.08%;churn0/18median+7.03% versusraw. Completionwarm
11/18median−1.00%,cold1/18median+1.78%,churn7/18median+1.42%. Not anold/newtuple
causalcomparison orFPSresult. ActualBatch1696B,Source248B,Tuple248B beforeactive
values. Census604tuplebuilds/229520logicalcopiedbytes butZEROexpansions/proofreuses:
PPD262 registers omittedexpired-proofcostfrontier before repair. 256notaccepted
by countersalone; caps andgrossshared-certificateaccountingunchanged.

PPD261terrain pass-publication registered beforeimplementation. Opaque owns
almetalworld.cpp/h,worlddraws ifneeded,worldprepare.mm,lldrawpoolterrain.cpp and
newprivatehooks; rootsolepipeline.cpp/.h/CMake/docs/build/test. Residency owns
NEWalmetal_terrain_pass_publication_test.inl only. ExactAPI androotproducerhooks
pending; no shaderABI/quality/cap changes authorized. Architecturegap owns
262costfixture correction inactualpublisherCPP+staticrestorationappendix only.
Rootregistercurrentlynext263. No shared concurrent edits or source261execution.

Next concrete actions: integrate/review262costfrontier, build/test freshsource;
continue261surface/per-viewcommon packet with exact nativeleases and rawroute,
then useful remainingprep/visibility and combinedcandidate. Goalremainsactive;
owner69085/69065unchanged, nolaunch/profile/push, broaderreleasegatespartial.


Integrated262fixture-only forcedexpired/live proofconditions (3/4), preserves
original0/1/2matrix andproduction. Rootsource-reviewed exactexternalreader/withdrawal/
admission/capture/settlement counters; build52/actual/defaultsmall/newexpiry costs
next. PPD263records oldworld-cost unownedscalar vs adoptedprepared baseline;
currentproduction bothadopts, so fair ownedcostfixture next beforeworkerpolicy.
Bankrange-splitting suspicion was disproved bysource10638existingadjacentmerge;
no productionfix/defect claimed. Terrain261 author ownsrenderer/prepared files;
rootreservespipeline/.h/CMake integration. Next264, no viewerlaunch/profile/push.


Required52 ceb989d223/69096 doctor/configure/build0(build30.976s),static/strictPASS.
Fixture-onlyproductiondiffempty; priornativecorrectness reusedexplicitly. Original
smallcost73578/1 andnewexpiredcost43532/1PASS. New672raw/48summaryrows show336
actualtupleexpansions and336liveproofreuses across measuredconditions, no tuple
rebuilds,144admittedcompiles percondition/zerodenials, exactnativebuffersreused.
Forcedexpired12pairsCPUallworse median+6.45%,+0.833..6.209us; completion5/12wins,
median+1.13%. Liveproof12pairsCPUallworse median+9.06%,+2.730..8.896us; completion
4/12wins,median+0.75%. Syntheticwithdrawal vsraw, not old/newtuplecausalcomparison.
PPD262frontiernowcovered; do not claimuniversalCPUgainfromcompactmetadata.

Integrated261terrainproducerhelper/nativecommonrecords, preparedaliases,
originalperpatchgeometry/factors retained. Original1eb44 terrainpool bindscommon
material/transform/maps beforedrawLoop; newpassownership follows thatboundary.
264exactnativelease and265staticqualifier/unusedownercleanup included. Native
fixture6normalcases+completecostpeerPASS; rootactualcontext2cases coveractual
namespacehelper withtypedregion/camera/maps frontiers, not fullLLpool traversal.
Preparedcost263 threeownedarms+forcedcreditdenial integratedfromfrozen0adce681
peerPASS, no productionworkeractivation. Rootpipelinepatch/PPD265guards reviewed;
next53required/fullnative+terrain/palette/probe/owned-prep/expirycohortcosts.
Allpreviousapps/captures retained; no viewerlaunch/profile/push. Next266.


53pre-build peer: architecture independent sevenproductionfiles+exactpipelinepatch
PASS; residencyownedcost263PASS; opaque independentterrain6casefixturePASS.
Rootindependentactualcontext2cases reviewed and strengthened validR32missinglease
case (not merelywrongformat); all perpatchoriginalsourcepreflight/range/factors
remainactualcode. No successclaimbeforeexecution. Fullnormalnative scope set
rerun aftersharedworld/FrameData/sourcequalification changes; allcostssequential
withvalidationlayersdisabledandno activeviewer/recorder.


Required53 00a90f4177doctor/configure0,build1after58.152s. PPD266newrootactual
helperfixtureconfusedf.image(viewerfacade)withf.white(realLLImageGL),fourcompile
errors; source54usesrealowner. No53native/static/costexecuted. Required54next,
sameproduction/correctnessassertions. Failedreceiptretained; next267.


Source54 **98acd3a1c2/69098** required0/0/0(build135.616s),static/strictPASS.
ALL27normalnative scopes rerunPASS:225831assertions/260cases, actual18661/75,
terrain5150/6. Eightcost suitesPASS includingterrain9504/1,ownedprep31595/1,
probe2448/1,empty2948/1,palette225024/1,expired43532/1,ordinary219988/1,many2645160/1.
No scopesreusedinthiscombinedrun; no viewerlaunch/profile/push.

Terrain12pairedconditions:64/256patchesall8CPU/GPU/completewins;64CPUmedian−11.74%,
complete−7.12%,GPU−10.18%;256CPU−19.82%,complete−21.14%,GPU−23.62%. Singlepatch
CPUall4worsemedian+3.42%,completion3/4worse(PPD268). Source55extends2/4/16cost
beforecallerthresholdselection. Actualrecorddraws30816,fallback0,published896B
perpass/288measuredpublications,30528reuses. Typednativefrontier,noLLpoolclaim.

Ownedprepfair18conditions54armrows/756rawsamples allassertPASS. Availabletwojobs
CPUownerwins1/9, combinedowner+worker0/9,completion1/9(theone−0.079%);median
combinedCPU+8.92%,complete+3.57%. Serial/allcreditdeniedalsooverhead. Workers
stayOFF; PPD267 nowimplements usefulproducer-derived scalar recipes instead of
activating unprofitableper-viewjobs. Exact54llrender_tests+almetal_tests.app saved
locally atsource54-native-cost-baseline withSHA/provenance for narrowold/newcosts.

Rootaddsperiodicsharedterrain andstaticrestorationcounters to existingcensus;
no perdrawlogging orruntimevieweraction. Source55newsmallterraincostnext;54full
productioncorrectness remains source-pinnedevidence. Originalarchitectureactive,
usefulvisibility/commands andwholeengineacceptancepartial; next269.


PPD267sixfrozenfiles integratedafterproductionpeerarchitecturePASS/nativepeer
opaquePASS/costpeerresidency+opaquePASS, finalnativealpha3boundedextension reviewed.
Private mapflagsreusepadding, onebyte/row exactcommonadjacency metadatacharged;
scalarconsumer skips400BassemblyANDcomparison onprivateexactview/adjacentproof,
restoresbuffer4everydraw, resetsraw/prepared/bankgaps. No shader/worker/bankpolicy
change. Native136format/familytuples3cases plusactualsingle/manycold/warm/churn
costs integrated; counters nowperiodicallylogged alongterrain/restoration.

Independentcacheaudit found no camera/read invalidation orvariantpromotionloss:
probefacesdonotagegFrameCount, worldpromotionpreservesotherworlds, geometryrebuild
skippedincubeSnapshots. Nativeworking-set withdrawal+CPUmetadataexpiry remains
unprovedruntimecause; current logging distinguishesfirst/revision/expiry/variant.
No blindcapraise. GPUheadroomaudit finds CPUbusy/creditdebt butno default-on
wholequeueGPU frame signal; benchmarkcommandsumsareoverlapping andunidentified.
Neitherwarmedbank norvisibilityautomaticactivation justified byCPUbusyalone.

Next55combinedbuild/fullnative+newscalarrecipe/oldhiddenbaseline/smallterrain
costs beforedefaultcallerthreshold268. Originalwhole-enginegoalactive, next270.


Source55 **639698d6ff/69100** doctor/configure/build0(162.691s),static/strictPASS.
28native scopes allPASS(241508assertion executions/266case executions); recipe3cases
7835assertions ran broadcohort andexplicitrecipe scope, not distinct case counts.
All11cost suitesPASS: newscalar265608/1,many1844512/1,oldtags142080/2 plusprior8.
Expandedterrain24pairs:16/64/256all12CPU+completionwins;1/2/4all12CPUworse.
Callerthreshold16 selectedfromactualcosts, no shader/geometry/cadence change.

Scalaractualmanywarm CPU8/8median−28.05%,complete7/8median−14.60% versusraw;
manychurnCPU6/8median−13.54%,complete6/8median−7.75%; manycoldall8CPUworse+8.97%,
complete+5.88%. SinglewarmCPU14/16median−6.10%,complete12/16median−1.81%; cold/churn
mostlyCPUworse. Actualrecipe reads153504/97536 andcommonreuse76608/45312(single/many),
manycompiledenials9600preservedraw. These wholepaths versusraw do not isolate267.

Exact54binaryoldtagbaseline matchedshaderhash3fe9685d...,94720/1material+47360/1
SIMPLEPASS. Old/new rawCPUdrift−20..+28%, so noabsolute causal before/afterclaim.
Cohort/raw normalizedCPUratio warm16/24improve median−1.36%, large512warm6/6median
−7.03%; small1/4mixed. Fullcomparison andnormalizedJSON retained. Worker55fair
availablecompletion0/9wins,keepOFF; banks/visibility likewiseunaccepted.

Probeaudit: no cadenceincrease G1=5836d41753→55. Ordinaryscheduledoneface and
existingrealtime6loop, hero6/rate+frameModulo unchanged; completiononlyretire,
notreplay. Managerfiles equalG1→627d1a698d preadmissiona69d6df872. No cadencefix.

PPD270 partialunusedhelperda11a16ead/56build superseded: guardassumed3contexts
actual4 andshellcontinuedafterPythonfailure. Rootallfourcallsitefix57 uses
fail-fastcell andvalidatesfullcountbeforewriting. No policyverified at56.
Next57required/selectednative/caller checks; originalgoalactive, next271.


Required57 ac2dfaeb6b/69102 doctor/configure/build0(build28.166s),static/strictPASS.
Actualterrain context44/2/nativeTerrain5157/6PASS; full55correctness/cost retained
byexact55→57diffproof (onlyselectedpoolcaller+staticpredicate changes). No runtime
LLpool claim. Staging local architecture-checkpoint-69102 via74697, exactinventory/
staticchecks pendingthenupdateCURRENTterminal. Ownerlaunchers69085/69065immutable;
no viewerlaunch/profile/push. Allnewrepair records remainownerpending/ready.

Nextconcrete work afterstaging: contained main→probe→probeNativeWorkingSet
lifetime/capacity reproduction atsamegFrameCount, compare CPUreaderretention vs
native-only transport andcensusfirst/revision/expiry/variant. No established
runtimecause/no blindcaps. Usefulvisibilitypolicy andspecializedsharingremain
open; allpriorgoalboundaries retained. Goalactive, next271.


Combinedcheckpoint69102staging COMPLETE/verified,74697terminal0. Canonicalartifact
architecture-checkpoint-69102 contains app,owner-operatedlauncher,README,inventory,
provenance andstagedstaticreceipt. Sourceac2dfaeb6b, executableSHA256
77c21ee9ec54132f2ce964f476b8ce9a53be0840f405314b643a9edb0d3a1302.
8272exactsource/stagedinventorynodes=7839files/431dirs/2links;346ARM64/signatures/
imports/retainedfeatures/source passed. LauncherSHA256
24cfcc8e85f3e2658df7c291cf48d0a02ad881e5234efc199e3f071eeae780ac;
GroupSources+StabilityON, otherexperimentalcontrolsOFF; neverexecutedbyagent.
No all-goals-complete/FPSclaim, existing69085/69065 unchanged. Allroottooljobs
terminal. Nextactualcachelifetime reproduction assignedindependenttestauthor;
remainingfullscopeunchanged, originalgoalactive. Next271.


Active next author: residency_diagnostics, exclusiveNEW
indra/llrender/tests/alpipeline_source_cache_lifetime_native.inl in
/private/tmp/alchemy-apple-first-cache-lifetime-69102 (branchcodex/apple-first-cache-lifetime-69102,
base71341f9fab). Rootowns existingCPP/EOF/register/build andintegration. Sameouterframe
main/probe/probe, sufficientvsboundedcache, native-onlyvsCPUreader controls; no policy
change orruntimecausalclaim authorized. No rootGPU/buildjobsrunning at handoff.


Cache-lifetime reproduction integrated fromfrozen8a7893626b82 afterroot+opaque
sourcepeerPASS. Newactual[source-cache-lifetime], onecasefourcontrols:WS8/WS2×
CPUmetadataretained/native-only,6distinctLegacyproducers3sameouterframeviews.
Grossallnativebytes<64MiB isolatesentryoverflow; variant/promotion/VB-write
controls separatereasons. No productionpolicy/cap change orruntimecause claim.
Next58required/scopedexecution; hold69102immutable.

PPD271registered beforeexperimental publishedCPUfrustumconsumer implementation.
Opaque exclusiveworld/cohort/visibility .h/.mm/.cpp; architecture onlyNEWnative
andactualcostappendices, rootpipeline/settings/EOF/docs/build. Source-qualified
bounds only, conservativeunknown/rounding failvisible, queryoffsetbeforeCPUskip,
no GPUbankbypass. DefaultOFFuntilcompletecosts, capsunchanged. Next272.


Source58/4870b28117 doctor/configure0, build1 after19.792s: PPD272 newfixture
CatchINFO conditional requires parentheses. Registered before narrowfixturefix;
allproduction unchanged, no native58 execution. Preserve failedreceipt/log.
PPD273 newCPUvisibilityWIPtestunioncontract +thirdarmparityfooter registered
beforeauthorcorrection. No production issue/runtimefault or run claimed.

Saved69085 numeric census source6af308d165: deviceworkingSet62,277,025,792B;
nativebyteslimit/16=3,892,314,112B(3712MiB), reached in35measuredsamples;
NativeWSentriesmax2904below8192. CPUlayouts2048all35samples, rows3155–3171
below8192, estimatedbytes29.63–29.76MBbelow32MiB;110evictions,0rejections.
WrapperestimatesarenotphysicalGPUallocations. Whichmissesfollowbytevsagevs
producerchanges remainsunknown; addbranchreasoncensus beforeanycapdecision.


Required59/5785fb5761 doctor/configure/build0(build32.956s),69107 static/strictPASS.
New[source-cache-lifetime]2516/2520assertions pass; all4same-frame build/hit
controls match; final4retirementchecks fail before releasing uploadframeowners.
PPD275registered before adding waitUntilIdle+frameFences.release at final
fixture-reader boundary. No productionlifetime orcachepolicychange.
PPD271frozen8fileintegration sourcepeerPASS; rootdefaultOFFsetting/producercapability/
scene/logwiring nowintegrated. Native+costvalidation next, GPUvisibility/banks
remainOFF. PPD273pre-execution fixturecorrections retained. PPD274nativecache
branchcensus activeindependent llcommon author, no caps/accounting policy change.


PPD271/276source complete and integrated after independent peers: conservative
publishedCPUfrustumconsumer +all-attemptedcoldcapability, main/probe requested
settings andwarmmissingcapability upgrade; failvisible/queries/nativelease safety
retained. Newsource/costappendices+actualpromotioncase integrated, no executionyet.
PPD274diagnostics3files integrated withpeerPASS; exactnativecapacity/age/touch
reasoncensus nowlogged, policy andlimits unchanged. Retaincounts areinvocations,
pressuremasks areloop-event counts, not unique producers; resourceoversize subset.
PPD275fixture nowalso invokes ordinaryLLVBflush after readerreset tocollect
completed retiredwrappers; no productioncollectorchange. Next60required/native
andCPUvisibility completecost, no repeatunrelated11costswithoutnewconcern.
PPD277pressuretexturemaintenance missingadmission registered; independentauthor
active, no production277integrationyet. Originalgoal remainsactive andincomplete.


Required60/3e9a0653ff/69108 doctor/configure/build0(build166.089s). Staticfirst
failedPPD279(newmarkerusedC++::syntaxinnm-mrawsymbols); binarycontainsactual
__ZNK24ALNativeSourceWorkingSet5statsEv. Retrymarker/census/defaultscheckPASS,
strictPASS. Originalfailedstaticreceipt/log retained, binaryunchanged.
28existingnative scopes237130assertion/279case executionsPASS incl actual
21713/77 (newcachelifetime/finalLLVBflush275+boundspromotion276). Duplicate
recipe broadtag nowexcluded; no unique-case total claim.
NewCPUvisibility786/787assertions,3/4casesPASS; mixedreferencefrontquery0at
family0format4beforeoptimizedarm. PPD281actualshaderreverseZusesrawnative
clipZ[0,1], while authorednegativeworldZbox+identityqueryMVPwasclipped.
Fixture-onlyqueryMVP[10]reflection forreverseZ nowpreservesrequiredpositive
coverage andALLMRT/depth/query-order assertions. No renderingproductionfix.
CPUcost60NOTRUNafterfailednative. Allfailedreceipts retained.

PPD2779filesintegratedafterindependentcore/native peers: newtypedmaintenance
2/frame+2inflight,16/32MiBlogicaldestinationmipbytes initialboundedpolicy;
pressure/busyoneprogress+solitaryoversize, actualcommandcompletionreceipt,
deniedqueuepending/order unchanged. No decoder/upload cap orqualitychange.
Rootregisteredstandalone.mm inCMake withexistingObjC++ARC/skipPCH policy and
logsmaintenanceframe/inflightcount+bytes. Threepurebudget/twoqueue/threeactual
nativecases authored; no277executionyet. WIPfixtureissues278helperordering
and280mutationcount corrected afterregistration, no executedfailures.
Next61required/scopednative+actualCPUcost; allpriorgoalboundariesretained.


Active execution handoff (original goal remains ACTIVE): rootintegrationHEAD
ba0a5063b39b807bc523da92c0adeba0572dace3 has271/274/276/277and281fixturefix.
Executiontree/private/tmp/alchemy-apple-first-architecture-g2-g5 exactsameSHA.
Required61 running via execsession2575 (`python3 /tmp/persistent_native_build_61.py`),
doctor/configure0, build pending; **poll existinghandle, do not restart**.
Sequential `/tmp/finish_architecture_validation_61.py` already running session29447,
waiting61receipt then static/strict,5scopedchecks and48conditionCPUcost; poll
existinghandle, no parallelGPU/build/cost work. Scripts guardcleanSHA/binaryhash,
processinterference andwritefresh61receipts. Reuseproof60→61 explicitlyasserts
allproductionalmetalrenderer/shadersunchanged; actualpublisher/budget/queue/
texturegeneration/nativeCPUparityrerun. No61PASS/FPSclaimbeforecompletion.

After61terminal: inspect EVERY exit/log; registerfailurebeforefix, retainreceipt.
Ifnative/costPASS run `/tmp/summarize_published_cpu_visibility_costs.py 61`, review
rawvsretainedvsCPU (especiallyallvisible/cold). KeepCPUcontrolOFFunlesscomplete
costsupportsconcreteselection; no bank/workeractivation. ThenupdateCURRENT/
checklist/register andcontinue remainingcache/admission+wholeengine boundaries.
Existingstaged69102immutable. No viewerlaunch/login/profile/push. Allauthors
frozen/terminal; no unresolvedsourceownership. Nextdefect282.


Required61/ba0a5063b3/69109 doctor/configure/build0 (full1442jobs804.100s),
static/strictPASS. Initialscopedcommon5585/44,queue174/23PASS; old/newtexture
646/649assertions15/18casesPASS, all3newmaintenancecases stopatnon-mipsetup:
PPD283spanmipoverloadrequiresmUseMipMaps; fixture nowusesrawpixeloverloadfor
non-mip andkeepsauthoredchain. No productionfix/maintenancePASSclaim yet.
2575/29447terminal. Independent61remaining74057terminal0: actual21713/77,
CPUvisibility1780/4allPASS underAPI/GPUvalidation. New48conditioncost3674320/1
PASS16.230s,144summaryrows; originalfailed61scope receipt/log retained.

Actual61CPUvsretaineddirect warm50%outside CPU8/8median−12.55%,complete8/8
−9.21%,GPU8/8−7.15%; churn50% CPU8/8−7.39%,complete8/8−7.17%.
AllvisibleCPUwarm8/8worsemedian+4.41%,churn8/8+5.97%; coldCPUall16worse
median+5.37%allvisible/+3.46%halfoutside. Versusraw containsotherretainedbenefits,
notisolatedvisibilitycausality. CPUvisibilitystaysOFF; noFPSclaim.

PPD284affinesupportintervalclassifier+3CPUoraclecases integratedafterpeerPASS;
API/shaders/caps unchanged; same48conditioncost next62. PPD281reversefixture
repairnow1780/4PASS. PPD282bodyproduction7files+2appendicespeerfrozen separately;
rootactualsharedhelperand2pipelinecallers/ownervectorsareWIPUNSTAGED, notpart
of62checkpoint. PPD285fixturecounts/attachmentowner/clamp-policy corrections
registeredbeforefix, noexecutedbodyfailure. Originalgoalunchanged, next286.


Required62/21676f507d/69111 doctor/configure/build0(build31.669s),static/strictPASS.
Original62orchestrator bootstrap failedNameError(outbeforeinitialization) before
anyvalidationcommand; failedscript preserved /tmp/finish_architecture_validation_62_failed_bootstrap.py.
Explicitcorrectedlauncher45380thenstatic/strictPASS, maintenance+oldgeneration/
mip843/18PASS inclall3real277GPUcases. CPUvisibility+oracle1916/1917assertions,
6/7casesPASS: only aggregateinsidecount0becauseunderflow/insidecontrolsnotcounted.
PPD286registeredbeforeone++counterfixturefix. All175616grid/floatimplications/
invalidchecks +4GPUvisibilitycasesPASS; nofalsecull. 5566/45380terminal.

Independentunchanged62cost27701terminal0,3674320/1PASS16.031s;48conditions/144rows.
Againstretaineddirect: warm50%outsideCPU8/8median−18.96%,complete8/8−11.86%,
GPU8/8−5.04%; churn50%CPU8/8−14.03%,complete8/8−9.36%,GPU8/8−5.60%.
AllvisiblewarmCPUmedian+1.15%(7/8worse)vs61+4.41%; churnallvisible+4.14%
(all8worse); coldallvisible+5.99%,coldhalfoutside+2.32%(7/8worse).
Same-runrelativeevidence, rawtimingdrift/cold/allvisiblecostlimits retained.
CPUcontrol remainsOFFpendingfinalselection/wholeengineevaluation; noFPSclaim.

PPD282FULLintegration afterbackend/native/actual/rootwiring peers:7production
files+rootactualhelper/2callers/2CPUownervectors and2appendices. Privatebodyrecord
preserves180float15jointlayout,requiredcompositioneveryactualdraw/view,
CPUadoptionpin withoutsecondcopy; nativepagescontentinternfirstdistinctidentity
andCPUidentityreuseafterwards. CPUrecordsnotretainedbyGPUcommands; FrameData
current-frameCPUcacheownsrecords onlyuntilbeginFrame. Staticsourceeligibility
explicitlyrejectsbodyproof. Bodylayout instrumentedforactualsizeofBatch/Shadow/
privateStorage; no capdiscount/increase. Four nativecases +oneactualnormalcase
and24conditioncompletecost pending63, nobodybuild/testsuccessclaimyet.


Required63/5258c8bf85 doctor/configure0;build1after55.336s:newnativefixture
unqualifiedshadow(body) conflictsmacOSMacTypesenum. PPD285history registered
beforequalifyingBodyPaletteNative::shadow only. No63native/static/costexecution.
75084/20701terminalfailed; preserve63receipts/binariesnotclaimedsource63passed.
Next64required/fullnative/CPU48+body24cost atsameproductionbodyimplementation.


Independentcompletioninventory5258: G2producerdirty/source/orderedadmission,
G3ownedjoinedpreparation, G5UMA/COW/globalnativecreditsnowhaveactualcode;
63/64bodygates+rollout/ownerwholeengineacceptance remain. StrictoriginalG4
CODEboundary stillmissing: retainedanimatedGPUrows/posevisibility before repeated
per-view command/constants reconstruction. OriginalrouteGoal4 explicitlyincludes
rigged/masked; notmerelyactivationofrejectedstaticpilots. RegisteredPPD287before
newimplementation. No newgeneraljobgraphorendlesscoldmicrooptimizationrequired
bythisinventory. Bodycamera-specificcomposition remainsrequiredspecialization.


## Owner progress checkpoint: source64 and remaining G4 integration

Source7308c43db9 builds26.4.0.69113: doctor/configure/build0/0/0
(140.989s build), static63 verifier and strict source audit0. Main native runner
failed only the new body-shadow raw reference fixture at avatar=true with
texture_mask (PPD285);17/18 palette/preparation cases passed. Test-only repair
uses existing avatar_mask coverage and captures the error after render returns;
source65 build and failed-scope retest are next. Source63 helper-name compile
failure and64 reference failure are retained, not counted as production bugs.

Independent64 remaining scopes all pass (handle17862 terminal0): probe7562/4,
old probe65/1, empty465/2, deferred27016/1, terrain5157/6, scalar7835/3,
CPU visibility1780/4, interval137/3, body123/2, maintenance/generation/mips843/18,
pressure174/23. Actual publisher22267/78 passed in the initial64 runner.
Receipts native-correctness-64.json and native-correctness-64-remaining.json.

Both64 actual cost frontiers passed independently: CPU visibility3674320/1
(48configurations,16.247s); body103104/1 (24configurations,2.696s). CPU warm
half-outside versus retained direct wins all8 CPU/completion conditions, medians
19.987%/11.133% lower; all-visible warm CPU median1.739% higher, churn3.253%
higher. Body costs are mixed/near parity (1/4/64 draw CPU medians+0.508%,
-0.907%,+0.606%); max logical native-byte ratio1.0. No FPS or physical-memory
reduction claim. Exact summary JSON files retained without overwriting.

PPD287 required G4 animated GPU records are authored in the exclusive shared
author tree: frontend/common/shader/test source reviews converge, backend final
lease/callback/cap review ongoing. Not integrated, built or measured yet. Root
next wires lazy admission, main/probe capture, scene flags, preparation bypass,
OFF/Persist0 setting, census and CMake. Source review is not executed validation.
Remaining deliverable is integrated G4 plus relevant native and actual-caller
cost checks, then one separately staged verified combined app. Full product
feature/stability/performance acceptance remains partial. No launch/profile/push.


## Source65 body-shadow fixture pass and first complete G4 source integration

Exact9c2b48c23d/26.4.0.69114 doctor/configure/build0/0/0 (29.166s build),
static verifier/strict audit0, repaired palette/preparation3150assertions/18cases
API/GPU validation0. Handles56730/68794 terminal0. Source65 changes only the
body-shadow reference fixture and documentation, so64 production and independent
cost evidence remains applicable without rerunning unchanged costs.

Root copied21 frozen/peer-reviewed G4 source files byte-exact and integrated
main/probe begin/capture, scene flag, owned renderer selection/preparation bypass,
clear lifecycle, lazy compilation admission, native/actual test includes, CMake,
OFF/Persist0 RenderMetalAnimatedGpuRecords and periodic capture/CPU/GPU census.
Freeze manifest /tmp/animated-gpu-source-freeze-root-integration.json. Backend
peer confirms native-only command ownership and idle-only weak proof retirement;
shader ABI/vertex math and exact final fixture deltas reviewed. This is source
review only: first integrated build66 and execution still required. No runtime
correctness or speedup claim for G4. No viewer/profile/push.


## First integrated G4 build66 and bounded recovery repairs

Exact a2fc865fe8 builds26.4.0.69115: doctor/configure/build0/0/0 (177.166s),
static66 native bundle verification and strict source audit pass. Initial
frame-budget suite43/45 cases passes; two existing recovery/capacity assertions
fail after290 reused service rank as genuine deferral age. Independent actual
publisher79/80 cases (23045/23046 assertions) passes, including both new animated
producer cases; its sole failure is the same aged cold-cohort recovery.

New animated native suite3/4 cases,1340/1341 assertions passes. Mixed-format raw
reference query at reverse-Z format4 returns0 before optimized comparison.
PPD288 fixture repair only reflects negative-world query Z into native[0,w],
keeping attachment transforms, all eight formats and positive query assertions.

All31 remaining native scopes pass66, including old shader libraries, source
pages/commands, geometry/material/resource ownership, shadow preparation, body
shadow parity, lighting/probe/terrain, CPU visibility/oracle, maintenance and
pressure. Exact counts/commands/hashes: native-correctness-66-rest.json.
Handles90970/41982/78497/21207/88429 are terminal; failure receipts retained.
No performance66 run occurred, because correctness did not pass.

Repair67 separates genuine pending age (progress eligibility) from recent
service rank (ordering only), retaining quotas and all failed old assertions.
Only common budget header/implementation/tests and new animated query fixture
change production/test source. Next run67 required/static/strict, common policy,
actual publisher, new animated native suite and admission/shadow/maintenance/
pressure checks; unchanged renderer parity reuses66 with an exact diff proof.
Then run48-condition animated actual-caller costs with validation disabled.
Original Goals1–5 were independently checked against actual callsites: no
further explicit implementation gap identified beyond validating/selecting G4
and delivering the combined candidate. New retained animated shadow submission
is not claimed; current native shadow producer sharing and rigged/mask indirect
paths remain. Owner performance/feature/stability acceptance remains partial.


## Source67 correctness pass and animated performance rejection

Exact17ce73c68c/26.4.0.69116 doctor/configure/build0/0/0 (805.967s full rebuild
from shared header), static66 verifier/strict audit pass. Affected checks all
pass: common5867/46, new animated native3434/4, actual publisher23280/80,
shadow98414/14, source admission2460/10, maintenance843/18, pressure174/23.
Handles30422/71546 terminal0. Prior66 independent31 renderer scopes remain
applicable with exact production diff (common budget header/implementation only;
new animated reference-query test fixture). No new viewer launch/profile/push.

The48-condition actual animated complete-path cost passes77438assertions/1case
in4.881s. Summarizer validates48pairs/96summaryarms/1680rawarms, including
264loading and576measured frame pairs; no denials/loading samples removed.
Warm group-mean CPU is17.75–29.66% worse, settlement7.92–24.82% worse;
cold/resource churn is substantially worse. Arena-byte ratios0.36–0.53 in
warm cases exclude retained metadata/ICB resources. New animated control stays
OFF. This is a rejected measured mechanism, not architecture acceptance.
Root will investigate the actual warm path rather than enable a regression.

PPD293 registers missing working-set evidence before source-only validation
additions:13 separate per-arm cache/cap counters outside CPU/settle timers and
one focused tiny-cap/pending-retirement native case. Counters overlap; do not
sum them or infer physical RAM from a zero unused raw-cache counter. Reviewed
source68 changes only two test appendices plus documentation; next required
build/static/strict, native5cases and actual48cost/working-set data. No repeated
unchanged fullrenderer run is needed for these test-only additions.


PPD293 exact source freeze: native appendix44b060510050 adds one focused
public-client metadata/assets/ICB cap and pending-credit retirement case; prior
four unchanged. Metadata rejection uses allocator's explicit4KiB rounding, not
an invented hardware assumption. Actual appendix7d386ea71458 adds13 separate
shared-cache byte/count/cap fields strictly after CPU/settle timestamps, sampled
throughmain once per sharedFD. Both bounded peers pass; no execution yet.

Follow-on287 work is bounded to measured warm overhead: inspect native compute
encoder cost and root-to-PoseView vertex indirection, plus actual frontend's
repeated warm work. New queues/frameworks and shadow-only scope are unnecessary.
The67 measured regression remains authoritative; all controls stayOFF until a
converted path wins complete costs with preserved output/lifetime.


## Source68 working-set gate passed; source69 implementation active

83c8b70ad7 builds26.4.0.69117: required doctor/configure/build0/0/0 (37.145s),
static66 verifier/strict audit pass; native animated3604assertions/5cases pass;
complete cost87446/1 in2.759s plus48pair/1680row parser pass. All68 handles
90324/63488 terminal0; no build/GPU/cost process remains. Exact reuse proof
native-scope-reuse-66-67-68.json retains66 failed records and31 successful
independent scopes,67 affected seven-scope passes and68 test-only changes.

At64sources warm, measured native metadata256.3KiB, grossbindings801.9KiB
oneview/930.4KiB threeviews, ICB64/192KiB, index12KiB. Stable1/3cached/live
banks; no warm recompile or unbounded retention observed. Scopes overlap; these
are cache ledgers, not process physical-memory totals. Cap and retirement tests
pass, so293 is ready_for_owner; original runtime acceptance remains pending.

Both67/68 reject activation: warm CPU14.6–30% higher, settlement8–25%higher
atgroup means. New287 work in author tree is explicitly bounded: separate
full-count retained commands from GPU visibility (mode-qualified banks prevent
reuse of GPU-culled args), omit unused compute setup when modeOFF, and move
owned GPU-view vectors instead of copying again while preserving public-span
snapshot/callback safety. No shader/root lookup rewrite: existing VS already
binds PoseView directly. Architecture agent peers; residency owns only two test
appendices/mode-aware cost parser. Root owns pipeline.cpp/settings integration.

Root currently has unbuilt69 wiring for Scene.animated_gpu_visibility and
ALMetalWorldAnimatedStats.visibility_dispatches plusOFF/Persist0
RenderMetalAnimatedGpuVisibility. Backend header fields have not been copied
yet, so do not build root until all author freezes arrive. Next: integrate
reviewed backend/test files, source69 required/static/strict, mode-switch/pending
parity and actual48costs for computeOFF thenON, preserving direct baseline and
source68 memory fields. Controls remainOFF; combined staging still pending.
No viewer launch/login/live profile or push; owner69085/69065/staged69102 intact.


## Source69 bounded compute/copy experiment frozen and peer-reviewed

Backend five files frozen (commands38f8a1c3/worldf6287210); native0c826ac47550
and actual32424f414524 appendices copied byte-exact. Both peers pass. Full-count
mode owns separate bank keys and omits Root/visible buffers, encoding and compute
PSO/dispatch; per-source PoseView reflection remains, so shader bytes unchanged.
Owned GPU-view vector is moved into Commands; span callers still get a private
snapshot.32Bglobals copied before admission callbacks (287 public-helper gap
registered before fix). Existing native leases/finite checks/pending readers stay.

Root wires OFF/Persist0 RenderMetalAnimatedGpuVisibility and periodic encoded
visibility_dispatches; primary RenderMetalAnimatedGpuRecords remainsOFF. Scene
API defaulttrue preserves previous callers; actual production capture sets the
explicit new diagnostic setting. Both native modes/mode switches, pending opposite
modes, cancellations and293caps remain in five cases. Actual normal cases cover
both; hidden48two-arm cells run once per explicit environment mode (0default
fullcount,1visibility). Control and actual dispatches printed perrow/summary.

Updated parser py_compile and complete68 backward-compatible parse (48pairs/
1680rows) pass to /tmp/animated-parser69-compatibility68 outputs; preserved68
artifacts unchanged. No69 build/runtime/performance claim yet. Next required
source69 build/static69 verifier/strict, native/actual+relevant command/resource/
oldlibrary regressions, then separate complete48cost runs with env0 and1.
Use /tmp/finish_architecture_validation_69.py after frozen69 builder; it records
controls and excludes viewer/profiler interference. No viewer launch/profile/push.


## Source69 two-mode results and source70 declaration reuse freeze

Exactd2e8b6d9ca/26.4.0.69119 doctor/configure/build0/0/0 (164.224s), static69
verifier and strict source audit pass. Native5029/5, actual23778/80,
common5867/46, pages5914/25, directrenderer9394/9, commands4055/12, oldcommands
84/1, resources12379/7, palette/preparation3150/18 all pass. Handles48592/88181
terminal0. Explicit env0fullcount92487/1 andenv1visibility87447/1 cost runs pass;
both summaries validate48pairs/1680raw rows with actual dispatch census.

Fullcount improves mechanism cost, but warm group means remain CPU4.61–15.46%
andsettlement2.67–16.33% worse than direct. Example64Legacy/oneview CPU
0.086396→0.089479ms andGPU0.088771→0.107541ms; these tiny contained timings
are not whole-viewer FPS. Both controls stayOFF. Visibility is stillworse.

Source70 freezes seven peer-reviewed backend/testfiles: world4339fbe5,
commands.h3d635821/.mmc4ecea9b, pipeline.h75eedec5/.mmd6e950b4,
native3df2706e/actual46be6b42. Encoder-scoped declarations use inert lazy
active-encoder identity, exact controlblock and actualBatch weak proof, no
nativeEncoder cycle/globalpointer cache. New/ended/stale encoders cannotreuse
proof; old API remains selfdeclaring. Recordmark follows ALL declarations and
fault/prefix behavior is preserved. Existing typedowned animated references
skip only their already-performed batchvalidation; public/borrowed/native
callback checks remain. Identical extra12float scans removed.

Positive tests exercise twoqueryranges/oneclosure, three real encoders, stale
scope rejection, and legacyAPI selfdeclarations with completeoutputparity.
Counters describe actual helper declaration attempts, sampledonce/sharedFD after
timers; root adds periodichelper_declaration_attempts. No70executionclaimyet.
Next /tmp/persistent_native_build_70.py then
/tmp/finish_architecture_validation_70.py required/static/strict+relevant
correctness+two explicit48costs. Source70 has no shader/common/frontend changes.
Later sourcefact only: PoseView IDs1–3/9 are fixed perbank but stillwarmencoded;
otherstaticbindings/Root alreadycold-only. Do notunfreeze70 for this beforecosts.
No launch/profile/push, staged69102 remainsuntouched; engineeringgoalactive.


## Source70 verified; source71 retained direct consumer integrated

142390cc67/26.4.0.69120 required0/0/0 (172.444s), static70/strict pass.
Native5193/5, actual23814/80, common5867/46, pages5914/25, directrenderer
9394/9, commands4055/12, oldcommands84/1, resources12379/7, palette3150/18
pass. Handles54044/58869 terminal0. Two48costs full95743/1 andvisibility
90703/1 andboth1680-row parsers pass. Declared-once counter verifies1closure
for2ranges; fullcount64warmCPU improves4.52%/1.53%(one/threeviews) while
settlement12.77%/12.33% andGPU19.51%/16.34% remainworse. Noactivation.

The nextconsumer preserves the sharedarchitecture and original direct shaders:
latePalette Draw getters replace1720BscratchBatch construction, cachedprivate
recipe flags and exactadjacent view/common proof avoid repeated400Bassembly
whereequal; first/changedvalue rebuilds andslot4stillrebinds afterqueries.
CPUcertificate seal omitsGPUdescriptor/bone/index work; gpuReady explicitly
distinguishes notrequested from attempted-but-unknown bounds. GPUupgrade clones
immutablePayload underexistingadmission/current/transientguards, oldreader stays
valid. Direct-to-GPU failure canstillcaptureCPUreadyreference andrenderdirect.
No shader, command, pipeline, common-policy orprepare.mm changes in71.

Sixbackend+threefrontend+twoappendices copied byte-exact withcross-peerPASS.
Root newRenderMetalAnimatedSourcesOFF/Persist0 usesbeginAnimatedSources
(CPUenabled||GPUrequested,GPUrequested), logsCPUrecipe/commoncounters and
sourcecapabilitypublications, andbypassesdiagnostic worldworker pathforselected
sourceconsumer. GPUcontrols remainOFF. Five nativecases nowcoverdescriptor-free
scalar/prepared, bothfamilies/masks/eightformats, dynamicUV/raw/body/clothmix,
allfiveviewmatrix proofresets, pendingCPUexpiry/cancel andGPUupgrade/transient
fallback. ExistingGPUscope/cap/lifetime coverage retained.

48two-arm frontier remainsalladverse, routeonce/run via
ALCHEMY_ANIMATED_GPU_COMMANDS(default0direct,1GPU) andoldGPU_VISIBILITY0/1.
Three explicitruns direct/fullcount/visibility useidentical ordinaryraw baseline;
newroute/counters logged andparserrejectsmixing. Updatedparser backwardparse
of70 passes48pairs/1680rows to/tmp/animated-parser71-compatibility70.
Savedclub data establishes onlyshadowrunlengths(max249), notmeshindexcounts;
96indices/source remainsdeclared fixturelimitation, no inventedclubmeshclaim.

Nextrequired71 builder +static71/strict, full34scopes justifiedbycoreDrawfacade
changes, thenallthree48costruns. No71runtime orperformanceclaimyet. No viewer
launch/login/liveprofile/push; ownerapps andstaged69102unchanged; goalactive.


## Source71 full regression pass; source72 proof/cadence changes integrated

Exact7d58e89d88/26.4.0.69121 doctor/configure/build0/0/0 (165.315s), static71
andstrict pass. All34native scopes pass:252278 assertion-executions/346case-
executions, includingnative7653/5 andactual24464/80. Handles96567/78876 terminal0.
Three48costs direct100120/1, full102464/1, visibility97424/1 andall1680-row
parsers pass. GPUtax isgone indirect-source route(originalshaders); warm small
CPU wins0.65–2.44%,64CPU loses3.33%/5.37%(one/threeviews), coldCPU7–11%
higher. Nearparity isnot a consistentperformancewin; allnewcontrols remainOFF.
Directcensus confirms1commonbuild+63reuses/view at64 andnativeGPUcommands0;
noextra SourcePages/ICB resources. This isnot a whole-viewer FPS measurement.

Source72 isintegrated/frozen/cross-peerPASS: backendworldfe59ac80, animated.h
ec96aba6/.mm4efb6d33; frontend35d9d2f3/4934aabd/7bcf6939; native3665c8d6,
actualb329905a, parser369993e4. PrivateSource alreadyseals matchedmaterial
proof; ordinarydirect consumer nowtrustsit. Per-render consecutive private
pose cache retainsnativepalette/jointcount slices after successfulpublication,
rebinds5/6/7 afterqueries, resetsraw/body/prepared/ICB/skipgaps; noCPUrecordGPU
retention orcrossviewcache. Exact nativeOwner control+pointer checks avoid
temporary three-owner Scope copies; explicit rawgeometryidentity remains.

OptionalDIRECTsource compilation canwait forTWO distinctouterframes ofsame
producerrevision usingexistingdeferUnstableRevision. No optionalticket/workbytes/
clone/seal duringinstability; main→probes sameframe onlyduplicates. RequiredRaw
andcurrentpose remain immediate; GPU/immediateAPIs defaultfalsecompatibility
unchanged. Rootprimary begin(...nullptr,true) selectsstability andlogsseparate
stability_deferred. Newpose pair acquisition/cache-hit counters areperaccepted
view, notphysicaluploads. Native/actual tests positively coverqueryrebinding,
newjointcounts, gapresets, pendingCPUexpiry, sameframeprobe deferral, nextframe
qualification, dirtyraw fallback andwarm/pose0tickets.

Next source72 required/static/strict, affectedninecorrectnessscopes andfour
explicit48costs: stableDIRECT, no-stabilityDIRECT ablation, GPUfullcount and
GPUvisibility. CostenvALCHEMY_ANIMATED_SOURCE_STABILITY=1defaultDIRECT/0GPU;
alloriginalcold/churn/loading refusals retained. Parser backwardparse71 passes
48pairs/1680rows to/tmp/animated-parser72-compatibility71. No72runtimeclaimyet.
No launch/profile/push, existingownerapps/staged69102 immutable; goalactive.


## Source72 complete costs and selected combined configuration

Exact beb6837b71f0429f116fef8e2ad7cc5d166bc010 /26.4.0.69122 required doctor,
configure and build all exit0 (165.459s build); builder72538 and validation98353
terminal0. Static72 (including required verify_app) and strict source72 PASS.
Nine affected native scopes PASS:74802 assertion-executions/203case-executions,
including animated7925/5, actual publisher26034/80, common5867/46.

Four explicit48-condition cost receipts and parser outputs PASS:
`actual-animated-gpu-record-costs-72-{direct,direct-no-stability,full-count,visibility}`.
Each retains96summaryarms/1680rawarms,576measured+264loading framepairs.
Stable direct CPU warm1/2 improves1.58%/1.88%;4one/threeviews is+0.10%/+0.24%;
64one/threeviews is+0.20%/+0.27%. Stable direct cold CPU ranges−0.50%..+2.16%;
resourcechurn+0.12%..+3.27%. The no-stability ablation retains cold+4.87%..+12.32%
andresourcechurn+2.74%..+5.45%; stability defers optional work rather than omitting
required draws or samples. Arena publication bytes at64warm are0.772x/0.755x;
these are not total native retention, physical RAM or memory bandwidth.

GPUfullcount64warm CPU−3.29%/−1.00% but settled completion+11.56%/+8.96%
andGPU+19.90%/+26.17%. GPUvisibility warmCPU+18.72%..+27.91%. Direct is now
near parity; none establishes a consistent net speed win. All three animated
controls remain OFF in the recommended combined candidate. All original masks,
query order, pose/lifetime, denials and current raw rendering contracts remain.
96indices/source remains an explicit workload limitation, not measured club geometry.

Selected `combined-policy-72.json` enables GroupSources and GroupSourceStability
(the existing accepted scalar recipe/cohort policy); explicit OFF for optional
workers, native/CPU visibility, depthprepass, compact rows/tables/joining/banks
and animated paths. Other integrated foundations are always active, including
shared publication main/shadow/probe, bounded joined shadow preparation, owned
attachment/body inputs, changed-range/COW native storage, coordinated frame
admission/retirement, terrain threshold16 and eligible probe lighting fusion.
Source55 warm-many CPU8/8 median−28.05%, completion7/8 median−14.60%;
cold debt remains in evidence. No whole-viewer FPS claim.

Full34source72 regression started with the unchanged exact binaries to avoid
reusing older-scope evidence for final staging. Then stage separate combined
app with exact inventory/signature/ARM64/native imports, selected policy and
source/evidence provenance. Existing owner apps/staged69102 remain immutable;
no viewer launch/login/live profile/push. PPD287 remains partial for owner gates.


### Source72 final integrated engineering delivery

Full34scopes rerun onexactsource72 PASS:254120 assertion-executions/346case-
executions in161.571s; native-correctness-72-full.json, handle13569 exit0.
No earlier-scope reuse required. Stage helper (peer-reviewedSHA31e3a76f39ee…)
executed withrun72,combined-policy-72.json,combined-evidence-note-72.json and
verify-g2g5-bundle-72.py; handle56225 exit0. Destination
`architecture-combined-69122`, provenanceVERIFIED,8273 identical app inventory
nodes, executableSHA101a327c68852a44e2ba435cd0744a9beaf4582e511fe16b239f171069c57428.
Staged static signatures/ARM64/imports/features PASS; SHA256SUMS recheck PASS.
`Launch Apple First Combined.command` applies the selected explicitpolicy and
was not invoked. Required commands/exits/source/binary/evidence hashes retained.

[Requirement audit](../../research/apple-first-engineering-delivery-20261115.md)
finds no further required original-route callsite; implementation/selection/
delivery finitework done. Owner whole-engine performance/feature/stability
acceptance and product release gates remain open; originalgoal active for those
outcomes. No more speculative cold tuning or shadow-only phase is prerequisite.
No launch/login/liveprofile/push; old owner apps/canonical tracked files and
staged69102 unchanged. PPD287 remains open for original acceptance.


### Post-delivery acceptance dependency audit

Previous goal turn made concrete progress: source72 full regression/cost selection
and verified immutable69122 delivery. Current continuation re-read original
PERFORMANCE/VALIDATION/route requirements and exact candidate receipts. No further
mandatory safe implementation/staging artifact was identified without runtime
evaluation. Optional [owner run card](apple-first-owner-evaluation-69122.md) now
pins69122 and previous native69085 executable/source hashes and distinguishes
architecture comparison from the original-Alchemy product reference. New ignored
evaluation-preflight-69122.json hashes full34/cost/static inputs; both current
executables match receipts. No viewer/xctrace process observed; no launch/profile.
PPD287 current next-action/verification now reflects implemented-but-unaccepted
capability; history retained. No proposed fix or code change was made.

Remaining dependency: owner combined runtime evidence or subsequent explicit
launch/profile authorization. Product/whole-engine performance/feature/stability
gates remain open. This is the first post-delivery dependency audit, not three
consecutive blocked turns; goal remains active. Do not rerun successful builds/
tests or add speculative implementations solely to fill this wait.

Documentation check initially rejected one added recovery line in CURRENT/metal-port (160/180-line limits). Removed the added lines and folded the run-card pointer into the current summaries before revalidation; no code/app change.


### Third consecutive owner-runtime dependency audit — blocked handoff

Audit02 and03 artifacts revalidate the same condition across three consecutive
post-delivery goal turns. Previous turn: no progress, no live job being waited
on, same runtime dependency. Currentclean3d06bdb708 and staged69122 verified;
sourcebeb6837b71 matches full34source72 receipt exit0. Provenance architecture
acceptance remains partial and owner acceptance pending. Bounded source/delivery
audits found no further independent mandatory implementation or staging item.
All authorized implementation/contained verification/delivery work is done;
whole-engine runtime performance, features, loading, memory and stability are
unverified. Do not declare original goal/product complete.

The original goal prohibits launch/login/profile/push without subsequent
explicit instruction. No new authorization or owner result arrived. Mark goal
blocked at the third audit, retaining its full objective. Resume after owner
results or subsequent explicit runtime authorization. Exactrun card and69122
launcher remain ready, all prior apps/source/evidence preserved. PPD287 remains
investigating for original acceptance; no defect closed and no runtime action.
Artifacts: evaluation-preflight-69122.json and
post-delivery-dependency-audit-02.json/-03.json under the existing ignored E2root.


### Testing Integrated Build: owner-authorized quiet and club captures

Owner ran exact69122/PID66666 then explicitly requested5minprofiling+logwatch.
Owner next requestedsecondrecordingBEFORETP; actual21:40:21.405tracebegan before
21:40:42TPrequest/44arrival/47finished. Owner confirmedviewerclosed afterboth.
Quiet301.061391s andclub300.725394s GamePerformanceOverview, both recorder and
TOC/CPU/GPUexport/parser chains terminal0. Sessions75274/20449 recordingdone;
quiet88453/busy89574 offlineanalysisdone. No live job remains. Initial150.24s
CPU-only attempt was deliberately terminated(code1), retainedsuperseded and
excluded. Quietofflinechain finished21:39:56.913 beforebusyrecordcommand21:40:19,
so noanalysisoverlap. Quiet339DWARFwarnings, busy0; aggregate-onlyframeschemas.
Postcapturedoctor95656/verify_app91558 exit0;346ARM64slices,0invalid. No build
needed forunchangedcapturedsource; no app/settings/source modification orpush.

[Whole-engine report](../../research/integrated-build-69122-quiet-club-20261115.md)
andprivate artifacts retain exactcommands/exits/source/binary/log/export hashes.
Last60Overviewcadence12.44/19.50FPS andmeaninterval73.28/51.31ms aredifferent
workloads,notbeforeafter architectureacceptance. Quietprobes27.078CPU-s/31.7%
main; clubmain239.714CPU-s withshadows/geometry/lighting. Detachedeligible/calls/
completed andcompactdispatch/serial0 inboth main/probecensus throughout both
windows; familymaxima666/1158 below8192. Thus no usefulparallelrender benefit
isdemonstrated inactualSLscene, despite implementedcallsites. Do not lower
thresholds blindly or forcecost-regressingoptionalGPU paths.

Nativeworking-setcap3,892,314,112bytes causes2,559,646/4,331,675sampled byte
rejections and1,090,835/1,535,092capacityevictions inquiet/club. Retain visible
CPU3.452/4.900s inclusive, notdominantframecauseproved. Clubloadingbacklog and
peakfootprint31,805MiB requirecontext; no leakproof. Actualwindowlogs24/129
WARNING,0ERROR/FATAL; OpenHTcodestreamwarning1, oldregioncleanup2. PPD005/240/
241/274 histories updated;newPPD294 recordsunknowncleanupcause withoriginal
bodyidentical/markDeadbehavior, no correctivechange/closure.

Documentationharnessinitiallyfailed because new294record lackednextIDadvance;
advancednext295 andreran beforehandoff. Owner explicitlyrequestedanalysis
message toArchitecture Session; finishlocalreport/docscommit, sendexactanalysis
andpaths tothatnamedthread, thenleaveimplementation withArchitecture Session.
No furthercapture, relaunch, sourcefix orpush authorizedbythis testhandoff.


## Runtime evidence resumed architecture work — PPD295

Owner-authorized Testing Integrated Build completed both69122 captures and sent
78edcee323 report/state/register handoff. Runtime evidence now exists; original
goal active again, acceptance remains partial. Cache turnover240/274 is real;
commonretain has heap resource-vector staging before every byte denial. Ready
lookups alreadytouch; producer-only negative caching and cap increases are not
justified. First bounded repair295 will stage small resource sets locally and
materialize the unchanged retained vector only after admission, with arbitrary
union spill, exact callbacks/accounting and independent actual readers. Benchmark
the actual common sources against pinned old source and retain adverse cases.
This targets an observed hot ownership boundary, not proven whole-frame dominance.

Independent zero-job source audit confirms bothloggedpaths: productionshadow
8192-per-family threshold dominates backend256; observed666main/1158probe never
qualify. World workers deliberatelyOFF and additionallyexcludecohorts. No broken
callsite is demonstrated, but useful detached rendering remains unproved for
actualsmall SL workloads. Do not lower thresholds blindly. Full G3 and whole-
engine performance acceptance remain open; this cache repair does not close them.
No new launch/login/profile/push; original69122 app is immutable. Root owns
shared integration/docs/builds; independent native common code may be authored
in its own worktree with exclusive files and tested before integration.


### PPD295 selected reservation integrated, full build73 next

Nativecommon source867fb2fb8bd238ab75b643e04622a7be36ee359adda97e4052a953fffd1194ed
andtestccd300e5f16a98c1a0339512a96576882d1dc1abff033aa9fdc337037badd070
copiedexact. Production delta is one reserve on the first validated unique
resource append. Two semantic cases add16/17/64/256 duplicates/conflicts, byte
recovery, caller-span mutation during eviction and independent reader expiry;
selected standalone601assertions/16cases PASS. Actualbaseline common sources
match69122. Inline16 correctbutcold1+13–16% rejected; selectedreserve48cells
plus2/3-resource16cells PASSfullStats/outcomes with9alternatingpairedblocks.
All receipts, compiler/argv/hashes, separateCPU/allocationbinaries and adverse
controls under E2/runtime-retention-295-20261115T220845Z.

Retain-only CPU2/3cold−15.28/−26.26%, denial−16.05/−28.04%;4/16/17denial
−26.31/−25.16/−31.08%. Heapgrowth reducedto1allocation; zeroallocationnot
achieved. One-resource denial+~3% control retained; warmtouchnearparity.
Duplicate-heavyspan reservation can retainlarger weakvectorcapacity; actual
sceneaccount andtypedgroup unionmaps deduplicate beforethisAPI. These are
notfullfactory/framecosts orFPS; thismitigatesbookkeeping, nottotalturnover.

Next cleanrootcommit→executiontreefastforward→/tmp/persistent_native_build_73.py,
staticverifier72withnewexactsource/version andstrictsource73, full34native
scopes via /tmp/run_source_cohort_correctness_71.py73native-correctness-73.
NoG3workerthreshold/settingschange. Within-view ordinary sources average423–867
bycensusendpoints; no segmentmaxhistogram oraggregated8192runtimeproofexists.
ScalarMVPandcompactalreadyuseequivalentNEONmath; snapshot/array/merge costs
mustbeincluded beforeclaimingaggregationuseful. Owner69122appimmutable,
no launch/login/profile/push; fullarchitecture/performanceacceptancepartial.


### Source73 validated and separately staged69129

Exact3841527bbd944f9a6283b12ad45acdaa878d073a required0/0/0,34.712s build;
handle60951 terminal0. Static native73 (requiredverify_app inside) and strict73
PASS. Full34 renderer scopes254316 assertion-executions/348case-executions
PASS,160.976s; common6063/48, actual26034/80, nativeanimated7925/5 unchanged.
Handle17867 terminal0; no tests/cost broadening afterpass.

retain-cost-gate-73.json proves the measured selected common source SHA matches
the integrated source; it limits results to standalone retain boundary, not
wholefactory/frame. Fullcurrentnative checks pass. Old source72rendererroute
profitability remains explicit prior evidence; settingspolicy unchanged.
Stagehelperrun73 withcombined-policy/evidence-note-73 andverifier72 accepts
exactcurrent source/version; handle56807 terminal0.
Destinationarchitecture-combined-69129, provenanceVERIFIED,8273 identical app
inventory nodes, stagedsignatures/ARM64/imports/features andSHA256SUMS PASS.
ExecutableSHAe958bb09075957bd84d921459af7802d734d69f75a353d5a764b5805370c99b9.
No viewer/runtimeaction; owner69122 app remainsimmutable.

Additional69122 evidenceaudit: probe tile/separate3960/0 quiet,3692/0club
equals allsampledsuccessfulprobeGBuffer views. Main0/0is probe-onlycounter
semantics, notmainfusiondecline. ExistingRG8visibility memoryless/dontcare and
compositionownershiptransfer alreadyremove intendedintermediates; persisted
colour/depth have real laterreaders. No additional safe probe pass/store/load
elimination proved. Do notinventunusedprobe switch ordropSSAO/post features.

RemainingactualG3contribution andwholeengineperformanceacceptancepartial.
Nextconcretework: readbusy/quiet cpu-analysis.json summary.by_disjoint_main_scope
shadowcapture/sourcepublication leaf costs toselect substantial existing owned
CPUwork; iftestingfamilyaggregation, add rawcurrent-scalar vscompactserial vs
Dispatch completecost atactual423–867meanviewscale, including snapshots/output/
admission/release. Do notactivate8192aggregatewithno measuredsegmenthistogram.
Keepcacheturnover240/274open; PPD295partialmitigationdoesnotclosethem.
No additional launch/login/profile/push authorized.
