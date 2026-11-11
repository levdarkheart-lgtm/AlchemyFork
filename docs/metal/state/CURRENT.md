# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `e75e69794b` (newer than the verified viewer app).
The remaining-feature/OpenGL-removal goal remains active. No viewer, appearance
utility or native window launch; no screenshots, menu invocation or user-cache
operation. Device-only tests submit actual Metal commands. A separate Foundation
resource lookup probe ran without the viewer. The owner-stopped20FPS campaign
remains deferred.

## Current implementation

[Latest finalization/preview/media closure](../work/active/native-finalization-and-media-closure.md)
extends [raster/float/reflection and fixture admission](../work/active/native-raster-and-legacy-fixture-admission.md), which
extends [Scene Monitor/UI/RenderInfo](../work/active/native-scene-ui-and-render-info.md),
[target/query/font](../work/active/native-target-query-invariants.md) and
[cache/coverage](../work/active/native-pipeline-cache-and-coverage.md).
Original source `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` remains the behavioral
reference. Preserve all prior RLVa sphere/silhouette/current-camera, viewport,
HUD bracket, Scene Monitor, resource, diagnostic and developer-menu repairs.

Native immediate UI now honors original polygon-offset signs/flush ordering.
Retained color or depth backing defines the real target extent independently of
a bounded viewport; a later no-backing draw resets that inference. Indexed HUD
offset sign and the old wrong-sign BatchSize test expectation are corrected.
No generic scissored LLRenderTarget clear was implemented or claimed: no live
product consumer remained after actual HUD bracket migration.

Original Screen remains RGBA16F with HDR off as well as on. Native21 scene
pipeline families and their consumers now preserve that range. Explicit HDR
intent keeps SSR/history, hero/ordinary probe, refraction and display format
boundaries intact. Flare and flare-lit dirt work in nonHDR without changing the
original luminance gate. Three unused alternative lighting/composition APIs,
their private resources/PSOs/MSL, and13 unused shader-program metadata graphs
were removed after executable-reader audits.

Reflection level zero now projects only the shading count to the default probe.
The full warmed pack/debug volumes/allocation/exposure remain intact; SSR then
hero still finish shading. Actual manager bodies and native base/water-helper
consumers have separate CPU/GPU controls, not fabricated viewer invocation.

Remaining legacy resource/FXAA/morph and28LLUI cases have native counterparts.
Orphan ALOnDemandTarget, displaced GL fixtures and timing-only benchmarks were
removed only after original invariant mapping. Native manual hidden-window
fixture remains compile-only and is not executed. Registered companion gates
passed; old reflection harness and120GLSL assets are now retired.
Complete shader/embedded-algorithm notices are tracked in
`indra/almetal/THIRD_PARTY_NOTICES.txt`; committed native staging preserves them.

## Observed validation

Evidence directory: `artifacts/metal/feature-completion-20261111/`.
Root registered resource+FXAA9140assertions/5cases, morph13203/3,
native LLUI73292/28 and CPU LLUI4821716/1516 passed. Matching raster ABI:
llrender333661/438(437passed/one known CPAL font skip), Font/UI1862/61,
native LLUI73292/28 and morph13203/3 passed with Metal API/shader validation
where applicable. Corrected affected Metal UI/selection/retained-background/
RenderInfo36441/11 passed. Initial old wrong-sign expectation1/36440failure
is retained and superseded, never waived.

Root float-policy and reflection-companion Release builds both exited0.
Registered original-method CPU lens73/3 +reflection117/2 passed together190/5.
Matching-header root float scene105890/23, independent conversion3462/3,
reflection1964/3 and affected UI36441/11 passed API/shader validation.
Exact argv/source/WIP/binary hashes and logs:
`float-reflection-registered-validation.json`.

Root doctor/configure/full build for frozen0fe all exited0 (session73684).
Old app stage with273GLSL files preserved by rename before configure recreated
the bundle. Never count that preserved stage as current packaging evidence.
Root tools49 and all four lens/dirt/exposure/SH models passed. Production strict
source3504files/0blockers/0errors; including tests4094files has2negative
SDL_WINDOW_OPENGL==0 assertions flagged, not an all-source strict pass. Keep
both native-window assertions; the window case was not executed.

**Last verified canonical viewer source0fe112ee902172c4983e5d277cfba08ded41751b,
version26.4.0d68778**. Doctor/configure/build/verify0,349Mach-O/zero invalid
ARM64 slices, deep strict codesign0, main-viewer forbidden imports0. Fresh app
has zeroGLSL/no shader subtree and exact MSL/metallib/notices. Receipt:
native-fixture-canonical-validation.json; binary SHA2561cee92cdc76050b8daae4e2c2799117a2d23240460488f382c9a958ad9b4ebcf.
Newer915 shader-metadata/test-mode cleanup requires affected rebuild; current
configure may update bundle metadata, so prior verification is source-pinned.
Optional appearance utility rebuilt from459→066+explicit root WIP hashes;
fresh native stage has no GLSL, exact MSL/metallib/notices+96character files,
ARM64/strict signature/imports0. Foundation-only lookup and extracted actual
Ninja shader/notice rebuild edges passed; final receipt:
/private/tmp/alchemy-native-appearance-resource-validation-20261111/validation-receipt.json.
Neither actual executable ran.

Foreign VLC/CEF links remain separate. Exact three VLC windowed-output/converter
exclusions integratede75 after335→332module CPU callback/frame/provenance and
fresh install controls. New actual app fresh-stage verification remains pending.
Keep screen/VT/CVPX/CEF capabilities. No whole-bundle native-only claim.

## Exact next actions

Root owns canonical integration/CMake/docs. Native registrations, notice hook,
viewer/utility staging and GLSL packaging cleanup committed0fe112ee90.
Reviewed test-mode5dc6dd54f3 and eleven dead shader metadata graphs9150c33dfc
are integrated after0fe static verification. No user changes discarded.
1. Root run56044 exited0; companion CPU117/2+GPU1964/3 gates passed.
   Reflection harness97d0b8a20e, pipeline dead leaves49fa6b4401 and GLSL
   assets066d9d4e53 are integrated. Stale alrender target/GLSL IDE/install
   boundaries and AL_TEST_SHADER_DIR are removed and committed.
2. Configure17387 exited0; all607actual CTest commands/properties are exact
   before/after obsolete mode cleanup. Full native alrender565814/137passed
   API/shader validation. Grouped full build/verification follows live fixes.
3. Review live finalization closure; preserve actual native owners before
   retiring still-called legacy traversal and persistent resource storage.
4. Recreate actual app stage for VLC exclusion because install cannot prune
   stale modules; verify332remaining exact modules and346ARM64 files expected.
   Exact receipt/source maps and pending caller fixes are in the latest note.
5. Record actual results, then review isolated commits and rebuild affected
   targets after integration. Keep viewer/utility/windows closed.

Exclusive isolated owners: rlva_port pipeline.cpp/h, new
pipeline_finalize_config.inl and CPU/GPU finalization tests: preserve native
reference one-shot/drop/resize, lens-fit preflight, callbacks, scope/statistics,
exposure epochs and SSR eligibility before retiring the live legacy traversal.
Original-mapped nonHDR DoF/scope gates are under investigation.
menu_port owns isolated media_plugin_libvlc.cpp +lifecycle.inl/CPU tests for
inherited overwrite/cleanup leaks and stop-before-layout ordering. Preserve guards,
events/seek/repeat/volume; no host/window. gl_tail_audit owns image preview caller
retry after failed initial texture allocation, actual-body inls and new native
GPU test. Root registers tests; LLRender ABI frozen until child GPU proof.

## Complete-product advancement assessment

| ID | State | Evidence / next boundary |
| --- | --- | --- |
| M-008 | doing | Native scene range/raster/reflection correctness advances; finalization and broader parity remain. |
| M-011 | doing | Native fixtures replace dead GL owners; final assets/build/runtime gates remain. |
| M-012 | complete | Prior native UI automation evidence retained; no launch during this goal. |
| M-013 | doing | Original-mapped RLVa repairs retained; full behavior remains unverified. |
| M-014 | deferred | Owner-stopped20FPS campaign remains deferred. |

Full original feature parity, prolonged stability, strict no-GL source/runtime,
independent build/review evidence and measured multi-scene performance remain open.
Preserve earlier lighting, self-turn, camera and voice issues in their notes.
Build/test/source success alone does not establish product completion.
