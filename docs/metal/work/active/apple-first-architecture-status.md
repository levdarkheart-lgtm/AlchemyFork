# Apple-first architecture implementation status

Updated2026-11-16. The original whole-engine architecture is **active with runtime-backed follow-up work; acceptance remains partial**.
The owner has now supplied quiet-house and teleport/club runtime samples; see
[the two-scene analysis](../../research/integrated-build-69122-quiet-club-20261115.md).
Detached render-preparation counters stayed0 in both, so useful parallel-render
benefit is unproved despite implemented callsites. Cache turnover also needs
Architecture Session follow-through. Earlier blocked status below is historical. Goals1–5 landed ownership/admission foundations, but ordinary-scene useful parallel rendering was still missing in69122. The earlier
[source audit](../../research/apple-first-engineering-delivery-20261115.md) did not establish that outcome; treating the remaining work as only owner acceptance understated the implementation gap.
Producer refresh and upload follow-through is delivered in69134; production owned-view capture/encoding overlap is now integrated in source, still defaultOFF and awaiting the current build/correctness/complete-cost gate. No whole-viewer performance gain or completed release gate is claimed.

Owner-profiled source: **beb6837b71f0429f116fef8e2ad7cc5d166bc010**, build
**26.4.0.69122**, experiment branch `codex/apple-first-native-cohort-consumption`.
Doctor/configure/build, ARM64/signature/import verification and strict source
audit pass. All34 renderer scopes pass on these exact binaries:254120 assertion-
executions/346 case-executions. All four48-condition animated cost comparisons
pass their execution/parity/accounting checks; that does not mean they are faster.

| Boundary | Integrated behavior | Selected configuration / remaining acceptance |
|---|---|---|
| Shared scene/resources | Producer generations, immutable material/resource tuples, dirty subscriptions, retained static groups, exact native leases, shared main/shadow/probe capture. | GroupSources and two-frame stability ON; owner loading/teleport/rebase parity pending. |
| CPU preparation | Immutable owned inputs, bounded shared credits, joined large-shadow preparation, scalar recipes, fairness and recovery. | World workers OFF after measured completion regressions. Owner frame-tail benefit pending. |
| Animated ownership | Shared attachment records and view-specific system-body palettes; retained regular-rigged direct and GPU consumers preserve current poses, masks and queries. | Optional animated direct and GPU modes OFF: direct near parity; GPU commands/visibility cost more. Owner animation/stability acceptance pending. |
| Visibility/submission | Persistent static/animated tables, ordered native commands and conservative visibility; exact query/raster splits and completion ownership. | Optional banks and CPU/native visibility OFF after unfavorable or incomplete complete-path gates. Existing native shadow specialization retained. |
| Unified memory/admission | Changed-range/COW storage; shared CPU/native/decode/upload/probe/maintenance admission; completion retirement and bounded caches. | Owner physical working-set, loading and long-session evaluation pending. Cache ledger scopes overlap and are not total RAM. |
| Terrain/probe lighting | Terrain sharing selected at measured16-patch threshold; eligible probe/base lighting fusion and empty lighting-work avoidance. | Integrated; owner whole-engine evaluation pending. |
| Combined candidate | One source72 app includes these foundations and all experimental consumers. Separate architecture-combined-69122 staging is VERIFIED:8273 inventory nodes match the built app; signatures/ARM64/imports/features and evidence hashes pass. | Ready for owner evaluation through the exact launcher linked below; original apps and69102 checkpoint preserved. |

## Measured selection

- Accepted retained scalar/cohort path: source55 warm-many CPU8/8 cases faster,
  median28.05%; settled completion7/8 faster, median14.60%. Cold debt remains
  recorded. This is a contained path comparison, not an FPS improvement.
- Source72 animated direct: warm64 owner CPU+0.20%/+0.27% for one/three views;
  arena publication bytes0.772x/0.755x. Cold CPU−0.50%..+2.16%; resource churn
  +0.12%..+3.27%. This is near parity with reduced publication bytes, not a
  consistent net speed gain or a physical RAM measurement; keep OFF.
- Source72 full-count animated GPU commands: warm64 settled completion
  +11.56%/+8.96%; visibility adds further overhead. Keep both OFF.
- CPU static visibility helps outside-bound workloads but adds all-visible/cold
  overhead; world workers and small terrain sharing also lost their cost gates.
  Those decisions are retained rather than forcing more hardware activity.

All four animated comparisons retain1/2/4/64 sources, one/three views,
cold/warm/pose/resource churn, denials and loading samples.96 indices/source is
an explicit fixture limitation; saved club data does not establish per-draw
geometry sizes. Exact receipts and failed attempts remain in the
[execution ledger](apple-first-direct-cohorts-20261114.md) and PPD287–293.

## What remains

Finish and validate useful owned CPU/native overlap on the actual small-view workloads, with original command admission, query/projector settlement, lifecycle joins and retained CPU-byte bounds. Keep rejected small-family preparation and SSAO fusion evidence; select work only on complete-path results. Then evaluate the combined viewer with the owner across busy, quiet, loading, turning and teleport scenes: frame times/tails, input latency, physical memory, stability and retained SL features. Any reproducible failure creates a concrete repair item. Whole-viewer performance,
feature parity and `VALIDATION.md` release gates remain open. No viewer launch,
login, live profile, push or release was performed in this continuation.

## Exact delivered candidate

[Launch Apple First Combined.command](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-combined-69122/Launch Apple First Combined.command>)
selects the measured policy on the separately staged26.4.0.69122 app.
`architecture-combined-69122/provenance.json` is verified;8273 file/mode/symlink
nodes match the built source app, executableSHA256
`101a327c68852a44e2ba435cd0744a9beaf4582e511fe16b239f171069c57428`.
Staged static verification and SHA256SUMS recheck pass. The app and launcher were
not executed. Evidence root is the canonical checkout’s ignored
`artifacts/metal/apple-first-persistent-native-scene-20261114/` directory.

[Owner run card](apple-first-owner-evaluation-69122.md) pins69122 and the previous
native69085 trial, settings/workload context and outstanding whole-engine evidence.

Post-delivery audits02/03 revalidated the clean3d06bdb708 worktree, verified
source72 candidate and missing runtime acceptance. No independent mandatory
implementation/staging work was identified. Completion is unproved; the full
objective is retained. Next action is owner combined-viewer feedback or
subsequent explicit runtime authorization under the existing goal restriction.
Do not rerun passing checks or invent additional phases to fill this dependency.

Latest owner-authorized samples:301.061s quiet and300.725s teleport/club, both
recorders/offline analyses exit0, owner closed viewer. No ERROR/FATAL rows,
24/129WARNING inactual windows; PPD294 cleanup observation nowregistered.
No source/app/settings change or push. Testing Integrated Build is handing the
report to Architecture Session for implementation decisions. Different scenes,
aggregate means and no old-build paired run do not prove an FPS gain or release.

## Active follow-up after69122 runtime evidence

The house/club recordings expose native-cache turnover240/274 and no exercised
contribution from the two detached renderer preparation mechanisms241. The old
blocked-runtime handoff above is historical; the analysis arrived as78edcee323.
PPD295 now investigates bounded local resource staging before native byte
admission. Caps, source generations, required readers and completion retirement
remain unchanged; this does not repair the whole cache policy or prove FPS gain.
Independent G3 source work examines coalescing already-owned small family inputs
within a view, preserving order and bounding all copy/job/join costs. No blind
threshold reduction or forced worker activation. Tested69122 stays immutable;
no further launch/login/profile/push authorized by the testing handoff.

## Previous implementation candidate —69129

Source3841527bbd944f9a6283b12ad45acdaa878d073a adds one native-resource vector
reservation at the first validated unique append. Selected after actual-source
retention-cost comparisons; inline staging rejected for its cold1regression.
The selected change reduces vector growth, keeps one scratch allocation on
denial, and can retain extra weak-vector capacity for duplicate-heavy callers.
It is a partial bookkeeping mitigation under PPD295, not a cache-policy repair
or whole-viewer FPS result. All caps and renderer settings are unchanged.

Doctor/configure/build, static native/strict and full34renderer scopes PASS:
254316 assertion-executions/348case-executions. New separate
`architecture-combined-69129` provenanceVERIFIED,8273 identicalappinventorynodes,
staged static verification andSHA256SUMS PASS. [Exact launcher](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-combined-69129/Launch Apple First Combined.command>)
is unexecuted; profiled69122 app is preserved. Wholeengine/G3/performance
acceptance remains partial and active. Next priority is actual owned-work
granularity and retained-source churn, guided by the captured CPU scopes.
Probe tile fusion already covers all successful sampledprobeviews; no further
safe probe-pass removal was established. No new live capture is authorized.


## Latest integrated candidate —69134

Renderer source `a5ea338f22d2ef3b419e6ff47c3cc7cb625695ce` includes two
runtime-guided changes across scene publication and texture ingestion:

- PPD296 reuses dependency subscriptions when content changes but the exact
  State and ordered signal identities are unchanged. Actual notifications and
  source revisions remain authoritative. The48-cell component comparison shows
  changed-content savings65.55–99.83% with no allocations. Low-fanout cold and
  topology replacement remain slower; there is no universal speedup claim.
- PPD297 uses ARM64 interleaved vector loads/stores for U8 RGB8 storage
  expansion, preserving RGB/RGBA/BGRA bytes, opaque alpha, padding and tails.
  All120 paired component medians improve. Fresh4096² conversion changes
  58.115ms to2.665ms; this excludes viewer/GPU work and does not establish FPS.

Periodic diagnostics report topology refresh/reuse/subscription/failure counts
and RGB CPU-stage calls/pixels. The latter counts completed CPU staging before
GPU commit; retries, mips and subregions are not unique images. Both diagnostics
are compiled into the exact app; runtime frequency remains unknown.

Doctor/configure/build and static ARM64/signature/import/features checks pass.
Strict source:3595files,0blockers,0errors. Full34 renderer scopes pass with
Metal API/GPU validation:1,130,416 assertion-executions/349 case-executions.
Additional source-specific scopes pass: dependency378assertions/7cases,
image/upload6464/34, corrected segment876100/1. These scopes overlap and should
not be added as unique coverage. Optimized/sanitized kernel checks and original
failed attempts remain in the component receipts.

The corrected mixed-family matrix passes108 configuration triples/8748 raw
samples, with0/108 median wins for either aggregate preparation route against
current scalar preparation. Dispatch median overhead at867/1158draws is
45.16%/40.89%. No threshold/queue/feature change follows. Encoding/GPU and
compact-certificate validation savings are excluded; existing large-family
specialization is retained.

[Launch Apple First Combined.command](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-combined-69134/Launch Apple First Combined.command>)
selects the same renderer policy as profiled69122 and staged69129. New69134
provenance is VERIFIED;8,273 app files/modes/symlinks match the built app,
staged native verification and SHA256SUMS pass. ExecutableSHA256
`6320a5c589bd55b0a72758c67d35d5d068c02e186cee405bed9882c1cfef41d2`.
The launcher is unexecuted; both earlier candidates remain unchanged.

Useful render CPU overlap remains a concrete open acceptance item. The bounded
source audit finds that `renderSpotShadow` consumes explicit native inputs,
but its renderer command, frame arena, material caches, stats and settlement
state are shared with subsequent capture. A batch-only worker would race.
An isolated non-query encoder context plus complete palette/source/target
ownership and owner-side settlement is needed before testing overlap. Borrowed
probe/hero commands and query views also have transaction dependencies. This
is an identified ownership boundary, not an implemented worker or a proven
profitable next path. No new live capture was taken.

Exact receipts under the existing private evidence root:
`frozen-source-build-75.json`, `native-correctness-75.json`,
`native-dependency-topology-75.json`, `native-image-75.json`,
`native-segments-75.json`, `shadow-segment-costs-75-summary.json`,
`combined-component-cost-gate-75.json`, `static-native-75.json`,
`strict-source-75.json`, `stage-75-sha256sum-check.json` and the staged
`architecture-combined-69134/provenance.json`. Owner whole-viewer feature,
loading, memory, frame-time and stability acceptance remains pending.


## 2026-11-16 — Actual owned native overlap delivered in69149

Frozen source `1f83c401af038e4d5140e3a7963cb7a8f02a746a`, local experiment branch
`codex/apple-first-native-cohort-consumption`; execution tree has this exact SHA.
Production captures the next view while a single worker encodes the preceding
owned packet in private mutable native state. It adopts the original command,
completion and shared2-group×6-view admission rather than creating another
transaction. Native query arrays are frozen; query tickets, projector validity,
commit/cancel and callbacks settle on the owner after join. Main G-buffer,
query reads, reset/reload/swap/readback and teardown drain outstanding work.
Borrowed probe/hero transactions retain ordinary native rendering.

CPU packet arrays carry their preparation reservation through encoding, joins,
result moves and partial recycling, separately from GPU-owned generations.
The common policy bounds startup jobs and32MiB logical packet capacity; this is
not physical RAM or another charge for shared native geometry/textures.
Reserved-empty recycling and GPU-independent retirement are native-tested.

Unconditional overlap at256 draws lost17.9–19.1% owner-phase wall and7–7.7%
settled time in source79, with almost all real overlap missed. Preserve that
failure. Source80 uses one shared cheap native-batch work gate BEFORE context,
snapshot or dispatch. Below1024, requested overlap keeps the ordinary serial
route;1024+ uses actual packet/adoption/worker/join. Dedicated logs report work,
budget/adoption/timing/readback/borrowed denials and logical CPU packet debt.

The complete12-cell native comparison covers32/256/867/1024/1158/4096 native
batches, query/nonquery, three authored views, cold+warmup+8 counterbalanced
pairs,240 raw samples. All pixel/query comparisons and20476 assertions pass.
All six eligible medians improve owner-phase9.0–16.4% and settled6.5–12.8%,
with real worker/next-producer intersection in every measured eligible sample.
The six smaller cells prove0workers/context; their timing variation remains in
full. Owner-phase is elapsed wall, not summed CPU-core time. GPU-sum at4096
nonquery rises10.16% while GPU envelope and settled wall fall; cause and
competition with a full busy-scene GPU workload remain unknown.

These are32×32 shared-geometry component costs. Live LL traversal/culling,
asset publication, pose solve and the LL query-ticket pool are excluded.
This selects an OWNER trial, not a viewer FPS or completed product claim.
The earlier295/296/297 measured files and all prior input hashes are unchanged;
`component-cost-reuse-80.json` records that limited reuse explicitly.

Doctor/configure/build, static ARM64/signature/import/media/voice/codec gates,
strict3597files/0blockers/0errors and full34renderer scopes1130524 assertion-
executions/356 case-executions pass. New task2424/5, bytes49/4, packet59/3,
context392/5 and shared-shadow/material/reload18390/29 also pass; counts overlap.

[Launch Apple First Combined.command](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-combined-69149/Launch Apple First Combined.command>)
requests the selected1024+ policy on the separately staged69149 app; use this
launcher to exercise the selection. Provenance is VERIFIED;8273 inventory
nodes match, staged static verification and all checksum rows pass. Executable
SHA256 `b0de99a90277f19c12f9a93edc59f9b6d8ecd73a2d3e3f0e0b672cc8cceb6745`.
The app/launcher were not executed;69122/69129/69134 stay untouched.

Exact private receipts: `frozen-source-build-80.json`, `native-correctness-80.json`,
`native-shadow-task-80.json`, `native-preparation-bytes-80.json`,
`shadow-task-cost-80{,-summary}.json`, `static-native-80.json`,
`strict-source-80.json`, `stage-80-delivery-check.json` and staged provenance.
PPD241/299 remain owner-pending. Next assess exact69149 runtime activation,
frame tails, full GPU competition, queries/projectors/probes, loading and
stability. The69122 profile cannot establish69149 performance. No new viewer
launch/login/live profile or push; original whole-engine goal remains active.

## 2026-11-16 — Whole-engine completion observation in69151

Frozen81/source `2038c60c365b63d6b55e705cb0660efb781106b2` preserves69149's
selected renderer. Independent source audits trace actual producer consumption
through main/probe/shadow, native command/visibility consumers and shared
upload/decode/probe credits. Their rejected optional activation is not a
forgotten callsite. Early GPU-owned discovery remains unimplemented; current
GPU visibility follows CPU discovery. Original whole-engine acceptance is open.

A concrete G5 measurement gap (PPD300) remains separate from optional credits:
required-main native completion did not reach the frame-plan diagnostics.
The new owner-only observer samples the window's existing Metal scheduler at
outer-frame begin and before ordinary probes after required-main accounting.
Periodic `Native main completion` lines report samples/unavailable/full/peak,
pending/capacity, submitted/completed/failed and last-submission retirement.
These counters are independently sampled. Retirement includes failure and is
neither successful rendering nor scan-out. No pointers, timing flags, extra GPU
work, admission or probe-cadence changes enter this observer. A normal pending
frame is not evidence of harmful queue pressure; policy needs real evidence.

Doctor/configure/build, native census60/1, existing outer-frame56/2, probe136/3,
common budget6112/52 and native bundle/strict3597zero blockers/errors PASS.
The source-diff whitelist and exact original FramePlan implementation proof are
`renderer-correctness-reuse-81.json`. All three frozen native test binaries are
identical to80. Earlier34 renderer scopes and12cell costs are explicitly reused,
not relabeled as new81 executions. `combined-component-cost-gate-81.json`
records unchanged measured helpers; new per-frame sampling has no live timing.

[Latest combined launcher](</Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-persistent-native-scene-20261114/architecture-combined-69151/Launch Apple First Combined.command>)
uses the same selected renderer policy as69149. Staging is VERIFIED8273 nodes;
all19 checksum rows pass. ExecutableSHA256
`e7b29adc9214f0851a2089960edb1c422a6b6791e42f656f9a23f224967537c8`.
69149/69134/69129/69122 stay unchanged; no app/launcher execution or push.
Next: exact69151 whole-engine runtime activation/quality/loading/stability,
frame tails and main/probe GPU/pass scaling; existing69122 evidence cannot
prove those outcomes. This is progress on observability, not goal completion.


## PPD301 early static classification — tested and rejected for enabled policy

Frozen renderer source `9be33e2b3e627bdb49f1fdae65076b513911bf4f`, build
**26.4.0.69155**, now contains the actual before-main-cull resident-group
producer and a later command reader. The weak2048-group/8192-row index accepts
only successful immutable publications, copies current region/camera data and
owns no idle CPU graph or native geometry. Native bounds encoding drops its
snapshot owners before CPU cull. Later reads require successful producer
completion, exact current cohort/control-block/used-matrix proof and sparse
logical remapping. Reader leases protect argument slots until GPU retirement;
original same-command checks, CPU cull/occlusion/pass policy and queries remain.
This is bounds classification, **not GPU-owned world discovery or CPU binding
elimination**. Depth prepass and incompatible modes decline speculative work.

Doctor/configure/build PASS; current native early500assertions/4cases, actual
producer75/2, old visibility7521/13 and shader reload1248/6 PASS with API/GPU
validation. Static exact69155/signature/ARM64/imports/features and strict3598files,
0blockers/0errors PASS. Source83 full34renderer1130599/358 executions are explicitly
reused, not rerun or relabelled as84: only docs/new early-test mask inputs changed,
all105 production archives/other native-test objects/native libraries match,
llrender/common binaries are identical, all34 filters exclude the changed test,
and the repaired four-case fixture actually executes on84. Both initial compile
and fixture failures remain in PPD301 history and immutable82/83 receipts.

One24-cell/480-row actual-owner comparison passes195988assertions/1case for
output/query/accounting. The runtime policy is **rejected**: all16 prefix-active
cells increase owner-phase wall medians16.61–86.59%;12/16 also increase settled
medians. Eight-source measured requested views adopt0/128;256-source adopt24/104.
The early result often misses the CPU-consumer deadline once retained capture
becomes cheap. Required loading competes too: one prefix credit reduces new
cohort admission16→15 while outstanding, delaying warm256 one-view settlement
fromsample4 to5. No cap is increased to hide that cost. Startup factory costs,
loading debt, denials, ready peeks, successful adoption, indirect submissions,
direct fallback and all unfavorable pairs remain in the raw receipt/summary.

The fixture includes768unique indices/source and16×16 targets, one-view main
scope plus three-view stress. Its mixed256 groups model outside rows inside a
CPU-accepted group; outside one-row groups are only an upper-bound/stress case
because real octree cull already rejects wholly outside tested group bounds.
CPU cull/capture/bindings still run, and physical slot-cache retirement latency,
real traversal/assets/dependency walking and target-wrapper retirement are not
timed. No viewer/FPS/physical-RAM/ALU-utilization inference follows.

Evidence root: `artifacts/metal/apple-first-persistent-native-scene-20261114/`:
`early-visibility-cost-84{,-summary}.json`, `early-visibility-selection-84.json`,
`native-correctness-reuse-84.json`, current dedicated/static/build receipts.
Tested branch `codex/apple-first-early-visibility-tested-69155` preserves source.
`RenderMetalEarlyGroupVisibility` remains nonpersistent/defaultOFF. There is no
newly recommended/staged app; **69151 remains the existing owner candidate**.

The next G4 boundary must remove actual per-row CPU submission together with
native visibility, using existing ordered cohort command consumers and honoring
required-main publication priority. Existing native bank cost failures remain
constraints; another early-dispatch-only variant is not selected. Broader GPU-owned
discovery and original whole-engine runtime/feature/stability acceptance remain
active and partial. The Testing Integrated Build session retains its separate
analysis responsibility; no viewer launch/login/profile/push occurred here.

## 2026-11-16 — Qualified native banks tested; main overlap proceeds

Source24fe03/build69160 (88) passes doctor/configure/build, exactapp/static,
strict3598files/0blockers and current native2848/4, declaration609/4, actual
producer3029/2, early500/4, plus34renderer scopes1134237assertions/364cases.
These are executed88 results; earlier85/86 fixture/build failures remain recorded.

The complete24cell/720row comparison now observes4816qualified/4816encoded
rows across92 actual classification samples. Missing qualified bounds do not
explain the tested bank cost. The separate native synthetic-bounds oracle
checks real GPU zero-count writes; a qualified census alone does not prove
visible counts. Even common256warmhits6–9 retain GPU penalties: visible bank
+22.30% allinside/+20.99% halfoutside; owner medians−43.36%/−42.91%,
settled+6.97%/−0.05%. Fullbank GPU+15.71%/+26.13%, settled+6.55%/+8.84%.
Small4/8row visible banks also lose. Requestedwarm loading, startup, churn,
default16 publication quota and all adverse pairs remain in the full result.
Banks stayOFF; no viewer FPS or universal GPU conclusion follows this fixture.
The tested submission boundary is implemented, but useful selected native
visibility remains open. Another identical run is not the next action.

Owner69151 quiet/club recordings retain full logs but under2seconds actual
CPU/GPU data because the detailed template used a rolling window (PPD305).
They cannot establish a five-minute bottleneck, phase comparison or FPS gain.
Independent continuous-template20s CPU retention now passes20.803s; no viewer
was relaunched. Owner clarified a new capture is optional help, not a gate.
All186 wrapper warnings were mapped to source;0ERROR/FATAL. Terrain null-buffer
omission differs from original and is repaired locally underPPD125 with three
actual-body cases, awaiting frozen build/tests. Other warnings remain recorded
underPPD122/127/294/306 without unsupported renderer attribution.

Full log telemetry also confirms selected1024-threshold ShadowViewOverlap
actually activated; counter deltas span log telemetry, not retained trace or
requested300s. Final activation review retains exact spans and cumulative/reset
semantics. Activation alone does not prove a speedup. Main-view encoding
overlap with an owned CPU point-light packet is the next implementation
boundary (PPD307); projector/native resources remain serial, original nearby
light updates remain ordered, and join precedes sky/commit/query/publication.

Evidence: artifacts/metal/apple-first-persistent-native-scene-20261114/
static-visibility-command-cost-88{,-summary}.json and selection-88.json;
architecture-69151-warning-review-20261116T072955Z;
capture-retention-preflight-20261116. Preserved owner69151 and older staged
apps are untouched. Goal remainsactive/partial; no launch, push or new staged app.
