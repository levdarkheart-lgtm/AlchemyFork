---
name: gpu-validation
description: Validate and optimize the Metal-native Alchemy viewer on real Apple Silicon. Use for Metal GPU captures, shader/texture/pipeline correctness, performance baselines, frame-pacing comparisons, CPU/GPU stalls, resource memory and runtime no-OpenGL verification.
---

# Native GPU evidence

Read `docs/metal/VALIDATION.md`, `PERFORMANCE.md`, `APPLE_SILICON.md`, and `docs/metal/benchmarks/scenarios.json`.

1. Inspect real Mac GPU/Metal capability with `tools/metal/doctor.sh`; identify device/system/build configuration. Do not use benchmarks from a different machine as M1 Max evidence.
2. For every scenario, keep source SHAs, quality, camera, avatar density, region/environment, texture cache state, power and display settings comparable. Run repeated warmup and >=300 measured frames per variant per scene. Collect real telemetry and save raw local data under ignored `artifacts/metal/`.
3. Use Xcode GPU Frame Capture, `MTLCaptureManager` (if instrumented), and Instruments, verifying pipeline events, attachments/load-store, hazards, CPU/GPU completion, present timing and textures. Use `tools/metal/capture.sh` only for tool discovery; it does **not** capture automatically.
4. Analyze supplied measurements with `tools/metal/perf_compare.py`. Never invent samples. Investigate changes in visual quality and GPU/CPU bottleneck attribution before claiming improvements.
5. Validate native-only condition via `tools/metal/verify_app.sh`, `audit_gl.py --mode strict`, runtime verification of viewer-owned GL contexts and Mach-O/framework inspection. Distinguish third-party browser subprocesses explicitly.
6. Maintain `feature-matrix.csv` evidence only for executed checks. Report failures as failures, not warnings. Underperforming components remain open work items with specific profile evidence.

Before a release-ready claim, verify every gate in `VALIDATION.md`; report unknowns, waivers and missing instrumentation accurately.
