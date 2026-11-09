# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `0d5d49c188f8bfee4dd866adbac757af750340d4`;
latest68581:9.454639FPS; best controlled68579:9.608673.20FPS/full parity open.
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

Root owns all shared source/docs/runtime. All viewers CLOSED/all startupplans RESTORED;68581 exactapp preserved.
Translator explicitly fully stopped via its uv service command, actual0; stays
STOPPED, no automatic restart. Latest68579/source1f2cc0 ready300:9.608672762FPS,
mean104.072646/p50103.895667/p95109.0435ms,bias1/main95/Ultra3456×2168. Freshstart/
end43controls/location/binary gates0; camera basis/projection unchanged. Prior
68577 8.011210/68576 6.728847/68572 9.382155FPS had uncontrolled TranslatorGPUwork:
no purecodegain claim.20FPS remains unmet. App68579 preserved with exact hash.
Formal unsplit; later diagnostic131presents viewerGPUunion103.5406ms/p, initial
familyunion2.61622ms/p, mainbase17.333/heroabove-water21.067 inclusive/overlapping.
Addedpassboundaries altercost; no shader-duration/unsplit equivalence claim.
[Complete evidence](../../state/archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Complete-product checkpoint gate

1. **Advancement:** native binding/submission work is reduced with GPU proof;
   current campaign targets general remaining CPU/GPU work and stronger controls.
2. **Preserved contract:** no translation/fallback backend, no dropped views,
   casters, quality or non-rendering functionality. Exact original remains1eb.
3. **Open gates:**20 FPS objective, multi-scene performance contract, strict
   source/runtime GL-zero, visual/feature parity, prolonged stability and review.

## Immediate executable action

Committed neighbor repair4b23 restores exact-original64 candidate/32 accepted
traversal. Both full optimized API+GPU suites427464/98 and independent12-case
oracle pass. Normal68588 built/verified, preserved; no new live FPS claim.
Hero-forward diagnostic0d5d49 is validated in BOTH configs: actual capture3059/3,
normal API+GPU427476/95. Diagnostic68590 built/verified, ran and captured face1;
300 diagnostic frames/all256 ready/stable120/43 gates/camera0. All viewers closed,
plans restored, Translator stays STOPPED. Canonical source recording ON; future
formal normal app must configure OFF. No overlapping profile/GPU work.

Actual Xcode1CB/4render/4blit/833draws;15.74ms MEDIUM not live FPS. Simple alpha
shadow58.02%/245 registers/304 spill bytes; encoder occupancy4.19%/overdraw1.58,
early PreZ rejects55.29%. Call Stack base-probe sampling71.05%, holds28.93%
(inclusive/optimized source attribution, not literal per-instruction timing).
User manual inspection complete; replay STOPPED/Finished app observed and CUA
completion recorded. Haze label includes later alpha draws; haze FS only1.79%.

Next PRIVATE probe-only shadow specialization: actual bound pack bit0x4 gate,
unchanged main PCSS/quality. Root verified frozen inputs/production prefix;
compile0/0/0 and API+GPU35852 actual0,200 strict color/rawD32 pairs50×4 formats.
No performance/occupancy gain yet; run quiet A/B/control timing before admission.
Distinct ambient-only simple probe walker is source-only preparation. Rejected
SH lifetimes/predecoded SH/scalar cursor/simple tile mask remain rejected.
Ordinary SCENE batching proposal/tests source-reviewed but unapplied/uncompiled;
commit before independent face publication, preserve failure/generation rules.
[Full checkpoint](../../state/archive/CURRENT-shader-registers-and-neighbor64-2026-11-09.md).

## Non-negotiable open gates

No retained OpenGL, Vulkan/MoltenVK/Rosetta/translation fallback. Preserve viewer
functionality and exact rendering behavior. Do not infer parity or FPS from
build/test counts. PRODUCT/VALIDATION/PERFORMANCE release contracts remain in
force. Owner permits no travel during this goal; no new release/public capture
upload is authorized. Preserve code/evidence/next actions at every handoff.
Do not mark feature parity from build/test success alone.
