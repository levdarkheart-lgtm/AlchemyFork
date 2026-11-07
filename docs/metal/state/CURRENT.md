# Current engineering state

**Observed:** 2026-11-07 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`
**Renderer code checkpoint:** `921aed5ca9` (`Fix Ultra Metal mirror shadow resolution`).
**Historical evidence:** [archive index](archive/INDEX.md)

## Active implementation checkpoint

The owner-directed essential-first OpenGL cutover makes startup, normal UI/text, world/avatar/material rendering, streaming, presentation and readback usable without GL before Debug/Develop/RLVa-only paths. This changes order, not scope: required features must be replaced rather than deleted.

Metal is the sole post-startup presenter. Production geometry, textures/fonts and async uploads are Metal-authoritative with no parallel GL names; Snapshot has no GL scratch/reconcile path. Direct Metal owners cover normal world, material, avatar, shadow, sky, water, atmosphere, selection and converted effects. Visual questions remain queued without blocking proven cutovers.

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
- `6e5047e50b` removes retained UI/text geometry's raw texture/sampler names and GL replay fallback; `b971465ca7` deletes superseded retained GL glow/SMAA; `f77024a20a` makes lens-dirt generation Metal-authoritative; `1bfe631627` removes `LLRender`'s dormant GL driver-state, clear, draw and compute authority while retaining semantic state for Metal encoders. Unsupported instanced/indirect/compute entry points fail closed pending native owners.

`LLGLSLShader::isComplete()` remains a linked-GL-program predicate; native identities do not masquerade as GL. Snapshot source state is repaired; execution remains owner-attended. The source-exact Release artifact is now a bounded owner-attended startup/login/world-load smoke-test candidate, not a parity or GL-zero claim.

The viewer, renderer tests and appearance utility have no OpenGL/Vulkan link or unresolved GL-family import; SDL's unselected stubs do not enter the product. Query culling remains a native-performance replacement task, not a parity claim.

### Attended-runtime correction committed as `2e2ac6e1c9`

Package `26.4.0.68493` was tested after tracing its Mid black screen against the original OpenGL renderer. The original path draws one tessellated wave patch plus three surrounding flat-water groups; Metal correctly published four groups, but world capture accepted only three and discarded every affected post-deferred world/UI frame.
Commit `2e2ac6e1c9` gives `ALMetalWaterSurface` one shared `expectedFieldDrawCount()` invariant (`4` with waves, otherwise `3`) for capture and renderer validation, plus a focused GPU assertion.
An attended login directly into persisted Mid (`RenderQualityPerformance=2`) rendered continuously for 500+ complete native world/UI publications. `first_incomplete_line=8073` and Metal validation/GPU errors did not recur; this proves the bounded Mid correction, not broad parity, High/Ultra stability or release readiness.

### Resource and UI checkpoints through `dbe1215cea`
Archived checkpoints cover [mirror resources](archive/CURRENT-mirror-resource-lifecycle-checkpoint-2026-11-07.md) and [UI clips/Conversations](archive/CURRENT-empty-ui-clip-checkpoint-2026-11-07.md). The latest fix clears the hero publication count before the mirrors-off return, retires only native hero source/radiance/binding state when the resource plan drops mirrors, and gates every direct Metal consumer on the live mirror plan. In attended `68499`, enabled-at-login then toggle-off visibly worked and logged count/resource retirement; a separate mirrors-off-before-login run logged successful login and continuous world/UI publication and visibly loaded normally.

### Attended directional-shadow correction — 2026-11-07

Release `26.4.0.68502` initially retained the Shadows preference but produced no visible world shadows. Original-path tracing showed null indexed GLTF-material slots are fragmented batch gaps and streaming textures bind the default image; Metal had invalidated a whole cascade for either condition. Commit `af65f062a1` preserves sparse slots with inert material data, uses the default native texture for streaming legacy/PBR/tree/covered-avatar casters and keeps null avatar images fail-closed. Diagnostics show four populated `3472x1920` cascades, moving caster shadows and no repeated main-cascade invalidation; one separate hero-probe terrain attempt recovered on retry. Release/349-slice verification and 52,774 assertions/57 cases pass.
An exploratory `Preset123`/Midday/128 m/Mid/Sun-Moon+Projector/ACES 1.0/no-sharpening `3456x2168` pair inadvertently used `/Applications/Alchemy Beta.app` (`7.1.9.2516`, executable SHA-256 `a361437f58866d9f41b029b21048cebc6dee635ee8678e41d40dfa5a20daa75a`) rather than the pinned QA reference. It suggests Metal is darker with softer/lower-contrast shadows, but is not acceptance evidence. The exact-base OpenGL reference has now been rebuilt and fully staged at `/tmp/alchemy-reference-1eb44bb/build-Darwin-ninja-os-arm64/newview/Release/Alchemy Metal Development.app`; its new executable SHA-256 is `de8fc8f817c0afb2063f641e9c24cbd58054c32c3053f4d0e863232498bb1797`. The prior `fac5225435df2c096d10b856f6e0f186b37e41446576398ee8f4842b884ef28d` executable survives only in preserved incomplete bundle `/tmp/alchemy-reference-67607-incomplete-backup.app`. The owner then captured the exact `67607` reference and Xcode Release `68502` Metal candidate at matching `3456x1916`; image SHA-256 values are `c73ed75207b9aa115cd193a9f5280fdbaf05fb8e1b44406c0316c7b4d6247b4c` and `39a08453dfe77ca732c9bb6440d05a097d8a153397aac84dc7a19912d3e792e1`. Settings screenshots preceding each save establish exposure `1.0`; the owner confirms matched Mid, `Preset123`, Midday/Lightbox, 128 m, Sun/Moon + Projector, mirrors 512/every third frame, ACES and zero sharpening. These are bounded owner-attended captures, not a formal parity pass: their run manifests predate the corrected QA profile and the candidate LEAP probe was blocked by an obsolete unsupported diagnostic key.

### High/Ultra quality corrections — 2026-11-07

The owner-observed Mid-to-High freeze was a repeated failed-frame loop, not a deadlock. Exact original tracing shows High enables `RenderDeferredSSAO` and the OpenGL noise authority is `GL_RGB16F`; Metal's source-correct CPU publication is therefore `rg16_float`, but `renderLightMap()` still required the superseded `rg32_float` GL-readback mirror. Commit `8a201381e2` aligns that consumer contract and its GPU test with the production `rg16_float` resource. In attended working-checkpoint package `68505`, live High plus a manual Mirrors re-enable continued current-frame world/UI publication past 1,300 frames with zero G-buffer incompatibility messages; High disabling Mirrors itself is original preset behavior. Ultra plus Mirrors exposed a separate repeated fail-closed loop: the light-map validator compared actual shadow depth to a stale planned resolution and required a positive PCSS scale even though the exact original cube-snapshot path takes its raw single tap before PCSS. Commit `921aed5ca9` derives resolution from the retained Metal texture and exempts only the probe-single-tap path from PCSS scale. Attended working-checkpoint `68507` then published continuous world/UI frames and six-face hero radiance with zero resolution-mismatch or light-map fallback messages; the owner-confirmed Midday scene, directional shadows and mirror all rendered normally with fixed exposure `1.0` and Auto Exposure off. The saved screenshot SHA-256 is `27fbe2ce02c58e05ed83dd5f0d0a7913d15c7dfd455a1575b3ee6292b6c662af`; because the run changed Mid to Ultra after launch, this is bounded attended evidence rather than a formal fail-closed gate artifact or broad Ultra parity pass.

## Build and validation evidence

- `tools/metal/doctor.sh` passes on macOS 26.5.2, MacBookPro18,2, M1 Max, 64 GiB unified memory and Xcode 26.6; Metal reports Apple family 7 and 62,277,025,792 bytes.
- Source-exact Xcode Release warnings-as-errors build/stage passes at `1bfe631627`; package `Alchemy Metal Development 26.4.0.68492`. The same checkpoint's Ninja RelWithDebInfo viewer and focused llrender test also rebuild and pass.
- `tools/metal/verify_app.sh` passes 349/349 inspected Mach-O slices as ARM64; the product, renderer-test and appearance-utility executables have no OpenGL/Vulkan dependency or unresolved GL-family symbol.
- Viewer executable SHA-256: `8c752d22881b2113ebf2be2b44ab09a9610e9e3e2e7066a516fea7ac005eabf4`.
- Packaged `almetal_builtin.metallib` SHA-256: `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- Focused no-context renderer proof passes 220 assertions/11 cases including native texture lifecycle/residency; query fail-closed proof passes 35 assertions; full llrender passes 1,856/1,856 and Metal passes 52,681/52,681 assertions.
- Native glow/SMAA pass 19,815 assertions/four GPU cases; native lens-dirt and display-post ownership pass 3,019 assertions/two cases; native font atlas upload/readback/release passes 28 assertions/five cases.
- Native transient-pool proof passes 943 assertions/14 cases; full alrender passes 556,617/556,617 assertions across 132 cases.
- `tools/metal/build.sh test` passes all 517 registered tests in 1,204.60 seconds; six Darwin GL-only migration suites remain intentionally omitted.
- The appearance utility compiles and links through the Metal-only hidden backend; params and joint-offset modes exit 0. Its texture fixture exits 5 before window creation on invalid J2C, so it is not backend evidence.
- Fresh shared renderer suite: 3,698 assertions pass across 111 cases.
- Focused shared GLSL suite: 1,353 assertions pass across 25 cases, with 13 hardware-dependent skips.
- The broad shared suite still reproduces pre-existing host/driver failures:
  four assertions in three frame-fence timing cases and one render-target
  depth-format assertion. The same failures occur in the earlier Release
  binary; changed files do not own those components.
- `python3 tools/metal/validate_harness.py` passes all 32 required files;
  `python3 -m unittest discover -s tools/metal/tests -v` passes 31/31.
- Focused alrender/llrender suites pass 28/28; `git diff --check` passes.
- Release `26.4.0.68493`, built from the mixed working checkpoint containing the now-committed correction at 2026-11-07 01:08:12 GMT, has executable SHA-256 `dfd02973a45085b790c334f1c47574549ce632b6b9e6f6a377946574f972ffd0`; `verify_app.sh` passes 349/349 ARM64 slices and attended Mid/HDR evidence is `/tmp/alchemy-metal-68493-water-wave-release-20261107.log`.
- At `dbe1215cea`, Xcode Release `68499` builds warnings-as-errors and verifies 349/349 ARM64 slices; the direct Metal suite passes 52,772 assertions across 57 cases. Release-tree `ctest` correctly reports no registered tests because that preset has `AL_BUILD_TESTS=OFF`; the separate Ninja test preset supplied the passing suite. Owner-attended mirror toggle-off and mirrors-off startup/login both pass; raw account/location logs remain local.
- Commit `af65f062a1`: Xcode Release `68502` builds and verifies 349/349 ARM64 slices; the direct Metal suite passes 52,774 assertions across 57 cases. Opt-in runtime diagnostics and owner evidence show four populated main-world directional cascades and stable moving avatar/object shadows.
- Commit `921aed5ca9`: source-exact Ninja Release `68508` builds, stages, signs and verifies 349/349 ARM64 slices; executable SHA-256 is `01dbd31503c99c2d3fb315c2584fe75d38c5f3478a35facc889c5b0fd82c6c5d`. The full Metal suite passes 52,777 assertions/57 cases, including the Ultra probe-single-tap/ordinary-Ultra validation split. Attended working-checkpoint `68507` run `/private/tmp/alchemy-metal-68507-ultra-resolution-fix-20261107-1` publishes hero radiance after six complete faces with no repeated depth-resolution mismatch or light-map fallback; `observed.json` SHA-256 is `0bfd6ef80f865c904168d32b57ffa647fb32ad73957ebcc42af7f14060c2dfad`.
- External OpenGL reference validation HEAD `734afd3179` is exact renderer base `1eb44bbd1b` plus the three instrumentation-only commits. `cmake -S indra --preset ninja-os-arm64 -DAL_CHANNEL:STRING="Alchemy Metal Development"` and `cmake --build build-Darwin-ninja-os-arm64 --config Release --parallel 10` both exit 0 after removing only the obsolete generated Ninja `-duplicate_symbols` option. The fully staged `26.4.0.67607` app has 8,111 files, includes `settings_files.xml`, passes deep strict signing and 349/349 ARM64 slice verification, and has executable SHA-256 `de8fc8f817c0afb2063f641e9c24cbd58054c32c3053f4d0e863232498bb1797`. A repeat build reports no work. The local fail-closed QA profile is corrected to Mid/exposure `1.0`/zero sharpening, Static+Dynamic reflections with coverage None, 64 probes, HDR/emissive, shadows 2 and mirrors 512/every third frame. The obsolete `ALMetalHeroMirrorSnapshotValidation` key is removed. Exact-path reference and candidate dry runs both exit 0 and assert the same 19 controls; graphics/Looks preset SHA-256 values are `8b5f56aaa26eed855ab4a671534d8a5beca8388b21d47a096789f87932eaef56` and `5108dd19f9c53aea37211b4cb08e84cce7b340cfc00c91a8a6b186b3bb58d4e6`. Fresh live gates remain required on new runs.

## OpenGL elimination gate

Strict source inventory intentionally still fails: 1,797 findings in 2,403 viewer-owned files (`gl_function` 340, `gl_shader` 1,457, `sdl_gl` 0).
Report SHA-256 is `219eb943960b124745e171a8d420e9ab983e74c04de50ad52f19a2ce47fee74d`.
No audit rule or suppression changed; this is 331 below 2,128 and 504 below the
owner's 2,301 reference. The gate remains zero; runtime proof remains queued.

## Visual review cadence

Viewer launches/login/visual changes occur only at the owner's explicit request and control. Checks accumulate in the [owner-attended queue](../work/active/visual-review-queue.md); no row is a parity pass without explicit owner evidence.

## Milestone state

| Area | State | Current fact |
| --- | --- | --- |
| M-003 platform/presentation | doing | SDL/Cocoa window source has no GL setup/context/swap owner; Metal surface is sole presentation owner; attended Mid login/world load now passes, while broader lifecycle and stability validation remain. |
| M-004 resources/readback | doing | Native targets now reuse exact owned attachment sets under a byte bound; streamed images use a separate containment capacity while the original override/controller semantics remain. Remaining copy/readback consumers and multi-scene policy validation follow. |
| M-005 pipelines/bindings | doing | MSL pipelines execute; native pass, sampler and texture identities no longer require GL objects, placeholder textures or GLSL/program names. |
| M-006 UI/media | doing | UI/font and media CPU texture publication plus retained/immediate UI geometry are Metal-only. Original-compatible empty clips no-op before Metal encoding; Conversations visibly opens/closes/reopens in attended `68497` evidence. Broader UI parity remains open. |
| M-007 world/material | doing | Normal world/material/avatar/shadow/sky/water layers have direct Metal owners; Mid water, High SSAO and the Ultra directional-shadow constants now remain stable in bounded attended checks. Exact-build matched owner captures exist; broader parity remains open. |
| M-008 effects/advanced views | doing | Native mirror/probe capture publishes scene reflections with bounded target reuse. Toggle-off, mirrors-off login and an Ultra-plus-mirrors transition now pass bounded attended checks, including hero radiance publication; formal mirror parity remains open. |
| M-009 parity/stability/no-GL | doing | Linked executable proof passes; strict source, runtime, visual and stability gates remain open. |
| M-010 measured performance | deferred | No quality-equivalent paired M1 Max claim yet. |
| M-011 GL authority removal | doing | Startup, windows, utilities, workers, linkage, frame completion, resources, targets, UI replay and retained glow/SMAA require no GL; render tails remain. |
| M-012 Computer Use bridge | complete | Semantic bridge verified; visual driving is owner-attended. |
| M-013 RLVa strict parity | deferred | Nonessential ordering only; final no-GL behavior is still required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Core execution, offscreen targets and writable image bindings remain valid without GL; required behavior was replaced.

**Still incomplete:** the reachable source tail remains, the strict no-GL gate fails, and broader startup/lifecycle coverage, runtime module proof, full visual parity, prolonged stability and performance remain open; independent build/review evidence is also still required. This is not product completion or release readiness.

## Exact next actions

1. Run fresh fail-closed source-exact `68508` and exact-base `67607` Ultra captures from launch, with identical QA profile/location/camera and complete manifests.
2. Compare Ultra shadow, mirror reflection content, avatar and clipping parity independently of the now-passing publication lifecycle.
3. Convert the exploratory 20–21 FPS OpenGL versus about 17 FPS Metal observation into a quality-equivalent, provenance-complete benchmark only after parity is established.
4. Classify texture churn and run all four scenarios before changing automatic texture capacity.
5. Continue the highest-value unblocked native-ownership cut while attended visual questions remain queued.
