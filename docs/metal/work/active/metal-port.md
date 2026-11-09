# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `1f2cc0e786f6c88f5a1dee23215c17650e6e83b0`;
latest68576:6.728847FPS/bias1 regression;68572baseline9.382155FPS. Full parity open.
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

Rootownssharedsource/docs/runtime. Renderer30715fgather+test committed; all
viewersclosed/startupplansrestored. Canonical68577preservedmeasuredapp; all349
ARM64/signature0. Full426213/91 API+GPUtests EACH Release/RWDI pass.
Latest68577:8.011210FPS/124.825083msmean,121.765792p50/154.566750p95; bias1,
300readyframes10487–10786,43controls/environmentmatch, cameraEXACT68572,main98.
Versus68576recent6.728847FPS:mean−16.007%; versus685729.382155:mean+17.113%.
Bothofficialperformancegates1;20FPSunmet. No netwin overolderbaseline claimed.
PostformalGPUviewerunion129.350ms/p vs143.136recent; mainopaqueinclusive20.756
vs44.844, lightmap4.743vs5.418, baseabout18. Othergroupsvary; nofullcausalclaim,
noadditionofoverlaplabels. ExtraGPUactivityidentifiedpythonPID35824 fromTranslatorvenv; userauthorizedFULLSTOPviauvcommand(actual0),workerabsent. DO NOTRESTART.
Current68579/PID78103 controlledwarmup/1f2cc0Middayplanactive; preserveformalunsplit,
postCSVfamilytraceonly. Both426444/91GPUconfigs/app349/sign0.
[Completeevidence](../../state/archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Complete-product checkpoint gate

1. **Advancement:** native binding/submission work is reduced with GPU proof;
   current campaign targets general remaining CPU/GPU work and stronger controls.
2. **Preserved contract:** no translation/fallback backend, no dropped views,
   casters, quality or non-rendering functionality. Exact original remains1eb.
3. **Open gates:**20 FPS objective, multi-scene performance contract, strict
   source/runtime GL-zero, visual/feature parity, prolonged stability and review.

## Immediate executable action

Allcurrenttests/builds/verify/profilesfinished; no runningviewer. Keeporiginal-
contractcubeSSAOrepair0fa4e3 and testednativegather30715f; quality/cadenceintact.
1f2cc0commitsvalidatedinitialfamilydiagnostic+counter/test, defaultOFF,
mainonlyandafterformalCSV. Preserveallbindings/depth/stencil/order; realparity
neededbeforecapture. Specificquestion: whichinitialfullbright/shiny/avataralpha/
glowgroupcostsremainingmainopaqueGPUtime. Addedpassboundariesarediagnosticcost,
notproductionFPS. Newtestsourcebeingpreparedprivately, notappliedyet.

SeparateD32arrayfeasibilityGPU98353passed8960rawqueries+96fullthreewaymaps exactly;
noopaqueFSnilcaster/clamp/coverage/migrationproof. Sourceplanonlynext, notproduct
optimizationclaim. Full20FPS,visualparity/generalperformance stillopen.

## Non-negotiable open gates

No retained OpenGL, Vulkan/MoltenVK/Rosetta/translation fallback. Preserve viewer
functionality and exact rendering behavior. Do not infer parity or FPS from
build/test counts. PRODUCT/VALIDATION/PERFORMANCE release contracts remain in
force. Owner permits no travel during this goal; no new release/public capture
upload is authorized. Preserve code/evidence/next actions at every handoff.
Do not mark feature parity from build/test success alone.
