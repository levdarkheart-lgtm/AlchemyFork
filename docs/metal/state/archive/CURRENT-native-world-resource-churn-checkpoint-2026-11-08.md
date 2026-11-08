# Native world resource-churn checkpoint — 2026-11-08

## Scope and classification

This checkpoint records the source review and bounded repairs for two claimed
performance issues on `codex/apple-silicon-metal`:

1. retained Main/Probe/Hero targets and the direct renderer allocated
   overlapping world resources, while water and atmospheric haze each retained
   a full-resolution depth snapshot; and
2. ordinary G-buffer batches rebound texture slots that material shaders
   immediately replaced, while shadow and G-buffer skinning created native
   palette buffers per invocation.

Both claims were grounded, with important limits. They are structural CPU/GPU
resource costs, not evidence that the scene renders twice and not proof of an
FPS gain. Commit `8f4d5654e53f0540a263c4f954ccbbc10d46a7c4`
implements the safe bounded reductions described below.

## Exact-original and current-source findings

Exact original commit `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`
binds the retained `deferredScreen` G-buffer as the actual deferred geometry
destination. Its resource plan owns the albedo, specular/ORM, normal, optional
emissive and depth/stencil attachments. Original indexed legacy and glTF
material families are capped at eight material slots.

Reference source SHA-256 values:

- `indra/newview/pipeline.cpp`:
  `bb52adcb83cf493d57246b6410294bb8579f429454b024aeae1ae66efa23231b`;
- `indra/alrender/alrenderresourceplan.cpp`:
  `398bbc83dbac2743a5bcd3809994029ec4864f47540ac73ca68a857cc213a6fd`;
- `indra/llrender/llglslshader.h`:
  `93edb05d16724dd632f0f21af215e600c4fbafa772bfc391c9e12b5e11a8424a`.

Current Metal source genuinely allocated the retained targets through
`LLRenderTarget::rebuildMetalTarget()` and separately acquired a four-color
plus D32S8 target in `ALMetalWorldGBufferRenderer::render()`. The retained
targets still supply live plan dimensions, readiness and native attachments;
deleting them would have broken consumers. This was duplicate physical
storage, not duplicate scene traversal.

The opaque post-deferred owner also ended its encoder, copied attached D32S8
into a shader-readable snapshot, then began a load/store pass for each haze
marker. Water haze and atmospheric haze used distinct snapshots even though
their use is sequential. The pass boundaries and copies are real, but remain
required by the current contract: each fragment shader samples a depth
snapshot while the output pass retains attached depth/stencil for later draws.
Making those sampled textures memoryless would be invalid.

The ordinary G-buffer bound 32 diffuse slots before both material families.
The legacy-material shader consumes 8 base + 8 normal + 8 specular slots, so
the old path made 48 individual texture calls for 24 final slots. PBR consumes
8 base + 8 normal + 8 ORM + 8 emissive slots, so it made 56 calls for 32 final
slots. The wrapper forwards every call to Metal; normal and ORM bindings use
linear views and cannot be folded into their sRGB base-color bindings.

Shadow rendering allocated one shared `MTLBuffer` wrapper for every skinned
batch invocation. G-buffer rendering deduplicated identical CPU pointers only
within one view, then rebuilt buffers for subsequent main/probe/hero views.
The stated 16-allocation mirror example is structurally possible but was not
measured. Palette values can be view-dependent, so cross-view pointer identity
alone is not a correct cache key.

## Implemented repair

- `ALMetalWorldGBufferScene` accepts an optional exact retained destination.
  The renderer validates dimensions, four formats, D32S8, 2D type and
  shader-readable usage, then creates a borrowed wrapper around those physical
  attachments. Production supplies Main/Probe/Hero `deferredScreen` storage
  only for an exact four-attachment plan and alternates with the pooled target
  when the preceding transaction still names the retained set. Three-color
  emissive-disabled plans keep the existing pooled path.
- Water and atmospheric haze now share one shader-readable D32S8 snapshot.
  Command order completes the water-haze sample before atmospheric haze
  overwrites that snapshot. This removes one simultaneous full-resolution
  depth allocation when both markers occur; it does not remove either required
  depth copy or encoder boundary.
- Legacy-material and PBR batches bind only their eight base textures before
  their specialized ladders. Ordinary simple/indexed batches continue to bind
  all 32 diffuse slots. Linear normal/ORM views remain distinct.
- Shadow and G-buffer palettes of at most 4,096 bytes use Metal inline command
  data, whose lifetime is completion-safe. This covers avatar palettes and
  ordinary rigged palettes through 85 joints without a separate native buffer
  allocation. Larger palettes keep the validated buffer path and its existing
  within-view pointer deduplication.

This is intentionally not the full arena/clustered-resource redesign. Retained
screen/post targets, larger skin palettes, opaque-post diagnostic palettes,
terrain factors, light constants and the haze copy/pass contract remain. A
future arena should use content/generation identity for actually identical
data and must preserve view-dependent values.

## Validation evidence

Physical host: MacBookPro18,2 M1 Max, 64 GiB unified memory, macOS 26.5.2,
Xcode 26.6, Apple family 7. `tools/metal/doctor.sh` exits 0.

Validated commands and results:

- Release and RelWithDebInfo `almetal_tests` targets build with warnings as
  errors;
- under `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`, the world G-buffer test
  passes 8,075 assertions in each configuration. It proves borrowed attachment
  identity, material/PBR/rigged rendering and combined water/atmospheric haze;
- under the same validation, the direct shadow test passes 129 assertions in
  each configuration and exercises skinned shadow palettes;
- the full validated Release Metal suite passes 52,869 assertions across 59
  cases;
- `tools/metal/build.sh build` succeeds from committed source as Xcode Release
  `26.4.0.68532`;
- deep strict signing and `tools/metal/verify_app.sh` pass; all 349 inspected
  Mach-O slices are ARM64.

Source-exact artifact hashes:

- executable:
  `f242b041ebcba41cdc07a003b4fa040321c87368190b8aa192c803655953e9f9`;
- `almetal_builtin.metallib`:
  `4f4778e6d6558654b52986ac4caa69b42add1ec1fe940bed331844a9f1ad97bb`.

No viewer launch, visual acceptance, allocation trace or FPS comparison was
performed. Measure settled resource high-water and CPU/GPU channels in a
matched owner-attended run before attributing performance improvement.
