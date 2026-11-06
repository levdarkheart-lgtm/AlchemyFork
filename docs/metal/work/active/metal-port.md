# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a
demo. **Evidence contract:** `docs/metal/VALIDATION.md` and
`docs/metal/PERFORMANCE.md`. **History:** [archive index](../../state/archive/INDEX.md).
**Renderer checkpoint:** Commit `def3811797`.

This ledger contains current decisions and executable work. Prior checkpoint
detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build/reference baseline | doing | Release 68442 builds, stages, signs and verifies 349/349 ARM64 slices; controlled performance baseline remains open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | Strict inventory is 2,315 and exits 1; product executable link audit is clean; runtime audit remains open. |
| M-003 | Native drawable, presentation and lifecycle | doing | Darwin now creates a plain SDL/Cocoa window and sole opaque Metal surface with no GL context; owner-attended startup/login validation remains. |
| M-004 | Metal resources, transfers, readback and sync | doing | Ordinary geometry, fonts/textures and async uploads are native-only; Snapshot has no GL scratch path. Remaining GL resource owners follow no-context exposure. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Production metallib and native encoders execute. Pass/variant identities configure without GLSL or GL program names. |
| M-006 | UI, fonts, HUD and media ingress | doing | Font/UI resources are native; prior owner check accepted readable UI text. Snapshot controls/preview remain visually open. |
| M-007 | World scene and material correctness | doing | Core world, terrain, materials, avatars, shadows, sky, water and atmosphere have direct Metal owners; broad parity remains open. |
| M-008 | Advanced views/effects and compute | doing | Core effects and many diagnostics are native. Debug/Develop-only tails remain behind the usable-core context cut. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Essential no-context/no-link viewer precedes optional-path cleanup; strict, visual and stability gates remain open. |
| M-010 | M1 Max profiling and optimization | deferred | Qualifying paired quality-equivalent measurements follow stable parity. |
| M-011 | Remove duplicate GL authority owner by owner | doing | Visible/hidden startup, utilities and product linkage require no GL context/library. Reachable source tail remains. |
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

## Validation checkpoint

- M1 Max host doctor passes (macOS 26.5.2, 64 GiB, Xcode 26.6, Apple family 7).
- `tools/metal/build.sh`: canonical Xcode Release warnings-as-errors,
  `BUILD SUCCEEDED`; package `26.4.0.68442`.
- `tools/metal/verify_app.sh`: 349/349 inspected slices ARM64; deep strict
  codesign passes.
- Executable SHA-256:
  `9daf87d570fe90ac6d285983a0a441e95f98742312c3a3f378bc08c61b24a22c`.
- Metallib SHA-256:
  `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- `llrender_tests`: 1,605 assertions / 209 cases pass plus expected font skip.
- No-context cutover CTest registration: 14/14 renderer tests pass.
- Fresh shared renderer tests: 3,698 assertions / 111 cases pass.
- Focused shared shader tests: 1,353 assertions / 25 cases pass plus 13
  hardware skips.
- Broad shared tests reproduce baseline host/driver failures: four assertions
  in three frame-fence timing cases and one render-target depth-format assertion.
- Harness structure passes 32/32 required files; tool tests pass 31/31.
- Strict inventory: 2,315 findings in 2,409 files (`gl_function` 651,
  `gl_shader` 1,597, `sdl_gl` 69), expected exit 1. Report SHA-256:
  `ea3e9358541641189dd3884534ee0b64a5157d6388a3daadb12472ceba4d7efe`.
  This is nine below the immediately prior 2,324 checkpoint. The earlier 2,301
  baseline predates later conservative shader-bridge inventory expansion.

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

1. Audit essential startup/login and ordinary-frame reachability for calls
   exposed by the absent context; replace owners rather than restoring GL.
2. Convert normal UI/text, world/avatar/material, streaming, presentation and
   Snapshot/readback paths before optional Debug/Develop/RLVa-only tails.
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
