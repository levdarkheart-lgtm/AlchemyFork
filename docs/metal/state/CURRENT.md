# Current engineering state

**Observed:** 2026-11-05 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer code checkpoint:** `3087308155`

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

`LLGLSLShader::isComplete()` remains strictly a linked-GL-program predicate;
`isNativeConfigured()`/`isAvailable()` cannot disguise a native identity as a
GL object. GL-backed occlusion remains fail-closed until a native query owner
replaces it.

The macOS SDL window still requests an OpenGL surface, creates/makes current a
4.1 context, initializes `gGLManager`, and tears that context down later. The
source-exact next boundary is to remove this Darwin-only context lifecycle,
then repair any remaining essential startup/resource consumer exposed by the
no-context run. The OpenGL link remains until the context and all reachable
application-owned calls are gone.

## Build and validation evidence

- `tools/metal/doctor.sh` passes on macOS 26.5.2, MacBookPro18,2, M1 Max,
  64 GiB unified memory and Xcode 26.6; Metal reports Apple family 7 and a
  62,277,025,792-byte recommended working set.
- `tools/metal/build.sh` rebuilt 1,059 cacheable tasks in Release with
  warnings-as-errors and ended `BUILD SUCCEEDED`.
- Staged package: `Alchemy Metal Development 26.4.0.68437`.
- `tools/metal/verify_app.sh` passes 349/349 inspected Mach-O slices as ARM64;
  `codesign --verify --deep --strict` passes.
- Viewer executable SHA-256:
  `3a812d9b9cdbf4f051bb6214d51d2396b7686a272cd3ef0a6eebae862c72edc2`.
- Packaged `almetal_builtin.metallib` SHA-256:
  `581fc1df32c6e76f77087353b483466428877c5a89b2fe2b92df296bb06a164f`.
- `llrender_tests`: 1,605 assertions pass across 209 cases, with the expected
  unavailable-font skip.
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

Strict source inventory intentionally still fails: 2,324 findings in 2,409
viewer-owned files (`gl_function` 657, `gl_shader` 1,597, `sdl_gl` 72).
Report SHA-256 is
`7e911a9e1793938bf2153bf85df22ba5901cb8efe35efa8d7e42980f67fb8330`.
No audit rule or suppression changed. Relative to the earlier 2,301 checkpoint,
direct GL-function findings fell by six; the aggregate rose because the native
semantic bridge still resides in `LLGLSLShader`-named source, which the textual
shader inventory conservatively counts. The release gate remains zero and
binary/runtime no-OpenGL proof remains open.

## Visual review cadence

Codex does not autonomously launch, drive or capture either viewer. Existing
font, exposure/mirror and Snapshot observations plus new checks accumulate in
the [owner-attended queue](../work/active/visual-review-queue.md). No queued row
is a parity pass without explicit owner evidence.

## Milestone state

| Area | State | Current fact |
| --- | --- | --- |
| M-003 platform/presentation | doing | Metal owns device, queue, drawable, sizing and vsync; Darwin GL context lifecycle remains. |
| M-004 resources/readback | doing | Ordinary geometry, textures/fonts and async uploads are native; Snapshot GL scratch is gone. |
| M-005 pipelines/bindings | doing | MSL pipelines execute; native pass identities no longer require GLSL/program names. |
| M-006 UI/media | doing | UI/font capture is native; owner accepted readable text previously; Snapshot UI remains visually open. |
| M-007 world/material | doing | Normal world/material/avatar/shadow/sky/water layers have direct Metal owners; parity remains open. |
| M-008 effects/advanced views | doing | Core effects and many diagnostics are native; remaining optional paths follow the usable-core cut. |
| M-009 parity/stability/no-GL | doing | Strict, linked, runtime, visual and stability gates remain open. |
| M-010 measured performance | deferred | No quality-equivalent paired M1 Max claim yet. |
| M-011 GL authority removal | doing | Capability/init/state/binding/pass setup now survive no GL; primary context/link remain. |
| M-012 Computer Use bridge | complete | Semantic bridge verified; visual driving is owner-attended. |
| M-013 RLVa strict parity | deferred | Nonessential ordering only; final no-GL behavior is still required. |

## Complete-product advancement assessment

**Advances the complete product:** yes. Core capability, initialization, state,
binding and pass configuration now remain valid without a GL context, while
required rendering behavior retains native owners instead of being deleted.

**Still incomplete:** the primary GL context and OpenGL link remain, the
strict no-GL gate fails, and full visual parity, prolonged stability, performance and
independent build/review evidence remain open. This checkpoint is not product
completion or release readiness.

## Exact next actions

1. Remove the Darwin SDL OpenGL-window flag, context creation/currentness,
   `gGLManager.initGL()` and context teardown while preserving Metal surface,
   input, display metrics, fullscreen and resize behavior.
2. Build and exercise no-context startup tests; repair essential resource/pass
   consumers rather than restoring a fallback.
3. Remove the OpenGL link once no reachable application-owned GL entrypoint
   remains; rerun strict source/link/runtime gates after every coherent cut.
4. Keep visual checks queued and do not launch either viewer autonomously.
