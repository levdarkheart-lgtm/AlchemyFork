# Current engineering state

**Observed:** 2026-11-09 on physical M1 Max/64 GiB, macOS26.5.2, Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `d1966efd09654659e83c0f2df926a36e8d7f023c` — early
shadow admission, obsolete probe-set removal and opt-in query/camera/pass census.
**Continuation:** [next session](NEXT_SESSION.md), [active ledger](../work/active/metal-port.md),
[20 FPS campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Active owner goal

Try to achieve at least20 FPS on Ultra with general native optimizations,
preserving original quality/functionality. Autonomous quit/relaunch/login and
measurement are authorized. Live tests stay at the current location and
Preset123; do not move the avatar or visit other regions. Use Mac keyboard
shortcuts first and verify their result. Broader coverage uses varied fixtures
and source parity, not scene-specific branches. Explain with evidence if the
goal proves infeasible. The product release gates remain separate and open.

## Latest observed artifact and runtime

- Latest measured artifact:6c694df237 source-exact Xcode Release26.4.0.68554 under
  `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
- Executable SHA-256 `223bdc1054b9cdf73c336a26530431ded5eb4ca001de0339c74390ec41af93db`.
- Metallib SHA-256 `5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`,
  byte-identical to measured68550. Deep strict signing and349 ARM64 slices pass.
- Complete preserved baseline: `/private/tmp/alchemy-metal-68554-goal-baseline.app`.
- Both full Release/RelWithDebInfo suites pass57,344 assertions/75 cases with
  Metal API/GPU validation. These results precede current uncommitted goal work.
- Same full-screen-before-login/Midday/Preset123,3456x2168,21-control Ultra gate:
  fresh68550→68554 CSV mean118.916→102.679ms (8.409→9.739 FPS),
  p50117.437→101.876ms, p95133.511→119.074ms,300 exact-native rows each.
- Eligible static-shadow records coalesce69.86%; median55,647 physical native
  draws/frame remain. Main thread94.28% of one core; hero50.44% of main samples.
- Traced presents7.003→7.998/s; encoder CPU60.772→44.052ms/present. Pre-encoder
  preparation is excluded; inclusive CPU scopes and GPU channels overlap.
- Important confound: main opaque-post GPU cost swings0.3–20ms/present across
  unchanged-shader runs. Observed throughput gain cannot be assigned entirely
  to the code. Exact main-camera/pass/query provenance is the next control.
- [Complete latest runtime record](archive/CURRENT-68554-direct-shadow-runtime-2026-11-08.md).
- The viewer is currently closed for isolated experiments. No region change
  occurred. Do not confuse historical PID89607 with a current live process.

## Active implementation checkpoint

- `shadow_contract`: completed early capture implementation/review;
  early static-source validation/coalescing, independent logical reconciliation,
  every family/view barrier. Release build and real GPU raw-depth proof pass;
  CPU benchmark saves0.157ms/10k sources at70%joins, nonmerge−2.6%to+3.3% noise.
- `trace_recheck`: query pool, octree/spatial hooks, benchmark/display files;
  default-off unique-query/poll/rejection counters and private exact main-camera
  provenance and obsolete probe-set cleanup. Release/RWDI tests and pipeline
  syntax pass; independent cleanup review passes. Actual viewer proof pending.
- `draw_cpu_costs`: layered6/12-view general integration deferred after low-overlap
  CPU/GPU regressions. A standalone base-probe accumulator A/B is prepared;
  named accumulator A/B was slower twice and rejected. No product shader change.
- Root owns almetalworld/shaders/CMake/docs/integration and opaque pass census.
  No shared build while isolated timings or measured live windows are active.
- Native direct production path remains active; experimental ICBs stay opt-in.
  The earlier68553 ICB experiment regressed and is not a performance success.
- Committed d1966efd09: Release and RelWithDebInfo full Metal66,962/79 each,
  llrender2030 assertions/229 cases (one unrelated single-palette-font skip),
  benchmark423 assertions/13 cases pass. Metal API/GPU validation enabled on
  actual host GPU. Restricted-process no-device attempt failed before host rerun.
  Source-exact product/runtime validation remains pending. Logs are
  `/private/tmp/alchemy-metal-20fps-{release,rwdi}-*`; harness32 passes.
  [Experiment results/source mapping](archive/CURRENT-ultra-goal-experiments-2026-11-08.md).

## Preserved contracts and known limits

- Metal is the sole application-owned presenter/resource authority. No GL
  fallback or translation backend is introduced. Non-rendering features remain.
- Native scene occlusion was implemented by e52de13acc; conservative/exact
  visibility results and original issue order have GPU proof. Live rejection
  effectiveness is not yet measured. Hero cubes disable it as exact original.
- f799262971 honors literal anisotropy (Ultra4; disabled0→Metal1); no forced16.
- e90ccdf18f preserves selective geometry uploads and retired native generations.
- Bounded point lights, direct R32 shadow slices, borrowed G-buffer/depth,
  main-owned probe projectors, sampler/buffer caches and frame arenas remain.
- The texture-pressure separation7cd1993d8e remains reverted at owner baseline;
  discard bias4 was observed. Do not restore it without controlled evidence.
- Preserve raw-depth PCSS, reverse-Z, clamp, alpha and all caster behavior,
  six Ultra mirror faces and two sun slices per face. No cadence/quality win.
- Exact original reference is1eb44bbd1bdc76d0100c75699bad1fb337e8cb45; external
  instrumented OpenGL app is67607 at734afd3179, not /Applications/Alchemy Beta.
- Published alpha-2 remains26.4.0.68512-alpha2. No new release authorization.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Native mirror path works; original parity and larger view batching require validation. |
| M-012 Native UI automation | complete | Scoped keyboard/AX operation verified; current owner authorizes fixed-location tests. |
| M-014 Ultra20 FPS campaign | doing | Baseline9.739 FPS; native CPU/GPU and control work active; broader live gates unexecuted. |

The current work advances measured efficiency while retaining Metal-only
authority. It does not establish20 FPS, source-isolated multi-scene benefit,
complete visual/feature parity, strict source/runtime GL-zero or release readiness.
Open gates include prolonged stability, strict no-GL and independent build/review evidence.
Earlier details are preserved in indexed archives, including the
[pre-goal snapshot](archive/CURRENT-before-ultra-goal-2026-11-08.md).

## Exact next actions

1. Configure/build d1966efd09 (or docs-only descendant), verify/sign the exact
   artifact, then run the controlled current-location300-frame comparison.
2. Pin main-camera state and initial/later opaque workloads using new census;
   measure query acquisition/unique results/actual group skips, not polls alone.
3. Keep rejected layered/named-shader experiments out of production. Probe tile
   pruning remains a source-only candidate; actual scan-vs-tap cost is unknown.
4. Preserve Release+RelWithDebInfo proof; source-exact build and verified
   source-exact, verify/sign, then keyboard-first login at unchanged location,
   full screen/Midday/Preset123 and resident300-frame comparisons.
5. Continue general optimization toward20 FPS; document rejected approaches,
   remaining limits and feasibility rather than declaring success from a microtest.
