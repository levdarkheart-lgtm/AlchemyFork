# Current engineering state

**Observed:** 2026-11-06 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `029694fedb`
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
- `6ec5884b85` publishes actual Metal texture identities through ordinary slots,
  makes native cube maps bindable, and removes the white-GL-texture detour from
  native-only image sampling while retaining dimension and sampler semantics.
- `e607b19e27` removes the slot's remaining driver texture/sampler/unit calls;
  unresolved integer render-target names fail closed pending native attachments.
- `07e4d7a626` replaces `LLRenderTarget` GL ownership with typed Metal owned and
  borrowed color/depth, shared depth, mip, clear, copy, resample, swap and
  sampling behavior. Default-window copies fail closed pending the retained
  Metal presentation texture; no GL driver call remains in this owner.
- `029694fedb` gives image slots typed Metal read/write attachments, validates subresources/access and rejects raw integer identities. It removes the dormant
  GL depth-pyramid owner/GLSL while retaining native water hierarchy/cull compute.

`LLGLSLShader::isComplete()` remains a linked-GL-program predicate; native availability cannot disguise a native identity as GL; GL occlusion fails closed.

The viewer, renderer tests and appearance utility have no OpenGL/Vulkan link or unresolved GL-family import; SDL's unselected generic stubs do not enter the
product. No viewer was launched; startup/login remains queued while work continues on the essential ordinary-frame tail, not optional paths.

## Build and validation evidence

- `tools/metal/doctor.sh` passes on macOS 26.5.2, MacBookPro18,2, M1 Max,
  64 GiB unified memory and Xcode 26.6; Metal reports Apple family 7 and a
  62,277,025,792-byte recommended working set.
- Source-exact Ninja warnings-as-errors build/stage passes at `029694fedb`;
  package `Alchemy Metal Development 26.4.0.68461`.
- `tools/metal/verify_app.sh` passes 349/349 inspected Mach-O slices as ARM64;
  the product, renderer-test and appearance-utility executables have no
  OpenGL/Vulkan dependency or unresolved GL-family symbol.
- Viewer executable SHA-256:
  `e986d61f0031269505fe9b5f3b31d1ca2a8bc384942bfe9545800ef204bf21f9`.
- Packaged `almetal_builtin.metallib` SHA-256:
  `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- Focused no-context renderer proof passes 156 assertions/eight cases including writable native image subresources; full llrender passes 1,792/1,792 and Metal
  passes 52,681/52,681 assertions.
- `tools/metal/build.sh test` passes all 517 registered tests in 1,204.60 seconds;
  six Darwin GL-only migration suites remain intentionally omitted.
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

Strict source inventory intentionally still fails: 2,033 findings in 2,407
viewer-owned files (`gl_function` 470, `gl_shader` 1,565, `sdl_gl` 0).
Report SHA-256 is
`783d855a26bc6e818dcf776b34b44e29982dc4ad39b6fbfb4a37ebe7cc4653ad`.
No audit rule or suppression changed; this is 95 below 2,128 and 268 below the
owner's 2,301 reference. The gate remains zero; runtime proof remains queued.

## Visual review cadence

Codex does not autonomously launch, drive or capture either viewer. Existing
font, exposure/mirror and Snapshot observations plus new checks accumulate in
the [owner-attended queue](../work/active/visual-review-queue.md). No queued row
is a parity pass without explicit owner evidence.

## Milestone state

| Area | State | Current fact |
| --- | --- | --- |
| M-003 platform/presentation | doing | SDL/Cocoa window source has no GL setup/context/swap owner; Metal surface is sole presentation owner; attended login validation remains. |
| M-004 resources/readback | doing | Ordinary geometry, textures/fonts, async uploads, GPU/uniform buffers, samplers, texture/image slots and legacy offscreen targets are native; default-window consumers remain. |
| M-005 pipelines/bindings | doing | MSL pipelines execute; native pass, sampler and texture identities no longer require GL objects, placeholder textures or GLSL/program names. |
| M-006 UI/media | doing | UI/font capture is native; owner accepted readable text previously; Snapshot UI remains visually open. |
| M-007 world/material | doing | Normal world/material/avatar/shadow/sky/water layers have direct Metal owners; parity remains open. |
| M-008 effects/advanced views | doing | Core effects and many diagnostics are native; remaining optional paths follow the usable-core cut. |
| M-009 parity/stability/no-GL | doing | Linked executable proof passes; strict source, runtime, visual and stability gates remain open. |
| M-010 measured performance | deferred | No quality-equivalent paired M1 Max claim yet. |
| M-011 GL authority removal | doing | Startup, windows, utilities, workers, linkage, frame completion, buffers, samplers, texture/image slots and offscreen target ownership require no GL; render tails remain. |
| M-012 Computer Use bridge | complete | Semantic bridge verified; visual driving is owner-attended. |
| M-013 RLVa strict parity | deferred | Nonessential ordering only; final no-GL behavior is still required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Core execution, offscreen targets and
writable image bindings remain valid without GL; required behavior was replaced.

**Still incomplete:** the reachable source tail remains, the strict no-GL gate
fails, and no-context startup/login, runtime module proof, full visual parity,
prolonged stability, performance and independent review remain open. This
checkpoint still lacks independent build/review evidence and is not product
completion or release readiness.

## Exact next actions

1. Move default-window copies and transient-target clears to retained Metal textures.
2. Convert remaining essential presentation/readback and Snapshot consumers.
3. Continue normal rendering before Debug/Develop/RLVa-only tails.
4. Rerun strict source, link and runtime gates after every coherent cut.
5. Keep startup/login queued; do not launch either viewer autonomously.
