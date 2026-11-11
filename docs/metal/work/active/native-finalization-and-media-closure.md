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

## Integrated native finalization

Integrated67a9b960ef retains renderFinalize as the short CPU/viewport/diagnostic
boundary, preserving exposure fixed-EV/reset, callback/rearm/coordinates,
statistics, semantic flush/unbind and original three-pass physics debug call.
The original dense lens-fit solver now runs in native preflight with the final
viewport aspect. SDR DoF and scopes retain actual native backing/device/geometry
guards without the incorrectly ported HDR-only gates. Scope dimensions come
from the planned scaled main view; real asynchronous cadence/readback persists.
The still-called displaced GL luminance/exposure/color/SSR/final-blit traversal,
orphan histogram VB and six aliases are retired after those duties are retained.

Original reference copy happens after AA/CAS/debug, before lens/print/final blit.
A committed offscreen still remains valid if a later presentation validation
fails. An initial hypothesis requiring final_presentation_available was corrected
by that source trace before integration: not an executed viewer bug. Standalone
copy commits before return; failed batched commit cancels pending publication.
One-shot requests are consumed at the actual copy attempt. Same-size committed
old owners survive failed/canceled recapture until a successful replacement;
explicit clear/dimension invalidation remains. Initial CPU baseline71/6 had
five targeted failures; recapture-hold control127/8 had four. Fixed127/8 passed.
Actual GPU checks cover linear SDR DoF, scopes before print, raw scene pixels,
reference cancellation, leases and invalid later print controls, independently
of CPU actual-body facade coverage. Source/peer proofs:
/private/tmp/alchemy-finalization-live-validation-20261111.json and
/private/tmp/alchemy-finalization-peer-review-gl-tail-20261111.json.

## Integrated preview retry

Integratedea12ba4c0a repairs the original-mapped caller failure: Avatar/Sculpted
renderWithoutTarget returns PENDING for a missing texture, while its generator
runs only in the constructor. A recoverable first allocation/backend failure
therefore never retries. Original dynamictexture postRender regenerated an
absent destination before publication after successful render.

Actual caller/generator bodies were byte-proven before adding only the two
retry calls. Real LLGLTexture/LLImageGL/ALMetalLitPreviewRenderer/GPU tests use
named camera/scene/identity facade boundaries; no full floater/avatar/sculpt
construction or viewer invocation is claimed. Baseline1176/4 fails two retry
checks; fixed1726/4 passes. Failed render/adoption preserves old bytes/identity/
stamp/dirty state; successful retry replaces once. A copy encoded before
replacement commits after owner destruction, proving an actual native lease.
Receipt: /private/tmp/alchemy-native-image-preview-validation-20261111/
validation-receipt.json. Registered root full llrender coverage below matches
the later LLRender layout rather than mixing child archives.

## Integrated media ownership

Integrated3120dc7994 repairs inherited original/current leaks: playMedia stops then overwrites
player/media without releasing old owners, repeated init can overwrite instance,
empty destructor and partial shm_remove cleanup leave ownership incomplete.
size_change changes callback-visible dimensions/pixels before stop, contrary to
the intended old-layout stop boundary. Exact original/current source trace
precedes fixes; this is within the goal's discovered-issue repair scope.

releaseMedia now stops/releases/nulls player before media; reset/destruction
also release the instance, idempotently. Resize captures seek before stop and
stops/releases before changing callback-visible dimensions/pixels. Active shared
memory removal stops callbacks before erasing/nulling backing. Empty URI and
failed Darwin setup guards, ten event/frame/data/volume/title bodies and seek/
repeat semantics remain. Actual method bodies execute with named path/env/
libVLC factory/refcount/order doubles and real LLPluginMessage; no real libVLC
or host is linked to the lifecycle test. Final same-test baseline1089/11 fails
71assertions/seven cases; fixed1267/11 passes. Separate real callback playback
above validates the packaging boundary, not this facade's ownership execution.
Receipt: /private/tmp/alchemy-vlc-lifecycle-validation-20261111/validation-receipt.json.

## Bloom, SSR isolation and original reference storage

Integrated5f1baee092 removes Bloom's accidental dependence on sScopeCapture.
Actual native world-presentation eligibility and original minimum three usable
Bloom mips remain; strengths, shader math and higher cutout guards are unchanged.
Actual policy baseline140/9 has three failures; fixed140/9 passes. Receipt:
/private/tmp/alchemy-bloom-admission-validation-20261111.json.

Integrated24f96a956f/0c39d845be maps original ALRenderIsolation.clearSceneMap
to both actual native history owners before their swaps. Cutout base/forward/
impostor paths cannot consume/publish SSR history. Missing/cleared history uses
valid current probe lighting; the prior warmup-withholds-presentation policy was
incorrect and is removed without weakening current-frame/device validation.
Actual policy baseline165/10 has12failures; fixed165/10 passes. Device-only
native base lighting/history tests cover legacy/PBR, HDR/SDR and normal/reverse
Z: red history retires, absent equals real black+far history bit-exactly, old
submitted work survives, and only a new green publication restores reflections.
These low-level GPU checks do not invoke production ALRenderIsolation itself.
Receipts: /private/tmp/alchemy-ssr-isolation-validation-20261111.json and
/private/tmp/alchemy-native-ssr-isolation-validation-20261111/validation-receipt.json.

Integrated3948f43537/6ce8831a2a/fcd276692e restores original RGB10_A2 still
storage for HDR and SDR and dimension-only invalidation across display changes.
HDR raw copies remain exact; SDR converts through the existing point-clamp
resample shader into RGB10_A2. All256component values/packed alpha, format/
shape/usage controls and eight presentation-format/layout combinations pass.
A real MTLSharedEvent gates commands until source/reference/renderer destruction,
then exact output proves in-flight storage/pipeline retention. Baseline125/4
fails11assertions; fixed173/4 passes. Actual policy baseline184/11 fails7;
fixed184/11 passes using valid RGBA8 display and F16 sharp-source descriptors.
Receipts: /private/tmp/alchemy-native-reference-format-validation-20261111/
validation-receipt.json and /private/tmp/alchemy-reference-format-policy-validation-20261111.json.

## Registered integration and dead metadata

Root registered builds exited0: caller-preview-lifecycle, native-finalize,
native-bloom-ssr and native-reference-format registered-build.log files in the
artifact directory. At fcd plus four CMake registrations, actual CPU
finalization/lens/reflection374/16, format173/4, finalization2286/2, SSR2172/1
and world105889/23 pass. The world filter includes the actual final print-chain
case with updated original ten-bit expectations. GPU groups use API/shader
validation. Initial sandbox no-device failure is preserved separately; host
device rerun passes. Exact source/WIP/argv/binary hashes/logs:
native-reference-registered-validation.json. Counts overlap; do not sum.

Integrated1d51dd50f9 removes only dead LLRender patch/point metadata, its single
terrain setter/pass override and three metadata assertions. Original setters
called glPatchParameteri/glPointSize; actual native terrain/shadow/water consume
explicit drawPatches(4). Pipeline loadMatrix always flushes before terrain's
deferred pass, and inherited pass bodies are empty. Font point-size state stays.
Three actual Release textual syntax TUs and exact live-body/reader proofs pass:
/private/tmp/alchemy-native-final-render-state-validation-20261111/preservation.json.
Matching root ABI full llrender335386/442(441passed/one known CPAL skip),
VLC1267/11, Font/UI1862/61, native LLUI73292/28, morph13203/3 and affected
Metal UI36441/11 pass. Receipts: caller-preview-lifecycle-registered-validation.json
and native-finalize-registered-validation.json. No hidden-window case runs.

Integrateda6f071bf92 removes only original-inert ShowTangentBasis setting,
English menu item, ten locale overlays and Portuguese comment:13XML/33deletions.
Original source has only storage/CheckControl/ToggleControl readers, no render
consumer. All retained XML projections are exact;12menu files parse and1498
retained settings remain unique. Other four enum/API/menu graphs remain intact.
Independent menu peer found no blockers. Receipt:
/private/tmp/alchemy-inert-developer-menu-validation-20261111.json.

## Remaining native tail

Bounded original/current live gate audit surveyed3505production C/C++/ObjC/header
files and manually traced completeness, shader levels, texture identity, debug
manipulators, RLVa overlays, impostors and snapshot admission. It found no further
proven suppressed native feature in that scope; this is not whole-feature parity.
Receipt: /private/tmp/alchemy-live-feature-gate-audit-20261111/
live-feature-gate-audit.json. That audit led to the concrete preview retry defect.

Integrated84747519cd retires exactly eight fixed targets:
SSRScene, DownRes, ExposureMeter/Histogram/Exposure/Last and LensFlareA/B.
Logical SSR intent is independent of physical descriptor changes. Actual
reconciliation preserves applied size/depth/HDR/enable/retry invalidation;
fixed exposure/flare reset only on initialization/recreation, with separate
explicit HDR reset unchanged. Original actual allocation-event fragment400/12
and replacement403/12 have216identical check cells across24events. Child combined
plan/policy2697/43, two Release syntax TUs, strict source3508/910info/0blocking/
0errors and independent source-hash-pinned peer pass. Native pressure/create
prefix and13live methods are byte-identical. Receipt:
/private/tmp/alchemy-native-fixed-resource-validation-20261111/validation-receipt.json.
Postfreeze review caught the removed target block's implicit gGL.flush; gl_tail
owns a narrow explicit semantic-flush follow-up, with rlva_port read-only peer.
No exposed pending-geometry runtime bug is claimed. Root build awaits that fix.

Independent transient census:
/private/tmp/alchemy-native-transient-pool-audit-20261111.json.
prewarmTransients is the only product ALTransientTargets scope; acquireResource
has no caller. Prewarming allocates real Metal textures that no pass consumes.
However its success bits still drive native Bloom mip count, and unused post
target failures can force resolution reduction. Replace those live policies
before deleting the graph; preserve the separate live ALMetalRenderTargetPool.
No transient removal is integrated or assigned yet.
Root owns CMake/docs/integration and required fresh full-app verification.
Current tested native registrations are four CMake WIP files; no user edits.
Before full build, preserve the old app by rename so exact VLC exclusions are
observed rather than masked by stale installed modules. Viewer/utility/hosts/
windows remain closed throughout.

Full original parity, stability, runtime native-only, independent release evidence
and measured multi-scene performance remain open. Earlier lighting/self-turn/
camera/voice notes and owner-stopped20FPS campaign remain unchanged. Keep goal
active; bounded build/test/source success is not product completion.
