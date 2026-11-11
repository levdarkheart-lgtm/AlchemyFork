# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `066d9d4e53` (newer than the verified viewer app).
The remaining-feature/OpenGL-removal goal remains active. No viewer, appearance
utility or native window launch; no screenshots, menu invocation or user-cache
operation. Device-only tests submit actual Metal commands. A separate Foundation
resource lookup probe ran without the viewer. The owner-stopped20FPS campaign
remains deferred.

## Current implementation

[Latest raster/float/reflection and fixture admission](../work/active/native-raster-and-legacy-fixture-admission.md)
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
`indra/almetal/THIRD_PARTY_NOTICES.txt`; root WIP stages them with native assets.

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

Root doctor initially hit sandbox sysctl denial; host-capability retry exited0:
`raster-native-doctor-host.log`. Required full newer viewer build/verification
is pending; no performance or release-readiness claim.

**Last verified canonical viewer source d9f624ea464d3ba3c305d7656bf9533f6e65a8dd,
version26.4.0d68755**. Doctor/configure/build0,349Mach-O/zero invalid ARM64
slices, deep strict codesign0, main-viewer forbidden imports0. Exact source/
binary hashes: `viewport-canonical-source.json` /
`viewport-canonical-identities.json`. Newer code is not represented by that app.
Optional appearance utility rebuilt from459→066+explicit root WIP hashes;
fresh native stage has no GLSL, exact MSL/metallib/notices+96character files,
ARM64/strict signature/imports0. Foundation-only lookup and extracted actual
Ninja shader/notice rebuild edges passed; final receipt:
/private/tmp/alchemy-native-appearance-resource-validation-20261111/validation-receipt.json.
Neither actual executable ran.

Foreign VLC/CEF OpenGL links remain separately audited. Three VLC windowed
output/converter exclusions are candidates, not applied. Keep actual screen
input, VT decoder and required CEF capability payloads. Runtime loading/backend
selection remains unobserved; no whole-bundle no-GL claim.

## Exact next actions

Root owns canonical integration/CMake/docs. Current8WIP paths are ours:
almetal/alrender/appearance/LLUI/newview registrations, builtin notice hook,
viewer and utility native-asset staging. No user changes discarded.
1. Root run56044 exited0; companion CPU117/2+GPU1964/3 gates passed.
   Reflection harness97d0b8a20e, pipeline dead leaves49fa6b4401 and GLSL
   assets066d9d4e53 are integrated. Stale alrender target/GLSL IDE/install
   boundaries and AL_TEST_SHADER_DIR are removed in root WIP.
2. Update this snapshot with results and commit coherent registration/staging
   work. Preserve exact source/WIP/binary hashes in the receipts.
3. Review live finalization closure; preserve actual native owners before
   retiring still-called legacy traversal and persistent resource storage.
4. Verify a fresh viewer native stage, since install does not prune old files.
   Exact120asset/notice hashes and reader gates: glsl-root-retirement-gates.json.
5. Required full app build/static verify/sign/import checks after frozen source.
   Keep viewer/utility/windows closed.

Exclusive isolated owners: rlva_port pipeline.cpp/h, new
pipeline_finalize_config.inl and CPU/GPU finalization tests: preserve native
reference one-shot/drop/resize, lens-fit preflight, callbacks, scope/statistics,
exposure epochs and SSR eligibility before retiring the live legacy traversal.
Original-mapped nonHDR DoF/scope gates are under investigation.
menu_port obsolete GL test-mode CMake/CI/docs/sanitizer plumbing only; root
retains actual target registrations/shared renderer files. gl_tail_audit owns
isolated llviewershadermgr.cpp/h eleven now-unused bloom/DoF metadata graphs;
preserve native reload/settings/aggregate semantics. Utility validation is done.

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
