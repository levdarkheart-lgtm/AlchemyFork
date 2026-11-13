# Remaining upstream backports: native texture correctness — 2026-11-13

## Source, ownership and retained scope

- Worktree `/private/tmp/alchemy-upstream-remaining-texture-20261113`, branch `codex/upstream-remaining-texture-20261113`, clean base `b04d9bf6ef86d275a747f54618090afbea7354d9`.
- Local coherent commit `9c318e57b332014e297b4b8487048524ddf7badf`,22exclusive files,+922/-54. No push.
- Device observed Apple M1 Max,10CPU cores/64GB,arm64,macOS26.5.2. Relevant continuity/original-viewer-reference/metal-port/GPU-validation skills and product/architecture/validation/register/work instructions read. Source behavior traced before editing.
- Root owns canonical PPD-029/035/040, documentation, CMake registration, viewerobject/vovolume consumers and integration. This worktree edits none of those shared files. Source/test helper APIs and exact object/HUD boost hunks were sent to integrator at `/private/tmp/alchemy-remaining-texture-material-boost-hunks.patch`.
- Required CMake integration: `newview/altextureboost.cpp`, public `altextureboost.h`, `newview/tests/altextureboost_test.cpp`; `llrender_native_test_sources` adds `tests/llimagegl_test.cpp`. Root owns those CMake changes.

## Original-to-native behavior mapping and changes

### PPD-029 — valid large readback

Original upstream `45007f232d` expands accepted readback dimensions from2048 toMAX_IMAGE_SIZE4096 and refuses invalid requests without LL_ERRS. Adapted precisely to the existing native `LLImageGL::readBackRaw`, preserving its published-generation dimensions/format/Metal mirror/CPU byte conversion and native command lifetime. No GL/PBO/async readback backend imported. Actual4096x2 native upload/readback returns identical bytes;8192x2 native upload remains readable only through correctly bounded APIs and this LLImageRaw request returns false with no allocation/fatal exception.

### PPD-035 — final alpha answers notify face owners

Import coherent final `bcce2bce3c` +`d66e1f4852` +`830be4b4f4` semantics:

- LLGLTexture registers itself as owner and clears that raw owner pointer before destruction. Pending analysis may retain LLImageGL without retaining its outer owner.
- LLImageGL records last answered mask separately from provisional no-mask, marks a consumed provisional answer atomically, and notifies owner only on changed final answer or mask following a consumed provisional no-mask.
- Main-thread final analysis, published pending alpha, synchronous analysis and disabled-mask answer use the same owner notification. Provisional clear is silent.
- Existing native `stagesAlpha`, begin/cancel/publish/snapshot boundaries remain. A failed/cancelled native upload changes no published alpha answer; successful publication notifies after staged alpha is admitted. Owner cleanup still increments the analysis serial, so an old queued result is ignored.
- LLViewerFetchedTexture rebuilds live groups whose diffuse alpha-pool faces depend on the changed answer, in both mask-to-blend and blend-to-mask directions. It retains native gPipeline rebuild ownership; no GL draw/bind code imported.

Native tests drive actual Metal creation plus actual General/mainloop deferred-analysis queues and main-thread callbacks, same-answer silence, consumed/unread provisional windows, owner detach, cancelled old analysis, fresh ownerless upload, and staged cancellation/publication. Original and native ownership are mapped; actual in-world foliage/pass screenshots remain owner verification, not inferred from these tests.

### PPD-040 — scoped demand, visibility, decay and fetch floors

- `a4e519127d`: exact upstream ALTextureBoostLevels/ALTextureBoost helper and tests. Own-role priority, counted RAII holds and frame-limited requests combine by maximum without overwriting another owner. Preview/profile/picker/material-preview/thumbnail/RLVa consumers use held/frame boosts. Fetched/LOD stats lapse stale frame demand; sculpt role uses its own role level so a temporary selection does not become permanent sculpt priority.
- Existing native accounting has no upstream updateVRAMClass/onWorldUseChanged; those accounting-only conflict lines are deliberately omitted. Effective boost callback still sets high-priority desired discard, retaining existing behavior. Native allocation/residency/pressure machinery is unchanged.
- `2ffcc7daf6`: real current LLDrawable/LLViewerOctreeGroup visibility stamps plus per-face transformed agent-space box determine seen demand. LLFace area alone is no longer counted as visibility. Root native pipeline still calls the existing setVisible/group stamps during its actual traversals; no fabricated visible flag inserted.
- `3397e5f6cc`: teleport/stat reset clears both carried sliced-walk maxima.
- `6811bd93f6`: media size window resets maximum and re-arms counter instead of ineffectively adding zero.
- `32a7d36381` +`1e3cc89683` +`95df03ec7f`: completed no-better fetches stop at current GPU level, saved sculpt-copy level or failed-forced-copy GPU floor; forced refetch/saved-copy destruction reset failure state. Include only the small CPU prerequisite from `076e6080ee`: a texture with a valid saved raw copy and no GPU level is not marked missing before floor handling. No asynchronous sculpt/PBO overhaul.
- Source diff confirms decoder4normal/6maximum/adaptive policy, OpenHTJ2K, native publication/residency, upload worker and application settings unchanged.
- Root owns matching selection/HUD consumers in viewerobject/vovolume; helper API is `boostThisFrame(S32)`, move-only `ALTextureBoost::hold/release/get` and vector holds for distinct HUD textures, trimmed per update and cleared at markDead.

## Actual verification

`git diff --check`/staged check exit0 before local commit; worktree clean afterward.

Command `python3 /private/tmp/alchemy-remaining-texture-validate-20261113.py` from this worktree:

-15translation units compile exit0 under canonical ARM64/C++23/Release/O3/Werror flags with worktree source headers and **no stale PCH**: llgltexture,llimagegl,altextureslot;10actual viewer consumers including llface,llviewertexture/list,RLVa and previews/newboost helper; new native regression and upstream boost test.
- Build/link selected tests outside canonical build using unchanged dependency/support archives read-only; no canonical build/object/app mutation by this workstream.
- `/private/tmp/alchemy-remaining-texture-checks-20261113/texture_tests --reporter console --colour-mode none`, env `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`: exit0, **1,170assertions/10cases**, native Metal API Validation Enabled and Metal GPU Validation Enabled observed in output.
- Includes6upstream boost-level policy cases and4new real native image cases described above.
- Frozen test SHA256 `6d975113d342f281671691925fc1e12f3d4ba93fc01a6689edfe77fb22edb37e`.
- Exact command-generation script, per-unit compile logs, compile-results.json, selected test link argv/log, runtime log and source-hashes.json live under the checks directory above. Per-invocation compile manifest records newly compiled objects; subsequent invocations skip current objects without changing them.

### Same-source failing controls

`python3 /private/tmp/alchemy-remaining-texture-red-controls-20261113.py`, same current headers/test objects/device and validation environment, compiles only a variant LLImageGL translation unit and relinks selected tests:

1. Remove only `mOwner->onAlphaMaskChanged()` from the current final-answer method: all3alpha cases fail, **10failed checks/58assertions**, exit42. Native math/creation/deferred queues remain identical.
2. Restore exact original fatal2048guard in current native readback body:4096case fails with unexpected exception `LL_ERRS: LLImageGL::readBackRaw: bogus params:4096x2x4`, **1failed check/7assertions**, exit42. The corrected native binary passes the same request and byte equality.

Receipts `red-controls.json`, per-control source/compile/link/run logs retain actual negative results; original tests and committed source were not edited for controls.

### Retained test construction attempts

- Initial fixture compilation attempted to access private sSkipAnalyzeAlpha; removed that unnecessary test access, preserving normal production default. No production fix for this compile error.
- First native run passed9/10cases but expected a queued alpha answer after destroying the outer owner. Existing owner cleanup invalidates the old analysis serial, so the correct expected result is cancellation. Test now checks cancellation and then verifies a new ownerless upload completes. Production implementation unchanged between that failed expectation and final passing run. Initial tool output recorded1failed check/1,168assertions; raw runtime log was overwritten by the final run, so no separate raw failed-run artifact is claimed.

## Deferred performance project — no implementation or measured gain

Retirement/purge/accounting are deliberately separate:

- #578 exact-format storage/category VRAM accounting (`f5c6b9e433`, `21c84cb4ef`, `66b3e72d62`, `2983d929a8`, reporting tail) must reconcile existing native descriptor/block/mip/slice/sample and image-owned counters before pressure policies change. Do not sum old GL-style totals with current native totals.
- #601 departed-region ownership, shared-live texture unmarking and native resource release (`dcf0b6356b`, `06a8260cbf`), reach/retirement integration and emergency purge spreading (`681ac651f8`) require source current owner/fence/callback admission and grid lifecycle measurement. Dropping a texture still referenced by live regions or queued GPU work is not authorized here.
- #568 asynchronous loaded-callback/sculpt readback is also deferred; native completion tickets and CPU-copy budgets require a separate implementation/validation boundary. Only accepted-size validation and the minimal saved-copy fetch prerequisite are changed now.

Next: integrator imports matching HUD/selection consumer hunks and CMake registrations, performs combined doctor/build/tests/app/source verification, then owner scene tests. Full viewer runtime, comparative loading/FPS/memory benefit and owner acceptance/push remain unverified. Builds/tests cannot close PPDs or authorize publication.
