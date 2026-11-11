# Active execution plan — native Alchemy Metal

**Owner:** Codex integrator. **Outcome:** docs/metal/PRODUCT.md, not a demo.
**Integrated checkpoint:** Commit `83567e407d`; original-mapped native repairs.
**Recovery:** [CURRENT](../../state/CURRENT.md), [NEXT_SESSION](../../state/NEXT_SESSION.md),
[deferred campaign](ultra-20fps.md), [archive](../../state/archive/INDEX.md).

## Current implementation — 2026-11-11

Owner requests unfinished RLVa, Debug/Developer features and OpenGL removal,
with original-viewer inspection before fixes. No viewer, utility, host or native
window launch so far; no screenshots. Owner now authorizes launching the exact
fresh viewer after checks for their manual confirmation. Utility execution and
screenshot comparisons are not requested. Owner-stopped20FPS campaign remains deferred.
Reference source1eb44bbd1bdc76d0100c75699bad1fb337e8cb45.

Latest: [native finalization/preview/media closure](native-finalization-and-media-closure.md),
and [settings/resource tail](native-settings-and-resource-tail.md),
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
| M-011 | Remaining owned GL authority | doing | Source/static pass; cleanup/build/foreign/runtime remain. |
| M-012 | Native UI automation | complete | Prior exact tooling/flow; no launch this goal. |
| M-013 | RLVa parity | doing | Sphere/silhouette/current-view repaired; broader audit open. |
| M-014 | Ultra20FPS campaign | deferred | Owner stop; original measured target unmet. |

## Complete-product checkpoint gate

M-008/M-011 advance through actual native owner/format/stencil fixes and proven
dead graph retirement. M-013 retains original-mapped RLVa repairs. Bounded tests
do not establish complete menu/RLVa/viewer parity. M-012 remains complete and
M-014 deferred. No release-readiness claim.

## Observed validation

Last verified full app ae2c601e0b805fe167bdb80dcef4cec7af226bd9, Release68801:
required doctor/configure/full build2519/static0,346ARM64/strict deep signature0,
viewer+owned media host direct imports0,332exact VLC modules/zeroGLSL/exact native
resources/notices. Prior d2d app preserved by rename. Receipt:
native-pool-cutout-canonical-validation.json. Later configure can modify bundle
metadata/resources before full build; source-pinned verification only.

At ae2 policy640/19, alrender564631/120, LLRender335391/442(one CPAL skip),
focused5861/8, world105889/23, reload2304/8 pass API/shader where applicable.
Root SMAA build82307/2552/4+world105889/23+reload2304/8 pass.
Root alpha build64115/focused10665/17+world105890/23+reload2304/8 pass.
Exact source/WIP/argv/binary hashes: native-{pool-cutout,smaa,cutout-alpha}-
registered-validation.json. Counts overlap; do not sum.
Matching legacy/root policy652/20 and phase-resource1894/40 pass after builds
57228/57070 exit0. Snapshot actual-body CPU184checks passes after16 baseline
failures; Release textual syntax0 and exact source peer clear.
Child normal389/2 exposes17 baseline failures then passes API/shader; optional
water caller policy352/1 exposes6 baseline flags then passes. Preview three/hero
two/compute-copy six Release checks0; compute-copy CPU1893/41 and physical-plan
projection pass. Root matching combined build/affected checks remain pending.

Production ae2 strict3504/910info/0blockers/0errors. Earlier all-source4094 has
two negative window-flag assertions flagged; retain them, no all-source pass.
Tools49/four models passed at unchanged-tool state; harness32last passed.
Optional utility older-source build/static/lookup/resources96characterfiles and
native dependency-edge proof passed without launch; refresh matching final core.

## Immediate executable action

Root owns canonical CMake/docs/integration. All agent source trees frozen and
released; no live GPU/build processes. Root water test registration/Compute debug
comment/docs are WIP. Commit freeze; run build-native-settings-stage.py for exact
doctor/configure/full-app commands, then verify-native-finalize-stage.py with
native-settings-phase-canonical build-state/prefix. Separate optional utility
refresh-native-appearance-stage.py builds/statically verifies current resources,
96character files and imports without execution. Run matching resource/input/
native policy, focused water/cutout/SMAA, world/water-cull/reload checks and strict
source/harness. Preserve all original physical/native ownership/publication keys.
After checks pass, launch the exact fresh viewer for requested owner confirmation;
record process/build identity, without a screenshot/full-parity claim. Bounded
RLVa wiring audit has no new confirmed omission; actual command behavior remains.

Preserve screen/VT/CVPX/CEF capabilities. Foreign/runtime no-context, full feature/
stability/performance and independent release evidence remain open. Keep all
utility unexecuted; viewer launch now authorized after checks. Keep prior evidence source-pinned.

## Non-negotiable open gates

Full original feature parity, prolonged stability, measured multi-scene
performance, strict source/runtime no-GL and independent release review remain
open. Preserve earlier lighting/self-turn/camera/voice issues. Do not alter
immutable product constraints. A bounded item never implies product completion.
Do not mark feature parity from build/test success alone.
