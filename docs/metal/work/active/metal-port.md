# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `30ef2ccb694ee3030ff059ec00f084de3fe7c748`.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[deferred campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current implementation — 2026-11-11

Owner resumed unfinished RLVa, Debug/Developer and remaining OpenGL removal.
No viewer launch or screenshot comparison. The separate20FPS campaign remains
deferred. Follow original source contracts before each fix.

[Current work and command receipts](remaining-feature-port.md).
Latest: [native shader ownership cleanup](native-shader-owner-cleanup.md)
removes the dormant compiler/linker/binary cache/reflection/uniform upload and
shader/query/resource entrypoints. Native pass/variant/CPU data contracts remain.
Remove write-only schema registration and unused buffer-name state. Actual
world/UI Metal libraries/PSOs reload on forced rebuild, retaining targets,
histories and old material leases. Failed reload/configuration requeues safely.
Diagnostics use actual Metal identity or unavailable state without GL queries.
Native/optional utility builds0; Metal-validated tests35550assertions/32cases.
First canonical link failed because root removed the still-needed native loader
failure helper; restored from original source in30ef2ccb69. Corrected frozen
configure/build0, canonical Release68655 verified349ARM64/0invalid, deepcodesign0;
viewer/utility have no OpenGL framework or GL/CGL/SDL_GL undefined imports.
Strict source audit remains failed1441findings/2406files,159GL-call
syntax matches. Root owns final integration/build/evidence. Independent utility
and preview reload worktrees/owners are pinned in CURRENT; their changes are not
integrated. Current reload coverage is world/UI, not every application shader.

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

Integrate isolated utility/preview reload and vertex allocation/upload cleanup
commits after reviewing exact diffs and running GPU tests. Root alone wires the
central builtin revision announcement and CMake; preserve generated/external
images and pending resources. Developer Frame Profile is being implemented in
its isolated worktree using actual Metal encoder measurements and honest
unavailable counters. Runtime M1 probe confirms only stage-boundary timestamps,
no per-draw/dispatch sampling or statistics. Continue remaining GL manager/
vertex-draw/image tails. No viewer or utility launches.
CURRENT pins exact worktree paths, branches and exclusive owners. Keep viewer closed.

## Non-negotiable open gates

Full original feature parity, prolonged stability, measured multi-scene
performance, strict source/runtime no-GL and independent release review remain
open. Keep all earlier unresolved lighting, self-turn, camera and voice issues
documented; do not apply speculative unrelated policy changes.
Do not mark feature parity from build/test success alone.
