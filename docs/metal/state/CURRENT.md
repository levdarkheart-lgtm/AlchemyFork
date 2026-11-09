# Current engineering state

**Observed:**2026-11-09, physical M1 Max/64 GiB, macOS26.5.2/Xcode26.6.
**Branch:**`codex/apple-silicon-metal`; renderer checkpoint below, source committed.
**Renderer code checkpoint:** `d1288129991e72b425c7dd60b2932f9fb1214f7b`.
**Recovery:**[next session](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[20 FPS campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Active owner goal

Work toward at least20 FPS on Ultra through general native Metal optimizations,
preserving exact-original quality and viewer functionality. Autonomous builds,
quit/relaunch/login and measurement are authorized. Stay at the current private
location/Preset123; do not move the avatar or visit other regions. Prefer Mac
keyboard shortcuts. Broader workloads use deterministic varied fixtures.
Apple guidance and architectural changes are allowed; preserve correctness and
accept measured benefit rather than assuming every native mechanism is faster.
The goal remains active;20 FPS and product release gates are not achieved.

## Latest completed runtime —68559

All viewers/profilers are CLOSED. Release68559/source7a2639ea4d built and passes
configure/build/349 ARM64/deep strict signature. Executable
219e7df316edf16f9f6ef3665e43c90bfc6d6ba45085c192389c25d0c0a5f98e;
metallib5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1 unchanged.
App build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app.
Current app EXCLUDES the uncommitted matrix memo below.

Same location/fullscreen/Midday/numeric Preset123,28 live controls,3456x2168/
Ultra6,120frames/300seconds warmup then300 exact-native/submitted frames.
68558→68559 mean99.366256→123.136859ms,10.063779→8.121045FPS;
p5098.375104→114.624521,p95114.118750→171.057208. Regression retained because
68558 used incorrect face-space probe data. No quality/cadence reductions.
Camera stable; all hero/HUD restorations true, local/effective Midday UUID exact.
48frames≥150ms,40clustered in one50frame block; draws increased only0.387%.
Actual constant reuse47.4795%,pool9/292.966MiB,discard bias4.
No screenshot parity claim. Temporary user state restored after graceful exit;
artifacts/metal/private-68559-startup-setup-plan-20261109.json cleanup DONE.

CPU separately recorded and fully exited before GPU recording: main89.4187%of
onecore (old94.1085%), reflection preparation89ms/30.4s (old10ms), too small
in this trace to attribute the entire regression to the added CPU call.
GPU viewer Active union86.3931→110.5173ms/present. Initial main opaque-post
label~0.36→19.2323ms despite identical103initial records; mainbase16.5766ms,
main atmospheric16.9401ms (includesalpha/glow),heroabove-water6.1180→12.2349ms.
ThermalNominal. Historical initial-post0.3/20ms anomaly remains unresolved;
per-draw/attachment inspection is needed before claiming a single shader cost.
All recording windows/exports ended before subsequent implementation work.
[Full68559 evidence](archive/CURRENT-68559-probe-view-runtime-2026-11-09.md).
Metrics /private/tmp/alchemy-metal-68559-metrics-20261109, CSV/provenance under
artifacts/metal/private-68559-startup-20261109-01.*, QA run same suffix.

## Active implementation checkpoint

7a2639ea4d restores both original per-view CPU updateUniforms calls before
native-face capture. Whole spatial/order/bucket/neighbor/scaling pack is rebuilt;
shader math and original quality/cadence stay intact. Independent source review,
product build and actual relative-space proof pass. New13records have13distinct
spatial packs (old allidentical).87matched hero probes have zero rotation-relative
residual; cross main/ordinary/hero max4.1438e-5 after accounting for translation,
versus old278–471 units. Depth buckets match. No absolute-origin/visual claim.

Root owns shared pipeline/world/CMake/shaders/docs. Matrix memo candidate was
REJECTED after meaningful tests: Release252assertions/4cases pass, hidden640/1,
but even repeated/null100k MV/MVP455.5→705us and inverse757.75→1064.08us regress;
all-unique inverse769→1911.29us. Direct SIMD is cheaper here. Uncommitted pipeline/
CMake integration removed; prototype+SHAmanifest preserved in
/private/tmp/alchemy-metal-matrix-memo-prototype-20261109. No production effect.

5a99 underlying Metal suites both121201/85, benchmark644/16, focused sampler
53910/4 passed;400B local reuse live gain small (68556→58+1.25%throughput).
Source/test logs /private/tmp/alchemy-metal-probe-workload-* and matrix-memo-*.

83641ef8f8 adds default-off Metal frame capture, now source committed/reviewed.
Both full Release/RWDI renderer suites121227assertions/86cases pass with API+GPU
validation/captureOFF. Real opt-in capture fixtures1076/2 pass in both configs
with API validation and shader validation UNSET; no-layer Release also passes.
Combined GPU shader validation blocks native capture start (initial fixture
failure retained), so capture uses a separate working configuration.
Display syntax passes after bypassing an old stale binary PCH (no source defect).
Logs /private/tmp/alchemy-metal-frame-capture-*.

Capture-only68561/source83641ef8f8 build/signature/349ARM64 pass. Executable
8168a914c9ec912e50cc743e3584fa00d843b8802b95985dc6c440df8bd95bbd,
source-embedded metallib9faffdd40ffb340029cb06c9f73715c605539ec713b65541487f7318d4460147.
CMake source recordsON, optimization/fast math unchanged. Diagnostic CSV is
NOT a formal performance comparison. Same28live controls/location,3456x2168/
Ultra6/Midday/Preset123, stablecamera;300resident rows completed. Capture started
frame2556, stopped2557, oneattempt/error0 after60eligibleintervals. Real4.2GB
.gputrace saved in artifacts/metal/private-68561-frame-capture-20261109/frame-capture/.
ViewerPID79875 now EXITED; temporary startup environment/persistence restored
(private68561plan cleanupDONE). No viewer live. Corrected68559 baseline preserved.

Xcode replay/profile is STOPPED; trace remains open on Replay landing page.
Plain replay permits command/resource inspection, but three profiling attempts
stalled85%; no per-draw timings or shader-line costs obtained. CUA completion
summary sent; all viewers closed and user state restored. Captured main shiny
draws384/216indices,instance1; no hidden giant draw count.29CB/129render/10compute/
37blit/51754draws,4.43GiBresources are static capture facts, not FPS measurements.
[Inspection record](archive/CURRENT-68561-capture-inspection-2026-11-09.md).
Shiny candidate d128812999 is COMMITTED: exact fullhero weight skips ordinary
legacy probes; partial/off/legacy selection and shared PBR/water/alpha unchanged.
Independent source/test review approved. Release existingfull121227/86 plus final
focused268745/1 and fullRWDI389972/87 pass API+GPU validation.24fixturecases×four
formats bitwise match frozen old shiny output, with actual GPU weight census.
Private fullhero synthetic~14→0.4ms; disabled/rejected~2–3%slower. Not liveFPS.
[Candidate evidence](archive/CURRENT-hero-overwrite-validation-2026-11-09.md).
Root owns shader/CMake/tests/docs/build; no GPU windows active. Next exactsource
normal build, source recordsOFF/captureunset, controlled300frame comparison.

Helper is off without explicit owned0700 directory. It observes timing, waits
for root's successful recorder completion+60eligible normal frames, starts all
queues of window device, stops at NEXT eligible display before commands. Nested
views don't truncate; ordinary/idle uploads are included. Only commands created
after start and committed before stop enter trace. No waits/submission changes.
Directory/session identity is rechecked before start; only own capture is stopped.
MTL_CAPTURE_ENABLED=1 adds overhead; completed68561 was capture-only, not FPS. Root owns CMake/display, agents' sources frozen/reviewed.

## Experiments and constraints

Layered shadows/named accumulators and always-on spatial masks regressed varied
workloads; not enabled. Ordered compact probe lists pass29pixel/counter fixtures
and58resolution pairs bitwise, speed sparse synthetic cases substantially but
regress dense/low-count cases. Actual per-view selectivity is unmeasured;
collect samples before designing admission. No product shader change.
[Latest isolated results](archive/CURRENT-probe-and-constants-experiments-2026-11-09.md).

Preserve native occlusion e52de13acc, literal Ultra4x anisotropy f799262971,
retired geometry generations e90ccdf18f, bounded lights/direct R32 shadow slices,
borrowed G-buffer depth, main-owned probe projector sharing, sampler/buffer
caches/frame arenas. Keep six Ultra hero faces/two sun cascades per face,
raw PCSS/reverse-Z/depth clamp/alpha casters. No quality/cadence reduction.
Exact original is1eb44bbd1bdc76d0100c75699bad1fb337e8cb45; external reference
67607 at validation HEAD734afd3179, not /Applications/Alchemy Beta.
Published alpha2 remains68512; no release/private upload authorization.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Native mirrors retained, measured optimization active; parity open. |
| M-012 Native UI automation | complete | Bounded keyboard/AX flow; world AX currently times out. |
| M-014 Ultra20 FPS campaign | doing | Corrected baseline8.121 FPS; shader experiment active. |

Advanced views/effects: measured mirror optimization preserves
original views; complete parity remains open. M-012 native UI automation is
complete in its bounded keyboard/AX scope; current world AX has a runtime limit.
M-014 Ultra20 FPS campaign is doing, corrected baseline8.121 FPS. This advances
native efficiency, not product completion. Strict no-GL, visual/feature parity,
prolonged stability and independent build/review evidence remain release gates.

## Exact next actions

1. Build committed shiny candidate, source-exact normal Xcode configuration;
   capture/API/shader validation unset, shader source recordsOFF. Verify/sign.
2. Repeat controlled normal300frame comparison with capture/source recordsOFF;
   preserve fixed location/Ultra/Midday/Preset123 and original per-view data.
3. Exact-original observer implementation/review is external/validation-only;
   no build or launch yet. Preserve approved67607 app before rebuilding.
4. Continue20FPS goal; no release/parity claim or private capture upload.
