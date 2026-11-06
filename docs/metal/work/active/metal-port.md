# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../../state/archive/INDEX.md).
**Renderer checkpoint:** Commit `cafe91682c`.

This ledger contains current decisions and executable work. Prior checkpoint
detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build/reference baseline | doing | Release 68442 builds, stages, signs and verifies 349/349 ARM64 slices; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | Strict inventory is 2,151 and exits 1; `sdl_gl` is zero and product link audit is clean; runtime audit remains open. |
| M-003 | Native drawable, presentation and lifecycle | doing | SDL/Cocoa window source has no GL setup/context/swap owner; Metal surface is sole presentation owner; attended login validation remains. |
| M-004 | Metal resources, transfers, readback and sync | doing | Ordinary geometry, fonts/textures, async uploads, frame completion, GPU/uniform buffers and sampler descriptions are native-only. Texture-slot resource identities follow. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production metallib and native encoders execute. Pass/variant and sampler identities configure without GL objects or GLSL/program names. |
| M-006 | UI, fonts, HUD and media ingress | doing | Font/UI resources are native; prior owner check accepted readable UI text. Snapshot controls/preview remain visually open. |
| M-007 | World scene and material correctness | doing | Core world, terrain, materials, avatars, shadows, sky, water and atmosphere have direct Metal owners; broad parity remains open. |
| M-008 | Advanced views/effects and compute | doing | Core effects and many diagnostics are native. Debug/Develop-only tails remain behind the usable-core context cut. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Essential no-context/no-link viewer precedes optional-path cleanup; strict, visual and stability gates remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Startup, windows, utilities, workers, linkage, frame completion, GPU/uniform buffers and samplers require no GL context/library. Texture-slot and render tails remain. |
| M-012 | Development Computer Use / LLUI accessibility | complete | Semantic operation verified; comparison driving is owner-attended only. |
| M-013 | RLVa strict parity | deferred | Owner-designated nonessential ordering; minimum final Metal/no-GL behavior remains required. |

States describe evidence, not effort. `complete` applies only to a bounded item
and never implies product completion.

## Current decision — essential-first no-context cut

Continue removing all application-owned OpenGL, but prioritize the shortest
source-correct path to a usable logged-in Metal-only evaluation candidate:

1. window/startup/context and renderer initialization;
2. normal shader/pass identities and bindings;
3. UI/text, world/avatar/material, texture streaming and presentation;
4. Snapshot/readback and other normal user workflows;
5. Debug/Develop/RLVa-only trigger paths;
6. final link/runtime/source zero proof.

This is ordering, not scope reduction. Do not delete required features merely
to lower inventory, do not retain a fallback, and do not let an unreviewed
visual row block source- and test-proven authority removal.
## Current implementation boundary

The existing native presentation/resource checkpoints remain active: Metal is
the only visible post-startup presenter; ordinary production vertex/index
stores, font/local/default/fetched textures and asynchronous uploads create no
parallel GL resources; Snapshot has no GL capture scratch/reconcile path. Core
world/material/avatar/shadow/sky/water/atmosphere layers and converted effects
have direct native owners. Exact history and visual artifacts are archived.

The essential initialization sequence now advances through:

- `9e90be44e1`: renderer capability truth comes from the Metal device rather
  than `gGLManager`.
- `70eca3a0c9`: native renderer initialization is separate from GL init.
- `e3ec32d3b2`: render-state transitions remain semantic without a context.
- `349c0278c3`: texture/shader binding bookkeeping is contextless.
- `3087308155`: loader-authored pass and rigged/classic/mirror variant graphs
  configure as native identities without opening/compiling/caching GLSL or
  creating program/shader names. MSL/Metal owners remain executable authority.

`isComplete()` still means a live linked GL program. Native consumers use the
separate `isNativeConfigured()`/`isAvailable()` contract. GL-backed occlusion
continues to fail closed; it is not falsely enabled through a semantic identity.

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

`cafe91682c` deletes GL sampler allocation, parameter and deletion calls.
Stable logical identities retain cache/generation behavior; backend-neutral
filter, mip, wrap, anisotropy and sRGB descriptions feed Metal directly.

## Validation checkpoint

- M1 Max host doctor passes (macOS 26.5.2, 64 GiB, Xcode 26.6, Apple family 7).
- Source-exact Ninja warnings-as-errors build/stage passes at `cafe91682c`;
  package `26.4.0.68454`.
- `tools/metal/verify_app.sh`: 349/349 inspected slices ARM64; deep strict
  codesign passes.
- Executable SHA-256:
  `c6cb92f0d09e62742973603d863630fb590f49b42f05242d7b4ef8daf992bfff`.
- Metallib SHA-256:
  `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- `llrender_tests`: 1,721 assertions / 217 cases pass plus expected font skip.
- No-context cutover CTest registration: 16/16 renderer tests pass.
- Native Metal/render CTest registration: 16/16 pass; 517 total tests remain
  after six Darwin GL-only suites are omitted.
- Fresh shared renderer tests: 3,698 assertions / 111 cases pass.
- Focused shared shader tests: 1,353 assertions / 25 cases pass plus 13
  hardware skips.
- Broad shared tests reproduce baseline host/driver failures: four assertions
  in three frame-fence timing cases and one render-target depth-format assertion.
- Harness structure passes 32/32 required files; tool tests pass 31/31.
- Strict inventory: 2,151 findings in 2,409 files (`gl_function` 556,
  `gl_shader` 1,597, `sdl_gl` 0), expected exit 1. Report SHA-256:
  `6fefba7089f2da35d016dafaa87ce1859ed647735dd3a89214e7ce40b704fc8c`.
  This is 11 below the immediately prior 2,162 checkpoint and 150 below the
  owner's earlier 2,301 reference. No audit rule or suppression changed.

## Owner-attended visual cadence

Do not autonomously launch either viewer, navigate login/world UI, or capture a
comparison. Add bounded questions and prerequisites to the
[daily visual queue](visual-review-queue.md). An unreviewed row is not parity
evidence and does not stop safe GL authority removal.

## Complete-product checkpoint gate

1. **Product advancement:** native renderer startup semantics no longer require
   GL capability, state, binding or program ownership.
2. **Replacement boundary:** visible/hidden contexts and the product OpenGL
   link are gone; the reachable source tail must be replaced without losing
   essentials.
3. **Open release gates:** strict source/runtime no-GL, feature and visual parity,
   prolonged stability, independent review and measured performance.

## Immediate executable action

1. Migrate ordinary texture-slot resource identities and their remaining
   pipeline-specific consumers onto native Metal state.
2. Continue normal UI/text, world/avatar/material, streaming, presentation and
   Snapshot/readback before optional Debug/Develop/RLVa-only tails.
3. Continue strict source and Mach-O link audits; collect runtime image/module
   proof during the next owner-attended viewer execution.
4. Keep the no-context startup/login check owner-attended and queued while
   source/test-proven authority removal continues.

## Non-negotiable open gates

- No retained OpenGL, translation renderer or fallback backend in the product.
- Preserve Alchemy functionality; replace GPU-dependent behavior as necessary.
- Do not mark feature parity from build/test success alone.
- No performance claim without provenance-complete, quality-equivalent paired
  M1 Max measurements meeting `docs/metal/PERFORMANCE.md`.
- Keep fail-closed native authority, Snapshot/readback and presentation gates.
