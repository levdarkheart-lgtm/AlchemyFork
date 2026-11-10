# Native multi-avatar correctness repair — 2026-11-10

**Implemented checkpoint:** `97dd825aaf428da4a50e6b8bf1659d5b60d62eda`.
**Original contract:** `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
Owner explicitly requested implementing all four audited gaps. Root integrated
shared pipeline/render-state changes; agents owned device timing, avatar cache,
body placeholder resolution and focused tests, with independent source review.
Build68633 was prepared/verified, then launched at owner's explicit request.
Old68626 closed; exact68633PID95871 login screen observed. Owner handles login
and travel. No live crowd/region/settings/FPS comparison yet.

## Preserved policy

Seventeen avatar policy methods and seven settings blocks matched exact original
before implementation: screen-area ranking, self rank1, configured full-detail
limit, visibility/complexity/mute/friends rules and update cadence remain unchanged.
Registered maximum full-detail avatars is12; effective saved settings can differ.
No cross-avatar skin-palette alias was demonstrated. Independent palettes, cloth
copies and byte-validated immutable frame-data reuse remain intact.

## GPU profiling now has an actual native producer

The original profiles an isolated complete avatar or selected attachment and
polls GPU timestamps without waiting. Native generic ALQueryPool spans were
invalid, leaving the avatar's derived GPU-time field at0 and ART thresholds
without measurements. Generic query spans remain unsupported; the avatar caller
now uses an explicit native completion receipt for the actual measured work.

- ALMetalCommandCompletion::gpuTime polls the retained command's GPUStartTime/
  GPUEndTime only after successful completion; pending, abandonment, errors and
  invalid timestamp pairs cannot publish fabricated zero-success samples.
- ALGPUTimer accepts that native completion and delivers it once through the
  existing LLVOAvatar::readProfileQuery/updateTooSlow path. Attachment results
  use the same receipt and update only still-live object owners.
- profileAvatar queues UUID-scoped requests, including calls made during UI draw.
  One subject is dispatched per main frame after updateImpostors restores
  CAMERA_WORLD and its caller restores main camera/matrices/viewport. Dead or
  detached subjects are removed; a loading one rotates instead of blocking all.
- The profiler borrows the warmed renderer with a separate temporary capture;
  current shadow resources and coherent SSR history remain pinned by the saved
  owner. packDeferredConstants refreshes current main shadow matrices before
  profiling. Exact original uses those main matrices even with its profile
  camera; no new shadow rebase was invented.
- Actual full-resolution opaque, opaque-forward and alpha writers run in one
  command buffer. Completeness is required before commit/measurement. The target
  is never published as the avatar's visible impostor. Main/probe owner, renderer,
  frame-data, camera/cull and shader are restored on ordinary return paths.
- Admission checks command slots without waiting; no new synchronous PSO build,
  CPU timing substituted for GPU time, or repeated GPU completion wait occurs.

The measured value is an isolated native command's GPU execution envelope,
excluding pre-command queue delay. It is not whole-frame time or busy-time.
Its native pass setup/clear overhead is included; it is not numerically identical
in scope/cost to an OpenGL elapsed query around retained draw calls. Geometry and
lighting features are preserved. Max16 bounded completion receipts per subject
family permit later live verification without logging avatar identities.

## Impostor publication belongs to avatars across views

ALMetalAvatarResources owns one validated immutable native four-texture record
per16-byte avatar ID. Main, hero and ordinary capture owners share that cache.
Per-owner initialization prunes only genuinely incompatible device/HDR/normal
records; a new probe owner cannot erase a valid main publication. Same-device
format transitions also reconcile at the normal/bake format boundary.

The actual pipeline uses this cache for publish/find/erase. Lookup validates the
requested device/formats. Each queued impostor batch holds a strong record lease;
the native command retains it through completion. Replacement/destruction/cache
reset therefore cannot free an earlier consumer's textures. Rejected quartet
publication keeps the preceding coherent record. Avatar destruction erases both
capture references to the same logical cache. No global last-avatar slot exists.

## Loading and placeholder behavior

getMetalAvatar now skips a not-fully-loaded avatar where original renderAvatars
produces no geometry, preserving other avatars and world content in that view.
The original pass0/particles-disabled debug-sphere case remains explicitly
incomplete because its direct native diagnostic producer is still absent. This
repair does not silently waive that unsupported geometry or publish its omission.

Direct joint-mesh capture resolves original requested -> Default -> Null texture
owners without touching UI slots. Immediate updates precede activity changes;
actual selected owners/activity/stats are preserved for world and shadow capture.
Fallback placeholders use the original slow-bind raw sampling, while ready bakes
retain sRGB decode. Alpha, depth, authored invisible bakes and caster admission
remain unchanged. If every candidate is unready, existing completeness rejection
remains. This is separate from the earlier generic UI/HUD fallback repairs.

## Executed checks and limits

Ignored local evidence: `artifacts/metal/private-avatar-parity-repair-20261110-01/`.
Exact commands, exits and binary hashes are in JSON alongside the actual logs.
Tests are Release on M1 Max with API and shader validation:

| Executed native check | Result |
| --- | --- |
| Command timing incl. real renderer batch/slot gating | 369 assertions /4cases, exit0 |
| ALGPUTimer + previous UI/vertex-buffer regressions | 375 /5, exit0 |
| Two-avatar cache/view/replacement/destruction/completion lifetime | 395 /2, exit0 |
| G-buffer + rigid/skinned/forward-avatar raw/sRGB sampling | 31498 /1, exit0 |
| Existing impostor G-buffer replay | 126 /1, exit0 |

The first backend build exposed a newly introduced invalid failure-helper name;
corrected to the existing renderer's failure path, then rebuilt successfully.
Two fixture rigid-input offsets were corrected during source review before GPU
execution. No failed GPU result is relabeled as a successful run.

Doctor/configure/Release build/verify_app/deep strict signature are recorded in
build-final.json for the committed source. No MSL source change was made.
Actual viewer-manager queue dispatch, two real avatars across main/mirror views,
loading/default-to-ready transitions and ART behavior still need live verification.
Backend tests exercise the actual cache/timing/rendering primitives; they do not
instantiate the whole viewer's avatar/cull manager or establish crowd FPS parity.
Inherited generateImpostor global-state cleanup is not fully exception-safe; the
new capture-owner restoration does not constitute a global exception audit.
All previous correctness/performance fixes remain included. The owner-paused
20FPS campaign is not resumed, and no FPS improvement is claimed here.
