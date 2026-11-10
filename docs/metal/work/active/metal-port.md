# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `d18641c3557996807de03d99a105419161e95dfb`.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[paused campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current correctness repair — 2026-11-10

The owner authorized avatar/HUD investigation independently of the paused 20 FPS
campaign. All attached/enabled HUD fills were absent while edit outlines rendered.
The first native-authority fix restored fills but exposed wrong transparency and
sampling. Exact original1eb comparison found dropped HUD forward alpha state,
stale sampler inheritance, and an opaque/MASK PBR tail that still used retired
draw calls. These are repaired with original color/coverage/depth contracts.
A later live log exposed pending HUD diffuse texture invalidating whole composed
frames; original default/Null placeholder behavior is restored in final68626.

State: **implemented and built**; owner accepted previous fixes and requested
all retained, including latest discovery. FinalPID43249 sourced18641c355
was observed launching in UI. Final19:24UTC startup check has26 success/publication
log entries and zero HUD-diffuse/upload/API assertion errors. Generic UI mirror
warmup was reported before publication resumed; no zero-wait claim. Root owns source,
canonical docs, builds and runtime. Agents source-reviewed/private patches only.
No camera/quality/outfit/asset-network change and no private uploads.

A separate native upload retry defect is reproduced and fixed: failed native
VB/IB uploads kept Written flags but lost their mapped queue entries. The repair
keeps retries through subsequent flush, disarms dying entries and clears pending
raw pointers on shutdown. Pre-fix test failed4 exact byte comparisons; corrected
native suite passes93/2 with API+GPU validation. No current production failure
proves this caused the random missing shoe. User repeated Wear restored that
occurrence; outfit-change spikes also remain unexplained.

Commands/evidence (all private ignored artifacts/metal): doctor/configure/build/
verify_app/deep strict signature0; UI/HUD763/1 and selection4732/1 API+shader0;
VB93/2 API+shader0. Direct UI fixtures bypass pipeline caller, so source/actual
account observation and owner feedback remain essential. Source checkpoints:
547daf9c31,082075249b,916ad7e79d,08bc61af05,d18641c355. Final evidence directory
private-hud-placeholder-fix-20261110-04; preceding native-upload-retry03 and HUD
blend02 preserve failures/reruns/commands/hashes. See CURRENT for full identities.

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

HUD and upload correctness advanced through original-source repairs, bounded
checks and actual account observations. Random attachment omission, full original
parity and product performance remain open. No product-completion claim.

## Immediate executable action

Final68626 startup inspection completed; leave viewer running for owner. For a recurring
attachment omission/spike, follow actual geometry/map/palette/batch state against
exact original; the shared update/flush/routing/query checks did not reveal a
shoe-specific difference. Do not label it fixed merely because re-Wear helped.

The owner-paused performance campaign remains deferred. Prior final9.745FPS and
trace evidence are archived in CURRENT-20fps-owner-stop-2026-11-10.md. No automatic
new benchmark/profiling/quality cuts. Translator fully stopped; Xcode replay off.
All CUA phases completed. No private capture upload/release. Product not complete.

## Non-negotiable open gates

Full original feature/pixel parity, prolonged stability, multi-scene performance,
strict source/runtime no-GL and independent release review remain open. Preserve
open-source non-rendering behavior, voices/media and native Metal ownership.
Do not infer full product parity or FPS from bounded test counts.

Do not mark feature parity from build/test success alone.
