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

## Latest result — normal texture bias, still9FPS

Resource scope3969405 and capacity46d622 fix the all-device/image threshold
mismatch and replace arbitrary recommendation/16 with non-image reserve then
original headroom/divisor once. Scope-only diagnostic proved image-cap pressure
continued despite ample global memory. Both repairs preserve original controller,
manual overrides and global/system safeguards. Tests159/21pressure+29/3capability
each configuration, independent review/build/349ARM64/signature allpass.

68572full-screen Ultra/Midday/Preset123,3456×2168,43controls,all256probes ready,
300consecutive frames9637–9936:9.382155FPS,mean106.585ms,p50105.948,p95110.883,
bias1all300. Prior68566bias4was8.864746; differing image workloads prevent an
isolated speedup claim. Normal bias is sustained with7838MiB image storage.
Fullpixelquality parity remains open, and20FPS is not achieved or proven
impossible. Use this preserved app as the next native baseline.

Root owns shared edits/runtime. All viewers closed and user state restored.
Canonical68574/source74ee5d adds validated fragment-constant API support only;
full391587/90checks pass each config, build/349/signature0. It is unmeasured,
with production shader unchanged. No experiment overlapped formal collection.
[Complete evidence](../../state/archive/CURRENT-resource-aware-texture-budget-2026-11-09.md).

## Decisions and next implementation boundary

- Keep compiler, forward-binding, scope/capacity and existing correctness repairs.
  Do not restore lower texture quality to simplify a performance comparison.
- Use measured68572/bias1 for subsequent comparisons. Earlier no-discard trial
  stays reverted; source/patch/app preserved. Rebuild stale GPU test binaries.
- Existing query metadata proves main occlusion is consumed; no need repeat the
  missing-query audit. Compression/excess-forward-loop hypotheses were ruled out.
- Private classic/SSR specialization is not admitted:55/324strict failures and
  modern2.7–3.5%effects near control variation. Private material split preserves
  depth/water bits but has69/388strict failures and mixed performance: uniform
  5.9–12.5%faster, checkerboards2.7–4.6%slower. Neither is a default renderer
  improvement. [Evidence](../../state/archive/CURRENT-base-specialization-experiments-2026-11-09.md).
- Next change needs a specific substantial GPU cause. Do not repeat small
  variants or infer individual shader milliseconds from inclusive labels.
- The11hour status concern remains valid: tests/profiling alone do not achieve
 20FPS. Keep owner informed; no lower quality, travel or scope waiver.

[Architecture experiments](../../state/archive/CURRENT-ultra-goal-experiments-2026-11-08.md)
retain rejected layered results and actual M1 timestamp capability limits.
No capture/validation-enabled CSV is performance evidence. Raw captures remain
private; broad pixel/feature parity, multi-scene performance, prolonged stability,
strict no-GL and independent build/review evidence remain required.
