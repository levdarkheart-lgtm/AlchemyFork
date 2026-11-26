# Aurora reconstruction: worthwhile as an isolated optional quality prototype

**Retained post-stop evidence:** Aurora shows much lower continuing frame-to-frame variation and gradual detail/energy redistribution, rather than a broad persistent old-position palm trail. It remains farther from its own mature output after stopping, but closer to the common Native detail reference. All existing recovered-area and settling failures remain unchanged. Whether the local recovery is objectionable in real foliage is still a visual judgment.

**Actual-resolution cost:** Aurora OFF costs **10.996 ms median / 12.846 ms p95**, versus **4.128/5.702 ms** and **4.147/5.773 ms** for the bracketing MetalFX controls. That is a substantial **approximately 6.86 ms / 2.66× GPU reconstruction premium**, not a viewer FPS result.

**Decision:** worth **one isolated, optional native-Metal viewer prototype**, using sharpening OFF. The stationary/detail advantage warrants checking the actual palm/grass view, provided the prototype is budgeted at this measured cost rather than an assumed native-MSL speedup. This does not qualify production quality, default selection or a renderer replacement. No prototype implementation, viewer login or DMG is part of this evaluation.

The owner explicitly superseded the initial no-cost gate. The [original quality checkpoint](https://github.com/levdarkheart-lgtm/AlchemyFork/blob/c31ffd288bcc555891634fb02070b5d58fd75b34/docs/metal/research/aurora-reconstruction-evaluation-20261126.md), all original scores/bounds and raw evidence remain preserved. PPD610 stays open.

## GPU and host cost at 2304×1446 → 3456×2168

Every arm uses 360 measured samples after 360 warm-up worlds on the **Apple M1 Max**, macOS 26.5.2, AC power. Validation/debug is disabled during measurement. Medians are consistently recomputed from raw samples using the midpoint convention; p95 is nearest-rank. Native's original lower-rank `p50` fields remain preserved separately.

| Arm | GPU median ms | GPU p95 ms | Included adapter interval median/p95 ms | Host encode median/p95 ms | Host submit median/p95 ms |
|---|---:|---:|---:|---:|---:|
| MetalFX A0 |4.127750|5.702250|0 /0|.106521 /.219000|.017834 /.027792|
| Aurora OFF |10.996104|12.846334|.109187 /.707125|.286375 /.356250|.027979 /.046417|
| Aurora RCAS 0.8 |11.374166|13.104667|.109312 /.698542|.314209 /.385250|.030396 /.045625|
| MetalFX A1 |4.146937|5.773333|0 /0|.080854 /.198000|.015750 /.026208|

The controls differ by **+0.465% median /+1.247% p95**, far less than the observed reconstruction premium. Both native controls report nominal thermal state. These are four ordered component windows, not360 independent experiments or Home FPS. Host measurements are wall intervals, not CPU-cycle self-time. Completion/input-producer waits are separately recorded and excluded.

MetalFX's interval is native `MTLCommandBuffer.GPUStartTime..GPUEndTime`, containing classic MetalFX **and Alchemy's existing output-resolution glow-alpha resolve**. Aurora's interval uses supported M1 Max render-stage timestamps: earliest adapter/autogen start through the latest **color terminal or display-depth terminal**. It includes required R32→Depth32 and RG16-pixel→RGBA16-UV conversion, reconstructed-depth clearing, every unchanged FSR pass, output-depth generation and RCAS where selected. Component stage intervals overlap; the adapter column is **not** an additive exclusive shader cost. The first-FSR-stage interval alone would omit the preceding clear, so it is not the reported total. [Apple's GPU-time API](https://developer.apple.com/documentation/metal/mtlcommandbuffer/gpustarttime?language=objc) defines the native timestamps.

**One small boundary limitation is explicit:** wgpu performs its 208-byte per-world uniform upload before the first measured render-stage timestamp. CPU staging is included in host encoding. A separate fixed64-sample actual `queue.write_buffer`/submit calibration encloses that transfer between native GPU bookends: median.420250 ms, p95.460375 ms, observed max.521292 ms. This deliberately conservative envelope includes host waits/driver gaps; it is **not GPU-copy self-time, an additive measured pass, a universal bound, or another reconstruction variant**. A planning allowance of roughly.52 ms for it would still leave an approximately11.5 ms OFF component budget; that is not another measured median. Precise upload GPU self-time remains unobserved and does not reverse the cost decision.

[Exact cost summary](media/aurora-reconstruction-20261126/metrics/cost-summary.json) and [all 2,944 raw rows](media/aurora-reconstruction-20261126/metrics/cost-samples.csv) retain the primary 1,440 samples, initial pipelined 1,440 observations and 64 upload-calibration samples with clock domains, timestamps, waits and identities.

## Workload, correction and validation

This is Aurora's actual **WGSL port of FSR3.1** at `ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f`, using pinned wgpu `6c13013b3f9c8a7e45154d45d120ada0cbe55a23` on Metal. It is not a benchmark of a future native-MSL implementation. Vendor `fsr.rs` and all five WGSL files remain byte-identical; two generated **host timestamp-descriptor** substitutions observe the existing graph without changing reconstruction settings, pass order or shader math.

One shared [C input producer](../../../tools/metal/aurora-evaluation/workload/README.md) constructs 18 actual-size resident phases once: finite linear-HDR/glow color, static perspective planes/opaque MASK foliage at 2/4/12m, reverse-Z depth, zero unjittered motion, unit exposure and production 18-phase jitter. Native/Aurora fingerprints match for all 18 phases. This cost scene is not a new quality qualification and does not measure arbitrary moving transparency. It does not resize the 384×192 quality fixture. No enormous image dataset is exported.

Each algorithm receives the same untimed GPU phase-copy producer into fixed inputs. The viewer already renders its inputs, so this synthetic producer is not proposed runtime work. All initial generation/uploads, file I/O, pipeline creation, readbacks and deliberate host waits are outside reconstruction GPU timing. Native shaders come from preserved production library`5d10cbca…`; its exact glow resolve executes. Aurora generates its own reactive/composition data from identical opaque/final color and marker 0. RCAS is genuinely `None` or `Some(0.8)`, never `Some(0)` presented as OFF.

The initial three-outstanding-world run exposed **elapsed dependency latency**, not a query-index bug. A next world's adapter vertex stage began while its fragment stage waited on preceding-world resources; its apparent adapter interval grew to about 11 ms. OFF's approximately 23 ms per-world span overlapped other worlds, while terminal completions were about 11.33 ms apart. We retained that evidence and made **one** matched quiescent correction: one world outstanding, previous work and input-copy completion observed before timed encoding. Those waits do not enter the GPU/host encoding intervals. The isolated graph now measures approximately11 ms, rather than misreporting the overlapping 23 ms span as service cost. No production queue limit changed, and no further repeat followed.

Separate target-size Metal API/GPU validation passed before optimized timing, including 29,970,432 finite output components. Validation timings are not performance evidence. Apple clang 21.0.0, SDK 26.5 and Rust 1.99.0 Release are pinned; binaries are ARM64. CPU/Naga 5 tests, analysis 19 tests and the full repository tool suite are recorded in verification receipts. The [driver](../../../tools/metal/aurora-evaluation/run_cost_comparison.py), [protocol](../../../tools/metal/aurora-evaluation/workload/benchmark_protocol.json) and [offline reducer](../../../tools/metal/aurora-evaluation/analysis/cost.py) reproduce the bounded sequence/reduction.

## Storage and allocation behavior

The 18-phase synthetic pool is benchmark-only: its payload is **915.047 MiB** in every runner. It must not be described as algorithm history or a proposed viewer allocation.

| Exposed storage | Native MetalFX control | Aurora current port |
|---|---|---|
| Retained device total, pool included |1,403.594 MiB|1,405.594 MiB|
| Common pool observation |945.000 MiB actual textures|932.375 MiB after-input device state, including backend/upload residue; not a pure texture-pool inventory|
| Reconstruction/adapter inventory |171.375 MiB explicit fixed inputs/output/mask/exposure; additional 286.844 MiB opaque observed residual|356,253,696 B reconstruction textures +41,222,144 B adapter textures +43,296,976 B buffer payload; fixed inputs additional53,305,344 B payload|
| Setup-to-warm increment |Opaque scaler already created before pool; its internals are not individually exposed|496,205,824 B incremental device storage after resident inputs|
| Net warm-to-end growth |0 observed|0 observed|

These inventories have different boundaries. Near-equal gross totals do **not** prove equal algorithm storage. Native's residual includes opaque scaler history, driver/pipeline state and rounding; it is not an exact private-history inventory. Logical buffer payload is distinguished from physical texture allocations. Full inventories and units remain in the JSON.

Native has no additional application-owned GPU texture/buffer creation during steady state; opaque MetalFX transient activity is unexposed. **Aurora still creates one new shared Metal staging-buffer API object per world** for its 208-byte parameter write—360 during measured samples—even though it retires after completion and live storage plateaus. Physical pages may be reused by the driver. Metal HAL buffer/texture/view counters are live net counts; its byte/allocation counters are unimplemented zeros, not evidence of zero memory or zero churn. One-phase preparation staging is released before warm-up; query-resolution/readback buffers are allocated only after timing. No allocation-free claim follows.

## Bounded next prototype, not acceptance

The reconstruction premium is material and must be budgeted, not hidden behind the quality improvement. Nevertheless, the roughly 80% stationary-variation reduction, stronger retained detail and lower Native-detail error make one **optional** owner-scene experiment worth the cost. Sharpening OFF is the primary candidate; RCAS 0.8 is not needed to establish that benefit. Do not assume native MSL will be cheaper or that isolated GPU cost predicts world-frame throughput.

The next work item should remain one 67% native-Metal main-view prototype:

- Reuse existing linear-HDR input, depth/proxy depth, rigid/attachment/avatar motion and previous poses, jitter/camera conventions, output sizing, shadows and native-resolution UI.
- Reuse the existing main-post transaction, submission-only authoritative history, cancellation/error/reset and completion-retirement boundaries. Stage FSR parity/previous jitter until submission; do not mechanically advance them on abandoned encodes.
- Add bounded algorithm-owned history/mask/pyramid resources and native passes. Define the required opaque-before-transparency color input and motion conversion; retain independent-view/snapshot isolation.
- Reconcile FSR's nearest-input glow alpha with Alchemy's existing jitter-adjusted linear glow resolve, and keep depth-dependent consumers coherent. Their final integration cost is not measured here.
- Keep Native, Spatial, existing MetalFX, saved identifiers and defaults intact. No wgpu viewer dependency, Metal4 migration, frame generation, dynamic-resolution system or broad rewrite.

The live question is narrow: **does the real PBR MASK palm/BLEND grass remain visibly steadier with useful detail, and is its local move–stop recovery acceptable at the added rendering cost?** Existing area/settling failures, real-scene motion stability, actual native-MSL cost and viewer FPS remain separate acceptance questions. No new DMG or production change was made in this task.

## Unchanged quality context

At 256×128→384×192, the corrected 360-warm shared perspective quality run remains exactly as published:

| Original metric | MetalFX | Aurora OFF | Aurora0.8 | Original bound |
|---|---:|---:|---:|---:|
| Stationary edge variation |.00432862|.00086731|.00094805|≤1/255|
| Recovered recall |.877865|.936028|.958572|≥.90|
| Recovered area |.745180|.821401|.832826|≥.85|
| Output edge energy |.209369|.232006|.252639|≥.163276|

Both Aurora arms still fail area and stop18/36 own-reference bounds. [Original exact scores and all bounds](media/aurora-reconstruction-20261126/metrics/comparison.json), [activation](media/aurora-reconstruction-20261126/metrics/activation.json), [input audit](media/aurora-reconstruction-20261126/metrics/warm360-input-and-metalfx-maturity.json) and [reference maturity](media/aurora-reconstruction-20261126/metrics/warm360-reference-maturity.json) remain unchanged. Selected [stationary](media/aurora-reconstruction-20261126/visuals/stationary-matched.mp4) and [move–stop](media/aurora-reconstruction-20261126/visuals/move-stop-matched.mp4) previews and [lossless matched crops](media/aurora-reconstruction-20261126/visuals/stationary-phase0-frame396-3x.png) remain available; playback is not viewer throughput.

## Retained post-stop clarification (cost follow-up)

The owner has now authorized reconstruction-cost measurement despite the unchanged quality failures. This clarification uses only the retained corrected 468-main/414-reference-world population, not another quality run. Stop 18 is frame index 449/world 450; stop 36 is index 467/world 468. Both use jitter phase 17 and each algorithm's independent mature reference at index 413/world 414.

| Algorithm / stopped worlds | Difference from own mature output | Difference from common Native detail | Existing old-history residue |
|---|---:|---:|---:|
| MetalFX / 18 | .00828879 | .07950969 | .00041922 |
| Aurora None / 18 | .01588627 | .07565108 | .00033709 |
| Aurora 0.8 / 18 | .01693322 | .07004918 | .00057333 |
| MetalFX / 36 | .00325456 | .07827248 | .00022897 |
| Aurora None / 36 | .01231943 | .07375672 | .00007560 |
| Aurora 0.8 / 36 | .01305086 | .06823541 | .00023045 |

The first two columns are mean absolute linear-RGB errors on the **same 2,251 changed pixels** selected by the existing Native-reference movement mask. The Native column is the already recorded `native_quality_error`, not a new oracle or acceptance bound. Native is one unjittered output-resolution material render with binary alpha coverage and zero added feature mip bias. Its sampling locations, mip policy and filtering differ from temporal reconstruction; it is not ideal antialiased or temporally integrated ground truth. Aurora is closer to this particular Native detail render at both endpoints while remaining farther from its own mature result. Neither fact cancels the original recovered-area or response failures.

Additional offline calculations separate changing sampling phases from history evolution. Across the first and second complete 18-world stopped cycles, mean **adjacent edge variation** is .004418→.004424 for MetalFX, .000993→.000959 for Aurora None and .001097→.001054 for Aurora 0.8. Equal-phase pairs between those cycles change by .005570, .002805 and .003038 mean RGB respectively. These are descriptive values, not additional quality gates or independent experimental repetitions. [Exact response data and definitions](media/aurora-reconstruction-20261126/metrics/post-stop-response.json) preserve all 36-world reductions and original-score identities; the [offline script](../../../tools/metal/aurora-evaluation/analysis/post_stop.py) changes no original score or bound.

The evidence supports **slow detail/energy redistribution in an image that remains substantially steadier with Aurora**, rather than a return to MetalFX-sized adjacent-frame crawl. Aurora None's mean edge energy rises .19608→.20028 across the two stopped cycles, toward its mature .21907; 0.8 rises .21711→.22094 toward .23842. This is not simply a uniform contrast increase: mean green excess falls toward each Aurora reference while edge energy increases. Native-detail error also decreases. The response is still incomplete at 36 worlds, which is why the original own-reference response bound continues to fail.

The old-history diagnostic projects residual color toward the old image on **252 valid old-only pixels**, clips the result to [0,1] and does not inspect every possible trail. Its low means, plus the retained matched frames, do not show a broad persistent previous-position palm. Over all 36 stopped worlds the maximum individual projected fractions are .02561/.07800/.07521 for MetalFX/None/0.8, and none of those tested pixels exceed .1. This supports the limited statement above; it does not prove universally absent ghosting. Whether slow local detail redistribution is objectionable in actual foliage remains a visual judgment for a viewer prototype, not something the aggregate error alone establishes.

The existing [move-stop preview](media/aurora-reconstruction-20261126/visuals/move-stop-matched.mp4) contains 18 moving plus 36 stopped output worlds at **12 playback frames/s** (4.5 seconds of inspection playback). Each encoded video frame represents one supplied world; playback duration is not viewer elapsed time, latency or FPS. MP4 is a compatibility preview; retained linear-HDR float outputs and lossless stills are the measurement source.
