# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `5e1ecb76459de8a0bc90dc16da56aec75f1fc02d`;
latest68601:9.833798FPS vspriorbest68594:9.635581; causal gain/20FPS/full parity open.
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

Root owns shared source/docs/runtime. AllviewersCLOSED;68601/source77977290
300readyframes9507–9806:9.833798FPS/mean101.690106ms/p50101.502709/p95106.397708.
Freshstart/end43controls/location/exactbinary gates0/fullscreen before login;
exact73462 SIGTERM/Goodbye/absence+currentMiddayplanrestore0. Allplansrestored.
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
300 diagnostic frames/all256 ready/stable120/43 gates/camera0. That diagnostic
viewer closed and its plan restored; Translator stays STOPPED. Canonical source
recording now OFF in normal68594. No overlapping profile/GPU work.

Actual Xcode1CB/4render/4blit/833draws;15.74ms MEDIUM not live FPS. Simple alpha
shadow58.02%/245 registers/304 spill bytes; encoder occupancy4.19%/overdraw1.58,
early PreZ rejects55.29%. Call Stack base-probe sampling71.05%, holds28.93%
(inclusive/optimized source attribution, not literal per-instruction timing).
User manual inspection complete; replay STOPPED/Finished app observed and CUA
completion recorded. Haze label includes later alpha draws; haze FS only1.79%.

Committed735a probe-shadow function constant0 folds existing bit4 single-tap branch;
stock/custom simple-alpha keys distinguish none/runtime/probe. Gate actual bound
validated shadow flag +actual probe count>1, retaining default-only runtime.
Two source reviews approve; MSL920d2e2d. Actual native API+GPU10157=0 ALL200
strict color/rawD32 pairs50×4 inclmain/default;168 quiet timings33685=0,
heavy23.2–31.8%lower/all7 blocks, fallback/control ranges cross0. Small capture actual1CB/4draws/profile29.09msMEDIUM;245→224 registers,
BOTH304spill bytes (no spill-elimination/liveFPS claim). Actual public renderer regression
patch applied: checkerboard main-vs-probe positive controls, full color/rawdepth,
rigid/skinned/standard/custom cache isolation. Ninja Releasebuild72719=0;
full API+GPU95754=0,427529/95; RWDI build+API/GPU70294=0, same427529/95.
Xcode test-target attempt65 corrected to Ninja tree. All jobs terminal/GPU idle.
User handed Xcode back to ROOT; replay remains STOPPED until all GPU tests end.
Small replay STOPPED/Finished app and CUA completion observed. Normal68594
sourceb299 doctor/configOFF/build13489=0, verify87922=0, independent349ARM64/
strictsignature96763=0. Formal68594 300readyframes/end43gates0:9.635581FPS vsbest9.608673, effectively
flat(+.28%); globalcamera4warmup-only, sampledpose/EEP fixed. Exact14587 closed/
Goodbye/absence75320=0 andb299plan restored0. Latestuserrequested startupFPS
observation: LIVE17022 current68594, same43gates0, startupDIAG6000frames complete.
ExactFPSwidget readviaofficialLLWindow.getInfo:32→11/22.4s,later10;160notcaptured.
Rawfirst10s12.26FPS/44.6kdraws,next10s11.04/50.7k, unchangedearlycamera.
Mainprobe pack2→98/hero up to256 despite somefixedpostbatchcounts. Correlation
notcausalGPUtime. CPU sample0 but only2main-threadsamples/DWARFwarnings: unusable.
All plans restored/Translator STOPPED. Readiness observations DISABLED in this
startup diagnostic. No image parity claim. Committed4e833 actual-selected-PSO
counters; both optimized nativeAPI+GPU428004/95 pass. First sandbox42/noGPU,
separate native rerun0. Normal68597 doctor/configOFF/build/verify25454 all0;
independent349ARM64/signature/sourceidentity0, exactPID48160 beforefullscreen/
login; fresh43gates0. Livehero ordinary.standard.true/main+avatarundefined.
Earlywarmup10s CPUtrace0/11076samples, hero51%mainCPUactive; staticbulk2.22%.
Prior48160 ready300DIAG8.159FPS/end43gates0/closed92003=0/restored. NewPID50766
continuousTimeProfiler12min14305 COMPLETED0/920867samples, raw6000allnative1,
freshstart/end43gates0;50766closed29248=0/Goodbye/absence/currentplanrestored0.
MainCPUactive92→79%; approximateCPU/frame77.6→76.5ms whileFPS11.8→10.75,
noCPUbusyramp. ThermalNominalfulltrace; clocksunknown. GPUspansgrowbutoverlap,
no criticalpathproof. Covered-shadow5e1ecb implemented; both optimized428946/95 pass, strengthened
native commandcounter1747/1 bothpass. Live covered merges16.27%main/18.77%probe
in one interval; independent39intervalaudit confirms4298.86commands/frame.
Whole-frame causal gain UNKNOWN; latest9.83vspriorbest9.64 is only~2%descriptive.
Private Metal3/4samePSO/oldMSL3.1 prototype compiled0/API+GPU0,12coverage/Z
pairs+positivecontrol rawR32/D32exact. No performance/fullviewerclaim. Normal68601/source779772 doctor/configOFF/build/verify91206each0; independent
identity/source/library/deepstrictsignature pass (root349ARM64 reviewed).
Current QA candidate-covered-shadow-20261109-01 completed300/allnative1/
fallback0/256ready/bias1; endgates+exactshutdown+restore0, noimageparityclaim.
Private fixed-textureM3/M4 CPUbenchmark xitJjX actuallycompiled0, API+GPUstrict
parity0, quiet168timings0. CachedM3 vsM4 precommit107.459→113.894us/1024draw,
387.270→404.291us/4096draw: NO WIN. Strictparser shTjM2Y0 SHA03f54b35 verifies
schedule/parity/thermal. NEWchanging-materialfrBX0a actualcompile0/API+GPU0,
24strictpairs, quiet168samples0:8slots−59.58%CPUencode/32slots−83.52%,all7paired
wins. Bindingstressfourrecurringmaterials EVERYdraw/prebuilttables/aniso1,
notliveFPS. No shippingMetal4 switch; mid-shadowAPI split breaks currentbatch
ownership. M3argumentBmrgf1actualcompile0/API+GPU0/24pairs/168CPUtimings0,
8slots−59.93%/32slots−87.18%nativeencode(all7paired); cachecoststillneedspricing.
Hf64pq85full2048²strictpairs0;2×252quietGPUtimings0 showprobablemodest32slot
cost (~.004ms/pass/repeatmedian+4.28%), notGPU-nonregression. Rootworkingdiff
integrates sharedFrameData cache/new8/32resourceABI/nativeowner+samplerflag,
boundedcoldretirement/directoracle/publictests; Releasefinalbuild87038=0.
Firstsandboxmodulecacheblocked, firstnativeWerrorbracesfixed; currentfulltests
PASS430654/100BOTHconfigs. Helper1354/5+world1927/1PASS; standaloneagingtestpass.
FullCPUfixtureRc4uL9 compile0/10nativeoraclepairs0/196quiettimings0: hot8−37%/
32−70%, cold512+117%/cap4096+1194%CPU rejected. Missfix+workboundsnowBOTHfull
434648/101PASS; unchanged196retesthot8−39.54%/32−67.8%,cold512−22.93%,
butcap4096still+124%UNACCEPTABLE. PressurepolicyAPPLIED;BOTH435459/102PASS,
196CPUretiming0:hot8−39.38%/32−68.60%,cold512−31.54%,cap4096still+27.24%.
Flat75dc nowBOTH435459/102PASS+196CPU0:hot8−40.21%/32−69.60%,cold512−31.10%,
cap4096+22.68%(+1.455ms). ControlledviewerTRIAL next; GameMode state unknown
([research](../../state/archive/CURRENT-apple-metal-research-2026-11-10.md)).
Whole-shadow2orderedchild proposal remains source-only. Fresh68597pairedcontrol
still required for causalcoveredFPS. No overlappingviewer/GPUtimings.
No formalFPSgain/infeasibilityclaim. Fullcontinuousdetails inNEXT/archive.
Ambient-only rewrite REJECTED (all timing ranges cross0). Rejected SH/predecode/
cursor/simplemask remain rejected. PBR/water source-only fixtures may prepare;
no combined optimization before current controlled viewer comparison.
Ordinary SCENE batching proposal/tests remain private/uncompiled, preserve
scene-before-publication/generation/failure ordering.
[Full checkpoint](../../state/archive/CURRENT-material-argument-buffers-2026-11-10.md).

## Non-negotiable open gates

No retained OpenGL, Vulkan/MoltenVK/Rosetta/translation fallback. Preserve viewer
functionality and exact rendering behavior. Do not infer parity or FPS from
build/test counts. PRODUCT/VALIDATION/PERFORMANCE release contracts remain in
force. Owner permits no travel during this goal; no new release/public capture
upload is authorized. Preserve code/evidence/next actions at every handoff.
Do not mark feature parity from build/test success alone.
