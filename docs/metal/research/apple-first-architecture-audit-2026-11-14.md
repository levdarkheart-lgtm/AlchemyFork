# Apple-first architecture follow-up audit — 2026-11-14

The delivered build is a partial shadow/resource experiment, not the complete
Apple-first architecture originally proposed. The earlier “all goals complete”
statement overstated architectural completion. The experimental app was built,
tested and staged; the original producer, scheduling and performance acceptance
boundaries were not all delivered. This audit corrects that distinction and
answers the owner's four questions using compiled6bae source and the saved run.
No source/settings change, rebuild, viewer launch or new capture was performed.

## 1. Why the opaque indirect fast path did not execute

**Enablement and hardware support worked.** `pipeline.cpp:14388` sets
`scene.opaque_indirect_draws=true` and passes that scene to the renderer; no
later override was found.45 main and45 probe snapshots consistently show one ICB
slot/one successful slot allocation. The native pool sets supported only after
Apple7 capability and a successful one-command allocation
(`almetalshadowindirect.mm:359–364`). Frame arenas are installed, ordinary direct
bulk rendering uses them, and required ICB PSOs are created eagerly: a PSO
creation failure would abort renderer creation, rather than silently omit them.

**Zero successful preparation/execution is real.** Generic prepared/executed/run
counters stay zero. Selected opaque static and attachment execution calls use
the same pool and increment those generic counters; this is not merely an
incorrect log field or absent sampled function name.

The default selector requires **2, 048 contiguous eligible native records in one
run**. It does not count all sources in the frame, all shadow views, one avatar,
or all visible opaque-looking objects. State/query/family boundaries split
runs; alpha/PBR coverage, terrain, avatar bodies/cloth and uncertified raw poses
are excluded. Existing admission can already coalesce sources into fewer native
records. A prepared packet is bounded to16, 384 records. The logged main ordinary
bulk average is about 150 draws/run, probe about 344: that suggests the accepted
2k unit poorly fits typical work, but averages cannot establish the maximum.

There is also a **silent optional fallback**. Constant publication, binding/
retention validation or command allocation failure can clear selected runs and
continue direct rendering successfully. `opaque_indirect_fallbacks` and other
opaque counters exist but the periodic logger does not print them. Therefore:

- Unsupported hardware and a forgotten enable assignment are ruled out.
- No qualifying run is the strongest current selection explanation.
- Failed optional preparation remains possible; the existing trace cannot
  distinguish it from no eligible run. No proved current-call-site bug explains
  the whole zero-execution result.

The accepted mixed headless fixture deliberately supplied2, 048 static opaque,
512PBR and2, 048 rigged opaque records, with boundaries exactly between families.
It proves correctness/net benefit for those long runs, not their prevalence in
Second Life. Smaller complete-cost cases lost, so lowering the threshold just
for a nonzero counter is not supported by the evidence.

A separate source-visible selection limitation is PPD-112: the legacy16k static
bulk budget is consumed before the opaque subset is extracted. Sixteen earlier
1, 024-record runs can exclude a later2, 048-record eligible run. The scene itself
has no16k total-input cap. This preserves direct rendering and is not known to
have occurred here; it must not be substituted as the captured cause.

The missing diagnostic is specific: expose the existing opaque attempt/fallback/
skipped counters, supported flag, static/rigged run-size maxima and rejection
reasons in the existing periodic report. Per-draw logging and another blind
five-minute trace are unnecessary merely to distinguish these alternatives.

## 2. What performed worse, and compared with what

Both builds are **already ARM64 native Metal Apple Silicon viewers**. G1 already
uses Metal device/queue/shaders, unified-memory-aware native resources, ARM SIMD,
GPU skinning, native frame arenas, OpenHTJ2K and earlier Apple-specific repairs.
This was not an original OpenGL or non-Apple viewer beating Metal.

The current busy-club run is worse in overall observed cadence: 17.77 FPS against
20.36G1 (−12.72%), with matching3456×2168 published surface and420 saved visual
controls. Amortized main CPU/present rises 33.97→49.30 ms. Source capture rises
2.22→7.34 ms and texture update/publication0.99→4.39 ms. That is an adverse engine
result requiring action, not a scene-variation excuse.

“Across the board” exceeds the data: only one current live interval was captured,
frame-time p95 is unavailable, and some work improves. State/post-sort CPU is
7.77→6.88 ms/present, while the associated GPU span changes only 85.39→87.22 ms.
The GPU metric is an overlapping command span, not proof of utilization or a
shader-only duration. More source/material/streaming work is quantified in the
[full profile](g2g5-busy-club-profile-2026-11-14.md); it helps identify scaling
costs without erasing the slower experience.

## 3. How much of the original architecture is actually present

| Original boundary | Delivered state |
|---|---|
| Publish geometry/material/resource generations at real dirty producers | **Missing central boundary.** Source records are still keyed and created inside each view's `captureMetalShadowScene`, not `registerFace/rebuildGeom` or a coherent material publication epoch. |
| Retained immutable data and safe leases | **Present for attachment/shadow records and native resources.** Useful foundation, with bounded cache and real lifetime/cancellation checks. |
| Ordered per-view admission | **Present for shadows.** Does not eliminate repeated CPU scene discovery across main/probe/shadow views. |
| Bounded detached frame preparation | **Partial.** At most two synchronous Dispatch tasks for one family>=8, 192, after owner-side key/cache/material/pose work. No whole-frame detached job/command-encoding graph. |
| Persistent GPU scene visibility/draw production | **Not in the production flow.** Experimental generation/visibility exists after CPU admission and remains disabled; there is no persistent GPU visibility stage before repeated CPU discovery. |
| Metal 4 | **Experimental compute pilot, off by default.** Metal 4 adoption was conditional on net benefit, so leaving a slower API experiment off is correct; it does not complete the missing data/scheduling architecture. |
| UMA publication | **Partial.** Shared native leases, dirty buffer ranges and texture COW are implemented. CPU staging and full-buffer/full-mip preservation still occur; no coherent whole-engine CPU/GPU residency budget. |
| Frame/resource scheduling | **Partial.** Separate world/shadow bounds and six-command logical shadow groups exist. Main, ordinary probes, decoding and uploads lack aggregate dependency/deadline/byte admission. |
| Tile-aware pass lifetimes | **Specific shadow improvement.** Memoryless temporary D32 is present; a complete main/light/post lifetime optimization was not implemented. |

The decisive G2 source is `pipeline.cpp:12913–12999`: build a full key, look up
cache state, possibly build/key/seal again, touch LRU, retain source/palette,
then prepare the view. `LLVolumeGeometryManager` is unchanged from G1. The CPU
profile positively sees key/cache/aggregate costs of roughly1.40/1.05/1.36 ms per
estimated present, respectively. Those categories overlap and are not summed.
The useful immutable boundary sits after expensive recurring owner work.

The original route required creation at scene/resource change, useful detached
preparation and whole-frame acceptance. The current implementation does not
satisfy those requirements. The canonical ledger is corrected to **partial
architecture**, with experimental build/staging delivery kept complete.

## 4. The log contains concrete problems, not a clean bill of health

The302 s measurement interval has112 WARNING and872 INFO lines, 0 ERROR/FATAL.
No logged native source/publication failure, device loss, GPU execution/command
failure or image-request exception was found. That does not rule out the silent
ICB fallback described above, stderr-only diagnostics or unlogged conditions.

The material texture symptoms are:

- 14 emergency low-memory/overbudget transition actions; the same category is 0
  in the G1 interval. The guard also allows genuine severe device overage, so
  the text alone does not prove every action was physical-system pressure.
- 39 printed immediate-downscale warnings are repeat milestones, 700→4500. They
  prove **at least3, 800 finer-than-desired create-list conditions and
  `scaleDown()` calls** during274 s, not merely39 events.
- Redundant-load handling can skip a new texture creation before that warning;
  scaleDown can queue/coalesce or decline. These are not3, 800 proved fresh
  uploads or successful GPU downscales. They do prove desired-LOD and completed
  decode/create-list processing repeatedly disagree.
- Six workers perform56, 104decode completions and628.6 CPU-s; waiting reaches
 2, 508 requests/4.64 s. Texture creation rises 6.97→16.22/frame. This load belongs
  to the engine's residency/publication budget, not an unrelated distraction.

Other warnings include asset/network failures, invalid animation joints and
nonfinite-joint fallback milestones. They deserve preservation but do not show
an Apple API failure.54exactness-withheld INFO messages are expected non-main
views (`ordinary_world=false`), not54failed main frames; G1 has62equivalent
messages. The final profiler log has336DWARF cursor warnings, corrected from
the preliminary333. CPU address-only leaf weight is about 0.53%; profiler
symbol warnings are not viewer GPU errors.

### Confirmed budget-domain problem: the16 GiB legacy process cap

`MaxHeapSize64` defaults to16.0 GiB with no Alchemy or saved-profile override.
It is a **policy cap**, not a16 GiB address-space/allocator limit: allocations
can exceed it. The actual Darwin control chain is:

1. `LLAppViewer::initMaxHeapSize` loads that setting.
2. `LLMemory::updateMemoryInfo` reads OS availability and process allocation,
   then clamps availability to
   `min(OS_available, max(16GiB - process_allocation, 0))`.
3. `LLViewerTexture::getFreeSystemMemory` consumes the clamped value, and
   `isSystemMemoryLow()` compares it against 512 MiB.
4. Pressure raises desired discard and can emergency-downsize offscreen images.

All five current and five G1 logged memory pairs exactly match this cap:

| Example | Process allocation | Reported availability |
|---|---:|---:|
| G1 |12, 722.55 MiB|3, 661.45 MiB|
| G2–G5 |15, 934.45 MiB|449.55 MiB|
| G2–G5 above cap |16, 890.92 MiB|0 MiB|

Each positive pair sums to16, 384 MiB. G1 remains about 12.4–12.7 GiB; the current
run reaches about 16.5 GiB and crosses the policy cap. Thus “system low” can mean
little space under a legacy process limit on this64 GiB Mac. The true pre-clamp
OS availability was not logged, so the label does not establish that physical
RAM was nearly exhausted.

This is PPD-111: a source-confirmed budget-domain conflict with observed
pressure/downsize churn. The warning/controller code is inherited and unchanged
from G1. The current source/resource retention, decoded/raw cache load and
native COW may contribute to the larger process footprint, but their allocation
shares are not measured. It is not proof the new cache alone consumed the extra
memory or that the cap accounts for the entire FPS difference.

Simply raising a setting is not the full architecture fix. A coherent native
UMA budget must distinguish actual OS pressure, intentional user process limits,
CPU/raw caches, native device working set and CPU/GPU-retained generations.
Otherwise the engine can decode toward one target while residency control
immediately asks for a lower-detail state.

## Rendering-engine conclusion and concrete next boundary

The evidence identifies a partial implementation with repeated serial
bookkeeping, an unexercised fast path and a real resource-policy/streaming
problem. It does not test the full proposed architecture or establish that
Apple-native rendering inherently performs worse.

The next work is to remove recurring discovery from the retained boundary,
reconcile memory/residency/decode/publication budgets, and coordinate producer
work on the native queue. Measure full-frame p50/p95, useful-work throughput,
texture response and quality across normal changing worlds. Keep shadows,
lights, projectors, probes and asset publication in that same dependency model.
Native APIs and spare cores cannot compensate for repeating work before the
immutable boundary or repeatedly revising residency targets after decode.

Source/render changes have not been made in this audit. PPD-110 remains the
adverse performance assessment; PPD-111 the budget conflict/churn; PPD-112 the
conditional packet-selection edge. Existing GPU support/native correctness
proofs remain valid, while architecture/performance completion claims are
explicitly corrected.

Safe detailed evidence in the ignored profile directory:
`opaque-indirect-source-audit.md/.json`,
`opaque-capability-runtime-census.json`, `architecture-completeness-audit.md/.json`,
`log-error-audit.md/.json`, `memory-cap-safe-audit.json`,
`memory-cap-independent-verification.json` and `capture-audit-addendum.json`.
Raw logs/settings/credentials were not exposed. No new capture, launch, build,
settings change, source fix or push occurred.
