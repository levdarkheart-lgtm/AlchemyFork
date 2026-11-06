# Current engineering state

**Observed:** 2026-11-06 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `def3811797`

**Historical evidence:** [archive index](archive/INDEX.md)

## Active implementation checkpoint

The current priority is the owner-directed essential-first OpenGL cutover: make
startup, normal UI/text, world/avatar/material rendering, streaming,
presentation and readback usable without a GL context before finishing
Debug/Develop/RLVa-only paths. This reorders all remaining removal work; it
does not relax the Metal-only product target or permit deleting required
features instead of replacing them.

Metal is already the sole post-startup presentation owner. Ordinary production
vertex/index storage, font/local/default/fetched textures and asynchronous
uploads are Metal-authoritative and create no parallel GL names. Snapshot has
no GL scratch/reconcile path. Direct Metal owners cover the normal world,
materials, avatars, shadows, sky, water, atmosphere, selection and converted
effects described by the archived checkpoints. Visual parity questions remain
queued rather than blocking source-proven authority removal.

The latest essential sequence is:

- `9e90be44e1` separates renderer capabilities from `gGLManager` and obtains
  Apple Silicon limits from the selected Metal device.
- `70eca3a0c9` separates native renderer initialization from legacy GL
  initialization.
- `e3ec32d3b2` makes renderer state semantic and safe with no current GL
  context.
- `349c0278c3` makes texture and shader binding bookkeeping contextless while
  preserving native authority.
- `3087308155` configures the loader-authored pass/rigged/classic/mirror graph
  as native semantic identities without reading, compiling or caching GLSL and
  without creating GL program/shader names. Essential avatar, terrain and
  world/UI composition consumers accept those native identities. Existing MSL
  libraries and Metal encoders still own executable GPU work.
- `22def49efb` removes the Darwin SDL OpenGL-window flag, context creation and
  currentness, shared-context path, `gGLManager` initialization/shutdown and GL
  renderer bootstrap. The opaque `CAMetalLayer`, native device/scheduler,
  Metal-only `LLRender` initialization, display/input/fullscreen lifecycle and
  renderer telemetry remain.
- `def3811797` removes the hidden Darwin GL backend and CGL shared-context
  implementation, initializes the appearance utility through the native Metal
  device/scheduler path, removes the product `llrender` OpenGL dependency and
  builds SDL with its Darwin GL/Vulkan platform backends disabled.

`LLGLSLShader::isComplete()` remains strictly a linked-GL-program predicate;
`isNativeConfigured()`/`isAvailable()` cannot disguise a native identity as a
GL object. GL-backed occlusion remains fail-closed until a native query owner
replaces it.

The Release viewer, renderer tests and appearance utility have no OpenGL or
Vulkan dynamic dependency and no unresolved CGL, NSOpenGL, GL, SDL_GL or Vulkan
imports. SDL's static archive can retain unselected generic API stubs, but the
product linker selects none of them. No viewer was launched under the
owner-attended visual policy, so startup/login behavior remains queued rather
than inferred. The next boundary is the reachable essential ordinary-frame
source tail, not optional Debug/Develop/RLVa paths.

## Build and validation evidence

- `tools/metal/doctor.sh` passes on macOS 26.5.2, MacBookPro18,2, M1 Max,
  64 GiB unified memory and Xcode 26.6; Metal reports Apple family 7 and a
  62,277,025,792-byte recommended working set.
- `tools/metal/build.sh` completed the canonical Xcode Release build with
  warnings-as-errors and ended `BUILD SUCCEEDED`.
- Staged package: `Alchemy Metal Development 26.4.0.68442`.
- `tools/metal/verify_app.sh` passes 349/349 inspected Mach-O slices as ARM64;
  `codesign --verify --deep --strict` passes.
- Viewer executable SHA-256:
  `9daf87d570fe90ac6d285983a0a441e95f98742312c3a3f378bc08c61b24a22c`.
- Packaged `almetal_builtin.metallib` SHA-256:
  `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- `llrender_tests`: 1,605 assertions pass across 209 cases, with the expected
  unavailable-font skip.
- Focused CTest registration passes 14/14, including the combined renderer
  executable, against the no-context cutover build.
- Fresh shared renderer suite: 3,698 assertions pass across 111 cases.
- Focused shared GLSL suite: 1,353 assertions pass across 25 cases, with 13
  hardware-dependent skips.
- The broad shared suite still reproduces pre-existing host/driver failures:
  four assertions in three frame-fence timing cases and one render-target
  depth-format assertion. The same failures occur in the earlier Release
  binary; changed files do not own those components.
- `python3 tools/metal/validate_harness.py` passes all 32 required files;
  `python3 -m unittest discover -s tools/metal/tests -v` passes 31/31.
- `git diff --check` passes.

## OpenGL elimination gate

Strict source inventory intentionally still fails: 2,315 findings in 2,409
viewer-owned files (`gl_function` 651, `gl_shader` 1,597, `sdl_gl` 69).
Report SHA-256 is
`ea3e9358541641189dd3884534ee0b64a5157d6388a3daadb12472ceba4d7efe`.
No audit rule or suppression changed. This cut removes nine findings from the
immediately prior 2,324 checkpoint. The earlier 2,301 baseline is not directly
comparable because later semantic-bridge source expanded the conservative
textual shader inventory. The release gate remains zero; linked proof now
passes for the product executable, while runtime image/module proof remains
open pending owner-attended execution.

## Visual review cadence

Codex does not autonomously launch, drive or capture either viewer. Existing
font, exposure/mirror and Snapshot observations plus new checks accumulate in
the [owner-attended queue](../work/active/visual-review-queue.md). No queued row
is a parity pass without explicit owner evidence.

## Milestone state

| Area | State | Current fact |
| --- | --- | --- |
| M-003 platform/presentation | doing | Darwin creates a plain SDL/Cocoa window and sole opaque Metal surface; owner-attended startup/login validation remains. |
| M-004 resources/readback | doing | Ordinary geometry, textures/fonts and async uploads are native; Snapshot GL scratch is gone. |
| M-005 pipelines/bindings | doing | MSL pipelines execute; native pass identities no longer require GLSL/program names. |
| M-006 UI/media | doing | UI/font capture is native; owner accepted readable text previously; Snapshot UI remains visually open. |
| M-007 world/material | doing | Normal world/material/avatar/shadow/sky/water layers have direct Metal owners; parity remains open. |
| M-008 effects/advanced views | doing | Core effects and many diagnostics are native; remaining optional paths follow the usable-core cut. |
| M-009 parity/stability/no-GL | doing | Linked executable proof passes; strict source, runtime, visual and stability gates remain open. |
| M-010 measured performance | deferred | No quality-equivalent paired M1 Max claim yet. |
| M-011 GL authority removal | doing | Visible/hidden startup, utilities and product linkage require no GL context/library; reachable source tail remains. |
| M-012 Computer Use bridge | complete | Semantic bridge verified; visual driving is owner-attended. |
| M-013 RLVa strict parity | deferred | Nonessential ordering only; final no-GL behavior is still required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Core capability, initialization, state,
binding and pass configuration now remain valid without a GL context, while
required rendering behavior retains native owners instead of being deleted.

**Still incomplete:** the reachable source tail remains, the strict no-GL gate
fails, and no-context startup/login, runtime module proof, full visual parity,
prolonged stability, performance and independent review remain open. This
checkpoint still lacks independent build/review evidence and is not product
completion or release readiness.

## Exact next actions

1. Audit the essential startup/login and ordinary-frame call graph for GL
   source now exposed by the absent context; replace each owner with Metal.
2. Prioritize normal UI/text, world/avatar/material, streaming, presentation
   and Snapshot/readback paths before Debug/Develop/RLVa-only tails.
3. Rerun strict source, Mach-O link and runtime gates after every coherent cut.
4. Keep the no-context startup/login check queued; do not launch either viewer
   autonomously.
