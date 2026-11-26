# Native Aurora component qualification

This directory qualifies the production `ALMetalAuroraReconstructor`, not a
second renderer. The viewer has no Rust/wgpu runtime dependency. The pinned
offline translation and exact upstream mapping are in [translator](translator/README.md).

Build with the repository's normal Release configuration (`AL_BUILD_TESTS=ON`):

```sh
cmake --build build-aurora-native-release --config Release \
  --target almetal_aurora_validation almetal_tests --parallel 4
```

`almetal_aurora_validation` is deliberately not an ordinary CTest: its rendered
parity arm requires the complete, retained, independently generated perspective
palm inputs and wgpu OFF outputs. It checks every world, including warm-up,
before scoring. Output directories must be fresh. A shortened/repeated stationary
cycle does not reproduce the same history and is rejected.

```sh
BIN=build-aurora-native-release/almetal/Release/almetal_aurora_validation
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 "$BIN" checks /absolute/fresh/checks
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 "$BIN" replay \
  /retained/shared-perspective/sequence.csv world- /absolute/fresh/main \
  /retained/quality-warm360/off-main/results
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 "$BIN" replay \
  /retained/shared-perspective/reference-sequence.csv reference-world- \
  /absolute/fresh/reference /retained/quality-warm360/off-reference/results
```

Parity compares immediate native reconstructed **RGB** against the pinned WGSL
result. The independent limits are max0.001 (two half ULPs near1 for this[0,1]
fixture) and whole-stream MAE0.00001; these are port tolerances, not altered
foliage-quality bounds. The qualified port is bit-exact, so no tolerance is
consumed. Alchemy's final alpha is assessed separately because its intentional
jitter-adjusted linear glow resolve differs from Aurora's nearest-input alpha.

`checks` covers real generated reactivity for opaque/final color differences,
glow and nonzero-jitter alpha edges, output depth, descriptor reuse, allocation
plateau, abandonment, reset/recovery, invalid required inputs, three asynchronously
submitted worlds and teardown. The `[aurora-inputs]` rendered suite additionally
exercises actual forward-world opaque preservation, the borrowed main-batch
Aurora call, required display rejection, cancellation, prior presentation retention,
recovery, and glow-fed bloom/display/final presentation. Existing Temporal motion,
visibility and post suites retain their substantive coverage.

Score with the unchanged quality reducer and references:

```sh
python3 tools/metal/aurora-native/analyze_native.py quality \
  /retained/shared-perspective/manifest.json /absolute/fresh/main \
  /absolute/fresh/reference /retained/aurora-off-warm360.json /absolute/native-quality.json
```

The original recovered-area and18/36-world settling failures remain failures.
The paired PNG/crop and54-world motion previews are derived by `make_media.py`;
12fps is explicit playback speed, not measured renderer throughput. Float outputs
are authoritative; previews clamp and apply gamma2.2 for inspection.

For cost, validate the actual target size separately, then clear all Metal debug,
capture and injected-library variables before the optimized run:

```sh
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 "$BIN" benchmark \
  /absolute/fresh/target-validation 18 18 validation
"$BIN" benchmark /absolute/fresh/native-cost 360 360
python3 tools/metal/aurora-native/analyze_native.py cost \
  /absolute/fresh/native-cost/cost.json /absolute/native-cost-summary.json
```

The18 phase inputs are produced at2304×1446 and GPU-resident before timing, using
the same pinned C producer as the retained comparison. An untimed GPU copy stands
in for existing world producers. The timed command includes actual opaque-HDR
preservation, depth/motion/composition conversion, all Aurora passes, output depth,
and Alchemy's glow resolve. The production shared constant arena supplies uniforms;
there is no per-world staging-buffer API allocation. Explicit synchronous gates
occur only in this isolated benchmark, outside GPU and host encoding clocks;
none are added to viewer rendering. CPU encoding/submission and excluded waits
are separate. Descriptor payload, physical device footprint and cumulative typed
allocation counts have distinct meanings. This is component cost, not Home FPS.
