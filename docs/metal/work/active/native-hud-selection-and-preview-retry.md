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

Root exclusively owns canonical integration/CMake/matrix/docs. Menu owns the
pipeline/HUD fix, private highlight inl/new test and reserved offscreen GPU slot.
RLVa owns three preview cpp files/private model inl/existing test; CPU/compile
until GPU release. GL-tail peers preview changes. Separate worktrees only.
Integrate reviewed source-pinned red/green fixes, register private sources/tests,
build/check isolated without overwriting owner-passed f254 or delivered Alpha4
stages. Future artifacts need a new version/path/checksum. Product gates stay open.
