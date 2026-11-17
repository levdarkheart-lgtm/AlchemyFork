# Home checkpoint69228 — cold boot and warm login

Owner returned2026-11-17 and explicitly requested the prepared checkpoint.
Frozen source840bae8c33/build69228 was launched by its exact staged executable,
SHA-256d6045737fd0231df26be6e8d2099e1c40cbdcf642be5acbb9dff410f1f22ba9a.
The previous shader [qualification](home-texture-selector-20261117.md) and all
private inputs/candidates remain preserved. No renderer code, graphics setting,
lighting or camera was changed by the agent during these runs. Broad
architecture acceptance remains partial and its rollout paused.

## Intended captures and owner observations

| Run | Exact viewer | Recorder interval/context | Owner reports |
| --- | --- | --- | --- |
| Alchemy-home-apd4_a6o |PID64472,69228 |Cold-boot login, requested07:19:09UTC; stop07:22:19; saved/collector0 |7→20→16FPS |
| Alchemy-home-7rbc49pc |PID68936,69228 |Warm relaunch including login, requested07:29:12; stop07:32:20; saved/collector0 |30→20→19→18→17FPS |

LOGIN cues were issued only after the recorder's readiness event. Both intended
recorders/exporters return0 and independently retain about180s of target CPU
and GPU data. Source/executable identities match. Full phase metrics are being
aligned against actual first-world/native markers; requested duration and
recorder success are not substituted for coverage verification.

The owner reports that the laptop shut down overnight, then booted and was
plugged in that morning. Keep that as cold-start context, not proof which
startup cost caused7FPS. A07:25:20power snapshot shows AC/charging12%; it is
after the cold trace and does not establish continuous GPU clocks or power.
The saved pmset output has no recorded thermal/performance warning.

The second login is explicitly **Shared environment**, per the owner. Do not
call it Midday or retroactively assign the cold run's environment identity.
Owner message receipt anchors and relative order are saved, but their exact
display-frame times are unknown. "More slowly trickling down" is an owner
observation pending measured phase slopes. No matched whole-viewer speedup or
resolution of the decline is claimed yet.

## Preserved wrong-scope attempt

After the first capture the owner requested another capture because of the
cold boot. The agent initially interpreted that as a warm settled interval in
the same process. `/Users/dekstroza/Alchemy-home-apd4_a6o/warm-overview` began
07:26:20, but the owner correctly required repeating **login**, where the FPS
change occurs. The agent acknowledged the mistake and requested a graceful
stop through `quiet_capture.py mark --phase stop`. The owner closed the viewer.
The recorder finalized0, but collection returns1 after target departure;
the output is partial/interrupted and **not** the requested warm-login test.

All output and failure receipts remain. No recorder was restarted merely on a
poll timeout. Once this recorder was terminal, the exact same verified app was
relaunched with fresh native and Overview outputs; that second intended login
capture is the7rbc49pc row above. PPD-339 retains the workflow correction.

## Diagnostic coverage gap

PPD-338: the new selected-PSO census remains guarded by the **legacy command
timing** flag in world.cpp21084 and pipeline.cpp3757. Qualified native async
timing does not enable that flag. Thus these normal-batching runs do not supply
actual constants3/4 hit coverage through that census. Missing rows are not
evidence that the optimized pipeline was disabled. Source, final native code
and actual renderer selection tests remain valid but do not replace live hit
coverage. Do not enable the legacy timing path in the middle of a performance
run, because its scheduling differs from this qualified normal-rendering path.

No diagnostic repair was made in either recorded viewer. A future repair must
make opt-in successful-descriptor telemetry available without changing normal
batching/overlap, retain default-off cost, and qualify its own overhead.

Private analysis root:
`artifacts/metal/selector-home-checkpoint-69228-20261117T0733Z`.
Run-local owner observations, launch/record commands, coverage, exports, hashes
and unsuccessful settled attempt are retained at their original paths. Native
streams include post-capture events; analysis must freeze a bounded input and
filter to the actual recorded intervals rather than pooling later events.

## Completed result: the warm decline persists

Both intended captures have independently qualified CPU/GPU coverage and
immutable native input clones. First-world markers are07:19:38.438233UTC cold
and07:29:43.244240UTC warm. Their common recorded world spans end154.75s and
151.54s after those markers. The native cadence below uses actual consecutive
eligible app-iteration periods; waits are unions clipped to those same periods,
not summed overlapping intervals. Independent Overview cadence agrees closely.

| Warm Shared world phase | Eligible periods | FPS | Mean frame ms | Recorded main wait ms/frame | All other elapsed ms/frame |
| --- | ---: | ---: | ---: | ---: | ---: |
|0–10s |275 |27.701 |36.099 |14.770 |21.329 |
|10–60s |995 |19.919 |50.204 |27.460 |22.744 |
|60–120s |964 |16.083 |62.178 |40.971 |21.207 |
|120–151.54s |490 |15.627 |63.993 |43.482 |20.511 |

From the first phase to60–120s, mean frame time grows26.079ms and recorded
main wait grows26.201ms. The remainder stays near21ms. That remainder includes
CPU work, other waits and unrecorded elapsed time; it is not pure CPU execution.
All1926late eligible wait-begins target the exact command envelope already
running on the GPU, with caller/owner display-frame delta2. Time after that
command's GPU end is only0.441ms/frame. Growing callback/wakeup latency and a
fully busy main CPU core do not explain this late decline.

Overview main running CPU falls47.1%→38.2%→30.2%→28.3%wall across these phases;
the main thread predominantly runs on performance cores. Warm image decoding
is0.080CPU-s over60–120s and0.001over the final31.54s. As the owner points out,
cold decoding is expected startup work; continuing decode is not supported as
the cause of the **warm** late decline.

## GPU workload grows across owners

Warm configured main probes rise0/1→24at60s→41at120s→49at the end. Ordinary
probe-view mean configured counts rise4.64→30.69→70.27→100.63. These operands
can increase candidate traversal and accepted SH work, but they are not actual
taps per pixel. Native command envelopes grow across several owners, early
0–10 versus60–120s: main Gbuffer12.09→19.41ms, lighting28.97→44.74ms,
opaque/optical-post39.67→64.91ms and ordinary base9.00→18.08ms. These envelopes
include dependencies/internal gaps and can overlap; they are neither additive
exclusive pass costs nor hardware occupancy measurements.

Thus the evidence supports continued GPU-completion pressure with growing
loaded scene/probe work, not a complete attribution to one helper or a generic
CPU threading shortage. The source-level selector repair remains qualified;
this run does not prove its live hit frequency or net FPS benefit. Filling a
CPU wait with work cannot by itself shorten the GPU work being awaited, and
additional preparation must respect the still-partial immutable ownership and
admission architecture.

Light count/scissor growth is not the explanation for these runs: main points
stay9 with40,259,010summed rectangle pixels; projectors stay39,2shadowed, with
203,870,474pixels. These match the earlierl9/qy rectangles. The51-projector
context from69224 does not apply here. Both runs remain3456×2168 with SSR25×4.
The final32whitelisted graphics values and19launch overrides match each other
and69224, but material/texture/probe state and lighting are not fully matched.
Initial environment ambiance differs1.23335cold/1.00631warm; only the warm
environment identity is owner-confirmed Shared.

Both traces have one Nominal OS thermal interval spanning their entire180.712s
and180.622s. No thermal-pressure transition appears. Actual GPU frequency,
memory bandwidth, package power/residency and charger limits remain unmeasured;
Nominal does not prove fixed clocks or exclude UMA contention. Physical
memory-low flags remain0. Logical native-source cache byte denials begin in a
logged sample around cold+91s versus warm+23s at the existing3.892GBlogical
cap; this is another retained workload/resource-pressure boundary, not proof
of system unified-memory exhaustion or the entire FPS cause.

## Cold startup and comparability

The cold capture has6–8FPS for much of world0–120s, with substantial expected
loading: ImageDecode contributes103.56CPU-s over10–60 and69.15over60–120s,
then0.080over120–154.75s as cadence recovers to17.76FPS. Main configured probes
stay1through60s despite106.29ms/frame recorded wait, so growing main probe
count alone cannot explain that cold phase. Texture creation/residency fills
much sooner on the relogin: cold create_pending reaches0by+123s, warm by+60s;
GPU image accounting reaches roughly6.9GBbycold+115 versus warm+64s. Those
observations support different loading states, not a controlled isolation of
OS cache, shader compilation, upload contention or power contributions.

Exact warm80–120 cadence is15.781890FPS from630eligible periods covering
39.919173s; configured main probes progress31→41. Old69224Region80–120 was
15.336FPS. This is **context only**, not an accepted improvement: current
probe/loaded state, environment/boot state and scene work differ. Exact80–120
wait union was not retained; do not substitute the60–120wait as that window.

## Logs, warnings and evidence limits

Full process logs contain138cold/118warm structured warnings across38sites
each, with0ERROR/FATAL. All37previous69224sites recur; the new site is four
startup cache body/index-size mismatches (PPD-340), detected by original-
equivalent cache validation. Existing code removes those entries; no cache
wipe or repair was performed. Their FPS contribution is unproved. HTTP
timeouts number11in each world0–180window; larger full-log totals have different
post-capture tails and are not comparable rates. Hero warnings concentrate
before world/first10s, with hero_ready0; they do not prove eventual recovery or
a new selector failure. Both stderr files have the same3libpng notices.
Both viewers follow normal requestQuit/disconnect/Goodbye, no crash.

The cold recorder has339DWARF cursor warnings after its time-limit-ended line;
the warm recorder has none. Those warnings constrain symbol attribution,
not demonstrate that they caused live FPS loss. CPU leaves retain96.4%/96.5%
named resolution. No sampled exclusive leaf is in MetalTools/GPUTools/checker/
capture images; async telemetry/sampling overhead is still not zero or measured
away by that absence.

Native footers retain314cold/203warm contention drops and sequence gaps;
classification is **partial**. No full/budget/IO drop or pending command remains.
Every included phase wait-begin has a known exact target GPU state; one global
unmatched closed wait per full prefix is retained without assigning it an
invented location. Reported wait unions are observed lower bounds. Wrong-scope
and later native tails are excluded entirely from performance aggregation.

Private native findings, period/bin CSVs, clock/input receipts and frozen
prefixes are in `native-analysis/`; independent Overview qualification, thermal
tables and phase joins are in `overview-analysis/`; complete sanitized warning/
settings/source audits are in `log-audit/` below the analysis root. Initial
private query/parser/export-check corrections remain in those histories.

## Next engineering boundary

The Home FPS problem is not resolved. The next attribution is current early/
late GPU frame evidence: inspect actual selected main/probe pipelines, shader
limiters and resource/dependency costs in this exact app. The existing verified
frame-capture launcher can supply that diagnostic without first changing shader
math; its capture overhead must not become FPS evidence. Independently repair
PPD-338's normal-mode telemetry gap without enabling the legacy timing/schedule
path, so future low-overhead runs can establish actual specialization coverage.
Preserve all scene/probe/light/material behavior and test optimizations against
those measured workloads. Further groups follow the owner's checkpoint
discussion; no additional renderer change, launch, capture, push or whole-
architecture completion is claimed by this report.
