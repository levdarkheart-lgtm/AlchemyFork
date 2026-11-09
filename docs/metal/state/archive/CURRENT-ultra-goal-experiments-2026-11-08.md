# Ultra goal — isolated architecture experiments, 2026-11-08

These are M1 Max experiments against renderer6c694df237, not viewer FPS
measurements or authorization to change original quality. Viewer closed;
timed windows did not overlap parent builds, captures or other GPU tests.

## Layered static shadow rendering: defer general integration

Local report `/private/tmp/alchemy-metal-layered-shadow-20261108.eywPG8/comparison.json`,
SHA-256 `dba011692a0b4606a183b1c273c2d84ac268c46a59f1e7f6aaf8cfd1494283f3`.
Standalone Objective-C++ only; production files/shaders were not changed.

- Actual6/12-layer R32Float color plus Depth32Float arrays work on M1 Max,
  including memoryless depth. Vertex `render_target_array_index` requires the
  explicit triangle input topology on this device.
- Membership uses the actual per-view lists and compact view IDs; GPU instances
  equal admitted view instances. No invisible dummy views or omitted casters.
-112 API/GPU-validation state cases and56 realistic per-slice bitwise R32 output
  comparisons pass across separate/inline view IDs and private/memoryless depth.
-11 paired timed samples/mode/case;1000/3000 visible meshes/view, overlap1/2/6/12,
  heterogeneous cached geometry and index formats. CPU includes union construction,
  constant packing and native encoding. Whole-command-buffer GPU duration is used.
  Original per-view frustum culling and full MVP generation remain outside timing.

Selected medians for the best inline-ID/memoryless variant,3000 meshes/view:

| Views / overlap | Sequential CPU ms | Layered CPU ms | Sequential GPU ms | Layered GPU ms |
| --- | ---: | ---: | ---: | ---: |
|6 /6 |1.668 |0.757 |0.910 |0.974 |
|12 /12 |3.333 |1.261 |1.694 |2.047 |
|12 /6 |3.297 |1.586 |1.717 |2.122 |
|12 /1 |3.326 |4.732 |1.810 |2.934 |

The high-overlap CPU saving does not establish a frame-time win, and the
low-overlap regression prevents adopting this as a general replacement.
Production integration would also need twelve retained hero slices and frozen
per-face receiver matrices. A static-only memoryless pass cannot end and then
resume other caster families while preserving its discarded depth. Retaining
private depth or routing every caster family through one layered pass requires
separate implementation and measurement. No whole-viewer gain is claimed.

## Timestamp capability and encoder attribution

Local probe `/private/tmp/alchemy-metal-counter-capability-20261108.log`,
SHA-256 `898230fface3595c4888b41c58256677bcd0a2c128084824c64aa9e5f8f4f421`.
M1 Max reports stage-boundary sampling, but no draw/dispatch/tile/blit-boundary
sampling. The exposed counter set is timestamp/GPU timestamp; no statistics or
utilization set is available through this API. Four shared samples allocate.
Do not implement unsupported per-draw timestamp sampling. Generic timestamp
conversion should use the device's CPU/GPU calibration API.

The existing atmospheric-haze encoder continues encoding later alpha/glow and
other draws until the next boundary. Its approximately17ms label is an encoder
aggregate, not isolated haze fragment duration. The new CPU census records the
actual family and segment sequence; an isolated shader timing would require an
explicit diagnostic pass split on this GPU.

## Apple guidance consulted

[Argument buffers](https://developer.apple.com/documentation/metal/improving-cpu-performance-by-using-argument-buffers)
can reduce resource encoding and driver tracking, provided resource lifetime and
residency remain explicit. This is a future measured candidate, not an assumed
replacement for the already implemented encoder binding caches.

[Deferred lighting on Apple GPUs](https://developer.apple.com/documentation/metal/rendering-a-scene-with-deferred-lighting-in-c%2B%2B)
can keep intermediate attachments in tile memory when the pass graph permits.
Current sampled cross-pass G-buffer/depth resources cannot simply become
memoryless; attachment consumers and pass lifetimes must change together.

[GPU timestamps](https://developer.apple.com/documentation/metal/converting-gpu-timestamps-into-cpu-time)
need calibration for general CPU/GPU timeline comparisons. The native capability
probe, rather than SDK API availability alone, determines supported boundaries.

No product change or20 FPS claim follows from these architectural possibilities.

## Early source admission: accept for product validation

Seven counterbalanced CPU repetitions,10,000 logical sources each, using real
native buffers created outside timing. Compare old record construction plus
modeled static backend validation with early admission plus physical validation.
This does not invoke the complete viewer capture or native encoding path.

| Case | Old median us | New median us |
| --- | ---: | ---: |
|0% contiguous joins |311.500 |306.125 |
|25% joins |332.792 |242.125 |
|70% joins |309.875 |152.958 |
|95% joins |308.208 |125.041 |
| Repeated offsets |310.583 |316.041 |
| Alternating cull |306.958 |299.000 |
| Alternating MVP |312.750 |323.125 |

At70% joins, the measured mechanism saves0.157ms/10k sources. Nonmerge medians
range−2.6% to+3.3% with overlapping raw ranges; this is not a universal no-regression
or viewer FPS proof. Completion23:51:13UTC. Raw
`/private/tmp/alchemy-shadow-capture-admission-bench-20261108.txt` SHA-256
`a64d3d68f58d4553973bb29972db7d692290c3272346bc26da7e17c7ee44bb68`;
JSON metadata SHA-256
`e6930d7ae3722e2a17acc82d2db556eefa6d1be7238977ab934a83200295dfb2`.

## Named probe accumulators: reject

The production class-3 base shader uses dynamically indexed two-family sums.
An ignored standalone candidate replaced those arrays with named fields/static
branches, retaining traversal and sampling arithmetic. Full production shader,
all13 resources, real BRDF/SH/radiance/hero inputs, default/automatic/manual/
box/neighbor/classic and PBR/legacy/mixed fixtures were exercised.

All eight64x64 API/GPU-validation fixtures are bitwise equal. Larger outputs
at1024x1024/3456x2168 differ by at most one RGBA16F ULP, up to275 components
among29,970,432; no nonfinite values. The candidate is slower in expensive cases
on two paired runs. Selected repeat medians at3456x2168:

| Case | Indexed ms | Named ms | Change |
| --- | ---: | ---: | ---: |
| Mixed32 |27.260 |27.970 |+2.61% |
| Neighbors256 |28.852 |29.613 |+2.64% |
| Boxes256 |45.153 |46.203 |+2.32% |
| Classic PBR |6.032 |6.339 |+5.09% |

No register-spill claim is established. No product shader edit is justified.
GPU work ended23:54:36UTC; thermal state Nominal. Prototype-only BRDF setup
initially lacked ShaderWrite usage and failed validation; corrected with the
failed log retained. Product descriptors were not changed.
Report `/private/tmp/alchemy-metal-base-accumulator-20261108.c2Vvep/RESULT.md`;
first comparison JSON SHA-256
`e7e8c1f40755b46cda0c8659253c036383e60a5c6607065a2ab48296428969f9`,
repeat SHA-256
`015bbb6bdbaebb91716dada80f2abb56e7e5a4e88239e41d8df4bb458a3aa351`.

## Obsolete native duplicate-suppression sets: product validation pending

Twelve raw-pointer sets in pipeline.cpp were filled/cleared per native probe
face solely to suppress later retained producers. Those producers no longer
execute: renderGeomDeferred/renderGeomPostDeferred unconditionally return after
the native face capture, including incomplete transactions. Begin failure defers;
end failure cancels/withholds publication. Main/impostor captures never fill the
sets. They have no ownership/lifetime role or exact-original equivalent.

Removed only those sets, inserts, clears and unreachable suppression guards.
Retain direct_probe_scene_active, water-surface once-only capture, avatar-alpha
accepted-batch state reset, all alpha/cull/depth/color flags, masked-bump ordering,
completeness and exception handling. Exact Release syntax passes; no runtime
gain/parity claim yet. Original1eb mapping: hero manager → reflection map update →
cubeSnapshot/display_cube_face → fresh per-face cull/shadows/stateSort/pool render.
Native retains fresh view preparation and authoritative scene/post capture.
