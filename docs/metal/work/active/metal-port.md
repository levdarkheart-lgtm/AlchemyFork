# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `d1288129991e72b425c7dd60b2932f9fb1214f7b`;
latest measured artifact is7a2639ea4d/Release68559 (8.121045 FPS, correct probe space).
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[campaign](ultra-20fps.md), [archive index](../../state/archive/INDEX.md).

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 reference/build | doing | Source-exact Metal68554 and exact-base OpenGL67607 build/sign/verify349 ARM64 slices; reproducible acceptance remains. |
| M-002 | GL inventory and release audit | doing | No product GL/Vulkan link; strict source inventory and runtime zero gate remain. |
| M-003 | Native presentation/lifecycle | doing | Metal-only world/UI publication and attended startup work; wider lifecycle/stability remains. |
| M-004 | Resource/transfer/sync ownership | doing | Completion-retired geometry/arenas, borrowed targets/depth and direct shadow slices; cache pressure/retention remain. |
| M-005 | MSL/binding behavior | doing | Goal edits pass Release/RWDI66,962/79 each; source-exact product/live validation pending. |
| M-006 | UI/fonts/HUD/media | doing | Native world/UI and font ownership; full parity/stability gates remain. |
| M-007 | World/material/shadow correctness | doing | Native families, original raw depth/casters/projector sharing retained; broader visual smoke remains. |
| M-008 | Advanced views/effects | doing | Native mirrors/probes work; preserve six Ultra faces, investigate general layered batching and original reflected-content parity. |
| M-009 | Feature/stability/native-only release | doing | Feature matrix, prolonged stability, independent review and strict release gates remain. |
| M-010 | M1 Max measured optimization | doing |68554 paired CSV observes8.409→9.739 FPS, with material opaque-GPU variability confound; full attribution/multi-scene contract open. |
| M-011 | Remaining owned GL authority | doing | Core rendering/resources/window/UI are native; remove reachable nonessential tails without feature deletion. |
| M-012 | Native UI automation | complete | Keyboard/AX flow verified; latest owner authorizes autonomous fixed-location measurement. |
| M-013 | RLVa parity | deferred | Owner essential-first ordering; final functionality still required. |
| M-014 | Ultra20 FPS campaign | doing | General CPU/GPU repairs, exact controls and varied fixtures; current location/Preset123 only. |

States describe evidence; a bounded complete item never implies product completion.

## Current implementation boundary

Native Metal owns application rendering and presentation; geometry/images/fonts
have no parallel GL names. Snapshot has no GL scratch authority. World,
materials, avatars, terrain, shadows, sky/water/atmosphere and converted effects
have direct owners. Existing non-rendering features are retained.

6c694df237 adds ordinary native direct shadow packets, immutable slab constants,
strict adjacent full-triangle range merging and actual populated G-buffer
bindings. Shaders match175cd155dd/68550 byte-for-byte. The optional ICB path
is not production's default; its68553 runtime did not establish a net win.
Full Release/RWDI API+GPU validation each passes57,344 assertions/75 cases.

Latest native query/aniso audit: e52de13acc implements real conservative/exact
visibility and original issue ordering; f799262971 maps literal0/2/4/8/16 levels
to native1/2/4/8/16 and live replacement. Current Ultra uses4. Actual group
rejections remain unquantified. Hero cube occlusion stays disabled as original.

Detailed prior lifecycle/binding/GL-removal history is retained in the
[pre-goal ledger](../archive/metal-port-before-ultra-goal-2026-11-08.md) and
indexed engineering archives. No release invariant was waived by this rewrite.

## Current source ownership

| Owner | Exclusive edits | Acceptance |
| --- | --- | --- |
| shadow_contract | Read-only independent hero-blend candidate review | Exact-original semantics, endpoint coverage and partial/off paths. |
| trace_recheck | Ignored hero-blend prototype, exclusive GPU window | Expanded parity and counterbalanced varied timings; no product edits. |
| draw_cpu_costs | Read-only exact-original external measurement audit | LEAP histories/cursor feasibility; no launch/build/GPU activity. |
| root | World renderer/shaders/CMake/docs/integration | Opaque family census, original-source review, serialized builds/benchmarks, final actual runtime validation. |

No source-overlap edits. Shared builds wait for file freezes and isolated GPU
timings. Benchmark/capture data stays local; no private upload is authorized.

## Latest measurement and limits

Full screen before login, Midday, Preset123,3456x2168 and21 live Ultra controls
were verified. Warmup120frames+300seconds; both variants300 consecutive exact
native rows, zero GL fallbacks. Fresh68550→68554 mean118.916→102.679ms,
p50117.437→101.876ms, p95133.511→119.074ms. Observed mean FPS+15.8%.
The same-shader main opaque GPU pass varies0.3–20ms; entire gain is not isolated
to source changes. Hero remains50.44% of sampled main activity and the main
thread94.28% of one core. Selected static-source coalescing is69.86%.
[Latest runtime](../../state/archive/CURRENT-68554-direct-shadow-runtime-2026-11-08.md).

## Complete-product checkpoint gate

1. **Advancement:** native binding/submission work is reduced with GPU proof;
   current campaign targets general remaining CPU/GPU work and stronger controls.
2. **Preserved contract:** no translation/fallback backend, no dropped views,
   casters, quality or non-rendering functionality. Exact original remains1eb.
3. **Open gates:**20 FPS objective, multi-scene performance contract, strict
   source/runtime GL-zero, visual/feature parity, prolonged stability and review.

## Immediate executable action

All viewers/profilers closed, user startup state restored. Corrected68559:
8.121FPS, GPUviewerunion110.517ms/present. Initial opaque-post19.232ms and hero
water12.235ms need per-draw/attachment inspection; CPU probe refresh is small.
[Latest record](../../state/archive/CURRENT-68559-probe-view-runtime-2026-11-09.md).

1. Matrix memo rejected after exact-output tests and slower repeated/unique CPU
   benchmarks; uncommitted integration removed, private prototype preserved.
2. Capture68561 saved; Xcode commands/resources inspected, profiling stalled85%
   and explicitly stopped. No per-draw timings. Viewer exited/userstate restored.
   [Inspection](../../state/archive/CURRENT-68561-capture-inspection-2026-11-09.md).
3. Keep per-face correctness repair and original Ultra workload. Broad ordered
   lists remain rejected by actual dense/neighbor-heavy selection data.
4. Shiny fullhero optimization d128812999 committed/reviewed. Release existing
   full121227/86+focused268745/1, fullRWDI389972/87 pass API+GPU validation.
   No live speed claim yet; exactsource normal build and300frame comparison next.
5. Continue goal;20FPS, parity, multi-scene and release gates remain open.

## Non-negotiable open gates

No retained OpenGL, Vulkan/MoltenVK/Rosetta/translation fallback. Preserve viewer
functionality and exact rendering behavior. Do not infer parity or FPS from
build/test counts. PRODUCT/VALIDATION/PERFORMANCE release contracts remain in
force. Owner permits no travel during this goal; no new release/public capture
upload is authorized. Preserve code/evidence/next actions at every handoff.
Do not mark feature parity from build/test success alone.
