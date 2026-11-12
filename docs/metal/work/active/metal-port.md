# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Integrated checkpoint:** Commit `b7bc118d68`; cb840/68841 built/checked/launched.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[deferred campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current implementation — 2026-11-11

Owner requests unfinished RLVa, Debug/Developer features and OpenGL removal,
with original-viewer inspection before fixes. No viewer, utility, host or native
window launch initially. Owner subsequently requested ready viewer launch for
their confirmation; fresh26.4.0d68817 was observed as PID90460. No login action
or screenshot by agent; utility unexecuted. Owner-stopped20FPS campaign remains deferred.
Reference source1eb44bbd1bdc76d0100c75699bad1fb337e8cb45.

Latest: [native finalization/preview/media closure](native-finalization-and-media-closure.md),
and [settings/resource tail](native-settings-and-resource-tail.md),
plus [post-launch corrections](native-post-launch-corrections.md),
and [HUD selection/preview followup](native-hud-selection-and-preview-retry.md),
extending [raster/float/reflection and fixture admission](native-raster-and-legacy-fixture-admission.md),
[Scene Monitor/UI/RenderInfo](native-scene-ui-and-render-info.md),
[target/query/font](native-target-query-invariants.md),
[cache/coverage](native-pipeline-cache-and-coverage.md),
[feature/resource completion](native-feature-resource-completion.md),
[menu audit](developer-menu-admission.md),
[source tail](native-opengl-tail-removal.md),
[buffer visualization](debug-buffer-visualization.md),
[appearance loader](appearance-native-loader.md),
[RLVa mapping](rlva-native-owner-cleanup.md) and
[developer diagnostics](developer-metal-diagnostics.md).

Retain native world/material/avatar/terrain/shadow/sky/water/effects, UI/font/
HUD/media and original-mapped fixes. Network/inventory/editing/simulation/voice
remain in scope. No fallback renderer or translation backend.

Integrated foundation: original polygon offset/flush/target extent; always-float
Screen and explicit HDR conversion boundaries; reflection-level-zero shading;
native resource/FXAA/morph/LLUI invariant admission; dead GL tests/120assets/
programs/buffers retired only after reader/algorithm mapping. Original notices,
native viewer/utility shader staging and dependency edges retained.

Integrated finalization preserves lens/DoF/scope/callback/reference/epoch duties
without legacy post traversal. Bloom admission independent of Scopes; native
SSR cutout isolation permits valid probe-only frames; references always10bit.
Preview allocation retry and VLC lifecycle cleanup/order fixed. Eight fixed
targets and unused transient prewarm pool removed; actual-source Bloom shape
handles native snapshot sizes. Native GPU owner remains failure-aware.

SMAA bfff8/dd796 restores stencil mark/test and output-sized alternate metrics.
Alpha5fa7/b20 restores original HDR10/SDR8 RGB and final rawF16 coverage sampling;
dead preserve_alpha workaround/PSO dimension retired. Original cutout depth pins
remain separate from shared exact-far coverage. Matching root GPU controls pass.
Legacy3b5 removes displaced seed/copy/VB/program after that native companion
passed; exact stream boundaries remain. Later runtime parity stays open.

Phase d78cc2d779 retires34 skipped descriptions/three dead copies while preserving
physical plans/native owners/full-input publication; c65e5d588e removes unused
describes. Snapshot4f5 restores forced-failure native admission after184 CPU
checks expose16 baseline failures, with unchanged caller/RAII recovery boundaries.
Preview7f6633a3fe and heroce65f508a4 retire proven dead branches/wrapper, with live
native preview/profile/debug owners preserved. Water6473c5c77d restores normal
filter setting; 83567e407d restores original optional Compute-culling gate and
waves remain. 2c16d75147 removes only redundant resource Compute copy; actual
shader/local setting latch and full resource-input publication stay live.
Owner provisionally passed f254/68817. Later source review found inherited mask
and preview/media gaps: integrated9838/65b5/158f/a380/8a14 fixes; matching combined
build/tests now pass. Source map, baseline red controls and peers in new note.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 reference/build | doing | Canonical builds pass; independent fresh-checkout release remains. |
| M-002 | GL inventory and release audit | doing | Owned source/static gates pass; foreign/runtime remain. |
| M-003 | Native presentation/lifecycle | doing | Original native finalization/leases; prolonged stability open. |
| M-004 | Resource/transfer/sync ownership | doing | Native owners retained;34dead phase descriptions retired. |
| M-005 | MSL/binding behavior | doing | Original formats/math/stencil advance; broader parity open. |
| M-006 | UI/fonts/HUD/media | doing | Native bounded tests pass; full behavior/stability open. |
| M-007 | World/material/shadow correctness | doing | Float/reflection repairs; broad original parity open. |
| M-008 | Advanced views/effects | doing | SMAA/cutout/lens/scope closure advances; broad parity open. |
| M-009 | Feature/stability/native-only release | doing | Matrix/stability/independent release remain open. |
| M-010 | M1 Max measured optimization | doing | Earlier baselines retained; multi-scene contract unmet. |
| M-011 | Remaining owned GL authority | doing | Source/static/build pass; owner/foreign/runtime remain. |
| M-012 | Native UI automation | complete | Prior tooling retained; exact-build requested launch observed. |
| M-013 | RLVa parity | doing | Sphere/silhouette/current-view repaired; broader audit open. |
| M-014 | Ultra20FPS campaign | deferred | Owner stop; original measured target unmet. |

## Complete-product checkpoint gate

M-008/M-011 advance through actual native owner/format/stencil fixes and proven
dead graph retirement. M-013 retains original-mapped RLVa repairs. Bounded tests
do not establish complete menu/RLVa/viewer parity. M-012 remains complete and
M-014 deferred. No release-readiness claim.

## Observed validation

Prior owner-passed full app f254feb048e832c101ec62f3e02335eeff13a2cb, Release68817:
required doctor/configure/full build1447/static69509 exits0,346ARM64/deep signature0,
viewer+owned media host direct imports0,332exact VLC modules/zeroGLSL/exact native
resources/notices. Prior stages preserved. Receipt:
native-settings-phase-canonical-validation.json. Binary SHA256
285c561302672cb9fab2fe4ab4ef402297796ca2272b579765d57b3abd4ab1eb.
Prior Info.plist staging defect/repair logs preserved; rename before configure.

Root77976 policy652/20, full alrender563966/115, focused11054/19, world105890/23,
water-cull250/1 and reload2304/8 all pass. GPU logs confirm API/GPU validation.
native-settings-phase-registered-validation.json holds argv/source/hashes; binaries
unchanged after packaging repair. Snapshot184checks also pass at clean f254 after
16baselinefailures. Child normal389/2red17→0, water caller352/1red6→0, independent
physical-plan/projection/source peers remain pinned. Counts overlap; no summing.
Earlier LLRender335391/442(one CPAL skip) remains older-source evidence.

Production f254 strict3504/909info/0blockers/0errors. Earlier all-source4094 has
two negative window-flag assertions flagged; retain them, no all-source pass.
Tools49/four models passed at unchanged-tool state; harness32last passed.
Optional utility f254 build/static83133 passes ARM64/signature/imports, exact
native MSL/metallib/notices/96character bytes and zeroGLSL without execution.
native-settings-phase-appearance-validation.json. Launch31758 exits0 and confirms
new exact binary PID90460 alive; native-settings-phase-viewer-launch.json.
Exhausted-plan source audit finds no concrete ordinary ownership/freshness bug;
typed pool recovery clears and checks current-frame/commit publication. Forced
native pool-failure injection remains an unexecuted diagnostic question, not a
proved rendering bug. No !mFullSize guard; full viewer parity stays unverified.
Owner says68817 “pass...for now”; exact7,840file/link/byte signed baseline copy
preserved. No later fixes are inferred from that observation. New child controls:
mask80red44→0; preview20red3→0 and combined1860/6 API/GPU; media same-source
599red112→831/2green. Root tools50/shell/harness pass; corrected -B wrapper and
actual separate followup configure75939 pass, old binary unchanged/installOFF.
Latest clean114: followup doctor/configure/full build98310 and static44362 exit0;
346ARM64/332exact VLC/signature/imports/native resources/zeroGLSL pass. Strict
source3505/911info/0blockers/0errors. Root55175 policy652/20, LLRender336356/445pass
+one known CPAL skip, alrender563966/115 pass; GPU/API validation observed.
Alpha4 same-source identity stage83415/static7544 pass, Release/LTO OFF; unchanged
renderer test hashes and identity+policy662/21. ZIP84942:319936679bytes/7840exact
files+links+modes after extraction/strict signature0/checksum0, no execution.
Evidence: followup-* and manual-alpha-* receipts under feature-completion-20261111.

## Immediate executable action

All identified fixes integrated through8ce7: selected HUD faces/debug overlays,
per-owner current-buffer preparation (including vertex-only admission), four
preview destination retries and early bake metadata bootstrap. Scheduler/generator
and metadata-valid no-backing bake adoption preserved; original projections,
actual old-body red/green controls and peers in linked followup note. No identified
coding fix remains; full scene/physical OOM evidence stays unverified.

Combined9f42/68836 doctor/configure/build0/0/0, Release/LTO OFF. Full matching
LLRender453086assertions/474passed+one CPAL skip; alrender563966/115; policy652/20
pass. Both renderer logs confirm API/GPU validation. Static346ARM64/332exact VLC/
signatures/owned imports/resources/zeroGLSL pass; strict3514/912info/0block/error.
ZIP319934684bytes/7840exact files+links+modes after extraction/signature/checksum0.
SHA2567e74d96fb3c991286fd4bbb44ac56fae3f21a72f9aed0b7e58d2f77535d2314e.
Evidence hud-preview-alpha-{foreground-build-state,validation,registered-validation,
strict-source,zip-validation,checks-state}.json. Background attempt interrupted130
intentionally, same source/objects resumed; job-wrapper restarts blocked by guard.
All temporary jobs removed; passed f254/older ZIP unchanged; no new product launch.

Owner-authorized release442275652 manually replaced:26.4.0.68841-alpha4,
https://github.com/levdarkheart-lgtm/AlchemyFork/releases/tag/26.4.0.68841-alpha4 .
New ZIP319935071bytes+SHA server hashes verified; tag exactcb840/owner-tested source.
Oldtag9f42 preserved; old68836 assets replaced only after newverification.
facelight-cutoff-20261111/release-replacement.json; no Actions/billing changes.
This correction is separate from the closed porting goal. Concurrent research
files untouched/excluded; full-product completion remains unclaimed.

Local [busy-turn investigation](busy-turn-local-investigation.md):68845 failed; push reverted0f7; explicit owner approval required before future fix pushes.
Preserve screen/VT/CVPX/CEF capabilities. Foreign/runtime no-context, full feature/
stability/performance and independent release evidence remain open. Keep all
utility unexecuted; prior f254 launch observation stays source-pinned.

## Non-negotiable open gates

[Shadow queries](shadow-camera-occlusion.md): implemented/tested68849; enabled181.281s profile analyzed, benefit unproven; keep local pending owner acceptance.

Full original feature parity, prolonged stability, measured multi-scene
performance, strict source/runtime no-GL and independent release review remain
open. Preserve earlier lighting/self-turn/camera/voice issues. Do not alter
immutable product constraints. A bounded item never implies product completion.
Do not mark feature parity from build/test success alone.
