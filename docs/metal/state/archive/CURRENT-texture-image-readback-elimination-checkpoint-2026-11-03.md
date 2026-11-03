# Texture-image readback elimination checkpoint — 2026-11-03

## Observed source state

Commit `7d3af86e6c88b6b4003b2c0ba52f30ccbe0e9ef3` removes every direct
production call to `glGetTexImage` and `glGetCompressedTexImage` from the
owned source tree.

`LLImageGL::scaleDown` no longer reads a selected mip through a scratch pixel
buffer and uploads it again. Its retained GL texture side effect now copies the
selected mip to new immutable storage with a framebuffer blit, entirely on the
GPU. Deprecated luminance/alpha storage keeps its exact stored channels and
swizzle, and mip tails are regenerated after the copy. The authoritative Metal
mirror remains native: it uses the existing native mip blit when a mip chain is
present and the cached native bilinear pass for no-mip scaling. This is not yet
a claim that `scaleDown` is Metal-only, because retained GL traversal still
needs the parallel GL texture.

The removed `LLCubeMapArray` resizing constructor was not a live growth path.
Its sole source caller was disabled under `#if 0` with an overflow warning. The
live reflection-map resize path already allocates a fresh array, reserves its
last two cubes as scratch space, invalidates every probe and clears its native
mirror and spherical-harmonic state. Removing the dead constructor therefore
removes an unreachable all-layer CPU round trip without changing live probe
preservation semantics.

`LLImageGL::checkTexSize` now reads dimensions from the immutable allocation
record rather than querying texture-level width and height. The function has no
call sites; it still contains GL binding diagnostics. Exact source search leaves
two `glGetTexLevelParameteriv` calls in `llgl.cpp`, both debug internal-format
validators. Function-pointer declarations and loaders for the removed texture
readback entrypoints remain until the GL loader is retired.

## Validation evidence

- `tools/metal/doctor.sh` passed on macOS 26.5.2, arm64, MacBookPro18,2 with an
  M1 Max, 64 GiB unified memory, Apple7-family support, Xcode 26.6 and the Metal
  toolchain present.
- The required command
  `AL_METAL_PRESET=ninja-os-arm64 AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=10 tools/metal/build.sh build`
  exited 0 at exact source
  `7d3af86e6c88b6b4003b2c0ba52f30ccbe0e9ef3`, with warnings as errors,
  producing Development build `26.4.0d68125` (`26.4.0.68125`).
- With `MTL_DEBUG_LAYER=1`, the source-exact
  `llrender_gl_fresh_tests` binary passed 1,216 assertions across 85 cases with
  Metal API Validation enabled. This physically exercises the selected-mip
  copy, deprecated two-component channel/swizzle preservation, native mip blit,
  native no-mip resample and upload-in-flight exclusion.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68125.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; the run is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `3226822da171f02d69b496bd4e628012635053fcb0bb1471666fb0963eb481fd`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,249 findings and exits 1 as expected, a
  reduction of 16 findings from the prior exact checkpoint. The ignored report
  is `artifacts/metal/gl-strict-texture-image-readback-elimination.json`.
- `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer.
  The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Cut over accelerated macOS CEF media ingress from its current duplicate
IOSurface-to-GL and IOSurface-to-Metal writes to the existing exact Metal import
and copy alone. The Darwin caller must publish a frame only after the native
copy succeeds, while non-Darwin interop remains unchanged. Moving-media visual
parity stays in the owner-attended daily queue rather than triggering an
autonomous viewer comparison.
