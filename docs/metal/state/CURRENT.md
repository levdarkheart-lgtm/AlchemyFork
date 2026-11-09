# Current engineering state

**Observed:** 2026-11-09, physical M1 Max / 64 GiB / macOS26.5.2 / Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `74ee5d48d97368d92e83ae40ba580516cef1e876`.
**Recovery:** [NEXT](NEXT_SESSION.md), [ledger](../work/active/metal-port.md),
[campaign](../work/active/ultra-20fps.md), [history](archive/INDEX.md).

## Goal and current result

The unbudgeted20FPS Ultra goal remains ACTIVE and unmet. Autonomous quit/build/
login/measure is authorized at the current private location and numericPreset123
only; no travel. Prefer Mac shortcuts. Preserve quality, six hero views/cadence,
shadows/casters and functionality. No private uploads or releases authorized.

Latest settled native run:9.382155FPS at desired texture bias1, up from prior
native8.864746FPS at bias4. These different texture workloads do not isolate an
FPS gain.20FPS is not achieved or proven impossible. After the owner's11hour
status concern, do not present tests, profiling or CPU relief as the FPS result.

## Active shader-specialization experiment — no viewer running

74ee5d commits validated fragment-bool constants, including optional-default
specialization and explicit rejection of unsupported vertex constants. Focused
425assertions/1case and full391587assertions/90cases pass with API+GPU validation
EACH Release/RWDI. Initial focused134 was fixed and retained in the evidence.
No production shader/world consumer exists yet; this support is not an FPS gain.

PrivateCM5KIW classic/SSR specialization failed strict bytes55/324, including
15nativeHDR cases differing by one half unit/channel each. Exploratory timing
found modern effects about2.7–3.5%, near same-PSO control variation, versus
17.9–19.8% in classic fixtures. It is NOT admitted: current profile is modern,
precision/downstream parity remains unverified, and fixture time is not FPS.

Private material-split prototype MxAxT0 has both independent source reviews.
It preserves the original normal-alpha predicate, masks stencil bit4 while
preserving water bits1/2, and includes reset/classifier/two lighting draws in
net timing.97fixtures×4formats cover mixed pixels, packed normals, depth and
stencil reuse. Compile/link0; correctness78056 completed0 with API+GPU validation.
Strict319/388pass69fail includes15nativeHDR one-half-unit differences; stencil/
depth/class/coverage errors0. Exploratory net timing65843 completed0: uniform
materials5.9–12.5%faster, checkerboardmixed2.7–4.6%slower; NOTadmitted.
No production shader/world edit or viewer launch. All processes finished. Root owns
shared files/runtime. [Evidence](archive/CURRENT-base-specialization-experiments-2026-11-09.md).

## Retained code and verified app — all viewers CLOSED

3969405 separates logical image storage from all-device pressure. Scope-only
68571 diagnostic proved its remaining small capacity still raised bias1→4:
image3053MiB exceeded target2969.6, device<4876 versus59392MiB recommendation,
all18system_low0. It ended early; NO68571FPSsample. State restored.

46d622 replaces arbitrary /16capacity with C=max(R−max(A−I,0),0), rawlogicalI;
residual includes padding/unpublished/retired storage and is an estimate. Apply
original divisor/headroom once; preserve manual bypass and independent global/
system/controller safeguards. No bias-derived state or high-water ratchet.
Both optimized configs pass159assertions/21pressure cases plus29/3capability
cases. Independent review approved; doctor/configure/build/verify349ARM64/
deep-strict signature all0. No shader code changed.

Last MEASURED app68572, source46d622; current canonical68574, source74ee5d, at
`build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`;
preserved `/private/tmp/alchemy-metal-68572-quality-baseline.app`.
Executable `84e49c91677697abf7527c93b21567950c5d525111b85ed3f3b0d189a1bbd155`;
metallib `8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943`,
byte-identical to older baseline. Current68574 executable is
`c19ea644343a61a4219771dd7beec351c59fc12903da511a21092bf75c7f5bbd`;
metallib unchanged. Doctor/configure/build/349ARM64/deep-strict signature all0.
68574 is unlaunched/unmeasured. Both Ninja GPU suites are current74ee5d and pass.
All temporary user-state plans are restored. Private GPU test is serialized.

## Latest controlled68572 sample

Full screen BEFORE login; actual Ultra6/3456×2168/Midday/Preset123 and43live
controls PASS. Exactly300consecutive frames9637–9936, all native/zeroGL,
all256probes complete/faded/current, main-visible98, stable120. Bias finite1
for all300rows; no missing observations. Mean106.585321ms,p50105.948188ms,
p95110.883167ms,9.382155FPS. Camera sampled delta0 and snapshots unchanged;
four camera resets were warmup history. Final camera equals68566. Native sun,
hero/HUD restoration and43controls match. No external scene change assumed.

During periodic formal checkpoints: image7838MiB,target45994–46000,device
9730–9737,recommendation59392MiB; all93diagnostic checkpoints system_low0.
This is normal bias with much more retained image data, not full pixel/residency
parity. Original main-visible104 versus native98 remains unresolved. A fresh
QA gate passed, but CUA screenshot timed out15seconds; no image obtained.
No new GPU/profile/build overlapped formal collection.

CSV `artifacts/metal/private-46d622-resource-ready-20261109-01.csv` +provenance;
QA `candidate-46d622-resource-ready-20261109-01`. Viewer96622 gracefully exited/
Goodbye12:27:46UTC, PID absence verified,46d622startup plan restored afterward.
[Complete source/build/measurement evidence](archive/CURRENT-resource-aware-texture-budget-2026-11-09.md).

## Remaining measured costs and baseline limits

Prior68566/source59177 repaired27/36Release targets previouslyO0:1607actual
compile actions nowARM64/O3/NDEBUG. CPU main91.476→72.768%ONEcore, no established
FPS gain. Clean older GPU viewer-active interval union109.245ms/present. Its
inclusive base/haze/hero labels overlap and include subsequent draws; never sum
or call them shader-line costs. That profile used bias4, not current bias1.

Latest existing query telemetry proves occlusion is consumed:120828spatial-group
skip events over299sample intervals, similar older120821. These are repeated
events, not unique groups or saved GPU time. Zero state-sort skips is not missing
occlusion. Original query diagnostics are unrecorded.

Original67611/exact1ebrenderer+observer:6.095519FPS,bias1,all256ready,43controls/
resolution matched. Settings/bias and camera agreement do not close full parity.
Prior native68564:9.270128FPS, bias not sampled; reverted68568:9.193167,bias4.
Do not use stale68558/10.064FPS as a correctness baseline.

## Active implementation checkpoint

KEEP per-view probe-space correctness, shiny full-hero bypass, compiler repair,
forward prefix bindings, scope/capacity repairs, occlusion, Ultra4×anisotropy,
bounded lights, directR32shadows, borrowed targets/depth, projector ownership,
arenas/caches/retired geometry. Hero stays6×1024faces/two cascades, raw PCSS,
reverse-Z/clamping/alpha casters. Full-image memory policy is not an FPS shortcut.

Layered/mask/SH/scalar/no-discard experiments were rejected/deferred; no repeated
micro-arithmetic search. Compression and excessive forward-light-loop hypotheses
were ruled out by exact source. Deferred non-Ultra edge: native alpha producer
lacks the local-light enable guard, so toggling count0 can retain stale lights.
This does not explain current Ultra cost. Broad parity remains open.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-004 Resource ownership | doing | Correct pressure scopes/reserve; broader lifecycle/cache policy open. |
| M-008 Advanced views/effects | doing | Preserve reflected views; broad parity/performance open. |
| M-012 Native UI automation | complete | Login/fullscreen verified; world AX/capture can time out. |
| M-014 Ultra20FPS campaign | doing | Normal bias restored at9.38FPS; target remains unmet. |

## Exact next actions

Use preserved68572/bias1 as the next native baseline. Material split has mixed
performance and remains private; do not enable it generally or repeat small
variants without new causal evidence. Next implementation must address a
specific measured GPU cost and preserve full quality. Resolve precise shader/
resource attribution where inclusive labels cannot. No register-pressure
diagnosis or FPS forecast follows from specialization.20FPS remains active.

Multi-scene performance, feature/visual parity, prolonged stability, strict no-GL
and independent build/review evidence remain required. No release claim.
