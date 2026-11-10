# Multi-avatar source comparison — 2026-11-10

Candidate code: `76413f2fcf76e528fcfe3740635111dcd4a54ea8` (docs HEAD
`663d30c9b2`). Reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
This is a source audit, not a crowd benchmark or visual-parity result. No viewer
launch, settings change, source repair, build or GPU execution in this audit.
Prepared build 68630 does not fix the gaps below. Root owns integration; agents
performed independent read-only comparisons and prepared one private small patch.

## Preserved crowd policies

Seventeen avatar methods were compared byte-for-byte with the exact reference,
including pixel-area ranking, update period/needs-update, visibility, complexity,
visual muting and updateImpostors. Seven relevant settings blocks match.
The original and native rank visible avatars by screen area, reserve self rank 1,
apply the configured maximum full-detail avatars and simplify others using baked
impostors. Registered RenderAvatarMaxNonImpostors is 12; saved/profile overrides
can differ, and zero or the slider's unlimited value disables that limit.
Do not claim all avatars are always rendered in full in Metal.

Each avatar has its own draw pool and skin cache. Native classic mesh palettes
use independent owned arrays; cloth data is copied per batch. Frame-data palette
deduplication compares actual payload bytes, including view rebase origin. No
cross-avatar pose/palette alias was established in this bounded inspection.

## Confirmed missing GPU-cost input

Original ALQueryPool::beginSpan/result supplied asynchronous timestamp/elapsed
measurements. Native `indra/llrender/alquerypool.cpp:222` always returns an invalid
span, and its span result always expires. ALGPUTimer therefore never opens.
`LLVOAvatar::readProfileQuery` cannot populate the derived avatar GPU render-time
field (initialized to zero in llvoavatar.h); `updateTooSlow` still compares that
field against positive ART thresholds. GPU-time-based simplification and avatar/
attachment profiling lack their native measurement producer. Geometry-complexity,
mute/friends and rank limits remain separate and functioning source policies.
This matters when the time-based policy is selected; current session settings
and the size of any FPS impact were not measured.

Restoring generic timer calls alone is insufficient: profileAvatar's isolated
avatar/attachment work must actually be encoded natively, associated with the
correct owner, and measured asynchronously without a main-thread GPU wait.

## Confirmed view-local impostor ownership mismatch

Original generateImpostor stores its complete G-buffer in avatar->mImpostor;
the same avatar-owned bake is read in main and reflection views. Native published
textures instead live in MetalWorldSimpleCapture::impostor_textures, keyed by
avatar UUID but owned by each capture independently (pipeline.cpp:897).
publishMetalImpostorBake stores only in the current capture (:16315), while
ordinary/hero face entry swaps the main and probe capture owners (:1731/:1971).
captureMetalWorldImpostorBatch looks only in the active owner's map (:16181).
The reflection pool loop reaches that reader for enabled avatar/control-avatar
pools and complete logical impostors. It cannot find a valid main-owner bake in
the other map: it clears the collected view and requests another avatar bake.
Re-baking on the normal scheduler does not share that publication with the probe
owner. View/bake HDR and normal-format settings use the same existing latches;
the missing cross-view owner, not a demonstrated conversion need, is the issue.
Avatar destruction also erases only the active map (:16343).

The repair boundary is one avatar-owned native published cache, with format/device
invalidation and complete-generation lifetime, visible to every rendering view.
Retain per-avatar UUID ownership; do not substitute a global last-avatar texture.
Actual affected reflection pixels and runtime failure counts remain unobserved.

## Confirmed unloaded-avatar admission mismatch

Original LLDrawPoolAvatar::renderAvatars skips a not-fully-loaded avatar. It draws
a debug sphere only for pass zero with particles disabled or max particles <= 0.
Native getMetalAvatar (:279) unconditionally invalidates the entire active direct
reflection transaction for every unloaded avatar, including cases where the
original produces no geometry. Other completed avatars/world content are then
withheld too. invalidateMetalReflectionProbeScene is probe-scoped, so this finding
does not establish that this particular call rejects the ordinary main view.
A private reviewed patch narrows invalidation to the original debug-sphere case;
it is NOT integrated or executed. No small existing viewer-manager test exercises
this admission boundary, and backend shader tests would not establish it.

## Pending body texture boundary

Original LLViewerJointMesh::drawShape binds the selected texture through the
standard immediate-update/default-image path. Direct captureMetalWorldAvatarShape
does not bind it, and captureMetalWorldAvatarBatch rejects a missing native image,
clearing the current collected view. This is reachable with RenderUnloadedAvatar
enabled and pending baked textures; default is false. Ready-avatar streaming stats
elsewhere do not restore the draw-local placeholder contract. This is distinct
from the UI/HUD texture fallback repairs already in 68630 and is not repaired here.

## Validation limits and next implementation boundaries

Native multi-avatar visual/performance parity remains unverified. No measured
claim is made that any gap explains a particular live FPS or screenshot.
Repair shared native impostor publication and original unloaded-avatar skip/
placeholder semantics first; verify two independent avatar bakes in main and
reflection views, destruction/replacement, and one unfinished avatar among ready
ones. Restore native isolated GPU profiling as a separate ownership/timing task.
The paused 20 FPS campaign is not resumed by this correctness audit.
