# Home GPU replay checkpoint — 69238, 2026-11-17

The owner requested fresh replayable Home GPU frames across login and the FPS
decline, followed by analysis and a recommendation before another engine change.
This recording localizes the growing cost to main-view fragment lighting,
especially simple transparent and BASE probe-sensitive shading. Prepared
post-water encoding now occurs in-world, but the inspected costly transparent
draw uses the generic entry. The exact late fallback reason and the remaining
probe traversal/accepted-SH/cache cost split are still unknown.

## Exact acquisition and retained evidence

App26.4.0.69238/source `f0c96273654c652842ca7ea6bad671224b891e01`,
executable SHA256 `e8fb943f1eede413e2c32c7182d70722a7b45476f67ba638723f93c514e45139`,
metallib `fae7a43b5b48b7375f3a47ae52b6c7c7d89dd6b5f6a7456562d9e99438e223d0`.
Root implementation035b3bbb50, handoffc6654ce1fd, identical `indra` tree
`3d4a2d9bd5dd743592f72e73e07421dd54d3ca52`. Apple M1 Max, macOS26.5.2,
Xcode26.6(17F113). No renderer/source/compiler option changed for this analysis.

Private run: `/Users/dekstroza/Alchemy-home-bxeu4ty7`.
The qualified manual-login launcher ran with `--capture-frames`, retaining the
same19 architecture arguments and256MiB bounded native sink. CPU/native logging
started before manual login. World start18:56:54.750961397UTC; Overview CPU/GPU
common interval spans world−28.036648689 through+152.510523311s. Home only in
this owner-authorized diagnostic; no club teleport was requested or performed.

Both device-wide captures have successful stopped metadata:

| Capture | Display interval | Capture API world seconds | Stop API cost |
| --- | --- | --- | --- |
| early |214→215|2.051011→6.912749|4.403210s|
| slowed |1428→1429|62.691561→71.580680|8.447478s|

Scope is normal display entry to next eligible entry, with main/shadows and
intervening ordinary probes/uploads; it is not a complete history of previously
submitted commands crossing that boundary. Originals are preserved; independent
APFS working copies alone were opened in Xcode. Exact initial manifests cover
7,164early nodes/1,908,080,653file bytes and16,357late nodes/8,785,061,167bytes.
`analysis/gpu/preservation-receipt.json` binds source and working-copy equality.
Post-analysis raw rehash verifies every original node and10,693,141,820file bytes
unchanged (26checks, exit0); `analysis/raw-integrity/receipt.json` retains the
before/after inventories, exact hashes and source/app/native identity checks.

The Overview recorder and all three exports exit0. The outer collector remains
**FAILED**: an actual5.110910s GPU bucket-start gap exceeds its normal5s guard.
Its4.108333s uncovered portion aligns with late capture finalization, and GPU
records resume~25ms after capture API return, continuing through world152.51s.
The early hole similarly ends~22ms after capture return. This is not an
end-truncation pattern; do not relabel the failed normal qualification as passed.
Both saved GPU frames remain usable diagnostic inputs. No replacement recording
or threshold relaxation was performed.

The native file89,361,415bytes has SHA256
`cd288fefb0c811203f4081d04415dc809ae51029b4a377f20b666c6c56985202`.
Footer reports requested stop,141contention losses,0budget/full/I/O losses and
all77,645committed commands completed with no pending commands. It remains a
lossy stream. Owner closed the viewer; logs requestQuit19:00:17, Goodbye19:00:20,
and process absence was observed. Actual manual process exit code is unknown.

## Where live diagnostic frame time grows

Capture API intervals and first following periods are excluded below. The
capture layer remains armed, with measurable CPU work outside saves; these are
diagnostic observations, not normal-mode FPS or a69236-versus69238 gain test.

| World seconds | Cadence FPS | Period ms | Main GPU-completion wait ms/frame |
| --- | --- | --- | --- |
|0–10|28.581|34.988|7.382|
|10–30|26.245|38.103|9.546|
|30–60|18.216|54.897|26.033|
|80–120|16.733|59.763|32.951|
|120–152.51|15.200|65.791|39.830|

From10–30 to80–120 the period grows21.661ms while observed completion wait grows
23.405ms; the arithmetic remainder falls. Most observed wait targets are
already-running GPU command envelopes two display frames older. Their late
GPU-end-to-wait-return tail is~0.46ms/frame, so a long completion wakeup tail is
not the demonstrated source of the growing critical wait. This does not describe
all callback classes, nor make overlapping command envelopes isolated GPU costs.

Main sampled CPU falls62.72→37.75%of one core; decode falls45.218CPU-s/20s to
0.001CPU-s/40s. Native configured probes grow1→49. Main9point lights,
39projectors/two shadowed, their summed rectangles,3456×2168, SSR1/25/4 and hero0
remain fixed. Both49typed control snapshots match the prior69236Home snapshots;
that does not establish identical scene/material/cache workloads. Cache churn
and vertex publication continue after decode subsides. Logical source-cache
capacity is not a measurement of physical GPU residency.

## Fresh GPU replay identifies the growing work

Unchanged captured shaders/resources were profiled once per capture in matching
**Medium / Overlapping** mode. Early reported74.36ms and late119.32ms. These are
GPU-debugger replay results, not viewer frame periods. Detailed counters may be
collected with passes isolated; overlap prevents adding them as a live budget.

Early has25command buffers,47render/8compute/13blit encoders,10,132draws and
14,042,006vertices. Late has18/41/8/7,6,189draws and15,903,594vertices. Retained
texture resource sizes grow1.87→8.08GiB; these descriptor totals are not physical
working-set or bandwidth measurements.

| Actual counter | Fused main early→late | Main alpha/haze early→late |
| --- | --- | --- |
|Fragment invocations|251.649440M→251.649472M|59.175328M→23.213376M|
|ALU instructions per fragment|118.045→208.828 (**1.77×**)|244.861→1206.070 (**4.93×**)|
|Sampler calls per fragment|2.75→3.58|7.54→16.09|
|Fragment occupancy|18.00→15.91%|24.27→12.73%|
|Buffer-L1 read bytes per fragment|0.560→2.024|0.591→13.415|

Main lighting shades essentially the same number of fragments yet performs more
work per fragment. Alpha shades fewer fragments yet performs substantially more
work per fragment. More draws or more overdraw alone cannot explain this result.
Probe evaluation is the strongest source-backed lead as main probe inputs grow,
but these counters do not isolate traversal, accepted SH/radiance, shadow work,
material mix, divergence or cache latency into exclusive causal percentages.

Exact capture-frame telemetry gives main probes1→26, ordinary probes2→52. Both
main frames have the same light rectangles, target, SSR and hero operands. The
ordinary view is256²/tile-lightmap enabled in each frame; sampled faces differ.
A cheap ordinary face in one saved frame cannot characterize every probe update.

Shader cost-share ranking (not isolated duration): simple shadow alpha grows
13.85→23.47%, BASE geometry7.99→21.33%, PBR alpha4.10→6.33%. Their rounded
share×replay-total proxies are10.30→28.00ms,5.94→25.45ms and3.05→7.55ms.
SSAO blur proxies10.75→11.25ms and light-map4.71→4.82ms are roughly stable;
projectors12.83→14.20ms remain material baseline work. Do not add these proxies
into a critical path. The atmospheric-haze encoder includes ordered alpha;
actual haze shader share is only.55%in the slowed frame.

Seven2256-index simple-alpha draws account for nearly all of the dominant
23.47%shader share. Direct inspection of draw38566(6.27%) confirms the main depth
target, High2/pair-labelled generic
`almetal_world_pre_water_simple_shadow_fragment`, reverse-Z read-only depth,
probes at fragmentbuffer1, lighting2 and shadows3, with no prepared buffer8/9.
This is actual hot-draw evidence, independent of shader-table name absence.
It does not prove every draw or every frame is generic.

## What the repair and spill data establish

Native/log evidence now proves successful prepared post-water CPU encoding during
the run: last sampled main25,425prepared/54,553eligible post draws and ordinary87/49,660;
all pre counts0. Preparation failure samples stay0. The PPD363 eligibility repair
is active, but preparation is partial and the inspected slow hot draw falls back.
Early count1 deliberately yields all-full/unavailable masks, as source requires.
Late count26 alone cannot identify its unavailable-mask reason; full input
bounds, certification and admission outcomes are not retained by current counters.
Do not assume a new bug, successful culling, or pixel coverage from these counts.

The observed shader tables contain129early and112late rows. Expensive simple and
BASE entries report214allocated registers; their spill fields are blank/unknown.
There are59explicit-zero spill rows in each table,70early/53late blank and no
observed nonzero row. The table is not proven exhaustive: a late exposure-history
encoder lacks its corresponding shader row. Blank and absent are not zero.

Buffer read traffic grows, while FS device-buffer writes do not show the former
GiB-scale write observation. This does not establish a spill-free pipeline or
justify attributing the present decline to304-byte spills. No current source
reload, controlled shader A/B/A, raw buffer export or new per-line call-tree
profile is claimed. Native CSV and raw-buffer export Save actions remained
disabled; exact observed AX snapshots and derived tables/hashes are retained.
All replay was stopped with explicit Finished-running/Replay observations.

## Recommended next group

**Focus deeper on the current main-view probe-lighting path first.**

1. Explain why the inspected expensive simple-alpha draw used the generic path
   while prepared consumers were active elsewhere. Add bounded preparation
   decline-reason visibility and qualify the actual captured packet/admission
   boundary. The selected pair path already shares the candidate selector's
   simple/standard/nonavatar/nondepth predicates; first inspect scene-wide mask
   availability/capability rather than presume another batch-routing defect.
   Preserve legitimate fail-open cases; do not force candidate use.
2. Use controlled variants of the saved slowed frame to split candidate traversal,
   accepted probe weighting/SH/radiance work, shadow filtering and data traffic.
   Keep captured geometry/resources fixed, include unchanged/restored controls,
   and assess numerical correctness before promoting any variant to production.
   The counters identify the right shaders; they do not yet quantify each helper.
3. Implement only the demonstrated quality-preserving improvement. If the cause
   is publication/admission or reusable derived probe data, complete that shared
   ownership boundary as part of this group. If accepted per-pixel work dominates,
   optimize that work with compiler/counter evidence and exact-output checks.

GPU-pressure-aware admission of fresh optional probe updates remains a concrete
unfinished architecture boundary. Current planning sees CPU/memory/streaming
pressure but not fresh required-main logical completion state. Presentation
scheduler census is a different authority and must not be substituted. This work
may help loading/contention, but cannot by itself remove the cost of sampling
already-published probes in every settled main frame. Existing update caps,
bootstrap, coherent continuations and oldest-progress fairness must remain.

The existing ordinary-probe memoryless RG8 fusion and fused main lighting are
already present. More CPU workers or another broad architecture group has weaker
support than the identified main fragment paths for this Home decline. Club and
other scenes can have different CPU/GPU limits. No new renderer implementation,
viewer run, build or push follows this recommendation; owner discussion is next.

Private evidence indexes: `analysis/native/final-receipt.json`, captured-frame
input addendum, `analysis/overview/summary.json` with input/output manifests and commands, GPU-derived reports
and receipts, GPU preservation/AX snapshots, log audit and architecture boundary
map/recommendation. The final root checkpoint receipt binds actual file hashes.
PPD319/343 and whole-engine/performance acceptance remain open; PPD363 stays
ready_for_owner with runtime activation observed and visual/FPS acceptance partial.
