# Ordered SIMD exposure histogram — 2026-11-17

The exposure histogram is a measured fixed GPU cost alongside the growing lighting, alpha and geometry costs in the [Home GPU phases](home-gpu-phases-69228-20261117.md). The new implementation reduces that compute cost on M1 Max while retaining the tested exposure outputs. It is not a resolution of the remaining Home decline or an accepted whole-viewer FPS gain.

## Source and retained behavior

Implementation commit `89c1a58838` is built from exact builder `7060967e3069f85d808d7b7ac6cca2bfef836dd2` as **26.4.0.69231**. Changes are confined to `indra/almetal/shaders/almetal_builtin.metal`, the exposure dispatch in `almetalworld.cpp`, and the existing real-renderer exposure test in `tests/almetal_gpu_test.mm`.

Pinned original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` constructs128×72×2 histogram contributions in parallel, then resolves64bins. The former native implementation instead scanned those18,432contributions in one GPU thread with two dynamically indexed private float[64] arrays. The actual Home capture reports560allocated spill bytes for exposure history and roughly4ms serialized cost. The512source-array bytes do not establish a byte-exact explanation of560 or traffic per invocation.

The new kernels issue one complete64-thread group. Every lane prepares one already-rounded meter cell per SIMD chunk; ordered broadcasts deliver each cell's ground then sky weight and EV to its bin owner. Each owner retains scalarfloat weight/log sums and executes the original `log += weight * EV` expression in the original contribution order.64float2 bins use512bytes of explicit threadgroup storage. A uniform barrier precedes thread0's unchanged ordered trim/adaptation/output. The active-lane bound also handles a partial SIMD group; actual hardware validation is M1 Max/width32.

The existing encoder validates actual supported group size. No new pass, externally owned resource, pipeline max-thread hint, CPU readback, feature toggle or history publication boundary was added. The meter and history remainRGBA16F, withfloat32 histogram arithmetic. All64bins, weighting modes, sky shares, binning, percentiles, key/range, asymmetric rates, finite fallback and existing native initial-EV0 bootstrap remain. Exposure epoch/reset/cancel/failure owners are unchanged; the original GL path is a reference, not a retained backend.

## Actual artifact qualification

Host: Apple M1 Max/64GiB, arm64 macOS26.5.2, Xcode26.6. Release C++/Objective-C++ object rules useO3/ARM64. Actual MSL options match the baseline exactly: metal3.1, ffast-math, macOS26.0 deployment, line tables and recorded sources; no extra optimization-level or precision flag was added.

- `tools/metal/build.sh build`: exit0,68.86seconds owner wall time.
- Seven relevant real-renderer/metadata cases: **75,059assertions pass**, API and shader validation enabled. Includes actual initial/history dispatch, a second adapted frame consuming pending GPU output within the same uncommitted batch, half-rounded prior EV, downward rate, restart, retained meter/debug display, completed water-depth metering, flare, batching and unchanged simple-alpha specialization.
- The **actual built** library passes236/236 exactRGBA16F output-vector comparisons against the packaged baseline, with79distinct baseline outputs and API/shader validation. Cases cover initial/history, all3meter modes, sky weights, partial/odd dimensions, bin/trim boundaries, dark/bright/subnormal inputs, delta/rates and nonfinite prior EV. This is bounded fixture parity, not exhaustive viewer visual acceptance.
- Actual built primary-mode cost:300dispatch samples per arm/150ABBA pairs, actual completed command GPU timestamps, validation off, no adjacent command-envelope overlaps. Median baseline **3.872167ms**, candidate **0.590303ms**; median paired change−84.7532%, **0/150adverse pairs**. These are kernel dispatches, not viewer frames or a critical-path subtraction. CPU encode/commit and owner-block wall remain separate.
- Earlier source-equivalent prototype controls for modes0/1/2 show−84.73%/−84.71%/−70.80% median paired changes,0/450adverse pairs. The actual production artifact is separately qualified above. Initial queued-command measurements with overlaps remain retained and excluded from isolated-cost qualification.

Current actual-built GPR and compiler Spilled Bytes were **not collected**; they remain unknown. Moving the private arrays and lowering execution time does not establish a new560→0compiler reading. Whole-phase traffic and [PPD-343's missing fields](../POST_PORT_DEFECTS.md) retain their separate scope.

## Staged candidate and checkpoint boundary

The separately staged69231 app has8272exactly matching non-root bundle nodes,346ARM64 Mach-O files, strict signature and static main-import checks passing. Executable SHA256`18fc28725dfebb5774beb5fa028378cb10829c0aba240b3c1ee32e8cbc061cde`; metallib SHA256`a5330e85acf8dc6a3624b054573b3c3445ccee95f905c6553bb4f0f48eeaac42`. Baseline69230 and all earlier candidates are retained. It includes the bounded normal-mode selected-PSO telemetry repair PPD-338.

Exact combined app: `/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/ppd345-exposure-integration-20261117/candidate-26.4.0.69231/Alchemy Metal Alpha G2G5.app`. Its source/staging receipt and the unchanged baseline identity are independently rehashed again in the connected checkpoint's `log-audit-v2/stage-identity-recheck.json`.

The first unattended normal-mode Home checkpoint has a valid baselineA1 but an invalid candidateB1: capability/inventory errors begin at login, scene population differs materially, simulator connection is lost near world+97s, GPU coverage stops and the later CPU loop floods the bounded native file. PPD-348 retains that incident; no A1/B1FPS gain is accepted. PPD-347 also records that the controller's SIGTERM exit skipped normal requestQuit/logout and final settings snapshots. Its contribution to the network failure is unknown. Those recordings/specifications remain intact; no renderer rollback or unrelated network rewrite was justified by the unproved attribution.

## Connected Home retest

Corrected private controller-v2 uses the existing public saved Login action only after recorder readiness, reads49explicit safe controls at world2s/100s/prequit, and requests normal viewer shutdown through LLAppViewer.requestQuit after recording. It reads no credentials. The launcher retains the same19architecture arguments and makes no additional control, scene or cache changes. Six pure lifecycle/framing tests and both identity dry-runs pass. Pre-release review caught late-login and nonzero-exit success races; both drafts and repairs are retained. New specifications also correct PPD-349's stale copied version/library labels and verify actual source staging receipt, Info.plist version, executable and metallib hashes before launch.

The subsequent runs are **B2 candidate69231** `/Users/dekstroza/Alchemy-home-aaul8w0j`, followed by **A2 baseline69230** `/Users/dekstroza/Alchemy-home-buin60t0`. Both collectors complete with exit0, both owned viewers exit0 through normal logout, and all six49-control snapshots are complete and equal by type/value. Dynamic exposure is1 throughout; legacy sky auto-adjust is0, a different setting. Both retain181positive GPU cadence buckets over approximately180seconds, without B1's cutoff. A2 has one18.94ms aggregate bucket-boundary gap at world0.018–0.037s; neither late comparison window contains it. No repeated B1-style AIS/event-poll/circuit failure appears. The source difference is confined to the histogram shader, exposure dispatch and GPU test.

Exact eligible native app-iteration periods, clipped to common retained GPU coverage:

| World window | Baseline A2 | Candidate B2 |
| --- | --- | --- |
|80–120s FPS|14.45045 (577periods)|15.04034 (601periods)|
|80–120s mean period|69.20200ms|66.48788ms|
|80–120s observed main wait|46.38444ms|43.56011ms|
|80–120s remaining period|22.81756ms|22.92777ms|
|120–167.606463s FPS|14.16207|14.90476|

Thus the candidate's80–120s period is2.71412ms shorter and its observed main wait2.82433ms shorter, with about4.08%higher FPS in this pair. Excluding the brief settings-query intervals removes four periods per arm and gives14.44929/15.04661FPS, essentially the same result. A wider99.5–100.5s exclusion gives14.44432/15.04038FPS. These use query start age plus elapsed wall/RPC duration; the initial shifted UTC-derived calculation is retained separately. Overview cadence independently agrees within0.02ms per arm. Main runningCPU is28.67%/29.20% of one core; sampled decode-workerCPU is0 in both, and sampled main pipeline compilation is0.004CPU-seconds each. Running-only CPU samples do not explain blocked time. Exported OS thermal state is Nominal throughout; this does not fix GPU clocks or prove equal power.

Native inputs retain83,018,106/85,710,349bytes with requested footers,227/228contention drops and no full/budget/I/O losses or pending commands. The event stream is partial, so observed wait totals remain bounds. All compared wait targets are known; the sole missing B2 target is outside retained Overview coverage. In80–120s,1,147/1,148baseline wait starts and1,192/1,192candidate starts target already-running GPU work owned two display frames earlier. Post-GPU-end wait is about0.531/0.512ms per period. This locates the measured late wait on the GPU completion path while retaining the instrumentation's limits.

Selected controls, quality6,3456×1916 output, ten points and47projectors with exact screen rectangle totals41,921,469/229,073,073 match. Each arm has39sampled main PSO observations/624batches with both intended specialization marginals covering every sampled batch. These remain bounded successful CPU-encoding observations, not an exhaustive GPU draw census or a fresh compiler spill reading.

Scene population is **not identical**: B2 has main probe counts32→43 versus A2's27→39; endpoint objects8,488versus8,048, textures6.460versus6.161GB and device allocation8.240versus7.931GB. A single sequential B2→A2 pair therefore supports a useful late GPU-completion-path improvement, without isolating its exact causal share or satisfying repeated matched-workload/owner acceptance. It does not establish that every kernel millisecond appears on the live critical path.

Both viewers still decline substantially during scene population. Overview early0–10/10–60/60–120s cadence is25.80→19.24→14.75FPS for A2 and27.47→19.48→15.27for B2. The fixed-cost exposure repair improves this checkpoint but does not remove growing lighting/probe/alpha work or resolve PPD-319. The earlier owner recollection of roughly14→17FPS after the selector/spill fix is retained separately; **both A2 and B2 already contain that fix**, so this pair does not isolate its gain.

Owner visual/runtime acceptance, repeated whole-viewer performance acceptance and explicit push approval remain pending. The wider Apple-first architecture remains paused behind the Home investigation; no release-complete claim.

Next engineering boundary is the growing lighting/probe/alpha work already mapped in the Home frame captures and PPD-344. Qualify conservative point-light radial/depth rejection and retained original projector behavior with contained image/cost controls before the next combined group. The current candidate and all evidence stay preserved; no additional unchanged viewer recording is needed to establish the facts above. Both viewers are closed.

## Private evidence

- `artifacts/metal/selector-followup-20261117/exposure-history-source-audit.md` and receipt: exact native/original ownership, hashes, quantization and reset/failure invariants.
- `artifacts/metal/ppd345-exposure-prototype-20261117/`: exact baseline/prototype/helper/flags,236case inputs and output bits, warmups, raw ABBA samples/adverse pairs, rejected queued-context data and manifest.
- `final-library-validation-20261117T094225.338184Z/` beneath that directory: frozen actual69231source/library/executable identity,236case parity and primary-only actual-built cost, qualified report and253-file raw manifest.
- `artifacts/metal/ppd345-exposure-integration-20261117/`: exact build, seven renderer checks and separately staged app/static receipts.
- `artifacts/metal/ppd345-home-checkpoint-20261117/`: A1/B1 launch/login/normal-mode telemetry, full logs, native/Overview/log audits and failed-candidate receipts. Account/location/session data remain private.
- The same private directory retains `controller-v2/` qualification/frozen sources/reference drafts, `controller-review/` root review and179tool-test log(one existing skip), `checkpoint-v2-scope.json`, and separate `native-analysis-v2/`, `overview-analysis-v2/`, `log-audit-v2/` derivations for the connected retest. Original raw recordings remain in their Home folders.
