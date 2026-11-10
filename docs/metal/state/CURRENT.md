# Current engineering state — native multi-avatar repairs

**Observed:** 2026-11-10, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `97dd825aaf428da4a50e6b8bf1659d5b60d62eda`.
**Last observed viewer:** 68633 PID95871, Release ARM64, built/verified/signed.
Owner then authorized closing older68626PID44312 and starting the repaired app.
Old PID exited; owner logged in and drove travel. Live QA subsequently FAILED
with repeated UI stalls and geometry corruption; owner may close the viewer.
Owner explicitly requested implementing the four avatar gaps. All prior fixes
retained. Earlier20FPS goal remains paused; no settings, camera, avatar travel,
cache/account changes occurred. Restart followed the owner's explicit request.

## Latest live travel observation — FAILED

[Travel findings](../work/active/travel-live-68633.md): owner reported teleport/UI/
movement/resize stalls, transient missing names and screenshot-proven corrupt
rigged triangles. Saved logs, both screenshots, weak3-observation sample and a
real10s CPU trace privately. Owner can close; no further capture required.
Trace shows5.580 sampled main CPU seconds and345 runloop calls, max82.18ms gap:
no continuous deadlock captured, but that does not disprove intermittent freezes
or establish keyboard delivery/visible publication. Main/probe log rows are mixed.
Confirmed remaining source gaps: UI cannot refresh without current valid world;
fetched terrain lacks original placeholder contract; native impostor publication
success is not checked before clearing retry; finite skin fallback differs from
original and may stretch mixtures. Actual missing terrain member/bad skin operand/
name-tag cause remain unknown. NO source edits or restarts during observation.
Earlier GPU tests do not supersede this failed live check. Performance goal paused.

## Implemented original contracts

Exact reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
[Detailed implementation and limits](../work/active/avatar-crowd-parity.md).

- **GPU cost measurements:** completion-safe native GPUStartTime/GPUEndTime
  receipt feeds ALGPUTimer and existing avatar ART policy; attachment timings
  update still-live owners. Pending/errors never publish fabricated zero samples.
  Requests from UI/autotune queue by UUID. One actual isolated subject per main
  frame runs after original impostor update and main-camera/CAMERA_WORLD restore.
  Reuses warmed renderer, separate targets and coherent saved shadows/SSR; current
  shadow constants are packed before drawing. Complete opaque/forward/alpha work
  commits once, without a visible impostor publication, GPU wait or UI mutation.
- **Impostor ownership:** shared ALMetalAvatarResources cache holds one immutable,
  validated quartet per avatar UUID across main/hero/ordinary owner swaps.
  Queued batches and native commands retain record leases through completion.
  Replacement/destruction cannot free earlier consumers; invalid publications
  keep preceding coherent data. Device/HDR/normal mismatch invalidation remains.
- **Loading:** original skip behavior now preserves other content when an avatar
  is not fully loaded and original emits no geometry. The explicit original
  pass0/particles-disabled debug sphere still lacks its native producer and
  remains incomplete; no silent omission waiver was introduced.
- **Body placeholders:** direct joint-mesh capture resolves requested/Default/Null
  owners without changing UI slots. Immediate update precedes activity changes;
  original raw placeholder versus sRGB ready-bake sampling preserved, with alpha,
  depth, invisible-bake/caster rules and world/shadow texture identity unchanged.

Seventeen original policy methods/seven settings blocks matched before repair;
ranking, full-detail cap, complexity/mute/friends and cadence remain unchanged.
No cross-avatar skin-palette alias was found. Generic ALQueryPool timing spans
remain unsupported: avatar callers now use their actual native command receipts.
GPU result is isolated command execution envelope, not whole-frame/busy-time;
native pass setup/clear overhead is included, so do not call it identical in
numerical scope to OpenGL elapsed queries. Max16 bounded completion log entries
per subject family permit future live verification without avatar identifiers.

## Executed checks and build

Doctor, configure, Release build, verify_app and deep strict codesign all exited0.
Exact commands/source/hashes: ignored
`artifacts/metal/private-avatar-parity-repair-20261110-01/build-final.json`.
verify_app inspected349 Mach-O files with zero invalid non-arm64 slices.
Canonical app: `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`.
68633 executable SHA256: `5695b820fcd7a7a3252d1dedb4e91dd8cd90a0595a4afb4ec13417e58ffbac83`.
Metallib SHA256: `a155144459e5899d838c35834d31ecd0b4c5ae67e511e26e81523e464cd994f8` (MSL unchanged).
Owner-run launch receipt: ignored
`artifacts/metal/private-avatar-parity-owner-try-20261110-01/launch.json`.
Exact path/hash/PID checked; no diagnostic/session overrides injected by launch.
Login screen is not a crowd/region/settings/visual-parity observation.

Actual Release native GPU tests with API+shader validation, all exit0:

| Check | Assertions /cases | Actual scope |
| --- | --- | --- |
| Command timing + real renderer batch/slot admission |369 /4| Positive native time, queued-delay exclusion, abandonment, no-wait receipt, actual8x8 clear pixels. |
| ALGPUTimer + UI/buffer regression cases |375 /5| Actual native receipt delivery/failure recovery, previous placeholder/upload behavior. |
| Two-avatar resource cache |395 /2| Distinct quartets/view swaps, replacement/erase, format/device invalidation, CPU and GPU completion leases, exact4MRT+depth. |
| Avatar sampling/G-buffer |31498 /1| Real rigid/skinned/forward alpha, independent raw-view oracle, true identity, alpha/cutoff/MRT/depth invariants. |
| Existing impostor replay |126 /1| Native baked G-buffer rendering retained. |

Evidence JSON/logs/hashes are in the same private directory. Initial backend
compile failed on a newly introduced invalid failure-helper name, corrected to
existing renderer failure handling; rebuild passed. Two fixture input offsets
were corrected during review before GPU runs. Final committed build passed.
Agents source-reviewed independently; root ran all builds/GPU checks/integration.

These backend checks do not instantiate the full viewer avatar/cull manager.
Actual queued profiling/ART effects, loaded/unloaded real avatars and cross-view
impostor visuals still need live verification. No crowd FPS/parity improvement is
claimed. New capture-owner restoration does not make inherited generateImpostor
manual global-state cleanup fully exception-safe.

## Preserved prior work and unresolved observations

[Cold-start/environment checkpoint](archive/CURRENT-cold-start-environment-repairs-2026-11-10.md)
records actual zero-file cache test, main-thread UI ownership, pending UI frame
publication, correct EEP gamma/no-post policy and completed default-probe reset
continuity. Those repairs (68630/code76413) are ancestors; visual improvement
still needs observation. No original viewer launch in the current investigations.
Original8978files/1703059096bytes cache remains preserved at ignored
`artifacts/metal/private-empty-cache-start-20261110-01/original-cache`.
Do not restore it over the active live cache. Saved account/settings untouched.

Low-DD terrain/water, reported sky/interior/shadow parity and random missing shoe/
outfit spikes remain unresolved beyond those confirmed source repairs. Original
terrain uses infinite-far culling; shared far-plane/water rules match. No guessed
terrain cutoff or shadow quality reduction was introduced. Prior indexed HUD/
alpha/PBR/upload retry repairs remain included. All CUA phases completed;
Translator remains fully stopped, Xcode replay remains stopped. Private captures,
cache and logs stayed ignored and were not uploaded. Root owns shared files.

## Exact next actions

Owner was told captures are sufficient and may close the stuck viewer. Read the
travel findings and private CPU/trace analysis. Next source work must restore
independent native UI presentation and original terrain loading semantics, fix
impostor publication success/retry ownership, and identify the bad skin product
before a matrix repair. Names still need actual text/value/bounds evidence.
No automatic restart/travel, performance-goal resumption or quality cuts.

## Complete-product advancement assessment

| Milestone | State | Evidence /remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Cross-view avatar resource ownership now GPU-tested; live broad parity remains. |
| M-012 Native UI automation | complete | Prior exact-build flow retained; no new CUA phase. |
| M-014 Ultra20FPS campaign | deferred | Owner-paused; no new FPS or speedup claim. |

[Paused campaign](archive/CURRENT-20fps-owner-stop-2026-11-10.md): last controlled
current-source9.745FPS;20FPS remains unmet, no hardware ceiling proved.
Full parity, prolonged stability, multi-scene performance, strict no-GL and
independent build/review evidence remain open. Product is not complete.
