# Native finalization, preview and media closure

Observed2026-11-11 on Apple M1 Max64GiB/macOS26.5.2/Xcode26.6.
Original source: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
Parent record: [raster/float/legacy fixture admission](native-raster-and-legacy-fixture-admission.md).
No viewer, actual plugin host or SDL/native window was launched; no screenshots.
Standalone CPU libVLC callback probes, Foundation resource probes and device-only
Metal tests are distinct evidence, never live viewer/menu parity.

## Current full application checkpoint

Frozen clean source `ae2c601e0b805fe167bdb80dcef4cec7af226bd9` passes required
doctor/configure/full build (session2519), fresh-stage verify_app, strict deep
signature and main/owned-media-host direct import/framework checks, all0.
Release26.4.0d68801 has346ARM64 Mach-O files,332exact VLC module names/bytes
from prior335 minus the three exclusions, zeroGLSL/no old shader subtree and
exact native MSL/metallib/notices. Verified d2d app preserved by rename first.
Viewer SHA2561f5f8373bbd362638c4998e92b036adf752020f9fba9af6a79026cc79f38f920;
MSLe0fdbfd7d818feafba7af9b1cbe65129e6c35ec08a5d8149a95aad995a0b35a2;
metallibb3bd96e1332c4361d4c12f26d3d4314532bbc7e0f0395658b2e60aa204140335.
Notice hash remains17bacda8956823fc0f740ff8bffa26b6215363eb69f2b50f5dff7f979bfeea75.
Exact source/status/argv/exits/resources: native-pool-cutout-canonical-
{build-state,validation}.json and corresponding logs.

Matching policy640/19, full alrender564631/120, full LLRender335391/442
(441passed/one known CPAL skip), focused5861/8, world105889/23 and reload2304/8
pass. GPU groups use API/shader validation. Receipt:
native-pool-cutout-registered-validation.json. Production strict3504/910info/
0blockers/0errors: native-pool-cutout-strict-source.json. Counts overlap.
Source-pinned observation only: later configure can change bundle metadata/
resources before a full rebuild. No product executable ran.

## Prior d2d full application checkpoint

Frozen clean source `d2d03803091c17d56977df3c905519894684fbec` passed required
doctor/configure/full build through the same wrapper/env below (session63895).
The previous335-plugin app was preserved by rename before configure recreated
the stage. Source stayed frozen; only metal documentation changed during build.
Release26.4.0d68796 passes verify_app:346Mach-O/zero invalid ARM64 slices,
deep strict codesign0, main viewer and both owned media host direct forbidden
GL/CGL/SDL_GL/Vulkan imports/libraries0. Foreign CEF/screen dependencies remain
separately audited; this is not a whole-bundle/runtime no-GL claim.

All332remaining VLC plugin names and complete bytes exactly match prior335
minus the three exclusions. Screen/VT/CVPX/vmem modules remain. Fresh bundle
has zeroGLSL/no shader subtree and exact native MSL/metallib/notices, with the
same three resource hashes as the earlier checkpoint below. Viewer SHA256:
230e3678c0049366b8bef60ffa398ad475f670efe2b22b11f0e2a7d53b4b9de1.
Evidence: native-finalize-canonical-build-state.json and
native-finalize-canonical-validation.json with exact argv/exits/source/status/
hashes and native-finalize-canonical-* logs. Product executables never ran.

Matching registered actual-body policy/lens/reflection629/18 and full
alrender565511/137 pass, API/shader validation for the resource suite.
native-fixed-resource-registered-validation.json records exact argv/hashes.
Current almetal binary SHAe6ee03ec7f8a19583e53757cc933d6cb704bd58ff54b7e5384019306bda71bab
is byte-identical to the prior reference/world GPU runs; those exact unchanged
GPU results are reused rather than repeated. Current production strict source:
3508files/910info/0blockers/0errors (native-finalize-strict-source.json).

## Prior full application checkpoint

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
EXCLUDE cannot prune a stale bundle: the current d2d full build recreated the
actual app stage and proved exact335→332 names/bytes. No complete host/media/
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
Postfreeze review caught the removed target block's implicit gGL.flush.
Integratedd2d0380309 restores one effective semantic drain after native pair
retirement for current SSR or newly enabled next-plan SSR, before cutout/plan
publication. No duplicate attachment, mask/depth/viewport mutation or GPU wait.
Exact same recorder test with immutable baseline body433/13 has17failures;
fixed439/13 and combined plan/policy2733/44 pass. Warm nonnull R11/R32 payloads
make retirement-order checks meaningful. These are actual-body CPU call/order
observations, not actual Scope/GPU execution or an exposed runtime bug claim.
Release pipeline syntax0; independent peer hashes/source0. Receipts:
/private/tmp/alchemy-native-fixed-resource-validation-20261111/flush-boundary-followup.json
and /private/tmp/alchemy-fixed-resource-flush-peer-review-rlva-20261111.json.

Independent transient census:
/private/tmp/alchemy-native-transient-pool-audit-20261111.json.
prewarmTransients is the only product ALTransientTargets scope; acquireResource
has no caller. Prewarming allocates real Metal textures that no pass consumes.
However its success bits still drive native Bloom mip count, and unused post
target failures can force resolution reduction. Replace those live policies
before deleting the graph; preserve the separate live ALMetalRenderTargetPool.
gl_tail_audit owns an isolated removal patch based on d2d; shared pure Bloom
shape policy now uses actual post-source geometry, including native snapshot
overrides that deliberately bypass retained resource reconciliation. It removes
the cached count/prewarm-only pool while retaining actual native allocation
failure and the unrelated spot-shadow invariant. Source/CPU/peer proof is in
progress; no patch is integrated. menu_port's three orphan uniform identities/
table entries are frozen5b0b8e136d with actual-manager1011checks, exact503-entry
projection, two Release syntax TUs and independent peer0. Matching-header root
rebuild remains required after integration.

The cutout trace maps actual native seed/blend/display/presentation/RGBA readback
ownership. Old pipeline seed/copy traversals produce no native pixel work but
retain real CPU stream/state side effects; do not delete those blindly.
Source review also found a numeric gap: original forward far quad uses
(0.99999f+1.f)*0.5f window depth; reverse explicitly uses0.000005f. Native shared
coverage vertex uses exact1/0. Existing tests cover far/mid depths and miss the
epsilon band. Actual unchanged-source GPU baseline1230/1 fails exactly12pin/
band checks across HDR/SDR and both depth conventions;1218controls pass.
Baseline: /private/tmp/alchemy-native-cutout-depth-pin-validation-20261111/baseline-gpu.log.
rlva_port owns the isolated cutout-specific repair, preserving shared exact-far
behavior for other users. Snapshot matrix
row is reopened; the stale claim of a retained GL fallback was corrected.
Root owns CMake/docs/integration and required fresh full-app verification.
Tested native registrations are committeddc6f505a0a; no user edits.
Clean frozen sourced2d0380309 doctor/configure/full build63895 and static
verification pass. Previous app with335plugins is preserved as
Alchemy Metal Development.pre-native-finalize-d2d03803-20261111.app before
configure. Exact source/status/argv/progress: native-finalize-canonical-build-state.json.
Current app verification and affected registered tests are recorded above.
Viewer/utility/hosts/windows remain closed throughout.

## Latest source integration — matching rebuild pending

Canonical f65325c1af/18a35acb8d/53825c5035 integrate the frozen uniform, pool
and cutout commits above. Root CMake removes exactly four retired pool source/
header entries and the pool-only test entry, retaining the spot-shadow case;
almetal registers almetal_cutout_coverage_test.mm with its own PCH exclusion.
The new full matching build and tests are pending; last verified app remains d2d.

Pool receipt: /private/tmp/alchemy-native-transient-retirement-validation-20261111/
validation-receipt.json. Actual policy450/14 and combined plan/policy2941/45,
four Release checks and final pinned source peer pass. All211680bounded old/new
description tuples match. Seventeen critical bodies/cases/failure branches stay
exact; no low-level Bloom/shader change. Cutout receipt:
/private/tmp/alchemy-native-cutout-depth-pin-validation-20261111/validation-receipt.json.
Same1230GPU assertions fail12before and pass after; final source peer0, original
shared vertex/other PSOs/seed order/blend/depth unchanged. Tests are device-only.

New source-mapped gaps remain explicit rather than hidden by dead-code removal.
gl_tail owns private legacy cutout traversal cleanup, retaining the real before-
alpha/end stream boundary and tracing next viewport consumers. Its alpha-copy
trace found native RGBA8 display alpha is quantized before final scaling, while
original final copy filters raw F16 alpha once; bounded device reproduction is
pending before any semantics retirement. RLVa owns separate world/MSL SMAA work:
original-active stencil optimization is unported, and alternate sample metrics
use sample dimensions instead of original output dimensions. Hardware mask and
unequal-size controls are required. Phase-ID census is read-only;34phase IDs
are unconsumed, but native setting equality/publication must survive retirement.
References: /private/tmp/alchemy-native-phase-resource-audit-20261111.json,
/private/tmp/alchemy-smaa-stencil-reference-audit-20261111.json and
/private/tmp/alchemy-cutout-legacy-owner-census-20261111.json. No overlapping
world/pipeline ownership; root retains CMake/docs/integration.

## SMAA and final cutout precision admission

SMAA core bfff8e8373 (child316b0b331c) and caller dd796eabdd (a0c963cc9f)
restore original RenderSMAAUseStencil and output-sized alternate sample metrics.
The retained private edge D32F/S8 marks initial threshold survivors, including
later contrast-suppressed RGzero; weights load/test/keep that mask through a
temporary pass view, publishing color-only weights. Neighborhood stays intact.
Exact original mark/test/discard/matrix and variable-size SamplePath maps:
/private/tmp/alchemy-native-smaa-stencil-validation-20261111/validation-receipt.json
and /private/tmp/alchemy-smaa-stencil-reference-audit-20261111.json.
Unchanged-core baseline756/4 has78failures; fixed2552/4 passes API/shader.
Hardware mask observation, independent stencil clear blocking nonzero weights,
unequal-source analytic offsets, output equality, cancel/retry and encoded mask
lease controls distinguish real execution from a no-op flag. Root registered
build82307 and2552/4+world105889/23+reload2304/8 pass:
native-smaa-registered-validation.json. One actual production caller reads the
applied native input; no live setting action is claimed.

Alpha initial1ad08 baseline64/1 has4physical failures: native early U8 coverage
quantization differs from original rawF16 BilinearMirror final-copy control.
Core5fa7c0df52 (d09172ef3f) and caller b20f03e40e (a82c74b918) restore HDR10/
SDR8 post color and optional raw F16 final coverage, without changing192B
constants or existing RGB math. A separate function-constant PSO samples raw
coverage at original untransformed final UV after lens/pane/print RGB work.
Invalid source shape/format/mip/usage/MSAA/array fails closed. The obsolete
preserve_alpha field and HDRRGBA8 PSO axis are removed after exact reader audit.
Caller supplies the current sharp local target only for cutout; it survives
through optional batch commit before moving to the capture owner.

Intermediate1540/4 success retained the old HDR8 workaround and is explicitly
superseded. Final2252/5 passes API/shader, including asymmetric rows, unchanged
RGB/effects, original HDR10 RGB+F16alpha independent resample controls, positive
wrong-premature-U8 RGB difference, invalid input/held output, cancellation and
encoded source retirement. Exact unchanged giant prefix/suffix outside the
approved outdated HDR8 block is proven. Receipts:
/private/tmp/alchemy-native-cutout-alpha-validation-20261111/validation-receipt.json,
/private/tmp/alchemy-hdr-cutout-display-format-audit-20261111.json and
/private/tmp/alchemy-native-cutout-alpha-caller-validation-20261111/validation-receipt.json.
Root matching build64115 and focused10665/17+world105890/23+reload2304/8 pass:
native-cutout-alpha-registered-validation.json. No actual snapshot/Scope invoked.

Only after that native companion passed, legacy3b5a6456e5 (afe3eb3437) retires
duplicate seed/copy calls, their sole triangle/VB/program graph and temporary
state churn. Original once positions retain unbind/flush then MODELVIEW; the
native RGBA read retains a pre-read unbind. Copy-only viewport has no next native
consumer and is omitted. drawFarPlaneQuad/gDebug/preview subtree stay. Child
actual-body recorder462/15 and three Release syntax TUs pass; recorder proves
dispatch/order, not actual shader/GPU state. Peer/source/queue maps:
/private/tmp/alchemy-native-cutout-legacy-validation-20261111/validation-receipt.json.
Root corrects only the stale helper-name comment in lldrawpoolsimple.cpp.
Full matching app/utility build after this closure remains pending. menu_port
now exclusively implements the independent34phase-description retirement;
RLVa and gl_tail perform read-only remaining-feature audits. No overlapping
World/MSL/pipeline edits; root owns canonical CMake/docs/integration.

Full original parity, stability, runtime native-only, independent release evidence
and measured multi-scene performance remain open. Earlier lighting/self-turn/
camera/voice notes and owner-stopped20FPS campaign remain unchanged. Keep goal
active; bounded build/test/source success is not product completion.
