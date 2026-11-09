# Performance contract: prove hardware benefit, not API ideology

## Reference and candidate
Reference: **unmodified ARM64 Alchemy open-source build at a pinned commit**. Candidate: same source lineage with native Metal. Record all source SHAs, build mode (`Release` or fullopt), macOS and Xcode versions, M1 Max GPU characteristics, RAM, display scale, power mode, thermals, viewer settings and source of data.

Verify effective compiler options for the actual C/C++ objects, including core
libraries, rather than relying on the configuration label. The native Xcode
cache once produced27of36 Release targets at `-O0` while mixed-language targets
used `-O3`; see the [diagnosis](state/archive/CURRENT-67611-original-and-release-flags-2026-11-09.md).
Preserve the completed build log and the response files it actually references;
stale response files elsewhere in the build tree are not current compile proof.

## Workloads
Use scenario IDs and descriptions from `benchmarks/scenarios.json`. Codex must determine reproducible lawful/consented ways to visit or construct comparison scenes and avoid automated load on public Second Life services. Same viewpoint, avatars, lighting, time, graphics configuration, texture/cache state and display resolution per paired run. Record minimum 300 measured frames per variant/scenario **after warm-up**, with multiple repetitions across steady thermal conditions. Capture separate cold-cache data; never mix with warm-cache comparisons.

## Metrics
- Primary: per-frame wall/frame time **p50 and p95 in milliseconds**, not headline FPS. Frame-pacing variance matters.
- Secondary: GPU frame duration, CPU render/scene time, draw/dispatch and Metal command-buffer submission counts, PSO compile stalls, allocated textures/buffers, memory throughput, working set, decode/asset cache load and frame-to-frame stalls; report if instruments cannot expose a metric. Submission count is pressure evidence, not GPU time or a pass criterion.
- Preserve render resolution, quality, material settings, avatar density, exposure/HDR and camera path. An apparent speedup due to reduced quality is a regression, not a success.
- Record sampled desired texture discard bias and the image/device pressure operands.
  Matching Ultra labels alone is insufficient: earlier native runs used bias4
  while the original used1. The [resource-aware budget record](state/archive/CURRENT-resource-aware-texture-budget-2026-11-09.md)
  restores bias1; its sequential FPS comparison does not isolate a speedup or
  prove per-texture/pixel parity.

## Initial product targets (provisional until baseline is actually captured)
- **Meaningful improvement target:** >=20% lower p50 frame time in >=3 of the four defined representative scenes; this is a *performance target*, not a prediction.
- **Regression ceiling:** no scene p95 frame time >5% worse than the corresponding Alchemy reference without documented root cause and explicit owner waiver.
- Investigate any CPU-bound scene rather than attributing lack of improvement to the renderer. A hardware native fork is not successful purely because the backend is Metal.
- These thresholds can be changed only by an explicit recorded product decision with measured evidence, not lowered to make CI pass.

## Data and reproducibility
Raw per-frame CSV input to `tools/metal/perf_compare.py` must have `scenario,frame_ms` columns; may include `cpu_ms,gpu_ms,draw_calls,metal_submissions,metal_completed,metal_timed,metal_gpu_ms`, the complete native-presentation census group and frame metadata. `metal_gpu_ms` is the asynchronous completion-window sum for app-owned Metal commands only; it is not interchangeable with whole-frame `gpu_ms`. Presentation-census counts and rates diagnose cutover coverage but do not decide the performance gate. Run:

```sh
python3 tools/metal/perf_compare.py --baseline artifacts/metal/baseline.csv --candidate artifacts/metal/candidate.csv --enforce --require-provenance --output artifacts/metal/comparison.json
```

Tool output includes sample counts, p50/p95, improvements and per-scenario source/build/display provenance. Common optional secondary columns are validated and summarized separately, without affecting the performance gate. `--require-provenance` also requires exact-native candidate rows and matching baseline/candidate build type, resolution and quality. It **does not generate or collect** telemetry. The development viewer's opt-in collector in `INSTRUMENTATION.md` supplies real wall/frame, wrapper-submission and app-owned completion-timing CSV; Instruments/Metal capture supplies separate labeled attribution and independent GPU evidence. Store raw captures in ignored `artifacts/metal/`; commit a redacted summary under `docs/metal/state/` with script version and SHA only when measured. Never invent results.

## GPU-specific hypotheses requiring validation
Tile pass merging/memoryless attachments; command encoder batching; cached PSOs; Metal compute paths previously disabled by macOS OpenGL 4.1; private/shared storage tradeoffs; reduced texture staging; pipelined readback; improved avatar/alpha draw submission; resource reuse. Profile before and after; reject any optimization that harms end-to-end frame time or quality.
