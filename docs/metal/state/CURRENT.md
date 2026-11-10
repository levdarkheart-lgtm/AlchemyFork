# Current engineering state — cold-start and environment correctness

**Observed:** 2026-11-10, M1 Max / 64 GiB / macOS 26.5.2 / Xcode 26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `76413f2fcf76e528fcfe3740635111dcd4a54ea8`.
**Prepared app:** 68630, Release ARM64, built/verified; NOT launched.
**Running viewer:** older 68626 PID 44312, source `d18641c3557996807de03d99a105419161e95dfb`.
PID existence rechecked after build. On-disk canonical app is now 68630; do not
attribute its new executable hash or fixes to the running older process.
The earlier 20 FPS campaign remains paused. Owner requested code comparison;
no restart solely for source inspection, no quality changes or avatar travel.

## Empty-cache startup actually executed

68626 was launched ONCE at about 19:33 UTC with API validation enabled, no
benchmark/private renderer flags and no alternate user profile. Old 43249 had
already exited. Entire active cache `/Users/dekstroza/Library/Caches/AlchemyNext`
was renamed intact to ignored
`artifacts/metal/private-empty-cache-start-20261110-01/original-cache`.
Recreated SAME active path had **zero files before launch**; 8978 files and
1703059096 logical bytes retained in backup with same-filesystem inode proof.
Settings, saved logins, account/outfit directories and Apple driver caches were
untouched. No legacy cache migration source existed. Do not restore live cache.
Effective 6144 MiB cache/35% asset policy matches exact original source.

Owner reported UI unresponsive, then recovered. Log 19:34:31–38 shows ready world
color but pending 2D UI texture withholding the entire ordered publication;
success resumed 19:34:42. Main/probe PSOs finished 19:33:40/41 (~91/73 ms), well
before this interval. Continuing log progress does not establish a deadlock.
The 60-second CPU sample completed (0); the later five-second sample began
19:34:52 AFTER reported recovery, so cannot prove the earlier blocking stack.
**Original and native both render world/UI on the main application thread.**
Native adds complete-frame publication dependencies and explicit GPU waits.
No persistent stale canvas-size defect was proved by source inspection.

## Confirmed original-contract repairs, all committed

Exact reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
All prior HUD, upload and performance repairs remain ancestors; none reverted.

- `b75e79cb73`: authoritative ALTextureSlot fast/slow/raw paths now honor original
  pending-texture immediate-update/default-image behavior. Previously a missing
  UI mirror could suppress the entire world/UI frame. Preserve ordered prefixes,
  original sampler distinctions and DELETED ordering; reject cyclic/unready
  defaults and unsupported slots. Real native RED/GREEN test reproduces failure.
- `0ef3e16a0b`: display legacy gamma uses effective sky probe ambiance == 0,
  as original colorCorrect, independently of surface classic mode. Explicitly
  authored zero ambiance is legal but nonclassic. Original no-post policy now
  consistently controls exposure, tonemap and bloom extraction/streaks while
  retaining alpha glow. Actual reported sky eligibility remains UNKNOWN.
- `76413f2fcf`: same-layout environment reset preserves the completed default
  radiance/SH pair until replacement is ready, like original initReflectionMaps.
  Native previously discarded it and bootstrapped dark lighting. Strict layout,
  device, resource and generation checks precede retention. Advance every slot
  epoch, clear nondefault publication receipts and pending tickets; incompatible
  layouts/devices and incomplete pairs still fully release. This is a grounded
  dark-blink mechanism; live visual improvement remains UNVERIFIED.

Bounded logs record actual encoded display policy and default-reset preservation
(max 32 events each). An encoding receipt is not GPU completion/presentation.
Agents reviewed source/private patches only; root integrated all shared edits.
No original executable was launched during this cold-start investigation.
Existing reference 67611 is source f66dedb2615be7981a5c30327c8fb5fe333bebde,
with exact 1eb renderer plus default-off observer/accessibility/profile support.

## Executed build and focused native checks

Doctor, `tools/metal/build.sh configure`, `tools/metal/build.sh build`,
`tools/metal/verify_app.sh <canonical app>` and deep strict codesign all exited 0.
Exact commands/exits/source and hashes:
`artifacts/metal/private-cold-start-lighting-repairs-20261110-04/build.json`.
Canonical app: `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
68630 executable SHA256: `fe0c6a41367b40352a76e941111af32a830a9818f3b84b900553619918dbda3a`.
Bundled metallib SHA256: `a155144459e5899d838c35834d31ecd0b4c5ae67e511e26e81523e464cd994f8`.
Running 68626 executable identity was pinned at launch (not its overwritten disk file):
`1050ef5ab4907c1fb15874a4bb216c81b057bc82e03030dfe446dc865570675a`.

Actual Release checks with API + shader validation, all exit 0:

| Check | Result | Scope |
| --- | --- | --- |
| Pending UI + vertex buffers | 201 assertions / 3 cases | Real gGL slot/capture/pixel readback; pre-fix pending test exit 42, repaired exit 0. |
| Display post | 1824 / 1 | Native transfer branches, not LLPipeline sky selection. |
| HDR bloom | 67 / 1 | Native extraction/pyramid math, not the no-post caller. |
| Ordinary probes | 134 / 1 | Filtering/publication primitives, not manager reset. |
| Hero probes | 589 / 1 | Cube publication primitives, not manager reset. |

Private evidence: `artifacts/metal/private-cold-ui-fallback-fix-20261110-02/`,
`private-eep-display-policy-fix-20261110-03/`, `private-default-probe-reset-fix-20261110-04/`.
Live same-layout manager reset, replacement and visual transitions remain untested.
Source review and these checks do not establish complete lighting/visual parity.
`python3 tools/metal/validate_harness.py` and `git diff --check` passed (0).
Earlier HUD/upload checks are in the indexed
[HUD checkpoint](archive/CURRENT-avatar-hud-upload-correctness-2026-11-10.md).

## Remaining source investigations

Sun/projector matrices, reverse-Z, bias and cascade/caster paths were compared;
no new concrete defect proved. Native sun cascades are recaptured, not served by
an invented sun cache. Shared projector cache matches original. Logs alone do not
prove actual shadow texels/current-view coverage correct.
Original terrain culling deliberately has infinite far reach; object draw distance
need not remove loaded terrain. ALFarPlane/ALWaterField match exact original.
Native terrain factors remain >=1, full-height evaluation has no DD fade, no far
plane GPU terrain culler, and water uses loaded scene depth testing. A missing
cube-only raster-span tessellation cap may add cube work, not explain MAIN low-DD
terrain loss. Actual affected terrain admission/map publication/pre-water depth
remain unobserved. Do not claim this symptom fixed or patch DD speculatively.
Random missing shoe/outfit spikes likewise remain unproved; re-Wear restored it.

Private source reports under `/private/tmp/alchemy-metal-*-20261110*/` cover
cold CPU/thread/canvas, base inputs, shadow cache, low-distance terrain/water,
EEP display policy and default-probe reset preservation. Private captures/cache
backup remain ignored and were not uploaded. All CUA phases completed.
Translator remains fully stopped; Xcode replay remains stopped.

## Multi-avatar source follow-up

[Crowd comparison](../work/active/avatar-crowd-parity.md): ranking, full-detail
limits, complexity/mute rules and update cadence match exact original (17 methods).
Confirmed remaining gaps: GPU ART timestamp spans have no native producer;
published impostor textures are capture-local and unavailable to the swapped
reflection owner; unloaded avatars can invalidate an entire reflection face
where original merely skips them; direct pending body bakes miss default-image
fallback. None is repaired in 68630. No palette cross-avatar alias found.
This audit changed docs only, no launch/build/crowd test or FPS claim.

## Exact next actions

Keep existing live session intact while reporting the confirmed source repairs.
Prepared 68630 is available for the next owner-directed viewer run. Observe actual
Midday/Sunset/Midnight transitions and encoded/reset receipts before declaring
blink or lighting resolved. A controlled reference/native comparison is still
needed for remaining sky/interior/shadow/terrain claims; use actual affected
geometry, current matrices and depth coverage rather than speculative cutoffs.
Root owns shared source/docs/build/runtime; agents idle after read-only review.

## Complete-product advancement assessment

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Original display/reset contracts repaired; actual broad parity still open. |
| M-012 Native UI automation | complete | Prior exact-build flow retained; current CUA phases closed. |
| M-014 Ultra20FPS campaign | deferred | Owner paused; no new benchmark or speedup claim. |

[Paused campaign](archive/CURRENT-20fps-owner-stop-2026-11-10.md): last controlled
current-source result 9.745 FPS; 20 FPS remains unmet, no hardware ceiling proved.
Full parity, prolonged stability, multi-scene performance, strict no-GL and
independent build/review evidence remain open. Product is not complete.
