# Quiet-scene FPS decline — current diagnostic objective

The owner explicitly paused the original Apple-first architecture goal on
2026-11-16. The goal service returned `paused`; it rejected creating a separate
goal because this chat still had an unfinished goal. The owner subsequently
set the new diagnostic objective active through the app; `get_goal` confirms
its full text. Original architecture implementation remains paused by owner
instruction. This ledger records the new sole objective: explain the quiet Home scene's approximately 24–25 to 14 FPS
decline, with evidence identifying the growing critical path and its cause.

Preserved baseline is `92ddb0966c21677956eab3ecfe8dbee44a9ab813`, build 69187.
Diagnostic-only engine is frozen at `fd5e8e088e1224e150e7ae71a17f827faa18249e`,
build 69200. Architecture implementation remains paused. The new native
instrumentation is integrated, built and separately staged; no viewer launch,
login, viewer profile or push occurred during preparation.

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

## Implemented and checked preparation

- The default-off bounded asynchronous JSONL sink records raw app iterations,
  explicit frame/view tags, command create/commit/GPU start/end/completion,
  existing command waits, CPU owner/join spans and workload/resource counters.
  PID/source/build and UTC-to-Mach calibration bind it to the actual process.
  Its producer never waits for a file, another producer or GPU completion;
  drops/cutoff/pending commands remain explicit. No benchmark timing or stage
  scope is enabled, preserving ordinary workers, batches and queue limits.
- Automatic device captures use real startup/cadence: early at 2 seconds and
  at least four normal entries; late after 60 seconds and two adjacent stable
  10-second slow windows (period >=1.35 times baseline, means within10%), or a
  labelled 90-second fallback. Maximum two attempts, one complete display
  interval each, heartbeat cancellation at five seconds, transient actual
  teleport/disconnect/logout invalidation and focus/cadence checks. All device
  queues are captured; actual create/commit boundaries remain inspectable.
- Attach-only `quiet_capture.py` requires exact PID/executable hash/source and
  real notification plus live owned raw data before the Login cue. Recorder
  parent status/finalization, size limits and actual CPU plus separate positive
  GPU metric-family ranges are checked. Requested native input must match PID,
  source and calibrated FRAME coverage. A cutoff cannot masquerade as success.
- Two real 3-second nonviewer recorder fixtures prove readiness/CPU retention;
  optional scheduler tracing proves blocked/runnable/preempted data but has
  approximately31x sampled raw growth. It is restricted to <=5 seconds rather
  than used continuously. These do not prove viewer GPU retention/overhead.
- Combined doctor/configure/build pass. API/GPU-validation native diagnostics
  and policy:1207 assertions/13 cases; completion/submission/main admission/
  lighting/shadow overlap:10161/24. Actual controller, each fresh process:
  default-off12/1, early-stop/invalidation45/1, collision25/1, cutoff33/1.
  Source full app-hook execution awaits the owner viewer session.
- Capture with API+GPU validation layers returned native NotSupported(1) in
  the actual controller fixture. The intended profiling mode with those layers
  unset passes all three native controller cases. Preserve both results;
  do not silently label the unsupported combination a successful capture.
- Static source3608/zero blockers/errors,346 ARM64 Mach-O/signatures/owned
  imports/104 retained feature rows and codec/media/voice provenance pass.
  All8273 staged app nodes match bytes/links/modes; strict stage signature pass.
  Optimized offline shaders carry source/line metadata; shader math/settings
  remain the baseline. Staged app: private
  `artifacts/metal/quiet-fps-diagnostic-preparation-20261116/candidate-26.4.0.69200/Alchemy Metal Alpha G2G5.app`.
- Xcode26.6 on this M1 Max replays the tiny retained native fixture and exposes
  counters/limiters, shader registers/spills, full MSL and source-line profiling.
  Replay stopped, original trace hashes unchanged. Actual viewer/render-draw
  replay has not occurred. Native timestamp envelopes are not hardware busy
  time, occupancy or additive frame costs.
- Tiny4KiB native blit prototype observer costs retain every adverse pair and
  both cohorts: utility writer owner median+0.205us/+2.57%, settled+4.332us/+2.12%.
  Their exact modified-unit/binary hashes are in handoff receipts; they precede
  final startup-only corrections and do not measure whole-viewer overhead.
- `powermetrics` MHz sampling requires root, unavailable to the current UID501
  session. NSProcessInfo thermal pressure is recorded at1Hz. Do not claim GPU
  frequencies were collected. Same-binary capture-off controls remain available
  if capture changes the decline or its timing.

Evidence root: private `artifacts/metal/quiet-fps-diagnostic-preparation-20261116/`:
`build-{first,second,third}.json`, `correctness/`, `static/`,
`preparation-receipts.json` and `next-owner-run.json`. Initial two compile
failures and first incorrect test-path attempt remain preserved. PPD-320/321
retain diagnostic lifecycle repairs; PPD-314/318 need actual viewer retest.

Offline `quiet_diagnostic_summary.py` is integrated separately from frozen
enginefd5. It reduces bounded raw native data into phase cadence, configuration
and capture exclusions, command-envelope unions, CPU spans and workload/resource
progression. Missing frame telemetry, losses and cutoffs remain explicit. It
does not diagnose shader causes automatically and refuses evidence overwrite.
Root final Python tool suite at ad213de8ec passes with the retained native stream
read unchanged; receipt/log under `tools-final/`.

Owner asks whether manual Xcode capture can replace scripted recording. Yes:
Xcode Debug Executable with GPU Frame Capture=Metal launches the verified app;
capture a small early and late frame set and export each .gputrace. Instruments
Game Performance Overview can record the continuous login-to-decline timeline.
Keyboard capture while the viewer stays foreground avoids focus-throttle
confusion; pause/capture intervals remain diagnostic overhead. No owner viewer
run has yet been performed or authorized by that question alone.

Next action: only on explicit owner instruction, launch exact staged69200 with
the prepared native environment, attach the recorder immediately, then cue
Login after actual readiness. Owner selects quiet Home and drives the viewer.
Collect the initial fast state and later slow state before any teleport.
The diagnosis remains incomplete until those measurements explain the change
and a contained causal check supports the explanation. Original architecture
work remains paused; no push is authorized.

Success requires an early-versus-late critical-path explanation, exact source
and evidence, and a contained causal check. A suspect list, another club trace,
or preparation alone does not complete this objective.

[Existing findings](../../research/engine-checkpoint-69187-findings-20261116.md).
