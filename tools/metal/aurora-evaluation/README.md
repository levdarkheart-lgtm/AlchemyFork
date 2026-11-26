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

`pipeline_cache.rs` preserves Aurora's call surface with direct Metal pipeline creation (the upstream disk cache is inapplicable here). `renderer.rs` reproduces the allocation helper and adds COPY_SRC solely for explicit output/diagnostic readbacks. `build.rs` copies the unmodified upstream module, appending only read-only host accessors for prepared motion, generated reactivity, latest internal color and effective sharpness. The original prefix and all WGSL remain unmodified. No filter, pass, uniform, history, sharpening or dispatch decision is replaced.

Input upload, file I/O, synchronous readbacks, first-use pipeline creation and CPU validation are qualification tooling. These process durations are **not** GPU reconstruction cost or Home FPS. Cost measurement is gated on passing the predefined image-quality checks in `analysis/`; no performance conclusion follows from this runner alone.
