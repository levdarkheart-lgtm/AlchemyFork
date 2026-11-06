# Current engineering state

**Observed:** 2026-11-06 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `8bd5ff1bfd`

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
- `1a76b76a6d` removes the hidden backend's remaining cross-platform GL
  implementation and the texture/vertex workers' shared-context fallback.
  Async uploads retain Metal batching and fail closed without authoritative
  Metal textures. Darwin no longer builds six legacy GL-only test executables.
- `f7ecf74e84` removes visible-window GL lifecycle; SDL/Cocoa now owns only its Metal surface, Darwin procedure lookup fails closed and `sdl_gl` is zero.
- `c865c15220` replaces ordinary-frame `GLsync` creation, polling, waiting and
  deletion with an ordered Metal command-submission timeline. Frame pacing,
  safe resource reuse and teardown draining remain explicit native behavior.
- `8bd5ff1bfd` replaces `ALGPUBuffer` GL storage/transfers with private/shared
  `MTLBuffer` and ordered blits; stream rings fail closed on open-frame reuse.

`LLGLSLShader::isComplete()` remains strictly a linked-GL-program predicate;
native availability cannot disguise a native identity as a GL object. GL-backed
occlusion remains fail-closed until a native query owner replaces it.

The Release viewer, renderer tests and appearance utility have no OpenGL/Vulkan
dependency or unresolved GL-family import. SDL's static archive can retain
unselected generic API stubs, but the product linker selects none. No viewer was launched under the
owner-attended visual policy, so startup/login behavior remains queued rather
than inferred. The next boundary is the reachable essential ordinary-frame
source tail, not optional Debug/Develop/RLVa paths.

## Build and validation evidence

- `tools/metal/doctor.sh` passes on macOS 26.5.2, MacBookPro18,2, M1 Max,
  64 GiB unified memory and Xcode 26.6; Metal reports Apple family 7 and a
  62,277,025,792-byte recommended working set.
- `tools/metal/build.sh` completed the canonical Xcode Release build with
  warnings-as-errors and ended `BUILD SUCCEEDED`.
- Source-exact Ninja staging build: `Alchemy Metal Development 26.4.0.68450`.
- `tools/metal/verify_app.sh` passes 349/349 inspected Mach-O slices as ARM64;
  the product, renderer-test and appearance-utility executables have no
  OpenGL/Vulkan dependency or unresolved GL-family symbol.
- Viewer executable SHA-256:
  `cf4f951ff214ef45d5949673c657d6ae4431d3feea0c667b944879d4d5695bae`.
- Packaged `almetal_builtin.metallib` SHA-256:
  `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- `llrender_tests`: 1,655 assertions pass across 213 cases, with the expected
  unavailable-font skip.
- Focused native Metal/render CTest registration passes 14/14, including the
  scheduler timeline assertions; 515 tests remain registered after omitting
  six Darwin GL-only migration suites.
- The appearance utility compiles and links through the Metal-only hidden
  backend; params and joint-offset modes exit 0. Its texture fixture exits 5
  before window creation on invalid J2C, so it is not backend evidence.
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

Strict source inventory intentionally still fails: 2,194 findings in 2,409
viewer-owned files (`gl_function` 599, `gl_shader` 1,597, `sdl_gl` 0).
Report SHA-256 is
`dd8b7b0fc8e71b1f7a0c048da244ddd608b12dd0a92317eea767fc3c90fea744`.
No audit rule or suppression changed; native GPU-buffer storage removes 39
findings from the immediately prior 2,233 checkpoint. The release gate remains
zero; linked proof passes while runtime image/module proof remains open pending
owner-attended execution.

## Visual review cadence

Codex does not autonomously launch, drive or capture either viewer. Existing
font, exposure/mirror and Snapshot observations plus new checks accumulate in
the [owner-attended queue](../work/active/visual-review-queue.md). No queued row
is a parity pass without explicit owner evidence.

## Milestone state

| Area | State | Current fact |
| --- | --- | --- |
| M-003 platform/presentation | doing | SDL/Cocoa window source has no GL setup/context/swap owner; Metal surface is sole presentation owner; attended login validation remains. |
| M-004 resources/readback | doing | Ordinary geometry, textures/fonts, async uploads and general GPU-buffer storage/transfers are native; worker contexts and Snapshot GL scratch are gone. |
| M-005 pipelines/bindings | doing | MSL pipelines execute; native pass identities no longer require GLSL/program names. |
| M-006 UI/media | doing | UI/font capture is native; owner accepted readable text previously; Snapshot UI remains visually open. |
| M-007 world/material | doing | Normal world/material/avatar/shadow/sky/water layers have direct Metal owners; parity remains open. |
| M-008 effects/advanced views | doing | Core effects and many diagnostics are native; remaining optional paths follow the usable-core cut. |
| M-009 parity/stability/no-GL | doing | Linked executable proof passes; strict source, runtime, visual and stability gates remain open. |
| M-010 measured performance | deferred | No quality-equivalent paired M1 Max claim yet. |
| M-011 GL authority removal | doing | Startup, windows, utilities, workers, linkage, frame completion and GPU-buffer stores require no GL context/library; uniform/binding and render tails remain. |
| M-012 Computer Use bridge | complete | Semantic bridge verified; visual driving is owner-attended. |
| M-013 RLVa strict parity | deferred | Nonessential ordering only; final no-GL behavior is still required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Core setup, state, pass configuration,
frame completion and GPU-buffer transfers remain valid without a GL context; required
rendering behavior retains native owners instead of being deleted.

**Still incomplete:** the reachable source tail remains, the strict no-GL gate
fails, and no-context startup/login, runtime module proof, full visual parity,
prolonged stability, performance and independent review remain open. This
checkpoint still lacks independent build/review evidence and is not product
completion or release readiness.

## Exact next actions

1. Migrate uniform-buffer rings and their ordinary shader binding consumers onto the native `ALGPUBuffer` store.
2. Continue normal UI/text, world/avatar/material, streaming, presentation and
   Snapshot/readback before Debug/Develop/RLVa-only tails.
3. Rerun strict source, Mach-O link and runtime gates after every coherent cut.
4. Keep no-context startup/login queued; do not launch either viewer autonomously.
