# 69277 owner checkpoint: Home lighting growth and Club CPU work

Saved-data analysis has reached its checkpoint; work pauses for the owner's reboot.
No renderer change or additional viewer run belongs to this analysis checkpoint.
Whole-viewer performance, visual, loading and stability acceptance remain partial.

[Joined private findings](/Users/dekstroza/Alchemy-home-69277-rqs_6lzz/analysis/checkpoint-findings-final.md)
and the [four-archive GPU qualification](/Users/dekstroza/Alchemy-home-69277-rqs_6lzz/analysis/gpu-replay-ui-01/four-archive-report.md)
retain exact counter values, selected UI observation provenance, raw-data pointers,
unknown fields and failed evidence history. Private captures/logs stay local.

The exact combined viewer is26.4.0.69277, canonical engine
`e426d7956f48268833513cae78894e92ce30a61e`, builder
`d2c9bd1533cf7d7ca5044d476bf02c8e2540eb35`, identical indra tree
`7c5eaa088e1e2cb325fa6934e0d1dab8b9d856ad`. Executable SHA256 is
`b2bd311fdd4bca654d8e1c2f059d51d0ea8fe580f6a3780d156136e9ebb59aaf`;
metallib is `b14478bea751dc27e099c2b575c23e7bb6066614eb1fd68997b496c741286cf6`.
The [frozen stage and build/flags evidence](/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/apple-first-engine-completion-20261118/final-combined-26.4.0.69277/README.md)
retains Release/O3/NDEBUG ARM64, finite-aware CPU flags, Metal3.1 fast math with
source/line records, LTO OFF, final native gates and exact stage verification.
No graphics/environment overrides were made for this run.

## Home: per-fragment work grows while main CPU stays nearly flat

Within the run, capture-excluded native cadence falls28.763→15.161Hz;
required-main CPU stays21.925→22.358ms while measured main waits rise
7.720→37.930ms. These are source-defined samples/iteration cadence, not
scanout FPS or an exact subtraction of separately averaged scopes.

Fresh Medium/Overlapping profiles on independent saved-frame copies show:

| Main encoder | Fragment invocations early→late | ALU instructions/invocation | Sampler calls/invocation |
|---|---:|---:|---:|
| BASE |7,520,256→7,520,256|922.35→5378.81|12.07→43.03|
| Alpha-containing atmospheric-haze |59,600,800→23,222,080|244.00→1424.76|7.51→16.15|
| Point lighting |47,779,104→47,779,104|132.40→137.96|4.01→4.05|
| Projector lighting |211,390,560→211,390,528|74.74→75.35|1.97→1.97|

BASE fragment buffer L1 reads grow20.65→576.09MiB, with zero reported fragment
buffer writes in both; alpha-containing pass reads grow33.64→394.03MiB and
occupancy falls24.71→12.64%. Traffic is not proof of spills. The latter encoder
contains multiple functions; its label does not establish that atmospheric fog
alone owns its cost. Late prepared BASE and simple-alpha shaders are actually
selected, so a universal failure to use the shared consumer is not the finding.
Point/projector primitive counts remain9/39; native sampled input rectangles are
also stable. Exact configured/accepted probe counts at these GPU frame IDs were
not retained in the cached reducer. Phase-only configuration growth must not be
substituted for accepted taps or an exact frame join.

This is strong evidence to focus on growing per-receiver lighting work. It does
not yet isolate traversal, accepted weighting/SH/radiance, filtering, shadows or
data access. Neither ALU pressure nor a low occupancy number establishes spills.
Home shader tables retain99/107 rows;40/48 trailing spill fields remain unknown,
while59 fields explicitly say zero in each. Dominant blank fields stay unknown.
Replay durations63.05→145.77ms are diagnostics, not live FPS or exclusive main
execution. See the [complete Home counter comparison](/Users/dekstroza/Alchemy-home-69277-rqs_6lzz/analysis/gpu-replay-ui-01/home-early-late-counter-report.md).

## Club: substantial main-thread work remains after decoding settles

Club120–173.292s retains52.473 main CPU-s over53.292s,98.46% of one core.
The measured required-main snapshot is52.363ms with zero measured main wait.
Inclusive sampled stacks include sun-shadow27.411s (including spot-shadow work),
geometry8.120s, drawIndexed5.848s, vertex bindings5.713s and resource planning
6.515s. These overlap and must not be added. The resource hash insertion leaf is
2.374s exclusive, still not guaranteed removable time. Late compilation samples
are only0.007s; sleeping compiler helper counts do not explain this sustained
window. Decoder and mesh work are substantial near arrival, then decline.

The [source-priority audit](/Users/dekstroza/Alchemy-home-69277-rqs_6lzz/analysis/source-priorities-69277-01/report.md)
identifies a bounded resource key-hit temporary-construction candidate while
preserving pins, generations, dependencies and completion retirement. Shadow
publication/membership reuse, native binding caches and spot-shadow caching are
already implemented. Further reuse of current-view preparation needs actual
repetition/cost proof; simply adding workers or skipping casters is not justified.
Instrumentation also consumes CPU; no matched capture-OFF counterfactual was
recorded, so neither its full saving nor a new architecture FPS regression is
established. Actual retained Club replay scope is recorded below.

## Coverage, warnings and appearance are retained separately

Native recording has994,696 rows/200,750,483bytes,191 contention losses and no
full/budget/I/O losses. It expires at21:01:05.947789251UTC, before Club's180s
automatic late target. Three automatic captures stop/error0; their stop calls
take3.993/8.566/8.916s. Overview recording/exports exit0 but GPU coverage fails
the5s gap gate with13.481942458s. A later manually attached external Club capture
is not simultaneous with that native/Overview coverage. Its incidental Xcode
dependency build starts after the original recording ends, so it cannot explain
the earlier Home trend. Owner forcequit during the later debugger/capture workflow
is not normal logout or proof of a spontaneous engine crash.

All317 viewer warnings across37 sites, zero ERROR/FATAL, plus three stderr PNG
warnings are source/condition mapped in the [closed full-log audit](/Users/dekstroza/Alchemy-home-69277-rqs_6lzz/analysis/log-warning-audit-02/report.md).
These include66 transport timeouts,22 distinct HTTP403/marked-missing textures,
19 material failures across15 assets,29 missing bake auxiliary masks and repeated
invalid skin-matrix fallback (at least2900 palette calls). The fallback changes
intended deformation; its producer cause and association with reported hair are
unknown. All29 bake-mask IDs differ from all22 marked-missing texture IDs. The
separate recorder transcript has339 DWARF diagnostics; debugger/ViewBridge, lost
signpost, optimized stepping and forced-quit messages are separately qualified
with closed Apple-source limitations, rather than omitted from the warning review. Delayed HUD/dark login, brief TP unresponsiveness and later slow hair
appearance are retained as PPD463/464/466. No exact affected-object link or recovery
of the22 marked-missing assets is established. All warning sites remain in the
audit even when they lack measured FPS attribution.

The [native](/Users/dekstroza/Alchemy-home-69277-rqs_6lzz/analysis/native-phase-audit-01/report.md),
[running-CPU](/Users/dekstroza/Alchemy-home-69277-rqs_6lzz/analysis/overview-cpu-followup-01/report.md)
and [process](/Users/dekstroza/Alchemy-home-69277-rqs_6lzz/analysis/process-audit-01/report.md)
reports retain phase limits, nonadditive scopes, CEF descendants and actual helper
conditions. PPD311/319/435/463–466 stay open. PPD465 retains coverage failures,
forced-quit workflow, rejected evidence drafts/parsers and their narrow repairs.

## Next action after the owner resumes

Use controlled, restored saved-frame experiments on the measured Home lighting
work before selecting a shader/layout/cache change. Preserve all valid lighting
and quality; evaluate accepted work, input access and traversal separately.
For Club, qualify the narrow resource-use candidate and then proven shadow
preparation repetition as complete owners, retaining completion-based safety.
Repair capture scheduling/retention before another runtime checkpoint and collect
matched normal controls. Investigate numerical/asset appearance defects with
bounded producer/object provenance. No new implementation, launch, recording or
push is authorized by this analysis note; pause for the owner's reboot first.

## Saved-frame results and end state

| Archive | Retained GPU workload | Fresh profile outcome |
|---|---|---|
| Home early190→191 |20 command buffers/44 render/8 compute/9 blit/5365 draws|63.05ms; all5 counter categories61 rows;99 shader rows|
| Home late3419→3420 |19/44/8/7/5826|145.77ms; all5 categories59 rows;107 shader rows|
| Club early5116→5117 |21/42/6/11/4980|45.40ms; all5 categories59 rows;131 shader rows|
| External settled Club, later separate interval |12/37/1/5/6580|Archive opens/replays; profiling did not complete, timing/counters/spills unavailable|

All completed profiles used Medium/Overlapping. Club early's shader costs include
SSAO blur21.70%, point lighting17.08%, light-map9.15% and main BASE6.33%; these
are profile shares, not exclusive live-frame milliseconds. Main projector encoding
is absent in that captured main sequence; a later small-view projector encoder
belongs to ordinary-probe work and must not be relabelled as main or zero cost.
All first-three captures retain ordinary-probe rendering/filter/downsample work.
No exclusive per-queue execution-cost decomposition is claimed.

The external debugger also displays11146 API-usage insights, including redundant
binding recommendations and resource-allocation recommendations. These are retained
debugger observations, distinct from the317 viewer log warnings. Their full
per-command source classification and removable cost remain incomplete. Existing
binding caches are source-confirmed; an insight count is not measured CPU savings
or proof of a missing cache. No recommendation is promoted to a rendering defect
or applied without caller/cost verification.

The external profiler continued to report Running in background with incomplete
activity. Its43 encoder rows showed '-' execution costs and zero placeholders;
these are explicitly unqualified, not measured zero work. No shader variant or
additional profile retry was run. Its observed encoder list lacks an ordinary-probe
render/filter chain; this is a retained-capture limit, not a claim that the live
viewer stopped updating probes. Medium/Overlapping settings and the failed attempt
are retained. GPU debugging was stopped; Xcode reports Finished running, and both
Xcode and viewer are absent from final app/process checks. The saved archives,
independent replay copies, completed counter tables, failure history and exact
selected UI observations remain private on disk. No automatic reboot is issued.

The originally requested four-way detailed comparison is partial: three fully
profiled frames and one preserved external archive with unavailable profile data.
The missing automatic Club-late frame, failed Overview GPU-gap gate, native cutoff,
unknown dominant compiler spill fields and absent normal controls remain open
coverage limits. PPD465 must be repaired before another recording. No whole-viewer
speedup or release acceptance is claimed; resume only after the owner returns.
