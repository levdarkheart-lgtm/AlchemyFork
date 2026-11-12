# Why heavy-place decode admission averaged 2.48 seconds

The main measured cause is a burst of ready-to-decode work arriving faster
than six busy workers can process it. It is not a2.48s codec operation or a
network/cache fetch duration. Long early waits and much shorter later waits
combine into the reported job-weighted average.

This follow-up uses the existing68880 heavy-return capture only; no viewer
operation, source change or new capture. Candidate source
98f41e652beb6150d9012cd25af102bd6c5e76df, PID36734, binary SHA256
243b5787b6b3d9789953a34b58e6b58615c859861c41c29dfe19c1400b5cd3d4.
Duration181.057662s; teleport onset precedes capture by2.664s.
The [two-profile analysis](adaptive-decoder-profiles-2026-11-12.md) retains
the broader CPU/GPU, startup and cache context.

## Reconstructing the mean

The inner observed counter interval is+0.336→180.336s:

- 12005 jobs start, accumulating29818200ms submit-to-admission wait.
- 29818200/12005 = **2483.815077ms**.
- 12009 jobs finish because four are already active at the first endpoint.
- 12005 starts =11822 new submissions +183 initially waiting requests.

Counters are monotonic and conserve pending/active work across all129 log
rows. Endpoint rounding contributes at most approximately0.00833ms to the
mean. The outer-bracketing cohort gives2.440851s, supporting the same result.

| Cohort by admission time | Starts | Mean wait | Share of total wait sum |
| --- | ---: | ---: | ---: |
| +0.336→27.336s |7120 |4.085534s |97.555% |
| +27.336→180.336s |4885 |0.149273s |2.445% |
| Combined |12005 |2.483815s |100% |

These are admission cohorts, including any waiting before the first endpoint.
Idle seconds with no jobs do not add zero-latency observations to a per-job
mean. Counts are not distinct textures, successful decodes or visible
resolution completions. Queue wait includes FIFO residence, gate waiting and
scheduling; network/cache acquisition precedes submission, and codec work,
callback and Metal upload follow admission.

## The queue grew while six workers were occupied

Times below are logged endpoints with one-second timestamp resolution.
Rates average each interval; queue gauges are not a continuous census.

| Interval, trace seconds | Submitted | Admitted | Submission/start rate | Waiting at end |
| --- | ---: | ---: | --- | ---: |
| +0.336→1.336 |867 |214 |867/s vs214/s |836 |
| +1.336→6.336 |2385 |1361 |477/s vs272.2/s |1860 |
| +6.336→11.336 |1062 |1457 |212.4/s vs291.4/s |1465 |

Across the first logged6s,3252 submissions exceed1575 starts by1677,
exactly explaining pending growth183→1860. Once starts exceed arrivals,
old requests still have to drain. The first empty snapshot is+17.336s.
A second burst reaches644 waiting at+22.336s and is empty at+27.336s.

At the early saturated rate281.8 starts/s,1860 queued jobs represent roughly
6.60s of backlog under an unchanged-workload/no-new-arrivals scale check.
This agrees with observed7.611s oldest pending age and8.207s maximum admitted
wait. It is not a predicted drain time: job sizes and arrivals change.

Time Profiler independently shows5.901 decoder core-equivalents in5–10s,
5.802 in10–15s, and5.847 averaged over the first15s after the finished
callback. Six slots were doing substantial CPU work. MQ/bitplane entropy
accounts for85.67% of decoder CPU. Three-minute averages include a long quiet
tail and do not measure burst capacity.

The matching12009-job marker/release cohort has about10ms total
service-minus-decode tail (approximately±2ms printed precision), including
callbacks. It does not support callback contention as a dominant cause.
214.378 sampled decoder CPU-s and215.328 job-wall seconds corroborate CPU
work; different time/cohort boundaries prohibit an exact utilization ratio.
Named scheduler samples are only a lower bound on its overhead.

## Contribution of adaptive admission

The limit reaches six at logged+1.336s and stays there until+129.336s.
Quiet reductions and later Fair thermals occur after the large backlogs.
With six threads and limit six, a worker reaching the gate has at most five
other worker permits; the gate does not deliberately park an available worker.

The captured4→5→6 ramp can contribute initial delay. An unchanged-service
fluid model gives1.672 missing permit-seconds, or0.279s equivalent six-slot
drain capacity; timestamp uncertainty expands this to roughly0.279–0.612s.
This is not a causal per-job bound or a measured fixed-six comparison, and
excludes pre-trace demand. Continued growth at maximum admission establishes
a capacity burst beyond the brief ramp.

## Ordering and remaining uncertainty

The decode queue is FIFO. decodeImage passes image, discard, aux flag,
responder and request ID, without visibility/HUD priority or queued
cancellation. Posted requests cannot be reprioritized through this API.
Obsolete callbacks are rejected after processing. Progressive/coarse decodes
can be useful; counters do not identify obsolete or low-priority work.

The LLTextureFetch introductory priority-queue comment is not proof of current
decode scheduling: LLQueuedThread posts/retries plain FIFO WorkQueue callables.
Image priority still sorts HTTP resource waiters but is not forwarded into
decodeImage. Exact current-source lines are in the private source review.

The frozen heavy trace has no printed DECODE FAILED/codec-error warning.
Generic fallback-word matches are MetalMemory diagnostics, not codec fallback.
The selected OpenHTJ2K branch has no runtime OpenJPEG decode fallback;
OpenJPEG decoding is compile-time alternative code. Suppressed debug messages
and uninstrumented outcomes still prevent an absolute retry/waste count.

The evidence supports testing six versus eight burst workers while measuring
UI/frame impact, and counting queued work by visibility/priority, progressive
generation and obsolescence before choosing a priority/cancellation policy.
Scaling on8P/2E hardware and frame safety remain unmeasured. Preserve useful
coarse previews and responder ownership. No implementation or gain is claimed.

## Evidence

Private directory: artifacts/metal/adaptive-decoders-20261112/runtime-180348/queue-forensics/.
arrival-service-backlog.py/.json/.md, admission-intervals.csv,
cpu-capacity-analysis.py, cpu-capacity-findings.json/.md, source-review.md and
execution receipts retain calculations/input hashes/limits. The36 inner CSV
intervals telescope to12005 starts and the original wait total;38 intervals
include the bracketing endpoints. Calculations exit0; raw captures and CPU
exports remain untouched. queue-explanation.png/.pdf and plot_queue.py use
observed queue markers, actual interval rates and5s CPU bins. Offline
matplotlib rendering exits0; root visually inspected the PNG.
