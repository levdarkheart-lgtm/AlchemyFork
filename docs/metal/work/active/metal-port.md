# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `d967552ae823ae6d1cc7c979c8528c4fe10fc5e8`;
latest settled sample494c/Release68564:9.270128FPS. Original6.095519FPS, closed; quality caveats remain.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[campaign](ultra-20fps.md), [archive index](../../state/archive/INDEX.md).

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 reference/build | doing | Source-exact Metal68554 and exact-base OpenGL67607 build/sign/verify349 ARM64 slices; reproducible acceptance remains. |
| M-002 | GL inventory and release audit | doing | No product GL/Vulkan link; strict source inventory and runtime zero gate remain. |
| M-003 | Native presentation/lifecycle | doing | Metal-only world/UI publication and attended startup work; wider lifecycle/stability remains. |
| M-004 | Resource/transfer/sync ownership | doing | Completion-retired geometry/arenas, borrowed targets/depth and direct shadow slices; cache pressure/retention remain. |
| M-005 | MSL/binding behavior | doing | Default source62e2passed391162/89 each Release/RWDI; restored68569built/verified, unmeasured; broad parity open. |
| M-006 | UI/fonts/HUD/media | doing | Native world/UI and font ownership; full parity/stability gates remain. |
| M-007 | World/material/shadow correctness | doing | Native families, original raw depth/casters/projector sharing retained; broader visual smoke remains. |
| M-008 | Advanced views/effects | doing | Preserve six Ultra faces and reflected-content parity; layered prototype rejected, broad correctness/performance open. |
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

## Current source ownership and latest result

Root owns shared renderer, shaders, tests, CMake, documentation and runtime.
Agents supplied private patches/reviews and are finished; no active process or
uncoordinated edit remains. All viewers closed and temporary state restored.

Latest combined trial68568:300consecutive exact-native frames, Ultra6/3456×2168,
Midday/Preset123/43controls,all256probes ready/stable120,bias4. Mean108.776ms,
p50108.786,p95113.496,9.193FPS. Last optimized68566 was8.865FPS, earlier settled
68564was9.270FPS. One combined window does not establish repeatable improvement.
[Complete record](../../state/archive/CURRENT-68566-68568-performance-trial-2026-11-09.md).

59177compiler repair relieved main CPU91.476→72.768% of one core without FPS
improvement. GPU viewer-active union109.245ms/present remains substantial.
Inclusive base/haze/hero labels overlap and cannot be summed as a frame budget.
No shader-line attribution or hardware-infeasibility claim exists.

## Complete-product checkpoint gate

1. **Advancement:** native binding/submission work is reduced with GPU proof;
   current campaign targets general remaining CPU/GPU work and stronger controls.
2. **Preserved contract:** no translation/fallback backend, no dropped views,
   casters, quality or non-rendering functionality. Exact original remains1eb.
3. **Open gates:**20 FPS objective, multi-scene performance contract, strict
   source/runtime GL-zero, visual/feature parity, prolonged stability and review.

## Immediate executable action

Default d967reverts the unproven opaque PBR trial, preserving source/app/patch for
future general testing. Retain Release compiler and forward binding repairs.
Default renderer/test sources match62e2; prior full API+GPU391162/89 pass each
optimized configuration. Trial full392926/89 pass each. Ninja test binaries still
contain trial code: rebuild before any fresh source-specific test claim.

Default app68569 configure/build/349ARM64/signature all0; not launched/measured.
All source provenance, hashes and local logs are in CURRENT and the trial record.
20FPS remains ACTIVE/unmet. Next change needs a bounded causal case addressing
substantial remaining CPU/GPU work, exact-original mapping and varied correctness
proof. Avoid speculative shader arithmetic and repeated broad profiles. No20FPS
or hardware-infeasibility conclusion is justified. Keep owner informed promptly.

## Non-negotiable open gates

No retained OpenGL, Vulkan/MoltenVK/Rosetta/translation fallback. Preserve viewer
functionality and exact rendering behavior. Do not infer parity or FPS from
build/test counts. PRODUCT/VALIDATION/PERFORMANCE release contracts remain in
force. Owner permits no travel during this goal; no new release/public capture
upload is authorized. Preserve code/evidence/next actions at every handoff.
Do not mark feature parity from build/test success alone.
