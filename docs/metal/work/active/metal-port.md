# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a demo.
**Evidence contract:** `docs/metal/VALIDATION.md` and `docs/metal/PERFORMANCE.md`. **History:** [archive index](../../state/archive/INDEX.md).
**Renderer checkpoint:** Commit `af65f062a1` (`Fix native directional shadow capture fallbacks`).

This ledger contains current decisions and executable work. Prior checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build/reference baseline | doing | Exact-base OpenGL `67607` and diagnostics-free Metal Release both build, stage, sign and verify 349/349 ARM64 slices; controlled visual/performance baselines remain open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | Strict inventory is 1,797 and exits 1; `sdl_gl` is zero and product link audit is clean; runtime audit remains owner-led. |
| M-003 | Native drawable, presentation and lifecycle | doing | SDL/Cocoa window source has no GL setup/context/swap owner; Metal surface is sole presentation owner; attended Mid login/world load passes, while broader lifecycle/stability coverage remains. |
| M-004 | Metal resources, transfers, readback and sync | doing | Exact-descriptor Metal target pooling retains owned attachments under a byte bound; streamed images use a separate containment capacity while original override/controller semantics remain. Copy/readback consumers and multi-scene policy validation follow. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production metallib and native encoders execute. Pass/variant, sampler and texture identities configure without GL objects, placeholder textures or GLSL/program names. |
| M-006 | UI, fonts, HUD and media ingress | doing | Reusable native world/UI output and copied depth eliminate per-frame target recreation. Empty UI clips now preserve original no-op semantics; Conversations visibly opens/closes/reopens in attended 68497 evidence. |
| M-007 | World scene and material correctness | doing | Core world, terrain, materials, avatars, shadows, sky, water and atmosphere have direct Metal owners. Attended `68502` shows four populated main directional cascades and stable moving avatar/object shadows; exact-base reference `67607` is rebuilt, while parity remains open because the exploratory pair used the wrong executable and the saved QA profile is mismatched. |
| M-008 | Advanced views/effects and compute | doing | Native mirror/probe capture publishes scene reflections with bounded target reuse. Attended 68499 toggle-off and mirrors-off login pass with synchronous hero retirement; visual parity and the separate shadow freeze remain open. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Essential no-context/no-link viewer precedes optional-path cleanup; strict, visual and stability gates remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Startup, windows, utilities, workers, linkage, completion, resources, targets, UI replay and retained glow/SMAA require no GL. Render tails remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation verified; comparison driving is owner-attended only. |
| M-013 | RLVa strict parity | deferred | Owner-designated nonessential ordering; minimum final Metal/no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded item and never implies product completion.

## Current decision — essential-first no-context cut

Continue removing all application-owned OpenGL, but prioritize the shortest source-correct path to a usable logged-in Metal-only evaluation candidate:

1. window/startup/context and renderer initialization;
2. normal shader/pass identities and bindings;
3. UI/text, world/avatar/material, texture streaming and presentation;
4. Snapshot/readback and other normal user workflows;
5. Debug/Develop/RLVa-only trigger paths;
6. final link/runtime/source zero proof.

This is ordering, not scope reduction. Do not delete required features merely to lower inventory, do not retain a fallback, and do not let an unreviewed visual row block source- and test-proven authority removal.
## Current implementation boundary

Metal is the only visible post-startup presenter. Production geometry, textures/fonts and async uploads create no parallel GL resources; Snapshot has no GL capture scratch/reconcile path. Core world/material/avatar/shadow/sky/water/atmosphere layers and converted effects have direct native owners.

The essential initialization sequence now advances through:

- `9e90be44e1`: renderer capability truth comes from the Metal device rather
  than `gGLManager`.
- `70eca3a0c9`: native renderer initialization is separate from GL init.
- `e3ec32d3b2`: render-state transitions remain semantic without a context.
- `349c0278c3`: texture/shader binding bookkeeping is contextless.
- `3087308155`: loader-authored pass and rigged/classic/mirror variant graphs
  configure as native identities without opening/compiling/caching GLSL or
  creating program/shader names. MSL/Metal owners remain executable authority.

`isComplete()` still means a live linked GL program; native consumers use the separate native availability contract and GL-backed occlusion fails closed.

`22def49efb` removes the Darwin OpenGL-window property, GL attributes,
context/current/shared-context lifecycle, `gGLManager` startup/shutdown and GL
renderer initialization. The compiled Release `llwindowsdl.o` has no undefined
`SDL_GL_*`, `gl*` or `LLGLManager` imports. Metal device/scheduler/surface,
display metrics, input, fullscreen, resize, vsync and telemetry remain. The
viewer was not launched under the owner-attended policy, so the exact
startup/login check is queued and not claimed from build success.

`def3811797` removes the hidden Darwin GL backend and CGL shared-context code,
routes the appearance utility through native Metal initialization, removes
OpenGL from product `llrender` linkage and uses a repository-owned SDL overlay
with Darwin GL/Vulkan platform backends disabled. Release viewer, renderer-test
and appearance-utility executables have no OpenGL/Vulkan dynamic dependency or
unresolved GL/CGL/SDL_GL/Vulkan imports. Generic unselected SDL archive stubs
do not enter the product executable.

`1a76b76a6d` deletes the hidden backend's remaining cross-platform GL context
implementation and both texture/vertex worker shared-context fallbacks. The
texture worker retains asynchronous Metal upload batching and fail-closed
authority checks. Darwin omits six legacy GL-only migration suites rather than
linking a test-only fallback runtime.

`f7ecf74e84` deletes visible-window GL attributes, context creation/currentness,
shared contexts, swap, teardown and reset polling while retaining the plain
SDL/Cocoa lifecycle, native Metal surface, VSync, input, resize and fullscreen.
Darwin GL procedure lookup now fails closed; the `sdl_gl` audit class is zero.

`c865c15220` deletes ordinary-frame GL fence ownership and maps logical viewer
frames to the scheduler's ordered Metal command-submission timeline. Completion
polling, pacing, resource-reuse waits and teardown draining remain functional;
they no longer create, wait on or delete `GLsync` objects.

`8bd5ff1bfd` replaces GL GPU-buffer storage/transfers with private/shared
`MTLBuffer`, ordered blits and exceptional synchronous readback. Timeline-safe
stream rings refuse open-frame reuse; binding/query/compute tails remain.

`dd41713fea` deletes `ALUniformBuffer`'s GL allocation, mapping, upload, binding
and scratch calls. Private/shared Metal stores publish native buffer, aligned
offset, size and serial. Timeline-safe rings grow, spill or fail closed;
existing producers remain and historical update modes select one native path.

`cafe91682c` deletes GL sampler allocation/configuration/deletion; stable
identities feed Metal directly. `6ec5884b85` gives slots real Metal texture
identity/dimension and deletes native-image white-GL-placeholder fallback.
`e607b19e27` deletes slot driver binding/unit calls; unresolved integer names
fail closed. `07e4d7a626` publishes real Metal attachments and replaces legacy
FBO allocation, borrowed/shared depth/color, clear, mip, copy, resample, swap
and sampling behavior without deleting the features.

`029694fedb` converts image slots from GL image units to typed Metal writable resources with validated access/subresources and fail-closed raw integers. The
dormant GL depth-pyramid owner/GLSL are removed; production water hierarchy and patch culling remain in the native Metal world renderer.

`e8eaa3701b` replaces transient-target GL side effects/poison clears with native attachment clears; shared depth remains owner-controlled.

`768250a2fc` removes GL-frame synchronization from native exposure/lens-flare history; fixed EV 0/black bootstraps and retained native history prevent the logged Snapshot preflight rejection. Runtime UI/save proof remains owner-attended.

`ecf223cf4c` deletes scale-down/CEF GL tails; `a3067b1aac` makes noise/SMAA
lifecycle native; `9c74277821` makes font atlas allocation, glyph publication
and release Metal-only with no GL fallback and exact native readback proof.

`60e350e29a` removes orphan cube-array/framebuffer-copy GL owners; `c62977bd49` makes ordinary texture publication Metal-only; `8a00656289` removes uncalled GL ownership modes; `b86b1e4c53` repairs Snapshot state; `1081614942` removes legacy GL queries; `2e255de87c` makes ordinary texture residency, invalidation, cache identity, preview/save, terrain/map/avatar/bump and projectors native.

`6e5047e50b` deletes retained UI/text geometry's raw texture/sampler identities and GL replay fallback. `b971465ca7` deletes superseded retained GL glow/SMAA. `f77024a20a` makes lens-dirt generation Metal-authoritative. `1bfe631627` removes `LLRender`'s dormant GL driver-state, clear, draw and compute authority while retaining semantic raster/blend/depth/viewport/scissor state for native pipeline descriptions; unsupported instanced/indirect/compute endpoints fail closed. Package 68492 is the bounded owner-attended startup/login/world-load candidate, not a parity, stability or GL-zero claim.

### Attended Mid runtime correction — 2026-11-07

The original OpenGL path was traced before correcting the Low-to-Mid black screen: it draws one tessellated wave patch plus three surrounding flat-water groups. Metal emitted four groups, but `captureMetalWorldWaterSurface()` accepted only the non-wave count of three and invalidated each affected post-deferred transaction.
Commit `2e2ac6e1c9` centralizes `ALMetalWaterSurface::expectedFieldDrawCount()` for capture and renderer validation and adds a four-group GPU assertion. Release `26.4.0.68493` (built from the mixed working checkpoint containing that correction at 2026-11-07 01:08:12 GMT; executable SHA-256 `dfd02973a45085b790c334f1c47574549ce632b6b9e6f6a377946574f972ffd0`) verifies 349/349 ARM64 slices.
An attended direct login to persisted Mid with HDR rendered for 500+ complete native world/UI publications; `first_incomplete_line=8073` and Metal validation/GPU errors did not recur in `/tmp/alchemy-metal-68493-water-wave-release-20261107.log`. This is bounded Mid evidence, not a High/Ultra or broad parity pass.

### Resource/UI corrections — 2026-11-07
Commits `ab98aae4a9` and `abde32f46e` record bounded mirror resources and original-compatible empty UI clips/visible Conversations; archived evidence is in the [mirror](../../state/archive/CURRENT-mirror-resource-lifecycle-checkpoint-2026-11-07.md) and [UI](../../state/archive/CURRENT-empty-ui-clip-checkpoint-2026-11-07.md) checkpoints. `dbe1215cea` applies the original count-zero/no-mirror contract before early return, gates direct Metal consumers on the live mirror plan and retires only hero source/radiance/binding state when mirrors are disabled. In attended 68499, live toggle-off visibly worked and logged retirement; the separate mirrors-off-before-login run visibly loaded and logged successful login plus continuous world/UI publication, disproving the prior hypothesis for this build.

### Directional-shadow correction — 2026-11-07

Original tracing showed sparse null GLTF-material slots are fragmented batch gaps and streaming textures use the default image; Metal had rejected a whole cascade for either condition. Commit `af65f062a1` preserves sparse slots with inert material data, applies the default native texture to streaming legacy/PBR/tree/covered-avatar casters and keeps null avatar images fail-closed. In attended `26.4.0.68502`, four populated `3472x1920` cascades, moving caster shadows and streaming stability pass; one separate hero-probe terrain attempt recovered on retry. Release/349-slice verification and 52,774 assertions/57 cases pass.
The owner-attended exploratory pair used `/Applications/Alchemy Beta.app` (`7.1.9.2516`, executable SHA-256 `a361437f58866d9f41b029b21048cebc6dee635ee8678e41d40dfa5a20daa75a`) rather than the QA reference, so its darker/softer Metal result is not an acceptance decision. External validation HEAD `734afd3179` (exact renderer base `1eb44bbd1b` plus three instrumentation-only commits) now produces a complete, deeply signed, 349/349 ARM64 `67607` reference with executable SHA-256 `de8fc8f817c0afb2063f641e9c24cbd58054c32c3053f4d0e863232498bb1797`. Its exact-path QA dry-run passes, but the saved profile reports level 3/exposure 1.1; correct and live-verify Mid/exposure 1.0/no sharpening before the controlled pair.

## Validation checkpoint

- M1 Max host doctor passes (macOS 26.5.2, 64 GiB, Xcode 26.6, Apple family 7).
- Source-exact Xcode Release warnings-as-errors build/stage passes at `1bfe631627` as package `26.4.0.68492`; the same checkpoint's Ninja RelWithDebInfo rebuild/stage and focused llrender test pass.
- `tools/metal/verify_app.sh`: 349/349 inspected slices are ARM64.
- Executable SHA-256: `8c752d22881b2113ebf2be2b44ab09a9610e9e3e2e7066a516fea7ac005eabf4`.
- Metallib SHA-256: `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- Focused no-context renderer passes 220 assertions / 11 cases; query fail-closed proof passes 35 assertions; llrender passes 1,856/1,856 and Metal passes 52,681/52,681 assertions.
- Native glow/SMAA pass 19,815 assertions/four GPU cases; native lens-dirt/display-post ownership pass 3,019 assertions/two cases; native font atlas upload/readback/release passes 28 assertions/five cases.
- Native transient pool passes 943 assertions / 14 cases; full alrender passes 556,617/556,617 assertions across 132 cases.
- Full harness passes 517/517 tests in 1,204.60 seconds; six Darwin GL-only migration suites remain intentionally omitted.
- Fresh shared renderer tests: 3,698 assertions / 111 cases pass.
- Focused shared shader tests: 1,353 assertions / 25 cases pass plus 13 hardware skips.
- Broad shared tests reproduce baseline host/driver failures: four assertions in three frame-fence timing cases and one render-target depth-format assertion.
- Focused alrender/llrender suites pass 28/28; harness structure passes 32/32 required files; tool tests pass 31/31.
- Release 68493 verifies 349/349 ARM64 slices; attended Mid/HDR remains stable through 500+ native world/UI publications without the prior water-count rejection.
- Commit `dbe1215cea`: diagnostics-free Xcode Release 68499 builds and verifies 349/349 ARM64 slices; the separate Ninja Metal suite passes 52,772 assertions/57 cases. Release `ctest` has no registrations because its preset has tests disabled; attended toggle-off and mirrors-off startup/login both pass.
- Commit `af65f062a1`: Xcode Release 68502 builds and verifies 349/349 ARM64 slices; the focused Metal suite passes 52,774 assertions/57 cases; attended diagnostics show four populated main directional cascades and stable moving avatar/object shadows.
- External exact-base OpenGL validation HEAD `734afd3179`: configure and full Release build/stage exit 0; the `26.4.0.67607` app contains 8,111 files including `settings_files.xml`, passes deep strict signing and 349/349 ARM64 verification, and its executable SHA-256 is `de8fc8f817c0afb2063f641e9c24cbd58054c32c3053f4d0e863232498bb1797`.
- Strict inventory: 1,797/2,403 files (`gl_function` 340, `gl_shader` 1,457, `sdl_gl` 0), expected exit 1; report SHA-256 is `219eb943960b124745e171a8d420e9ab983e74c04de50ad52f19a2ce47fee74d`; 331 below 2,128 and 504 below 2,301 with unchanged rules.
## Owner-attended visual cadence

Launch/login/capture only at the owner's explicit request and control; add bounded checks to the [daily visual queue](visual-review-queue.md).

## Complete-product checkpoint gate

1. **Product advancement:** native renderer startup semantics no longer require GL capability, state, binding or program ownership.
2. **Replacement boundary:** visible/hidden contexts and the product OpenGL link are gone; the reachable source tail must be replaced without losing essentials.
3. **Open release gates:** strict source/runtime no-GL, feature and visual parity, prolonged stability, independent review and measured performance.

## Immediate executable action

1. Correct and live-verify the Mid/exposure 1.0/no-sharpening QA profile, then rerun the controlled shadow pair against rebuilt OpenGL reference `67607` and trace any confirmed direct-light/shadow-filter difference.
2. Refine and recheck the transient hero-probe terrain shadow rejection while preserving fail-closed publication.
3. Revalidate mirror reflection content/avatar/clipping separately from the passing setting lifecycle.
4. Classify texture churn and run all four scenarios before tuning capacity.
5. Continue the highest-value unblocked native-ownership cut.

## Non-negotiable open gates

- No retained OpenGL, translation renderer or fallback backend in the product.
- Preserve Alchemy functionality; replace GPU-dependent behavior as necessary.
- Do not mark feature parity from build/test success alone.
- No performance claim without provenance-complete, quality-equivalent paired M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Keep fail-closed native authority, Snapshot/readback and presentation gates.
