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

## Capture validity and cost

- Respect the current owner's launch/login/profile restrictions. Offline source
  and existing-evidence analysis do not authorize another viewer recording.
- For representative multi-minute scenes, use the qualified low-overhead
  Overview configuration. Inspect retention settings and actual exported CPU
  and GPU first/last timestamps and gaps. A requested duration, successful
  recorder exit or nonempty trace is not proof of usable coverage. Keep failed
  and truncated runs separate from valid replacements.
- Detailed Metal System Trace is a bounded diagnostic interval, normally 2–5
  seconds, alongside one complete-display frame capture when needed. Bound
  recording bytes/time, finalization and exports; preserve failure receipts.
  Do not record unrestricted detail for an entire multi-scene session or wait
  indefinitely on duplicate large imports. Never delete evidence without
  authorization.
- Running-only CPU samples cannot attribute blocked wall time. GPU in-flight
  spans and layer usage ratios are not hardware occupancy or isolated execution
  cost. Diagnose queue/admission waits, CPU starvation, GPU execution and
  drawable/presentation separately using a suitable timeline and thread states.
- Include ordinary probe work in a whole-engine frame capture. Counter replay
  can change pass overlap; use it to locate pass/shader limiters, then assess
  FPS and pacing without capture or validation overhead.
- Rolling LLStats period means are not raw wall-frame percentiles. Do not pool
  overlapping histories or fill missing transition frames with inferred data.
  The opt-in continuous wall-frame stream remains unverified until its actual
  app hook, output and runtime coverage have been tested (PPD-314).
- Component cost checks must retain the actual observations, run order,
  warm-up labels and adverse pairs. Label elapsed owner wall time accurately;
  it is not thread CPU time or viewer FPS. Stop after compile/link failure and
  verify the measured binary, source hashes and requested test/cell coverage;
  a preceding executable cannot supply new-source evidence.

Before a release-ready claim, verify every gate in `VALIDATION.md`; report unknowns, waivers and missing instrumentation accurately.
