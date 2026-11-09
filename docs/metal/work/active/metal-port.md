# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `46d6227526ef28850e0b306d1dcb0537c7d59f07`;
latest settled68572:9.382155FPS/bias1. Original6.095519FPS; full parity remains open.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[campaign](ultra-20fps.md), [archive index](../../state/archive/INDEX.md).

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 reference/build | doing | Source-exact Metal68554 and exact-base OpenGL67607 build/sign/verify349 ARM64 slices; reproducible acceptance remains. |
| M-002 | GL inventory and release audit | doing | No product GL/Vulkan link; strict source inventory and runtime zero gate remain. |
| M-003 | Native presentation/lifecycle | doing | Metal-only world/UI publication and attended startup work; wider lifecycle/stability remains. |
| M-004 | Resource/transfer/sync ownership | doing | Completion-retired geometry/arenas, borrowed targets/depth and direct shadow slices; cache pressure/retention remain. |
| M-005 | MSL/binding behavior | doing | Shader source62e2passed391162/89 each config; current68572built/verified/measured at bias1; broad parity open. |
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
to native1/2/4/8/16 and live replacement. Current Ultra uses4. Latest metadata
records120828spatial skip events/299intervals, not uniquegroups or savedGPUtime.
Hero cube occlusion stays disabled as original.

Detailed prior lifecycle/binding/GL-removal history is retained in the
[pre-goal ledger](../archive/metal-port-before-ultra-goal-2026-11-08.md) and
indexed engineering archives. No release invariant was waived by this rewrite.

## Current source ownership and latest result

Root owns shared renderer/shaders/tests/CMake/docs/runtime. All agents and
viewer/build/test/profile processes finished. All temporary user-state plans
restored. Canonical/preserved68572 source46d622 is the next bias1baseline.

Scope3969405 and resource-aware capacity46d622 preserve original controller,
manual/divisor/headroom semantics and global/system safeguards. CPU tests pass
159/21pressure+29/3capability checks EACH Release/RWDI; source review approved.
App configure/build/349ARM64/signature all0; shader8bbbyte-identical baseline.

Latest68572:300consecutive exact-native frames9637–9936,Ultra3456×2168/Midday/
Preset123/43controls,all256ready/stable120,main98,bias1all300,camera delta0.
Mean106.585ms,p50105.948,p95110.883,9.382155FPS. Prior68566bias4was8.864746FPS;
different image workloads prevent isolated FPS attribution. Images7838MiB
retained at normal bias;20FPS remains unmet. No fullpixelparity claim.
[Complete evidence](../../state/archive/CURRENT-resource-aware-texture-budget-2026-11-09.md).

Older59177compiler repair relieved CPU91.476→72.768%ONEcore without FPS gain.
Its bias4GPUunion109.245ms/p is not current-bias1 attribution; inclusive labels
overlap. Live occlusion consumes results:120828spatial skip EVENTS/299intervals,
not uniquegroups/savedGPUtime. No shader-line or hardware-infeasibility claim.

## Complete-product checkpoint gate

1. **Advancement:** native binding/submission work is reduced with GPU proof;
   current campaign targets general remaining CPU/GPU work and stronger controls.
2. **Preserved contract:** no translation/fallback backend, no dropped views,
   casters, quality or non-rendering functionality. Exact original remains1eb.
3. **Open gates:**20 FPS objective, multi-scene performance contract, strict
   source/runtime GL-zero, visual/feature parity, prolonged stability and review.

## Immediate executable action

Keep memory-policy quality repairs and use preserved68572/bias1baseline. Rebuild
stale Ninja GPU test binaries before new source-specific GPU claims. Next bounded
audit: original compile-time view/shadow/SSR variants versus native runtime
branches in base/IBL/forward shaders and pipeline creation. Immutable view
invariants may allow substantial function-constant specialization; this is not
a measured register-pressure diagnosis. Require varied output proof/repeated
control before admission. Avoid rejected micro-arithmetic/broad profile loops.

Deferred non-Ultra correctness edge: native alpha light producer misses the
RenderLocalLightCountenable guard on count0 transitions; preserve for later fix.
No current-FPS attribution. Goal20FPS ACTIVE/unmet; independent release gates open.

## Non-negotiable open gates

No retained OpenGL, Vulkan/MoltenVK/Rosetta/translation fallback. Preserve viewer
functionality and exact rendering behavior. Do not infer parity or FPS from
build/test counts. PRODUCT/VALIDATION/PERFORMANCE release contracts remain in
force. Owner permits no travel during this goal; no new release/public capture
upload is authorized. Preserve code/evidence/next actions at every handoff.
Do not mark feature parity from build/test success alone.
