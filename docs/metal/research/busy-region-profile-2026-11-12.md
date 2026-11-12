# Owner-chosen busy-region profile — 2026-11-12

The strongest CPU lead is shadow preparation and submission, rather than avatar
animation alone. The main thread accumulated 251.590 sampled CPU seconds during
a 301.302-second capture; 103.805 seconds were inside `generateSunShadow`.
Despite its name, that function handles both directional and projector shadows;
41.26% is their combined CPU owner, not a sun-only measurement.
The short GPU capture also shows substantial shadow, lighting and SSAO work.
This is a workload diagnosis, not a measured optimization or a quiet-versus-crowd
comparison. No renderer implementation changed.

The [follow-up shadow audit](shadow-preparation-audit-2026-11-12.md) adds an
original-versus-port comparison, proven scratch-texture churn, a guarded
directional/projector GPU split from resource/order evidence, and a passing
M1 Max depth-array capability probe. It refines the experiment priorities below. The [enabled shadow-query follow-up](shadow-occlusion-enabled-profile-2026-11-12.md) records the subsequent local implementation and181-second run; it does not establish a causal speedup.

## Build, workload and capture

- Existing owner-tested Alchemy Metal Alpha 4 **26.4.0.68841**, Release/LTO off.
  Binary source `cb840a5b9d68eec51a6cf5e7c0d6a3e3dbaea656`; executable SHA256
  `736d56f5fd940803a2e1c24cb236cfa7a48c9fe33b88f06aa8002eed02064875`;
  Mach-O UUID `5218D144-5031-357D-A165-B5CE23BF7D4F`, captured PID 92651.
  Checkout `24515e9cdf255f9d430e910884b4cefd1a17baaf` differs only in documentation.
- Apple M1 Max, 64 GiB unified memory, macOS 26.5.2, Xcode 26.6/17F113.
  Doctor exit 0 after the sandboxed hardware query was denied. Static app
  verification exit 0: 346 Mach-O files, all ARM64. No rebuild was necessary.
- Owner logged in, chose a crowded region and preferred graphics, retained the
  disk cache, and reported stopping a separate GPU-using translator before this
  clean run. Exact live graphics values, drawable dimensions and avatar counts
  were not independently recorded. Login and teleport remained owner-controlled.
  Owner subsequently clarified that the clean scene was inside a club with
  moving projector lights; an outdoor lighting assumption is inappropriate.
- **Game Performance Overview:** 09:04:39.412–09:09:40.714 UTC,
  **301.301703 seconds**, Deferred recording, exit 0. CPU sample timestamps span
  300.779 seconds, all 61 five-second bins populated. Address-only main leaves
  are 0.71%; missing main backtraces are 0.0012%.
- **Metal System Trace detail:** 09:11:09.510–09:11:21.423 UTC,
  **11.912206 seconds**, Deferred recording, exit 0. This is a separate short
  sample of the same owner-selected scene, not five minutes of detailed GPU data.
  Both timers stopped before the owner logged out.

Raw traces, exports, scripts, receipts and private scene metadata are local under
`artifacts/metal/busy-region-profile-20261112/` and remain ignored by Git.
The clean trace is open in Instruments for inspection.

## Frame delivery and CPU ownership

The overview recorded approximately 10,700 presented frames. Its 301 interval
FPS observations cover 300.702 seconds. Duration-weighted average FPS is
**35.57**; the median interval FPS is **35.90**, and the 10th/90th percentile
interval values are **32.61/38.99**. These percentiles describe roughly
one-second aggregates, not individual frame times or a 1% low benchmark.
The interval range is 19.18–42.89 FPS. Profiler overhead is included.

All target threads accumulated **491.094 sampled CPU seconds**, equivalent to
**1.63 continuously running cores**. Main-thread work is **83.50% of elapsed
time on one core**. Every main sample was on a performance core; this capture
does not support a diagnosis of the main thread being stranded on efficiency
cores. It also does not show sustained saturation of all CPU cores.

| Main-thread category | Sampled CPU seconds | Share of main CPU |
|---|---:|---:|
| Combined directional/projector shadow processing | 103.805 | 41.26% |
| Native rendering and driver encoding union | 75.664 | 30.07% |
| Explicitly named avatar methods, broad union | 45.740 | 18.18% |
| Avatar idle updates | 20.427 | 8.12% |
| Avatar animation and pose union | 10.636 | 4.23% |
| Skin palette/matrix union | 4.424 | 1.76% |
| Memory-copy leaves | 9.126 | 3.63% |

**These categories overlap and must not be added.** Rendering/encoding and
avatar methods occur inside shadows. Native renderer/driver calls can perform
avatar-caused work without an avatar-named ancestor, so 18.18% is not a complete
measure of the cost caused by avatars.

Within the combined shadow-generator subtree, native shadow rendering accounts for 44.039 CPU
seconds, state sorting 14.987, post-sorting 14.951, culling 5.497 and palette
preparation 3.917. Those subtrees also overlap. Both ImageDecode workers together
used **131.486 CPU seconds**, predominantly OpenJPEG decoding. Warm disk caches
did not eliminate decoding during the session; the profile does not establish
why each asset required decoding.

## GPU work and the avatar connection

The detailed trace has 314 present requests, approximately 313 world-render
chains and **1,878 shadow slices: six per world chain**. Source explains those
six as four directional cascades plus two projector slots:

- Main resource planning supplies four directional cascades
  (`indra/alrender/alrenderresourceplan.cpp:124`). `generateSunShadow` loops them
  at `pipeline.cpp:27087`, begins directional capture at 27502, then loops two
  projector slots at 27582 and begins projector capture at 27722.
- Both paths reach `renderSpotShadow` through `pipeline.cpp:14888`. The native
  command/encoder labels are generically named “spot-shadow”
  (`indra/almetal/almetalworld.cpp:6731,6897`). **That label does not mean all six
  slices are local lights**. Labels alone do not split their GPU costs; the
  follow-up audit derives a guarded source-correlated family estimate.
- Being indoors does not itself disable directional processing. Some planned
  directional slices can be empty clears, including when directional light is
  black or a cascade has no receivers (`pipeline.cpp:26960,27139,28789`). Six
  encoder submissions therefore do not prove four populated directional maps.
- Each slice can traverse rigged attachment families and avatar pools
  (`pipeline.cpp:13622`; `lldrawpoolavatar.cpp:513`). Visible avatar casters can
  therefore add preparation, bindings, skinning and rasterization across several
  shadow views. This supports the owner's observation about crowds, but a single
  scene without a controlled avatar-count comparison cannot establish the cost
  per avatar or prove avatars account for all shadow work.
- Projector caching already exists. Rigged casters/avatar pools mark maps as
  changing (`pipeline.cpp:26254`; `lldrawpool.cpp:293`). Animated casters can keep
  both projector maps updating; skipping those updates indiscriminately would
  change rendering correctness.
- Moving/rotating projectors also change the cache's view/projection matrices
  (`pipeline.cpp:27696`; `alspotshadowcache.cpp:97`), independently requiring map
  redraw. This is consistent with the club description; its measured share of
  the combined CPU/GPU shadow cost remains unknown. Only two projector shadow
  slots are selected by this path; other projector lights can still add lighting
  work without each receiving a separate shadow map.

Target GPU Active intervals union to **7.973 seconds**, within a
**10.749-second target activity span** (the trace includes startup).
This short sample is nonstationary: several one-second bins are 92–98% active,
but a roughly 1.7-second present-request gap produces much lower activity near
the end. It cannot establish one fixed GPU-bound/CPU-bound classification for
the entire five-minute session.

| GPU label/group | Union of target Active intervals |
|---|---:|
| Shadow slices, directional and projector combined | 2.377 s |
| SSAO horizontal and vertical blur, combined union | 2.023 s |
| Light map | 1.479 s |
| Local point lighting | 1.152 s |
| Simple G-buffer | 1.129 s |

These are overlapping interval unions, **not additive frame costs**. Shader
counters were not captured, and these labels do not separate rigged avatars,
static geometry and alpha work. The GPU evidence identifies substantial work
outside avatar animation; it does not attribute all that work to avatars.

Present-command-buffer-only timing would miss most GPU work: offscreen-only
activity contributes 6.954 seconds of the target union. However, frame assignment
associates 3,404 of 3,407 active nonpresent command buffers with frames that have
present requests. There is therefore no basis to claim the Overview excludes
the offscreen world rendering. Its 100% “GPU Usage” value is a normalized
drawable-associated in-flight/frame-interval ratio, not proof of 100% hardware
execution-unit or memory-bandwidth utilization. In-flight work from adjacent
frames overlaps.

## What this says about direct shared-memory geometry

The experiment remains possible, but it targets a smaller measured CPU slice
than the shadow path:

- All main-thread memory-copy leaves: **9.126 s / 3.63%**.
- Copies whose direct caller is `ALMetalBuffer::write`: **3.112 s / 1.24%**.
- Copies anywhere under vertex-buffer code: **2.913 s / 1.16%**.
- Frame-data publication copies: **0.580 s / 0.23%**.

These are overlapping views of copy time. The vertex-buffer figure is not a
measurement of all GPU geometry traffic, and not every copy there would be
removed by generating geometry directly into shared storage. Even removing all
of that named copy CPU cost would affect only about 1.2% of main CPU time in
this scene; this is not a predicted FPS improvement.

**No exact geometry-upload/copy-byte count or bandwidth measurement was
collected.** CPU sampling cannot recover it. The process's reported peak
physical footprint ranged from approximately **21.55 to 29.31 GiB**; this is not
GPU upload volume, a residency-budget measurement, or evidence of a leak.
Thermal state was Nominal for 96.655 seconds, then Fair for 204.647 seconds;
neither a clock-rate trace nor an isolated thermal comparison was collected.

Existing code already caches rigged attachment matrices by avatar, skin and
frame (`llvoavatar.cpp:10858`) and byte-deduplicates palette publication
(`almetalworld.cpp:1419`). `prepareMetalBuffers` checks publication/readiness and
pending unmaps (`llvertexbuffer.cpp:1156`); it does not copy all geometry every
shadow pass. Built-in body palettes incorporate each view's model-view matrix
(`pipeline.cpp:14709`; `llviewerjointmesh.cpp:221`) and cannot simply be reused
unchanged across different shadow cameras.

## Best next experiments, not implementation authorization

1. **Shadow preparation/submission reuse.** Investigate frame-scoped immutable
   geometry/material/pose handles, repeated palette validation
   (`almetalworld.cpp:6529`) and native per-draw binding. The existing bulk shadow
   path excludes avatar/rigged draws (`almetalworld.cpp:6630`). Preserve the
   caches that already work and measure any new reuse rather than assuming it.
2. **Separate shadow traversal from GPU raster cost.** A controlled A/B of the
   existing `RenderShadowCullMode=1` union-walk/cascade-bucketing option
   (`pipeline.cpp:26187`) can isolate CPU traversal effects. Its live value was
   not recorded in this session, so this is not evidence that it was disabled.
   Separately label directional/projector slices and collect caster counts,
   triangle coverage and map dimensions to attribute GPU shadow cost.
3. **Inspect lighting and SSAO as independent GPU costs.** Improving CPU copies
   or animation scheduling does not remove those passes' GPU work. Any
   optimization must retain the owner's chosen rendering behavior and be
   checked for visual correctness.

Shadow traversal currently mutates shared cameras, visibility, face pools and
mesh state (`pipeline.cpp:21345,21556`). Moving those functions directly onto
workers would race existing ownership. Image decoding already uses workers
(`llimageworker.cpp:104`). The profile supports investigating a narrower immutable
render-input boundary; it does not demonstrate that a whole threading rewrite
is required, or that such a rewrite alone would improve GPU throughput.

## Evidence and reproduction

Primary files under the private artifact directory:

- `busy-region-clean-5min.trace`, `clean-toc.xml`, `clean-time-profile.xml`,
  `clean-cpu-analysis.json`, `clean-cpu-subtrees.json` and execution receipts.
- `clean-count.xml`, `clean-duration.xml`, `clean-process.xml`,
  `clean-thermal.xml`, `clean-overview-analysis.json` and
  `analysis/overview_metrics.py`. Duration-statistic variant IDs are preserved
  without assuming their enum names; FPS results use the documented count table.
- `busy-region-clean-gpu-detail.trace`, `clean-detail/` exports and analyses.

Actual recording command shape (absolute output paths are in private receipts):

```sh
xcrun xctrace record --template 'Game Performance Overview' \
  --attach 92651 --time-limit 5m --no-prompt --output busy-region-clean-5min.trace
xcrun xctrace record --template 'Metal System Trace' \
  --attach 92651 --time-limit 10s --no-prompt --output busy-region-clean-gpu-detail.trace
```

The initial Game Performance attempt is excluded: its template's five-second
rolling buffer retained only a short segment. An enlarged-window setup check
produced excessive raw data and was stopped. A separate dSYM preparation was
terminated before recording and its partial output is invalid. Neither failed
setup artifact is evidence for the findings above. The final Overview preflight
verified full-duration CPU coverage before the clean run.

Apple's [long-session profiling demonstration](https://developer.apple.com/videos/play/meet-with-apple/242/)
describes Overview for multi-minute captures and Metal System Trace for short
detailed followups. There is no unprofiled control, quiet-scene baseline,
translator on/off comparison, settings A/B, implementation change, or measured
speedup in this session. The stopped earlier 20-FPS campaign remains separate.
