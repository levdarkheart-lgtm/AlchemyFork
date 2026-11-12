# Retained-cache profile: completed CPU, decode and GPU analysis

The retained-cache session still performs substantial JPEG2000 decoding, but
its steady frame cost remains in rendering. In the final 30 seconds, four
decoders use only **0.645 sampled CPU seconds**, while shadows consume **44.91%
of main-thread CPU** and presentation remains **22.377 FPS**. GPU-associated
frame spans stay substantial throughout. Shadow preparation/submission is the
strongest measured CPU subsystem to improve; encoder-level timing is needed to
choose the GPU work to reduce.

## What stopped, and what was recovered

Recording stopped at the configured three-minute limit. Apple xctrace then
trapped during recorder-agent teardown; the viewer remained running at that
point. Original failure status is SIGTRAP/-5, not a successful normal save.
The 423,432,368-byte raw ktrace and partial package were preserved. A separate
import succeeded, retaining **181.345001 seconds** of recording. CPU sample
coverage and broad frame-metric coverage were verified before this analysis.

The first handoff stopped after capture/recovery instead of completing the
analysis. This note completes that analysis; the crash does not justify treating
a recorded trace as an answered performance question.

Exact viewer: 26.4.0.68869, captured source
`6404f9f6ad431dda9e679f9d220b5a79ba86a20e`, ARM64 Release/O3/LTO off,
OpenHTJ2K 0.27.0, four decode workers and one inner lane per image.
Executable SHA256 `d71560aa643ddd90b7aa07c79120c5d5311516a9bd58ff2761e0e71b598e94e5`;
UUID `506571D0-455F-3A76-9DB9-C714EFADF593`. Host is Apple M1 Max/64 GiB,
macOS 26.5.2/Xcode 26.6. Capture runs 14:51:43.667–14:54:45.012 UTC on
2026-11-12, PID 89531. No teleport occurs inside it; the latest logged
teleport-finished event precedes it by 20.667 seconds.

Existing disk cache was retained across relaunch. Actual cache hits, texture
resolution/workload counts, camera, visibility, avatars and effective graphics
controls were not matched against the previous recording. Decoded CPU images
and Metal texture residency are not established by disk-cache reuse.

## CPU and decoding time

There are **335,315 target Running samples**, totaling **335.315 CPU seconds**
over 181.345 wall seconds: approximately 1.849 cores' worth of sampled execution
for this viewer process. Main thread uses 132.096 CPU seconds (0.728 core
equivalent); four ImageDecode workers use 142.911 seconds (0.788 core equivalent).
This excludes other processes and is not a whole-Mac CPU-utilization percentage.

The four workers are balanced: **35.841 / 35.121 / 36.030 / 35.919 CPU seconds**.
They account for **42.62% of the viewer's sampled CPU work**.

| Window from capture start | Decoder CPU seconds | Main CPU seconds |
|---|---:|---:|
| First 15 seconds | 59.520 | 9.865 |
| First 30 seconds | 104.310 | 21.054 |
| First 60 seconds | 106.962 | 44.319 |
| Last 60 seconds | 5.591 | 42.211 |
| Last 30 seconds | 0.645 | 20.121 |

Windows overlap. First 15 seconds average 3.968 busy-core equivalents across
four workers; last 30 seconds average only 0.0215. Later bursts remain: successive
30-second decode bins are 104.310, 2.652, 28.019, 1.930, 5.348 and 0.652 CPU
seconds. The exact last-30-second window differs slightly from the 150–180 bin.

Within decoder CPU, MQ arithmetic uses **67.733 seconds** and bit-plane loops
**54.816 seconds**, together **122.549 seconds / 85.75%**. Inverse-wavelet math
uses 7.049 seconds / 4.93%; viewer adapter self work 4.143 seconds; color math
1.643 seconds; dequantization 1.330 seconds. The remaining leaf weights include
unresolved external calls and other work. Function names confirm actual
OpenHTJ2K decode; NEON wavelet/color paths are already in use.

This is sampled execution, not milliseconds per texture, decode-queue delay or
time until every texture is sharp. Those require completed-texture counts and
queue/decode/upload/first-visible timestamps. More decoder workers could affect
initial loading, but cannot be credited with improving the nearly decode-idle
last-30-second frame rate from these data.

## Main-thread hotspots

The following precedence-defined rows are disjoint and add to 132.096 CPU seconds.
Avatar work inside shadow/geometry owners belongs to those owners.

| Main-thread owner | CPU seconds | Main share |
|---|---:|---:|
| Directional/projector shadows | **56.053** | **42.43%** |
| Main deferred geometry | 15.793 | 11.96% |
| Texture updates/publication | 11.826 | 8.95% |
| UI | 7.727 | 5.85% |
| Avatar idle/update outside above owners | 6.558 | 4.96% |
| Remaining work | 34.139 | 25.84% |

Important nested boundaries, not extra additive costs:

- `stateSort OR postSort`: **19.932 CPU seconds**, including **14.570 inside
  shadows**. `endMetalSpotShadowCapture`: 28.231 seconds; its native renderer
  subtree is about 27.9 seconds. Shadow preparation and native submission both
  warrant attention.
- Native rendering/encoder ancestry: **51.977 seconds**, overlapping the stage
  table. Named draw and binding wrappers recur; unresolved external leaves
  prevent a complete new driver-function breakdown.
- Hash-family exclusive leaves: **3.714 seconds**. Immediate caller
  `setVertexData` accounts for **2.726 seconds**; native-shadow ancestry 1.854
  seconds. These ancestry sets overlap. The specific CityHash leaf is 2.812
  seconds, not another cost to add to the family.
- `LLFace::calcPixelArea`: **3.317 exclusive seconds**, inside priority/texture
  work. `createMetalTexture`: **7.729 inclusive seconds**, including conversion,
  upload and publication work; `uploadMetalTextureRegion` is 3.152 seconds.
- In the exact last 30 seconds, shadow ownership remains **9.036 / 20.121 main
  CPU seconds (44.91%)**, while `createMetalTexture` falls to **0.022 seconds**.
  Last 60 seconds show shadows at 45.82% and texture creation only 0.145 seconds.
  Persistent shadow/render work is therefore separate from initial texture work.

Only main-executable symbols were restored. Named leaves cover 59.43% of main
CPU and 98.01% of decoder CPU. About 40.57% of main leaf weight is external
address-only work, but recovered caller ancestry still attributes much of it
to rendering owners. Binary symbols lack some cold-trace DWARF inline frames:
missing memcpy, AGX or `finiteValues` names do not prove zero work or a saving.

## GPU, frame timing and resource use

Apple's installed Instruments framework enum constants and call sites prove
that duration metric variant 0 is Average, 1 Min, 2 Max and 3 Stddev.
`metrics/duration-variant-proof.json` preserves symbols, disassembly and command
receipts. These are frame-count-weighted averages, not arbitrary unnamed IDs.

| Verified warm average | Value | Meaning |
|---|---:|---|
| Presented cadence | **22.423 FPS** | Covered interval aggregate |
| On-display frame interval | **44.620 ms** | Time between displayed drawables |
| CPU begin to present | **38.254 ms** | Command creation/drawable-acquire to client present; includes waits |
| GPU associated active/in-flight | **69.823 ms** | Associated command-buffer in-flight span |
| GPU begin to end | **70.232 ms** | First to last associated GPU work |
| CPU begin to display | **76.813 ms** | Frame-production start to display, not input latency |
| GPU end to display | **4.546 ms** | Last associated GPU work to display |
| Command buffers per presented frame | **15.990** | Frame-weighted scheduled-buffer count |

These CPU/GPU spans overlap across pipeline stages and consecutive frames.
They cannot be summed into one frame cost or used to infer 14 FPS from a 70 ms
GPU span. The GPU-in-flight/frame-interval ratio is 1.0 in every aggregate bucket;
it is evidence of persistent graphics pipeline activity/pressure, not measured
100% shader occupancy or proof that a particular shader is GPU-bound.
Apple documents that command-buffer timing can contain idle between encoders;
encoder timing better isolates the GPU workload:
[Metal performance monitoring](https://developer.apple.com/documentation/xcode/monitoring-your-metal-apps-graphics-performance).

Within this recording:

| Window | Decode CPU seconds | Presented FPS | GPU associated active/in-flight average |
|---|---:|---:|---:|
| First 30 seconds | 104.310 | 21.143 | 73.551 ms |
| Next 30 seconds | 2.652 | 22.874 | 67.122 ms |
| Last 30 seconds | 0.645 | 22.377 | 72.346 ms |

Thus graphics costs remain after decoder demand collapses. The capture does not
establish the CPU/GPU critical path or distinguish shader, bandwidth, dependency
and queueing limits. CPU-only savings must be checked against actual FPS/GPU
results rather than assumed to improve throughput.

Frame-metric coverage is 180.874 / 181.345 seconds (99.740%). No zero-FPS or
below-10-FPS buckets occur; minimum aggregate bucket is 14.268 FPS. Both Overview
per-frame tables are empty. Summary interval extrema are 29.167–91.666 ms, not
individual-frame p95/p99 or proof that every displayed frame has a fresh world.
Thermal state remains Nominal. Reported peak process footprint grows
17.415 to 23.331 GiB; that is not GPU texture residency or a leak diagnosis.
Disk Write/s averages 5.092 MiB/s, Disk Logical Write/s 4.791 MiB/s and Disk
Read/s 1.548 MiB/s. Last-30-second writes/reads fall to 0.567/0.109 MiB/s.

A bounded attempt to recover detailed GPU events from the same raw data also
completed: Metal System Trace reimport, TOC and event exports exit 0. Encoder,
label, command-buffer submission/completion, driver and GPU-execution tables
have zero rows. This recording cannot reconstruct labeled GPU pass/shader
hotspots. It does contain 4,065 CA present requests and 4,064 presented-handler
events; their timestamp semantics were not used for latency/percentile claims.
No new recording was performed for this probe.

## Comparison with the preceding recording

Same executable/source, but different recording phase and unverified matching
of scene, camera, settings, avatars and cache coverage:

| Measure | Earlier teleport recording | Retained-cache session |
|---|---:|---:|
| Trace duration | 181.041 s | 181.345 s |
| Decoder CPU | 204.803 s | 142.911 s |
| Main CPU | 119.640 s | 132.096 s |
| Shadow share of main CPU | 40.52% | 42.43% |
| Last-60-second cadence | 23.023 FPS | 22.840 FPS |
| Last-60-second GPU associated span | 68.816 ms | 70.114 ms |
| Last-60-second CPU begin to present | 37.799 ms | 37.789 ms |

The lower total decode work is descriptive, not a measured cache/codec speedup.
The earlier trace includes teleport and the initial burst; this one begins
post-arrival. The useful recurring observation is that late cadence remains
near 23 FPS and shadow ownership persists even when decoder work is small.
The prior zero-presentation anomaly is absent here; this does not prove its
cause or close PPD-002.

## Where to improve next

1. **Shadow palette/constant publication reuse: first bounded CPU experiment.**
   Source `almetalworld.cpp:1419–1450` hashes and byte-compares deduplicated data;
   `setVertexData:1581` republishes it. The measured dedup-enabled vertex sites
   are skin palettes. Begin with attachment palettes whose immutable pose/data
   generation can be proven; exclude view-dependent body/cloth in the first
   scope. Publish/validate once per qualified generation, reuse the retained
   slice across compatible passes, and preserve per-command completion lifetime.
   Existing matrix-math caching and byte dedup already work: this removes
   repeated scans/hash/admission rather than adding a duplicate content cache.
   Original `1eb44` `LLRenderPass::uploadMatrixPalette` reuses consecutive equal
   avatar/skin/shader runs. Direct measured hash budget is 2.726 CPU seconds;
   broader shadow preparation/submission requires additional changes.
2. **For sustained FPS, examine shadow preparation and rigged native emission.**
   Keep the entire 56.053-second owner as the subsystem target, with per-view
   sorting/preparation and roughly 28-second completion/submission subtree.
   A short settled-scene GPU encoder trace should determine which passes/draws
   dominate GPU work. Then broaden compatible batching or reuse immutable
   draw metadata where measurement proves repeated work. Do not remove shadow
   views, weaken coverage or change quality to manufacture a result. Pass-local
   avatar policy snapshots are a smaller candidate: retain camera-specific
   visibility, policy invalidation and cost-overlap limits.
3. **For faster loading, specialize RGB8 conversion and fuse staging.**
   `llimagegl_upload_native.inl:327–356` expands RGB into a temporary array;
   `almetalresource.mm:1512–1527` separately stages rows. Benchmark a specialized
   path writing exact RGBA bytes directly into aligned staging storage, keeping
   alpha, color space, mip behavior, lower-resolution generations and asynchronous
   publication. This targets the 7.729-second texture-creation boundary during
   loading; almost none of that remains in the final 30 seconds.

Keep the accepted OpenHTJ2K decoder while testing these. MQ/bit-plane entropy
work dominates its CPU; wavelet-only SIMD work has a much smaller budget.
A controlled four-versus-six-worker test is a loading experiment with frame-time
and memory gates, not a demonstrated cure for the steady 22–23 FPS. Per-request
resolution/redecode and decode-to-visible timestamps are needed before changing
progressive decoding or adding a decoded disk cache.

Existing static G-buffer joining is an explicitly default-off trial; query
boundary joining is not ranked as a proven warm-run win without effective-setting
and eligible-draw evidence. Name-label regeneration is also lower priority in
this particular recording. No application code was changed for this analysis.

## Evidence and verification

Ignored root: `artifacts/metal/ppd001-warm-profile-20261112/`.

- `capture-complete.json`, raw/original failure, recovery-preservation/import and
  TOC receipts retain original recording and recovery status.
- `symbol-recovery/receipt.json`: exact UUID/SHA/warm load, 20,351 of 20,575
  main-text PCs resolved; every non-frame-name XML byte unchanged. Partial dSYM
  was intentionally interrupted and was not used.
- `cpu-analysis-symbolicated-execution.json`, `decode-analysis-execution.json`:
  canonical and detailed analyses exit 0, assert exactly 335,315 rows,
  132.096 main and 335.315 total CPU seconds. Name canonicalization affects
  analysis only; both original and derived XML hashes remain unchanged.
- `decode-analysis.json`, `cpu-stage-analysis.json`, `hotspot-source-notes.*`:
  raw costs, explicit overlapping/disjoint partitions, source mappings and
  proposed validation requirements.
- `metrics/{summary,overview-analysis,gpu-metric-notes}.json`, duration proof,
  GPU/decode correlation CSV and Metal System reimport event-availability
  receipts: all relevant exports/analyzers exit 0. Detailed GPU absence is
  recorded, not replaced with guesses.
- `plot_warm_profile.py` produces `warm-profile-timeline.{png,pdf}` with
  `uv run --with matplotlib python <artifact>/plot_warm_profile.py`; exit 0,
  final PNG visually inspected.

Only redacted aggregates and source references enter versioned documentation.
Raw traces, runtime environment and private scene/account/asset data remain
local. Documentation harness must pass after this note. PPD-001 owner acceptance
and push permission remain pending; this analysis does not close PPD-002/003 or
establish release readiness, visual parity or a controlled performance gain.
