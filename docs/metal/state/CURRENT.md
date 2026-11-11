# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `83567e407d` (newer than the verified viewer app).
The remaining-feature/OpenGL-removal goal is active. No viewer, appearance
utility, actual plugin host or native window launched so far; no screenshots.
Owner now authorizes launching the exact fresh viewer after checks for their
manual confirmation. Utility execution and screenshot comparisons are not requested.
Device-only Metal tests and separate CPU/Foundation probes are bounded evidence.
The owner-stopped20FPS campaign remains deferred.

## Current implementation

[Latest finalization/preview/media closure](../work/active/native-finalization-and-media-closure.md)
and [settings/resource tail](../work/active/native-settings-and-resource-tail.md)
extends [raster/float/reflection and fixture admission](../work/active/native-raster-and-legacy-fixture-admission.md),
[Scene Monitor/UI/RenderInfo](../work/active/native-scene-ui-and-render-info.md),
[target/query/font](../work/active/native-target-query-invariants.md) and
[cache/coverage](../work/active/native-pipeline-cache-and-coverage.md).
Original source `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` is authoritative.

Preserve prior RLVa sphere/silhouette/current-camera, viewport/HUD, diagnostic,
float-Screen, reflection-level-zero, native UI/raster/font, resource/lease and
developer-menu repairs. Existing non-rendering features remain in scope.
120GLSL assets, orphan fixtures/backends/programs/metadata and unused buffers
were retired after original invariant/reader mapping; native notices are staged.

Native finalization retains lens preflight, SDR DoF/scopes, one-shot reference/
callbacks and exposure epochs without the displaced GL post traversal. Bloom
works with Scopes closed; native SSR isolation retires history while valid probe
lighting remains presentable. Reference stills always store RGB10_A2 and survive
HDR/SDR changes. Preview first-allocation retry and VLC lifecycle/order are fixed.
Eight fixed targets and the prewarm-only transient pool are retired. Native
Bloom shape uses actual post-source dimensions, including snapshot overrides.

SMAA bfff8e8373/dd796eabdd honors the original stencil setting and output-sized
alternate-image metrics. Root matching GPU mask/world/reload tests pass.
Cutout far-depth pins are original-matched. Alpha 5fa7c0df52/b20f03e40e restores
HDR10/SDR8 post color plus raw F16 coverage filtered at final untransformed UVs,
with original BilinearMirror and one final alpha quantization. The old early-U8
workaround/PSO axis is removed. Root focused/world/reload tests pass.
Legacy cutout retirement3b5a6456e5 removes duplicate seed/copy/VB/program graphs;
real before-alpha/end and pre-read stream boundaries remain. Native pixel duties
are already owned by the validated renderer. Live gDebug remains.

Phase retirement d78cc2d779 removes34 skipped descriptions and three dead input
copies; physical plans/native owners/full-input publication remain. c65e5d588e
removes the unused describes helper. Snapshot4f5f23251b restores the original
forced-allocation-failure admission/recovery boundary without an mFullSize guard.
Preview7f6633a3fe removes the unreachable true branch/flag/far-quad; native preview/
ordinary/profile specialization stays exact. Heroce65f508a4 removes an uncalled
wrapper; live debug owners remain. Water6473c5c77d restores point/aniso normals;
83567e407d restores optional Compute culling gate while waves remain. 2c16d75147
removes only redundant resource Compute copy; real shader/local latch stays live.

## Observed validation

Evidence: `artifacts/metal/feature-completion-20261111/`.
Frozen clean ae2 doctor/configure/full build session2519 and static checks0.
Version26.4.0d68801:346ARM64 Mach-O/zero invalid slices, strict deep signature0,
main viewer and both owned media hosts forbidden direct imports/frameworks0.
Fresh332VLC plugin names/bytes exactly match old335 minus the three exclusions.
Keep screen/VT/CVPX/vmem and complete CEF capabilities. Foreign links/runtime
remain separate; no whole-bundle native-only claim. ZeroGLSL/no old shader tree;
exact native MSL/metallib/notices. Previous verified d2d app preserved by rename.
Receipt: native-pool-cutout-canonical-{build-state,validation}.json.
Viewer SHA2561f5f8373bbd362638c4998e92b036adf752020f9fba9af6a79026cc79f38f920.
Later configure may change metadata/resources before a full rebuild; this is a
source-pinned observation, not a current-artifact guarantee.

At ae2: policy640/19, full alrender564631/120, LLRender335391/442(441passed/
one known CPAL skip), focused5861/8, world105889/23 and reload2304/8 passed.
GPU groups use API/shader validation. native-pool-cutout-registered-validation.json.
SMAA matching build82307 exited0; registered2552/4, world105889/23 and reload
2304/8 passed at dd+test registration. native-smaa-registered-validation.json.
Alpha matching build64115 exited0; root10665/17 focused, world105890/23 and
reload2304/8 passed at b20+registrations. native-cutout-alpha-registered-validation.json.
Counts overlap. Matching legacy/root policy652/20 and phase-resource1894/40 pass;
resource/policy builds57228/57070 exit0. Snapshot actual-body CPU184 checks expose
16 baseline failures then pass; Release textual syntax0 and source peer clear.
Water normal child389/2 exposes17 baseline failures then passes API/shader;
caller gate actual policy352/1 exposes6 baseline flags then passes. Preview three
Release checks, hero two and compute-copy six/CPU1893/41 pass; peers clear.
Matching combined root full build/static and affected checks remain pending.

Production ae2 strict3504files/910info/0blockers/0errors passed. Earlier tests
4094files flags two negative SDL_WINDOW_OPENGL==0 assertions; retain both, no
all-source strict pass or manual window execution. Tools49/four content models
passed at unchanged-tool state. Root harness32files last passed; rerun after docs.
Optional utility was independently rebuilt/static/resource/dependency-validated
at older459→066+explicit WIP hashes; it needs a matching final refresh. Receipt:
/private/tmp/alchemy-native-appearance-resource-validation-20261111/validation-receipt.json.
No actual utility or viewer executable ran.

## Exact next actions

Root owns canonical CMake/docs/integration; no user edits. All three agent source
trees are frozen/released; GPU slot free. New water test registration and Compute
debug comment are root WIP with these docs.
1. Commit source/evidence freeze, then run required doctor/configure/full-app
   build via artifacts/metal/feature-completion-20261111/build-native-settings-stage.py.
   Statically verify exact stage with verify-native-finalize-stage.py and fresh
   native-settings-phase-canonical build-state/prefix. No product run by scripts.
2. Refresh optional utility through refresh-native-appearance-stage.py; separate
   build/cache, strict static/native resources/96character bytes, no utility run.
3. Run matching affected resource/input/native policy, focused water/cutout/SMAA,
   world/water-cull/reload checks, strict owned-source and harness. Record exact
   source/commands/exits/binary hashes; do not use earlier app as current artifact.
4. Once checks pass, launch the exact fresh viewer for the owner to confirm.
   Keep screenshots unrequested; record process/build identity, not parity claims.
   Full feature/stability/performance and ordinary exhausted-frame policy remain open.

## Complete-product advancement assessment

| ID | State | Evidence / next boundary |
| --- | --- | --- |
| M-008 | doing | Original SMAA/cutout/optical duties advance; broader parity remains. |
| M-011 | doing | Owned source gate passes; final cleanup/build/foreign/runtime remain. |
| M-012 | complete | Prior native UI automation tooling retained; no launch this goal. |
| M-013 | doing | Original-mapped RLVa repairs retained; full behavior unverified. |
| M-014 | deferred | Owner-stopped20FPS campaign remains deferred. |

Full original feature parity, prolonged stability, strict no-GL runtime,
independent build/review evidence and measured multi-scene performance remain open.
Preserve earlier lighting, self-turn, camera and voice issues in their notes.
Build/test/source success alone does not establish product completion.
