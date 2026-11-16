# Quiet-scene FPS decline — current diagnostic objective

The owner explicitly paused the original Apple-first architecture goal on
2026-11-16. The goal service returned `paused`; it rejected creating a separate
goal because this chat still had an unfinished goal. The owner subsequently
set the new diagnostic objective active through the app; `get_goal` confirms
its full text. Original architecture implementation remains paused by owner
instruction. This ledger records the new sole objective: explain the quiet Home scene's approximately 24–25 to 14 FPS
decline, with evidence identifying the growing critical path and its cause.

Engine source stays `92ddb0966c21677956eab3ecfe8dbee44a9ab813`, build 69187.
No new renderer change, viewer launch, login, profile or push occurred during
this discussion. Architecture implementation remains paused.

## Evidence and missing boundary

PPD-319 reproduces the decline: main on-core CPU falls while older GPU-group
waiting and associated completion latency grow. The display tail does not grow
similarly. Overview covers the quiet decline; detailed encoder evidence covers
only the later club. Do not project club pass costs onto the quiet scene.
PPD-318 retains collector startup/readiness/cost failures and the large AGX
trace-start observer effect.

24 FPS means approximately 41.7 ms/frame; 14 FPS means 71.4 ms/frame. Explain the
additional approximately 29.8 ms in the actual critical path. Overlapping stage
or buffered-frame elapsed intervals cannot simply be added to this budget.

## Capture method to prepare

1. Arm a bounded continuous startup timeline before the owner Login cue. Join
   raw frame IDs/cadence, CPU work and waits, command commit, GPU start/end and
   completion. Attribute main, probes, shadows and uploads separately. Resolve
   telemetry asynchronously without added GPU waits or queue-depth changes.
2. Automatically collect early, declining and late quiet windows from actual
   world startup/cadence. Acquire the early window while FPS is still high;
   manual post-login setup cannot arm it late. Use sparse physical-pass stage
   timestamp windows and workload/resource counts throughout the decline.
3. Save short complete-display Metal workload captures from early and late
   states, covering all application queues and verifying actual command
   create/commit boundaries. Xcode replay supplies per-draw/pipeline attribution
   inside the fused lighting encoder and available limiter/bandwidth/shader
   profiling. Preserve live timing as the authority for the decline: capture,
   replay and isolated counters can perturb execution and overlap.
4. Collect available GPU frequency/residency and thermal-pressure observations
   on the same clock. Compare work counts, target sizes, light/probe/SSR work,
   resource/upload/cache changes, execution and queue gaps. Distinguish added
   workload from slower execution or resource/dependency stalls.
5. If early capture disturbs settling, use separate early/late reproductions
   and a continuous capture-free timing control. Preserve graphics and normal
   rendering/worker overlap. After attribution, use a contained check of the
   specific causal hypothesis. Do not substitute speculative optimizations.

## Confirmed preparation gaps

- Existing debugger capture waits for successful benchmark completion and is
  one-shot with a 60-eligible-interval cooldown (`llviewerdisplay.cpp:652–675`,
  `almetalframecapture.h`). Existing stage timing also waits for formal readiness
  and cooldown (`llviewerdisplay.cpp:1102–1147`). Neither is a ready startup
  early/late collector.
- Real stage-timing windows exclude shadow encoding overlap
  (`pipeline.cpp:15176–15179`). Instrumentation must preserve the ordinary
  schedule before treating its timings as representative.
- Existing counter/capture code is the foundation; no new profiler framework
  is justified. Candidate CMake cache has `AL_METAL_RECORD_SHADER_SOURCES=OFF`;
  shader-line profiling needs supported source metadata in an optimized
  diagnostic build, preserving shader optimization and graphics settings.
- Installed `powermetrics --help` lists `gpu_power` frequency and `thermal`
  samplers; actual access/output remains untested. No samples acquired here.
- `xctrace record` lists start notification and bounded recording. Installed
  `xcrun --find gpudebug` fails; use the installed Xcode Metal debugger for replay.
  Verify M1 Max counter availability; newer GPU-only features cannot be assumed.

Diagnostic-only preparation is now in progress: independent agents own native
command telemetry, repeatable device capture/trigger policy, and the attach-only
external recorder. Root exclusively owns viewer hooks, build registration and
integration. No renderer optimization is selected. No viewer recording is
authorized by this preparation step.

Next action: diagnostic-only instrumentation/collector preparation and actual
start/stop/retention checks before an owner runtime session. Capture method is
specified; preparation, new runtime and causal explanation remain incomplete.

Success requires an early-versus-late critical-path explanation, exact source
and evidence, and a contained causal check. A suspect list, another club trace,
or preparation alone does not complete this objective.

[Existing findings](../../research/engine-checkpoint-69187-findings-20261116.md).
