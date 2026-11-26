# Isolated Aurora reconstruction evaluation

This Apple Silicon-only command-line tool executes Aurora's pinned WGSL FSR 3.1 port through **wgpu's native Metal backend**. It is an offline evaluation dependency, not a viewer backend or production dependency. The runtime fails closed unless its selected adapter is Metal / Apple M1 Max.

Upstream Aurora: `ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f`. Its `fsr.rs` and five WGSL files are byte-identical in `vendor/aurora/`. `PROVENANCE.json` records source hashes, the pinned Aurora wgpu revision, compiler identity and the ordinary `Some(0.8)` RCAS source. GPL and FidelityFX notices remain alongside their source. Dependencies and exact revisions are locked in `Cargo.lock`; `DEPENDENCIES.json` inventories their declared licenses; the isolated manifest enables Metal, not Vulkan/OpenGL. Rust **1.99.0** was used successfully, rather than upstream's repository-wide 1.100 toolchain.

## Build and check

Run from this directory with Rust 1.99.0 on Apple Silicon:

```sh
cargo build --locked
cargo test --locked --test adapter_contracts
../../../artifacts/metal/aurora-evaluation-20261126/cargo-target/debug/aurora-eval adapter-checks /absolute/fresh/adapter-output
```

The local Cargo configuration uses Git CLI for secure dependency fetching and stores build products privately under `artifacts/metal/aurora-evaluation-20261126/cargo-target`. Compiler pinning is enforced by `build.rs`. CPU tests cover matrix/depth/motion/jitter math and validate all four concatenated WGSL modules with pinned Naga. The GPU check reads actual adapted depth and motion, upstream prepared/dilated motion, and upstream-generated reactivity: identity, two-axis camera fallback, explicit motion, finite near/far depth and clear0.

## Generate the matched perspective inputs

The preserved run rebuilt only the fixture translation unit and explicitly relinked the existing **b73abe1** native archive and shader library (`5d10cbcaefd65d98e50fb2894870a3364e30527b28b7dfd6addc7c7436c09e8a`), as recorded under private `cpp-build/`. It was not a fresh viewer build. The existing original quality fixture remains unchanged; the new `[aurora-shared-perspective]` case produces a separate perspective population for this comparison.

After building `almetal_tests` with the repository's normal Release test configuration, generate once into a fresh output directory:

```sh
ALCHEMY_TEMPORAL_FOLIAGE_OUTPUT=/absolute/fresh/shared-inputs \
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 \
/path/to/almetal_tests.app/Contents/MacOS/almetal_tests '[aurora-shared-perspective]'
```

The preliminary144/54 MetalFX control deliberately reports **four known quality failures** (10,198 assertions; exit42), while the substantive raster/motion/coverage checks pass and all generated files are retained. Those failures reproduce the target symptom in the newly matched population; they are not a successful quality qualification or a Home FPS result. Use the completed `shared-perspective/{sequence.csv,reference-sequence.csv,manifest.json}` and full per-world files for both reconstructors.

## Replay the complete generated stream

Inputs come from Alchemy's independently validated perspective reverse-Z fixture. Do not resize the old orthographic test or repeat one captured jitter cycle. Each invocation owns one persistent Aurora history and consumes **every** main world and independently generated final-position reference world chronologically. Counts are checked against the manifest: preliminary144/54, then the predeclared468/414 stream with360 warm worlds.

```sh
AURORA=../../../artifacts/metal/aurora-evaluation-20261126/cargo-target/debug/aurora-eval
SCENE=/absolute/path/to/shared-inputs/shared-perspective
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 "$AURORA" run "$SCENE/sequence.csv" world- /absolute/fresh/off-main --rcas off
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 "$AURORA" run "$SCENE/reference-sequence.csv" reference-world- /absolute/fresh/off-reference --rcas off
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 "$AURORA" run "$SCENE/sequence.csv" world- /absolute/fresh/ordinary-main --rcas ordinary
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 "$AURORA" run "$SCENE/reference-sequence.csv" reference-world- /absolute/fresh/ordinary-reference --rcas ordinary
```

The initial144/54 stream exposed continuing Aurora same-phase history evolution: independently warmed reference cycles18–35 versus36–53 differed by about0.0071/0.0077 in the stopped-comparison region (OFF/ordinary). Its stop errors therefore include immature-reference drift. The final, single corrective qualification predeclares **360 warm +54 stationary +18 moving +36 stopped =468 main worlds**, with an independent **360 warm +54 measured =414-world** final-position reference. Both methods receive all newly generated inputs chronologically. This is a convergence check, not a tuning sweep or a guarantee that360 worlds suffice. Preserve the preliminary run and inspect final-cycle drift before interpreting settling errors.

`off` passes `None`, while `ordinary` passes `Some(0.8)`; `Some(0)` would still execute RCAS and is not OFF. Each arm runs in a separate process. All upstream FSR environment overrides are rejected. Output directories must be new. `run.json` preserves every frame's matrices/jitter/reset, selected Metal device, requested/effective RCAS and a first-frame readback proving OFF output RGB equals accumulation RGB or ordinary output differs. Files `world-NNN-output.rgba32f` contain expanded half outputs, row order unchanged. This is linear HDR reconstructed RGB; output alpha retains upstream glow semantics and is not a coverage oracle.

### Input adapter contract

* Exact half-expanded `-input.rgba32f` becomes RGBA16Float, with alpha unchanged.
* Original R32 reverse-depth values are written to Depth32Float. No depth reinterpretation, approximation or rescaling.
* Original RG16Float previous-minus-current **input-pixel** motion becomes RGBA16Float `(0,0,-mx/width,-my/height)`. No extra Y flip or jitter is added. Zero vectors retain Aurora's camera fallback, including its `max(depth,1e-7)` clear-depth behavior.
* Actual column-major `projection_N` and `reprojection_N` arrays are supplied directly. The producer already conjugated Alchemy matrices for Aurora's UV-Y-down/NDC-Y-up convention. Reported jitter is actual image displacement in the unchanged raw rows.
* The accepted fixture is MASK-only: opaque and final bind the **same** color view, composition XY=0. Aurora generates its own reactive/mask data. MetalFX's supplied reactive map is never imported.
* The parser rejects missing/discontinuous worlds, wrong dimensions/phase/reset/exposure/depth contracts, nonfinite matrices/data and any float color that cannot round-trip exactly through half.

## Host-only adaptations

`pipeline_cache.rs` preserves Aurora's call surface with direct Metal pipeline creation (the upstream disk cache is inapplicable here). `renderer.rs` reproduces the allocation helper and adds COPY_SRC solely for explicit output/diagnostic readbacks. `build.rs` preserves the original vendor module, appending read-only host accessors. Its generated compile copy also replaces exactly two host render-pass `timestamp_writes` fields with optional benchmark timing hooks; [the exact patch](benchmark-host-timing.patch) is versioned. The original vendor source and all WGSL remain byte-identical. No filter, pass sequence, uniform, history, sharpening or dispatch decision is replaced. Quality runs leave those timing hooks inactive.

Input upload, file I/O, synchronous readbacks, first-use pipeline creation and CPU validation are qualification tooling. These process durations are **not** GPU reconstruction cost or Home FPS. The owner subsequently authorized a separate bounded cost comparison despite the unchanged quality failures. Cost does not turn a failed quality bound into acceptance; no viewer FPS conclusion follows from this runner alone.


## Target-size reconstruction cost mode

The optimized benchmark uses the shared C producer in `workload/` at **2304×1446 →3456×2168**. Eighteen exact phase inputs are generated and uploaded once, retained on the GPU and fingerprinted identically to the native MetalFX runner. Before each timed reconstruction, an untimed GPU copy places the selected phase in the fixed input textures. This copy stands in for the viewer's existing producers; it is not proposed integration work.

```sh
cargo build --release --locked
AURORA=../../../artifacts/metal/aurora-evaluation-20261126/cargo-target/release/aurora-eval
# Separate correctness run; not a performance observation:
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 "$AURORA" benchmark /absolute/fresh/validation --rcas off --warmup 18 --samples 18 --validation --quiescent
# In a clean environment WITHOUT Metal/WGPU validation or debug variables:
"$AURORA" benchmark /absolute/fresh/off --rcas off --warmup 360 --samples 360 --quiescent
"$AURORA" benchmark /absolute/fresh/ordinary --rcas ordinary --warmup 360 --samples 360 --quiescent
```

The parent comparison brackets those two unchanged arms with warmed native MetalFX controls. The benchmark rejects unoptimized/debug Rust builds for timed observations. The original pipelined mode permits at most three pending worlds. Its fixed inputs and adapter targets can produce previous-world resource waits inside elapsed stage envelopes, so those observations describe pipeline latency/service rather than isolated shader work. The one corrective `--quiescent` mode finishes each preceding world and waits for the resident input copy **before** timed encoding, leaving one world pending. These deliberate producer/completion waits are recorded separately and occur outside GPU timestamp intervals and host encoding/submission intervals. This is an isolated component measurement, not production overlap or viewer throughput. Every timestamp attachment is exercised during warm-up before its measured indices are overwritten.

M1 Max supports render-stage timestamp queries, **not** arbitrary encoder timestamps. Five observations per world cover adapter begin/end, first FSR stage begin, actual color terminal end (accumulate or RCAS) and display-depth end. The reported complete graph envelope uses the earliest observed beginning and latest terminal end. Signed stage gaps/overlap are retained; overlapping envelopes must not be added. All required FSR render passes, reconstructed-depth clear and the combined depth/motion conversion lie inside that graph envelope. The FSR-only pass envelope omits the preceding clear and is labelled accordingly. Intermediate passes receive no timestamps. Query resolution/readback, input construction/uploads, output readback and file operations occur outside reconstruction GPU intervals.

The source-defined 208-byte per-world uniform update is a separate `Queue::write_buffer` transfer before the observed graph. Its host staging is included in encoding time. A **single** separate `aurora-uniform-calibration FRESH_OUT` run measures 64 exact-API writes after16 warm writes, with completed native GPU markers before and after each. The resulting interval includes host waits, staging and scheduling gaps. It is an observed conservative envelope, **not** uniform-copy GPU self-time, a universal worst-case bound or an exact additive component.

Memory evidence distinguishes these quantities:

* Actual Metal `currentAllocatedSize` before inputs, after inputs, after warm-up and after measurement; texture-level `allocatedSize` records persistent FSR/adapter surfaces.
* Fixed descriptor-derived buffer payloads, the separate18-phase benchmark input pool, fixed input textures and post-measurement diagnostic query buffers.
* HAL buffer/texture/view counters are **live** counts, not cumulative allocation counts. The Metal backend does not populate its `buffer_memory`, `texture_memory` or `memory_allocations` counters; raw zeros mean **unimplemented**.
* The pinned wgpu implementation allocates a fresh shared Metal staging buffer for every `Queue::write_buffer`, including the208-byte FSR uniform update. Therefore one transient Metal-buffer API allocation occurs per world even when persistent resource counts and physical footprint plateau. Physical driver-page reuse is unknown. No staging-pool claim follows from a plateau.

This is Aurora's current WGSL implementation through the pinned Metal backend. It does not predict the cost of a future native-MSL implementation and is not a Home FPS benchmark.
