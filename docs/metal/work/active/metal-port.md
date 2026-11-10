# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `0cbb48f104ad2f7d6e358266c1abf32d4d487a63`.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[paused campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current correctness work — 2026-11-10

Owner explicitly requested code fixes following failed68633 travel QA. Renderer
checkpoint `0cbb48f104`; all prior repairs retained. Root integrates/builds/docs;
independent agents source-reviewed owned components and actual regression tests.
[Implementation/evidence](travel-correctness-repair.md): fresh native ordered UI
uses a retained PRE-UI color/depth pair while the current world is incomplete;
resize/device/Z mismatch uses cleared backing. Full UI-size color/raw-depth
resampling supports reduced/supersampled world targets. Early redundant main
G-buffer clear removed; borrowed writes exclude retained depth. Fetched classic/
PBR terrain restores original revival/Default/Null/raw/sRGB sampling, without
substituting generated maps. Complete empty main views retain original sky/lighting progression.
Impostor failures retain retry, complete empty bakes
publish transparent data. Object palette validation ignores unused W lanes while
preserving every valid coefficient. Pending display names retry despite distance
lines. Actual bad XYZ inputs and full live regression results remain unknown.

Actual Release tests: skin201/3, terrain272/1, impostor17070/1, retained UI/resize/
scale/HUD29130/3, existing UI/pool/impostor17964/4 all passed. GPU runs used API+
shader validation on M1 Max. One UI fixture shader-selection error corrected
before passing rerun. Canonical Release68638 built/verified/signed, left closed; doctor/configure/
build/verify/deep strict signature all0,349 Mach-O files all ARM64.
No viewer launch, camera/profile/account/cache changes or FPS campaign resumption.
Private exact commands/logs under private-travel-correctness-repair-20261110-01.

[Failed live travel](travel-live-68633.md) still governs visual status. Owner saw
UI/movement/resize stalls, missing names and stretched geometry. Trace's active
runloop does not disprove intermittent visible freeze. New backend tests are
mechanism proof, not live acceptance. Prior [avatar repairs](avatar-crowd-parity.md)
and [cold startup/environment checkpoint](../../state/archive/CURRENT-cold-start-environment-repairs-2026-11-10.md)
remain included. Cold cache backup private/intact. Translator stopped, replay off.

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

Specific loading, UI/depth lifetime, scaled composition, name retry and affine
palette defects are repaired with actual native pixel/ownership tests. Whole
viewer input/resize/teleport/crowd/ART and prior shadow/sky/low-DD visual parity
remain unverified. Non1-scale final grain/dither coordinate parity is open.
Build/tests do not establish product completion or FPS gains.

## Immediate executable action

Canonical68638 is ready from source0cbb48f104, verified/signed and left closed.
Next owner-driven travel should validate actual loading/teleport/UI input/resize,
name arrival, HUD alpha, attachments and earlier lighting/terrain/water symptoms. Earlier20FPS campaign remains paused;
last controlled9.745FPS is not a new result. No uploads/releases or quality cuts.

## Non-negotiable open gates

Full original feature/pixel parity, prolonged stability, multi-scene performance,
strict source/runtime no-GL and independent release review remain open. Preserve
open-source non-rendering behavior, voices/media and native Metal ownership.
Do not infer full product parity or FPS from bounded test counts.

Do not mark feature parity from build/test success alone.
