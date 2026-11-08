# Full-screen 68550 runtime comparison — 2026-11-08

The owner explicitly authorized closing `68547` and requested the same test:
full-screen login, Midday, `Preset123`, settling, frame collection and the
previous CPU/GPU measurements. This run executes that request against
[the draw-payload/shared-depth repair](CURRENT-draw-payload-depth-reuse-checkpoint-2026-11-08.md).
No additional renderer or graphics repair was applied during measurement.

## Verified setup and provenance

- Candidate Xcode Release **26.4.0.68550**, renderer source
  `175cd155dd90947066113d3dbe8015f1c394a91f`, launch repository HEAD
  `bbabaa72f97c038e3b9f185afd73a3d2a7d78bcc`.
- Exact bundle:
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
  Executable SHA-256:
  `b54af1c45224738f9a8f6ec42e17a5ea3cc20478cb900e260c77101d9a66ecf9`.
  Packaged metallib remains unchanged, SHA-256:
  `5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`.
- Native CUA quit the older viewer, selected the `68550` login window and
  clicked full screen **before** Log In. The resize log is `1728x1084` points
  at 20:33:44 UTC; native world logs confirm **3456x2168** pixels. Login
  reached `STATE_STARTED` at 20:34:02 UTC.
- Sent the Midday shortcut, opened camera presets, read `Preset123`, explicitly
  selected it, closed the panel and sent Midday again. Setup completion was
  confirmed by 20:37:36 UTC; the camera was untouched afterward.
- The live verifier passed exact binary, world-global position and all 21
  compared Ultra controls. Those match the previous run, including anisotropy
  4, 256 m, shadows 2, SSAO, mirrors, hero 1024/update rate 1 and occlusion.
  The first sandboxed verifier could not signal-check the PID; rerunning with
  host process access passed. This was not an application exit.
- Same physical M1 Max/64 GiB acceptance Mac, macOS 26.5.2/Xcode 26.6, AC
  attached, no recorded power/thermal warning and Nominal trace thermals.
- Local run manifests/logs:
  `artifacts/metal/qa-runs/candidate-68550-draw-payload-comparison-20261108-01`.
  Primary raw measurements:
  `/private/tmp/alchemy-metal-68550-draw-payload-metrics-20261108.DnhpTn`.
  Baseline is the primary maximized `68547` evidence in
  `/private/tmp/alchemy-metal-68547-hero-cpu-metrics-20261108.Ol8EPh/maximized`.
  Raw captures remain local; no account-specific captures are uploaded.

## Consecutive candidate frame collection

The opt-in collector requires both 120 warm-up frames and **300 seconds**,
then 300 exact-native samples. It finished at **20:39:37 UTC**, after setup
and more than the requested minute of settling. Instruments was started only
after this collection completed.

The existing `perf_compare.load_capture(require_provenance=True,
candidate=True)` validates all 300 rows: consecutive indexes 2508–2807, source
`175cd155dd…`, Release, quality 6, 3456x2168, 300 exact/submitted publications
and zero GL fallbacks.

| Candidate wall-frame statistic | Result |
| --- | ---: |
| Frames | 300 |
| Duration | 34.373215 s |
| Median | 112.7755 ms |
| p95 | 133.6875 ms |
| Mean | 114.5774 ms |
| Range | 101.4542–201.0145 ms |
| Frames divided by elapsed time | 8.7277/s |
| Mean native submissions/frame | 29.48 |

`metal_gpu_ms` averages 424.4434 ms but is an overlapping asynchronous command
duration sum, **not whole-frame GPU duration**. The current `draw_calls` field
is zero throughout and does not provide an actual native draw census.
`68547` has no admissible matching CSV because its earlier collection preceded
final camera/window setup. No self-comparison or synthetic baseline was used;
the formal paired 300-frame product gate remains open.

## Matched Metal trace

The final untraced window starts after CPU/GPU recording, finalization and all
exports have ended. Compare the first 20 ten-second rows from 20:51:00 onward
with the previously published `68547` window:

| Settled log statistic | 68547 | 68550 |
| --- | ---: | ---: |
| UTC window | 19:33:26–19:36:37 | 20:51:01–20:54:13 |
| Samples | 20 | 20 |
| Median FPS | 8.2 | **8.8 (+7.32%)** |
| Mean FPS | 8.16 | **8.745 (+7.17%)** |
| Range FPS | 7.9–8.3 | 8.3–8.9 |

The new trace runs **20:42:50.727–20:43:21.367 UTC**, exactly 30.640063 s,
with 220 presents. The old trace spans 30.633711 s with 208 presents. All
per-present costs below use those complete present counts consistently.
Presentation-event intervals are distinct from the CSV's CPU wall-frame times.

| Metric | 68547 | 68550 | Change |
| --- | ---: | ---: | ---: |
| Traced presents/s | 6.7899 | 7.1801 | +5.75% |
| Presentation interval p50, ms | 128.592 | 123.043 | -4.32% |
| Presentation interval p95, ms | 156.513 | 147.301 | -5.89% |
| Total CPU encoder ms/present | 65.6952 | 59.3295 | -9.69% |
| Main G-buffer/sky CPU ms/present | 11.1322 | 8.6917 | -21.92% |
| Hero CPU encoder ms/present | 31.4189 | 29.5568 | -5.93% |
| Hero shadow CPU ms/present | 14.5808 | 13.7969 | -5.38% |
| Hero G-buffer CPU ms/present | 13.1287 | 12.2960 | -6.34% |
| Nonhero shadow CPU ms/present | 19.2200 | 16.8196 | -12.49% |
| Encoders/present | 171.4856 | 163.5909 | -4.60% |
| Submissions/present | 29.6731 | 29.7318 | +0.20% |
| GPU-active wall union | 62.136% | 67.068% | +4.93 percentage points |
| Metal allocation, GiB | 5.214–5.323 | 4.425–4.519 | resize-history confound |

Ordered encoder sequences directly confirm the targeted base-light depth
copies fell from **8.0144/present to zero**: main light-chain Blit 4, hero
Blit 9 and standalone base-light Blit 1 disappear immediately after base
lighting. This identification follows pass ordering, not numeric blit names
alone. Later water/haze snapshots remain. Removing encoders inside existing
transactions does not require fewer command submissions.

Hero transactions remain 6.0182/present and directional shadow passes
12.0364/present, preserving two maps per face. GPU labelled intervals remain
large: main haze 17.2423, main base 17.1014 and all hero-labelled work
40.5369 ms/present. The latter rises from 36.2189; hero haze rises 32.75% and
shadow intervals 32.43% despite unchanged shader code and essentially unchanged
shadow counts. These intervals overlap and are not a serial whole-frame GPU
budget. Their rise is unresolved and must not be attributed to depth aliasing,
frequency or changed geometry without more evidence.

The recorded GPU-state tables do not support a lower categorical performance
state in the candidate: Maximum covers 96.31% versus 96.93% of recorded state
time, with coverage of 80.69%/82.67% of total trace time. No MHz values are
provided. Nominal thermals alone do not establish identical clocks.

The allocation reduction is not a clean code-only memory result. The prior
run retained a 35-target/1,125.54-MiB pool after resizing from a smaller world
window; this run enters full screen before login and has a nine-target/
292.966-MiB pool. Both source changes and run history affect the footprint.
The matched FPS windows have 13 old and 14 new memory samples: driver median
5,422.83 to 4,566.835 MiB, buffer estimate 319.893 to 286.2615 MiB. The pool's
832.574-MiB difference explains most of the 855.995-MiB driver reduction.
Discard bias remains 4 in both runs.

## CPU attribution and remaining bottlenecks

All 28,894 candidate running-main-thread samples have valid backtraces and
1 ms weights over a 30.517-second span. Address-only leaves rise from 3.53%
to 5.34%; Instruments also emitted DWARF cursor warnings while resolving some
objects. Symbolized ancestors remain available, but leaf attribution has that
limit. Fixed-duration busy-main totals are not a throughput metric.

| Main-thread sampled category, ms | 68547 | 68550 |
| --- | ---: | ---: |
| Running main | 28,961 | 28,894 |
| Strict hero call tree | 14,473 | 14,521 |
| Named constructor/emplace inclusive union | 2,658 | 960 |
| Batch-associated memset leaf | 2,217 | 54 |
| Wider arena/data-setter union | 3,009 | 2,185 |
| Arena copy leaf | 845 | 403 |
| Arena hash leaf | 344 | 193 |
| Arena equality leaf | 250 | 183 |
| Native buffer-write inclusive | 1,279 | 130 |

The broader arena/data-setter union includes `setVertexData`,
`setFragmentData` and `setVertexAndFragmentData`. It falls from **10.39% to
7.56%** of main activity. The narrower named arena/hash category falls
9.60% to 4.58%, but some attribution moves into unresolved getter leaves;
do not call that an exact halving of total arena cost. Likewise, replacing
outlined memset with stores means 97.6% fewer memset samples is not a 97.6%
total constructor speedup. Inclusive constructor/emplace attribution falls
9.18% to 3.32%, consistent with the separate construction benchmark.

Remaining CPU categories include culling **15.74%**, indexed draw encoding
**14.76%**, texture setters **10.38%**, vertex-buffer setters **6.87%** and
resource-list leaf work **5.48%**. These overlap and must not be added. The
main thread remains roughly 95% active, with hero rendering about half its
sampled work. The initial fixes are doing less work, while view traversal,
draw/binding submission and substantial GPU shading remain.

A source-exact disassembly check identifies an immutable buffer-length getter
within the publication capacity checks; caching page capacity is a possible
small follow-up. Its isolated benefit is unmeasured. No additional code change
was made during this comparison.

## Evidence and executed commands

Launch uses the visual-QA skill's exact-path/hash launcher and pinned Ultra
profile. Native CUA setup is recorded in `setup.json`. CPU and GPU commands
are `xcrun xctrace record --template <Time Profiler|Metal System Trace>
--attach 40740 --time-limit 30s --no-prompt --output <trace>`; both exit 0.
All exports and `tools/metal/trace_submissions.py` exit 0. CPU/GPU recordings
and exports do not overlap the final clean FPS window.
`python3 tools/metal/validate_harness.py` and `git diff --check` exit 0 after
the evidence update. No renderer source changed or new build was required in
this measurement task.

Local summary SHA-256 values:

- CPU comparison: `ae4b40145c27d5777de58e5d484b3ba7d1f2a75dbd7186cc5c9952fbfa375fe2`
- GPU comparison: `f1a73b05ea45589a9784f1fa48565b03c4d1be343327a154be9bc4baa3b798c5`
- Candidate CSV summary: `b9249b93d1c3cc0d985cf47102d5bdb5e6a216940547b509439d2f9294b3fde0`
- GPU state summary: `78e0deb8bb3495b05546a5dd3bf918583e2947e757b5ad7adbe55e3cec1eaa3c`
- Settled-log comparison: `9264013830061af9d5dec0885135f405c89cd485bf3c852bf305997e6f9aa597`
- Candidate CSV: `704c21dffbedf6e2c2d506815dd6c33b459ad8a5eb9bedddb1c8b16221784f7a`

This is another modest bounded before/after improvement. It does not establish
the 24-FPS objective, exceed the historical best or close formal paired
performance, visual parity and release gates. Exact camera transforms and
per-view geometry/residency censuses remain absent; the owner-requested
preset/location/settings are matched in a live, nondeterministic region.
