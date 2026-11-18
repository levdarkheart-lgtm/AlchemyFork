# Shared shadow publication and main completion admission

Status: **doing**, combined source/build/static qualification complete;
Home acquisition and analysis complete; owner feedback pending. The owner redirects emphasis to unfinished Apple-first boundaries after the fresh
[69241 Home/club recording](../../research/home-club-replay-checkpoint-69241-20261117.md),
with correctness/contained cost checks during implementation and another combined
viewer checkpoint afterward. Broader architecture/performance acceptance remains
partial. No shader experiment is selected for this group.

The owner explicitly authorizes unattended Home login using the saved Login
button after a combined candidate is built, tested and separately verified.
Collect replayable frames, CPU/native/resource data, full logs and a separate
normal-rendering control. Close the viewer afterward. Club and owner visual
acceptance remain for the owner's return; no push is authorized.

## Evidence and original contract

Source base canonical5cb45787ebd25b111ba598dbb43e67aebf85843b, buildercb3e148ec9,
same indra4008c621935d5abd374f33e83c94c1241c6bae37. Original rendering
reference1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 retains authoritative per-view
culling/order, opaque→query→covered/pool boundaries, current animation/UV state,
shadow coverage and ordinary-probe bootstrap/continuation behavior. New native
publication must preserve those owners rather than cache a camera's cull result.

Home80–120s has63.946ms periods/38.770ms required-main completion waits and
35.38%main running CPU; club55.869ms/4.027ms and79.63%CPU. Club sun-shadow
owner10.615CPU-s/40s is a measured target, not predicted recoverable time.
These are capture-armed diagnostics with failed normal GPU-gap qualifications,
not uninstrumented or matched whole-viewer performance acceptance.

Current group publication explicitly excludes sShadowRender. Per-record shadow
publication and safe completion leases already exist, but capture resolves and
pins sources per admitted row/view before the encoding worker. Producer-owned
LLSpatialGroup publication/invalidation is an existing hook; prior cull lists
must not be reused by frame/camera pointer. Small-family parallel preparation
already lost cost checks, so its8192gate and serial fallback remain authoritative
until complete-path evidence warrants a change.

Current ordinary-probe admission follows display and required-main CPU recording.
Its budget sees CPU/memory/credits, not required-main GPU blocking. Therefore
the current frame's actual logical-main wait is available before fresh optional
admission. No window-scheduler backlog or summed GPU envelopes are substituted.

## Ownership and implementation boundaries

- Root: canonical source/ledger/register, pipeline.cpp, renderer wait observation,
  outer-frame observation lifetime, diagnostics, CMake, integration/build/stage
  and later unattended runtime. Shared shader source remains unchanged.
- Shadow helper author: isolated worktree
  `/private/tmp/alchemy-shadow-publication-20261117`, branch
  `codex/shadow-publication-20261117`; exclusive new helper/test files. Build a
  producer/group-owned typed immutable membership/range/resource publication,
  observing source invalidation before COW, under existing native working-set
  ownership plus explicit bounded CPU metadata. Warm reuse must occur before
  repeated factory/resource-union work. Per-view pose/model/UV stays current.
  Exact membership/order and visibility/query boundaries remain authoritative.
- Budget author: `/private/tmp/alchemy-main-gpu-admission-20261117`, branch
  `codex/main-gpu-admission-20261117`; exclusive llcommon frame-budget files/tests.
  Explicit frame-token/owner/open-lifetime feedback accepts wall and optional
  threadCPU observations. Only proven nonCPU blocking adds to required-main CPU
  pressure; unknown CPU remains unknown. Existing75%threshold, hard credits,
  fair aged admission and completion retirement remain unchanged.

Main GPU observation is installed only around the logical renderer's actual
older-required-group wait. It does not create a wait, change queue depth, pump
mutable main-loop callbacks or count presentation/probe/utility waits. Outer
MainScope closes feedback at frame end; next frame clears it. Diagnostics must
retain operands and describe admission pressure, not physical GPU busy time.

## Required qualification and next action

1. Review the source contracts and actual hooks. Tests cover publication hits,
   dependency changes/COW, cull/order changes, poses/animated UV, denial/transient
   fallback, prefix/query ownership, resource retirement and bounded CPU storage.
   Global metadata debt must be explicit, not a per-entry bound presented as a
   global cap.
2. Exercise CPU/GPU wait accounting, unknown/stale/off-thread/closed observations,
   saturation, frame reset and real native gated main completion. Preserve
   bootstrap, existing coherent probe continuations and aged progress under load.
3. Compare complete old/new shadow publication/preparation costs for small SL
   ranges and realistic multi-view cold/warm/change sequences. Avoid a last-view
   cache that thrashes across cascades or a lower worker threshold without proof.
4. Integrate one combined source, run doctor/build/static/appropriate native and
   tool/harness checks, separately stage/verify and pin exact evidence. Then use
   authorized unattended Home controls and detailed captures. No live FPS gain,
   visual parity, club retest or product completion is inferred from source/tests.

Pure candidate-utility metadata and the no-output shader strict fixture remain
private, uncompiled/unrun drafts. The mask consumer saved-frame triplet is a
modest one-workload signal; no general threshold is selected. The all-disabled
lobe return Home trial is inconclusive because restored-control drift covers
the variant. Preserve PPD368–370 private experiment/qualification history and
do not integrate that shader on an unproved benefit.

## Integration observations, 2026-11-18

Common feedback is integrated through root0e97041840. Full common component
5619assertions/42cases and actual native main-admission1299assertions/4cases
pass (API/GPUvalidation on, capture off); the utility-wait negative control
records no main pressure. The first native include compile failure is retained
as PPD376. CPU-only captured-membership ownership and join/recycle assertions
are authored; target compilation passed but its native test was withheld after
builder integrity failed (PPD379). Proper tool venv180tests/one opt-in skip
passes; default-Python missing dependencies remain retained PPD374.

Root pipeline integration is a draft: stamp whole visible group/pass spans
before deferred mesh flush, translate exactly through the existing positive-count
filter, and look up the immutable group before per-row factories/retains. Hits
use unchanged fresh pose/model/UV, scalar small-family preparation and the same
single8192-family join. CPU memberships transfer through encoding/join;
completion retains native resources separately. Optional source allocation or
admission failure keeps the required old path. New transient bookkeeping is
bounded to8192group spans/view and131072rows/group, with reused capacities.
An environment-only disable control permits same-binary diagnostic comparisons.
This source has not yet been compiled, selected on cost, staged or run.

Shadow helper qualification exposed real issues PPD373(native metadata retires
prematurely),375(reentrant stale transaction),378(obsolete dependency edges)
and380(pending age reset). Failures and corrections remain private evidence;
378/380 follow-up qualification is in progress. Factory-frontier PPD377 requires
actual old source-factory fast paths and equivalent independent arms; no timing
benefit is accepted from draft fixtures. Broader architecture acceptance remains
partial, including streaming that occurs before current display feedback.

Combined builder7c0d43ccad /indra64ba9c01864996c48873edc2e8860766541bca3a
native qualification now passes978662assertions/39cases, with API and GPU
validation enabled, capture disabled. It includes main-wait accounting, shadow
publication, captured CPU owners, task retirement and all shadow preparation
correctness. Test binaryd8d26e7e5121 and activefae7a43b5b48metallib are unchanged
through that run; receipt checks/shadow-combined-native-01. Full prescribed
viewer build01 is underway after source/generated-state repair; no application
stage/runtime acceptance yet. Sourcebase root40630ecd8e additionally carries
syntax/review-qualified PPD381 late-capture override; it is not yet on the
in-flight builder and requires actual controller fresh-process tests.

PPD379 source repair is verified against exact stage/OID/mode manifests, then
Dullahan's pinned50-file ba893db7 source export was restored from local objects
(3survivors unchanged,47missing files restored). Cached configure02 passes.
Missing generated VerifyGlobs/configure01 failures are preserved. Root cause
of midnight deletions is unknown. Historical Python-peer/LL-reference-compiler
exclusions were traced to missing modules in selectedHomebrewPython; they
predate this repair. Active alscript remains linked; existingalchemy-qaPython
has the modules. No runtime feature loss or FPS cause is inferred.

Factory-frontier correctness passes6702assertions/1case on consistent frozen
libraries. That result includes true source factory/relay behavior and exact
pose/UV/scalar output, but initially excludes added owner bookkeeping. Refined
stamp/positive-filter/native-owner cost cells1/2/4/8/16/64/256 are being qualified;
there is no cost selection or live speedup yet. Keep the original8192 family
dispatch gate and conservative row fallback.

## Provisional combined checkpoint 69258

Builderafc9278d23d7fa5e800e322a7e1fa913e21d1070 and
canonicalcfc20af4ba19c7df9d1cf406cc07a5eba4c7eda6 share production indra
f926c2ed3b1acd3c9b33a3bd900ffa22051cbf90. Prescribed fullbuild03 passes42nodes;
final native978792assertions/44cases and actual current-ABI ownerfrontier
427323/3 pass with API/GPUvalidation1,capture0. MainScope125/3 and dedicated
late-controller16fresh processes/384assertions+11expected named warnings pass.
Strict source3613files/0blocking/0scanerrors and harness33files pass.

Separate candidate26.4.0.69258 matches8273nodes/799193351regularbytes,
346ARM64/0invalid, deep strict signature0 and3owned direct-import audits0.
Exe67b701241d5f78ca41fd69c3938f112ec841a913837cff50c6f1fc0f15a153bb;
active optimized source-embedded metallibfae7a43b5b48b7375f3a47ae52b6c7c7d89dd6b5f6a7456562d9e99438e223d0
is unchanged. Static receipt3c3af1eb and qualification507dde7a under newgroup
artifacts pin source and scope. Runtime not yet started at this entry.

PPD382 remains open: repeated optional cold/churn/denied preparation is guarded,
but fixed checking/observation costs remain measurably adverse in the isolated
changing-source and early-busy regimes. Stable medians improve and larger groups
amortize startup; four-row startup tails are not uniformly better. Final cost04
report04d23a0d retains all5040positions and seven regimes; actual source buffers
are shared across3group keys in this stress model. This is a provisional combined
owner comparison, not blanket performance acceptance or a whole-viewer gain.

Every build/config warning group is retained in final-build-warning-audit-01:
duplicate static archives use identical paths, historical catch2 holder metadata
warning retains full license text (compliance not closed), historical Python
peer/reference-compiler gaps remain, and one saved-assembly test MD5 metadata
warning has unknown historical incidence. No build error or runtime/FPS defect
is established from those warnings; no warning suppression was introduced.

## Home acquisition checkpoint, 2026-11-18

Owner-authorized saved Home Login uses the existing LLPanelLogin button API only
after recorder readiness and explicit request flags. Private helper7c59f03b/spec3add876d
retain27offline tests/3exact staged dryruns. First CUA-control yna82l0q never
leaves Login and is retained under PPD384; its recorder success is not world
coverage. The actual API-login retest succeeds, without credentials/settings
changes. Each completed run closes normally through LLAppViewer.requestQuit;
CLI inventory is empty after final quit03:33:01UTC. No club visit occurred.

| Run | Mode | World UTC | Collector result | Captures |
| --- | --- | --- | --- | --- |
| Alchemy-home-uphzzchc | capture-disabled360s control |03:07:48.979995|complete0/verified_target_timestamp_ranges|none by design|
| Alchemy-home-77ib0k3e | default decline replay360s |03:19:16.430218|recorder0, qualification1|223→224;1588→1589, stopped/error0|
| Alchemy-home-sowsq5_l | explicit late180s replay360s |03:26:27.832836|recorder0, qualification1|228→229;3625→3626, stopped/error0|

Both replay runs retain full CPU exports:360.478/360.416s, maximum CPU timestamp
gaps0.100/0.064s. Their GPU Overview coverage gates FAIL at maximum consecutive
timestamp gaps5.123247958/5.225964750s (>5); positive-frame gaps10.115/9.215s.
Original/export receipts and capture API pauses are retained, not relabeled
as complete normal FPS evidence. Native/CPU/warning analysis and independent
working-copy GPU profiling are pending at this entry. Current drawable3456×1916
differs from69241's3456×2168; no matched old/new performance gain is claimed.

Read-only unfinished-boundaries/flags audit is retained in
checks/unfinished-boundaries-audit-01/report.md (5fa32952). Current selected
Release pipeline/LLRender/world objects are O3/ARM64; MSL3.1/fast-math plus
recorded sources, with no explicit MSL-O claim. Shared shadow publication and
current-main completion feedback are now implemented; broader lazy discovery,
useful small-workload visibility/commands, accepted-light reuse, earlier-stream
feedback and complete physical/pass-resource graph remain partial.

## Initial normal-control reduction (GPU profiles pending)

Capture-disabled uphzzchc reproduces the decline. These are observed eligible
consecutive display periods, FPS=N/summed observed periods, not an average of
instantaneous FPS or a release-gate comparison. Native writer declares234
contention drops (no full/budget/I/O loss); raw data and sequence gaps remain
qualified partial rather than claimed lossless. All committed commands complete
at shutdown, pending0, no missing wait-target timestamps in the analyzed prefix.

| World age | Observed periods/seconds | FPS | Mean period | Main wait union | Period minus observed wait |
| --- | --- | --- | --- | --- | --- |
|0–10s|325/9.952|32.657|30.622ms|9.166ms|21.456ms|
|80–120s|685/39.943|17.150|58.310ms|38.999ms|19.311ms|
|240–300s|974/59.941|16.249|61.541ms|42.802ms|18.739ms|

At240–300s,42.389ms of the42.802ms observed waits overlap their target command's
GPU start/end envelope; before-GPU-start overlap is0. This supports a growing
GPU-completion bottleneck, not the inference that extra CPU threads alone fix
Home. An envelope is not exclusive shader/hardware execution time. Accepted
lighting/probe work versus unrelated queues still requires actual replay.

Both armed runs show the same broad31→16FPS decline. Their remaining period is
~25–26ms versus normal~19ms late. Correction to the initial interpretation:
this difference cannot be attributed solely to capture-layer scheduling. All9×49
typed controls match, but first encoded effective probe ambiance differs
1.50006/1.24202/1.07969 across normal/default/late180; full EEP/camera/workload
equality is not established. Region lighting changes are expected. Capture API
pauses are observed; censoring them does not establish uninstrumented equivalence.
The independent normal control proves the decline exists without the capture
layer. No old/new gain is claimed from a single current binary, differing old
resolution or ordinary SL changes.

Overview cached-export reduction independently corroborates normal decline:
associated cadence32.697→16.245FPS (0–10→200–260s), main sampled running
CPU57.81→25.90%onecore. Decode20.579CPU-s during initial10s falls effectively
to0 late. Late60s disjoint main CPU scopes: sun shadow4.159s, ordinary probes
3.395s, deferred geometry3.202s; inclusive nested capture3.433s and publication
1.299s cannot be added. Main feedback bookkeeping samples0.004CPU-s.
Report9fe63d72/receipt7bf98287 under checks/runtime-overview-analysis-01 retain
all actual XML ranges, source/PID/clock checks and PPD385 adapter/archive errors.

Warning/control audit preliminary: successful runs129/129/126WARNING across
37/37/36sites,0ERROR/FATAL, plus3libpng warnings each. Failed Login11/9sites.
All9×49typed settings snapshots complete/equal. Main terrain CPU-capture
fallbacks42/81/15 are already present in the first world-entry-boundary census
and never increase during the decline; probe counters0. Log timestamps are whole
seconds, so they do not prove every failure strictly precedes the native world
marker; this corrects the initial overly precise timing interpretation. Encoded pass93 is terrain7 plus86render
types, fail lambda14865 is invoked by terrain-prerequisite gate14891. Exact
failing operand is unrecorded. These match native precommit-abandoned counts,
not GPU failures; all committed commands complete. Preserve relation to open
PPD125 without claiming its closure or inferring a new steady-FPS cause.

## Completed four-frame replay and final decision

All four unchanged Medium/Overlapping controls profile and close normally:
67.91→110.26ms default;66.80→131.43ms late180. These are replay observations,
not live frame periods. Exact main configured probes1→22 and1→47 accompany
unchanged10point/47projector counts/scissor areas. Both early hot draws are
generic/all_full; both late actual main draws are prepared with buffers8/9
and16/16eligible consumers. This confirms the existing prepared path, not a
new main-lighting selection change from this shadow/admission group.

Fused main encoder FS invocations stay277.630M while ALU/inv rises112→192/218.
Alpha-containing encoder invocations fall46–48M→19.6M while ALU/inv rises
248→1233 /243→1381; occupancy and buffer-read counters change substantially.
These encoder aggregates contain multiple shaders. Accepted-probe contribution,
traversal/SH/radiance, shadow filtering and data-access costs remain to be
separated under unchanged/restored saved-frame controls. Fixed SSAO/lightmap
work is still costly, but does not explain the measured growing work. No
buffer-write explosion is observed; hot simple/BASE/PBR spill fields remain
blank/unknown after profiling, versus explicit0 on SSAO/lightmap. No shader
experiment, source Reload or quality reduction is promoted.

All67/44/116/107encoder rows and all five categories are retained; three frames
actually include labelled ordinary probe work, default-late does not. Exact
Metal queue IDs/census remain unavailable. Late180 ordinary face3 uses105probes
and declines useful10consumer mask preparation for admission; main is prepared.
Main47at180s is not the final54probe tail, although FPS is already near its
later plateau. Qualify that retained coverage rather than call it final state.

Final warning audit accounts395LLwarnings/39emitters plus12libpng,0ERROR/FATAL,
including failed Login. Every callsite/trigger/functionality/performance limit
is mapped in checks/runtime-analysis-01/warning-sites.md; PPD125hero recovery
remains unproved. Main terrains fallbacks do not grow after entry. Input hashes,
exact-frame addendum0c02cad9, failed-native303b0644, both interpretation errata,
GPU report913f9bf6/receipt1ddb185b and Overview final digest25bc6924 are retained.
All viewer/replay windows close normally, all heavy work stops.

Next group should focus on measured shared per-accepted-receiver probe work and
actual mask efficacy, with controlled saved-frame separation of components;
implement further derived-lighting publication only on demonstrated benefit and
correctness. Broader visibility/commands, early streaming feedback and physical
resource/pass graph remain partial. This coherent group is ready for owner
visual/performance feedback and club checkpoint; no whole-viewer win or push.
Full findings: [69258 Home checkpoint](../../research/shared-shadow-main-admission-checkpoint-69258-20261118.md).
