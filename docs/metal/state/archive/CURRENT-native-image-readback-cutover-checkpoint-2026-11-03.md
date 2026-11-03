# Native image readback cutover checkpoint — 2026-11-03

## Observed source state

Commit `0fb0d391d1e0bdaac0ba30ba554eb92cafb3c6b1` makes
`LLImageGL::readBackRaw` native-only on the Darwin product path. The function
requires the published `mMetalTexture` mirror and published discard/mip
metadata; it no longer falls back to `glGetTexImage`,
`glGetCompressedTexImage` or GL texture-level metadata queries.

Uncompressed byte storage with one, two or four components reads directly from
the Metal texture. Three-component storage reads the native RGBA backing and
collapses it to RGB. Explicit `compressed_ok=true` preserves block-compressed
native readback when the mirror descriptor is compatible. A compressed request
with `compressed_ok=false`, a missing or mismatched mirror, unsupported storage
or an invalid discard level fails closed because no native decompression owner
exists. An upload in flight does not expose pending data: readback stays on the
published mirror until publication completes.

The only production caller is `LLViewerFetchedTexture::readbackRawImage`, which
passes `compressed_ok=false`. Its retained saved-raw consumers include fetched
and sculpt images, thumbnails/profile textures, material and glTF previews and
RLVa effects. Production explicit-format inspection found no BC/DXT
configuration on this caller path; fetched textures upload decoded byte
`LLImageRaw` data.

The hidden GL test fixture now publishes its hidden-window Metal device to
`LLImageGL` and restores the previous device after cleanup. It also refreshes
the published device when a test replaces the hidden context. Physical Metal
coverage proves byte-exact one-, two-, three- and four-component reads, upload
publication ordering, wider-format handling and fail-closed behavior while a GL
texture name still exists but the native mirror does not.

Exact source search leaves two production `glGetTexImage` calls: the
`LLImageGL` scale-down PBO path and `LLCubeMapArray` growth. Two
`glGetTexLevelParameteriv` calls remain in `LLImageGL::checkTexSize` debug
validation. `LLImageGL::readBackRaw` itself has no direct GL read or metadata
query.

## Validation evidence

- `tools/metal/doctor.sh` passed on macOS 26.5.2, arm64, MacBookPro18,2 with an
  M1 Max, 64 GiB unified memory, Apple7-family support, Xcode 26.6 and the Metal
  toolchain present.
- The required command
  `AL_METAL_PRESET=ninja-os-arm64 AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=10 tools/metal/build.sh build`
  exited 0 at exact source
  `0fb0d391d1e0bdaac0ba30ba554eb92cafb3c6b1`, with warnings as errors,
  producing Development build `26.4.0d68123` (`26.4.0.68123`). A subsequent
  source-exact wrapper invocation reported no work and exited 0.
- With `MTL_DEBUG_LAYER=1`, the source-exact
  `llrender_gl_fresh_tests` binary passed 1,216 assertions across 85 cases with
  Metal API Validation enabled.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68123.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; the run is recorded as failed, not promoted to green.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `8616f6803def8f4b0e57f2866eda99ed39e12a6eb2e5d4ea72e3634a625498d8`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory remains 2,408 files / 3,265 findings and exits 1 as expected.
  Removed calls are offset by the scanner's identifier/comment/string
  `gl_shader` false positives, so the unchanged aggregate is not represented as
  strict progress. The ignored exact report is
  `artifacts/metal/gl-strict-native-image-readback-cutover.json`.
- `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer.
  The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Replace `LLCubeMapArray` growth's all-layer `glGetTexImage`/CPU
scale/`glTexSubImage3D` round trip with explicit native six-face-per-probe
copy/scale while preserving growth semantics. Then remove the `LLImageGL`
scale-down PBO readback and make debug texture-size validation use published
native metadata.
