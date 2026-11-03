# Native pipeline BRDF-LUT checkpoint — 2026-11-03

## Observed source state

Commit `8914212806ff815379092343c54d0b9c5764379a` removes the duplicate
Darwin pipeline GL BRDF owner. `ALRenderResourcePlan` does not request
`Id::BrdfLut` on Darwin, `LLViewerShaderMgr` neither builds nor unloads
`gDeferredGenBrdfLutProgram`, and pipeline stale/create/bind operations are
Darwin no-ops. Non-Darwin retains the original 512x512 RG16F target, raster
generator and binding behavior.

The native world renderer and native PBR preview remain the authoritative
consumers. Each allocates and computes its own Metal RG16F split-sum table. The
Darwin shader build defines `AL_METAL_NATIVE_BRDF`; the retained GLSL helper
returns deterministic zero instead of sampling arbitrary state when it is
compiled for Darwin. That retained helper is not native presentation authority.

The new physical regression dispatches the production
`almetal_pbr_preview_brdf_lut` kernel into an exact 512x512 private,
shader-read/write RG16F texture. It reads six edge/interior texels and compares
both components within 0.004 of an independent CPU 1,024-sample
Hammersley/GGX split-sum oracle. Resource-plan tests prove the Darwin target is
absent and the non-Darwin target retains its exact size and nonzero format.

No viewer was launched or driven. Roughness, grazing-angle energy and
world/preview agreement are queued for an owner-attended daily review rather
than inferred from deterministic or numerical tests.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and Metal compiler present.
- The required `tools/metal/build.sh` command exited 0 at exact source
  `8914212806ff815379092343c54d0b9c5764379a`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68151` (`26.4.0.68151`).
- `cmake --build build-Darwin-ninja-os-arm64 --config RelWithDebInfo --target
  almetal_tests alrender_tests llrender_tests llrender_gl_fresh_tests
  llrender_gl_shared_tests newview_tests -j 10` exited 0 and linked the exact
  source binaries. The separate warnings-as-errors RelWithDebInfo appearance
  utility build also exited 0.
- With `MTL_DEBUG_LAYER=1`, `almetal_tests` passed 52,345 assertions across 55
  cases with Metal API Validation enabled. The new physical case contributes
  56 assertions for the exact descriptor, production dispatch and independent
  numerical reference.
- `alrender_tests` passed 557,292 assertions across 137 cases, including the
  platform-specific BRDF resource-plan contract.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- `llrender_gl_fresh_tests` passed 2,243 assertions across 94 cases on the
  physical acceptance host.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped. Compiled and runtime build numbers both report
  68151.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `9776041d3b9344892d90458fb62c9a8f61ed79515e1ad279006226c5ad94e9bd`.
- Appearance-utility SHA-256:
  `514308e32e9afd6e17c72c5545061749b8bc1d8bb35205ccca471a11623c864a`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
  Source and packaged `deferredUtil.glsl` are byte-identical at
  `78002b1d66fd816dc2b16fac62a05c1136d0c9302c13b62dec5ef15ee0a2ecba`.
- `strings` finds neither `Brdf Gen Shader` nor either `genbrdflut` shader name
  in the packaged viewer executable. The packaged GLSL source is byte-identical
  to the validated repository source.
- Strict inventory is 2,408 files / 3,259 findings and exits 1 as expected.
  Precommit and source-exact reports are byte-identical at
  `ae1a31c0bca1242a4950118e029a09685fd2f79764f58c0f879808376a905862`.
  The ignored exact report is
  `artifacts/metal/gl-strict-native-brdf-lut.json`.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use the SMAA lookup resources as the next bounded duplicate owner. Source
inspection confirms Darwin still allocates GL area, search and optional sample
textures when SMAA is selected. `ALMetalWorldRenderer` already owns private
Metal area/search tables, binds them in its native SMAA transaction and has
physical edge-detection, blending-weight and neighborhood oracles. Remove the
duplicate Darwin GL names without weakening fail-closed retained-presentation
behavior; preserve non-Darwin GL behavior and prove exact table orientation,
contents, lifecycle and allocation-failure handling before pruning traversal.
