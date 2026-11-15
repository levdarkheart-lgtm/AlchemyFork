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
