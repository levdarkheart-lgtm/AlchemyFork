# Adaptive decoder profiles — loading pressure and first-world readiness

2026-11-12, Apple M1 Max /64 GiB /macOS26.5.2 /Xcode26.6.
Executable SHA256 `243b5787b6b3d9789953a34b58e6b58615c859861c41c29dfe19c1400b5cd3d4`.

Build 68880, source `98f41e652beb6150d9012cd25af102bd6c5e76df`, ran both recordings in PID 36734 on the M1 Max. The isolated cache was retained: 30,813 files and 4,522,534,437 logical bytes before launch. These are different destinations and workloads; neither is an empty-cache trial. The first destination’s unvisited history and the second’s prior visits are owner-reported. This evidence shows adaptive behavior and loading costs; it does not establish a speedup from four to six workers. `cache-before-launch.json`

The first 180.840 s trace includes login: initial world movement completes at +18.965 s and `STATE_STARTED` at +25.965 s. It contains no separate nonlocal teleport. The second 181.058 s trace starts after teleport onset: start −2.664 s, movement/arrival −0.664 s, finished callback +1.336 s. Those original offsets are retained with one-second log uncertainty; full pre-teleport coverage is unavailable. `new-destination/runtime-analysis.json`, `visited-heavy/runtime-analysis.json`

| Measurement | Owner-described new destination | Revisited heavy destination |
|---|---:|---:|
| Decoder sampled CPU, whole trace | 95.182 CPU-s | 214.378 CPU-s |
| Peak five-second decoder core-equivalent | 3.607 | 5.901 |
| Decoder sampled CPU, last 30 s | 17.428 CPU-s | 0.235 CPU-s |
| Mean admission queue wait* | 126.051 ms | 2483.815 ms |
| Mean `processRequest` wall duration* | 9.555 ms | 17.931 ms |
| Presented cadence, last 30 s | 8.226 FPS | 20.967 FPS |
| Associated GPU Active Time span, last 30 s | 188.272 ms | 51.695 ms |

*Admission means use the actual inner counter intervals, +4.965..178.965 s and +0.336..180.336 s, rather than exact full-trace boundaries. They cover 10,046 and 12,009 job completions, without establishing successful visible textures. Queue wait combines WorkQueue residence and admission-gate delay; it excludes preceding network/cache fetch. Process wall duration excludes the subsequent completion callback and GPU texture publication. Cumulative floating sums are rounded; small late differences can round to zero.

First whole-trace cadence includes startup/login UI. Even the13.826FPS after
STATE_STARTED includes the native world warm-up interval described below, so it
is not a settled-world metric. The heavy whole-trace cadence is21.447FPS with
its onset clipped. Only the explicitly labeled late30s cadence is tabulated;
these different scenes and arrival histories are not matched benchmarks.

All six pool threads appeared in CPU samples in both recordings. MQ arithmetic and entropy loops account for approximately 85.8% and 85.7% of decoder CPU. Decoder named-leaf coverage exceeds 99.98%; main-thread coverage exceeds 99.50%, with some unresolved ancestry. CPU Running weights are CPU activity, not blocked time, per-texture latency or time until visible textures become sharp. The heavy run’s peak bin averages 5.901 sampled CPU-seconds per wall second, while its last 30 s has very little decoding. The first run still decodes late; “scene present” does not establish completed texture resolution.

Admission rose 4→5→6 under backlog and later reduced. A [follow-up queue audit](decoder-queue-wait-forensics-2026-11-12.md) reconstructs the 2.484s mean: 97.555% of recorded wait belongs to admissions in the first logged27s, while six busy workers trail the arrival burst. Largest sampled pending counts were 970 and 1860; largest sampled oldest-request ages were 0.841 s and 7.611 s. Recorded admitted-wait maxima reached 1.451 s and 8.207 s. Counter conservation holds, and no cancellation was logged. These measurements justify instrumenting queue priority, visibility and requested resolution to identify delayed useful work; they do not identify cache misses, downloaded bytes or wasted requests.

The heavy run retained limit 6 until +129.336 s, then limit 5 until +154.336 s. Between +99.336 and +129.336 s, 80 jobs completed with 879 ms aggregate process wall time; all seven logged active/pending gauges were zero. Their latest control-interval occupancy ranged 0–4.423%, not a whole-window average. Source policy requires five continuously empty seconds, so intermittent small jobs can delay reduction. An elevated ceiling alone does not show wasted CPU. Native Fair thermal state begins near +159.336 s, agreeing with Instruments within log resolution; the controller was already at baseline 4. `controller-trickle-analysis.json`

Texture publication offers a separate measured CPU target: main-thread texture updates cost 4.593/15.514 CPU-s; their copy leaves account for 2.128/6.808 CPU-s. `createMetalTexture` subtrees cost 4.217/12.608 CPU-s and overlap those figures. Audit whether upload preparation can share buffers or avoid a copy, and measure its frame-budget behavior. Sun shadow preparation also costs 14.681/22.756 CPU-s, but CPU attribution cannot assign the long associated GPU spans to shadows or any particular pass. GPU Active Time here includes associated in-flight/queue effects; it is not execution-unit utilization or execution-only shader time. Per-pass GPU evidence is needed to separate rendering costs.

## Black world during login: PPD-008

The source/log investigation supplies a grounded mechanism consistent with the
reported5–10s black world while UI stays visible. Original/current display
traversal begins at STATE_PRECACHE, but native capture/renderer warm-up is
withheld until STATE_STARTED. In this capture, asynchronous main-world renderer
warm-up begins at+25.965s and completes at+32.965s, with measured worker wall
**6.60176s**. UI submissions continue in between. With no real world prefix,
the native UI path uses a black cleared background; the normal startup curtain
fades over1s without a native-world-readiness predicate.

This is a source-confirmed readiness difference with strong timing correlation;
the actual curtain branch and first displayed world pixels were not recorded.
The early zero-Overview buckets0–9.529s precede login processing and do not prove
the owner's black interval. The safe correction to investigate is earlier
asynchronous renderer/PSO prewarm while preserving the existing STARTED scene
admission and generation checks. Simply removing that guard would bypass
startup water/alpha/environment ordering. No rendering fix was applied.
See [PPD-008 investigation](../work/active/login-black-world-investigation.md).

## Highest-value next boundaries

1. First-world readiness: remove late preparation from the visible login
   handoff through safe early asynchronous prewarm, then verify curtain/world
   publication timing and actual pixels.
2. Loading: measure queued work's current visibility, priority and obsolescence.
   Heavy admission wait dominates mean per-job process wall; no counter yet
   identifies wasted work or unique textures. More workers require a matched
   throughput/responsiveness test, not a core-count assumption.
3. Upload: heavy pixel preparation consumes9.039 main CPU-s inclusive; direct
   copy leaves use4.900s there and1.826s in native upload. Investigate avoiding
   redundant preparation/staging copies without changing texture formats.
4. Rendering: heavy main CPU includes22.756s shadows,21.248s deferred geometry
   and15.514s texture updates. By its last30s, decoder CPU is only0.235s while
   presentation remains20.967FPS. Obtain encoder-level GPU attribution before
   selecting a GPU-pass optimization; associated spans alone cannot assign it.

Private evidence root:
`artifacts/metal/adaptive-decoders-20261112/runtime-180348/`.
Both normal record/save/TOC exports exit0. Both CPU exports/base/custom/findings/
correlation/plot stages and12metrics exports/four analyzers exit0. Raw CPU XML
hashes remain unchanged; normal named coverage is usable with no recovery.
Exact argv/exits/hashes are in phase receipts, `analysis-prep/execution-summary.json`,
`analysis-prep/metrics/FULLANALYSIS.json`, and runtime admission execution proof.
Source-only black investigation uses eight recorded successful Git source reads.
Both CPU PNGs were inspected; heavy bars were independently inspected by root.
The viewer retained and populated the same cache. Heavy analysis began only after both recordings saved; no agent changed viewer settings during either capture.
All implementation and evidence remain local; no push or runtime-fix acceptance.
