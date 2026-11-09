# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `30715f3cde5d6b29e2f0fe974e5d0ef72a7db94f`;
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

Root owns shared renderer/shaders/tests/CMake/docs/runtime. All viewers closed
and temporary user-state plans restored. Preserved68572 source46d622 is the
next bias1baseline. Canonical68574/source74ee5d is built/verified349/signature0,
unlaunched/unmeasured, with unchanged production metallib.74ee5d adds fragment
bool specialization support only; full391587/90 API+GPU checks pass EACH config.

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

## Immediate executable action — committed gather, live result pending

0fa4e3 restores original main-onlySSAO: no cubeAO/2blurs, main preserved.
68576fullready300samples neverthelessREGRESSED6.728847FPS,mean148.613868ms,
p95156.249375 vs68572/bias1mean106.585321/9.382155FPS. Controls/environment match,
cameradelta0/tinytranslation,main97vs98. Officialperf gate1. GPUtracecurrent
viewerActiveunion143.136ms/p,maininitialopaqueinclusive44.844ms/p; not shader
cost orcontrolledhistoricalcomparison. SSAOblur removal observablyconfirmed.
38935quit/Goodbye/PIDabsent; stateRESTORED. New68577 is now running as described below.

30715f commits native rawshadowgather preserving exactintegerfootprint/order.
Initial15680queries+96lightmaps andextended23480IEEE/prodextentqueries allEXACT;
GPUvalidation0. Quiet7blocktimingsPCSS22–29%lower,unchangedGaussian/probepaths
+.023/.016ms explicit, noFPSforecast. Newpermanenttestfocused34626/1 andfull
Release426213/91 API+GPU0; independentreviewapproved. FullRWDI71370/appverify9881 actual0,426213/91 and349ARM64; Xcodebuild/signature0.
Canonical68577/PID58207 RUNNING warmup, fullscreenbeforelogin/live43controlgate0;
30715fplanINSTALLED/NOTrestored. Config/launcher dryrun0. Finish residentcomparison,
noGPU/profile/buildoverlapformal. Usefreshstartupplan andrestoreaftergracefulexit.
[Fullsource/runtime/filterevidence](../../state/archive/CURRENT-probe-ssao-and-shadow-gather-2026-11-09.md).

## Non-negotiable open gates

No retained OpenGL, Vulkan/MoltenVK/Rosetta/translation fallback. Preserve viewer
functionality and exact rendering behavior. Do not infer parity or FPS from
build/test counts. PRODUCT/VALIDATION/PERFORMANCE release contracts remain in
force. Owner permits no travel during this goal; no new release/public capture
upload is authorized. Preserve code/evidence/next actions at every handoff.
Do not mark feature parity from build/test success alone.
