# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `bccbb7be48000fab91b43688bc585f2f9ab474cf` plus current feature-port changes.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[deferred campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current implementation — 2026-11-11

Owner resumed unfinished RLVa, Debug/Developer and remaining OpenGL removal.
No viewer launch or screenshot comparison. The separate20FPS campaign remains
deferred. Follow original source contracts before each fix.

[Current work and command receipts](remaining-feature-port.md).
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
fixed and final Release build pending. Live feature parity remains unverified.
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
| M-008 | Advanced views/effects | doing | Preserve six Ultra hero faces; reflected-content parity/performance open. |
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

M-008/M-011/M-013 advance through corrected setting/policy consumption and
removal of displaced GL execution, with bounded physical GPU evidence.
This does not establish complete Debug/Developer menus or RLVa parity.
M-012 stays complete; M-014 remains deferred. No release readiness claim.

## Immediate executable action

Finish current Release app build, inspect ARM64 slices with verify_app and
record app identity. Then implement native RenderBufferVisualization modes0–6:
the current presentation gate rejects all selected buffers and the displaced
GLSL visualization is still their only consumer. Trace original attachment,
luminance mip8 and SMAA-edge/weight lifetimes and scope/reference/UI order.
Continue dormant GLSL compiler/linker/binary-cache removal after that boundary.
The detailed work note records exact paths and tests. Keep viewer closed.

## Non-negotiable open gates

Full original feature parity, prolonged stability, measured multi-scene
performance, strict source/runtime no-GL and independent release review remain
open. Keep all earlier unresolved lighting, self-turn, camera and voice issues
documented; do not apply speculative unrelated policy changes.
Do not mark feature parity from build/test success alone.
