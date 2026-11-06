# Current engineering state

**Observed:** 2026-11-06 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `cafe91682c`
**Historical evidence:** [archive index](archive/INDEX.md)

## Active implementation checkpoint

The owner-directed essential-first OpenGL cutover makes startup, normal UI/text,
world/avatar/material rendering, streaming, presentation and readback usable
without GL before Debug/Develop/RLVa-only paths. This changes order, not scope:
required features must be replaced rather than deleted.

Metal is the sole post-startup presenter. Production geometry, textures/fonts
and async uploads are Metal-authoritative with no parallel GL names; Snapshot
has no GL scratch/reconcile path. Direct Metal owners cover normal world,
material, avatar, shadow, sky, water, atmosphere, selection and converted
effects. Visual questions remain queued without blocking proven cutovers.

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
- `dd41713fea` replaces `ALUniformBuffer` GL allocation, mapping, upload and
  binding with Metal storage and timeline-safe stream/scratch rings while
  retaining matrix/light/environment/deferred/GLTF producers.
- `cafe91682c` replaces GL sampler objects/enums with stable logical identities
  and backend-neutral descriptions consumed directly by Metal encoders.

`LLGLSLShader::isComplete()` remains strictly a linked-GL-program predicate;
native availability cannot disguise a native identity as a GL object. GL-backed
occlusion remains fail-closed until a native query owner replaces it.

The viewer, renderer tests and appearance utility have no OpenGL/Vulkan link or
unresolved GL-family import; SDL's unselected generic stubs do not enter the
product. No viewer was launched, so startup/login remains queued. The next
boundary is the essential ordinary-frame tail, not optional paths.

## Build and validation evidence

- `tools/metal/doctor.sh` passes on macOS 26.5.2, MacBookPro18,2, M1 Max,
  64 GiB unified memory and Xcode 26.6; Metal reports Apple family 7 and a
  62,277,025,792-byte recommended working set.
- Source-exact Ninja warnings-as-errors build/stage passes at `cafe91682c`;
  package `Alchemy Metal Development 26.4.0.68454`.
- `tools/metal/verify_app.sh` passes 349/349 inspected Mach-O slices as ARM64;
  the product, renderer-test and appearance-utility executables have no
  OpenGL/Vulkan dependency or unresolved GL-family symbol.
- Viewer executable SHA-256:
  `c6cb92f0d09e62742973603d863630fb590f49b42f05242d7b4ef8daf992bfff`.
- Packaged `almetal_builtin.metallib` SHA-256:
  `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- `llrender_tests`: 1,721 assertions pass across 217 cases, with the expected
  unavailable-font skip.
- Focused native Metal/render CTest registration passes 16/16, including the
  GPU- and uniform-buffer cases; 517 tests remain registered after omitting
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

Strict source inventory intentionally still fails: 2,151 findings in 2,409
viewer-owned files (`gl_function` 556, `gl_shader` 1,597, `sdl_gl` 0).
Report SHA-256 is
`6fefba7089f2da35d016dafaa87ce1859ed647735dd3a89214e7ce40b704fc8c`.
No audit rule or suppression changed; native sampler semantics remove 11
findings from the immediately prior 2,162 checkpoint and the inventory is 150
below the owner's earlier 2,301 reference. The release gate remains zero;
linked proof passes while runtime image/module proof remains open pending
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
| M-004 resources/readback | doing | Ordinary geometry, textures/fonts, async uploads, GPU/uniform buffers and sampler descriptions are native; worker contexts and Snapshot GL scratch are gone. |
| M-005 pipelines/bindings | doing | MSL pipelines execute; native pass and sampler identities no longer require GL objects or GLSL/program names. |
| M-006 UI/media | doing | UI/font capture is native; owner accepted readable text previously; Snapshot UI remains visually open. |
| M-007 world/material | doing | Normal world/material/avatar/shadow/sky/water layers have direct Metal owners; parity remains open. |
| M-008 effects/advanced views | doing | Core effects and many diagnostics are native; remaining optional paths follow the usable-core cut. |
| M-009 parity/stability/no-GL | doing | Linked executable proof passes; strict source, runtime, visual and stability gates remain open. |
| M-010 measured performance | deferred | No quality-equivalent paired M1 Max claim yet. |
| M-011 GL authority removal | doing | Startup, windows, utilities, workers, linkage, frame completion, GPU/uniform buffers and samplers require no GL context/library; texture-slot and render tails remain. |
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

1. Migrate ordinary texture-slot resource identities and their remaining
   pipeline-specific consumers onto native Metal state.
2. Continue normal UI/text, world/avatar/material, streaming, presentation and
   Snapshot/readback before Debug/Develop/RLVa-only tails.
3. Rerun strict source, Mach-O link and runtime gates after every coherent cut.
4. Keep no-context startup/login queued; do not launch either viewer autonomously.
