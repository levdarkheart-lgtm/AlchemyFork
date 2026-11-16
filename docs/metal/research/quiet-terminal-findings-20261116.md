# Quiet Home decline — clean Terminal recording, 2026-11-16

The owner-run Terminal recording reproduces the original fast-to-slow world
transition without Xcode's validation/capture helpers. The main thread waits
increasingly long for earlier native commands to complete. A growing reflection
probe shading pack is a concrete changing GPU workload; its share of the
slowdown is not yet proven. The original architecture goal remains paused and
PPD-319 remains open. No viewer, profiler, build or push was started for this
analysis.

## Evidence and provenance

Owner input: `/Users/dekstroza/Alchemy-quiet-l9vbrfzj`. An exact stable copy,
242 files/186,388,665 bytes, is private under
`artifacts/metal/quiet-terminal-analysis-20261116T172257Z/owner-run`.
`input-receipt.json` retains source/copy hashes and stability checks. Derived
`analysis-native/`, `analysis-cpu/`, `analysis-gpu/` and `analysis-log/` are
siblings, leaving the copied inputs unchanged.

Viewer PID62863, source `fd5e8e088e1224e150e7ae71a17f827faa18249e`,
26.4.0.69200-Release. Executable SHA256
`3bf526349d0b2702f3de3d12433c7d223fa41e469f41b24b28523cb7c736332d`.
This is the frozen diagnostic engine, not a new optimization. All19 preserved
RenderMetal override pairs were supplied, with GroupSources,
GroupSourceStability, ShadowViewOverlap and MainLightingRenderPass TRUE.
Runtime census and sampled native worker stacks corroborate activation.

Overview records180.691688s, 17:17:32.040–17:20:32.731UTC, from before login.
The native `quiet_world_started` marker is17:17:46.647911UTC. Native phases
below use that marker; GPU aggregate phases use the first exact-world iteration
end, 30.2ms later, and only wholly contained buckets. Main target3456×2168,
quality6, unchanged throughout the measured world interval.

## What the recording establishes

| World age | Eligible raw cadence | Mean raw period | Main sampled CPU / wall | Configured main probes at phase end |
|---|---:|---:|---:|---:|
| 0–10s | 24.16FPS | 41.39ms | 38.34% of one core | 1 |
| 10–60s | 18.49FPS | 54.08ms | 32.56% | 22 |
| 60–120s | 14.47FPS | 69.12ms | 25.53% | 39 |

Raw cadence excludes startup, focus/window/teleport invalidity and boundary
crossings. It is independent of the rolling viewer FPS display. The late raw
cadence including focus losses is14.45FPS, so excluding those periods does not
remove the slow state. CPU sampling measures Running time, not blocked or
runnable wall time; its falling utilization alone is not a GPU-bound proof.

The independent displayed-frame aggregate means are41.59/53.96/68.88ms.
Their GPU-associated envelopes grow66.12→95.96→131.54ms, while GPU-end-to-display
tail remains4.22→4.33→3.72ms. These are overlapping associated-work envelopes,
not isolated GPU execution, occupancy or additive per-frame costs.

Native command wait records, GPU timestamps and source admission establish
completion backpressure. Source `almetalworld.cpp:2498–2525` waits for the older
main-frame group's submitted commands when the two-frame limit is full.
`almetaldevice.mm:381–405` brackets the actual Metal wait. The corrected group
contract remains active; this is not evidence that its former same-frame
three-transactions/two-command mistake has returned. Raising queue depth is
not a demonstrated solution to slower completion.

Exact intersection of main-thread wait unions with the same eligible frame
periods provides the observed wall-time accounting:

| Per eligible frame | Early0–10s | Late60–120s | Change |
|---|---:|---:|---:|
| Total iteration period | 41.387ms | 69.123ms | +27.736ms |
| Recorded main command waits | 20.674ms | 49.068ms | +28.394ms |
| Remaining iteration wall time | 20.712ms | 20.055ms | -0.658ms |
| Wait after target's reported GPU end, included above | 0.337ms | 0.433ms | +0.096ms |

All4384 paired main waits in the retained stream have a surviving target GPU
interval. Almost all target lighting/post commands exactly two display frames
older than the caller. Their wait occurs during the target's reported GPU
envelope, rather than before its GPU start. The waited lighting/post command
callbacks remain near0.2ms after GPU end. Other G-buffer completion-observer
notifications do become delayed, but these are not the measured eligible wait
targets; no claim that every native callback is timely is warranted.

The matched commit-cohort command-family GPU envelopes also grow: main
light-map/deferred lighting33.82→50.87ms, main opaque/optical post47.93→73.09ms.
Main G-buffer/ordered sky remains14.10→14.35ms. These overlap each other and
other queues and must not be summed into a frame budget. They narrow the capture
targets; they do not identify an individual shader's cost. Exact matched
cohorts/intersections and five-second time series are in
`analysis-native/deep-summary.json` and `world-time-series.csv`.

## What changes while the view appears still

The configured main reflection-probe pack grows0→1 in the first10s,1→22 over
10–60s, and22→39 over60–120s. The ordinary probe view's pack grows8→45→82.
In contrast, the main point-light list remains9, with summed bounded rectangle
area40,259,010 pixels; projectors remain39, two shadowed, with summed area
203,870,474 pixels. Main SSR is ready after its initial bootstrap and retains
25 configured trace iterations/four samples. These are configured inputs,
not measured shader invocations or actual samples per pixel.

Source explains why probe warm-up can increase GPU work without a graphics
setting change. `llreflectionmapmanager.cpp:1896–1903` admits completed,
valid-cube probes under current-view/occlusion policy. Its config helper copies
`mProbeData.refmapCount` into shader config[0]. The native base-lighting walk
at `almetal_builtin.metal:6396–6467` tests candidate volumes and samples accepted
probes, with up to32 accepted taps and64 neighbor candidates; rejected candidates still
perform checks. Original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`
class3/deferred/reflectionProbeF.glsl:703–746 has the corresponding behavior.
Thirty-nine configured probes does not mean39 taps per pixel or linear cost.
Actual overlap, neighbor lists, material flags and texture behavior require
the GPU capture.

Resource loading also changes the workload. Device current allocation rises
from1.503GB at the first world sample to~8.826GB by60s, then stays near8.83GB;
tracked texture bytes settle near6.915GB. Object count settles around8,020.
These allocations do not prove exhaustion of this64GiB unified-memory system,
residency faults or hardware cache misses. The publication source cache hits
its distinct3.892314112GB logical allowance around world20s and continues to
reject/evict entries. Rejection/retention counters are attempts, not unique
assets or physical allocation traffic. This shared-resource policy remains
relevant, but neither a larger cap nor a cache-cause claim is justified here.

Texture decoding cannot explain the sustained late slowdown by CPU work alone:
the six decoder threads contribute52.951 sampledCPU-s in world10–60s but only
0.017CPU-s in60–120s. Native shadow preparation is observed on five non-main
threads, with5,587 reported jobs/no failure by17:20:31. The main thread runs
almost entirely on performance cores. This does not certify the full threading
architecture; it identifies the current critical-path question as completion
and its changing GPU/resource workload, rather than merely too few CPU workers.

## Profiling overhead, warnings and limits

No MetalTools/GPUTools/Backtrace/MainThreadChecker frame appears in any sampled
main-thread stack in this run, versus the prior manual Xcode trace's substantial
validation/helper work. Sampled native telemetry contributes0.088mainCPU-s and
1.358writerCPU-s over the recording; those observations are not an OFF control
or a bound on all instrumentation overhead. Reproducing the decline with those
Xcode helpers absent rules out their necessity for this decline, without
quantifying a counterfactual FPS saving.

Thermal state is Nominal in all native samples covering world0–120s. No GPU MHz,
power, bandwidth or limiter counters were acquired, so clock-related effects
are not excluded by that status alone.

The full retained log contains116 warnings/38 source callsites, three libpng
stderr notices and zero ERROR/FATAL. Startup/missing assets/capabilities and
hero-probe warm-up messages require their individual functional interpretation;
there is no demonstrated warning storm causing the quiet decline. Safe records,
source mapping, runtime controls and resource census are in `analysis-log/`.

Two diagnostic limitations are recorded separately from the renderer symptom:

- PPD-325: real recorder/export exit0, but the collector returns1 because a
  5.016658s gap between positive-frame bucket starts crosses its5s gate. The
  intervening explicit zero-associated-frame rows are retained and coincide
  with loss of application focus. Native iterations continue. This is not lost
  GPU recording, nor a measurement that the viewer ran at zero FPS.
- PPD-326: native JSONL reaches64MiB after163.85s from its header/~149s of world.
  It contains3430 raw iterations, no malformed rows,203 contention drops,
  141 budget drops, no full-ring/I/O errors and20 pending committed commands.
  World0–120s is covered, but individual missing events and the tail cannot be
  manufactured. Matched observations remain useful; full-stream qualification
  and a complete loss-free command critical path are not claimed.

## Remaining causal boundary

The clean run establishes the decline and growing completion backpressure,
and identifies reflection-probe warm-up alongside resource/cache changes as
specific measured workload evolution. It does not yet establish which encoder,
shader, bandwidth/resource dependency or combination makes GPU work slower.

The next useful runtime evidence is the prepared bounded early and late device
`.gputrace` pair, with API/shader validation off and ordinary worker overlap
preserved. Compare the base-lighting probe traversal/taps, opaque/water/probe
sampling, static point/projector costs, shadows and resource hazards/counters.
Live capture-free timings remain the cadence control; replay timings and
capture overhead require separate interpretation. Then test the identified
cause in a contained matched check. No speculative renderer optimization,
graphics-quality reduction, new viewer run or push is selected by this report.
