# Existing controls for a next retained-world investigation

Read-only source inspection,2026-11-19. No capture, viewer launch, tool preflight or profiling was performed. Bind any next recipe to the exact newly built executable and hash; this note does not invent its path.

**There is no existing retained-world-triggered capture or UI/LEAP capture arm.** The only production ALMetalFrameCapture factories are the launch-configured display controller and the private quiet controller. Both initialize once. The C++ factory is not a runtime control surface. The Advanced render benchmark menu calls gpu_benchmark(), not this controller. tools/metal/quiet_capture.py attaches an Overview recorder and records observation marks; its own source explicitly says it does not trigger the native frame captures.

The stale-world observation is late in beginMetalWorldUICompositionCapture, after that display's world commands have been created/submitted. Metal captures only commands created after native start and committed before stop. No rolling pre-event buffer exists. Consequently the current helpers cannot recover GPU work preceding the first retained-world notice. Event correlation can only be coincidental under existing timed triggers. A future event trigger would need to latch the CPU observation and begin a later eligible display before any commands; that is not currently implemented or requested here.

## Existing quiet route, without benchmark or selected-pass topology changes

Use a new existing owned absolute0700 directory. Parent must pass the exact qualified executable using its established owner-login launch route. The following is a launch environment recipe, not an executed command:

```sh
env -u MTL_SHADER_VALIDATION -u MTL_DEBUG_LAYER \
  -u ALCHEMY_METAL_FRAME_CAPTURE_DIRECTORY \
  -u ALCHEMY_METAL_FRAME_CAPTURE_MODE \
  -u ALCHEMY_METAL_BENCHMARK_CSV \
  -u ALCHEMY_METAL_STAGE_TIMING_FRAMES \
  MTL_CAPTURE_ENABLED=1 \
  ALCHEMY_WARNING_DIAGNOSTICS=1 \
  ALCHEMY_METAL_DIAGNOSTIC_DIRECTORY="$private_capture_directory" \
  ALCHEMY_METAL_DIAGNOSTIC_MAX_MIB=64 \
  ALCHEMY_METAL_QUIET_CAPTURE=auto \
  ALCHEMY_METAL_QUIET_CAPTURE_LATE_SECONDS=120 \
  "$viewer_executable"
```

- alquietdiagnostics.cpp68–131 reads these startup settings once. Unknown capture mode leaves telemetry active; invalid late seconds disables captures. Telemetry requires a new exclusive diagnostic.jsonl, not a pre-existing file. Legacy directory/mode must be empty/unset; either conflicts with quiet capture.
- llviewerdisplay.cpp1533–1564 calls quiet control before any commands. A normal view excludes snapshots/cube snapshots/HUD rendering, requires zoom1/subfield0. World readiness is STARTED, no teleport/disconnect/logout/exit, visible window and no pending resize. Cadence additionally requires focus and interactive mode. It does not require current world presentation, so retained-world UI frames can still be eligible.
- almetalquietcapturepolicy.h21–31: early at >=2s of first eligible world and >=4normal intervals. Default late requires age>=60s, two adjacent10s windows each >=1.35times initial mean period, and means within10%; otherwise90s fallback. The60..240seconds override sets both late minimum and fallback to that age. Example120 is time targeting only. The owner mark11:11:55.814 followed the first native world-publication marker11:09:58 by~117.8s; quiet-world origin was not observed in this run.75s was only the earlier diagnostic-snapshot-to-mark gap, not world age.
- The period metric is normal display cadence. A moving UI/nametags over a frozen world can retain normal cadence and never satisfy decline. Default90s fallback can consume the late attempt before a later freeze. An actual teleport can rearm one second scene generation; maximum2attempts per generation/4total, no rearm after failed capture or expiry. Each sequence expires300s after its first eligible world; the separate telemetry stream defaults600s from initialization.
- Capture starts once at eligible display entry and stops at the next distinct eligible entry, across all queues on the same device, including intervening probes and idle uploads. It does not wait for GPU completion. It is not one presented frame's GPU duration.
- Output is quiet-early-frame-capture/display-interval.gputrace or quiet-late-frame-capture/display-interval.gputrace, plus private metadata.jsonl. Second-scene output is under scene-2/. Directories are exclusively reserved; existing children never reused/overwritten. Source validation of private paths does not make Metal's URL-only path resolution atomic.

The low-instrumentation first step can instead set ALCHEMY_WARNING_DIAGNOSTICS=1 and leave automatic capture off. Optional native diagnostic telemetry uses the same private directory with ALCHEMY_METAL_QUIET_CAPTURE=off and no capture-enabled env. Fresh base-stop/hero identity facts should distinguish CPU preflight from GPU encoding before another capture is requested.

## Other existing route and limits

ALMetalFrameCapture::createIfRequested uses ALCHEMY_METAL_FRAME_CAPTURE_DIRECTORY, optional MODE=display/main-base/hero-forward, and MTL_CAPTURE_ENABLED=1 at launch. llviewerdisplay.cpp639–699 requires successful strict benchmark collection, observes commandTiming enabled then disabled, then60eligible intervals. Without a successful benchmark it never starts. The public configuration is startup-only; UI/LEAP has no rearm event.

MODE=main-base begins only after the hero/ordinary/legacy preflight, immediately before creating the owned base command. An early hero-input rejection can keep it waiting without a base capture; on its selected diagnostic invocation the outer lighting batch is deliberately omitted. MODE=hero-forward selects face1 after similar benchmark gating and captures only its actual forward command. Neither is the preferred mode for the suspected pre-base CPU failure.

almetalframecapture.mm records CPU request/return bounds around startCaptureWithDescriptor and stopCapture. These native calls can add overhead. Inspect quiet_start_request_monotonic_ns/quiet_start_return_monotonic_ns and corresponding stop fields; they are explicitly not GPU timestamps. A stopped file is not replay success or proof of command completion. Capture-enabled or adjacent disturbed intervals cannot establish FPS gains.

Quiet heartbeat cancels on invalidation/focus loss/sink cutoff and when an active interval exceeds5s (alquietdiagnostics.cpp179–183). That watchdog runs on app iterations; it is not a hard external wall-time guarantee if the main owner/native start-stop blocks. The64MiB diagnostic sink cap,8192record ring and600s cutoff do not cap .gputrace file size. Only the short interval and bounded attempt count bound requested trace work. Do not run a simultaneous sampler, Overview recorder, validation layers or second capture controller merely to infer this CPU branch. Source fixture history requires shader validation unset for capture; API debug alone passed historically, but both are unset in the lower-overhead recipe. Current host/new candidate capture support remains unexecuted.

[Freeze investigation and candidate evidence](world-turn-freeze-69377-20261119.md).
