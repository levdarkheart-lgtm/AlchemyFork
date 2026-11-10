# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `c2804f88df2923a9cbc172bc551fad5cd38243eb`.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[campaign](ultra-20fps.md), [archive index](../../state/archive/INDEX.md).

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 reference/build | doing | Source-exact originals/native normal builds verified; independent fresh-checkout release remains. |
| M-002 | GL inventory and release audit | doing | Owned native rendering; strict source/runtime zero gate remains. |
| M-003 | Native presentation/lifecycle | doing | Native world/UI and attended lifecycle; prolonged stability remains. |
| M-004 | Resource/transfer/sync ownership | doing | Arenas/retirement/direct slices/borrowed targets/material ownership validated; net performance open. |
| M-005 | MSL/binding behavior | doing | Original math preserved; material resource ABI validated; full parity open. |
| M-006 | UI/fonts/HUD/media | doing | Native implementation retained; full feature/stability proof remains. |
| M-007 | World/material/shadow correctness | doing | Raw depth/PCSS/casters/reverseZ retained; broad original parity remains. |
| M-008 | Advanced views/effects | doing | Preserve six Ultra hero faces; reflected-content parity/performance open. |
| M-009 | Feature/stability/native-only release | doing | Feature matrix, prolonged stability and independent release review remain. |
| M-010 | M1 Max measured optimization | doing | Controlled baselines/captures collected; multi-scene contract unmet. |
| M-011 | Remaining owned GL authority | doing | Native resources/window/rendering; strict remaining-tail audit open. |
| M-012 | Native UI automation | complete | Exact fullscreen/login/Preset123 flow; unsupported system-helper targeting avoided. |
| M-013 | RLVa parity | deferred | Owner essential-first ordering; retained functionality still required. |
| M-014 | Ultra20FPS campaign | doing | Water68610 improved with workload caveat; PBR68614 live9.566vsfresh9.581flat;20FPS unmet. |

States describe evidence; a bounded completed item never implies product completion.

## Current implementation boundary

Native Metal owns world, materials, avatars, terrain, shadows, sky/water,
atmosphere/effects, UI/font/HUD, media textures and presentation. Existing
network/inventory/simulation/editing/voice features are retained. No fallback
renderer or general cross-platform framework is authorized.

Retain existing repairs: rawR32 direct shadow slices, bounded local-light
coverage, resource borrowing, image/global memory accounting, completion-safe
arenas, per-view probes/cubeSSAO, main visibility queries, Ultra4xanisotropy,
binding caches, static adjacent covered draw coalescing and64/32 probe traversal.
NativeD32 changes original raw-depth semantics; no migration/epsilon waiver.

## Current source ownership and latest result

Root exclusively owns shared source/docs/runtime/GPU. Agents prepare private
source or read-only analysis; no concurrent uncoordinated shared edits.
Translator FULLY STOPPED/NEVERrestart; Xcode replay STOPPED; same location only.

Material-buffer cache1407 passed native correctness/isolated CPU checks, but
normal68605 live9.066675vsproper68601 9.410237 did not prove a benefit. Restored
material_buffers=false/ordinary samplerABfalse in06d16. Both optimized435462/102
passed, normal68606 doctor/build/349ARM64/signature0, preserved. No cache revival
without new mechanism. [Evidence](../../state/archive/CURRENT-material-argument-buffers-2026-11-10.md).

Earlier admitted68601/source77977290 ready300:9.833798FPS/mean101.690106ms.
Preserved/private/tmp/alchemy-metal-68601-normal-preserved.app, exef277/lib61c5.
Fresh68601 trial9.408707FPS/mean106.284533/p50106.109021/p95116.8194 had missing
start gate and unmeasured CUA stall; exploratory only. End43gates0/exact98164
SIGTERM/Goodbye/absence/planrestore0. Camera/EEP equal new68605 at frame5046.
Owner reports fullscreen GameMode notification; leave mode unchanged/drop as
primary lead. Unsupported ControlCenter CUA inspection stalled; do not retry it.
[Detailed current evidence](../../state/archive/CURRENT-material-argument-buffers-2026-11-10.md).

## Complete-product checkpoint gate

1. **Advancement:** source-exact material cache/ownership integrated, real GPU
   correctness and bounded complete CPU-cost evidence; normal viewer trial completed with no FPS gain.
2. **Preserved contract:** original rendering/features/all six hero faces, no
   quality reduction, no translation/fallback backend; exact original remains1eb.
3. **Open gates:**20FPS and multi-scene performance, full pixel/feature parity,
   strict source/runtime GL-zero, prolonged stability and independent release review.

## Immediate executable action

6b80 integrates bounded default-off native stage timing, independent samplebuffers
percommand/completion-safe bufferowners/strict decodecalibration/no waits or
extra commits. Counterfailure abandons prepared slots before original passretry.
Controller requires300formalexactreadyframes,60eligible cooldown, then1–3normal
intervals with main/hero/ordinary/filter tags; logs only after completion.
Focused41331/2 and full Release+RelWithDebInfo476793/104 API+GPU suites0.
Normal68608 doctor/configure/build/verify/signature0;349ARM64, exe d8f4cc0a/
lib39320181, actual O3/NDEBUG/C++23/arm64 flags independently verified.

68608 formal300readyframes9.069479FPS,mean110.259914/p50109.686104/
p95120.610834; propercontrol9.410237 butdraws+2.814%, noisolateddelta/gain.
Both43start/endgates0/exactnative/bias1/ready256stable120. EEPexact; tinycamera
residuals remain.334resolvedstagepasses/3intervals AFTERformal; base17.4ms,
mainhaze13.8–18.0/hero5water13.2–15.1 elapsed, notadditiveGPUbusytime.
Exact57546exit/Goodbye/absence0 and OWNplanrestored0; no viewer/profiler.
[Fullresult](../../state/archive/CURRENT-stage-timing-runtime-2026-11-10.md).

Read-only postwater SIMPLE early tests passed actual API+GPU11598/1 and six
1024² pairs11753/1. Quiet224 samples14676/1 showed no convincing win beyond
same-path drift. Rejected; root froze full tested patch624a611d and restored
all five files exactly6b80 before the new water candidate. No viewer trial.

Root applied water coref8425015/fixture0122e99e plus required library restoration.
Corrected API+GPU15425/1 and eight1024² preflights15719/1 passed. Separate quiet
R1+R4 EACH18642/1/224rows/thermal0: R4 eligible GPU savings15.42–17.17%, all7
blocks faster in all4 cases, SAME+1.27/−0.35%. Independent raw audit passed;
CPU gain unproved. Private cost code removed after freezing tested2b48b8e3;
strict permanent fixture exactly matches validatede68cb9. Generic producer now
enables only actual guarded mode; bounded opt-in logs report encoded PSOs.
Both full optimized API+GPU suites482529/104 pass0;15final source hashes match.
Committedf549a796d2. Normal68610/source8eea built+verified0; independentfc35passed.
Candidateformal300readyframes9.785333FPS (mean102.193761ms), exactcamera/EEP/43
gates versus68608; observed+7.893%FPS but draws−1.87%/mainprobes95vs98. No full
causal claim. Both candidate91508 and freshcontrol92996 exited/Goodbye/absence0+OWNplansrestored0.
Freshcontrol300readyframes9.257293FPS versuscandidate9.785333 (+5.704%); exact
camera/EEP/43controls/native/bias1/256ready match. Draws+1.616%,probes95vs100:
retain workload caveat. Independent129db056 recommends RETAIN;20FPS unmet.
Post-formal CPU+baselinecoupledMetaltrace captured/exported0. Matched2461CBs,
GPUActiveunion87.60% (not utilization); two profiler-metadata pauses retained.
All viewers/profilers/build/GPUtests stopped. Next bounded GPU address candidate.
[Water evidence](../../state/archive/CURRENT-water-probe-specialization-2026-11-10.md).
Main-base probe address-space candidate was implemented and compiled0. Actual
API+GPU40strict+6large pairs passed3319/1 after two fixture-input corrections.
Quiet168samples passed5521/1, but both dense cases were slower in all7blocks;
mean GPU+5.205/+5.876%, no CPU gain,162out-of-target rows retained. Rejected;
all4sourcefiles restored byte-exactly7772, no viewer trial. Ninja test artifacts
are still experimental and require rebuilding. [Evidence](../../state/archive/CURRENT-probe-address-space-rejection-2026-11-10.md).
Stable7772 was pushed at owner request; unfinished experiments excluded. The
owner chose manual existing alpha-workflow dispatch because CLI/browser lacked login.
PBR FC0 candidate coreb809 and guarded producer/actualreceipts integrated. Actual
68pairs+6large25379/1 and quiet16828122/1 pass0; GPU mean−13.555/−17.747%, all7
blocks faster, no CPU/FPS claim. Expanded88strictpairs+6large27593/1 pass0.
Private benchmark stripped, strictfixture byte-proof7ccb823c. Both optimized fullAPI+GPU suites494211/104 EACH pass0; final15sourcehashesmatch.
Committed765c305; normal68614/source271 doctor/configure/build/verify/signature0,
independent5722identity0. Fresh68610baseline300frames9.581135FPS; candidate68614
300frames9.566138FPS (flat−0.1565%), p50104.361208/p95114.900709ms. Both43start/
endgates0/exactcamera+EEP/native1/bias1/256ready120; draws+.4043%, probes98→97,
retain drift caveat.11encodedPBRreceipts; no demonstrated livegain. Both actual
exactexits/Goodbye/PIDabsence0 and OWNplansrestored0, CUAimages actually observed.
Officialenforce/provenance1: provenance passes, target/scene count unmet.
No viewer/profiler/GPUtest/build live. Root/agents audited remaining hero prep costs. Native face-local occlusion is
not a parity defect and lacks a proved removable population; main-camera state
must not be reused. Root added default-off ALCHEMY_METAL_DRAW_JOIN_CENSUS: after
successful encodes,13independent view/phase samples every120frames count ONLY
necessary geometry adjacency, not full-state eligibility or GPUcost. Query-bearing
Gbuffers are explicitly gated/unmeasured. No draw/state/resource changes. Normal68616/source1ff build/349ARM64/signature0, independent074cidentity0.
Completed78formalrows/all26tuples; substantialheroGBufgeometryupperbounds but
no actualstateeligibility/FPSclaim. ExactPID37648exit/OWNplanrestored0.
Root integrated default-false actual-native-state indexed joining prototype in
7wrapper/world/test/CMakefiles committedc2804f88, genericproducerfalse. Actualfocused
API+GPU7969/1wrapper+2120/1production pass0; strict67pairs/2removalpositives and
native2/8→1activation. Initialcompile0; updatedfullRelease504300/106 passed0,
RWDIbuild/full504300/106 also0. PrivatequietcompleteCPU/GPUcost next. No FPSadmissionyet.
[Census/prototype](../../state/archive/CURRENT-indexed-draw-joining-2026-11-10.md). [Evidence](../../state/archive/CURRENT-pbr-probe-specialization-2026-11-10.md). Preserve actual bound pack bit4/count>1 guards, main/default/no-shadow paths,
exact original cube single-tap and shader math. Root owns shared source/GPU.

Parallel shadow prototype was compiled/GPU-tested435842/103, including two views
before one commit. First full-preparation CPU trial regressed+19%/+13%; bounded
slot-cache revision still+9.71% at640 and4096 apparent−4.45% overlapped identical
path drift−5.24%. Both280-sample runs passed parity/provenance/thermal checks.
Rejected/no viewer trial. Exact13-file prototype removed before6b80; no parallel path retained.
Normal68606 preserved/independently verified. Ninja artifacts remain experimental
and need rebuilding before reuse. Ten private evidence packets are preserved.
[Complete trial](../../state/archive/CURRENT-parallel-shadow-encoding-2026-11-10.md).

Prior sampledCPU/frame and overlapping completed-command sums are not a current
CPU floor or causal GPU budget. Closed shader/register/mask/mutability variants
remain rejected. Full pixel/feature parity and release gates remain open.

## Non-negotiable open gates

No retained OpenGL/Vulkan/MoltenVK/Rosetta/translation fallback. Preserve original
quality and non-rendering functionality. Do not infer parity/FPS from test counts.
PRODUCT/VALIDATION/PERFORMANCE remain authoritative; no release/public capture
upload authorized. No travel under this goal. Retain raw private evidence and
record real commands/exits/source identity at handoff. Goal ACTIVE/unmet.
Do not mark feature parity from build/test success alone.
