# Projector-fix viewer: teleport, decoding and busy-scene hotspots

The recording shows two different costs: a large initial texture-decode burst,
and persistent render work after that burst subsides. Four OpenHTJ2K workers
consume **204.803 sampled CPU seconds** over 181.040704 wall seconds. Combined
directional/projector shadows remain the largest main-thread owner:
**48.475 CPU seconds,40.52% of main sampled CPU**. An Overview zero-presentation
interval also needs investigation before choosing a broad rendering change.

## Capture and accounting

- Exact viewer 26.4.0.68869, source 6404f9f6ad431dda9e679f9d220b5a79ba86a20e,
  Release/O3/LTO off, ARM64, OpenHTJ2K 0.27.0/four outer workers/one lane per image.
  Binary SHA256 d71560aa643ddd90b7aa07c79120c5d5311516a9bd58ff2761e0e71b598e94e5;
  UUID 506571D0-455F-3A76-9DB9-C714EFADF593 matches its trace symbol store.
- Apple M1 Max/64 GiB/macOS 26.5.2/Xcode 26.6. Game Performance Overview,
  PID 86326,14:23:13.598–14:26:14.638 UTC on 2026-11-12,181.040704 seconds.
  Record/TOC export exit 0, end reason Time limit reached.
- Owner teleport begins at 25.402s and finishes at 28.402s relative to trace.
  The capture includes the transition and 152.639 seconds after teleport completion.
  Packet arrival is logged at26.402s; analysis uses teleport-finished28.402s as
  its operational arrival boundary.
- CPU export contains 410258 target Running samples/410.258 CPU seconds.
  Main thread 119640 samples/119.640 CPU seconds. Summed worker CPU can exceed
  wall duration. These are sampled execution costs, not texture latency,
  frame time, GPU duration or full runnable/wait accounting.
- DWARF warnings occurred during recorder finalization, but 99.995% of decoder
  leaf weight and 99.392% of main leaf weight resolve to named symbols. This
  does not guarantee every optimized ancestor/inline/source-line frame.

## Texture decoding

Worker CPU seconds are 51.180,51.113,51.677 and 50.833: balanced, with no observed
single-worker imbalance. Together they represent 49.92% of the viewer process's
sampled CPU, averaging 1.131 core-equivalents over the whole capture. Two mesh-LOD
workers add 28.134 CPU seconds; main rendering and other work are separate.

| Period | Wall seconds | Decoder CPU seconds | Average core-equivalents |
|---|---:|---:|---:|
| Before teleport |25.402|0.011|0.0004|
| Teleport transition |3.000|0.090|0.030|
| First 30 seconds after teleport completion |30.000|115.564|3.852|
| Next 30 seconds |30.000|59.686|1.990|
| Remaining time after first minute |92.639|29.452|0.318|
| Late trace bin 150–180 seconds |30.000|2.364|0.079|

The late bin overlaps the preceding remaining-time row. At first the queue is
feeding nearly four busy workers; decoding falls substantially later while
presentation remains near 23 FPS. Decoder effort is therefore important for
arrival/loading, but cannot explain all later frame cost.

Within decoder CPU, the disjoint leaf partition assigns 95.110 seconds to MQ
arithmetic and 77.388 seconds to bit-plane processing:172.498 seconds,84.23% combined.
Inverse-wavelet math takes 11.272 seconds/5.50%; color transform 2.391 seconds,
dequantization 2.291 seconds, viewer adapter self work 5.659 seconds. Dominant
functions are `mq_decoder::decode`, `decode_sigprop`, `decode_cleanup` and
`decode_magref`; NEON wavelet/color functions are already observed. Inclusive
wavelet ancestry contains entropy work and must not be mistaken for wavelet
math alone.

This recording has no completed-texture count or per-request start/end events.
It cannot provide average milliseconds per texture, queue delay, time to the
last sharp texture, or decode-to-visible latency. The earlier OpenHTJ2K profile
had 70.591 decoder CPU seconds, but began 34 seconds after arrival in an unmatched
workload. The larger current total is not evidence of decoder regression.

## Main-thread costs

These rows are a precedence-defined, mutually exclusive partition; they add to
119.640 CPU seconds. Avatar work inside shadows/geometry belongs to those rows.

| Main-thread stage | CPU seconds | Share of main CPU |
|---|---:|---:|
| Directional/projector shadows |48.475|40.52%|
| Texture updates/publication |13.139|10.98%|
| Main deferred geometry |12.585|10.52%|
| UI |7.886|6.59%|
| Avatar idle/update outside above owners |5.852|4.89%|
| Other main work |31.703|26.50%|

Within those owners, overlapping subtrees identify concrete work:

- `stateSort OR postSort`:23.113 CPU seconds/19.32% main, with 17.644 seconds
  inside shadows. `endMetalSpotShadowCapture`:20.587 seconds includes backend
  preparation/submission; it is nested in the shadow row, not additional time.
- Main memory-copy leaves total 9.835 seconds/8.22% main:3.965 under shadows,
  5.091 under texture updates,0.779 elsewhere. Immediate copy callers include
  RGB/upload preparation 3.248 seconds, buffer writes 2.632, texture upload 1.637
  and frame-arena publication 1.014. They include dirty geometry, not just palettes.
- `createMetalTexture`:9.209 seconds includes 5.969 seconds in
  `prepare_metal_upload_pixels`. The RGB expansion at
  `llimagegl_upload_native.inl:327–356` and the separate staging copy at
  `almetalresource.mm:1512–1527` are specific measured investigation boundaries.
- `finiteValues` exclusive leaves:3.716 seconds/3.11% main. Shadow completion
  accounts for 1.785 seconds, post-deferred 1.059 and G-buffer 0.849; not every
  scan is a palette. Projector frontend/backend callers account for only
  0.002/0.008 seconds. The PPD-001 validation is not a significant sampled cost.
- `LLFace::calcPixelArea`:2.702 exclusive CPU seconds/2.26% main. It is not
  another disjoint stage to add to texture updates.

The recurring shadow owner remains near the 41% main share of earlier long
profiles, but different sessions/settings prevent a speedup or regression claim.
Existing per-frame palette caching, static batching and native binding caches
already exist. Actual view preparation, repeated validation/publication and
rigged emission need measurement; do not infer six complete mesh rebuilds.
Probe-tagged main scopes are only 0.043 CPU seconds here, so this capture does
not justify prioritizing a broad probe rewrite.

## Cadence and the presentation anomaly

Overview reports 20.270 FPS over 179.596 covered seconds/99.20% of trace duration;
post-arrival 20.991 FPS, last 60 seconds 23.023 FPS, last 30 seconds 22.977 FPS. These are
duration-weighted presentation interval aggregates, not individual-frame p95/p99
or proof that each frame contains a newly rendered world.

From 49.368–64.358 seconds, fifteen buckets report zero presented frames:
14.318 seconds of zero-covered data inside a 14.989 second span, with 0.672 seconds
uncovered. All belong to the same viewer layer. The buckets report162 skipped
frames and zero drawable-associated command-buffer/GPU values. Those zeroes
do not establish zero offscreen command activity or zero GPU execution.

The main thread continues working:7.489 CPU seconds in that span, including
shadows3.353, postSort2.393 and geometry rebuilds; decoders use58.538 CPU seconds.
Small G-buffer, post-deferred, projector, command-commit and presentation-prep
stacks remain sampled. Viewer-loop statistics report10.9 FPS at 59.402 seconds,
inside the Overview gap. That is not an independent actual-present counter.
There is no single 15-second CPU function explaining the interval.

The owner recalls the viewer being visible and active but is not certain;
visibility remains unverified. Both per-frame export tables are empty.
Visibility/occlusion, instrument dropout,
publication readiness, queued GPU work and unsampled waits cannot be separated
with this capture alone. The record establishes an Overview anomaly to correlate,
not a proven 15-second total application freeze or the cause of PPD-002.

Later, the drawable GPU-in-flight/frame-interval ratio is near 1.0. This indicates
that a CPU/GPU critical-path capture is worthwhile; it is not GPU hardware
utilization or proof of a shader bottleneck. Thermal state stays Nominal.
Reported peak process footprint rises 13.637→27.007 GiB and is flat in the last
minute. During the gap, process metric buckets report about 62.1 MiB/s writes and
11.1 MiB/s reads. These are correlations, not a leak or I/O bottleneck diagnosis.
Periodic native-pressure logs show system_low=0, discard bias=1 and device usage
well below its recommendation; they do not demonstrate memory-pressure eviction.

## Recommended next investigations

1. **Resolve presentation/fresh-world critical path first.** Capture Metal System
   Trace with frame IDs and native publication/readiness/failure reasons,
   command commit/completion, in-flight slots, drawable acquisition and actual
   present timestamps. PPD-003's reset/suppressed diagnostics remain relevant.
   Distinguish failed publication from visibility/dropout and GPU backpressure.
2. **For sustained FPS, split shadow preparation from native submission.** Measure
   per-view sort/rebuild time, batches/draws, binding changes and unique versus
   reused palette generations. Target `generateSunShadow`, per-view
   `stateSort/postSort`, and `endMetalSpotShadowCapture/renderSpotShadow`.
   Trial immutable validated/published data reuse only where counters prove
   repeated unchanged work; preserve original visibility, coverage and order.
3. **For faster texture convergence, instrument decode→visible latency.** Record
   queue delay, requested discard/resolution, repeated decode passes, RGB
   conversion, staging/mips, commit and first displayed generation. Benchmark
   conversion plus staging as one boundary. Four workers already run nearly
   flat out during the initial burst; a controlled concurrency experiment may
   improve loading, but this trace cannot predict its frame/memory tradeoff.

The next broad codec replacement has less evidence than these concrete boundaries.
No application code changed for this analysis. PPD-001 remains ready_for_owner;
owner acceptance and permission to push remain pending.

## Reproducible local evidence

Ignored root: `artifacts/metal/ppd001-profile-20261112/`.
CPU export, preserved analyzer and custom decoder/stage analyzer exit 0;
all six metrics exports and metrics analyzer exit 0. Exact argv/exit/hash receipts:
`cpu-export.json`, `cpu-analysis-execution.json`, `decode-analysis-execution.json`,
`metrics/export-commands.json`, `metrics/analysis-execution.json`.

Results: `cpu-analysis.json`, `decode-analysis.json`, `cpu-stage-analysis.json`,
`metrics/{summary,overview-analysis}.json`, `runtime-analysis.json`,
`runtime-stall-analysis.json`, `runtime-memory-samples.json`,
`symbol-identity.json`, `hotspot-source-notes.{md,json}`.
`plot_profile.py` produces standalone `profile-timeline.{png,pdf}` with
`uv run --with matplotlib python <artifact>/plot_profile.py` (exit 0).
Only redacted aggregate findings enter this note; raw traces, environment,
region/account/asset data remain local. Live graphics/camera/cache/avatar
conditions were not matched against earlier runs. No formal performance gain,
visual parity, fresh-world pass or GPU instruction hotspot is established.
