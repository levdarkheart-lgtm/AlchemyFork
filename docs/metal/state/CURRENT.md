# Current engineering state

**Observed:** 2026-11-06 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `b971465ca7`
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
- `e8eaa3701b` replaces transient-target GL side effects/poison clears with explicit Metal attachment clears while preserving shared depth; `768250a2fc` makes Metal exposure/lens-flare history native-owned.
- `ecf223cf4c` removes scale-down/CEF GL tails; `a3067b1aac` makes noise/SMAA lifecycle native; `9c74277821` makes font atlas allocation, glyph publication and release Metal-only with no runtime GL fallback.
- `60e350e29a` removes orphan cube-array/framebuffer-copy GL owners; `c62977bd49` makes ordinary texture publication Metal-only; `8a00656289` removes uncalled
  GL ownership modes; `b86b1e4c53` repairs Snapshot state; `1081614942` removes legacy GL queries; `2e255de87c` makes ordinary texture residency, invalidation,
  cache identity, preview/save, terrain/map/avatar/bump and projector availability native.
- `6e5047e50b` removes retained UI/text geometry's raw texture/sampler names and GL replay fallback; `b971465ca7` deletes the superseded retained GL glow/SMAA implementations and unreachable CAS selector after focused native replacement proof.

`LLGLSLShader::isComplete()` remains a linked-GL-program predicate; native identities do not masquerade as GL. Snapshot source state is repaired; execution remains owner-attended. The source-exact Release artifact is now a bounded owner-attended startup/login/world-load smoke-test candidate, not a parity or GL-zero claim.

The viewer, renderer tests and appearance utility have no OpenGL/Vulkan link or unresolved GL-family import; SDL's unselected stubs do not enter the product. No viewer was launched; startup/login remains queued. Query culling remains a native-performance replacement task, not a parity claim.

## Build and validation evidence

- `tools/metal/doctor.sh` passes on macOS 26.5.2, MacBookPro18,2, M1 Max, 64 GiB
  unified memory and Xcode 26.6; Metal reports Apple family 7 and 62,277,025,792 bytes.
- Source-exact Xcode Release warnings-as-errors build/stage passes at `6e5047e50b`; package `Alchemy Metal Development 26.4.0.68486`. The current `b971465ca7` Ninja RelWithDebInfo viewer also rebuilds and stages cleanly.
- `tools/metal/verify_app.sh` passes 349/349 inspected Mach-O slices as ARM64;
  the product, renderer-test and appearance-utility executables have no
  OpenGL/Vulkan dependency or unresolved GL-family symbol.
- Viewer executable SHA-256: `81ce65c17b76feb59bd389d474fa96343cacdf853972972694dbdf41f58e1737`.
- Packaged `almetal_builtin.metallib` SHA-256: `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- Focused no-context renderer proof passes 220 assertions/11 cases including native texture lifecycle/residency; query fail-closed proof passes 35 assertions;
  full llrender passes 1,856/1,856 and Metal passes 52,681/52,681 assertions.
- Native glow extract/blur/combine and all SMAA stages pass 19,815 assertions/four focused GPU cases; native font atlas upload/readback/release passes 28 assertions/five cases.
- Native transient-pool proof passes 943 assertions/14 cases; full alrender passes 556,617/556,617 assertions across 132 cases.
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
- Focused alrender/llrender suites pass 28/28; `git diff --check` passes.

## OpenGL elimination gate

Strict source inventory intentionally still fails: 1,867 findings in 2,403 viewer-owned files (`gl_function` 396, `gl_shader` 1,473, `sdl_gl` 0).
Report SHA-256 is `2fc73ab9a0455634364b27d19ed74f67f50e47693f06c0cce013ae79c14b89b9`.
No audit rule or suppression changed; this is 261 below 2,128 and 434 below the
owner's 2,301 reference. The gate remains zero; runtime proof remains queued.

## Visual review cadence

Codex does not autonomously launch, drive or capture either viewer. Runtime
proof, visual evaluation and stability/profiling are reserved for the owner-led
phase after engineering items 1-3. Existing observations and new checks accumulate in
the [owner-attended queue](../work/active/visual-review-queue.md). No queued row
is a parity pass without explicit owner evidence.

## Milestone state

| Area | State | Current fact |
| --- | --- | --- |
| M-003 platform/presentation | doing | SDL/Cocoa window source has no GL setup/context/swap owner; Metal surface is sole presentation owner; attended login validation remains. |
| M-004 resources/readback | doing | Geometry, ordinary texture publication/residency/lifecycle, fonts, buffers, samplers, image slots, targets and reflection arrays are native; remaining copy/readback consumers follow. |
| M-005 pipelines/bindings | doing | MSL pipelines execute; native pass, sampler and texture identities no longer require GL objects, placeholder textures or GLSL/program names. |
| M-006 UI/media | doing | UI/font and media CPU texture publication and retained/immediate UI geometry are Metal-only; Snapshot state/thumbnail/save source path is repaired but runtime behavior remains owner-attended. |
| M-007 world/material | doing | Normal world/material/avatar/shadow/sky/water layers have direct Metal owners; parity remains open. |
| M-008 effects/advanced views | doing | Native display owns glow and SMAA with focused GPU proof; remaining optional paths follow the usable-core cut. |
| M-009 parity/stability/no-GL | doing | Linked executable proof passes; strict source, runtime, visual and stability gates remain open. |
| M-010 measured performance | deferred | No quality-equivalent paired M1 Max claim yet. |
| M-011 GL authority removal | doing | Startup, windows, utilities, workers, linkage, frame completion, resources, targets, UI replay and retained glow/SMAA require no GL; render tails remain. |
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

1. Run the bounded owner-attended startup/login/world-load smoke test against package 68486; record the first failure without doing a reference comparison.
2. Finish remaining essential texture copy/readback ownership and raw manual identities.
3. Treat Snapshot source repair as ready for later owner-attended execution proof.
4. Continue ordinary UI/world GL removal before optional tails and rerun strict source/link gates.
