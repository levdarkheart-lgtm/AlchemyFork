# HUD face selection and preview publication — 2026-11-11

Continuation starts at clean6bfad82535, with rendering/tool/workflow source
identical to built/tested114fedf245. Previous goal turn made progress: combined
build/tests/static checks, normal push and verified manual Alpha4 ZIP delivery.
Preserve that artifact and checksum. No viewer/window/utility/screenshot action.
Original reference:1eb44bbd1bdc76d0100c75699bad1fb337e8cb45.

## Concrete HUD selected-face gap

Original render_ui -> render_hud_attachments -> HUD stateSort ->
renderGeomPostDeferred -> renderHighlights -> renderSelectedFaces ->
LLFace::renderSelected executes an indexed draw. Selected bit and material UV
channel govern the checker overlay, separately from object outlines/manipulators.
Original highlightF multiplies sampled RGBA by the selected/normal/specular or
physical-touch color. Rigged selected-face drawing was disabled in the original.

Native6bf retains the caller and filtering, but LLFace enters
captureMetalWorldHighlightBatch. Its world post-deferred capture has already
closed before authoritative UI/HUD capture, so it silently drops HUD selection.
Source receipt: /private/tmp/alchemy-native-feature-consumer-completion-20261111/audit-receipt.json.
Menu agent owns isolated pipeline fix/new actual-owner test. Preserve checker,
transforms, UVs, order, read-only depth and alpha behavior; invalid data must
invalidate the authoritative capture. Ordinary world behavior must remain.
Shared actual capture/renderHighlights bodies are permitted for testing; a named
prefiltered-selection facade is not actual LLFace/stateSort/GLTF/viewer execution.
Preserve those untouched source bodies and record that scope honestly.

## Adjacent preview generation gates

Original Sculpt render explicitly calls setBuffer before drawing, as do Model
static/physics/skinned draws. Original BVH renderAvatars reaches
LLViewerJointMesh::drawShape, which also calls setBuffer. Native6bf extraction
instead checks only raw Metal handles. Failed real LLVB publication keeps an
allocated/prior initialized handle while the current CPU store stays dirty.
Global flush retries but does not certify the current generation after failure.

The prior avatar-image repair already retries/requires both published stores.
RLVa agent owns the three adjacent cpp files and existing preview test: direct
setBuffer +current vertex/index predicates before extraction, no shared API or
scene/math/cloth/physics change. Exact model append-helper extraction permits
actual-body proof with real LLVB/native rendering and named light inputs.
First allocation, independent warm vertex/index failure, global-flush failure
and no-new-write retry are required controls. Sculpt/BVH source wiring/compile
remains distinct from actual UI or full avatar/model construction.

Preview child2e11a19053 integrated as39beddedd7, adding private CMake listing
and removing only an extra EOF blank line; helper body unchanged. Exact-fixture
old helper927assertions/13fail ->fixed1494/1pass, combined3354/7API/GPU pass.
Three actual production ReleaseTUs compile0; independent5file peer no blockers.
Authored initial bad linear/sRGB descriptor failure is preserved separately,
corrected before the exact final red/green fixture; no production/tolerance change.
/private/tmp/alchemy-native-indexed-preview-validation-20261111/validation-receipt.json
and /private/tmp/alchemy-native-indexed-preview-peer-gl-tail-20261111.json.
Full model/Sculpt/BVH scenes remain unexecuted; matching root build pending.

## Additional original owner duties

Root/menu source review finds original renderHighlights owned an enable-blend
scope omitted by the prior native port; the actual UI/HUD caller can enter with
blend disabled. The world selected batch also defaulted to alpha writes despite
original renderGeomPostDeferred's RGB-only mask. Menu is restoring both duties
with actual-owner blend-off and native world alpha-mask controls before freeze.

Selected child05e1def23b integrated208dcee3dc, with private source/test CMake
registration and only an extra EOF blank line removed. Actual6case final suite
passes18,150assertions with API/GPU validation, after813baseline failures in
the identical final fixture. Isolated original-wrong alpha44, missing blend648
and removed flush27 failure controls distinguish the duties. Root independently
mapped original RGB-only mask and original LLGLSPipelineAlpha enable scope.
Both actual production ReleaseTUs compile0; independent4file source peer has no
blockers. Original notices and global renderHighlights scope are retained.
World selected capture now records the caller's alpha mask; scene math/filtering
and the ordinary world suffix otherwise stay exact. No new shader/renderer/API.
/private/tmp/alchemy-native-hud-selection-validation-20261111/validation-receipt.json
and /private/tmp/alchemy-native-hud-selection-peer-rlva-20261111/peer-receipt.json.
Root hud-selection-integration-map.json verifies child bytes except EOF cleanup.
Full actual HUD selected-TE/GLTF scene remains unexecuted; source unchanged.

GL-tail confirms three more dropped HUD overlays through the same spatial/HUD
debug traversal: TextureAnim yellow half-alpha triangles, UpdateType five-color
half-alpha triangles, and light bounds/radius line boxes. Original ShadowFrusta
and global particle-raycast crosshair explicitly exclude hud_only and remain
world-only. Exact source contracts and filters:
/private/tmp/alchemy-native-hud-diagnostic-admission-audit-20261111/audit-receipt.json.
GL-tail owns only a new private three-method inl/native test; root will integrate
pipeline removal/include serially after menu releases shared-file ownership.

Diagnostics child2d53ea6dea integrated as578191f6c6. Root verifies the three old
definitions still match6bf exactly, removes only those and includes the private
inl at former LightDebugBox site; both new files are exact child bytes. Private
source and native test are registered in existing CMake targets. Actual same-
fixture baseline61,654/3 has15failures; fixed61,654/3pass. Generated actual
pipeline Release projection compiles0; independent two-file peer no blockers.
Original world suffixes, full notice and24vertex box construction remain exact.
The tests use named Face/DrawInfo/Capture inputs with real LLVB/gGL/native
offscreen rendering. API layer enabled and shader-validation environment are
recorded; no direct shader-validation property probe or drop-before-commit
resource control was executed by these three cases. Existing owner unchanged.
/private/tmp/alchemy-native-hud-diagnostics-validation-20261111/validation-receipt.json
and /private/tmp/alchemy-native-hud-diagnostics-peer-menu-20261111/peer-receipt.json.

## Per-owner resource qualification

The same original retry boundary requires a wider source audit. RLVa confirms
opaque simple-pool capture enters captureMetalWorldGBufferBatch with raw handles;
LLRenderPass::pushBatch can record native geometry before its later setBuffer.
Failed real LLVB publication retains a prior initialized handle plus dirty CPU
store, so a later successful retry can leave that batch on the earlier store.
Material-preview and appearance-hint raw extraction are additional live chains.
This is a per-draw ordering/publication issue; unrelated pending previews must
not block the world through a global dirty-resource gate.

Frozen578 census classifies170tokens/12files/42owners plus API; these are source
counts, not visual bug counts. Five pipeline owners consume only vertices; direct
ALMetalBuffer owners are outside LLVB publication flags. Evidence:
/private/tmp/alchemy-native-buffer-publication-census-20261111/audit-receipt.json.

Core child355 integrated4a6 adds nonvirtual prepareMetalBuffers(require_indices):
setBuffer followed by current vertex and conditional index readiness. No fields,
layout/vtable/raw getter changes, global dirty/device veto or unrelated-buffer
blocking. Real LLVB fixtures cover first/warm V/I failures, repeated global flush,
unrelated stores, nonindexed dirty-unused-index admission, clean absent publisher,
no-new-write recovery and previously encoded leases. Explicit raw-presence policy
control (not historical API)14663/2fails8 ->fixed14663/2; combined19875/4API/GPU.
/private/tmp/alchemy-native-buffer-publication-owner-validation-20261111/validation-receipt.json
and /private/tmp/alchemy-native-buffer-publication-core-peer-gl-tail-20261111.json.

Preview childb364 integratedcc788 changes only three material/appearance guards;
two actual ReleaseTUs and whole-file reverse projections pass. Rootef0 qualifies
cached UI before first handle capture plus two HUD drawpool owners and selection
outline, retaining existing failure policies. Real LLVertexBufferData draw and
drawWithMatrix tests expose two old-body failures; fixed16460/1 and combined
82335/5API/GPU pass. Actual drawpool/outline Release syntax and independent peer
pass. Initial adjacent stale-PCH syntax failures are preserved; private textual
PCH copies fix the test command without changing production.
/private/tmp/alchemy-native-ui-buffer-publication-validation-20261111/validation-receipt.json
and /private/tmp/alchemy-native-ui-buffer-publication-peer-menu-20261111/peer-receipt.json.

Pipeline childdfcdea integrated17574e covers31owners/32guards before handle
capture after producer writes. Five vertex-only owners require no index;26indexed
owners have27guards including opaque/water GBuffer. Actual GBuffer/Star bodies
share private source with the fixture; other owner projections preserve filters,
layouts, material math and failure policy. Occlusion's mutable buffer signature
matches its real mutable caller; no cast. Same fixture old282/5fails35 ->fixed
306/5API/GPU; wrong Star-index requirement28/1fails3. Four actual production
ReleaseTUs and independent peer pass. Full pipeline/viewer not constructed.
/private/tmp/alchemy-native-pipeline-buffer-publication-validation-20261111/validation-receipt.json
and /private/tmp/alchemy-native-pipeline-buffer-publication-peer-gl-tail-20261111.json.

## Final destination allocation recovery

Original dynamic texture postRender145–163 retries absent GL backing after a
successful scene. Current constructor generates only once and targetless scheduler
does not retry destinations; factories retain owners. Four previews can therefore
remain pending after initial failure: Model, BVH, visual-parameter hint and glTF.
Bake composite can adopt without old storage when metadata is valid, but failure
before LLImageGL metadata bootstrap leaves discard=-1 and adoption impossible.
/private/tmp/alchemy-native-dynamic-destination-lifecycle-audit-20261111/audit-receipt.json.

Child8bfb integrated8ce7 adds native nonvirtual ensureMetalTexture, preserving
healthy backing and retrying missing backing through unchanged generator. Four
preview gates use it; glTF retains original failure classification/camera flow.
Bake preRender bootstraps only null/uninitialized metadata, retaining storage-
independent adoption for valid metadata. Scheduler unchanged; no fields/vtable.
Five private actual-body sources carry nine methods/full original notices. CPU
Reset bodies are unchanged and execute without a publisher. Root CMake lists all
five private sources; existing preview test remains registered.

Actual old-body fixture2123/10fails5 ->fixed3991/10; combined7357/18 with API/GPU
validation; CPU reset12/1 and six actual ReleaseTUs pass. Failed final adoption
retains prior image/stamp/pixels/dirty/update callbacks. Initial scaffold duplicate
viewport alias and authored float-versus-byte color mismatch are preserved and
corrected in fixture only, before final paired runs; no production/tolerance fix.
Source proof confirms nine baseline bodies/five old test bodies/generator/scheduler
and all13frozen hashes. Independent peer no blockers, pinned8bfb225ebb.
/private/tmp/alchemy-native-dynamic-destination-retry-validation-20261111/validation-receipt.json
and /private/tmp/alchemy-native-dynamic-destination-peer-gl-tail-20261111.json.
Camera/scene/readiness wrappers are named inputs; no full Model/BVH/avatar/layer
content execution, physical OOM injection or general recovery claim.

## Bounded closures and ownership

RLVa semantics review finds no further original-active defect:33 exact body/
default/constructor/tween projections and9 original-identical command/CPU files;
actual multi-effect/tween/touch/restart bridge remains unexecuted.
/private/tmp/alchemy-rlva-effect-semantics-closure-20261111/audit-receipt.json.
Upload audit resolves original/current format producers, four media senders and
utility forwarding. Packed RGB8/wider swapped-RGB API gaps have no retained
producer and are not asserted correct; no generic GL-format emulation is needed.
/private/tmp/alchemy-native-image-upload-format-closure-20261111/audit-receipt.json.
Window gamma get/set/restore, including SDL3's explicit ramp no-op, are exactly
original-identical; do not invent an algorithm and call it original behavior.

Root exclusively owns canonical integration/CMake/matrix/docs. All child source
released/integrated and peer-reviewed; all device handles terminal. No identified
code fix pending. Freeze docs and build final Alpha4 in isolated followup tree,
preserving existing Alpha4 before configure and owner-passed f254 in place.
Matching full registered tests/static/strict checks precede normal feature-branch
push and new ZIP+SHA/extraction/signature verification. Concurrent untracked
research files are excluded/untouched. No further CI dispatch/upload/viewer/
window/utility/screenshot action. Product gates remain open.
