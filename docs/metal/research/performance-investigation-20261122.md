# Current Metal viewer: Home and Club performance investigation

Observed 2026-11-22 on M1 Max, 64 GiB, macOS 26.5.2, Xcode 26.6 (17F113).
This is an investigation and proposed engineering sequence. No production code,
shader math, quality setting, commit or push was changed. PPD-319 remains open.
Raw evidence, exact private destinations, process receipts, settings, symbols and
reproducible reductions are retained locally under the ignored, private directory
`artifacts/metal/performance-investigation-20261122` (abbreviated **EV** below).

## Decision

**The scenes have different limiting sequences.** Home becomes predominantly
GPU-dependent as its lighting inputs populate; the render owner waits for older
required main commands. Club keeps the main CPU thread almost continuously busy
preparing and encoding a substantially more dynamic scene. Adding workers without
changing that work or its ownership is not a demonstrated remedy for either.

The first small implementation I recommend is **PPD-567: publish the frozen
forward-probe constant block once per consuming view/command invocation**, retaining
the existing immutable frame pages and completion ownership. Repeated hashing of
that same block accounts for about **2.17 ms of sampled Club main CPU per late
associated frame interval**. This identifies affected work, not promised FPS.

The larger Home opportunity is **probe evaluation in the existing candidate
shaders**. A separate optimized source-metadata capture attributes 66.40% of the
dominant base shader and 57.51% of the dominant simple-alpha shader to
`almetal_sample_base_probes`. Both accepted evaluation and containment are material.
This supports a focused probe-kernel experiment; it does not select an old
tile-SH/scalar prototype, prove spilling or justify reducing probe quality.

## What actually ran

The branch is `codex/apple-silicon-metal`, HEAD
`d219d538ab7880a76710f7844d5d071eb0e8cff5`. Its renderer/scheduling code checkpoint is
`960bd3644a09e6da8da57715947ef479fa16de18`; the intervening changes are documentation.
The `indra` tree is `7d8c095d360bd3f77ac64d1b69e23c21840e0011`. The tracked tree was
clean before this investigation; the unrelated untracked `worktrees/` was retained.

The baseline is the owner-accepted **Alchemy Metal Scheduling 26.4.0.69450** app in
`~/Alchemy-candidates/Alchemy Metal Scheduling 69450.app`, executable
`Contents/MacOS/Alchemy Metal Scheduling`. Full absolute paths and actual argv are
in EV process receipts. Its executable SHA256 is
`df28cec46e833d8eba36c38e45385a4ed469964cc2ae84a753a78a86ccd47335`, native arm64 UUID
`C505BD31-BD99-3274-8134-DE7ED4BD796A`; the retained dSYM matches. Selected Release
translation units use `-O3 -g`. A global multi-config `CMAKE_BUILD_TYPE` value was
not substituted for the selected compiler commands. Baseline metallib SHA256 is
`6fe71e2833331703acc4912f83f21c9e6ff0a14b7ef5fad70b1f75dccd688793`.

Startup **and first pixel-decode markers** identify OpenHTJ2K 0.27.0 as the sole
texture pixel decoder. OpenJPEG runtime 2.5.4 is present for separate header/encoding
support; it is not an alternative pixel decoder. No Rosetta or application-owned
OpenGL rendering path was used. `doctor.sh`, normal Release `build.sh build` and
`verify_app.sh` pass; exact commands, exit files and logs are in EV.

Normal baselines had automatic GPU capture off, no shader/API validation injected,
and bounded native telemetry on. Relevant actual process environment includes
`MallocNanoZone=0`; its origin was not found in the selected controller/launchd
environment, bundle environment or source setter. Its performance effect is
unmeasured. There is no supported configuration correction to make from that alone.
Native thermal samples report the nominal state; AC power was independently
observed after collection, not continuously sampled. Actual GPU clock frequency
was unavailable and was not inferred from utilization or the replay performance state.

All measured viewer runs used **native full screen**, foreground, at
**3456 × 2168** drawable/main target resolution (7.49 million pixels). The initial
windowed observation is excluded. The saved `FullScreen=0` is not evidence of a
windowed run: Mac initialization ignores that control and the native full-screen
window action was independently verified. No requested camera movement occurred
during the main recordings. Home was independently confirmed stationary by about
age 67.7 seconds; earlier loading is not labelled proven stationary. Club population,
asset arrival and parcel landing movement are uncontrolled scene inputs.

### Effective configuration and precedence

Settings load from compiled defaults and saved preferences, then startup/command
overrides and runtime consumers. These runs supplied scene/observer arguments but
no graphics override. Fifteen live controls were queried in the continuous baseline;
66 were queried in later GPU variants, matching those fifteen and relevant saved
preferences. Later queries are not falsely represented as 66 baseline observations.
EV `analysis-native-full/configuration-table.json` preserves each query's coverage
and original numeric-string storage separately from parsed interpretation.

| Option | Effective value and origin | Current consumer / effect | Activation evidence |
| --- | --- | --- | --- |
| Quality / distance | Stored quality 6; saved distance 112 m | `llfeaturemanager.cpp:407`, `llagentcamera.cpp:218`, `pipeline.cpp:18900`; preset and ordinary scene visibility | Live queries; no preset override |
| Retina / scaling | HiDPI 1, divisor 1, multiplier 1 | `pipeline.cpp:18634`, native target allocation | Native FRAME and captured G-buffer/SSR targets 3456 × 2168 |
| AA / filtering | Saved FXAA type 1, quality 3; anisotropy 4 | `pipeline.cpp:18657`, `9821`; `llappviewer.cpp:570` | FXAA encoder/shader present; quality enum is **not** MSAA sample count |
| HDR / shadows | HDR 1, detail 2, filter quality 2, scale 1, spot maximum 2048 | Native lighting/shadow pipeline selection | Live queries and shadow passes; actual map dimensions retained in captures |
| Reflections | SSR and mirrors enabled; probes requested 128, level 3, detail 1, face 128, supersample 2 | `llreflectionmapmanager.cpp:1287`, `pipeline.cpp:18590`; population and sampling are separate | Main captured shading packs: Home early 1 / late 39; Club late 13 |
| Hero probes | Resolution 256; actual hero count 0 in the examined main frames | Base/alpha constants and hero binding | Captured/native inputs; enabled preference alone does not establish use |
| SSAO | Enabled | Native light-map and bilateral blur | Actual light-map/blur encoders and shaders |
| Avatars | Maximum non-impostors 16, complexity 350000 | Avatar visibility/LOD consumers | Live limits, Club rigged rendering; no claim of a fixed repeatable avatar count |
| VSync / pacing | VSync 1; max frames ahead 3; pacing slack work 1 | `llviewercontrol.cpp:1003`, `pipeline.cpp:18390`; renderer separately limits required main groups | Native wait/command data; no evidence of a fixed 15 FPS cap |
| Target / yielding | AutoTune 0, TargetFPS 15, YieldTime −1, background yield 40 ms | `llappviewer.cpp:1777`; target is tuning input, background rule is conditional | Foreground baseline; MaxFPS is explicitly obsolete/unused, not a limiter |
| Main replies | MainWorkTime 1 ms plus safe-boundary pacing work | `llappviewer.cpp:5707`; deadline checked between callbacks | Regular drain exists; arbitrary callbacks are not safe inside mutable render waits |
| General | 1 worker from `ThreadPoolSizes`, constructor fallback 3 | `threadpool.cpp:187`, validated override at `225`; mixed background jobs | Actual startup width/origin |
| Decode / upload | ImageDecode max 6, normal admission 4, adaptive 1; native texture owner 1 | `llappviewer.cpp:2476`; sole pixel decoder and ordered upload ownership | Runtime log and actual worker upload posts, not just enabled controls |
| Optional native paths | Group sources, compact inputs, animated sources, join-static, main lighting renderpass, small lighting tiles and shadow overlap all 0 | Existing eligibility/ownership-specific paths | Complete later live snapshots; no stale override enabling them was found |

The 32 × 32 native lighting raster tile and **16 × 16 pixel probe-mask cells with
eight 32-bit words** are different mechanisms. A tile-light-map metric is a third
concept. None can be substituted for another when diagnosing candidate availability.
The upload/publication counts, retained-packet limits and two required main groups
are bounded scheduling/lifetime policies, not automatically defects. In particular,
the fixed-width upload owner preserves batch/flush ordering.
`alnativeframebudget.h:27` specifies 16 upload admissions/frame, 32 in flight,
32/64 MiB normal upload byte allowances; 16 scene publications/frame, 32 in flight,
8/32 MiB normal publication allowances; two preparation credits and 32 MiB retained
preparation capacity. An aged progress rule uses eight frames; oversized items have
progress exceptions. These policies can depend on FPS, but this investigation does
not establish credit denials/backlog age as the cause of settled throughput. The
decode-ahead one-eighth-spare-memory policy is explicitly initially unmeasured in
`alnativeframebudget.cpp:56`; it is separate from GPU upload credits.

## Recording coverage and accounting

| Recording | Exact PID | UTC bounds / actual retained coverage | Qualification |
| --- | ---: | --- | --- |
| Club pilot | 33661 | About 20 s; CPU 20.646 s / 27,575 samples; GPU 20.011 s | Useful matching symbols, positive target GPU families; native telemetry off |
| Home Overview | 36864 | 00:40:34.031–00:43:35.778; CPU 181.175 s / 229,498 samples; GPU 181.009 s | GPU max bucket-start gap 1.004 s; about 164.32 s retained in-world |
| Club Overview | 38547 | 00:53:31.682–00:56:33.303; CPU 181.010 s / 438,088 samples; GPU 180.987 s | GPU max bucket-start gap 1.066 s; about 164.55 s retained in-world |

The main recordings include pre-login time; it is excluded from world statistics.
The earlier as-used Club UI showed about 17.4 FPS / 61.3 ms before the pilot and
15.9 FPS / 65.5 ms afterward. These are rolling point observations with loading/
crowd confounds, not raw period statistics or a controlled profiling-overhead result.
The native world anchor is the end of the transition into `STATE_STARTED`, with
approximately 0.10–0.12 s transition uncertainty. Native timing uses exact integer
Mach time. Trace/header UTC alignment carries tens-of-milliseconds uncertainty
(about 33 ms sensitivity); a narrow calibration bracket does not remove that.
Native phase and CPU/GPU windows are explicitly joined rather than equating IDs.

Home's same-process Overview **off / on / off** control means were
65.683 / 66.170 / 66.672 ms, with 457 / 307 / 449 eligible native periods. Recorder
startup and exports were excluded. This single sequential bracket shows no isolated
large Overview penalty; drift remains possible. Native telemetry stayed enabled,
so its net overhead is unknown. Its writer used about 0.82% / 0.90% of one core in
Home / Club; that is observed CPU, not an incremental-overhead bound.

Both baseline FRAME iteration streams are contiguous. Other records have 264 / 298
contention drops; they are not lossless command histories. Club's 600-second native
cutoff is after the measured Overview, with 15 committed commands pending at that
later cutoff, not a demonstrated leak. Home's final footer records no pending commands.
Pre-login failures, a raw-size failure and a finalization timeout are preserved and
excluded, rather than relabelled successful measurements.

Two-second detailed Metal System Trace + Thread State requests were sufficient for
the wait question. The retained common Home window is 1.798 s. Club has an important
789 ms observer hole: target CPU samples, submission and GPU records disappear
together while native telemetry shows 15 continuing FRAME periods and 268 commits.
It is excluded, not interpreted as application/GPU idleness. Retained Club segments
are analysed separately. Present callbacks are retained, but display-table linkage
and handler time semantics do not establish scanout latency. Drawable wait tables
contain no rows; this is not proof of zero drawable stalls in every session.

### Frame statistics

These are **consecutive eligible native application-loop periods**, not physical
presentation/scanout timestamps. The strict late windows are exactly the time
windows used by the CPU/Overview reduction; counts need not equal Overview's
associated frame counts. Reciprocal mean is throughput over those periods, not
the mean of instantaneous FPS values.

| Scene / world age | Period count | Mean ms | p50 ms | p95 ms | p99 ms | Reciprocal mean |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Home loading 0–30 s | 797 | 37.609 | 36.282 | 51.252 | 57.756 | 26.59 Hz |
| Home 60–120 s | 1048 | 57.208 | 57.111 | 71.101 | 77.182 | 17.48 Hz |
| Home strict late 120.333–164.322 s | 725 | **60.587** | 60.048 | **73.151** | 78.846 | **16.51 Hz** |
| Club loading 0–30 s | 561 | 53.390 | 52.211 | 74.590 | 84.222 | 18.73 Hz |
| Club transition 30–60 s | 473 | 63.398 | 62.227 | 78.288 | 89.513 | 15.77 Hz |
| Club 60–120 s | 1109 | 54.026 | 53.896 | 60.999 | 65.155 | 18.51 Hz |
| Club strict late 120.559–164.548 s | 847 | **51.869** | 51.898 | **58.044** | 61.117 | **19.28 Hz** |

The previously reduced full native 120–180 s rows include approximately 15 seconds
outside Overview retention and are not substituted for this matched late baseline.
Native tails and later observer controls remain separately labelled evidence.

## Home: the limiting sequence

Home slows as scene/probe inputs arrive, without increased main render dimensions.
Late aligned sampled main CPU is **22.096 ms per Overview-associated interval**
(727 associations over 43.989 s), and main-thread one-core share falls to about
36.5%. A broader native 120–180 s observation records required-main thread CPU
17.588 ms and earlier admission wait 38.586 ms per recorded required-main pressure
observation. These are
different scopes/windows; they are not additive components of the 60.587 ms mean.

The dependency is visible in the short detailed follow-up: `reserveMainFrame()`
(`almetalworld.cpp:2812`) enforces the pending-main limit and waits for older
required lighting/post commands, two display tags behind the current owner.
There are 46 substantive waits totalling 878.976 ms, median 20.826 ms; 98.75% of
their union overlaps the waited GPU envelopes. The GPU-end-to-wait-return interval is
small (median 0.248 ms, maximum 0.364 ms). This supports **GPU-dependent admission
blocking**, not a worker-reply delivery bottleneck. Target vertex/fragment/compute
active-stage union is 90.86% of the retained short window; that is not occupancy,
all-engine saturation or memory bandwidth. The capture-enabled short recording is
dependency evidence, not a replacement FPS baseline.

The full-device early/late GPU captures include ordinary probes, uploads and all
device-queue commands created after capture start and committed before stop.
Original Home captured packs are
1 and 39 shading probes, not the later baseline pack sizes. The base encoder has
exactly **7,520,256 fragment invocations and three vertices** in both captures,
but isolated encoder counter cost rises **4.489 to 36.524 ms (8.136×)**. This is a
large increase per reported invocation, not more base pixels. The haze-labelled
encoder rises 13.195 to 48.272 ms while fragment count falls to 0.569× and vertices
increase to 5.816×. That scope includes later alpha/PBR geometry: it is not the
cost of the atmospheric-haze shader alone. Local-point, projector and SSAO-blur
costs are broadly stable in these Home comparisons.

Normal late shader profiling identifies base candidates 26.80% and simple-shadow
candidates 26.25% of aggregate shader cost; PBR pre-water is 6.15%, material 1.54%,
actual haze 0.59%. **The dominant shaders already use prepared masks.** Extending
missing PBR/material masks cannot be offered as the main explanation.

### Source-enabled attribution, kept separate from baseline

A separate private optimized metallib adds only `-gline-tables-only
-frecord-sources` to the same shader text/math/compiler options. Its staged app
retains CPU sections/data/dyld contents and UUID under documented signature-field
normalization, but has a new signature and executable hash. This is not a claim
of identical GPU native code or timing. Canonical metadata remains off and the
accepted app is unchanged. Strict/deep signing and 346 ARM64 app checks pass.

This diagnostic's late capture has **55 probes**, not the original capture's 39;
all 66 queried controls match. Xcode actually displays the embedded source and
profile call tree. In Overlapping / Medium mode:

| Shader / call | Base candidate shader | Simple-shadow candidate shader |
| --- | ---: | ---: |
| Root diagnostic cost / aggregate share | 37.73 ms / 29.70% | 30.39 ms / 23.92% |
| `almetal_sample_base_probes` inclusive | **66.40%** | **57.51%** |
| Accepted `almetal_base_probe_tap` inclusive | 27.78% | 23.03% |
| SH irradiance, nested within tap | 11.77% | 13.52% |
| Weight, nested within tap | 4.69% | 4.67% |
| Radiance, nested within base tap | 4.50% | Not separately transcribed |
| Containment `almetal_probe_holds` | 10.04% | 7.75% |
| SSR in base geometry | 1.91% | Not an equivalent alpha call |

Percentages are **inclusive relative to each shader root**; parent and children
must not be added. Compiler inlining/reordering produces repeated caller names.
The residual is not an exact exclusive rejected-traversal timer. Containment also
runs on accepted candidates; these percentages do not count successful/failed
visits. Nevertheless, both accepted evaluation and candidate testing matter, and
SSR is a smaller part of this specific base shader. This does not prove that all
early-to-late growth is caused solely by probe count. The early generic and late
candidate entries and probe counts differ. Material roughness/normals, accepted SH
contributions, SSR hit work and history may differ and are not proved equal. Both
original frames already have SSR enabled, limit 25,
sample ceiling 4, hero 0 and unchanged resolution.

Source constraints are explicit: `almetal_builtin.metal:6444` traverses candidates;
the mask rejection is at `6504`, containment at `6512`, and accepted tap at `6522`.
The 32 cap counts **accepted taps**, not visits; neighbors are bounded at 64.
SH evaluation (`6275`) and radiance/parallax (`6084`) depend on the receiver.
Manual-priority and conservative projected bounds can retain full-view candidates.
Complete irradiance cannot simply be cached once per probe without changing output.

Xcode's headline values reconcile numerically to the **sum of isolated encoder
counter costs**, not overlapping replay makespan. Raw CSV GPUTime is nanoseconds,
independently checked against UI milliseconds. The sums are used only for unit
validation. Detailed counter passes are measured in isolation, whereas shader
sample attribution uses the overlapping workload; neither becomes live FPS.
[Apple's counter guidance](https://developer.apple.com/documentation/xcode/analyzing-apple-gpu-performance-using-counter-statistics)
and [shader profiling guidance](https://developer.apple.com/documentation/xcode/optimizing-gpu-performance)
describe this distinction. Heavy shader spill fields are blank/unknown; there is
no new positive spilling diagnosis. M3-only shader cost graphs/heat maps were not
assumed available on M1 Max.

## Club: the limiting sequence

Club's aligned late main CPU is **51.213 ms per associated interval** (848
associations over 43.989 s), with roughly 98.7–98.8% of one core busy throughout
world phases. Native required-main CPU is approximately 49.974 ms in the broader
late observation, without the Home earlier-admission wait. A CPU render owner
prepares shadows/publication/instances, performs resource bookkeeping and encodes
commands while GPU work from earlier submissions runs. The owner remains the
strongest current throughput constraint; GPU work remains substantial.

| Late sampled CPU context | Home ms/association | Club ms/association | Accounting |
| --- | ---: | ---: | --- |
| `sortShadow` | 0.986 | **7.980** | Inclusive query/cull/state-sort context, not just sorting |
| `Command::use` / resource declarations | 2.590 | **5.282** | Inclusive; map insertion leaves about 1.190 / 2.822 separately |
| Native draw encoding | 2.432 | 3.654 | Inclusive, overlaps surrounding render context |
| Lighting-data setup | 0.620 | 3.051 | Includes hashing below |
| Repeated forward-pack CityHash leaves | **0.424** | **2.167** | Exclusive sampled leaves with exact ancestry |
| Source-cache retain | 0.421 | 0.843 | Inclusive; not a large cost merely because counts are high |

Club publication/dependency/observer contexts are approximately 2.708 / 2.456 /
2.169 ms; they nest and cannot be summed. Both scenes' source caches sit near
their 3,478,924,288-byte **logical estimated-retention cap**, with many rejections,
capacity evictions and new retains. Counts establish churn, not physical residency,
bandwidth exhaustion or a proven dominant CPU cost. Club's native dynamic vertex
upload work is much greater (about 5.317 GB logical bytes in the broader 60-second
late observation versus Home 0.989 GB); this is workload, not measured DRAM traffic.

Club's captured local-light scene count is 129 versus Home 9 (the separate late
baseline has 128). Its local-light cost is about
3.86× Home late with 3.60× fragment work and only about 1.07× normalized cost.
This is largely a **work-volume difference**, unlike the Home base increase.
Club shader samples include local-light fragment 17.85%, base 13.55%, SSAO blur
12.42%, skinned material vertex 6.21% and rigged spot-shadow vertex 4.95%. Those
shares describe the captured workload, not independent wall-time savings.

The detailed Club trace confirms mixed operation but has the observer gap described
above. Its retained 1.112 s / 0.832 s segments have main Running fractions 73.86% /
98.72% and target active-stage unions 52.95% / 74.16%; there is no justified pooled
whole-window utilization. A 211.705 ms block there and a 157.047 ms Home block are
specifically **Apple driver resource-allocation locks under Metal capture**, through
`IOGPUMemoryInfo`, native allocation and CaptureMTLDevice. They are not ResourcePlan
mutex waits. Their ordinary no-capture frequency/cost is unknown (PPD-568). One
separate 57.824 ms Home block remains unattributed. Baseline p95/p99 and the Club
loading transition establish unevenness, but no lock observed under capture is assigned as
the cause of those baseline tails.

## Ranked engineering proposals

These proposals preserve current quality unless explicitly marked otherwise.
Affected costs overlap; do not sum this list into a predicted frame-time reduction.

### 1. Publish frozen forward-probe constants once — PPD-567

**Finding/confidence:** demonstrated redundant full-block content hashing. The
49,216-byte snapshot is made once at `almetalworld.cpp:20064`, then repeatedly
published at `20564` and `21367` through `setLightingData:2417` and hash `2097`.
The exact exclusive sampled affected cost is 2.167 ms Club / 0.424 ms Home.

**Mechanism/experiment:** lazily publish one invocation-local `ALMetalWorldDataSlice`
on its first actual consumer, keep deduplication enabled, and rebind it at both
existing slots (1 and 3) and encoder restarts. Preserve exact bytes, ABI, shader
math and existing completion-pinned pages. Avoid a global/raw-stack-address cache.

**Risk/acceptance:** stale cross-view data, wrong offset/slot and early page recycle
are the risks. Prove at most one publication per consuming invocation, none for
nonconsumers, and distinct differing packs in simultaneously pending commands/views,
including caller reuse, abandonment, reload and standalone FrameData. Run existing
forward/candidate/transparency output checks plus a real differing-forward-pack
lifetime case; existing local-light-list tests alone do not prove it. Keep only if
matched Club total main CPU falls including all new overhead, with no p95, memory,
loading or image regression. Home can remain GPU-limited despite less CPU work.

### 2. Qualify actual probe demand, then select one kernel experiment

**Finding/confidence:** source-enabled attribution establishes probe work as a major
part of both dominant Home shader families. Accepted taps and containment both
matter; exact reject/accept demand and exclusive traversal remain unknown. Affected
replay shader costs are large, but live savings are unquantified.

**Smallest experiment:** a BASE-only bounded census on at most 1,024 receivers from
the preserved 55-probe draw's immutable G-buffer, constants, mask and grid. Reconstruct
receivers by the existing `almetal_builtin.metal:13345` rules, write one private
fixed-size record per sample with local counters and distinct indices, and read
only after completion. Count visits, mask/containment rejects, ordered accepted IDs,
box/sphere and manual/automatic paths, positive-scale SH calls, radiance lobes,
default contribution and termination. No fragment atomics or rendered-output changes.
Default probe 0 can contribute after nonzero accepts when automatic and below 32;
do not accidentally change that rule. Qualify boundary/manual/neighbor/cap cases
against the current traversal. These are workload counts, not timings, and BASE
results do not establish alpha's interpolated receiver distribution.

The reason to measure demand first is concrete contrary evidence: PPD-353 already
tested predecoded coefficients. Matching coefficient bits did not initially preserve
an actual BASE pixel; corrected variants later passed consumer parity but regressed
in 7,990 intervals each. New SH attribution includes parallax, normalization, four
reads, unpacking and polynomial work; it does not isolate decode cost or overturn
those results. After the census, test one identified repeated operation with exact
output parity and alternating restored controls, including all preparation,
transport and storage cost. No new representation is selected yet.

**Risk/acceptance:** preserve ordered selection, manual precedence, neighbor pivot,
accepted-32/neighbor-64/default-probe rules, half packing, parallax, weights, SSR/hero and
rigid/skinned alpha. Changed coefficient representation can increase traffic or
register lifetime. Require exact established parity, better complete affected-pass
cost under matched controls, then lower live Home mean/p95 at matched age/quality
and no Club regression. Old scalar/ambient/clone and tile-SH experiments were
rejected or unselected; their historical codegen/parity/geometry confounds remain
contrary evidence, not a ready implementation to restore.

### 3. Reduce repeated resource declarations before adding encoding workers

**Finding/confidence:** Club resource-map insertion leaves affect about 2.822 ms
sampled CPU; inclusive use is 5.282 ms. The previously fixed ownership temporary
and completion-time owner destruction outside the registry lock are already
present (`almetalresourceplan.cpp:250`). They must not be diagnosed again.

**Experiment:** count distinct uses/hits/rehashes and binding-cache invalidations,
then test one bounded reserve or redundant-declaration elimination at the existing
command boundary. Shared-registry contention is not established by PPD-568.
Additional encoders/workers may increase it; preserve generations/read epochs,
write dependencies, alias/COW and retirement.

**Potential/risk/acceptance:** 2.822 ms is affected leaf work, not a guaranteed
removable or exclusive wall budget. Accept lower total main CPU/frame time without
retained-memory/latency growth, with completion-before-submit, generation reuse,
error/abandon/retirement and GPU output checks intact.

### 4. Establish a useful immutable shadow/preparation job before parallelizing

**Finding/confidence:** Club's 7.980 ms `sortShadow` context is material, but contains
query settlement, culling, state sorting and shared-state mutation
(`pipeline.cpp:26532`). Native capture/encoding are neighboring owner work, not
automatically inside that function. It is not a divisible sorting kernel. The
measured narrower direct preparation is about 1.111 ms.

**Experiment:** isolate one numeric classification/transform or private sort with
immutable inputs/private outputs. Measure snapshot copying, queue age, dispatch,
join and ordered publication. Trial at most a coarse two-way split only where an
independent overlap window exists. Global camera/cull state and scene lifecycle
remain owner-controlled.

**Potential/risk/acceptance:** savings unquantified; 7.980 ms is only inclusive
context. Shadow/query ordering, stale ownership and copying/join overhead are the
risks. Require reduced owner wall time and live Club mean/p95 with correct shadows,
occlusion and loading. Existing high admission thresholds and failed small-job
experiments are contrary evidence to simply enabling more workers.

### 5. Explain publication/cache churn before enlarging budgets

**Finding/confidence:** logical capacity churn is established; its dominant FPS cost
is not. Direct cache-retain time is only about 0.843 ms Club / 0.421 ms Home; broader
publication/dependency work overlaps it.

**Experiment:** correlate eviction/withdrawal, producer revision/topology changes
and membership-refresh reasons. Distinguish real scene mutation from avoidable
revalidation. Change one invalidation/reuse opportunity or bounded capacity only
after the causal path is shown. Do not suppress required material/shadow updates.

**Potential/risk/acceptance:** savings unquantified. More retained logical capacity
can displace useful unified-memory work. Require fewer unnecessary publications,
lower complete owner cost, exact reentrant/in-flight-reader invalidation and bounded
memory. Queue counts alone do not justify raising upload/publication credits.

### 6. Qualify correctness/observer issues without mistaking them for FPS fixes

PPD-569 retains an early replay warning for generic G-buffer `diffuse_textures`
slot 3. The generic shader has **32 slots** and guards every sample with
`texture_index < texture_count`; CPU validates/binds only the active prefix. Thus
slot 3 is unreachable under this shader source when count ≤3, while count ≥4
requires it bound; the actual warned draw remains unqualified.
The shared PSO/encoder label covers 1,095 draws; the warning alone establishes
neither an executed invalid read nor its performance effect. Smallest follow-up:
inspect the exact preserved draw/function, vertex constant buffer 4 count and slot
3, then selector values if needed; use existing `[diffuse-bindings]` GPU tests and
a separate targeted Shader Validation run if still ambiguous. Do not change
texture binding policy from a static warning alone.

For PPD-568, first reproduce a material allocation stall without capture before
changing resource ownership or pooling. The captured driver stack is now known;
one 58 ms block remains unknown. Runtime queue IDs and scanout linkage are also
unqualified. Improving observer coverage is useful measurement maintenance, with
no demonstrated production FPS gain attached.

## Configuration corrections, quality choices and rejected shortcuts

No stale graphics launch/configuration correction explaining these results was
found. Native full screen was enforced for every measured run. All 66 whitelisted
stored graphics/thread/scheduling keys are unchanged before/after (18 explicitly
present, 48 absent in both); no restoration write was required. Credentials and
unrelated preferences were not copied into report output or reset. Normal staged
app identity and canonical metadata-off configuration remain unchanged.

General width 1 is real, but settled Club samples do not identify its queue as the
critical path. Upload owner width 1 is intentional. Existing texture-worker uploads,
decode-ahead, cancellation, bounded cache handoffs and PPD-556 retry progress must
not be reported as universally absent. The regular MainWorkTime setting is live
after an accepted repair; practical lost reply slack remains unproved. The current
earlier GPU wait is recorded separately in the frame budget. Running arbitrary
callbacks inside that active renderer wait
would risk reentrant scene mutation. Increasing pending frames changes latency and
memory, not the measured amount of shader work.

Reducing resolution, shadows, probe count, SSR or avatar quality would be separate
**quality/sensitivity experiments**, not quality-preserving repairs. No such
reductions were made. If needed later, change one nonpersistent value, verify the
actual affected resource/work and restore it. Removing an unused MaxFPS preference
is maintenance; it is not removal of a live FPS cap. The present request did not
authorize a broad preference/experiment cleanup.

## Evidence index and remaining limits

The report is reproducible from these private EV groups:

- `identity-before.json`, `release-flags-before.json`, `symbols/`, run launch and
  actual-process receipts: exact app/source/argv/filtered environment/decoder.
- `configuration-before/`, safe setting snapshots,
  `analysis-native-full/configuration-table.json`,
  `analysis-configuration-preservation/`: provenance, query coverage and unchanged
  checked preferences/app inventory/canonical metadata configuration.
- `home-overview-02/`, `club-overview-01/`, `analysis-cpu-full/`: retained CPU/GPU
  coverage, matching symbols, exact late associations and exclusive leaf ancestry.
- `analysis-native-full/` and `analysis-native-matched/`: Mach joins, cadence,
  losses/cutoff, strict Overview windows and clock sensitivity.
- `home-gpu-01/`, `club-gpu-01/`, `gpu-working/`, `analysis-gpu-counters/`: untouched
  all-device originals, working replays, positional CSV, unit reconciliation and
  captured-frame input confounds. Duplicate headers/rows are retained; one malformed
  Club record is explicitly uninterpreted.
- `analysis-mst-home-03/`, `analysis-mst-club-01/`, `mst-final-findings.md`: thread
  states, GPU/submission tables, exact create/commit-bracket joins (421/423 Home,
  536/538 Club, zero ambiguity), wait stacks, coverage gaps and limitations.
- `optimized-shader-source-diagnostic/`, `home-gpu-source-01/`,
  `gpu-source-working/`: separately hashed/signed source-metadata variant,
  same-quality live checks, original late pack55, source profile observations and
  a completed Xcode export with 770,148,040 bytes of embedded performance data.
- `ppd569-source-review.md`, `analysis-cpu-full/final-source-review.txt`:
  correctness contracts, implementation safeguards and historical contrary evidence.

Actual GPU frequency, DRAM bandwidth saturation, positive spilling, complete
display-to-scanout latency and zero-overhead native telemetry are **not established**.
No live matched performance patch has been tested in this investigation. The first
decision is therefore narrow: implement/test PPD-567 locally, and separately qualify
actual probe demand before selecting a kernel change. Keep baseline
resolution/quality, repeat matched age windows in both scenes, and retain rejected
results rather than letting another disabled experiment become production baggage.
