# Settled native68564 runtime and admission decisions

Observed2026-11-09 on M1Max/64GiB, macOS26.5.2/Xcode26.6. Source
`494cbaab582cd189ce61b4d8c68db4056f280df8`, Release26.4.0.68564. This commit
adds default-off readiness/material diagnostics; its compiled metallib is
identical to68563. Production shiny optimization remainsd128812999.

## Build and controlled measurement

`tools/metal/doctor.sh`, configure/build through `tools/metal/build.sh`,
`tools/metal/verify_app.sh` and deep strict signature verification exit0.
349Mach-O objects have ARM64-only slices. App is
build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app.
Executable SHA256bd81cddb5f249544d58efe6f1d1124f9f021a14a623a90ea439359e5b721d867;
metallib8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943.
Native CPU2873assertions/27cases and fullMetal390463/89 pass BOTH Release and
RelWithDebInfo; Metal tests use API/GPU validation on the physical device.

Runtime validation/capture were unset, shader source recordsOFF. Fullscreen
before login, fixed owner-selected location/numericPreset123/Midday/Ultra6,
3456x2168.43livecontrol assertions pass.300-second/120-frame warmup runs
alongside a new120-stable-frame readiness requirement: all256 assigned probes
ever-ready, faded and published as current-generation SH/radiance pairs, no
pending membership/reset changes. Routine refresh remains enabled. Actual
initialization took about17minutes; the previous five-minute samples did not
establish settled state. No external scene change is assumed.

Raw CSV artifacts/metal/private-68564-ready-20261109-01.csv and matching
provenance.jsonl; QA run artifacts/metal/qa-runs/candidate-68564-ready-20261109-01.
CSV SHA2561ce987a19708736cae268cf4d4564f06930d856acfd0e9a5993d105bfd07b195;
JSONL64581c4196843b49aa40785f4bfec0f9dfddb07f9a1810e4732ce83d9b995bb8.

| Metric | Observed |
| --- | --- |
| Consecutive frames |9266–9565,300exactnative/zeroGL |
| Mean frame time |107.87338125ms |
| p50 / p95 |106.4846455 /118.416ms |
| Throughput,1000/mean |9.270127518FPS |
| Mean encoded draws |53867.016667 |
| Readiness through sample |256ready/faded,120stable,no membership loss |

PID52070 exited gracefully07:55:45UTC; temporary environment/persistence restored
07:56:16. Startupplan records cleanup. The20FPS goal remains active.

## Separate profile attribution

CPU30s recording/finalization completed before the separate GPU30s recording.
GPU recorder actually exited0 at07:54:46 before exports/analysis/build work.
All seven exports and parsers complete0 in
/private/tmp/alchemy-metal-68564-metrics-20261109. These are separate perturbed
windows, not timestamps from the unprofiled CSV. Do not divide CPU samples by
present counts from another capture. Readiness was not sampled in every later
profile frame after the collector disarmed.

Main-thread Running28.105s over30.723783s is91.476% of ONEcore. Disjoint actors:
hero50.09%,ordinary probes6.02%,remaining43.89%. Capture/G-buffer/shadow-finish
union56.30% of Running. Inclusive shadow32.28%,cull17.38%,G-buffer16.22%,native
draw14.36%,texture/sampler6.47%,arena/data6.02% overlap. This is real traversal,
capture and driver encoding, with no evidence of one dominant waiting call.
Named copy attribution changed among optimized/unknown frames; arena/data union
stayed essentially unchanged. Named memmove alone does not prove a regression.

GPU viewer-only Active interval union106.527463ms/present,81.680299% tracewall;
device-wide84.284% includes other processes. Nominalthermal,4.389–4.470GiB.
Encoder CPU intervals45.692ms/present,29.506submissions/present; encoder intervals
exclude scene preparation outside encoders. Selected per-label GPU unions:

| Label family | ms/present, overlapping |
| --- | --- |
| Main base |17.463 |
| Main haze, including later draws |17.025 |
| Hero above-water surfaces, aggregate |21.438 |
| Hero haze |8.339 |
| Hero/nonhero shadow encoders |6.198 /8.123 |
| Main initial opaque post |0.308 |

These unions cannot be summed into a frame budget. Above-water includes later
alpha and does not isolate water; no shader-instruction attribution is available.
The original~19ms mainopaque anomaly remains absent after shiny optimization.

FINDINGS.md SHA256065731dae6349109538fa612eecf0beab282f577a654554148d6f622b12d3f87;
analysis-summary.json cfec4ef671b69a8be30c0e64fce4171f0e13bb39ffaba71c7c2941dca513709c.

## Coverage and rejected candidate

All13V2 post-timing sparse64x40 packets complete0, unique cube slots. Main
depth3456x2168,97probes,hero config[0,1,9,0]. Of2453geometry samples,2145use
gloss and28have exact-full glossy hero weight;21use legacy environment and0have
full legacy hero weight. Thus fullgloss is1.14% of geometry/1.31% ofgloss-used.
Hero sixfaces:1748geometry/1680gloss-used/15legacy-used,allfullhero0. Ordinary
sixfaces:7760geometry/3402gloss-used,allfullhero0. Main/hero packets are oneframe
after sampling; ordinary packets0–5frames later and may refer to different probes.
Sparse compute proxy is not whole-image overdraw or raster-boundary parity proof.

The private base-mask candidate passed212packed+53ambient comparisons and full
hero synthetic cases improved14.95–26.65%; off/partial cases cost0.34–1.50%.
Actual coverage does not justify integration: DEFAULTREJECTED/deferred. No
production base shader was changed. Reader/report stay in private/tmp paths;
source binding is external, not intrinsic packet metadata.

## Comparison limits and next action

68563 observed9.028899FPS but its probe population was still growing. The28
shared asserted controls/Midday match; camera differs only0.038mm vertically,
projection/basis match.68564 adds15quality assertions. This is not an isolated
renderer A/B; do not attribute9.029→9.270 to new performance code.

External original67611 calibration is ongoing with43controls, matching dynamic
hero detail and actual retained attachment dimensions. Original allocation can
silentlyhalve targets, so window dimensions alone are insufficient. Bias is
telemetry, not proof of equal textureLOD. Full pixel/feature parity remains open.

Installed QAhelper regression fix normalizes int0/1 versus bool only for controls
declaredBoolean by immutable hashed session settings; other numeric comparisons
stay strict. Backups/tests under /private/tmp/alchemy-qa-boolean-normalization-20261109;
regressions pass, raw config/session/observations unchanged. Both43control gates
pass. Initial CUAgetApp blocked~17minutes at the original login screen; there
was no frame measurement during that stall.

Next private candidate predecodes exact packed-half SH coefficients once, while
preserving traversal/arithmetic and matching publication generations. This can
increase load/cache pressure, so no speedup is assumed. Compile/GPU work waits
until the live original calibration quiet window ends. Safe scene-preparation
reuse is independently source-audited.20FPS and all release gates remain open.
