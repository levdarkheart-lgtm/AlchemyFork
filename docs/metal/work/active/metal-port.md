# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `f75d9e306ffd1371582ff40b8662bf7944aabee6`.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[deferred campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current implementation — 2026-11-11

Owner resumed unfinished RLVa, Debug/Developer and remaining OpenGL removal.
No viewer launch or screenshot comparison. The separate20FPS campaign remains
deferred. Follow original source contracts before each fix.

[Current work and command receipts](remaining-feature-port.md).
Latest: [native feature/resource completion](native-feature-resource-completion.md)
integrates utility/preview reload, native vertex allocation/upload/direct draws,
actual capacity-based batching, GL manager/state cleanup, write-only GLTF sink
removal, native About/report identity, Developer Frame Profile and native fixed
texture benchmark. Native draw regressions67598assertions/42cases and combined
FrameProfile/utility/preview regressions60425/12 pass with Metal validation.
Registered benchmark target and GPU integration run0,19123assertions/10cases.
Frozen696c43ba78 app build0; canonical68677 verified349ARM64/0invalid,
deepcodesign0 and viewerimports0forbidden. New image readback/helper cleanup,
native labels, displaced shader profiler, primitive-counter fix, SDL context/
OpenGL dependency removal and native build gates are integrated through815416675e;
full dependent native regressions0,67773assertions/47cases. Wrapper/Win32/WGL/
Vulkan tails removed; native water count fixture31516/1 passes. [Menu audit](developer-menu-admission.md)
repairs inherited timer controls and identifies the remaining orphan Shader
Cache enable control. [Source-tail evidence](native-opengl-tail-removal.md):
CPU vocabulary and expanded source gate integrated,3507files/0blockers/0errors/
1188informational;39tool tests0. Native regressions67793/49 and93869/22 pass.
Native archive dd9372cd69/root policy53e3599a36 are integrated, including actual
failed-save retry, shader-validation compatibility and persistence proof.
[Current cache/coverage receipts](native-pipeline-cache-and-coverage.md) supersede
historical counts above: frozen d28b0477a0 Release26.4.0d68712 doctor/configure/
build/verify0,349ARM64/0invalid/deepcodesign0/imports0forbidden; optional appearance
Release build/ARM64/signature/import checks0. Full llrender77929/312(311pass/one
CPAL multi-palette skip),alrender556639/132,targeted GPU93869/22 all0. Cache API
5905/6,global instrumentation882/1,selective98/1 all0. Tools39/harness32files0;
strict source3509files/0blockers/0errors/1188informational. Filters overlap.
No launches. Exact commands/failures in latest note.
[New target/query/font invariants](native-target-query-invariants.md): native
font/UI24cases276assertions0, defaultFont/TextCache61cases1862assertions0 in
separate registered process,2Dhelper4cases76assertions0. Dead image-slot state
removed after exact live format predicate relocated; usage/font447/340.
Native timer-only receipt owner5666/30; displaced attachment spans removed.
Target/depth f75461d142 and query ac4a125458 fixes are integrated with actualGPU
regressions. Native sphere/cached draw diagnostics restored f36e8aabdb. Full
registered llrender88827assertions/375cases(374pass/one known palette skip),
separate Font/UI1862/61 both0 under Metal API/shader validation.
[Latest Scene Monitor/UI/RenderInfo evidence](native-scene-ui-and-render-info.md)
supersedes that checkpoint: frozen9918621da8 includes renderer0b; Release
llrender96864/384(383passed/one CPAL skip), Font/UI1862/61, alrender556641/132,
affected Metal73523/38 all0. Canonical26.4.0d68744 doctor/configure/build/verify/
deepcodesign0;349ARM64/zero invalid, viewer forbidden imports0. Appearance
build/ARM64/signature/imports0. All349Mach-O imports inspected: foreign VLC/CEF
GL links are recorded separately and require consumer/policy audit. No launch.

Earlier verified bounded checkpoints below remain included; their older build
and inventory counts do not supersede the current observations above.
Follow-up: [native buffer visualization](debug-buffer-visualization.md) now
owns modes0–6 in the original post order, retaining spatial meter/SMAA sources.
[Optional baking loader](appearance-native-loader.md) now configures native
AlphaMask without shared GL compilation; initialization failure cleanup is
checked. Remove displaced visualization GLSL and dead texture introspection.
Canonical Release68651 and optional Release utility build0. ARM64 bundle349/0
and deep strict codesign0. Metal-validated tests59386/11 +959/18 pass; no apps
launched. Strict source audit still fails1783findings/2406files (333call syntax).
Older bounded fixes and receipts follow; they remain included.
Native sphere ownership replaces its displaced GL execution; current-view
parameter sampling restores original per-execution policy. Typed overlay order
is retained. [RLVa mapping](rlva-native-owner-cleanup.md).
[Developer diagnostics](developer-metal-diagnostics.md) now select next-run
Metal API validation before device creation, including legacy requests and
debugsession; explicit driver-crash testing enters Metal without GPU work.
The crash action was not executed. Correct deferred emissive OFF, Glow ON and
Build No Post-processing preflight; preserve original alpha/HUD exceptions.
Remove dead GL depth-bounds unit/tests/setting and sphere GLSL files.

Doctor/configure/test builds0. Native renderer910/17 and focused native38307/5
plus display-post1824/1 assertions/cases pass under Metal API/shader validation.
First app compile found an exposed transitive camera include; explicit include
fixed. Final Release68649 build0; ARM64 inspection349files/zero invalid slices
and deep strict codesign0. Exact app hashes are pinned in the work note.
Live feature parity remains unverified.
Strict GL inventory still fails1792findings/2406files. No performance claim.

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
| M-008 | Advanced views/effects | doing | Native buffer views and real intermediate ownership validated; full parity remains. |
| M-009 | Feature/stability/native-only release | doing | Feature matrix, prolonged stability and independent release review remain. |
| M-010 | M1 Max measured optimization | doing | Controlled baselines/captures collected; multi-scene contract unmet. |
| M-011 | Remaining owned GL authority | doing | Native resources/window/rendering; strict remaining-tail audit open. |
| M-012 | Native UI automation | complete | Exact fullscreen/login/Preset123 flow; unsupported system-helper targeting avoided. |
| M-013 | RLVa parity | doing | Native-only sphere ownership and current-view state repaired; broader behavior remains unverified. |
| M-014 | Ultra20FPS campaign | deferred | Ownerrequestedstop; finalcurrentOFF9.745FPS/20FPSunmet; no validatednextfix. |
States describe evidence; a bounded completed item never implies product completion.

## Current implementation boundary

Native Metal owns world, materials, avatars, terrain, shadows, sky/water,
atmosphere/effects, UI/font/HUD, media textures and presentation. Retain network,
inventory, simulation, editing and voice behavior. Preserve all earlier native
resource, shader, avatar, HUD/upload, travel, music and editing repairs.
No fallback renderer or general graphics framework is authorized.

## Complete-product checkpoint gate

M-008/M-011 advance through real buffer-view consumers, native world/UI shader
reload and removal of shared GL compiler/driver ownership; M-013 prior fixes remain included.
Bounded physical GPU evidence does not establish whole-feature/runtime parity.
This does not establish complete Debug/Developer menus or RLVa parity.
M-012 stays complete; M-014 remains deferred. No release readiness claim.

## Immediate executable action

Frozen9918 verification is complete; root owns integration/CMake/docs. Native
fence495efc818c integrated asf75d9e306f (original12case map, actual2318/20pass,
bounded two-counter fix), registered2157/9pass. Target-local almetal_tests bundle
resource ownership registered indexed-join7969/1pass; direct CLI CTest only.
Review RLVa
silhouette fill (menu owns pipeline/world/MSL; child21566/4pass). Release pipeline
ownership to rlva for CPU-preserving zero-reader uniform closure, then gl_tail's
ALGPUBuffer/schema/native-byte diagnostic cleanup. Dead stencil graph follows.
Root isolated postchain test-only tree proves original window-viewport lens
mask mismatch at reduced/oversampled world sizes:1080failed/9249assertions;
fix after shared-owner release with unchanged pixel oracle. Trace foreign
VLC/CEF module consumers separately. CURRENT pins exact ownership and receipts.
Existing native postchain4487/4pass is not the missing final-viewport invariant.
Goal stays active; build/test counts do not imply complete feature parity.
Keep viewer and appearance utility closed.

## Non-negotiable open gates

Full original feature parity, prolonged stability, measured multi-scene
performance, strict source/runtime no-GL and independent release review remain
open. Keep all earlier unresolved lighting, self-turn, camera and voice issues
documented; do not apply speculative unrelated policy changes.
Do not mark feature parity from build/test success alone.
