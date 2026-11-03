# Native CPU-generated sky-face checkpoint — 2026-11-03

## Observed source state

Commit `0464f5466ec53c94fb9b9e736e26420afe363020` removes OpenGL
ownership from the Darwin `LLSkyTex` CPU-generated sky and shiny face pairs.
The publication boundary is now renderer-neutral `createImage`: first
publication creates explicit linear RGBA8 Metal storage and subsequent updates
replace the complete native face in place. Current/next double-buffer selection,
raw face generation and downstream legacy sky/cubemap consumers are preserved.

Darwin GL cleanup no longer drops these logical/native textures. Restore
republishes both raw buffers so a global renderer cleanup that discarded native
storage cannot leave either half of the pair missing. A transient in-place
update failure retains the previous complete native texture for the next retry.

The physical RGBA regression now distinguishes this owner from sRGB parcel
storage: explicit `GL_RGBA8` creation produces `rgba8_unorm`, creates no GL
name and retains the prior exact RGBA create/update/readback coverage. The same
commit refines parcel-overlay retry so its native-ready flag remains set when a
failed update leaves the prior complete Metal texture intact.

No viewer was launched or driven. Legacy sky/shiny appearance is queued for
owner-attended daily review rather than inferred from deterministic validation.

## Validation evidence

- The required `tools/metal/build.sh` command exited 0 at exact source
  `0464f5466ec53c94fb9b9e736e26420afe363020`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68139` (`26.4.0.68139`).
- `cmake --build build-Darwin-ninja-os-arm64 --target almetal_tests
  llrender_gl_fresh_tests llrender_gl_shared_tests newview_tests -j 10`
  exited 0 and linked the source-exact RelWithDebInfo physical test binaries.
- With `MTL_DEBUG_LAYER=1`, the complete source-exact `almetal_tests` binary
  passed 52,221 assertions across 52 cases with Metal API Validation enabled.
- With `MTL_DEBUG_LAYER=1`, the source-exact `llrender_gl_fresh_tests` binary
  passed 2,191 assertions across 92 cases with Metal API Validation enabled.
  This includes explicit linear RGBA8 native-format/no-GL proof plus the prior
  exact RGBA update, typed allocation, partial update and mip regressions.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68139.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --target
  appearance-utility-bin -j 10` exited 0 with warnings as errors at source
  build 68139.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `02401ca36b16f8ee4622cf50c8414906b69c1e38be77edca628672c6db0ca20a`.
- Appearance-utility SHA-256:
  `5769d937be848ee0b6edf9c153eefb0b0107898f90bc42fb377399e2ff7b337e`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/validate_harness.py` passed all 32 required files, and
  `git diff --check` passed before this documentation update.
- Strict inventory is 2,408 files / 3,259 findings and exits 1 as expected.
  The sole delta from the parcel checkpoint is the native
  `LLImageGL* image = ...getGLTexture()` handle line, classified heuristically
  as `gl_shader`; no GL call site was added. The precommit and exact-source
  reports are byte-identical at SHA-256
  `1b1c1c444707ce6b14f60326a6eb10d322658380a189dfc9bb2a74ea103fdbce`.
  The ignored exact report is
  `artifacts/metal/gl-strict-native-sky-faces.json`.
- The packaged viewer has no undefined `CGLTexImageIOSurface2D` dependency.
  `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer
  and source-exact appearance utility. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use the `LLNetMap` CPU-painted object and parcel layers as the next bounded UI
resource owner. Both raw-image constructors allocate GL immediately and every
refresh calls `setSubImage`, while native UI capture consumes the same sampled
logical textures. Preserve size/scale regeneration, object/parcel refresh
cadence and UI identity while moving Darwin creation/update to Metal-only RGBA.
Do not launch either viewer; queue minimap appearance for the owner-attended
daily review.
