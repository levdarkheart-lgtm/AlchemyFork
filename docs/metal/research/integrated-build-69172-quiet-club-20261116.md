# Integrated69172 owner session — assessment in progress

The owner completed both five-minute scene intervals and normal logout/close.
Whole-viewer performance acceptance remains **unproved**. CPU/GPU trace
finalization/recovery is still pending; the independently saved logs and rolling
viewer statistics do not replace that evidence.

## Standing workflow

Follow the [integrated checkpoint](../OPERATING_MODEL.md): deliver one pinned
candidate, measure the owner-driven quiet/club/transition/shutdown session,
verify actual retained coverage, assess the coupled frame pipeline and every
warning category, then choose repairs or further architecture work. Suspend
builds and tests during measurement. Component cost changes never establish
whole-viewer improvement. Preserve quality, functionality and loading behavior.

## Identity and actual intervals

- Source: `350b803597e48a8c0e6b26b1050a47a5ae41e2ee`, build26.4.0.69172,
  exact staged `architecture-combined-69172/Alchemy Metal Alpha G2G5.app`.
- Executable SHA256:
  `b8e5596c3abcc69403a36d9bdb7935a991877edbc1f792b4513f2f161b41e9e4`;
  viewer PID65178, launched10:20:52UTC.
- Quiet:10:24:01.548650–10:29:02.374326UTC,300.817seconds.
- Busy:10:32:53.334627–10:37:53.463707UTC,300.125seconds.
- Owner-controlled logout/close observed10:38:54UTC; full startup, transition
  and shutdown logs retained. No competing build/native test during measurement.
- Private local evidence:
  `artifacts/metal/architecture-69172-integrated-session-20261116T101436Z`.
  `phase-status.json`, `authoritative-phase-timeline.jsonl` and
  `operator-phase-receipt.json` establish phase boundaries. Original controller
  phase labels are invalid metadata under PPD309 and remain preserved.

## Active, disabled and demonstrated

All37 exposed graphics/architecture controls remained stable. Main lighting
render-pass fusion, native group sources/stability and large-shadow overlap were
enabled. Main light-values worker, newer command banks/visibility and animated
GPU paths remained disabled. ShadowDetail2, FarClip112, deferred1, SSR/SSAO,
FSAASamples3 and VSync1 were observed; AutoTuneFPS0. TargetFPS15 alone does not
prove an active15FPS cap. No policy/quality/cache change was made mid-session.

The last logged main lighting census is15120requested/prepared/encoded, with
zero exclusions, structural declines, prefix/serial failures, missing inputs or
decline-reason buckets. The feature really ran; neither this activation nor
contained native tests prove a whole-viewer gain. Shadow worker34341jobs were
encoded/submitted, with zero canceled/failed or budget/adoption denials; its
32.048s cumulative owner-join wall sum requires normal CPU/GPU overlap analysis.

## Saved scene statistics and limits

| Observation | Quiet | Busy |
|---|---:|---:|
| LLStats replies |134|140|
| Selected disjoint histories |16|23|
| Selected period observations |3184|4577|
| Sum of implied positive-FPS period durations |232.012s|250.322s|
| Selected-history cadence estimate |13.72FPS|18.28FPS|
| Period-mean frame time p50/p95/p99 |72.13/87.82/171.17ms|53.16/64.53/173.38ms|
| Tracked texture allocation median |6739.73MiB|10596.68MiB|
| Sampled process RSS median/max |3.94/4.64GiB|15.95/16.20GiB|

These are **incomplete retrospective held-period observations**, not complete
phase raw-frame quantiles or scanout. Arrays have no frame IDs/timestamps and
overlap across polls; the analysis selects conservatively separated histories
instead of pooling every response. Apparent tails near150–228ms need actual
CPU/GPU/presentation timestamps before periodicity or a cause can be claimed.
The different scenes are different workloads; quiet-versus-club is not an
old/new comparison. No baseline/OFF run was recorded in this session.

Texture accounting divides the legacy doubled LLStats field by2; it is not
physical residency/currentAllocatedSize and cannot be added to RSS as total UMA
use. Inventory fetching completed, but that API does not expose texture loading
completeness. AC power and no recorded pmset warnings do not exclude throttling.
Observer errors0; query round trips/gaps include frame servicing and periodic
serial controls. Observer/Instruments perturbation was not calibrated.

## Complete warning review

All365 structured WARNING lines at46callsites plus3 libpng stderr warnings were
reviewed. Structured ERROR/FATAL counts are0; absence of those labels does not
prove correctness. Private `analysis-warnings-review.md`, summary,
classified-lines and census-series JSON retain every category/source/UTC and
original-source mapping. Startup/quiet/transition/busy/post-busy counts are
100/19/137/65/44. Quiet19 are HTTP timeouts; busy65 are55throttled skin-fallback
notices and10timeouts.

- PPD311:70throttled palette notices represent at least6700calls, not unique
  avatars. Of16bounded diagnoses,10have nonfinite inverse-bind XYZ and6finite
  operands but nonfinite product XYZ; all live world matrices are finite. The
  earlier native stabilization substitutes avatar-root bind-space matrices.
  Intended rigging quality and malformed-input origin remain unproved.
- PPD241:10incomplete main-shadow captures,0probe, all pool-stage at the separate
  shadow-terrain helper's compound guard. Increments3startup/7post-busy, none
  within either measured window. The specific operand remains unknown. PPD125's
  classic-terrain nullVB skip was not demonstrably exercised; do not close it.
- PPD274/295/296:16,730,384native-retain calls include8,625,499byte-pressure
  rejections; all other rejection reasons0. Capacity/age evictions are
  5,673,095/892,699. Estimated cap3,892,314,112B, retained3,888,122,790B;
  17,629optional group publication failures are all native_retention, preserving
  required raw/direct rendering. This is real cache work to correlate with CPU,
  not physical memory exhaustion or proof a larger cap improves FPS.
- Loading:22texture403episodes/21unique IDs progress through abort/missing state;
  25glTF failure callbacks concern14IDs.51HTTP policy timeouts are real. Their
  server/content/capability/client causes are unresolved; scene equivalence and
  appearance cannot be assumed. Do not add follow-on warning stages as new assets.
- PPD312:2gray-ICC/RGB-PNG and1interlace stderr warnings have no phase/asset IDs.
  Original/current PNG wrapper lack an explicit interlace setup before the
  read-image path. Emitter and visual impact require contained reproduction.
- Other retained categories include startup mirror readiness, cache-body purge,
  appearance/capability/attachment lifecycle, float alpha analysis, optional
  settings/list files and shutdown queue cleanup. They were individually traced,
  not blanket-dismissed as inherited/network noise. Shutdown reaches Goodbye;
  queued172requests and316delete-list workers are explicitly drained, not proven
  leaks. Old-region860/1475cleanup notices remain PPD294.

## CPU/GPU collection issue and next decision

PPD310:continuous detailed Metal system tracing produced269.3GB raw ktrace.
After the viewer exited the recorder waited in system tracing teardown;
DTServiceHub sampled in authorized_ktrace_stop/XPC reply, while protected
dtsecurity remained active. The raw file was APFS-cloned without restarting or
repeatedly interrupting the recorder. An offline import was started separately,
then suspended and operator-canceled(exit1) once the original recorder resumed
processing, to avoid duplicate analysis CPU/memory growth. Both raw input and
receipts remain preserved. The original attachment has1261024additional bytes
compared with the early snapshot; their nature and snapshot completeness are
unverified. No second viewer session is needed for this recovery attempt.
Actual sample/interval coverage, symbols and presentation coverage remain
unverified. A short successful CPU-only retention preflight did not qualify the
configuration's cost/finalization for this workload. This is a collector problem;
no viewer crash is inferred.

The next decision must join actual CPU/GPU timelines to cache churn, owner waits,
scene/probe/lighting work, streaming and pacing tails. Add precise shadow helper
first-reason diagnostics without weakening guards. Investigate skin/PNG input
correctness separately. Do not raise caps, lower quality, repeat unsuccessful
worker/ICB variants or declare fusion faster from activation counters. Future
continuous capture uses lighter CPU/GPU instrumentation and bounded detailed
diagnostics, with representative collection-cost/finalization checks.

## Bounded skin-source follow-up

The network-skin loader, constructors and hash function are byte-identical to
original1eb44. Matrix decoding coerces/narrows LLSD values without matrix-shape,
finite or F32-range checks. No affected asset bytes are available, so this is a
producer validation gap rather than an identified live asset cause. Every local
inverse matrix lane is assigned; inspected output owners do not alias inputs.
PPD313 retains separate inherited missing-bind-shape initialization and empty
inverse/output-vector indexing hazards. They require actual CPU contract tests
before repair and are not explanations for the nonempty invalid inverse values
seen in PPD311. The audit makes no rendering or FPS claim. Private
analysis-skin-source-review/functions/excerpts/receipt retain source evidence.

## Future collector repair

The archived future collector now recognizes the actual xctrace control prompt
and avoids a second stop request during automatic exit/stopping/saving. Eight
pure lifecycle cases plus syntax pass; the active collector and original phase
metadata remain unchanged. A qualification guard prevents that future script
from launching until low-overhead instrumentation has been explicitly qualified.
This is not a replacement recording or verified continuous-capture performance.
Private future-collector-logic-check.json preserves exact source hashes/results.

## Frame policy and timing-collector follow-up

TargetFPS15 is the autotuning target, not an active frame cap with AutoTuneFPS0.
MaxFPS is obsolete; VSync maps to CAMetalLayer displaySyncEnabled. Background
policy can sleep40ms and pause texture-cache work when unfocused/invisible,
but its live setting/focus was not sampled. Existing foreground counters advance
about300seconds per300seconds at10:23/10:28/10:33/10:38UTC, supporting predominantly
focused operation rather than proving every frame was foreground.

The existing strict benchmark CSV intentionally terminates on teleport and
cannot rearm. Its defaults are120frames/30seconds warmup followed by300samples,
not a five-minute duration. It emits intervals/frame indices without UTC frame
anchors. No environment-only change makes it a continuous multi-scene logger.
A future narrow opt-in wall-frame mode at LLAppViewer::frame can provide actual
steady/UTC timestamps and transition/focus status while preserving strict
benchmark semantics. This source audit does not reconstruct missing frame data.
Private analysis-frame-csv-audit.md/.json retain exact source/policy evidence.

## Analysis-method qualification

Future collector lifecycle logic passes8actual-CLI/negative cases and syntax.
Repository tools pass116tests in15.322s in an isolated environment installed
from numpy2.4.3/PyYAML6.0.3 pins. Initial system-Python run failed only the missing
PyYAML import; that failure and corrected environment/commands are retained.
No application code/build or replacement viewer recording was made.

The prepared detailed GPU parser was checked against saved short69151 XML:
PID/ref resolution, coverage and329/364command-buffer joins match prior results.
This validates parsing only, not full-run performance or current69172coverage.
A per-file cache release and12-table first pass plus sizing/RSS guards bound the
next analysis attempt; optional tables remain deferred. Large-file behavior is
not claimed executed. Private analysis-gpu-qualification.json/.md and original
prepared script retain the changes and measurements.

The original detailed recorder is now processing symbols and emits DWARF cursor
warnings. Count and actual exported symbol coverage still need checking. A later
269311200736-byte raw attachment snapshot was APFS-cloned; a separate installed
Game Performance Overview(windowLimit0) import started11:22:50UTC to avoid full
Metal API analysis if it can provide CPU/GPU overview sooner. Neither path has
verified CPU/GPU phase coverage yet. Original recorder and both raw snapshots
remain preserved; no repeat owner run was requested.
