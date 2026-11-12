# Six decoders — first cold-cache login capture

Retain this as the **first cold viewer-cache login capture**, separate from the newly requested fresh-cache login at a never-visited location. It establishes neither a matched four-to-six-worker speedup nor a warm-cache result.

Host: Apple M1 Max /64 GiB /macOS26.5.2 /Xcode26.6. Viewer **68876**, PID **8113**, ARM64 Release/O3/LTO off, OpenHTJ2K0.27.0, source `c5c57a80bd35d80e97f4cbe2b2be8a945e7ce9d7`. Executable SHA256 `6558ad34136c5deac313a17c0025503d230296d0b6c5c97d19d38770b72dd0d8`. The dedicated viewer cache contained **zero files / zero bytes before launch**, and runtime receipts confirm its use. The existing user cache was not deleted. Operating-system caches and remote-service state were not controlled.

## Capture and phase-specific results

Recording completed normally: **17:02:07.353–17:05:09.481 UTC**, **182.128547 seconds**, recorder/export/analysis exits 0. Recording-active confirmation preceded the owner's login messages. Logs place initial login movement at **33.647 s** and `STATE_STARTED` at **44.647 s**, with one-second timestamp precision. A teleport-finished callback during login does not establish a separate nonlocal teleport.

Whole-record cadence is 46.60 FPS because startup/login UI is included. Use these world-entry windows instead:

| Recorded period | Presented cadence | Decoder CPU seconds | Shadow share of main CPU |
| --- | ---: | ---: | ---: |
| After `STATE_STARTED` | 25.04 FPS | 133.027 s | 37.89% |
| First 30 s after `STATE_STARTED` | 31.35 FPS | 87.203 s | 28.31% |
| Last 60 s | 23.54 FPS | 20.873 s | 41.74% |
| Last 30 s | 23.02 FPS | 20.570 s | 42.56% |

Overview buckets cover **99.33%** of the trace: no zero-FPS or positive-below-10-FPS buckets; minimum bucket 17.95 FPS. Individual-frame tables are empty and coverage has gaps, so there is no frame-time percentile or every-present fresh-world guarantee. Thermal state is nominal. Footprint rises from 0.69 to 23.11 GiB during loading; that alone is not a leak diagnosis.

GPU-associated active/in-flight spans average **54.688 ms after STATE_STARTED** and **65.051 ms in the last60s**; late associated activity ratio is1.0. These overlapping intervals can include scheduling/idle and are not execution-only GPU time or hardware utilization. No individual shadow-pass GPU attribution was captured.

## Texture-loading work

Independent CPU passes agree on **393,912 target Running samples**, **120.176 main CPU seconds** and **393.912 total process CPU seconds**. Startup and sampled execution confirm **six decoder workers**. They consume **184.944 CPU seconds**, individually 30.421–31.319 seconds.

**83.80% of decoder CPU** is MQ arithmetic and bitplane entropy decoding (85.505 + 69.476 seconds). Inverse wavelet math is 10.569 seconds / 5.71%; OpenHTJ2K and NEON transform functions execute. Inclusive wavelet stacks include entropy callbacks and are not isolated transform cost.

The busiest five-second bin averages **5.72 decoder core-equivalents**; the first five seconds after `STATE_STARTED` average **5.33**. These are sampled CPU weights divided by elapsed time, not direct simultaneous occupancy. Decoder samples run **97.05% on P cores / 2.95% on E cores**; all six appear on both kinds, without affinity or reserved cores.

Decoding declines over 90–150 seconds, then resumes at 150–180 seconds with **20.571 CPU seconds**, involving all six workers. The scene cannot be called permanently settled.

After decode, main-thread texture updates consume **11.124 CPU seconds**. Native texture creation owns **7.806 s inclusive**, including pixel preparation **5.006 s** and native upload **1.811 s**. These overlap. Explicit copy leaves attribute **2.717 s to pixel preparation** and **1.330 s to upload**: conversion/staging remain concrete loading-cost targets. These are CPU costs, not GPU completion latency. Shadows remain a separate rendering expense, as the phase table shows.

## Limits and follow-up

Live settings are observed only at **131.221–179.211 s**. They remain unchanged there: six decoders, `EmulateCoreCount` 0, `RenderGLMultiThreadedTextures` 0, draw distance 112, shadow detail 2, resolution scale 1, spot maximum 2048, SSR enabled, shadow cull mode 0, shadow occlusion disabled and `UseOcclusion` 1. Position changes. Earlier graphics/camera/avatar equivalence is unproved. Recorded pressure values are system-low 0 and discard bias 1; these are observations, not continuous resource-state proof.

Completed texture counts, bytes, cache-hit coverage, queue depth/age, disk-versus-network waiting, per-texture latency and time-to-sharpness are unmeasured. Earlier four-worker captures have different cache, scene and arrival histories; neither CPU totals nor aggregate FPS proves worker-count speedup or an optimal count. GPU-associated intervals are not execution-unit utilization.

Preserve this run independently. The newly requested never-visited-location capture and any warm follow-up are **not claimed completed here**.

Private evidence root: `artifacts/metal/six-decoders-20261112/cold-profile/`.
Normal `xcrun xctrace record --template 'Game Performance Overview' --attach 8113 --time-limit 3m --no-prompt --output .../six-decoders-cold-3min.trace` exits0.
TOC/time-profile/six metrics exports and CPU/base/stage/runtime/metrics analyzers
exit0; exact argv/exits/hashes are in the adjacent receipts. Key outputs:
`cpu-findings.json`, `decode-analysis.json`, `cpu-stage-analysis.json`,
`metrics/summary.json`, `runtime-analysis.json`, `cold-cache-proof.json`.
The prior launcher initially checked cache initialization too early and exited1;
the same still-running process was verified afterward without another launch.
The observer requested a nonexistent `TextureMemory` key; removing that query
at+125.843s restored observations without changing viewer settings. Both issues
are corrected in the new unvisited-location run's launcher. Raw CPU XML remains
unchanged. No new viewer source change, owner acceptance or push is implied.
