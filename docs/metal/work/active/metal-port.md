# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `76413f2fcf76e528fcfe3740635111dcd4a54ea8`.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[paused campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current correctness work — 2026-11-10

Owner requested fully empty startup then exact-original source comparison for UI
stalls, lighting/sky/shadow mismatch, environment-switch blink and low-DD terrain.
Prior 20 FPS goal remains paused. Earlier HUD/upload/performance repairs retained.
Root exclusively owns shared source, integration, builds, runtime and canonical docs;
agents produced private patches/read-only reviews, no competing shared edits.

**Implemented/committed and built, live improvement pending:**

- b75e79cb73: restore original ALTextureSlot pending/default-image semantics for
  generic native UI fast/slow/raw binding. Cold log showed ready world blocked by
  missing UI texture19:34:31–38, success19:34:42. Actual native RED/GREEN test:
  pre-fix pending exit42; repaired pending+VB suite201 assertions/3 cases exit0.
- 0ef3e16a0b: restore original effective sky probe ambiance==0 display gamma,
  independent of surface classic flag, plus consistent original no-post exposure,
  tonemap/bloom/streak policy retaining alpha glow. Actual reported sky eligibility
  remains unknown. Backend display1824/1 + bloom67/1 pass, caller source reviewed.
- 76413f2fcf: retain completed default radiance/SH across compatible sky resets,
  as original. Strict layout/device/generation guards; advance epochs, clear
  nondefault receipts/pending tickets. Backend ordinary134/1 and hero589/1 pass;
  actual LLReflectionMapManager reset/replacement and visual transitions untested.

All native tests used API+shader validation. Doctor/configure/Release build/
verify_app/deep strict codesign0; build68630 ready, **NOT launched**. Exact commands,
exits/source/hashes in private-cold-start-lighting-repairs-20261110-04/build.json.
Prepared canonical disk app differs from still-running68626PID44312/sourceD186;
its prior binary identity was pinned at launch. Do not assign new fixes to it.
Cold launch occurred ONCE from zero files at SAME active cache path;8978 original
files/1.70GB preserved privately. Account/settings untouched, no driver-cache purge.
No original viewer launch or subsequent restart solely for source inspection.

Original and native UI/world use main thread. Native pending publication explains
one actual stale-display mechanism; no hard canvas deadlock or stale-size bug
proved. The later five-second CPU sample started AFTER UI recovery. PSOs completed
~50s before the reported freeze. Cache byte sizing policy matches original.
Sun/projector source audits found no additional concrete defect. Original terrain
culls with infinite far reach; shared water/far-plane code unchanged. Native terrain
factors>=1, no DD height fade. Missing cube-only raster-span cap cannot explain
MAIN terrain loss. Affected geometry/map/depth evidence is still missing; no
speculative cutoff or quality reduction applied. Random shoe/outfit spikes unknown.

Historical HUD/upload routing, original alpha/sampler/PBR and retry fixes plus
executed checks are preserved in the indexed avatar-hud-upload checkpoint.
All private artifacts/caches remain ignored; no upload/release. See CURRENT.

## Multi-avatar follow-up — source audit only

[Detailed comparison](avatar-crowd-parity.md) preserves exact original ranking,
full-detail limits and complexity rules. Remaining concrete native gaps: absent
ART GPU timing, view-local impostor publication, excessive unloaded-avatar probe
invalidation and missing body-bake default-image substitution. No source repair,
viewer action, build or benchmark was performed for this follow-up; build68630
does not address them. Private admission patch is reviewed but unintegrated.
No cross-avatar palette alias was found; crowd parity stays unverified.

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

Native UI loading, EEP display policy and probe continuity now follow confirmed
original contracts. Build/native primitive checks passed; live correction of the
reported lighting/blink/terrain/shadow symptoms is not yet established. Broader
parity, stability and performance remain open. No product-completion claim.

## Immediate executable action

Report concrete source repairs and leave current live session intact. Prepared
68630 awaits the next owner-directed run: observe actual environment transitions,
encoded/reset receipts and completed replacement. Then compare remaining actual
sky/interior/shadow/terrain behavior against exact original; trace affected
geometry/map/palette/depth instead of speculative admission changes.

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
