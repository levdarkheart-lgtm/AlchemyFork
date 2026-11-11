# Native finalization, preview and media closure

Observed2026-11-11 on Apple M1 Max64GiB/macOS26.5.2/Xcode26.6.
Original source: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
Parent record: [raster/float/legacy fixture admission](native-raster-and-legacy-fixture-admission.md).
No viewer, actual plugin host or SDL/native window was launched; no screenshots.
Standalone CPU libVLC callback probes, Foundation resource probes and device-only
Metal tests are distinct evidence, never live viewer/menu parity.

## Full application checkpoint

Frozen clean source `0fe112ee902172c4983e5d277cfba08ded41751b` built through
the required tools/metal/build.sh wrapper with AL_METAL_PRESET=ninja-os-arm64,
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64, AL_METAL_JOBS=8. Doctor,
configure and1302-action full build all exited0. Old app with273staleGLSL files
was preserved by rename before configure; source bundle was recreated fresh.
Artifacts: `artifacts/metal/feature-completion-20261111/`,
native-fixture-canonical-{doctor,configure,build}.log and
native-fixture-fresh-stage.json. Build handle73684 completed0.

Release26.4.0d68778 then passed tools/metal/verify_app.sh:349Mach-O files,
zero invalid/non-ARM64 slices. Deep strict codesign0; nm/otool main-viewer
forbidden GL/CGL/SDL_GL/Vulkan symbols/framework imports0. Fresh bundle has
zeroGLSL files and no app_settings/shaders subtree. Packaged MSL/metallib and
preserved notices exactly match the built sources/artifact:
- Viewer SHA2561cee92cdc76050b8daae4e2c2799117a2d23240460488f382c9a958ad9b4ebcf.
- MSLca72796a30cd59d46a4a4d68fc4d2d171f8c6f7987a765df6c3e4c55b947508e.
- Metallib4b8dfe20e45db28e7711a16861df89162e872f08118eb64ba2eb689de4f11870.
- Notice17bacda8956823fc0f740ff8bffa26b6215363eb69f2b50f5dff7f979bfeea75.

Exact commands/version/source/status/resource hashes:
native-fixture-canonical-validation.json. This is a source-pinned observed
check; later configure can update bundle metadata before a subsequent build.
Newer source below is not represented by that full build. Foreign VLC/CEF
links remain separate; main-viewer import zero is not whole-bundle no-GL.

Tools49unit tests passed, including intentional malformed-input diagnostics.
Both default lens/dirt scripts and exposure/SH models exited0:
native-fixture-tools-tests.log /native-fixture-content-validation.json.
Production strict source:3504files/0blockers/0errors/1062informational.
Including tests:4094files/2blockers/0errors; both are explicit
SDL_WINDOW_OPENGL==0 assertions in a window case. Preserve those assertions.
No all-source strict pass or window execution is claimed.

## Obsolete test mode and shader metadata

Integrated `5dc6dd54f3` (childdc7c9c6adb) removes only obsolete GL mode
branches/option/report/CI/env/docs and GL-driver-only sanitizer suppressions.
Remaining CPU/native suppression rules/order are byte-identical; no general
CI platform rewrite. Independent gl_tail review found no blockers.

Child actual CMake4.4/Catch2 contract:16file/9per-case entries, commands,
properties/targets identical before/after,12CPUtests pass and two explicitly
Python-disabled. Real parent configure17387 exited0; full parent CTest snapshot
then compared all607entries/commands/properties exactly, excluding only source
backtrace line metadata. No test/window executes for this inspection.
Receipt: native-fixture-ctest-preservation.json, paired before/after CTest JSON.
Full registered alrender suite after removal:565814assertions/137cases pass
Metal API/shader validation, no window. native-fixture-alrender-full-validation.json
pins binary/source/argv/logs. Child proof:
`/private/tmp/alchemy-native-test-mode-validation-20261111/`.

Integrated `9150c33dfc` (child3c7d71802c) removes11now-disconnected Bloom5/
DoF6 shader globals/load/unload graphs, newly orphaned local and cross-tap constant.
Native renderBloom/renderDepthOfField, live settings/reload/aggregate behavior,
CROSS_FILTER_MAX_STRENGTH and LENS_DIRT_MAX_LINES remain. Exact projection,
eight control bodies and352surviving-program statement lines across30identities
are unchanged; actual Release textual syntax0, independent root review.
Proof: /private/tmp/alchemy-native-bloom-dof-metadata-validation-20261111/
preservation.json. No new whole-viewer build claim.

## Exact VLC packaging exclusion

Integrated `e75e69794b` (child514566920f): exactly three Darwin-only
install PATTERN EXCLUDE entries for libvout_macosx_plugin.dylib,
libcaopengllayer_plugin.dylib and libglconv_cvpx_plugin.dylib.

Original/current media plugin use the same no-xlib+transform-vflip arguments,
video callbacks and RV32 format. Shipped ARM64 callback disassembly sets
avcodec-hw=none,vout=vmem,window=none. These three modules are windowed outputs/
GL converter, not required by that output contract. Keep screen input,
VideoToolbox, CPU CVPX/vmem, all other modules and the complete CEF payload.

Actual standalone libVLC probe used complete335 vs filtered332 modules copied
from the preserved signed pre0fe stage. Prior339dylib inventory included four
core-library files/aliases; it was not339distinct plugins. Nine CPU callback
runs covered32x24, same-input resize64x48, fresh-instance48x16.18,432output bytes
compare exactly complete/filtered. Independent no-transform output is exact
row reversal at all three sizes; colored anchors, nonidentical flip control,
guard bytes, balanced lock/unlock and no callbacks after stop/release passed.
Actual logs select vmem/window-none; no test vout/window/hardware override.
dyld loaded333 vs330module images, exact difference the three excluded modules,
and no plugin was loaded outside the owned temporary stage. Test-only
ignore-config/config-path/no-cache/log flags isolate state; no user settings
or cache was changed. All source/module hashes remain unchanged.

The exact extracted production CMake install command produced332remaining
names/hashes in a fresh owned destination, preserving screen/VT/CVPX/vmem.
Root read the helper/receipt and reviewed the five-line production diff;
independent gl_tail review found no blockers.
Receipt: /private/tmp/alchemy-vlc-callback-validation-20261111/validation-receipt.json.
Existing GUI Growl/Sparkle module load warnings occur in both complete/filtered
baseline and are recorded; no unrelated module deletion was made.
EXCLUDE cannot prune a stale bundle: required grouped full build must recreate
the actual app stage and verify all three absent. No complete host/media/
stream/audio/HW/URI/screen/CEF-runtime or whole-bundle no-GL claim.

## Live finalization — implementation in progress

rlva_port exclusively owns isolated pipeline.cpp/h, pipeline_finalize_config.inl,
newview/tests/alfinalize_policy_test.cpp and a new almetal GPU admission file.
Actual method-body CPU baseline71assertions/6cases fails5targeted assertions:
reference one-shot consumption2, changed lens-fit preflight1, SDR DoF1, SDR scope1.
Callback/rearm/coordinate/exposure/guard controls already pass. Final commit,
GPU controls and registered root validation remain pending.

Retain renderFinalize as the short CPU/viewport/diagnostic boundary. Move the
actual lens-fit solver to native preflight, reference request consumption to the
actual copy attempt, and valid still availability to the committed native owner.
Preserve exposure fixed-EV/reset tail, callbacks/scopes/statistics, noPost/cutout/
hold behavior, generation epochs, SSR eligibility and actual physics debug pass
ownership before removing the still-called legacy rendering graph/storage.

Original reference copy happens after AA/CAS/debug, before lens/print/final blit.
A committed offscreen still remains valid if a later presentation validation
fails. An initial hypothesis requiring final_presentation_available was corrected
by that source trace before integration: not an executed viewer bug. Standalone
copy commits before return; failed batched commit cancels pending publication.
One-shot-at-attempt semantics remain a separate real defect.

## Preview retry — implementation in progress

gl_tail_audit found original-mapped caller failure: native Avatar/Sculpted
renderWithoutTarget returns PENDING for a missing texture, while its generator
runs only in the constructor. A recoverable first allocation/backend failure
therefore never retries. Original dynamictexture postRender regenerated an
absent destination before publication after successful render.

Approved exclusive llfloaterimagepreview.cpp/h, lldynamictexture.cpp generator
only if necessary, new image_preview_native.inl and dynamic_texture_native.inl,
and llrender/tests/alimage_preview_native_test.cpp. Existing caller/generator
bodies are extracted byte-identically before edits, then real LLGLTexture/
LLImageGL/ALMetalLitPreviewRenderer/GPU tests exercise retry/publication/
retention with named camera/scene/identity facade boundaries. No allocation or
adoption algorithm is copied/mocked. Root owns registration; native preview
renderer implementation is outside current scope without another proven defect.
LLRender ABI remains frozen while child uses matching canonical archives.

## Media ownership — implementation in progress

menu_port found inherited original/current leaks: playMedia stops then overwrites
player/media without releasing old owners, repeated init can overwrite instance,
empty destructor and partial shm_remove cleanup leave ownership incomplete.
size_change changes callback-visible dimensions/pixels before stop, contrary to
the intended old-layout stop boundary. Exact original/current source trace
precedes fixes; this is within the goal's discovered-issue repair scope.

Approved exclusive media_plugin_libvlc.cpp, new narrow lifecycle.inl and
tests/alvlc_lifecycle_test.cpp, with root CMake ownership. Actual init/reset/
play/destructor/size-change/cleanup bodies are first byte-proven then executed
under explicitly named path/environment/libVLC factory/refcount/order doubles,
using real LLPluginMessage values. Preserve empty-URI guards, event/seek/repeat/
volume/status semantics and stop before changing callback-visible layout.
Real frame/data callbacks stay intact. Production ownership validation is
separate from the real standalone libVLC playback above. Final source/red→green
receipt/registered tests pending; no actual plugin host execution.

## Remaining native tail

Bounded original/current live gate audit surveyed3505production C/C++/ObjC/header
files and manually traced completeness, shader levels, texture identity, debug
manipulators, RLVa overlays, impostors and snapshot admission. It found no further
proven suppressed native feature in that scope; this is not whole-feature parity.
Receipt: /private/tmp/alchemy-live-feature-gate-audit-20261111/
live-feature-gate-audit.json. That audit led to the concrete preview retry defect.

Root has inspected unused LLRender patch/point metadata but has not edited it.
setPointSize/getPointSize have only one test consumer; preserve distinct font
point-size fields. mPatchVertices has no native consumer, but its setter has a
live legacy terrain call plus a test. Original setter drove GL_PATCH_VERTICES;
actual native terrain/shadows and water drawPatches(4) consume explicit constants.
Before retiring that setter/call, preserve the pre-pool flush guarantee:
pipeline calls loadMatrix before beginDeferredPass and loadMatrix always flushes.
Existing four hardware tessellation cases cover actual native control points,
winding, subdivision/edge coverage and generated triangle barycentrics. This
cleanup follows the preview matching-ABI freeze and requires affected rebuilds.

Full original parity, stability, runtime native-only, independent release evidence
and measured multi-scene performance remain open. Earlier lighting/self-turn/
camera/voice notes and owner-stopped20FPS campaign remain unchanged. Keep goal
active; bounded build/test/source success is not product completion.
