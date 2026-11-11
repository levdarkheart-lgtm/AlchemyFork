# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `d2d0380309` (matches the last verified viewer app).
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
preserved. Explicit native stream flushes retain the old clear boundary for
current SSR and initial/off-to-on reconciliation, without a duplicate target.

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

Root doctor/configure/full build for frozen d2d all exited0 (session63895).
Registered policy629/18 and full alrender565511/137 pass with API/shader
validation where applicable. Almetal test binary is byte-identicale6ee03ec7f
to the prior reference/world GPU receipt; those unchanged tests are reused.
Root tools49 and four content models passed at the prior unchanged-tool state.
Current production strict3508files/910info/0blockers/0errors. Earlier tests4094 has2negative
SDL_WINDOW_OPENGL==0 assertions flagged, not an all-source strict pass. Keep
both native-window assertions; the window case was not executed.

**Last verified canonical viewer sourced2d03803091c17d56977df3c905519894684fbec,
version26.4.0d68796**. Doctor/configure/build/verify0,346Mach-O/zero invalid
ARM64 slices, deep strict codesign0, main-viewer forbidden imports0. Fresh app
has zeroGLSL/no shader subtree and exact MSL/metallib/notices. Both owned media
hosts also have zero forbidden direct imports/libraries. Fresh332VLC plugins
are exact old names/bytes minus the three exclusions. Receipt:
native-finalize-canonical-validation.json; binary SHA256230e3678c0049366b8bef60ffa398ad475f670efe2b22b11f0e2a7d53b4b9de1.
Previous335-plugin app preserved by rename. Future configure can update bundle
metadata before rebuilding; this is a source-pinned static observation.
Optional appearance utility rebuilt from459→066+explicit root WIP hashes;
fresh native stage has no GLSL, exact MSL/metallib/notices+96character files,
ARM64/strict signature/imports0. Foundation-only lookup and extracted actual
Ninja shader/notice rebuild edges passed; final receipt:
/private/tmp/alchemy-native-appearance-resource-validation-20261111/validation-receipt.json.
Neither actual executable ran.

Foreign VLC/CEF links remain separate. Three VLC windowed-output/converter
exclusions integratede75 now also pass actual fresh-app335→332verification.
Keep screen/VT/CVPX/CEF capabilities. No whole-bundle native-only claim.

## Exact next actions

Root owns canonical integration/CMake/docs. Registrations committeddc6f505a0a.
1. Flush follow-up integratedd2d0380309: child actual policy439/13 and combined
   plan/policy2733/44 pass, Release syntax0, independent peer0. Recorder checks
   call/order with warm owner payloads; it does not encode actual GPU geometry.
2. Frozen d2d full build/static verification and affected registered tests pass.
   Preserve native-finalize-canonical-* and native-fixed-resource-registered
   receipts; do not relaunch executables or rerun identical GPU binaries.
3. Review queued uniform5b0b8e136d plus the isolated pool/cutout repairs, then
   integrate a coherent batch and rebuild matching headers/resources.
4. gl_tail_audit owns the isolated transient pool retirement and original Bloom
   shape policy, including actual snapshot source size. menu_port owns only
   three orphan reserved-uniform identities in llshadermgr/tests. rlva_port
   owns a separate native cutout far-depth pin repair. Its legacy seed/copy
   trace is read-only; snapshot matrix reopened for this numeric gap. No shared
   uncoordinated edits; keep the separate native Metal target pool.
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
