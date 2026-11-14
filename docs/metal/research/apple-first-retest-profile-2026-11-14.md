# Apple-first retest profile — 2026-11-14

The retest recovered the previous G2–G5 run's average FPS loss, reaching **20.36
FPS versus17.77**, essentially repaired G1's20.36. This does not demonstrate an
architecture win over G1. CPU-to-display association is still about6% longer
than G1, and substantial serial shadow-source capture remains. All three are
native Metal viewers; G1 is not the original OpenGL viewer.

The new diagnostics answer the opaque-path question: **no preparation was
attempted**. Every active main/probe shadow view had no eligible candidate;
there were no typed preparation fallbacks. Neither the2048draw ICB crossover
nor the8192input detached preparation gate was reached. Raising hardware API
coverage or enabling a setting does not make these mechanisms help this workload.

## Verified capture

Owner manually started the staged retest viewer and reported arrival at the
busy club, then explicitly requested profiling. Root verified livePID58603,
exact staged path, version26.4.0.68999, compiled source
`4918fd7ce3976576eababff430910d73017c9bdb` and executable SHA256
`13e44a15bfbe746072b5c5642f123310a8365d01947e621a25775637e19d351f`.

`xcrun xctrace record --template "Game Performance Overview" --attach 58603
--time-limit 5m --no-prompt --output <private>/retest-busy-club-5min.trace`
exited0. Actual TOC measurement **10:19:21.899–10:24:23.667 UTC,
301.767390seconds**, end reason `Time limit reached`. Recorder wall421.533s
includes deferred symbol processing, not additional measurement. Owner was told
closing was safe after the actual stop message; subsequent process absence is
not a crash observation. No agent launch, login, camera/settings/cache change,
restart, extra live capture, source repair or push occurred.

Private evidence root:
`artifacts/metal/retest-busy-club-profile-20261114-101904/` in the canonical repo.
Raw trace, viewer logs and exports remain ignored/local. All420 saved visual
controls match68995 and the within-session post-measurement snapshot. All61
logged published surfaces are3456×2168. Region/camera/population/asset state is
owner-controlled live input, not asserted identical. We quantify that work
rather than dismiss its cost as irrelevant world variation.

## Whole-frame evidence

| Observation | Repaired G1,68991 | Prior G2–G5,68995 | Retest,68999 |
| --- | ---: | ---: | ---: |
| Presented FPS, bucket duration weighted |20.3639|17.7729|20.3569|
| Last60s FPS |20.1134|17.9249|21.0331|
| Associated display interval, ms |49.1355|56.3854|49.1306|
| Command buffers/present |23.8821|32.8420|24.4044|
| CPU begin to display association, ms |91.6623|97.0845|97.1438|
| Associated GPU Active Time metric, ms |85.3942|87.2204|90.1590|
| GPU Begin to End metric, ms |85.6161|92.0893|90.5070|

Retest FPS is14.54% higher and command buffers/present25.69% lower than68995.
Against G1, average FPS is effectively unchanged (−0.034%); command count is
2.19% higher, CPU-to-display association5.98% longer and GPU Active Time metric
5.58% longer. Last60s FPS is4.57% higher than G1. A better tail-window rate does
not establish a general engine win or unchanged latency.

All thermals are nominal. Retest has one target process/layer,6,127 Overview-reported
presentations and99.739% FPS bucket coverage. Individual-frame tables remain
empty, so **no frame p50/p95** is reported. The prior report's associated GPU
span column used GPU Active Time; it is kept distinct from GPU Begin to End
here. These metrics can span overlapping commands/frames and are not additive
GPU utilization, pass timings or a percentage of all hardware used.

## Actual work and CPU owners

Values below are sampled Running CPU seconds divided by each trace's estimated
present count, expressed as approximate CPU ms/present. This uses one consistent
normalization: G1≈6141.20, prior≈5354.95, retest6127. Inclusive parent/child rows
overlap and must not be summed. CPU and log counter windows differ slightly;
these are amortized costs, not correlated per-frame critical-path intervals.

| CPU owner, ms/present | G1 | Prior | Retest |
| --- | ---: | ---: | ---: |
| Main thread |34.002|49.471|34.089|
| Sun shadows |15.517|21.936|18.562|
| Shadow-source capture |2.221|7.367|6.832|
| Source-key construction |not separately classified|1.405|1.205|
| Source-cache operations |not separately classified|1.050|1.048|
| Aggregate source leases |not separately classified|1.360|1.167|
| Main texture updates |0.993|4.400|0.983|
| Decoder workers combined |1.645|117.387|3.083|
| Native renderer/encoder union |12.359|17.150|11.226|

Main CPU is back to G1's aggregate level while shadow capture still costs
approximately4.61ms/present more. Retest source-key/cache/lease ancestry costs
7.380/6.424/7.152CPU-s respectively; these overlap with capture, not additive
new frame costs. Lease pointer-set insertion has4.251CPU-s of exclusive leaf
samples. The source owner remains a concrete optimization target.

Existing resource counters quantify admitted work:

| Periodic work/present | G1 | Prior | Retest |
| --- | ---: | ---: | ---: |
| Main shadow source inputs |6547.72|8482.10|7747.78|
| Probe shadow source inputs |603.35|755.78|848.81|
| Native textures created, all producers |6.97|16.22|7.75|
| Native buffers created, all producers |217.99|256.26|93.54|

Retest main inputs are18.33% above G1, probe inputs40.68% above; those differences
matter. The approximately3.08× capture cost/present versus1.18× input count is
not explained by input count alone, but different source/material/rigged mixes
remain unisolated. Lower buffer creation is observed resource-churn relief; allocation reuse
versus changed dirty/streaming work is not isolated. New generation recycling
counters are positive, but the architecture has not won on frame cadence.

Decoder Running CPU totals are18.892s versus628.601s prior and10.103s G1.
Periodic decoder counters observe869completions across297s, versus56,104 in the
prior profile's sampled interval; sampled queue waiting is0, active peaks3.
Retest texture-update cost0.983ms/present is near G1. Relief is sustained across
30-second CPU bins; the prior streaming storm is absent at that scale. This is
engine work to manage, not background noise to subtract from FPS.

## What actually activated

Opaque counter deltas cover10:19:21–10:24:22UTC,301s; baseline is0.899s before
exact start, endpoint1.666390s before exact end. There are51 in-window snapshots
per series plus the baseline. Root independently checks matching deltas.

- Main36,720 and probe8,488 requested/active/completed views are all no_candidate.
  Here completed means CPU renderSpotShadow returned true, not GPU completion.
- Supported=1; gate deltas, selected packets/draws, attempts, preparation,
  execution, typed fallbacks, oversize and budget rejections all0.
- No static/rigged run reached2048draws. Main static has128,782/49,117/24,385/0
  runs in <64/64–255/256–1023/1024–2047 bins; main rigged171,344/31,513/0/0.
  Probe static97,526/2,132/2,067/1,167; probe rigged0.
- Main retained source hits45,744,475/47,145,760 visits (97.03%), builds1,401,285.
  Probe hits3,030,244/5,153,475 (58.80%), builds2,123,231. These are repeated
  per-view events, not unique objects. No source failed views/cache declines.
- Detached preparation eligible/calls/completed deltas all0. Lifetime source
  family maxima1320main/971probe are below8192; those maxima are process-life
  gauges, not measurements of the largest draw run in this exact interval.

PPD-112's earlier-direct16k budget edge did not occur here. The unused opaque
path is a workload/policy mismatch in this run, not hidden failed preparation.
Prior68995 had no such census; this does not retroactively identify its exact
selection/failure cause.
Lowering thresholds blindly would discard the complete-path cost evidence that
selected them. Future packet experiments must use the observed small-run mix,
query/material/cull boundaries and complete CPU/GPU cost, preserving caster
membership and direct fallback.

## Memory, streaming and resource generations

Startup verifies automatic1/physical65536MiB/process ceiling49152MiB, a
policy governor limit rather than an OS allocation reservation. In30
complete-second pressure samples spanning291s, bias is exactly1 and OS/process/
native pressure flags are non-low. OS availability17,163.8–18,818.5MiB,
process allocation11,542.9–11,746.4MiB; no emergency or finer-than-desired warning
events. Critically, **the old16GiB cap would also have4,637.6–4,841.1MiB headroom
here**, above512MiB. This run verifies the new policy is active, but does not
exercise the old cap crossing or isolate its repair as the cause of recovery.

Create-list deltas over291s:956dequeued/attempted/true/finalized,755,184,720raw
input bytes; redundant/finer/retry/scale-down deltas0. Generation deltas over294s:
COW attempts/replacements/encoded mips/bytes/failures0; worker uploads/commits/
failures0;1,097immediate upload commits,0failures. Lower-level scale-down calls13,
committed0: ordinary decline reasons are uninstrumented, not13proved failures.
No old COW census exists, so zero now cannot establish a historical COW cause.

Vertex full/partial publications628,819/341,795, logical uploaded bytes
28,264,235,608 (91.683MiB/s), recycled generations123,880, leased orphans53,736.
These describe traffic/generation turnover, not driver allocation, completed GPU
copy bandwidth or a leak. Source-cache estimate1,716–2,029MiB across3,109–4,373
entries is not a physical-memory scope to add blindly to native/process totals.

Object residency factor1, effective bounds16384/39321.6MiB, draw112m,
near/rear/evict32/84/105m and64invisible frames throughout. The cap remains
coupled to that established object-retention policy; changed retention is part
of engine workload, not a free performance gain.

## Errors, evidence limits and continuation

Capture logs have **0ERROR/0FATAL**,11warnings:10numeric libcurl Easy_28 timeout
warnings after request policy handling, and one unconditional avatar-complexity
notification log. No native preparation or renderer warning is recorded in the
window. Network failures can affect streamed workload; no renderer causality is
inferred from them.

PPD-117 records a newly exposed inherited statistic defect: raw_images becomes
negative in26/30 publication samples, minimum−21. The const-pixel-data LLImageRaw
constructor omits the live-count increment while destruction decrements; root
confirms the same omission in original1eb44bbd. This statistic feeds UI/stats,
not pressure decisions; it is not negative memory or evidence of GPU corruption.
No code repair was mixed into this capture.

Recorder emits336DWARF warnings. CPU parser resolves305,247 target rows with no
other-PID rows; current main address-only leaf weight0.589%, deduplicated-symbol
leaves2.832%. Named owner attribution is strong, exact leaf coverage incomplete.
Source paths, raw stacks, session/account/location data remain private. Historical
G1 normalization is corrected consistently here; first comparison-script schema
mismatch and whole-second boundary check correction are retained in private
analysis notes, not silently mistaken for viewer failures.

Next concrete architecture work is producer-side publication for immutable
shadow sources in pipeline.cpp, beginning at captureMetalShadowScene,
shadowSourceKey and aggregate lease/cache bookkeeping. Stop paying full key and
ownership insertion cost per shadow view for already current geometry/material
records; preserve exact dirty invalidation, current pose, textures, type and
coverage, plus borrowed-command and actual GPU completion lifetimes. Probe's
lower hit rate needs miss/key-change attribution before choosing retention.
Then integrate dependency-safe preparation and whole-frame main/probe/upload
admission using the measured real-run distributions. The full proposed
architecture remains partial; 2048/8192 pilots do not establish its implementation
or superiority. Evaluate complete-frame cadence, latency, response and quality
at the integrated boundary. PPD-110 remains investigating; PPD-111/113/114 retain
owner acceptance requirements. No release or performance gate is closed here.

Reproducible private receipts: recording.json, capture-verification.json,
measurement-window.json, saved-graphics-*.json, cpu-export/analysis receipts,
gpu-export-receipts.json, gpu-analysis.json, whole-viewer-comparison.json,
legacy-resource-comparison.json, safe-log-window-summary.json,
safe-warning-*.json, integration-numeric-checks.json, cpu-review/comparison.json,
opaque-analysis/final and memory-analysis summaries. Export and analysis reuse
saved data only; no new runtime measurement after this five-minute run.
