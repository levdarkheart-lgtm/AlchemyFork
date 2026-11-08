# Active execution plan — full native Alchemy Metal conversion

**Owner:** Codex integrator. **Outcome:** complete `docs/metal/PRODUCT.md`, not a demo.
**Evidence contract:** `docs/metal/VALIDATION.md` and `docs/metal/PERFORMANCE.md`. **Continuation:** [next-session handover](../../state/NEXT_SESSION.md). **History:** [archive index](../../state/archive/INDEX.md).
**Renderer checkpoint:** Commit `177c443cfd` (`Restore binding cache on measured renderer`), measured renderer `8f4d5654e5` plus only the encoder-local binding cache; benchmark evidence gate `1e962c84e2`; alpha-2 remains at `26.4.0.68512-alpha2`.
This ledger contains current decisions and executable work. Prior checkpoint detail belongs in the indexed archive.

## Work board

| ID | Work | State | Evidence / next boundary |
| --- | --- | --- | --- |
| M-001 | Reproducible ARM64 OSS build/reference baseline | doing | Exact-base OpenGL `67607` and diagnostics-free Metal Release both build, stage, sign and verify 349/349 ARM64 slices; manual alpha-2 CI also clean-builds, verifies and publishes the ARM64 archive. Controlled visual/performance baselines remain open. |
| M-002 | GL/shader/pass/resource and binary inventory | doing | Strict inventory is 1,797 and exits 1; `sdl_gl` is zero and product link audit is clean; runtime audit remains owner-led. |
| M-003 | Native drawable, presentation and lifecycle | doing | SDL/Cocoa window source has no GL setup/context/swap owner; Metal surface is sole presentation owner; attended Mid login/world load passes, while broader lifecycle/stability coverage remains. |
| M-004 | Metal resources, transfers, readback and sync | doing | Vertex/index generations are completion-retired; retained G-buffers are borrowed and sequential haze shares one depth snapshot. Owner-directed comparison checkpoint temporarily restores pre-separation texture-pressure accounting; broader cache trimming remains. |
| M-005 | MSL and explicit pipeline/resource binding | doing | Material/PBR base ladders bind 8 slots; the exact encoder-local native-handle cache again skips unchanged texture/sampler calls while preserving linear/sRGB views. Palettes/constants and wider state caching remain. |
| M-006 | UI, fonts, HUD and media ingress | doing | Reusable native world/UI output and copied depth eliminate per-frame target recreation. Empty UI clips now preserve original no-op semantics; Conversations visibly opens/closes/reopens in attended 68497 evidence. |
| M-007 | World scene and material correctness | doing | Core world, terrain, materials, avatars, shadows, sky, water and atmosphere have direct Metal owners. Ordinary point lights preserve the bounded split and direct slices avoid shadow copies; the comparison checkpoint restores pre-sharing probe projector work. Attended light/shadow smoke remains open. |
| M-008 | Advanced views/effects and compute | doing | Native mirror/probe lifecycle and an Ultra-plus-mirrors transition pass, but the matched pair shows a blue-gray Metal versus green-terrain OpenGL reflected background; trace content/clipping before any fix. |
| M-009 | Feature parity, stability and native-only cleanup | doing | Essential no-context/no-link viewer precedes optional-path cleanup; strict, visual and stability gates remain open. |
| M-010 | M1 Max profiling and optimization | doing | Owner-directed `68543` is measured `68532` renderer source plus only the binding cache after `68537`/`68539` remained 39.9%/30.4% below its traced rate. Fresh same-scene/camera evidence and formal paired proof remain open. |
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

- `9e90be44e1`: renderer capability truth comes from the Metal device rather than `gGLManager`.
- `70eca3a0c9`: native renderer initialization is separate from GL init.
- `e3ec32d3b2`: render-state transitions remain semantic without a context.
- `349c0278c3`: texture/shader binding bookkeeping is contextless.
- `3087308155`: loader-authored pass and rigged/classic/mirror variant graphs configure as native identities without opening/compiling/caching GLSL or creating program/shader names. MSL/Metal owners remain executable authority.

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

`1a76b76a6d` deletes the hidden backend's remaining cross-platform GL context implementation and both texture/vertex worker shared-context fallbacks. The texture worker retains asynchronous Metal upload batching and fail-closed authority checks. Darwin omits six legacy GL-only migration suites rather than linking a test-only fallback runtime.

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

`e90ccdf18f` repairs the persistent geometry-buffer path after tracing exact original `1eb44bbd1b`: native initialization no longer depends on deliberately zero GL names, uploads remain independently dirty, and later writes rotate reusable frame-retired Metal generations while resize/destruction retains captured wrappers through completion.

`8247d09c5d` repairs ordinary deferred point-light coverage after exact-original tracing: camera-outside lights use conservative projected cube scissors and one-light draw ranges, while camera-inside lights retain full-target batches capped at 16. `f93c9f1927` renders directional/projector raw-depth fragments directly into retained R32 slices. `8f4d5654e5` then borrows compatible retained G-buffers, shares sequential haze depth, removes overwritten material/PBR bindings and inlines common skin palettes; visual equations remain unchanged. `7cd1993d8e` pairs tracked Metal texture bytes with the bounded texture target and total device allocation with the full recommendation while preserving exact-original GL accounting.

`dd41713fea` deletes `ALUniformBuffer`'s GL allocation, mapping, upload, binding and scratch calls. Private/shared Metal stores publish native buffer, aligned offset, size and serial. Timeline-safe rings grow, spill or fail closed; existing producers remain and historical update modes select one native path.

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
The owner-attended exploratory pair used `/Applications/Alchemy Beta.app` (`7.1.9.2516`, executable SHA-256 `a361437f58866d9f41b029b21048cebc6dee635ee8678e41d40dfa5a20daa75a`) rather than the QA reference, so its darker/softer Metal result is not an acceptance decision. External validation HEAD `734afd3179` (exact renderer base `1eb44bbd1b` plus three instrumentation-only commits) now produces a complete, deeply signed, 349/349 ARM64 `67607` reference with executable SHA-256 `de8fc8f817c0afb2063f641e9c24cbd58054c32c3053f4d0e863232498bb1797`. The owner subsequently captured that exact reference and Xcode Release `68502` candidate at `3456x1916`; image SHA-256 values are `c73ed75207b9aa115cd193a9f5280fdbaf05fb8e1b44406c0316c7b4d6247b4c` and `39a08453dfe77ca732c9bb6440d05a097d8a153397aac84dc7a19912d3e792e1`. Pre-save screenshots and owner confirmation establish matched exposure `1.0`, Mid, `Preset123`, Midday/Lightbox, 128 m, Sun/Moon + Projector, mirrors 512/every third frame, ACES and zero sharpening. The old run manifests prevent a formal gate decision: the local QA profile is now corrected, the unsupported diagnostic key removed and both exact-path dry runs pass with identical expectations, but fresh live-gated runs remain required.

### High/Ultra quality corrections — 2026-11-07

Original `1eb44bbd1b` tracing shows High enables SSAO and supplies tiled noise as `GL_RGB16F`; commit `8a201381e2` therefore makes the light-map consumer and GPU fixture accept the producer's source-correct `rg16_float` resource. Owner-attended `68505` advanced past 1,300 High-plus-manually-restored-mirror publications with zero G-buffer incompatibility messages; High setting `RenderMirrors=0` is original preset behavior. Ultra plus Mirrors exposed a separate repeated fail-closed loop: Metal compared actual shadow depth to a stale planned resolution and demanded positive PCSS scale although the original cube-snapshot path takes a raw single tap before PCSS. Commit `921aed5ca9` reads resolution from the retained Metal texture and exempts only probe-single-tap from that scale. Source-exact `68508` and exact-base OpenGL `67607` owner captures at `3456x2168` have image SHA-256 values `30e90493ffea570c74f71c8f2e1a26c5044731b398151dee678ffc9dc1c5ef2c` and `082c610acb4d00963dc4a450a6abf5f7533b6bdd8aa75609b2e85a99bb72997f`; fresh LEAP samples match all 17 compared Ultra controls and exact world position. The owner accepts broad lighting, directional shadows, sky and color as similar enough to move on; mirror reflected-background content remains open. Both runs changed Mid to Ultra after launch, so this is bounded evidence rather than a formal gate artifact. Instantaneous 5 FPS Metal versus 7 FPS OpenGL is exploratory only.

Bounded source-exact `68508` profiling records 4.9 versus 8.1 FPS and 66.60 versus 28.19 command buffers/present with Mirrors on versus off, 40.4% enabled GPU activity and Nominal thermals. Exact original `1eb44bbd1b` tracing confirms all six hero faces remain required at update rate 1 and orders each face's shadow work before its draw. Release `68511` folds face shadow work into the hero transaction for median 6.8 versus 5.4 FPS and 30.2% fewer command buffers/present. Original tracing next showed camera-outside projectors rasterize a bounded volume; commit `3bd0451efc` applies its conservative screen projection as a Metal scissor with full-target eye-plane fallback. Release `68512` passes 52,796 Metal assertions, signing and 349/349 ARM64 verification; all four presets publish continuously, main/hero projector GPU-channel time falls 78.9%/86.7%, total labeled work falls 28.9%, and Ultra remains median 7.1 FPS. Atmospheric haze/base light are now largest; the 24 FPS objective and formal gate remain open. The [archived diagnosis](../../state/archive/CURRENT-ultra-mirror-performance-diagnosis-2026-11-07.md) preserves provenance and limitations. Commit `50bb59dd6c` adds a manual-only Apple Silicon alpha path without changing ordinary Build triggers; tag run `42432438018` created `26.4.0.68512-alpha2`, and clean Build run `42432449254` / job `128584093350` passed build, package/unpack, checksum, deep signing and 349/349 ARM64 verification before preserving artifact `13599588258` and publishing the prerelease. The 214,268,948-byte release archive SHA-256 is `26c0ffd5d65a97c406215727323df25005d682e5e7a1158909feca85674e6d3e`; this is bounded distribution evidence, not completion of open product gates.
Original deferred-soften tracing next showed expensive geometry is depth-classified away from clear sky. Commit `c436b9fe6e` borrows the exact G-buffer depth in one Metal pass and splits cheap sky transfer; bounded main base-light work falls from 11.470 to 2.688 ms/present (-76.6%). Atmospheric specialization and fractional-power strength reduction regressed haze by 3.2%/3.9% and are reverted. An attended working-tree `68516` run falls from startup 22.8 to 9.1 FPS when hero/probe publication activates, settles at 8.2-8.9, and stays 8.3-8.7 after Midday; `1e962c84e2` therefore requires 30 elapsed seconds as well as 120 frames before measurement. Source-exact `68518` embeds that commit, passes 52,798 Metal assertions, 99,002 viewer assertions and 349/349 ARM64 verification, and has executable SHA-256 `764a0e8f5372e62357f7b67b354a0fbe1e50e53c25aed39c466caeb1a1fe3fde`. The 24 FPS objective and formal paired gate remain open; [archived evidence](../../state/archive/CURRENT-base-light-depth-classification-2026-11-08.md) preserves hashes and limitations.
Release `68521` verifies live Ultra anisotropy `4` and `UseOcclusion=1`. After full residency, 54 ten-second log samples have median/mean `6.35`/`6.322` FPS versus immediate predecessor `68515` at `5.9`/`5.833`, a bounded 7.63%/8.38% directionally positive comparison with camera/live-scene and sampling limitations. Time Profiler attributes only 0.4% of sampled main-thread time to query mechanics while shadow and probe generation dominate. A settled Metal trace records 5.310 traced presents/s, 38.74 submissions/present, 77.0% GPU-active union, Nominal thermals, and main opaque-post/haze/base-light channels at 55.307/17.544/17.267 ms per present. Because the prior base-light trace began about 32 seconds after launch and contained only 0.579 ms/present of opaque-post work, it was not fully resident and cannot establish haze as the next target. The [new archived evidence](../../state/archive/CURRENT-native-occlusion-anisotropy-performance-2026-11-08.md) corrects the priority to original-source tracing of opaque post-deferred; issued/ready/culled telemetry and the formal paired gate remain open.
Exact original `1eb44bbd1b` confirms bounded camera-outside point lights, direct final shadow targets and per-slot unchanged binding suppression; commits `8247d09c5d`, `f93c9f1927` and `f8af790f32` restore those contracts. Commit `8f4d5654e5` applies retained attachment borrowing, one sequential haze snapshot, exact 8-slot material ladders and 4 KiB inline palettes; `7cd1993d8e` separates streamed-image and global Metal pressure. Owner-confirmed same-scene/camera `68537` regressed from settled `68532` at 8.633 traced presents/s to 5.188 while projector GPU work fell and CPU binding/hero encoding rose. After rejecting a warm-up-contaminated trace, settled source-exact `68539` reaches 6.009 presents/s and 7.1 median log FPS: +15.8%/+18.3% over `68537`, with 23.4% lower sampled standard texture-binding time and 21.3% lower hero encoder time. It remains 30.4% below `68532`; hero CPU encoder time is still 277.7% higher despite similar labelled hero GPU interval count/time. The next bounded work is allocation/binding telemetry and remaining palette/constant/state setup, not reverting projector sharing. [Texture-pressure](../../state/archive/CURRENT-native-texture-pressure-accounting-checkpoint-2026-11-08.md) and [binding runtime](../../state/archive/CURRENT-68537-texture-binding-runtime-checkpoint-2026-11-08.md) evidence preserve hashes and limits.
## Validation checkpoint

- M1 Max host doctor passes (macOS 26.5.2, 64 GiB, Xcode 26.6, Apple family 7).
- Source-exact Xcode Release warnings-as-errors build/stage passes at `1bfe631627` as package `26.4.0.68492`; the same checkpoint's Ninja RelWithDebInfo rebuild/stage and focused llrender test pass.
- `tools/metal/verify_app.sh`: 349/349 inspected slices are ARM64.
- Executable SHA-256: `8c752d22881b2113ebf2be2b44ab09a9610e9e3e2e7066a516fea7ac005eabf4`.
- Metallib SHA-256: `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- Exact original `1eb44bbd1b` tracing (query-source SHA-256 `df2536af88d9edf75f68fbccfe082d4b536d531684f19176e177fc6e405dd534`) establishes deferred opaque issue order, conservative versus exact zero/nonzero modes, original cube fans and asynchronous fail-open lifetime. The Metal replacement inserts queries at retained batch offsets in the same G-buffer pass and attaches tickets only after commit. Query proof passes 49 assertions; full Release llrender passes 1,881 assertions across 224 cases (223 passed, one skipped), and Metal passes 52,815 assertions across 58 cases with real-device result and ordering coverage. Release `68519` builds/stages/signs and verifies 349/349 ARM64 slices; executable SHA-256 is `1f7d1f5ae827dbad23ae24e1cbc8413925ab03488b53fe8d21a44146cf1cb28b`, packaged metallib SHA-256 is `163168fe50cedaa022a87608f354655246eb110838ad6ba113172555882181a1`, and attended cull-rate/visual/FPS evidence remains open.
- Native glow/SMAA pass 19,815 assertions/four GPU cases; native lens-dirt/display-post ownership pass 3,019 assertions/two cases; native font atlas upload/readback/release passes 28 assertions/five cases.
- Native transient pool passes 943 assertions / 14 cases; full alrender passes 556,617/556,617 assertions across 132 cases.
- Full harness passes 517/517 tests in 1,204.60 seconds; six Darwin GL-only migration suites remain intentionally omitted.
- Fresh shared renderer tests: 3,698 assertions / 111 cases pass.
- Focused shared shader tests: 1,353 assertions / 25 cases pass plus 13 hardware skips.
- Broad shared tests reproduce baseline host/driver failures: four assertions in three frame-fence timing cases and one render-target depth-format assertion.
- Commit `f799262971`: exact original tracing proves literal anisotropy levels and Ultra `4`; native mapping/state/world replacement proofs pass 8,092 assertions, full llrender passes 1,892 assertions/225 cases (one skipped), full Metal passes 52,842 assertions/59 cases, and Xcode Release `68521` builds/signs and verifies 349/349 ARM64 slices (executable SHA-256 `a527489ee69388abd6c34003e5d22632190807d7b8d95b589087f72b63a04353`). Harness structure remains 32/32; tool tests remain 31/31.
- Commit `e90ccdf18f`: exact original tracing proves selective vertex/index uploads and fresh-backing lifetime. Focused Release and RelWithDebInfo native tests each pass 46 assertions; full Release llrender passes 1,938 assertions/226 cases (one skipped), full Metal passes 52,842 assertions/59 cases, and source-exact Xcode Release `68525` builds/signs and verifies 349/349 ARM64 slices (executable SHA-256 `cb42e7fd22575fba27096ba40ce2e514cade94a82eb8a0a68a2cdb9840315403`). Harness/tool tests pass 32/32 and 31/31.
- Commits `8247d09c5d`/`f93c9f1927` restore bounded point lights/direct shadow slices with focused/full proof. Commit `8f4d5654e5` passes validated G-buffer/shadow/full Metal proof; `7cd1993d8e` passes texture-pressure and full viewer proof. Commit `1682010004` passes 1,991 projector, 10 visibility and 52,870/59 full Metal assertions. Commit `f8af790f32` retains 52,870/59; source-exact Xcode Release `68539` builds/signs/verifies 349/349 ARM64 slices, executable/metallib SHA-256 `427414330ab546c034536fc27227c1bad335726290bc963a1eb0087fe631a6cb`/`4f4778e6d6558654b52986ac4caa69b42add1ec1fe940bed331844a9f1ad97bb`.
- Release 68493 verifies 349/349 ARM64 slices; attended Mid/HDR remains stable through 500+ native world/UI publications without the prior water-count rejection.
- Commit `dbe1215cea`: diagnostics-free Xcode Release 68499 builds and verifies 349/349 ARM64 slices; the separate Ninja Metal suite passes 52,772 assertions/57 cases. Release `ctest` has no registrations because its preset has tests disabled; attended toggle-off and mirrors-off startup/login both pass.
- Commit `af65f062a1`: Xcode Release 68502 builds and verifies 349/349 ARM64 slices; the focused Metal suite passes 52,774 assertions/57 cases; attended diagnostics show four populated main directional cascades and stable moving avatar/object shadows.
- Commit `921aed5ca9`: source-exact Ninja Release 68508 builds, stages, signs and verifies 349/349 ARM64 slices; executable SHA-256 is `01dbd31503c99c2d3fb315c2584fe75d38c5f3478a35facc889c5b0fd82c6c5d`. The full Metal suite passes 52,777 assertions/57 cases. Its bounded owner-attended Ultra pair with exact-base OpenGL `67607` matches 17 live controls and world position; broad lighting/shadow/sky/color appearance is accepted while mirror content and formal launch-gated capture remain open. First-alpha tag `26.4.0.68508-alpha` targets this exact source commit. Release `68511` (`d4b150e31414a1437858a6e188e466564cb9100c0d9140afd05ca454ee3e4c07`) passes 52,786 Metal assertions; commit `3bd0451efc` Release `68512` (`86c520d97bdf640d9e791b6108c62bfb81cd1d40b99685dd73cd135ffcd411fe`) builds through the wrapper, passes 52,796 assertions, signing and 349/349 ARM64 verification, publishes across all four presets and reduces traced main/hero projector work by 78.9%/86.7%.
- External exact-base OpenGL validation HEAD `734afd3179`: configure and full Release build/stage exit 0; the `26.4.0.67607` app contains 8,111 files including `settings_files.xml`, passes deep strict signing and 349/349 ARM64 verification, and its executable SHA-256 is `de8fc8f817c0afb2063f641e9c24cbd58054c32c3053f4d0e863232498bb1797`. Corrected local QA graphics/Looks preset SHA-256 values are `8b5f56aaa26eed855ab4a671534d8a5beca8388b21d47a096789f87932eaef56` and `5108dd19f9c53aea37211b4cb08e84cce7b340cfc00c91a8a6b186b3bb58d4e6`; reference and candidate dry runs both exit 0 with identical 19-control expectations.
- Strict inventory: 1,797/2,403 files (`gl_function` 340, `gl_shader` 1,457, `sdl_gl` 0), expected exit 1; report SHA-256 is `219eb943960b124745e171a8d420e9ab983e74c04de50ad52f19a2ce47fee74d`; 331 below 2,128 and 504 below 2,301 with unchanged rules.
## Owner-attended visual cadence

Launch/login/capture only at the owner's explicit request and control; add bounded checks to the [daily visual queue](visual-review-queue.md).

## Complete-product checkpoint gate

1. **Product advancement:** native renderer startup semantics no longer require GL capability, state, binding or program ownership.
2. **Replacement boundary:** visible/hidden contexts and the product OpenGL link are gone; the reachable source tail must be replaced without losing essentials.
3. **Open release gates:** strict source/runtime no-GL, feature and visual parity, prolonged stability, independent review and measured performance.

## Immediate executable action

1. Quantify issued/skipped texture/sampler bindings and palette/constant allocations per main/hero/probe view, then attribute the remaining per-present hero/shadow CPU expansion against `68532` before selecting the next repair.
2. Add bounded issued/ready/culled query telemetry and run a dense-scene no-popping gate; live enablement is verified but effectiveness is not quantified.
3. Require launch-gated Ultra manifests and captured camera transforms before accepting screenshots or the next source-exact performance pair.
4. Trace original mirror terrain/content and clipping before deciding whether the bounded blue-gray-versus-green reflected-background difference needs a Metal fix.
5. Capture the camera transform and repeat exact-base `67607` with the fully resident protocol; smoke projector/ordinary-light/shadow continuity and run the formal paired gate.

## Non-negotiable open gates

No retained OpenGL, translation renderer or fallback backend; preserve Alchemy functionality by replacement. Do not mark feature parity from build/test success alone. Performance requires provenance-complete, quality-equivalent paired M1 Max measurements under `docs/metal/PERFORMANCE.md`. Keep fail-closed native authority, Snapshot/readback and presentation gates.
