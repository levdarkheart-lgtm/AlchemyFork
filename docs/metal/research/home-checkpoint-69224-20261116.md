# Home 69224 checkpoint: the decline remains

Both owner-authorized Home runs used the separately staged source
`819f98d71ca479441f635fb619e8dccb5d26e6a8`, build `26.4.0.69224`.
Both recorders completed and both viewers were closed. The High2 shadow
specialization was present in the launched executable and shader library;
it was not confined to the earlier Xcode replay experiment. The Home decline
remains PPD-319, and no whole-viewer performance gain is accepted.

Private evidence is preserved under
`artifacts/metal/home-checkpoint-69224-20261116T202910Z` in the original
repository. The original architecture rollout stays paused for discussion.
No new implementation, recording, viewer launch or push follows this checkpoint.

## What the change did and did not establish

The selected change prebuilds 39 High2 pipelines and selects them from the exact
shadow constants subsequently bound. The owner runs report shadow quality 2.
Executable and metallib hashes match the staged, tested candidate. This proves
the implementation was shipped in the tested app; it does not prove selection
for every draw. Avatar, custom blend, water and impostor paths retain generic
filtering. Matching fused tile light-map producers are specialized.

The earlier fixed Home capture source A/B/A was142.99→119.21→140.90ms.
It demonstrates a quality-preserving GPU mechanism, not live FPS. Its compiler
observations show material-alpha spills32→0bytes and PBR-alpha80→32bytes, while
the dominant simple-alpha shader still spills304bytes. The new Overview runs
do not contain fresh per-shader register/spill counters. Scalar accumulator
variants removed five runtime array indices in intermediate code, but did not
eliminate accumulation or establish a spill repair. They gave no useful measured
GPU gain and were rejected, so they were absent from this viewer. Compiler
qualification and all rejected variants remain in the linked
[integration evidence](quiet-home-shadow-specialization-20261116.md).

## Where the new frame time goes

These are eligible cadence and recorded main-thread wait unions. The remainder
is frame wall time outside those waits, not a CPU execution measurement.

| Run and window after world start | FPS | Frame period | Main GPU-completion waits | Remainder |
| --- | ---: | ---: | ---: | ---: |
| Region environment,0–10s |25.51 |39.20ms |15.57ms |23.63ms |
| Region environment,60–120s |15.34 |65.21ms |45.17ms |20.04ms |
| Requested Midday run,0–10s; initial lighting unconfirmed |26.13 |38.27ms |16.66ms |21.61ms |
| Requested Midday run,60–120s; partly before selection receipt |15.48 |64.61ms |46.04ms |18.57ms |
| Confirmed Midday,80–120s |15.18 |65.89ms |47.11ms |18.78ms |
| Confirmed Midday,120–150s |15.06 |66.40ms |48.66ms |17.74ms |

The Midday confirmation at world78.123s is the owner's report receipt, not
the exact selection frame. Both confirmed windows are within retained Overview.
Wait targets remain two display frames older. Delivery after the target GPU
end contributes under0.43ms/frame; late wait before GPU start is zero. This
supports GPU completion backpressure rather than a growing post-completion
CPU wake-up delay. Overlapping GPU command envelopes are not isolated pass
costs, hardware occupancy or additive frame costs.

Configured main probe populations grow from one to41/42 by120seconds, while
per-frame main light counts and rectangle sums have settled.
Configured populations are candidate bounds, not accepted samples per pixel.
Together with the prior fixed-capture probe intervention, this supports a
material probe-dependent shading cost. It does not attribute the entire
decline to one helper, spilling or a CPU scheduling defect.

The source explains both sides of that ramp. In
`llreflectionmapmanager.cpp:2678`, a same-layout reset retains the completed
default but retires nondefault publication epochs until rebuilt. In
`almetal_builtin.metal:6375`, visible fragments traverse spatially filtered probe
candidates and accumulate ambient irradiance and applicable reflection lobes,
with a32accepted-probe limit. More published candidates can increase this work;
the configured count does not mean every pixel samples every probe. Rendering
probe updates is additional GPU work, distinct from sampling them in the scene.

The late Region native-only tail supplies another observation: after the
environment change, configured main probes drop56→1 and cadence returns from
about14.78 to25.27FPS; by world310–320s, probes regrow10→16 and cadence is18.10FPS.
The owner corrected an initial answer and confirmed changing the environment
before closing. The reset log is20:16:25UTC; the exact preset and action frame
are unconfirmed. This event is outside Overview and cannot establish matching
CPU/hardware-counter attribution. It is consistent with the replay mechanism.

## Comparisons and remaining work

The new settled scene has51projectors versus39 in the older capture. Projector
rectangle pixels rise10.56%; point-plus-projector rectangle pixels rise8.56%.
Those are workload proxies, not exact shaded pixels or GPU cost. Counts and
rectangles are stable during the new within-run decline, but the additional
lights prevent an old/new FPS comparison from measuring the fix's gain.

Memory and source-cache work remain relevant: late tracked device allocations
are about9.3–9.4GB, source residency churns near its3.892GB logical cap, and
vertex partial uploads continue around16MB/s. Late immediate texture commits
are sparse and the sampled thermal metric stays nominal. Logical cache bytes
overlap resource leases and must not be added to device allocation. These are
remaining costs, not proved causes or new specialization regressions.

The next engineering target is the expensive probe-dependent fragment work in
the actual transparent and base-lighting shaders, including the remaining
simple-alpha spill. Background probe-update work also grows and remains coupled
to main shading. Qualify a specific change on the saved scene and preserve
all probe/material/light behavior; do not infer a remedy from register count
alone. Broad CPU-threading or queue-depth changes are not justified by this
Home decline. Further implementation follows the owner findings discussion.

## Logs and evidence qualification

Region/Midday logs contain121/122structured warnings, zero ERROR/FATAL, and
three PNG stderr warnings each. All37 union warning sites are known from69200;
no new renderer error category appears. Hero-publication warnings occur during
startup. Five texture and ten material failures per run are real content
delivery problems; their private ID sets match between runs. HTTP timeouts,
inherited settings/appearance warnings and PNG handling remain separately
mapped, without proved causation of the late decline. Absence of new errors
does not establish complete feature or visual parity. Full warning counts,
phases, source anchors and qualifications are in private `analysis-logs/report.md`
and `warning-source-map.json`.

Both input copies are exact and stable. Both retain full world0–120 and the
confirmed Midday120–150 window, with no native iteration-ID gaps or byte-budget
cutoff. Native retention is below256MiB and both footers report requested
shutdown and zero actual pending submissions. Sparse contention losses remain:
293/251events over the full sessions, so event completeness is partial. Running
CPU samples are not complete blocked-thread states; Overview GPU families are
not per-shader occupancy/spill measurements. Longer native-only tails stay
separate. See private `ingest-coverage/findings.md`, `analysis-native/`, and
`analysis-logs/` for exact hashes, phase boundaries and source-mapped findings.
