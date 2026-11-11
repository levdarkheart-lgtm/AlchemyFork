# Native Scene Monitor, sampled UI and RenderInfo — 2026-11-11

Renderer checkpoint0b219b9728b7dda9864357e1546a7e7337ce274c on
codex/apple-silicon-metal. Physical M1 Max64GiB/macOS26.5.2/Xcode26.6.
Original source1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 inspected before fixes.
Goal active; no viewer/appearance/window/menu/screenshot/user-cache action.
Actual offscreen native GPU commands/pixel readbacks are bounded test evidence.

## Scene Monitor ownership and original behavior

22e648236a/9e4c7eb894 (children9d7da20e26/14e508eea8) replace Windows-only
comparison/query rendering and the native no-op window-copy caller. Root frame
getter1c77fa745a/7162dd92f8 accepts only exact current complete main-world
transactions, excluding snapshots/probes/cutouts/warm-up/uncommitted work.
Reuse ALMetalRetainedBackground surface normalization (actual BilinearMirror
color/PointClamp depth) so reduced world resolution matches pre-UI window bytes.
Two source leases are pinned through actual command completion; dropping the
ticket/service during blocked GPU work cannot recycle pooled source handles.

Preserve original lower-left RGB8 crop, PointClamp comparison into full diff,
raw4x4Bayer channel cutoff, RGBA8 quantization, BilinearMirror half-size sample,
strict RGBsum<tolerance and numeric .5 legacy normalization. Original alpha is
constant1 from RGB8 capture, so input alpha differences are ignored. Raw Bayer
bytes remain raw despite sRGB storage because original sampler SKIP_DECODE.
Original double-face count comment is unsupported by actual two-triangle quad;
no duplicate work is invented. Any finite tolerance is accepted; negative/all
and above3/zero outcomes match original comparisons, NaN/inf fail honestly.

Initial cutoff test had14 failures: actual normalized n/255 texture read and
fast reciprocal expression differ one ulp. Independent actual38assertion read
control proves round(read*255) recovers exact authored bytes. Use integer RGB8
delta/BayerByte*15 comparison without epsilon or widened tolerance. Do not
claim bit identity with incidental old driver floating ties. Same GPU oracle
passes after correction. Final registered root513assertions/sevencases0 with
Metal API/shader validation: channels/alpha/equality, bilinear threshold, crop/
orientation, odd extents/normalization, finite tolerances/invalid inputs and
actual blocked completion/source-pool lifetime.

Viewer retains cadence, independent nonblocking result fetch and trace data;
failed/unavailable measurements never record successful zero. Keep last
completed debug result while pending. Restore only state actually frozen by
monitor (prior FreezeTime/render flags/particle budget/avatar pauses), and
refreeze active monitor on public/teleport reset. Disable/shutdown guards remain.
Visible draw retains original ALWAYS/no-depth-write bracket. Policy is source/
Release checked, not a live viewer result. Root shader-manager/monitor syntax0.

c50db7562f registers native service/tests, removes displaced two shader globals/
Windows loaders/four GLSL files, and removes uncalled LLRenderTarget window-copy
stub. Only corresponding old driver case removed; all other old state bodies
compare identical. Native crop/current-owner/real-output tests replace its
meaningful source-selection boundary; no live window test claimed. Reserved
uniform indices remain unchanged.

## Raw and target texture admission

634bbd02c5 (child00c56d1c23) fixes existing bindNative raw identity never reaching
authoritative UI texture/sampler admission; mirror mode also lacked explicit
texture forwarding. Original manual bind flushes preceding work and applies
sampler changes. Native hook does that explicitly; optional typed draw argument
keeps cached LLImageGL replay independent of current raw slot. Invalid slot/
type/integer/depth/usage is rejected; image display lists cannot retain raw
borrowed owners and invalidate instead of storing an unowned pointer. Existing
begin/end saved state restores exact image/raw/type/sampler bindings. Caller
retains wrapper through flush; actual commands retain encoded native resources.

Real red two-case baseline1153failed/1578assertions: raw stripes white, mirror
draw incomplete. Child final core7209/8GPU0 includes retained-image replay after
caller image release despite unrelated current raw texture. Five ReleaseTUs0.
9e27977324 (child6881ce2fd8) fixes same bug in target convenience overload:
red772/798 (white pixels plus falsely accepted depth/unknown sampler), final
8007/9GPU0. Preserve resolved ALSamplerDesc exactly (implicit BilinearMirror,
PointClamp and custom S-repeat/T-clamp), not a reconstructed bit mask. Normal
world target/depth metadata remains unchanged. Independent reviews no blockers.

abc91e6dec/32894ca519 (child885eabe439) actual Scene Monitor filtered texture ->
authoritative UI test: red80assertions/eight white-pixel failures, unchanged
oracle fixed80/onecase0 with API/shader validation. Actual asymmetric BGRA
red/green/blue/yellow pixels, replace/alpha blend over retained black+depth0 and
numeric ticket destruction afterflush/beforeUIcommit all proved. No viewer hook.

## Native RenderInfo

9814944134/0b219b9728 (child1545967f56) restore missing typed-world contribution
to DebugShowRenderInfo. Original LLVertexBuffer counted issued draws and input
elements (including instances); native world/shadow/water/post owners bypass
that generic path. ALMetalRenderEncoder now records actual emission sites,
including joined-index flushes, direct/static, instanced and patch draws. Publish
once at encoder end, independent of timing/commit/completion; generation reset
discards older encoders. Default disabled, no GPU query/wait/readback/IO.

Count every native UI emission too. Earlier encoder-wide UI opt-out missed
background/HUD/selection; actual red four failures/two cases while all pixels
passed. Final actual mixed background/foreground/resume/HUD gives4calls/12
elements with pixel oracles. HUD uses native totals alone; accepted LLRender
UI batches are a separately labeled unit. No per-draw opt-out framework.
ICB only reports range/slot metadata and explicitly unknown total draws/elements;
no guessed GPU contents. Exact direct records count their actual indices.
Overflow saturates/marks unknown. Function-static state follows synchronous
encoder owners; no speculative heap lifetime sink or timing-sink change.
Child actual11455assertions/16cases0 includes six new counter cases and existing
join/indirect/timing, API/shader validation. Seven ReleaseTUs/link0, peer reviews
clean. Frozen root9918 affected Metal73523/38 and llrender96864/384 pass
(383passed/one known CPAL skip), separate Font/UI1862/61pass.

## Buffer tail and receipts

b7433702b0/31e07a06d8 (childbd97c56add) remove unconsumed ALGPUStream and zero
getGLName stub. Five old storage invariants retain/expand actual native transfer,
partial read/nonzero clear/static+selfcopy/coherent map/GPU readback/move/bytes/
queued-copy retirement; three stream cases test an API with zero original and
current product users. All retained ALGPUBuffer bodies compare unchanged;
uniform/frame owners untouched. Root registered108assertions/fourcases0,
API/shader validation. Independent review no blockers. Full wrapper retirement
awaits separate actual consumer/diagnostic audit after uniform cleanup.

Root artifacts/metal/feature-completion-20261111/: native-buffer-invariant-*,
native-scene-monitor-registered-*, native-scene-monitor-viewer-syntax.json,
native-scene-monitor-tail-preservation.json, native-raw-ui-integrated-build.log,
native-scene-monitor-ui-integrated-build.log, native-ui-sampled-target-build.log,
native-scene-render-info-final-test-build.log. Child receipts:
/private/tmp/alchemy-native-scene-monitor-validation-20261111.json,
/private/tmp/alchemy-native-scene-monitor-ui-validation-20261111.json,
/private/tmp/alchemy-native-ui-raw-texture-validation-20261111/validation-receipt.json,
target-followup-receipt.json in same directory,
/private/tmp/alchemy-native-render-info-validation-20261111.json,
/private/tmp/alchemy-native-gpu-buffer-validation-20261111/validation-receipt.json.

## Frozen canonical verification and corrected fixtures

Frozen source9918621da8fc7bbc3de3edbd63d33150c8e00d1a includes renderer0b and
one shared-depth test assertion correction: wrappers differ but retain the
same nativeHandle. Initial one-failure alrender log is preserved; final
556641assertions/132cases0. Doctor/configure/canonical build/verify/deep strict
codesign0, version26.4.0d68744,349ARM64/zero invalid slices, viewer imports0.
Optional appearance Release build/ARM64/signature/imports0; no launch.
ViewerSHA256 e2ba381f3e2bdb85efc32a198bd9ce6fce9e838b743bd532b7a41cd6074abf8e;
metallib5491d075dc49ae016bd5844ae3cbf131ba2b17f8bb36f3489ef5dcaa413003ad;
appearance8d8518f72f7ba3562e9b4fc6c0a24144429e29815c487a3f4ed1ca4b0dfd1ba4.
Exact source/commands/hashes: scene-ui-canonical-source.json,
scene-ui-canonical-identities.json, scene-ui-appearance-static.json.

All349Mach-O files also inspected for imports: foreign VLC screen/vout/
caopengllayer/glconv and CEF retain GL framework/API links; videotoolbox imports
a CV OpenGL-compatibility key. This is a concrete foreign-module audit tail,
not an owned-viewer failure or whole-bundle no-GL proof. Trace actual media
selection/consumers before removal; preserve media functionality.

Root tests were rebuilt in Release and pinned in scene-ui-test-identities.json.
Two accidentally selected older RelWithDebInfo runs are excluded. Current
Release indexed-join requires an actual NSBundle metallib; unbundled run37/38
passed, then the identical executable plus exact offline asset in an offscreen
CLI test bundle passed73523/38. No source assertion weakened. Add target-local
CMake bundle resource ownership to prevent this recurring packaging error.
Integrated f75d9e306f preserves all12 historical frame-fence invariants over
nine actual native cases. Red2157/9 found one pending wait diagnostic2vs3;
only two counters change, all timeline/pacing behavior identical. Child
2318/20with scheduler/completion/slack passes; root registered fences2157/9
and actual bundled indexed-join7969/1pass API+shader validation. Target-local
almetal_tests MACOSX_BUNDLE and concrete per-config generated resource rules
use existing TARGET_FILE CTest paths; no launch or global helper changes.
Both Release/Debug resource-only update behavior also checked by independent
Foundation-only probe (notGPUproof). Root native-fences-bundled-* logs pin
the real offline asset. Deleted only the mapped554-line legacy fence fixture.
API-only cache5905/6, instrumented882/1 and selective98/1 all0; actual disk
render/compute hits1/1 withzero misses. Test receipt scene-ui-test-validation.json.
Strict production source3509files/0blockers/0errors/1177informational; including
excluded old tests4109files/486blockers/0errors (also two deliberate no-GL
negative assertions). Source results never substitute for runtime proof.

Historical filters overlap and are not summed. Goal remains active: live RLVa
RGB fill is under repair (initial G-buffer already supplies alpha to empty
forward bakes; previous alpha-gap claim corrected), zero-reader uniform/stencil
graphs remain, fence fix awaits integration. Root original final-viewport oracle
also now proves1080failed/9249assertions at world/output size mismatch: native
lens mask is computed before resize, unlike original window-pixel evaluation.
Private postchain tree owns only its test file; production fix must wait for
shared pipeline/world/MSL owner release. Full parity/stability/runtime no-GL/
independent release review/performance gates remain open.

## Silhouette, uniform, buffer, stencil and final-viewport integration

Code1f357c900e7ff42128f1de25ca999a93dbd37148 supersedes the pending work above.
b741e156ab/a92290a0f1 integrate child8c40fc6/75a497: original conditional
RGB/alpha stamp at its CPU policy boundary, normalized depth pin .999995/.000005,
both implicit stamps removed, zero-forward-batch admission, borrowed CPU fill
copied during encode and exact prior normal/profile join eligibility retained.
Initial native alpha already existed; no missing-alpha regression claimed.
Child21566/4 plus unchanged impostor replay38636/5 pass; root registered21566/4
passes in actual native test bundle. Three reviews no blockers. Exact source
and receipt /private/tmp/alchemy-native-avatar-coverage-validation-20261111/
validation-receipt.json. Original replay ignores vertex tint, so none invented.

6a4058d209 (child81fddbb17b) removes ALUniformBuffer's zero-reader native binding
table/rings, producers, packed-light/matrix shadows and dead hash counters,
mode setting/listener/five feature-table rows, four obsolete class/test files
and exact stale helper hooks. Preserve CPU Environment/Deferred/NativeShadow
layouts/math, pack-before-compaction, actual light/SUN_UP routing, native matrix
assembly/stacks and368-byte MatricesUBOData. First-bind CPU initialization now
has an explicit release-reset guard. Child8130/19CPU+native UI and378/4native
arena/completion/G-buffer owner cases pass; eight Release checks,14matched
objects/link0, ten exact body/schema proofs, independent review clean.
Receipts /private/tmp/alchemy-native-uniform-retirement-{validation,original-map}-20261111.json
and cpu-body-proof.json with the same prefix. No native constant arena removed.

c69d0fcc78 (child881822c95d) follows that zero-producer dependency: retires
ALGPUBuffer and its old4native cases, preserves five historical meaningful
storage invariants in three direct ALMetalBuffer cases (actual uploads/partial
word fill/read/static+self copy/coherent publication/GPU retirement). Wrapper
flags/move mechanics have no product owner. Remove empty zero-reader storage
metadata/capacity, preserve reserved prefix and all8CPU uniform indices.
TextureView shows accurately labeled tracked Metal buffer wrapper bytes,
excluding raw staging/post-wrapper leases; never device-wide allocation bytes.
Actual119/4with reserved metadata passes, five Release checks/link0, peer clean.
Manifest /private/tmp/alchemy-native-buffer-owner-validation-20261111.json.

3d1c751d7c/1f357c900e (child972042f5fa) remove dead ALGLStencil/ALWaterStencil,
seven LLRender recording fields/accessors/setters and exact13legacy helper
save/restore/member lines; zero remaining class/API readers. Original driver
setters had real GL calls; native water/exclusion/SMAA now own typed immutable
states/reference values. Keep those implementations, masks and actual GPU
fixtures untouched. Child167/10native state tests pass: nine bodies byte-exact,
one exact seven-assertion prune; two excluded driver cases/helpers retired and
all surviving non-stencil bodies/projections proven. Manifest under
/private/tmp/alchemy-native-stencil-foundation-validation-20261111/.

65c2dac883 (child7ee520c634) repairs actual prior final-viewport errors.
Original renderFinalize10825/10904 uses final WorldViewRaw dimensions/origin;
native previously ran effects at render resolution then resized their result.
Actual flat-color original hard-mask oracle red1080/9249failures (936at8x6,
144at64x48,32x24controlpasses), exact baseline source recovered/hash-verified.
Explicit output dimensions default to source when bothzero; matching192-byte
CPU/MSL constants carry viewport size/origin. Evaluate lens/print/seam before
UI resize. Independent final-color/world-depth sizes normalize through existing
BilinearMirror/PointClamp owners; exact snapshot lease survives UI end for
current normalized depth readback and releases on cancel. Cutouts remain color.

Peer review also caught an inherited Y inversion: offscreen rawrow0 is GLlower
row (resample_vertex/present_gl_rows), flipped only at CAMetalLayer. Corrected
positive-origin crop oracle red238/322, then native input.position.xy+origin
fixes print noise. Initial inactive-grain and inverted-oracle green fixtures
are retained as superseded controls, not final proof. Final matching-header
49366/9native GPU cases pass, including four existing post and three retained
background cases, non-square input/window mask, active translated noise, depth,
invalid-policy atomicity; textual viewer syntax0 and peer no blockers. Receipt
/private/tmp/alchemy-native-postchain-admission-validation-20261111/validation-receipt.json.
No viewer, window or snapshot workflow executed. Frozen d9/renderer1f registered
full suites and canonical app are now verified; source strict3504files/
0blockers/0errors/1170informational. Goal active; further owners are in CURRENT.

## Subsequent admission and live-caller audit

Frozen d9f624ea464d3ba3c305d7656bf9533f6e65a8dd Release26.4.0d68755:
doctor/configure/build/verify0,349Mach-O/0invalidARM64 and deepstrictcodesign0.
Main viewer forbidden framework/graphics API imports0; optional appearance
Release build/static checks0. Viewer SHA256
`a9823d27163e63eba0b35aabb88004cc1857fc763a12929d960bb671cf7536a4`,
metallib `e0337cf1984d56b4796b300dbccb63c4e39b40e84574ca444d7d75c303bd4451`.
Matching-header llrender98965/389(388pass/one known CPAL skip), Font/UI1862/61,
alrender556641/132 and affected Metal106377/50 all0, API+shader validation.
Exact command/hash/exit receipts: viewport-canonical-source.json,
viewport-canonical-identities.json, viewport-test-validation.json and
viewport-appearance-static.json under artifacts/metal/feature-completion-20261111.
Foreign VLC/CEF links remain separate; CEF libvulkan own ID is not an import.
No executable launch or measured performance claim.

Integrated88ff446002 (childadcd0055b6) removes only four zero-caller finalizers
applyCAS/applyFXAA/compositeBloomHDR/bindScreenToTexture and sole CPU CASF
include.193deletions; all remaining source bytes identical1f. Original callers
and current typed native owners mapped; Release syntax0, independent review.
Receipt /private/tmp/alchemy-native-finalizer-tail-validation-20261111/validation-receipt.json.

ab128e02e5 (childb8 postchain admission) replaces six obsolete CAS/DoF fixture/
benchmark files with five actual native cases preserving original source-size,
sharpened center, blur/glow tag, disabled near blur and shaped impulse invariants.
9ac375d389 (child708895c8e4) replaces old image/tessellation fixture mechanics
with24native image cases551assertions and four real hardware tessellation cases
193688assertions. M1 hardware barycentrics recover actual raster triangles;
bounded post-tessellation GPU writes prove outer edge mapping. No nominal count
or CPU grid substitute. Hidden-window lifecycle is live for appearance utility;
its adapted manual fixture compiles but is never executed/registered routinely.
Root strengthens slot test with actual logical/native image identity, then
registers new cases: actual194243/28 image/tessellation and170713/5 postchain
pass API+shader validation. Build also compiles manual window fixture without
running it. Obsolete SSR benchmark measured GL copy timing only and
had no product assertions; existing real native color/depth SSR-history case
preserves behavior. No performance equivalence claim.

a272c2b365 (child254f199fac) admits all eight alfarplane CPU GLSL asset reads and
both lens content scripts against actual native source/payload ABI.364/9CPU,
49tool tests and both default scripts0 in isolated receipt; default preset/
metric bytes unchanged. Two original/native model bugs corrected: six omitted
radius*mscale factors and inclusive step(hash,density) boundary. Actual old
models fail7assertions; fixed controls pass. Native GPU flare/dirt1846/2passes.
Source checks are bounded drift contracts, not algebraic equivalence proofs.
Receipt /private/tmp/alchemy-native-lens-content-validation-20261111/validation-receipt.json.

New actual-caller regressions: HUD helper isComplete alwaysfalse
skips depth clear; availability-only then flips asymmetric rows (100/803fail).
Native-only clear/restore with lower-row coordinates and pre-operation flush
passes initial803/2GPU. Integrated8bb61bb34d (child5940153141) expands to9595/9
actual GPU assertions/cases, maps all seven old fixture invariants and retires
the displaced GL fixture. Queued draws explicitly flushed at fixture final
producer boundary; original no-order mutant then fails100/818assertions. Earlier
green no-order control omitted that boundary and is superseded for ordering
proof. Both conventions, asymmetric lower rows, translated/clipped/disjoint/
empty rects, nearer HUD conditional restore, exact scene-depth equality and
unavailable identity covered. Sole viewer HUD caller is inside exact retained
world color/depth composition; no bake color-only transaction invokes it.
Viewer textual Release syntax0; independent source/lifetime review0. Receipt
/private/tmp/alchemy-native-hud-depth-validation-20261111/validation-receipt.json.

Actual lens CPU methods fail10/36positive assertions; integrateda54314153d
(child2707f854c0) corrects two availability gates, retires old generator/caller/
GL-only validity flags, transfers disabled main-view clear before history
selection, and resets native history independent of old RT completeness.
Hold, sun/moon and native epoch/device/frame guards retained. Registered
actual CPU73/3passes after Release build0, existing native HDR1846/2passes.
Extracted actual method bodies share one production inl; only viewer input
facade is scoped in test. Math body preservation proof and full original map:
/private/tmp/alchemy-lens-policy-cpu-validation-20261111.json. Root registration
and exact pending nonHDR gap are in CURRENT. All nonHDR admission guards remain
consistent until that closure; no new whole-frame rejection introduced.

Actual nonHDR probe exposed two further inherited defects: native scene8bit
cannot exceed original luminance threshold2 (exact zero state is correct);
original linear Screen alwaysRGBA16F even when HDRfalse. Native display rejects
nonHDR flare despite kernel support. RLVa now maps central float screen PSOs,
output/input descriptors and explicit HDR-dependent SSR/probe/refraction/display
boundaries before changing them. No threshold changes or fabricated positive
RGBA8 state oracle. Initial28assertion/twofailure probe preserved. No global
shader-status replacement. Full feature remains active, not complete.

Root HUD/lens registration build0 and full matching-header Release suites0:
llrender302803assertions/426cases(425pass/one known CPAL skip), Font/UI1862/61,
alrender556674/132; API+shader validation. hud-lens-render-validation.json pins
actual commands/source/binary hashes. Registered lens CPU73/3 build/run0.
Production strict audit3505files/0blockers/0errors/1141informational; excluded
historical tests still232blocking inventory findings across4099files, not a
whole-source strict pass. Both reports/logs are hud-lens-{source-audit,test-inventory}.

Foreign read-only policy audit /private/tmp/alchemy-foreign-media-audit-20261111/audit.json:
actual ARM64 VLC callback implementation forces vmem/window-none, same original
API;339module import census has zero direct importers for proposed three windowed
GL output/converter exclusions. Plugin enumeration can load unselected modules;
no runtime no-load/context claim. Preserve VT decode/CV compatibility attribute,
CoreGraphics screen input (unused legacy OpenGL framework link needs dependency
source rebuild, not feature deletion), and full required CEF/Vulkan/SwiftShader/
ICD capability set while actual browser backend remains unobserved. Root owns
future packaging/fresh-stage/callback validation. No viewer/media playback launch.
