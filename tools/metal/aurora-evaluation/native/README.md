# Native MetalFX cost control

This evaluation-only executable uses classic `MTLFXTemporalScaler` on the native **Apple M1 Max**, followed by Alchemy's existing HDR RGB/glow-alpha resolve. It does not link the viewer or change production rendering. The resolve functions must come from the preserved `5d10cbca...` Metal library; the executable checks its complete SHA-256 before using it.

```sh
tools/metal/aurora-evaluation/native/build_metalfx_benchmark.sh /absolute/output/metalfx-benchmark
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 /absolute/output/metalfx-benchmark \
  --validate --warmup 18 --samples 18 --max-inflight 3 \
  --library /absolute/preserved/almetal_builtin.metallib --output /absolute/fresh/validation.json
```

The normal optimized run uses `--warmup 360 --samples 360 --max-inflight 3` and must have validation, capture and injected-library environment variables unset. Use the root comparison driver for the prescribed MetalFX/Aurora OFF/Aurora 0.8/MetalFX ordering rather than treating one control as a comparison. Validation output durations are not performance evidence.

The shared `workload/input_workload.c` producer is compiled once into each runner with `-O3 -ffp-contract=off`. It makes the same 18 target-sized, jittered, perspective color/depth/motion phases; per-phase fingerprints establish identical input bytes. All preparation and initial uploads occur before warm-up. A separate untimed GPU blit selects each resident phase into the fixed bound inputs before reconstruction, matching the Aurora runner's cache/input preconditioning. This dummy producer is not proposed viewer work: the viewer's existing main rendering already produces its inputs.

The timed native command contains the complete MetalFX operation and unchanged output-resolution glow-alpha resolve. Native MetalFX accepts the viewer's R32 reverse-depth and RG16 pixel-motion formats directly, so no extra reconstruction adapter is required. It uses fixed dimensions 2304×1446 → 3456×2168, explicit unit exposure, a zero reactive mask for this opaque/MASK cost scene, first-use reset only, and one persistent scaler/history. There is no sharpening or shader-policy change.

`GPUStartTime`/`GPUEndTime` preserve the timed command's actual execution envelope, not exclusive shader self-time. Internal MetalFX passes are opaque; core and resolve are not individually partitioned by inserting extra counter/barrier encoders. The raw producer GPU timestamps establish whether the dummy copy finishes before reconstruction begins. A separate `input_ready_to_completion_ms` span includes any GPU submission gap/transfer from producer completion to final output. It is distinct from actual main-command GPU elapsed time. The program records and flags producer/main overlap instead of forcing a hidden CPU synchronization.

CPU command encoding, commit, dummy producer work, and completion-capacity waits are recorded separately. Up to three required-world commands remain pending; capacity waits and final draining occur outside encoding/submission timers. No output file serialization or readback occurs during the measured interval. Optional validation reads final pixels only after all measured commands complete.

Memory records distinguish the common resident phase pool, fixed source inputs, the raw/resolved output targets, explicit texture allocation sizes, and the one temporary preparation staging buffer. Application-owned GPU texture/buffer creation counts cannot change during steady state. Device `currentAllocatedSize` snapshots additionally expose net/observed storage changes. MetalFX does not expose its private history or temporary resource inventory; these aggregate snapshots are not proof that its internal allocator performs no transient allocation.

The one corrective component-cost comparison uses `--quiescent`: it forces one outstanding world and waits for the untimed phase-copy producer **before** the CPU encoding clock begins. `excluded_producer_wait_ns` and its distribution preserve that wait separately. The reconstruction command and shaders are unchanged. This configuration removes the demonstrated previous-world/resource-wait latency from the component interval; it is an isolated reconstruction-cost observation, not a model of viewer submission throughput. The original three-in-flight run and binary remain retained separately.
