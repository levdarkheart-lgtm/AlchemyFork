# Integrated69122: quiet house and busy-club transition

Testing Integrated Build session,2026-11-15. Owner launched and closed the viewer;
Codex attached two requested five-minute Game Performance Overview recordings
and watched the logs. Both recording/export/analysis chains exited0. Codex changed
no source, graphics setting, cache or app and did not launch the viewer.

**The integrated viewer ran through both captures and the teleport. These runs
provide engine evidence, not a before/after architecture performance verdict.**
They are different workloads on one candidate, with no corresponding old-build
quiet/club pair. Feature/stability/performance acceptance remains partial.

## Identity and measurement boundaries

Source `beb6837b71f0429f116fef8e2ad7cc5d166bc010`,26.4.0.69122,PID66666.
Exact staged executable SHA256
`101a327c68852a44e2ba435cd0744a9beaf4582e511fe16b239f171069c57428`.
M1 Max64GiB; both trace thermal states Nominal. Post-capture doctor and
verify_app exit0;346 Mach-O slices ARM64,0 invalid. Current runtime flags in
startup logs match the selected launcher: GroupSources+GroupSourceStability ON;
optional workers/rows/tables/banks/CPU-native visibility/animated modes OFF.
Other integrated native publication, admission, terrain/probe changes are active.

| Actual trace window UTC | Duration | Context |
|---|---:|---|
|21:33:05.395–21:38:06.456 |301.061391s |Owner-described quiet place in front of house |
|21:40:21.405–21:45:22.130 |300.725394s |House→club; TP requested21:40:42, arrival44, finished47 |

The second trace began20.6s before teleport. Its full window includes loading;
the final60s is a later club sample, not a declaration that all loading settled.
Quiet offline analysis finished21:39:56.913, before the second recorder began
21:40:19; no overlap. Setup/save log spans365.965s/368.435s are separate from
actual trace windows. Initial CPU-only150.24s capture was deliberately terminated
and superseded; it is retained separately and excluded from these results.

## Whole-engine observations

| Measurement | Quiet house | Teleport/club |
|---|---:|---:|
|Whole-window presented cadence, weighted Overview mean |13.30FPS |18.66FPS |
|Last60 presented cadence |12.44FPS |19.50FPS |
|Last60 mean frame interval |73.28ms |51.31ms |
|Main-thread running sampled CPU |85.519CPU-s |239.714CPU-s |
|All target threads, running sampled CPU |130.307CPU-s |608.739CPU-s |
|Ordinary probes, disjoint main-thread scope |27.078CPU-s(31.7%) |14.951CPU-s(6.2%) |
|Reported last60 GPU-use ratio |0.9201 |1.0000 |
|Sampled peak process footprint, maximum |11593MiB |31805MiB |

These are aggregate means and sampled CPU totals; CPU-s is not frame wall time.
GPU-use ratio is the tool's active-time/frame-interval ratio, not shader ALU
occupancy, bandwidth saturation or a sum of pass costs. Overlapping GPU duration
spans can exceed frame interval and must not be added to CPU or other spans.

The quiet house was not a cheap scene: ordinary probe work was almost a third
of sampled main CPU, with reported GPU-use high. The club increased main CPU to
about79.7% of one core over its trace, compared with28.4% at the house. Club
inclusive samples include generateSunShadow95.165CPU-s, shadow capture33.456,
native shadow rendering25.874, deferred lighting39.302 and world sources20.172.
These scopes overlap and cannot be summed. The club being faster in presented
cadence does not establish an architecture gain; scene composition matters beyond
avatar count. GPU pass attribution is unavailable in these Overview traces.

### Useful parallel renderer preparation was not exercised

For both main and probe shadow census in both windows, cumulative
`detached_eligible/calls/completed` and `compact_completed(dispatch/serial)`
remain0 throughout. Session maximum family inputs remain666(main)/1158(probe),
below the existing8192-input detached threshold. Main G-buffer preparation is
explicitlyOFF and reported preparation_requested0. Background asset work is
visible in all-thread CPU samples, but these runs do not demonstrate detached
render-preparation benefit. This is a concrete original-G3 acceptance gap:
implemented callsites/contained large-family wins are not proof of useful
parallel rendering in the profiled SL workload. Do not lower thresholds blindly;
prior small-family complete-cost losses remain relevant. PPD241 retains runtime
census context; shadow-preparation-runtime-census.json preserves exact deltas.
Main failed_views remained4→4 and probe0→0: no new sampled shadow capture failure
in either measured window.

### Retention and admission are active, but their cost remains material

Both scenes repeatedly reach the3,892,314,112-byte native source-cache ledger
cap. This is separate from process memory, GPU residency and physical allocation.
Inside actual trace intervals, sampled first→last native counter deltas show:

| Counter | Quiet sample21:33:08→21:37:59 |Club sample21:40:23→21:45:19 |
|---|---:|---:|
|Native retain calls |4020636 |6825396 |
|Byte-cap rejections |2559646 |4331675 |
|Capacity evictions |1090835 |1535092 |

These are occurrences, not unique objects. Club group-publication native_retain
failures rose4003; other reported group-failure stages stayed0. Quiet retained
ledger estimate ranged3.865–3.892GB; club ranged0.205–3.892GB across teleport.
CPU samples include working-set retain3.452s/4.900s inclusive(quiet/club).
This establishes substantial turnover and measurable ownership work; it does not
prove that cache churn dominates total frame time or justify simply raising caps.
PPD240/274 cover the publication/cap attribution path; no policy fix was made.

Club decode waiting peaked4149, oldest wait47.852s, then ended0waiting/0active.
Texture create_pending peaked4346 and ended0. However sampled streaming still
had33 queued, one786432-byte upload in flight and71 raw images late in the run.
The shared admission/decoder ceilings were active; queue drain alone does not
prove optimal loading or final steady state. Texture discard bias remained1,
with no sampled OS/process-low-memory flags. The process-footprint peak increase
is observed, not proof of a leak or extra physical cost of immutable records.

## Warnings and errors

Counts use actual trace intervals and strictly interior one-second log bins;
no warnings occurred in ambiguous boundary seconds.

- Quiet:24WARNING rows,0ERROR/FATAL.19HTTP timeouts,1DNS failure,1translation
  parse failure,2notification XML warnings and1notification-removal warning.
- Club:129WARNING rows,0ERROR/FATAL. This includes17textureHTTP403 chains with
  four log rows per chain,9material failures,14HTTP timeouts,7DNS and
  7translation parse failures, avatar/attachment/loading notices, and one
  OpenHTJ2K decode callback failure(SOT marker expected;3FEE found).
- Club region cleanup warned twice,1045 and1529 map objects. Current and
  original `LLViewerObjectList::clearAllMapObjectsInRegion` function bodies are
  byte-identical: after the warning, each remaining region object is markDead'd.
  The counts must not be summed as unique leaked objects. The triggering state
  remains unproved; PPD294 records the observation, not a Metal regression claim.
- No Metal command-error category appeared. This does not mean zero failures:
  asset/decode warnings and optional-publication failures above still occurred.
  Failed assets can change what ultimately renders; successful HTTP fetch is not
  equivalent to all material/texture feature parity.

Original reference SHA `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` also contains
notification XML `embedded_items`; this alone does not prove parser warning parity.
Codec warning is retained under PPD005 without claiming an OpenHTJ2K regression.

## Consequences for Architecture Session

Use the whole frame as the unit: source publication/ownership/admission, shadow
and geometry preparation, lighting/probes, GPU execution and presentation all
have observed costs. Do not enable the rejected optional GPU paths to claim
completion. Prioritize the existing retention/admission turnover evidence and
its producer/current-reader/completion contracts; reconcile inactive detached
preparation with actual666/1158-family session maxima before claiming G3 benefit; assess the loading backlog
alongside render latency. Preserve quality and all required raw rendering.
A source/cost repair can be pursued from this evidence; another live capture is
not authorized by this handoff. A GPU-specific optimization claim still needs
GPU-pass evidence that these traces do not provide.

No true individual-frame p50/p95 or detailed encoder/pass timings are present;
do not fabricate them or run the product performance gate on aggregate buckets.
Quiet recorder emitted339 DWARF cursor warnings; busy0. Symbol completeness and
inlining limit exact function attribution. Foreground state and exact pixel/
feature parity were not visually verified. Seven/twenty zero-FPS buckets exist
in quiet/club Overview output; do not describe cadence as uniformly stable.
Neither improved whole-viewer performance nor all feature/release gates are proved.

## Evidence

Private local artifact root:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/`.

- `architecture-69122-quiet-house-profile-20261115T213258Z/`
- `architecture-69122-busy-club-transition-profile-20261115T214019Z/`

Each contains the original.trace, capture-receipt.json, capture-verification.json,
CPU/GPU exports and analyses, offline-analysis-receipt.json, log-review.json and
runtime flags. Busy also contains descriptive-quiet-busy-comparison.json,
source-warning-review.json, shadow-preparation-runtime-census.json, phase markers, doctor/verify logs and the analysis
interference check. Evidence-files.json hashes top-level files; it is explicitly
not a full trace-package inventory. Raw logs retain private identities/locations
and stay ignored. Owner confirmed viewer closed after both recordings.
