# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `8a14710e80` (newer source; separate rebuild pending).
**Owner-passed viewer:** `f254feb048`,26.4.0d68817; provisional “pass...for now”.
The remaining-feature/OpenGL-removal goal is active. Owner superseded the earlier
no-launch instruction and requested the ready viewer for manual confirmation.
Fresh26.4.0d68817 launched with open -na; exact new executable PID90460 was
confirmed alive. No login action or screenshots by the agent; utility unexecuted.
Device-only Metal tests and separate CPU/Foundation probes are bounded evidence.
The owner-stopped20FPS campaign remains deferred.

## Current implementation

[Latest finalization/preview/media closure](../work/active/native-finalization-and-media-closure.md)
and [settings/resource tail](../work/active/native-settings-and-resource-tail.md)
plus [post-launch corrections](../work/active/native-post-launch-corrections.md)
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

Post-launch source audit found additional issues; earlier completion assessment
was premature. Integrated9838 fixes inherited feature-mask off clearing all bits;
65b5 restores dirty1 avatar-preview morph refresh;158f retries/current-qualifies
preview vertex/index stores; a380/8a14 fixes RGB8 media source stride/opaque alpha.
Child CPU/GPU/source peers pass; matching combined root build remains pending.

## Observed validation

Evidence: `artifacts/metal/feature-completion-20261111/`.
Frozen clean f254 doctor/configure/full build1447 and static69509 exit0.
Version26.4.0d68817:346ARM64 Mach-O/zero invalid slices, strict deep signature0,
main viewer and both owned media hosts forbidden direct imports/frameworks0.
Fresh332VLC plugin names/bytes exactly match old335 minus the three exclusions.
Keep screen/VT/CVPX/vmem and complete CEF capabilities. Foreign links/runtime
remain separate; no whole-bundle native-only claim. ZeroGLSL/no old shader tree;
exact native MSL/metallib/notices. Previous stages preserved by rename.
Receipt: native-settings-phase-canonical-{build-state,validation}.json.
Viewer SHA256285c561302672cb9fab2fe4ab4ef402297796ca2272b579765d57b3abd4ab1eb.
First full compile31819 exited0 but task staging renamed the configure-generated
Info.plist into backup. Static check caught it; preserve pre-plist-repair evidence.
Corrected rename-before-configure, rebuilt/signed fresh stage and verified it.
No source change or test weakening. Current app matches f254; later rebuilds
must not overwrite it while the owner is testing.

Matching root77976: policy652/20, full alrender563966/115, focused water/cutout/
SMAA11054/19, world105890/23, water-cull250/1 and reload2304/8 all pass. GPU
logs confirm API/GPU validation. Exact argv/hashes: native-settings-phase-
registered-validation.json. Tested binaries unchanged after packaging repair.
Snapshot actual-body184/0fail passes again at clean f254 after16 baseline fails;
real water caller policy352/1 passes after6 baseline flags. Normal389/2 red17→0
and independent source/physical-plan/preview/hero peers remain source-pinned.
Counts overlap; earlier full LLRender335391/442(one CPAL skip) is older evidence.

Production f254 strict3504files/909info/0blockers/0errors passed. Earlier tests
4094files flags two negative SDL_WINDOW_OPENGL==0 assertions; retain both, no
all-source strict pass or manual window execution. Tools49/four content models
passed at unchanged-tool state. Root harness32files last passed; rerun after docs.
Optional utility f254 build/static83133 exits0: ARM64/signature/direct imports,
exact same native MSL/metallib/notices,96character names/bytes, zeroGLSL. Receipt:
native-settings-phase-appearance-validation.json. Utility not executed.
Launch31758 exits0; exact process90460/new verified binary alive after launch.
native-settings-phase-viewer-launch.json. Actual viewer parity not inferred.
Ordinary exhausted-plan audit finds no concrete ownership/freshness defect:
independent exact typed pool recovery clears storage and retains commit/current-
frame publication gates. No !mFullSize guard. Forced pool-failure injection is a
separate unexecuted diagnostic question, not a proved rendering bug.
/private/tmp/alchemy-native-exhausted-plan-audit-20261111.json.
Owner subsequently provisionally passed68817. Exact7,840file/link/byte baseline
copy and strict signature0: manual-pass-baseline-receipt.json. App path untouched.
New fixes: mask80checks red44→0; normal preview20red3→0; realLLVB/publication
combined1860/6 API/GPU pass; media CPU831/2pass after same-source599/2red112.
Tools d053 honor isolated configure -B; all50tools tests/shell syntax pass.
Real new followup directory bootstrap/wrapper configure75939 exit0, existing
dependencies read with installOFF, passed binary hash unchanged. No new app run.

## Exact next actions

Root owns canonical integration/CMake/docs/workflow. Agent source released; no
live GPU/build/check handles. Private inl listing, strict release-source gate and
these docs are WIP. Preserve passed app; all further builds use followup directory.
1. Commit combined freeze, doctor/configure/full build through corrected wrapper
   with AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64-followup. Run matching
   LLRender/preview/upload/policy tests and static app/resources/import/source
   checks. Recheck baseline copy names/bytes/signature; no baseline overwrite.
2. Owner now explicitly requests GitHub push and release action when fixed.
   origin private levdarkheart-lgtm/AlchemyFork; feature branch only/no force.
   Existing Metal alpha job needs tag ref, Project, Metal Alpha4 and fresh exact
   26.4.0.<revision>-alpha4 override. Run generic tag defaults would select wrong job.
3. Owner configured CLI; gh auth status outside sandbox confirms levdarkheart-lgtm
   keyring authentication. No push/dispatch yet. Use authenticated gh
   after validated push to trigger and monitor exact run/artifacts. No credentials,
   default merges, public exposure or permission changes. Keep screenshots of
   viewer unrequested; CI evidence is separate from owner provisional pass.

## Complete-product advancement assessment

| ID | State | Evidence / next boundary |
| --- | --- | --- |
| M-008 | doing | Original SMAA/cutout/optical duties advance; broader parity remains. |
| M-011 | doing | Source/static/build pass; owner confirmation/foreign/runtime remain. |
| M-012 | complete | Prior tooling retained; requested exact-build CLI launch observed. |
| M-013 | doing | Original-mapped RLVa repairs retained; full behavior unverified. |
| M-014 | deferred | Owner-stopped20FPS campaign remains deferred. |

Full original feature parity, prolonged stability, strict no-GL runtime,
independent build/review evidence and measured multi-scene performance remain open.
Preserve earlier lighting, self-turn, camera and voice issues in their notes.
Build/test/source success alone does not establish product completion.
