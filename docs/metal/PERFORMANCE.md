# Performance contract: prove hardware benefit, not API ideology

## Reference and candidate
Reference: **unmodified ARM64 Alchemy open-source build at a pinned commit**. Candidate: same source lineage with native Metal. Record all source SHAs, build mode (`Release` or fullopt), macOS and Xcode versions, M1 Max GPU characteristics, RAM, display scale, power mode, thermals, viewer settings and source of data.

## Workloads
Use scenario IDs and descriptions from `benchmarks/scenarios.json`. Codex must determine reproducible lawful/consented ways to visit or construct comparison scenes and avoid automated load on public Second Life services. Same viewpoint, avatars, lighting, time, graphics configuration, texture/cache state and display resolution per paired run. Record minimum 300 measured frames per variant/scenario **after warm-up**, with multiple repetitions across steady thermal conditions. Capture separate cold-cache data; never mix with warm-cache comparisons.

## Metrics
- Primary: per-frame wall/frame time **p50 and p95 in milliseconds**, not headline FPS. Frame-pacing variance matters.
- Secondary: GPU frame duration, CPU render/scene time, draw/dispatch and Metal command-buffer submission counts, PSO compile stalls, allocated textures/buffers, memory throughput, working set, decode/asset cache load and frame-to-frame stalls; report if instruments cannot expose a metric. Submission count is pressure evidence, not GPU time or a pass criterion.
- Preserve render resolution, quality, material settings, avatar density, exposure/HDR and camera path. An apparent speedup due to reduced quality is a regression, not a success.

## Initial product targets (provisional until baseline is actually captured)
- **Meaningful improvement target:** >=20% lower p50 frame time in >=3 of the four defined representative scenes; this is a *performance target*, not a prediction.
- **Regression ceiling:** no scene p95 frame time >5% worse than the corresponding Alchemy reference without documented root cause and explicit owner waiver.
- Investigate any CPU-bound scene rather than attributing lack of improvement to the renderer. A hardware native fork is not successful purely because the backend is Metal.
- These thresholds can be changed only by an explicit recorded product decision with measured evidence, not lowered to make CI pass.

## Data and reproducibility
Raw per-frame CSV input to `tools/metal/perf_compare.py` must have `scenario,frame_ms` columns; may include `cpu_ms,gpu_ms,draw_calls,metal_submissions` and frame metadata. Run:

```sh
python3 tools/metal/perf_compare.py --baseline artifacts/metal/baseline.csv --candidate artifacts/metal/candidate.csv --enforce --require-provenance --output artifacts/metal/comparison.json
```

Tool output includes sample counts, p50/p95, improvements and per-scenario source/build/display provenance. Common optional secondary columns are validated and summarized separately, without affecting the performance gate. `--require-provenance` also requires exact-native candidate rows and matching baseline/candidate build type, resolution and quality. It **does not generate or collect** telemetry. The development viewer's opt-in collector in `INSTRUMENTATION.md` supplies real wall/frame and wrapper-submission CSV; Instruments/Metal capture supplies separate GPU timing and labeled submission evidence. Store raw captures in ignored `artifacts/metal/`; commit a redacted summary under `docs/metal/state/` with script version and SHA only when measured. Never invent results.

## GPU-specific hypotheses requiring validation
Tile pass merging/memoryless attachments; command encoder batching; cached PSOs; Metal compute paths previously disabled by macOS OpenGL 4.1; private/shared storage tradeoffs; reduced texture staging; pipelined readback; improved avatar/alpha draw submission; resource reuse. Profile before and after; reject any optimization that harms end-to-end frame time or quality.
