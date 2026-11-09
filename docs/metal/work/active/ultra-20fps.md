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

## Latest result — no established FPS gain

59177fixed empty Release flags; main CPU91.476→72.768% of one core, yet measured
68566was8.865FPS versus settled68564's9.270FPS.62e2forward binding repair passes
full391162assertions/89cases EACH Release/RWDI; shader bytecode unchanged.

Combined binding+opaque PBR no-discard trial68568/source d3d139passes all392926/
89production API+GPU assertions each configuration and source-exact app checks.
Actual Ultra/Midday/Preset123/fullscreen/43controls,3456×2168,all256probes ready,
bias4,300consecutive frames:9.193FPS,mean108.776ms,p50108.786,p95113.496.
One window is3.57%better mean than68566 but below earlier68564; no isolated or
repeatable gain established. The trial is reverted by d967, with source/app/
patch preserved for future general testing. Default68569 rebuilt/verified but
unmeasured; it keeps the confirmed compiler and tested binding fixes.

Root owns shared edits/runtime. All viewers closed; user state restored. No
profile overlapped the latest formal window and none is pending. Agents finished.
[Complete evidence](../../state/archive/CURRENT-68566-68568-performance-trial-2026-11-09.md).

## Decisions and next implementation boundary

- Goal20FPS remains ACTIVE/unmet. The owner's11hour status concern is valid;
  test/profiling work does not constitute FPS improvement. No hardware
  infeasibility conclusion is supported.
- Preserve original quality, cadence, casters, per-view probe correctness and
  established native fixes. No travel or quality reduction is authorized.
- GPU viewer-active union remains about109ms/present. Inclusive main/reflected
  labels overlap; no single shader cost is established. Next source change needs
  a bounded causal case addressing substantial work and varied output proof.
- Avoid another speculative microshader variant or broad profile without a
  specific unanswered question. Rejected/private experiments remain recorded.
- Rebuild Ninja test binaries before any new source-specific claim; they still
  contain the trial even though default source has been reverted.

[Architecture experiments](../../state/archive/CURRENT-ultra-goal-experiments-2026-11-08.md)
retain rejected layered results and actual M1 timestamp capability limits.
No capture/validation-enabled CSV is performance evidence. Raw captures remain
private; broad pixel/feature parity, multi-scene performance, prolonged stability,
strict no-GL and independent build/review evidence remain required.
