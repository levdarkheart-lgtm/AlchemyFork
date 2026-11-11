# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `7710ae4f977295d7c3539516a2d3814485a7db45`.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[paused campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current correctness work — 2026-11-11

Owner requests all accumulated fixes pushed. Broader porting goal and20FPS goal
are paused; viewer must stay CLOSED. No launch or screenshot comparison.
[Edit Rotate repair](object-rotation-repair.md): exact original Debug shader is
position-only/uniform-color. Native omitted MAP_VERTEX, rejecting immediate
rings/fans and authoritative UI publication. Repair restores metadata and nonindexed
cull-state propagation. No input/permission gate, hidden geometry or math changes.
Saved live log corroborates original washer/fan vertex counts; not exact clicks.

Actual native fixture passes62/1. Same frozen test on old cull code fails4/62;
restored candidate plus nearby UI/metadata regressions pass419/14 with Metal API
and shader validation. Tests cover real immediate helpers and independent GPU
oracles; newview loader and live dragging are separate source/build/live gates.
Independent source/test reviewers approved; live object rotation unverified.

Canonical Release68648 at7710ae4f97 built/verified/signed, never launched for
validation. Doctor/configure/build/verify_app/deep codesign all0;349ARM64 Mach-O.
Private evidence: private-object-rotate-audit-20261110-01/final-build-identity.json.
Viewer census empty. No account/cache/settings/camera/region/object changes.
Earlier inspection opened login-only viewer; it was closed after owner said no
launch. Do not use app-selection APIs that can auto-launch.

Keep [classic-avatar W1](classic-avatar-position-w.md), [LibVLC module/cleanup
repair](parcel-music-host-repair.md), [travel repairs](travel-correctness-repair.md)
and all preceding renderer changes. Old avatar library fails10/256; repaired
256/1 and existing48568/2 pass; actual local silent media host smoke passed.
Owner reports geometry fixed in76-avatar crowd; complete parity still open.
[Crowd/turn findings](crowd-live-68643.md): animated limit11→40 restored owner
responsiveness, distinct self-arrow-turn/body lag unresolved. Avoid intrusive
LLDB repeats. [Camera/voice](teleport-camera-voice-audit.md) source matches;
actual teleport fly-in cause and attended voice/media acceptance still open.

[Display audit](display-settings-lighting-audit.md): HiDPI restart and internal
HDR wired, physical outputSDR. UNFIXED deferred emissive-OFF and Glow-ON callback
defects; preserve original alpha/HUD emission exceptions. Bright/dark lighting
complaint open; Lightbox/EEP/bounded AE math match, no speculative tuning.

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
| M-014 | Ultra20FPS campaign | deferred | Ownerrequestedstop; finalcurrentOFF9.745FPS/20FPSunmet; no validatednextfix. |
States describe evidence; a bounded completed item never implies product completion.


## Current implementation boundary

Native Metal owns world, materials, avatars, terrain, shadows, sky/water,
atmosphere/effects, UI/font/HUD, media textures and presentation. Existing
network/inventory/simulation/editing/voice features are retained. No fallback
renderer or general cross-platform framework is authorized.

Keep all prior repairs: bounded local lights, rawR32 direct shadow slices,
resource borrowing, image/global pressure accounting, completion-safe arenas,
per-view probes/cubeSSAO, main visibility queries, Ultra4xanisotropy, binding
caches, static adjacent draw coalescing and64/32 probe traversal. Joining stays
nonpersistent/defaultOFF. New HUD/upload fixes build on source611e1f3a0f/88528d27a5;
ancestry was explicitly verified. No measured performance improvement claimed.

## Complete-product checkpoint gate

Editing renderer advanced through source-confirmed Debug layout and cull fixes,
with actual failing-old/passing-new GPU evidence. Not live edit-feature parity.
All previous repairs retained; broad goal and performance campaign stay paused.
No full product, FPS improvement or release-readiness claim.

## Immediate executable action

Complete normal push of all commits to origin/codex/apple-silicon-metal, verify
remote HEAD equals final local HEAD, keep exact receipt private. Owner already
authorized this push. No force push, release or default-branch merge. Keep viewer
closed; do not resume broad implementation or screenshot comparisons.

## Non-negotiable open gates

Full original feature/pixel parity, prolonged stability, multi-scene performance,
strict source/runtime no-GL and independent release review remain open. Preserve
open-source non-rendering behavior, voices/media and native Metal ownership.
Do not infer full product parity or FPS from bounded test counts.

Do not mark feature parity from build/test success alone.
