# Build69236 Home and club checkpoint — 2026-11-17

The Home slide persists and the intended transparent consumer has not been demonstrated in-world. Shared/base mask reuse is active, but all sampled prepared-alpha counters are zero. Source review confirms PPD363: preparation eligibility requires pre-water alpha, whereas the actual forward draw owner supports both pre- and post-water alpha. This is an integration omission to repair and test before judging that part of the group. No engine change or new GPU frame capture was made during this assessment.

## Recording method and identity

This checkpoint uses continuous low-overhead CPU/GPU Overview recording plus native frame/command/resource metrics and logs. It is **not a replayable Metal frame capture**. It identifies CPU work, command completion waits, associated GPU latency and presentation, but not exclusive shader execution, hardware occupancy, memory-stall reasons, current register allocation or spill traffic. Those require appropriate fresh GPU frame captures/counters and, for queue/synchronization gaps, a bounded detailed trace. Older replayable captures predate the prepared transparent entry points.

Exact staged69236/source `f99b2daf1469ae238a0f93403c2a2fe0bfa79031`, PID42620; executable `763c0a441648e2c4d5790eb32842d081813b274909d952ff818cac6886741346`, metallib `fae7a43b5b48b7375f3a47ae52b6c7c7d89dd6b5f6a7456562d9e99438e223d0`. Private run `/Users/dekstroza/Alchemy-home-e2onni_5`; Home `overview/`, club `overview-club/`, one continuous native file. API/shader validation and GPU capture layer were off. Owner drove Login, teleport and logout; no agent graphics/location changes or settings-file writes.

Initial attempt `/Users/dekstroza/Alchemy-home-v5xr0z4g` is preserved separately. Root incorrectly increased the controller's native and collector caps to512MiB although both support at most256. The collector rejected512; a256attach retry recorded Overview while the viewer's512configuration had disabled native metrics. The error was disclosed, owner closed the viewer, and corrected256launch produced the good run. PPD362 retains this failure and replacement; the invalid attempt is excluded from comparisons. Cache state is warm following that login; no caches were purged.

Both corrected collectors finish complete/exit0. Home common CPU/GPU coverage is world−25.4996→+154.7243s; club covers arrival−17.6761→+162.2885s, including teleport/loading and more than120s after arrival. World start is17:41:39.274202938UTC; teleport starts17:47:06.001044480, first normal post-teleport frame starts17:47:07.711857647. Owner reports Home about31initial→16settled and visually okay; club above30arrival→19–20during recording, then22after the trace. Receipt times are not exact scene-transition or FPS interval boundaries.

## Where the time went

| Native eligible Home phase | FPS | Period ms | Main GPU completion wait ms | Other elapsed ms |
|---|---:|---:|---:|---:|
|0–10s|31.060|32.196|13.336|18.860|
|30–60s|18.632|53.671|32.914|20.757|
|80–120s|16.103|62.101|42.999|19.102|

The main wait increase accounts for the Home period increase. Every1,286late eligible wait start targets an already-running GPU command envelope two display frames older. Only0.434ms/frame follows the target's GPU end. Main running CPU falls48.52→27.86% of one core; late decode is0.099CPU-s/40s. This remains GPU-completion limited, not late sustained decoding or main-core saturation. Envelopes include dependencies and gaps, so they cannot identify a specific shader or hardware stall.

At the same3456×2168main target and exactly matching49selected Home controls, previous69233 versus current69236 late native values are15.8859→16.1027FPS,62.9489→62.1015ms/frame and43.0256→42.9990ms waiting. The wait difference is only0.0266ms; other elapsed decreases0.8208ms. There is no material demonstrated reduction in the dominant Home wait. Current39projectors versus previous51, different probe/resource/loading state and one sequential comparison prevent attributing the small period difference to the code. These are scene workload observations, not evidence the owner changed graphics settings. No whole-viewer gain is accepted.

Club is a different mix. Native arrival60–120s averages18.510FPS/54.024ms, with9.984ms main wait and44.040ms other elapsed. At120–162.288s it averages19.770FPS/50.582ms, with12.862ms wait. Independent Overview80–120s has main running72.84% of one core and49.178decoder CPU-s/40s. Main CPU owners in that40s are sun/shadows10.213CPU-s, geometry3.538, lighting3.700, simulation3.717 and ordinary probes1.766, with UI/other making up the rest. These are CPU samples, not GPU pass times, and the80–120window is distinct from the native60–120row. Decoding nearly clears in the final42.288s (1.359CPU-s), while main still runs68.58% of one core. Club therefore needs CPU scene/shadow work considered alongside GPU pressure; the Home explanation cannot be applied wholesale.

## What actually ran

Shared FrameData sampled brackets show Home17:41:43–17:43:35 deltas289preparations/1,589reuses/2,052declines/0failures; club17:47:09–17:49:45 deltas136/93/4,941/0. This establishes live reuse and preparation, not a timing benefit or resident-memory measurement. Published mask bytes remain cumulative encoded payload.

Prepared-alpha latest-successful snapshots are0unshadowed/0shadowed in all38Home0–120rows and56postarrival club rows, with fresh source frame/kind/index. They are sampled call snapshots, not exhaustive cumulative per-draw activity. Existing High2/pair PSO census proves those specializations ran but cannot substitute for prepared-entry activation.

PPD363 source proof: `almetalworld.cpp` preparation gate requires `batch.pre_water_alpha`; the draw branch accepts `pre_water_alpha || post_water_alpha || avatar_alpha`, and candidate pipeline selection otherwise supports a standard post-water simple batch. `pipeline.cpp` publishes mutually exclusive pre/post owner flags with the simple-lighting family independently selected. A valid post-water-only scene therefore cannot request preparation. The new authored fixture always sets pre-water alpha and missed this boundary. Runtime per-draw stage histograms were not retained, so this source omission is consistent with the zero samples without proving it was their only cause. Do not call the intended transparency extension exercised or effective from this recording.

## Warnings and retained limits

All324WARNING rows at44source sites are classified;0ERROR/FATAL, three libpng stderr warnings. Content/transport/lifecycle observations include64invalid joint-name warnings, an animation asset failure, a dropped plugin message, known old-region cleanup and shutdown warnings. They do not establish a new shader/performance cause. Full source mapping and phase counts remain in the private log audit; current shader spilling is unmeasured.

Native JSONL is268,431,716B and ends at the byte limit. Last ordinary event17:51:03.547428 and footer17:51:03.587710855 occur after both timed traces. Footer425contention/110budget drops, zero full/I/O loss,27committed and one other created command unknown at cutoff. Pending at this diagnostic cutoff is not evidence of GPU failure. Every compared-phase wait has a known target, but the stream remains partial/lossy. Native-only tails, later second teleport17:51:29–32 and owner22FPS after-trace report are kept separate. Viewer follows requestQuit17:51:37→Goodbye17:52:00; PID absent, actual manual-launch process exit code uncollected.

## Decision and next boundary

First correct PPD363 and add actual pre/post-water owner coverage, including rigid/skinned, shadow/High2/pair, exact color/depth/stencil, generic fallback and observable activation across successful calls. Verify the repaired path is used before assessing its cost. This remains unfinished group1 integration; it does not establish that the optimization will improve FPS once active.

For the remaining Home GPU work, use fresh whole-frame Metal captures of the corrected build's early and settled Home states, with actual pipeline/source/binding identity, shader/counter profiling and compiler/native-code spill evidence where available. Keep capture/replay cost separate from the continuous FPS baseline. Club additionally points to CPU shadow/scene preparation and loading. Continue the approved per-pixel, completion-admission and tile-local plan from those findings; preserve all accepted lighting contributions and functionality. No new implementation, recording, replay, build, push or private upload follows this assessment without the next authorized work step.

Private frozen reports: `analysis/native/findings-final.md`, `analysis/overview/report.md`, `analysis/log-audit/report.md`, each with receipts, raw inputs and hashes. Root `analysis/checkpoint-receipt.json` binds the combined assessment. [Implementation and staged source](../work/active/probe-transparency-preparation-20261117.md). PPD319/343/363 stay open; architecture and performance acceptance remain partial.
