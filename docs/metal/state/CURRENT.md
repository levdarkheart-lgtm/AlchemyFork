# Current engineering state — native shader ownership cleanup

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `30ef2ccb694ee3030ff059ec00f084de3fe7c748`.
Remaining-feature/OpenGL-removal goal stays active, including RLVa and
Debug/Developer behavior. No viewer/appearance utility launch or screenshot.
No account/cache/settings/camera/location/simulator state changes. Separate
20FPS campaign remains deferred.

## Current implementation

[Native shader ownership cleanup](../work/active/native-shader-owner-cleanup.md)
removes the dormant shared GL compiler/linker, binary cache, reflection, uniform
upload/cache, stand-in program, shader/query/resource entrypoints and GL query/
compute capability initialization. Preserve native semantic pass identities,
eight-corner variants, CPU uniform values, batching widths, minimum-alpha and
exact current reserved-table order. Remove the obsolete GL compiler suite after
moving meaningful native lifecycle/table checks. Remove the write-only engine
block registry and unused storage-buffer name cache; retain actual producer
structs, CPU data, packing and native ABI checks.

Forced rebuild now refreshes actual world/probe-world and UI Metal libraries/
PSOs, including used lazy variants and material argument encoder caches. Retain
external targets, histories, pending commands and old material leases. Reject
active transactions, stage per owner, requeue failures and withhold incomplete
world publication. Native diagnostics use actual capabilities or unavailable
state, never GL queries or fabricated GL measurements.

Keep [native buffer views](../work/active/debug-buffer-visualization.md),
[appearance loader](../work/active/appearance-native-loader.md) and prior
[RLVa/Developer/settings fixes](../work/active/remaining-feature-port.md), plus all
earlier world/avatar/editing/travel/music/HUD/upload repairs.

## Observed validation

Doctor/configure0. Final native test and optional appearance Release builds0.
Metal API/shader-validated tests pass35550assertions/32cases:1347/26 native
metadata/diagnostics/baking/regressions,74/2 reload ownership,34129/4 world/UI/
RLVa/display regressions. CPU sentinels verify reporting makes zero GL queries
with either Metal or unavailable capabilities. Offscreen GPU tests retain real
targets and material command leases through shader replacement. Neither app
was launched.

First canonical build failed65 at link: root accidentally removed
LLViewerShaderMgr::built with the old loader range. Restore original failure
tracking helper in30ef2ccb69. Failure receipt is retained. Corrected frozen
configure0 and canonical Release build0; bundle26.4.0d68655 matches30ef2ccb69.
ARM64 inspection passes349 Mach-O files/zero invalid slices, deep strict codesign0.
Viewer imports contain no OpenGL framework or GL/CGL/SDL_GL undefined imports.
Exact output hashes are pinned in the linked work note and identities receipt.

Final optional utility is ARM64, SHA256
`336570864d723317f0046b992df687728acc96d701e2acf830642b1475b3bc50`.
No OpenGL framework or GL/CGL/SDL_GL undefined imports. Harness validation0.
Ignored receipts: `artifacts/metal/feature-completion-20261111/`.
Strict audit still exits1:1441findings/2406files,159GL-call-syntax matches
(previous1783/333). Names/comments/excluded code contribute; this is not a live
call count and omits some owned directories. Runtime context proof stays open.

## Complete-product advancement assessment

M-008/M-011 advance through native feature consumers, executable shader reload
and removal of displaced GL ownership. M-013 remains doing. Full menu/RLVa
parity, strict no-GL, prolonged stability, independent release review and
multi-scene measured performance remain open. M-012 remains complete; M-014
deferred. No release-readiness or performance claim.

## Exact next actions and ownership

Root: integrate foundation896971dd84, utility1c1fd66c74,
previewab8da73a9e and vertexcd8b13d2dd after reviewing exact diffs. Wire the
builtin revision announcement only after successful forced world/UI+metadata
rebuild, register new GPU test files, and run native/GPU regressions plus builds.
Do not announce revisions on ordinary setting reloads or failed requests.
These commits are not yet integrated or GPU-tested. No viewer launch.

Independent utility worktree:
`/private/tmp/alchemy-utility-shader-reload-20261111`,
branch `codex/native-utility-reload`, base30ef2ccb69, ownergl_tail_audit.
Owns almetalpipeline builtin revision API, textureops, terrainpaint and retained
background scaling plus its new test file. No canonical-root changes.

Independent preview worktree:
`/private/tmp/alchemy-preview-shader-reload-20261111`,
branch `codex/native-preview-reload`, samebase, ownermenu_port.
Owns sculpt/lit and PBR preview renderers plus its new test file. It consumes only
the utility agent's tiny shared API foundation commit. Root owns integration,
CMake, central revision announcement, tests and canonical builds. Both utility
and preview implementations are committed/clean and syntax-checked; actual GPU
execution is still required.

Independent vertex-upload worktree:
`/private/tmp/alchemy-vertex-upload-cleanup-20261111`,
branch `codex/native-vertex-upload-cleanup`, samebase, ownerrlva_port.
Owns llvertexbuffer.cpp/.h and its native tests only. Remove dormant GL allocator/
upload ownership while preserving staging, retirement and independent retry state.
Draw/client-array and image tails remain separate traced boundaries.

Independent Developer Frame Profile worktree:
`/private/tmp/alchemy-developer-frame-profile-20261111`,
branch `codex/native-developer-frame-profile`, base896971dd84, ownermenu_port.
Owns stage timing/pipeline helpers, llviewerdisplay and a new focused test file.
Utility pipeline API changes are frozen; ownership transferred without overlap.
Root independently verified M1 Max supports stage-boundary timestamp counters
only: no draw/dispatch/blit/tile sampling or statistics set. Report actual pass/
compute intervals and MSL identities; per-pipeline time/statistics unavailable.

Current forced reload covers world/UI, not all persistent native shaders.
These worktrees must refresh remaining utility/preview owners without discarding
external/generated textures or in-flight leases, and regenerate the PBR preview
BRDF LUT. Then implement Developer Frame Profile at actual Metal encoder/
pipeline boundaries. Native legacy query timing/statistics are unsupported;
do not emit zero/fake GPU data or activate semantic CPU-bind timing hooks.

Continue actual GL manager/loader, vertex-buffer and image tails from original
source. Preserve earlier unresolved lighting, self-turn, camera and voice issues
recorded in their linked work notes; no speculative unrelated policy changes.
