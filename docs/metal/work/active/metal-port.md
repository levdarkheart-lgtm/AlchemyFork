# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Renderer checkpoint:** Commit `2c60e47b0eac61f3a62b504163504516d1729bed`.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[paused campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current correctness work — 2026-11-10

Owner requested actual original-source repairs after failed68638 travel. Root
integrated and built; independent agents source-reviewed separate contracts.
[Classic-avatar repair](classic-avatar-position-w.md): original position vec3 is
explicitly XYZ/W1. Native shared skin/cloth helpers used raw stored padding W,
which is not homogeneous position. Two helper normalizations restore original
translation behavior without hiding geometry or changing object skinning.
Actual old68638 library fails10/256 assertions; repaired shader passes256/1.
Existing world/impostor regressions pass48568/2. GPU API+shader validation used.

[Music repair](parcel-music-host-repair.md): inherited relative LibVLC module path
fails initialization in actual bundle. Canonical native-executable-derived path
succeeds. Rebuilt real playback host passed local3s silent WAV atvolume0 through
Playing/time updates/Done/normal exit0, no audio-output errors. Remote/audible
parcel playback remains open. [Camera/voice audit](teleport-camera-voice-audit.md)
found no changed teleport interpolation or voice contract; functional voice and
actual fly-in diagnosis remain open, no guessed changes applied.

Canonical Release68643 source2c60e47 is built/verified/signed; owner subsequently
requested launch. Exact hash/PID48817 and login window verified; no login by root.
Doctor/configure/build/verify_app/deep signature all0;349 Mach-O files ARM64.
Private commands/logs/hashes: private-classic-avatar-position-w-20261110-01.
Owner closed68638;68643 now at login, no account/cache/settings/camera
changes or performance campaign resumption. Translator stopped; replay off.

[Failed68638 live result](travel-live-68638.md) remains failed; bounded GPU proof
is not full live acceptance. Earlier [travel repairs](travel-correctness-repair.md)
and [avatar ownership work](avatar-crowd-parity.md) remain included. Potential
VBO/fence publication seams are private/unapplied, not established live causes.
Cold cache backup remains private/intact. No release/private upload/new push.

[Latest crowded-region trial](crowd-live-68643.md): owner raised max animated
avatars11→40 and confirmed UI/walking/nearby-avatar movement responsive. Root
captured20sCPUtrace, no source/settings/restart changes or formal FPS comparison.
Earlier cloud/bad-inverse warnings remain separate unclosed correctness evidence.
Follow-up LEFT/RIGHT turning name-tag/body mismatch remains unresolved; original
rotation policy unchanged. Capture actual world age before a guessed fix.
One pure-data sample showed current world; snapshot+detach succeeded. It did not
capture a controlled bad turn. Avoid repeated disruptive debugger attachments.

[Display-settings audit](display-settings-lighting-audit.md) found original HiDPI
and scene-HDR wiring retained; physical display output stays SDR. Two confirmed
source defects need repair: deferred PBR emissive OFF is disconnected, and Glow
ON tests GL-only shader handles. Current check changed no code/settings. Owner
lighting/Lightbox/Auto Exposure complaint remains visually unverified; bounded
source math/open callbacks match original. Do not tune brightness from screenshots.

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

Original classic-avatar shader ABI and packaged LibVLC initialization defects
are repaired with actual failing-old/passing-new GPU and actual-host playback
proof. Prior loading/UI/depth/terrain/name repairs retained. Whole-viewer travel,
input/resize/HUD/crowd/ART/music/voice and shadow/sky/low-DD parity remain open.
Non1-scale final grain/dither coordinate parity remains open. Build/test counts
do not establish visual acceptance, product completion or FPS gains.

## Immediate executable action

68643 runs for owner-directed travel. Crowd responsiveness is owner-confirmed
after animated-avatar limit change; verify previously corrupt
avatar, outfit/teleport loading, HUDs, input/resize, names and real parcel music
with exact-build logs. If camera symptom persists, correlate camera pose and
published world source-frame/region; original camera code is unchanged. Voice
requires attended functionality before parity. No speculative camera/voice fix.
Earlier20FPS campaign remains paused; last controlled9.745FPS is not a new result.

## Non-negotiable open gates

Full original feature/pixel parity, prolonged stability, multi-scene performance,
strict source/runtime no-GL and independent release review remain open. Preserve
open-source non-rendering behavior, voices/media and native Metal ownership.
Do not infer full product parity or FPS from bounded test counts.

Do not mark feature parity from build/test success alone.
