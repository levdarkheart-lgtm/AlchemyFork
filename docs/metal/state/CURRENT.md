# Current engineering state — native Edit Rotate repair

**Observed:** 2026-11-11 UTC, M1 Max / 64 GiB / macOS 26.5.2 / Xcode 26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `7710ae4f977295d7c3539516a2d3814485a7db45`.
All previous renderer, avatar, HUD, loading, environment and music repairs remain.
The broader porting goal and earlier 20 FPS campaign are paused. Translator and
Xcode replay remain stopped. Owner explicitly prohibits starting the viewer.
The viewer is CLOSED: final process inspection found no running Alchemy app.
No login, settings, account, cache, camera, region or object changes in this repair.

## Confirmed Edit Rotate defect repaired

[Original mapping and validation](../work/active/object-rotation-repair.md).
Exact original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` uses position-only,
uniform-color Debug geometry for rotation rings. Native Debug metadata omitted
MAP_VERTEX; immediate buffer admission failed and invalidated authoritative UI
captures. Previous UI could remain displayed, making mode selection appear absent.
The saved owner-trial log corroborates mask0 Debug rejections at washer/fan vertex
counts, with successful publications interspersed; not one continuous freeze.
Declare the actual native layout before shader creation. Also copy original cull
state for nonindexed immediate geometry, previously handled only for indexed
batches. No input/permission gates, hidden geometry or shader math changes.

M1 Max Release tests under Metal API and shader validation:

| Check | Result |
| --- | --- |
| New Debug configuration/immediate GPU fixture | exit0; 62 assertions / 1 case |
| Same frozen fixture against previous cull implementation | exit42; 4 failures / 62 assertions |
| Restored candidate plus nearby native UI/metadata regressions | exit0; 419 assertions / 14 cases |

The test exercises real native shader configuration, immediate helpers, capture
and GPU pixels against independent triangle oracles. It does not link the newview
loader or exercise mouse handling; loader placement is source-reviewed and app-
compiled. Production and test source reviews approved. Live rings, picking,
object dragging and simulator rotation remain UNVERIFIED. No FPS claim.

## Exact build and evidence

Canonical Release **26.4.0.68648**, clean renderer source checkpoint7710ae4f97.
`tools/metal/doctor.sh`, `tools/metal/build.sh configure`,
`tools/metal/build.sh build`, `tools/metal/verify_app.sh <app>` and
`codesign --verify --deep --strict --verbose=2 <app>` all exited0.
Slice inspection checked349 Mach-O files, zero invalid slices. Canonical app:
`build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
Executable SHA256:
`c96203506cfedcfebef3aa92514d839a3e1b8427efd1ce294e104413a314b9b7`.
Metallib SHA256:
`52d5aec4853511526cc5198f077fdb289d566c602271464f57c3f6174314e36c`.
Root ran tests/build; peers source-reviewed. No fresh-checkout release claim.

Ignored evidence: `artifacts/metal/private-object-rotate-audit-20261110-01/`.
`final-build-identity.json` pins app/source/hashes and exact command receipts;
`cull-red-control.json` records temporary baseline and restored candidate.
No raw logs, screenshots, cache or private capture uploads are authorized.
The inspection-only CUA selection earlier opened a login-only process; it was
closed with SIGTERM after the owner instruction. No login occurred. No further
viewer launches or screenshot comparisons are allowed by this handoff.

## Earlier fixes and remaining symptoms

[Classic-avatar repair](../work/active/classic-avatar-position-w.md) restores XYZ/W1
at the shared skin/cloth shader entries, matching the original attribute contract.
Old shader fails10/256 assertions; repaired256/1 and existing48568/2 GPU tests pass.
Owner later reported geometry fixed among roughly76 avatars; full parity remains
open. [Music repair](../work/active/parcel-music-host-repair.md) fixes packaged
LibVLC module resolution and failed-init cleanup. Actual playback host passed
silent local3sWAV volume0 through Playing/time updates/Done/normal exit0; remote
parcel/audible playback remains unverified. All earlier repairs stay included.

[Prior crowd trial](../work/active/crowd-live-68643.md): owner raised animated-avatar
limit11→40 and confirmed responsiveness. LEFT/RIGHT self-turn body/name-tag lag
remains open; original CPU turn code matches. One current-backing snapshot did
not establish actual presented world age or capture a bad turn. Do not repeat
intrusive debugger sampling. [Camera/voice audit](../work/active/teleport-camera-voice-audit.md)
found no source-proved incorrect port; actual fly-in cause and attended voice
functionality remain open. No speculative camera/voice changes were applied.

[Display/lighting audit](../work/active/display-settings-lighting-audit.md) found
HiDPI restart wiring and scene HDR connected; physical output remains SDR.
Deferred PBR emissive OFF and native Glow ON callback defects are confirmed but
UNFIXED. Preserve original inline alpha/HUD emissive exceptions in future work.
Lightbox/EEP and bounded exposure math match original. Reported bright/dark
lighting remains OPEN; no matching original/EEP/Look/exposure proof, no guessed
brightness change. The previous detailed state is preserved in the
[indexed archive](archive/CURRENT-classic-avatar-music-display-audit-2026-11-11.md).

## Complete-product advancement assessment

M-006/M-008 editing rendering advanced through a confirmed missing-layout and
cull-state repair with actual GPU failure/success evidence. Editing remains
unverified as an end-to-end feature. M-014 stays deferred: last controlled
current-source9.745FPS, 20FPS unmet, no hardware ceiling proved.

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Debug geometry and original avatar contracts repaired; live parity remains. |
| M-012 Native UI automation | complete | Prior exact-build flow retained; no new launch is authorized. |
| M-014 Ultra20FPS campaign | deferred | Owner-paused; no new FPS claim. |

## Exact next actions

The owner requested a normal push of ALL accumulated fixes to the feature branch;
verify final remote HEAD equals local HEAD and record the push receipt privately.
Do not resume broad implementation, launch the viewer or start visual comparison.

Full original sky/interior/shadow/low-DD/HUD/crowd parity, prolonged stability,
ART/loading-sphere behavior, non1-scale final noise, real media/voice, strict
source/runtime no-GL and independent release review remain open. Full parity,
strict no-GL and independent build/review evidence remain open release gates.
No release
readiness or full product completion claim. Cold cache backup remains private
and intact; do not restore it over the active cache.
