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
producer9882/47 pass. Native19 runner is still finishing the resource/old-library
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

Next finish native19, obtain both source freezes, review replacement order/
failure/query invariants, commit/integrate locally and run fresh required20,
native correctness and the unchanged48-case complete cost scope with actual
post-flush same-view promotion included. Preserve18 costs and GPU outlier.
Do not enable the path or stage it as a completed architecture before evidence.
Next global defect197. No viewer operation or push; canonical owner tree and
earlier staged owner/reference applications remain untouched.
