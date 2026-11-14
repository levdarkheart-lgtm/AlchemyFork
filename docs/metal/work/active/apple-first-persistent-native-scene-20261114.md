# Persistent native scene membership and small mixed commands

Status: doing; continuation of the original active Apple-first architecture goal.
Previous goal turn: **progress**. Exact69008/f3a7ac5f58 was built/tested/staged;
its [results](../../research/apple-first-whole-engine-integration-2026-11-14.md)
and all earlier artifacts remain unchanged. This turn continues remaining source
boundaries; it is not another viewer launch/profile or shadow-only stopping point.

Worktree `/private/tmp/alchemy-apple-first-architecture-g2-g5`, branch
`codex/apple-first-architecture-g2-g5`, starting clean267f89749b (documentation
handoff; compiled candidatef3a7ac5f58). No other worktree is being edited.

## Source/original invariant

Original1eb44bbd and current viewer both require the CPU cull/stateSort work for
LOD, occlusion, texture/cache priority, visibility policy and alpha groups.
Main display updateCull1972/stateSort2069 and probe display_cube_face2402/2427
lead to postSort, which walks each accepted group's mDrawMap and pushes each
LLDrawInfo into per-view render maps. Main deferred pools walk those maps;
native probes' captureMetalReflectionProbeScene walks them too. Both then call
captureMetalPublishedWorldBatch. Do not delete these CPU policy side effects.

A group build is incomplete during registerFace: subsequent faces can extend its
last range/material list. Deferred mesh work completes after postSort's map walk,
then LLVertexBuffer::flushBuffers publishes the native stores. Therefore seal a
complete native group packet after that flush, use it in a later view, and keep
current raw native enumeration when dirty/ineligible/unpublished. Region/model
transform is copied and separately checked; shift alone need not dirty geometry.

G-buffer setters already cache scalar PSOs/buffer ranges/textures/samplers. The
remaining legacy/PBR path still visits up to32 individual texture bindings per
draw. Stable uniform publication does not solve that resource/command boundary.
Native resource tables are a prerequisite for useful ordered mixed ICB work;
changing the old2048shadow threshold does not provide that architecture.

## Implementation in progress

- Root: completed group/pass payload at spatial-group producer state, native
  range admission in LLCullResult and merged main/probe simple capture. Preserve
  raw/group/raw ordering with raw offsets; account native counts, original index
  totals and query positions. First slice: non-rigged region-space PASS_SIMPLE,
  no animated UV/material/avatar/bridge payload. Other families stay on their
  existing native path. No full-batch temporary reconstruction by workers.
  Per-view capture still copies ordinary batches; compact source/instance layout
  remains further work, not falsely described as eliminated.
- Root: clearDrawMap/registerFace/shift withdraw native group publication;
  cached matrix comparison detects independent region rebases. Dirty mesh/group
  stays raw; postSort seals after deferred writes/flush. Extra source->group
  publisher leases must disappear before native COW decisions.
- Architecture agent: lazy weak State::observe dependency signal, mutation and
  eviction notify derived caches before releasing publisher leases, without
  changing child revision on eviction or breaking PPD120 reentrancy. Six new
  native publication cases authored/frozen; not yet executed. Now owns actual
  group publication/capture/range fixture in alpipeline_buffer_publication test.
- Opaque agent: native legacy/PBR resource-source snapshots, exact C++ native
  texture generation leases (native object retains alone do not protect COW),
  bounded resource argument tables, matching shader entrypoints and renderer
  integration. Preserve arithmetic, exact linear/sRGB sampling and anisotropy.
  Tables own resources, never their Source/context; shader/sampler generations
  and pending GPU leases remain explicit. Scalar native fallback remains intact.
- Residency agent: NEW native world-resource correctness/cost fixture; actual
  direct/prepared outputs,8visible indexed slots, formats/masks/reverseZ,
  equal-depth/query order, pending/abandoned GPU ownership, sampler/shader changes,
  allocation/capacity fallback. Cold/warm/churn16/512 and main+two-probe full paths.
- Root: source factory seals resource generation tuples once per dirty flavor,
  using pointer-matched aliasing leases from already-accounted Payload.resources.
  New nonpersistent controls RenderMetalWorldResourceTables and
  RenderMetalGroupSources are OFF until complete-path gates. No default gain
  or useful GPU activation is yet claimed. Logging distinguishes request/bind/
  fallback and actual group admission/capture; typed failure reasons are pending.

Exclusive edits: opaque owns almetalworld.h/.cpp, almetalworldresources.h/.mm,
almetalpipeline.h/.mm and builtin shader. Residency owns new resource test.
Architecture owns existing alpipeline buffer publication fixture; its common
publication files are frozen. Root owns pipeline, group/spatial/volume owners,
settings, CMake and all canonical documentation. Root alone executes builds/tests.

## Compatibility and verification

Independent read-only audit: directional and spot shadows each call sortShadow
with sShadowRender=true before cache/admission/capture, and do not consume main
converted ranges. Impostor/preview own results and are excluded. Picking walks
spatial partitions; highlights use faces; debug is excluded. hasRenderBatches
now includes native counts. RenderDepthPrePass is a remaining raw map consumer;
new group conversion excludes that control (PPD158). Existing generic native
world prepass appears inert; no new visible depth regression is attributed.

PPD157 records the unexecuted test-author self-review finding: a closed synthetic
GPU gate followed by shader reload would wait on itself; shader reload checks
were moved after gate completion. Sampler/cache changes remain tested pending.
No production deadlock or actual hung run is claimed. Next defect159.

Private evidence: canonical artifacts/metal/apple-first-persistent-native-scene-20261114.
Next: finish/freeze sources, run common derived-publication cases, actual-owner
native group/resource tests, quiet complete-path cold/warm/churn cost matrix,
then required clean-source doctor/configure/build/tests/source/static/staging.
Retain failed attempts and reject net regressions. No viewer launch/login/profile
or push. Original GPU-before-discovery/full cull/encoding/pass/memory architecture
and whole-viewer acceptance remain partial and active.


### First executed prerequisite check

Narrow dirty-source diagnostic: cmake --build build-Darwin-ninja-os-arm64-g2-g5
--config Release --target llcommon_tests -j8 exits0/5.729s. Exact common binary
[native-publication] --reporter compact exits0/0.011s,131assertions/11cases.
Source hashes/argv/logs in derived-publication-1.json at the new evidence directory.
This tests actual sixnew dependent-publisher/reentrancy cases and retained source
publication cases; it does not validate the unfinished renderer integration or
a new viewer candidate. Common source assignments are frozen.

Common prerequisite committed29f0140686. Existing affected working-set cases
[native-source-working-set] also exit0,82assertions/7cases (derived-working-set-1
receipt). No common source change after the passing binary. New GPU/group owners
remain dirty implementation; do not treat69008 checks as validation of them.

Native core diagnostic build1 failed (PPD159): three private encoder access errors
in the new resource helper. Add narrow friend grant then rebuild. Shader/core
source edits overlapped this diagnostic, so it cannot qualify as a frozen viewer
build even after repair. No live viewer execution. Next defect160.


### Source freeze before the combined required build

PPD159 narrow friend repair is implemented; encoder internals remain private.
PPD160 review found group residency was not charged/touched independently: child
source age defaults2, not a long grace interval. Repaired with a unique group
serial/resource union in the SAME working set, O(1) warm touch, and owner-held
weak dependency relay that ignores child cache eviction while still withdrawing
on true producer mutation before COW. Failed group budget admission never creates
a strong unaccounted publisher, even from a valid transient raw reader. Eight
actual native group cases plus cold/warm/churn costs are now authored/frozen.

Resource core/wrapper/shaders and four normal resource cases plus old69008shader
parity/cost cases are authored/frozen. The prior library is explicitly pinned by
path/hash in previous-shader-baseline.json; hidden parity rejects accidentally
loading a library containing the new argument entrypoints. No native result is
yet claimed. Source factory excludes unused forward-only resource flavors.
Every resource fallback stage has typed counters/logging. New group/resource
controls remain nonpersistentOFF until measured selection. Current nextID161.

Next exact commands: freeze/commit all integrated source, required doctor, exact
Release configure and tools/metal/build.sh build, then focused actual-group and
resource tests with MetalAPI/shader validation, old-library parity, quiet complete
costs, broad native/source/static checks and separately staged combined app.
Never launch/login/profile the viewer or push during this phase.

Required clean144aa7 build1 doctor/configure/build allpass0 (build839.404s,1507
steps). Native actual group/publisher/capture/ranges1152assertions/18cases pass
with MetalAPI/shader validation. Resource4cases3pass,7670/7734assertionspass;
all64fails identical before-query>0 using a subpixel box with no raster centers
(PPD161). Pixel/table/lifetime/fallback/joining results otherwisepass. Old shader
parity notyetexecuted; stoppedonfailure. Fixfixturecalibration and expand actual
small group/varied texture costcoverage before defaultselection. Next162.


Source2 repairs PPD161 fixture raster coverage (.08halfextent at32pixel viewport)
and PPD162 inactive-pass publication. Resource correctness also exercises actual
rigged material/PBR PSOs with owned16float joint1 palettes across formats. Hidden
cost coverage now includes group1/4/8 aswellas16/512, and resource1/4/16/512
standalone plus512main/two128probe. Resource cases distinguish shared vs8distinct
native texture tuples, not merely8uniform variants overoneidenticalresourcetable.
Cold/warm/churn keys and actualsourcevariants are reported; original short/
shared-only cases remain in earlier source144aa7 for history, not falsely taken
as comprehensive small mixed workload acceptance. Normal group cases unchanged.
All assignments frozen, next163. Native1152/18 group results at144 remainprior
evidence; rerun altered resource/previous-library cases on exact rebuilt source.


### Measured source2 and incremental repair

Clean7422adf812 required doctor/configure/build2 all exit0 (build155.560s).
Actual native group1152assertions/18cases, resources8889/5 including rigged,
and previous69008compiled-library parity4721/1 all pass with Metal validation.
Exact receipts: native-focused-correctness-2.json, frozen-source-build-2.json.
Neither a new viewer run nor a live performance improvement is claimed.

Quiet complete-path-costs-1 has both processes exit0. Warm512 native groups
reduce CPU0.592084->0.241459ms(oneview),1.819750->0.812500ms(threeviews).
One changed source per16-source group instead costs0.705292->2.267620ms and
1.964000->2.763170ms: PPD164 registered before repair. Resource eight-slot warm
paths reduce CPU, but single-slot warm512 loses4–15% and cold/churn also costs
more (PPD163). Full adverse/small/shared/distinct rows are retained in
complete-path-costs-1-summary.json; group fixture has5samples, not a tail gate.
No unconditional group/resource default is justified from favorable rows.

Active repair: architecture agent exclusively owns group publication .inl and
existing group fixture, splitting persistent CPU-only descriptions/weak native
generation identities from completion-owned strong leases; refresh only changed
rows after producer writes. Root owns spatial-group Layout lifecycle and call
site integration. Opaque agent exclusively owns resource .mm and world.cpp:
use certified immutable hot proof and bounded command retention identity cache;
apply matching numeric-source retain improvements to direct encoding too.
Residency agent is read-only auditing next compact source/instance and mixed
command boundaries. No builds/tests run concurrently with edits. Next defect165.
Next: freeze/review integrated repairs, build3, rerun actual native correctness,
old-library parity and complete costs; preserve failures and reject regressions.
Original whole-engine architecture/performance goal stays ACTIVE/partial.


Independent PPD163 component diagnostic uses exact file hashes while disjoint
newview/llrender group tests are authored: resource-hot-proof-diagnostic-build-1
exits0/9.446s; native-1 exits0/4.909s,12379assertions/7cases with Metal validation.
New tests cover >16 recurring sources, command-owner cache collisions, pending
completion/abandon and old valid table leases after context/sampler invalidation.
No clean whole-viewer build claim. Quiet component costs-1 exits0/2.254s; warm
512/eight-slot CPU gains23–50% and completion gains are observed, but legacy
single-slot/shared three-view CPU still+4.47%, with a distinct one-view completion
outlier. Keep all120rows, small/cold/churn included; no final default selected.

PPD165 separately records CPU-only Layout metadata outside native-cache bounds.
New owner-only weak layout LRU caps2048layouts/8192rows/32MiB estimated metadata,
evicts only matching publisher serial, leaves immutable queued readers valid,
and logs occupancy/evictions/rejections. Root integrates Layoutreset only at
membership mutation; child source writes preserve descriptions, release native
publisher leases before COW. Native budgets and actual completion stay separate.
Next compact slice is specified in private /tmp/apple-first-compact-contract.md:
general nonrigged main/probe simple/legacy/PBR source references plus copied view
fields and ordered raw/ref indices, with explicit bump-edit/raw preservation.
Current CPU-vector reclamation does not retire every Source/GroupPayload before
GPU completion: separate CPU-description and native-tuple aggregates remain
part of that next slice. Mixed persistent commands follow compact inputs;
CPU cull LOD/visibility/texture-priority side effects remain on scene owner.
Next defect166. All new controls remain diagnosticOFF, original goal ACTIVE.


Final policy review extends PPD165 with current-frame CPU-cache protection:
when a busy frame fills the budget, decline incoming optional cache admission
instead of cycling already-used groups across main/probe views. Cost-fixture
12samples/max/raw output and exact one-changed-row getter checks are authored.
Independent review found PPD166 CPU eviction-observer reentrancy; root traced
the same missing exact tuple/insertion guard into native source ledgerPPD167.
Both registered before repairs; group code has NOT yet executed. Architecture
agent owns CPU group helper/fixture fixes; residency agent now exclusively owns
llcommon/alnativesourceworkingset.cpp and existing focused tests. Root owns
pipeline/lifecycle/docs and all execution. Recheck caller tuple after callbacks,
never erase a newer lookup, and retain exact accounting with no duplicate entry.
Resource component source/tests remain frozen and independently passing.
Next defect168; clean combined build3 is pending both source freezes/review.


ARM64 native-batch-layout-size standalone Clang compile/run both exit0:
ALMetalWorldSimpleBatch3672bytes/alignment8, MaterialSlots384, PBRSlots1600.
Receipt pins exact world.h hash and command. The next304byte view/reference
proposal addresses measured representation size, not a copied sizeof estimate.
Private persistent-command contract records physical constant-page identity and
source/table/PSO generations required before small mixed ICB reuse; keep real
400byte common dedup and original query/cull/depth barriers. A lower old2048
threshold alone cannot supply those ownership guarantees. No new GPU command
implementation or acceptance is claimed by these private source contracts.


Source freeze3: all PPD163–167 repairs and fixtures authored/reviewed, diff clean.
Group adds four actual cases including reentrant replacement and current-frame
CPU-cache protection; common native residency adds four Observer cases covering
age/pressure, invalidation, same-request admission and clear/newer publication.
No group/common new execution result yet. Resource12379/7 remains component
only. All agent assignments returned; root runs common focused validation then
required clean-source doctor/configure/build3/native correctness/costs. Private
compact and persistent-command contracts are copied into E2 with checksums in
next-source-contracts.json; /tmp copies are convenience, not sole recovery.
No launch/profile/push; subsequent compact/source-instance work remains next.
