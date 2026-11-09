# 68561 native GPU capture and Xcode inspection —2026-11-09

Source83641ef8f82dd058c9d28f49b6568f98ac8e5bf3; physical M1 Max64GiB,
macOS26.5.2/Xcode26.6. Native capture diagnostics defaultOFF; no shader arithmetic
or quality/cadence changes. Existing per-face correctness repair7a2639ea4d retained.

## Validation and capture-only build

Both full Release/RWDI renderer API+GPU suites121227assertions/86cases pass with
capture off. Actual opt-in native capture fixtures1076/2 pass in both configs
with API validation and MTL_SHADER_VALIDATION UNSET. Release also passes without
validation. Initial capture tests with combined shader/API validation failed
native start; working configuration identified rather than hiding the failures.
Logs /private/tmp/alchemy-metal-frame-capture-*; independent helper/display/
path/lifetime review approved. Display syntax initially hit stale binary PCH,
then passed with identical textual include/Release flags; no hook source error.

Doctor, configure, build, verify_app349 ARM64 slices and deep strict signature0.
Product Release68561, executable SHA256
8168a914c9ec912e50cc743e3584fa00d843b8802b95985dc6c440df8bd95bbd.
AL_METAL_RECORD_SHADER_SOURCES=ON adds only -frecord-sources, preserving optimization
and fast math in separate shaders-with-source directory. Embedded metallib SHA256
9faffdd40ffb340029cb06c9f73715c605539ec713b65541487f7318d4460147,2083399bytes.
Default source-record option isOFF. No debug-source shader parity/FPS claim beyond
unchanged source math and passed renderer tests. Corrected normal68559 full app
preserved /private/tmp/alchemy-metal-68559-corrected-baseline.app.

Run artifacts/metal/qa-runs/candidate-68561-capture-only-20261109-01, PID79875
(now EXITED). Fullscreen beforelogin, unchanged location/Ultra6/Preset123/Midday,
3456x2168,28livegatepasses, stable camera, exact local/effective sky asset.
MTL_CAPTURE_ENABLED=1; API and shader validationunset; probe samplerunset.
120frames/300seconds warmup and300exact-native samples arm capture. CSV is
explicitly NOT_PERFORMANCE_COMPARISON: capture-enabled tracking adds overhead.
No FPS improvement/regression is inferred from this diagnostic CSV.

Capture parent artifacts/metal/private-68561-frame-capture-20261109,owned0700.
Exclusive child frame-capture contains metadata.jsonl and display-interval.gputrace.
One capture attempt: start2556,stop2557,error0,after60eligibleintervals. Stop
returned and4.2GB trace exists. Device scope includes allqueues/display/main/
hero/end-frame/ordinary/idle uploads. Only commands CREATED after start and
COMMITTED before stop enter capture. This is one logical display interval,
not a measurement of one present's GPU duration. No production waits/submission
changes. Temporary user environment/persistence restored after verified graceful
exit; private68561startup plan cleanupDONE. No private capture upload.

## Xcode inspection — commands available, performance profiling unsuccessful

Initial CUA getApp('/Applications/Xcode.app') unexpectedly blocked about74minutes.
Subsequent15000ms calls generally respond. Return dismissed What'sNew,CmdO and
CmdShiftG selected trace. Paste initially navigated/tmp; explicit PathTextField
setValue worked. Replay+Profileafterreplay was requested on sameM1Max/OS.

Trace loaded and API/resources are inspectable:29commandbuffers,129render,
10compute,37blit,51754draws,10dispatches,4.43GiB total (textures4.12GiB,
buffers313.55MiB,other8.09MiB),3456x2168/BGRA8. Preview shows current outdoor
scene/camera; this is not an original-vs-candidate image-parity comparison.

Automatic profile stalled85% with no timing result. An explicit Profile session
used displayed Overlapping/Medium defaults and also did not finish. Stop shortcut
CmdPeriod did not finish the GPU workload; actual StopGPUworkload button did.
Plain replay with ProfileafterreplayOFF works. A selected-draw context menu
GeneratePerformanceData also reached85% without results; whether it profiles
only selection is unverified. Stopped that workload too. Final UI says Finished
running macOSApp, trace remains open on Replay landing page, ProfileafterreplayOFF.
CUA completion summary sent exactly once: Inspected captured commands and resources.
One15s timeout reset kernel; rebind xcodeMetal recovered. No per-draw timings,
shader-line costs or completed GPU-profile claim. No active replay/viewer now.

Export button exports another GPU trace, not a summary report; cancel preserved
original. Summary static insights show799.40MiB memory opportunities,2.13GiB
bandwidth opportunities and65431APIinsights. These are advisory, not measured
savings. Unused main.post.ping.color0 is28.69MiB; other examples are old drawables
and bound-but-unused image textures. Lifetime tracing is required before removal.
No action was taken solely on these suggestions.

## Concrete captured command data

Grouped navigator by PipelineState (instead of APICall), filter final-color shiny.
Two renderer instances have distinct pipeline objects with same label. Hero
variant draws34436/92365 each3indices; main variant draws322028/322038 have
384/216UInt16indices,instance1; main opaque encoder103draws. No hidden giant
instance count was observed in those two commands. Actual viewport/scissor and
pixel coverage/per-draw timing are not established by this count.

Main shiny vertex/fragment functions are almetal_world_shiny_vertex/fragment.
Bound ordinary radiance cube-array267.39MiB,hero1024cube32.06MiB,SH32KiB,
BRDF1MiB, native4x anisotropic sampler labels and mainD32S8depth. Main frame data
and probe buffers are actually bound; raw target57.88MiB,depth36.86MiB.
Some attachment previews show invalid-data warning, so image contents are not
claimed valid replay readback. Typed112B hero data is inspectable:config(0box,
1active,9LOD,0),sphere approximately(2.620,.605,-11.44,r2.374),clipplane approximately
(-.682,-.175,.710,10.01). Display rounding prevents exact fixture reconstruction.

## Next source investigation, no product shader edit

Current shiny shader unconditionally walks ordinary probes BEFORE hero blend.
Full hero weight can overwrite ordinary result; algebra alone does not prove
Metal mix endpoint bitwise equality for finiteHDR. trace_recheck owns ignored
prototype /private/tmp/alchemy-metal-hero-overwrite-20261109.b3NS61: exact production
baseline source SHAe9345c0dd8f4a1b02624a2f5194144006f40e61ac9b37561d918021d9dcc8d56.
Only shiny call ordering/fullweight branch candidate; selection/default/neighbor/
cap paths untouched in partial/off cases. GPU correctness tests currentlyactive,
then counterbalanced timings only if parity passes. No product edits/benefit claim.
Root reserves GPU exclusively for that experiment; no reference viewer/testing.

Source-only zero-parameter check on corrected68559dump: ordinary6faces88probes
allambiance0; main44/hero88allambiance≥1, nofade/radiancescale0 in those samples.
This is not proof of a large general skip opportunity. Existing broad ordered
list and matrix memo candidates remain rejected.20FPS objective stillopen.
