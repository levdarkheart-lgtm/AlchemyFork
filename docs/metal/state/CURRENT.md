# Current engineering state

**Observed:**2026-11-09, physical M1 Max/64 GiB, macOS26.5.2/Xcode26.6.
**Branch:**`codex/apple-silicon-metal`; renderer checkpoint below, source committed.
**Renderer code checkpoint:** `5a99ceb9ece582728339920658f1f0e75e6041e4`.
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

## Latest accepted runtime

All viewers are CLOSED. Historical23144/22915/21685 are exited.
Source-exact Release68556 is cf398b2fab, executable SHA256
`264e4fe2d7f2169195471be7cb6d415e8945d196e93f77ea62830a367c87f1aa`.
Metallib `5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`
is unchanged from68554/68550. Build/signature/349 ARM64 slice checks pass.
App: `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
It DOES NOT contain the new constant reuse/diagnostics committed as5a99ceb9ec.

Run03 at3456x2168/Ultra6, full screen before login, exact numeric Preset123,
Midday startup loader,28 live controls,120frames+300seconds warmup and300
exact-native frames:68554→68556 mean102.679→100.613ms,9.739→9.939 FPS;
p50101.876→98.969ms,p95119.074→114.090ms. Observed+2.05%throughput is small;
do not assign all of it to source changes. Camera stable, restoration passes.
CSV `artifacts/metal/private-68556-startup-20261109-03.csv`, SHA256
`2516022d2a5937f424630fca79630fdedd15c27b4cb7389d820d7ae3571d126a`.
[Complete runtime/evidence](archive/CURRENT-68556-capture-runtime-2026-11-09.md).

Separate30s CPU/GPU traces: main thread93.96%of one core, hero49.97%of main
samples, ordinary6.01%; target-viewer GPU union86.705ms/present, essentially
unchanged. Encoder CPU43.933ms/present excludes pre-encoder work. Families and
GPU channels overlap; do not add inclusive durations or use summed CSV command
spans as frame GPU duration. Main base17.218ms; atmospheric encoder17.139ms
includes528 alpha/glow/haze records and does not isolate haze arithmetic.
Main query results prove real rejection:118422 spatial skips/299frame intervals,
no pending/expired/duplicate polls. Median52400.5 physical draws/frame remains.
Driver allocation4.39–4.47GiB; discard bias4. Texture-pressure fix stays reverted.

CUA login/fullscreen works; world AX/screenshots time out. Runs01/02 are INVALID
FPS comparisons. Native startup recovery loads exact numeric camera preset and
headerless LLSD Midday sky selection. Graceful exit rewrote exact Midday asset;
old release logging cannot timestamp asynchronous completion during sampling.
New environment provenance addresses that gap. No screenshot/parity success
claimed. Temporary user environment/persistence state was restored after exit.
Private setup plan: `artifacts/metal/private-startup-setup-plan-20261109.json`.

## Active implementation checkpoint

- Root owns shared world renderer/pipeline/CMake/shaders/docs/integration.
- Local400B G-buffer constant-slice reuse is implemented, always restores slot4,
  only within one render/command. New raw-color/depth GPU fixtures and actual
  reuse counters: final Release/RWDI full suites each67291assertions/81cases pass
  with API/GPU validation. Logs `/private/tmp/alchemy-metal-400b-cache-final-*`.
  Microbenchmark shows repeated-state CPU savings and unique-state overhead;
  actual runtime hit rate/full-viewer FPS effect remain unknown.
- trace_recheck completed/froze effective/local sky provenance and explicit
  intentional infinite reverse-Z far-plane flags. Environment stays outside
  camera equality; CSV unchanged. Independent source review passes. Release
  benchmark suite644assertions/16cases passes in both configurations; product pending.
- draw_cpu_costs completed/froze almetalprobeworkload.h/.mm and focused test.
  Root integrated factory/view identity/capture site/CMake source.
  Default off; explicit private directory compiles tiny sampler before timing,
  captures64x40 actual G-buffer samples/probe constants AFTER collector disarms
  on same producing command, completion-only readback. No normal-render failure
  on diagnostic errors. Independent completion/file/integration review passes.
- shadow_contract independently reviews helper completion/lifetime/file safety.
  No concurrent shared edits/builds/GPU timings. Raw data remains local.

Final integrated source5a99ceb9ec: full Release/RWDI Metal API/GPU suites each
121201assertions/85cases pass. Focused sampler53910/4 passed; both benchmark
suites644/16 pass. Build logs /private/tmp/alchemy-metal-probe-workload-*;
source review approved. Doctor rerun0,5a99-doctor-20261109.log. Source-exact
product build/runtime remains next. Earlier no-matching-test invocation used
old executable before build finished; no result from it is accepted.

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
| M-014 Ultra20 FPS campaign | doing | Latest9.939 FPS, source/runtime work continues. |

Advanced views/effects: measured mirror optimization preserves
original views; complete parity remains open. M-012 native UI automation is
complete in its bounded keyboard/AX scope; current world AX has a runtime limit.
M-014 Ultra20 FPS campaign is doing, latest9.939 FPS. This is progress toward
native efficiency, not product completion. Strict no-GL, visual/feature parity,
prolonged stability and independent build/review evidence remain release gates.

## Exact next actions

Source/tests/review committed and pass. Configure/build/verify source-exact
product from5a99ceb9ec or docs-only descendant. Relaunch same location/fullscreen/Midday/Preset123 with
new sky provenance, historical scenario pbr-interior, private0700 workload dir.
Collect300resident frames, then wait sparse dump completion before separate
CPU/GPU traces. Restore temporary user state after graceful exit. Use actual
samples to accept/reject conservative ordered probe lists; do not forecast FPS
from fixtures. Continue toward goal; explain remaining bounds if infeasible.
