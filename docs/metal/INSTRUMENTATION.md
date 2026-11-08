# Renderer instrumentation requirements for autonomous Codex testing

This document defines the evidence contract and the implemented development-viewer collection route. The harness and analyzer do not fabricate GPU telemetry or authenticate a Second Life client.

## Implemented per-frame collector

Commit `3b8997edd7` implements an opt-in collector in `indra/newview/almetalbenchmark.*`; commit `14adac8c70` adds the per-frame native-wrapper command-buffer submission delta. The current collector also enables native command-completion timing only while an explicit run is active. The viewer calls the collector from macOS builds, including optimized download/Release builds, after a complete display iteration. With no environment request it allocates no recorder, installs no timing completion handlers and writes nothing. Previously the download-build compile guard silently omitted the collector from the measured Release viewer despite its benchmark environment; this prevented the requested CSV from being produced.

Arm collection with all three required values:

```sh
env \
  ALCHEMY_METAL_BENCHMARK_CSV="$PWD/artifacts/metal/candidate-run-01.csv" \
  ALCHEMY_METAL_BENCHMARK_SCENARIO="quiet-region-fixed-view" \
  ALCHEMY_METAL_BENCHMARK_VARIANT="candidate" \
  ALCHEMY_METAL_BENCHMARK_REQUIRE_EXACT=1 \
  ALCHEMY_METAL_BENCHMARK_WARMUP_FRAMES=120 \
  ALCHEMY_METAL_BENCHMARK_WARMUP_SECONDS=30 \
  ALCHEMY_METAL_BENCHMARK_SAMPLE_FRAMES=300 \
  "/absolute/path/Alchemy Metal Diagnostic.app/Contents/MacOS/Alchemy Metal Diagnostic"
```

- `ALCHEMY_METAL_BENCHMARK_CSV` must be an absolute path to a new `.csv` whose parent already exists. The collector refuses to overwrite evidence. Raw files under `artifacts/metal/` are ignored by Git.
- Scenario and variant are pseudonymous 1-64 character labels restricted to letters, numbers, dot, dash and underscore. Do not encode account, avatar, region or credential data in either label or the filename.
- Warm-up requires both thresholds: 120 eligible frames and 30 elapsed seconds by default. Either threshold may be configured as zero with `ALCHEMY_METAL_BENCHMARK_WARMUP_FRAMES` or `ALCHEMY_METAL_BENCHMARK_WARMUP_SECONDS`; samples default to 300 and must be nonzero. The elapsed gate prevents a high-FPS startup burst from completing warm-up before deferred, probe, mirror and streamed-scene work becomes active. The first eligible timestamp establishes both the elapsed gate and interval baseline and is not a measured row.
- Set `ALCHEMY_METAL_BENCHMARK_REQUIRE_EXACT=1` for the candidate. Every candidate row then requires the current frame's complete native world/UI texture to have actually committed to the drawable; a setting alone is not accepted as proof. A presentation-off GL reference uses `0`.
- Pre-sample discontinuity restarts warm-up. After the first row, a missing frame, ineligible frame, resolution/quality change, regressed submission/completion/timing counter or invalid clock interval fails and closes the run instead of combining unlike samples. The viewer logs arming, completion or one failure summary but no per-frame content.
- Eligible frames require `STATE_STARTED`, a visible window, no snapshot, teleport, disconnect or logout transition, and either GL or exact native presentation. Frame intervals use `std::chrono::steady_clock`.
- Each row is stamped with the CMake-configured Git commit and build type. Reconfigure after the source commit and verify the generated `LL_VIEWER_SOURCE_REVISION` before collecting source-exact evidence.
- The publication census is taken from the actual current-frame transaction,
  not from the default setting. `native_requested` records enabled Metal
  authority, `native_attempted` distinguishes a transaction that reached the
  ordered UI capture, `native_exact` requires a committed drawable,
  `gl_fallback` independently records whether a requested-but-unpublished
  frame actually exposed a GL swap. It is zero for native-only authority that
  retains the prior Metal drawable after a failed current-frame transaction.
  `native_presentation_state` identifies the boundary:
  `disabled`, `not_attempted`, `capture_started`, `capture_failed`,
  `surface_unavailable`, `surface_submit_failed` or `submitted`. Contradictory
  flags fail the run rather than producing ambiguous evidence; a fallback may
  only accompany a requested, unpublished native frame, but such a frame does
  not imply that a fallback occurred.
- The collector records wall/frame intervals, draw calls and `metal_submissions`: the delta of the process-wide count incremented after each successful `ALMetalCommandBuffer::commit()`. That delta includes all native-wrapper commits in the sample interval, including worker activity.
- During an armed run only, each newly committed native-wrapper command installs a completion handler. Monotonic `metal_completed` and `metal_timed` deltas distinguish completed commands from commands with valid `GPUStartTime`/`GPUEndTime`; `metal_gpu_ms` is the sum of those completed app-owned Metal command durations observed between two CPU sampling points. A mutex makes each cumulative timing snapshot internally coherent. The M1 Max physical regression proves one real blit produces one submitted, completed and timed command plus a positive duration, normally and with Metal API validation.
- Completion is asynchronous, so `metal_completed`, `metal_timed` and `metal_gpu_ms` can lag the frame that submitted the work. `metal_gpu_ms` excludes legacy-driver work and is neither whole-frame GPU duration nor pass attribution. Treat all four Metal columns as secondary diagnostics and average them only across a steady consecutive sequence. Use Instruments/Metal capture for per-label attribution and independent GPU evidence.

## Frame observability (real viewer process)
The implemented local CSV schema is:

`scenario,variant,frame_ms,frame_index,source_sha,build_type,drawable_width,drawable_height,quality_profile,draw_calls,metal_submissions,metal_completed,metal_timed,metal_gpu_ms,native_requested,native_attempted,native_exact,gl_fallback,native_presentation_state`

For a Metal System Trace captured separately, produce a deterministic labeled
submission summary with:

```sh
python3 tools/metal/trace_submissions.py artifacts/metal/example.trace \
  --output artifacts/metal/example-submissions.json
```

The tool exports the Instruments
`metal-application-command-buffer-submissions` table (or accepts an already
exported XML table), resolves xctrace's compact ID/reference encoding, and
reports per-label counts plus application event/encoder durations. These are
submission-table durations, not GPU frame time or a performance pass gate.
Rows for which Instruments omits a distinct encoder duration are counted and
reported explicitly; their application event time is not substituted as GPU
or encoder work.
Keep raw traces and generated summaries ignored until a redacted result is
intentionally selected for versioned evidence.

Future extensions may include `cpu_scene_ms,cpu_encode_ms,gpu_ms,dispatches,psos_created,texture_bytes,frame_in_flight,thermal_state` when reliably measurable. Here, a future unqualified `gpu_ms` would mean whole-frame GPU duration; it must not be populated from the narrower `metal_gpu_ms` completion-window sum. Do not fill unknown fields with zero; leave them absent and label that in the redacted summary. Do not mix clock domains when interpreting GPU events. Use supported Metal timing APIs/instrumentation for GPU spans when valid on the device.

## Correct collection
- Explicitly separate warm-up from measurement. Collect >=300 consecutive steady-state frames per scene per variant; capture repetitions and thermal/power/display state.
- Never log account tokens, avatar names, region addresses or other user-identifying data in versioned benchmark results. Scene IDs are pseudonymous labels.
- Ensure metrics are generated by the actual viewer executable at a recorded git SHA; no manual/AI-generated sample points used as product evidence.
- Keep visual and performance capture modes separate: Metal API validation and capture tools perturb timing. Benchmark normally with validation/capture disabled, while correctness uses captured frames.
- Rendered scenes should be deterministic or document sources of nondeterminism (avatar motion/network, camera path, streaming). No scripted server abuse.

## Programmatic Metal capture
On supported macOS versions use `MTLCaptureManager` with an explicit request (scene/time/frame) to capture a controlled frame; provide a user-visible opt-in only in development builds. `MTL_CAPTURE_ENABLED=1` can allow developer capture. Store `.gputrace` locally and record frame indexes, shader variants and draw/pass labels. Do not commit large captures or user-identifying textures.

## Automated quality checks
Create a reference/candidate image comparison mechanism over deterministic camera/scene captures; report color-space/format/scale, pixel diff distributions and screenshots with sensitive content redacted. Do not use a single global pixel-equality test for temporal rendering; define local tolerances for stochastic/animated effects. Validate asset/UI editing flows separately.

Capture the effective graphics contract after each isolated viewer exits, using
the shipped settings files in viewer load order followed by that run's profile:

```sh
python3 tools/metal/graphics_settings.py capture \
  --defaults "/absolute/reference.app/Contents/Resources/app_settings/settings.xml" \
  --defaults "/absolute/reference.app/Contents/Resources/app_settings/settings_alchemy.xml" \
  --profile "/absolute/reference-profile/user_settings/settings.xml" \
  --label reference --output artifacts/metal/reference-settings.json

python3 tools/metal/graphics_settings.py capture \
  --defaults "/absolute/candidate.app/Contents/Resources/app_settings/settings.xml" \
  --defaults "/absolute/candidate.app/Contents/Resources/app_settings/settings_alchemy.xml" \
  --profile "/absolute/candidate-profile/user_settings/settings.xml" \
  --label candidate --output artifacts/metal/candidate-settings.json

python3 tools/metal/graphics_settings.py compare \
  artifacts/metal/reference-settings.json \
  artifacts/metal/candidate-settings.json \
  --output artifacts/metal/settings-comparison.json
```

The comparison is fail-closed: every captured visual `Render*`,
`AlchemyRender*`, `AlchemyExposure*` and global-light key must exist on both
sides with the same effective value, and the required quality, mirror, LOD,
texture, exposure, lighting, shadow and probe keys must be present. Exit `1` means
the images are incomparable; fix the profiles and recapture before computing
SSIM, PSNR or performance claims. The manifests include source-path hashes for
provenance but no account or session data.

## Automation acceptance
A performance comparison is admissible only with real raw CSV. Use `tools/metal/perf_compare.py --enforce --require-provenance`: the provenance gate rejects missing source/build/display fields, nonconsecutive frame indexes, metadata changes within a scenario, baseline/candidate build-type, resolution or quality mismatches, and any candidate row without `native_exact=1`. Common optional `cpu_ms`, `gpu_ms`, `draw_calls`, `metal_submissions`, `metal_completed`, `metal_timed` and `metal_gpu_ms` columns are validated and summarized under `secondary_metrics`; they do not affect the pass gate. When the complete presentation-census group is present, the analyzer validates its internal consistency and reports per-scenario exact-publication and GL-fallback counts/rates plus state counts under `presentation_census`; those diagnostics also do not affect the performance pass gate. The analyzer still only proves that the supplied numbers and metadata meet those mechanical checks and performance targets. It does **not** prove images, hardware identity, data authenticity, power state, thermal equivalence or camera/scene equivalence; Codex must supply that evidence explicitly.
