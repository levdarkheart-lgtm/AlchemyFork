# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `84747519cd` (newer than the verified viewer app).
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

Native finalization now preserves original lens preflight, SDR DoF/scopes,
one-shot reference/callback ownership and exposure duties without traversing
the displaced GL post chain. Bloom admission no longer depends on Scopes.
SSR cutout isolation retires actual native history and permits valid probe-only
lighting while history rebuilds. Reference copies always store RGB10_A2 and
survive HDR/SDR changes, matching original source. Preview allocation retries
and VLC player/media/instance cleanup are repaired. Dead LLRender patch/point
metadata and the original-inert tangent setting/menu graph are removed.
Eight obsolete fixed-target IDs/instances and the unreachable DownRes tail are
retired. Original SSR intent/reset keys and fixed history initialization are
preserved; an explicit replacement for the old target's flush is under review.

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

New registered Release builds exited0. Matching LLRender ABI: full llrender
335386/442(441passed/one known CPAL skip), VLC actual-body CPU1267/11,
Font/UI1862/61, native LLUI73292/28, morph13203/3 and Metal UI36441/11 passed.
At fcd276692e plus four CMake registrations: CPU finalization/lens/reflection
374/16, reference format173/4, finalization2286/2, SSR2172/1 and world105889/23
passed. GPU groups used API/shader validation; filters overlap. Initial sandbox
no-device failure is preserved separately, followed by host-device passes.
Exact argv/source/binary hashes: native-reference-registered-validation.json.

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
Newer source requires a fresh full build; configure may update bundle metadata,
so prior verification is source-pinned.
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

Root owns canonical integration/CMake/docs. Four test/IDE CMake registrations
are the only implementation WIP; their pre-cleanup builds/tests passed.
1. Eight fixed targets are integrated84747519cd, original/event source review0.
   Child actual plan/policy2697/43 and two Release syntax TUs pass. gl_tail_audit
   owns a narrow follow-up preserving clearSceneMap's old implicit flush as an
   explicit semantic boundary; rlva_port supplies independent read-only review.
2. Commit registrations/evidence; rebuild affected policy/resource tests with
   matching headers after that follow-up. Tangent XML cleanup is integrated
   a6f071bf92. No shared-file concurrent edits.
3. Freeze source, preserve the old app stage, then run required doctor/configure/
   full build/static verification. Fresh staging must prove332VLC modules and
   expected346ARM64 files; install cannot prune the old three excluded modules.
4. menu_port's read-only census proves transient prewarm allocations are unused
   by passes; replace their live Bloom count/resolution-failure gate before
   removal. Keep the separate native Metal target pool. No new patch assigned.
5. Record exact results and next source boundary. Keep viewer, utility, actual
   plugin hosts and windows closed; no screenshots or runtime parity claim.

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
