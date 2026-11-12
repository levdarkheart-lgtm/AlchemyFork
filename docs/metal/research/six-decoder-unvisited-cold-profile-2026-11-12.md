# Six decoders — second cold-cache login capture

The owner requested another run at a never-visited login destination after
feeling the first arrival loaded quickly. A separate dedicated viewer cache
contained **zero files / zero bytes immediately before launch**; its active
path was verified. The original cache and first capture/cache remain preserved.
The destination's visit history is the owner's report; no region name was
independently verified. Bundled/local textures, operating-system caches and
remote-service state were not cleared.

## Capture identity and coverage

Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6. Same six-decoder ARM64 Release/O3,
LTO-off viewer68876, OpenHTJ2K0.27.0, source
`c5c57a80bd35d80e97f4cbe2b2be8a945e7ce9d7`, PID10691. Executable SHA256
`6558ad34136c5deac313a17c0025503d230296d0b6c5c97d19d38770b72dd0d8`.

The owner explicitly requested recording now. Normal capture/save exits0:
**17:19:32.032–17:22:33.248 UTC,181.216215s**, stopped at its three-minute limit.
Login cleanup occurs at+3.968s, initial world movement/precache at+7.968s and
STATE_STARTED at+14.968s; logs have one-second precision. No distinct nonlocal
teleport start occurs. TOC, CPU and six metrics exports/analyzers exit0.

## Loading and rendering costs

| Measurement | Observed result |
| --- | ---: |
| Six decoder workers combined | 62.173 sampled CPU-s |
| Individual decoder workers | 9.810–10.896 CPU-s |
| Busiest five-second decoder window, trace10–15s | 2.425 sampled core equivalents |
| Last60s / last30s decoder execution | 0.078 /0.000 CPU-s |
| MQ arithmetic + bitplane entropy | 51.858 CPU-s /83.41% of decoder CPU |
| Inverse wavelet math | 3.469 CPU-s /5.58% of decoder CPU |
| Decoder samples on P cores | 99.55%, without affinity or reservation |
| Main-thread texture update /native texture creation | 4.346 /2.788 CPU-s |
| Pixel preparation /native upload CPU | 1.757 /0.698 CPU-s, nested within creation |
| Main CPU /all viewer CPU | 85.000 /202.298 CPU-s |

CPU-seconds sum sampled execution across workers, not elapsed loading time.
All six ran, but this visit did not sustain six cores of decoder pressure at
five-second resolution. Absence of late decoder samples does not prove all
textures reached their target resolution or identify outstanding network work.

After STATE_STARTED, shadows consume33.003 of81.171 main CPU-s (**40.66%**).
In the last30s they consume5.869 of13.988 (**41.96%**). This is CPU preparation
and submission ancestry, not shadow GPU execution time.

World-entry cadence averages **45.03 FPS**, first30s54.33 and last60s42.70.
Whole-trace50.58 includes faster login UI. GPU-associated active/in-flight spans
average30.787ms after STATE_STARTED and33.450ms in the last60s. These overlap
and can include scheduling/idle; ratio1.0 late is not hardware utilization.
No individual GPU-pass timing or frame-time percentiles are available.
Overview coverage is99.79%, with no zero/below10FPS buckets; thermal state
remains Nominal. Peak footprint rises0.725→8.971GiB, compared with23.111GiB in
the first six-worker cold run. The scenes/workloads differ substantially.

## Interpretation and next measurement

The [first cold capture](six-decoder-cold-profile-2026-11-12.md) exercised more
decoder concurrency, peaking at5.72 sampled cores over five seconds, versus
2.43 here. Neither the lower work total nor higher FPS in this different scene
establishes a worker-count speedup. A matched four/six-worker comparison and
texture counts, queue ages, acquisition waiting and target-resolution completion
are still needed to measure loading improvement. Entropy decoding is the main
codec cost; shadow preparation remains a separate steady rendering cost.

Twenty-two retained live snapshots span+96.440–179.951s. The collector initially
used the wrong timestamp field, then was corrected; no viewer control changed.
Earlier live settings were not retained. Late settings remain six workers,
draw distance112, SSR enabled, shadow detail2/scale1, spot maximum2048,
shadow occlusion0/cull0, UseOcclusion1, threaded textures0/media1. Position
changes; camera/content equivalence is unproved. Periodic pressure logs show
system-low0/discard bias1. These are observations, not continuous proof.

Private evidence: `artifacts/metal/six-decoders-20261112/cold-unvisited-171832/`:
`record-result.json`, `profile-toc.xml`, `frozen/`, `cold-cache-proof.json`,
`runtime-analysis.json`, `cpu-analysis.json`, `decode-analysis.json`,
`cpu-stage-analysis.json`, `metrics/summary.json` and adjacent exact command,
exit and hash receipts. The cache snapshot after save contains6610files/
775226264 logical bytes; this is not downloaded texture bytes or physical
residency. Retain this exact cache for the warm repeat. Viewer remains open;
no warm run, owner acceptance or push is claimed.

The private `timeline.png`/`.pdf` and `timeline.py` show the six decoder workers,
presented cadence and GPU-associated spans on the same trace clock; source
bin totals cross-check62.173CPU-s. Offline matplotlib rendering exits0 and
the PNG was visually inspected. Gaps and metric-semantics limits are retained.
