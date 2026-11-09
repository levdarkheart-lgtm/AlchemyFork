# Ultra performance campaign — active owner goal

Owner instruction, 2026-11-08: improve the native Metal viewer toward at least
20 FPS on Ultra, preserving functionality/quality and using general changes.
Codex may quit, relaunch, log in, restore setup and measure autonomously. Stop
with an evidence-based explanation if the target proves infeasible. No token
or time budget was requested.

## Constraints and acceptance

- Live tests stay at the current private location and camera Preset123. Do not
  move the avatar or visit other regions. Use Midday, full screen before login,
  pinned Ultra controls, original shadow/mirror cadence and matching resolution.
- Try Mac keyboard shortcuts first; inspect outcome before using mouse fallback.
- Do not lower quality, omit required views/casters or tune to scene identities.
  Exercise low/high overlap, heterogeneous materials, geometry and view counts
  in deterministic renderer fixtures. The full live multi-scene release gate
  remains unexecuted; this goal does not waive PRODUCT/PERFORMANCE contracts.
- Aim for mean frame time at or below 50 ms over repeated resident 300-frame
  captures; report p50/p95, variability, exact source/settings and limitations.
  A transient counter or microbenchmark is not attainment of 20 FPS.
- Compare the exact external OpenGL reference at 1eb44bbd1b lineage when useful;
  retain no OpenGL fallback in the Metal fork. General performance benefit
  requires quality and functionality proof.

## Baseline and evidence

Renderer 6c694df237, Release68554, 3456x2168, current Ultra scene: paired CSV
observes 8.409 to 9.739 FPS, p50 117.437 to 101.876 ms, p95 133.511 to119.074 ms.
GPU main opaque cost varies approximately 0.3–20 ms across unchanged-shader
runs, materially confounding complete source attribution. Main thread remains
94.28% of one core; hero views are 50.44% of sampled main work. Eligible static
shadow draws coalesce about70%, yet all logical captures are still constructed.
Full GPU-validated Release/RelWithDebInfo suites each pass57,344/75.
[Complete runtime record](../../state/archive/CURRENT-68554-direct-shadow-runtime-2026-11-08.md).

## Active work and exclusive ownership

| Work | Owner | State / next proof |
| --- | --- | --- |
| Early static shadow admission/coalescing | shadow_contract | pipeline.cpp plus new almetalshadowcapture.h/test; preserve logical validation/count reconciliation and every family/view boundary. |
| Layered6/12-view static shadow experiment | draw_cpu_costs | General integration deferred:112 validated states/56 exact depth comparisons pass, but GPU and low-overlap costs regress. See experiment record. |
| Base-probe accumulator layout experiment | draw_cpu_costs | Ignored standalone production-shader A/B fixture compiled; no GPU timing yet. Preserve fused traversal and exact math. |
| Query/camera benchmark provenance | trace_recheck | Code frozen for integration; default-off unique ticket/outcome counters, actual main-camera hooks and private JSONL sidecar, unchanged CSV. Runtime tests pending. |
| Opaque pass workload census and integration | root | almetalworld.h/.cpp, shaders and CMake; capture family/count/state signatures before attributing the variable fragment cost. |

Only the integrator runs shared builds after file freezes. No heavy benchmark,
compilation or trace export overlaps a measured live frame window. Raw captures
remain local. Current viewer is closed for isolated experiments; its full
baseline bundle is `/private/tmp/alchemy-metal-68554-goal-baseline.app`.

## Decisions / next actions

1. Control camera, query effectiveness and opaque family workload; existing
   native occlusion and literal Ultra4x anisotropy are already implemented.
2. Remove repeated logical shadow record work while retaining full source
   validity and independent expected-source reconciliation.
3. Prototype layered native shadows only for compatible states and actual
   visible view instances. Reject low-overlap/extra-GPU-work regressions.
4. Inspect original shader/pass contracts for measured GPU costs. Do not repeat
   previously rejected haze arithmetic or treat nominal GPU bandwidth as proof.
5. Integrate successful changes, validate both configurations, build source-exact
   and repeat the fixed-location comparison. Record rejected experiments and
   remaining feasible opportunities before any infeasibility conclusion.

[Isolated architecture experiments and Apple guidance](../../state/archive/CURRENT-ultra-goal-experiments-2026-11-08.md)
record the negative layered result and supported M1 timestamp boundaries.
The atmospheric-haze encoder includes later draws; its aggregate GPU label is
not an isolated haze shader measurement. Opaque segment census must resolve this.

Current working-tree Release verification (2026-11-08): targeted shadow/census
9618 assertions/4 cases, full Metal66,962/79, render-resource/query2030/229
(one unrelated Noto single-palette skip), benchmark423/13 pass. Native suites
use MTL_DEBUG_LAYER=1 and MTL_SHADER_VALIDATION=1 on actual host GPU. Initial
restricted-process attempt returned no Metal device; host rerun passes.
Build/test logs: `/private/tmp/alchemy-metal-20fps-{release,rwdi}-*`. Source is
committed as d1966efd09654659e83c0f2df926a36e8d7f023c. RelWithDebInfo also passes
the same full Metal/render-resource/benchmark suites. Product build and actual
resident viewer comparisons remain. Obsolete native-only duplicate sets are
removed; source mapping and independent review are in the experiment record.
No product shader math changed.
