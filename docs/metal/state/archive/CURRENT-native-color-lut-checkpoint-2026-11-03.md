# Native color-grading LUT checkpoint — 2026-11-03

## Observed source state

Commit `e213e9ac01d24efbdb5b587e818b24b845a456cb` removes OpenGL
ownership from the Darwin pipeline color-grading LUT. Loaded state is now
backend-neutral. Darwin clears and publishes only `mMetalCGLut`; non-Darwin
retains the existing `ALTexture3D` allocation and upload for later platform
pruning. The retained Darwin GL post disables the LUT sampler when the GL
object is absent instead of dereferencing missing storage, while the native
world path continues to consume the Metal LUT.

Both supported source encodings use the same native publication primitive.
`alMetalCreateUnormVolumeArray` accepts 8- or 16-bit RGB/RGBA unorm data,
expands RGB to RGBA with opaque alpha, maps one volume Z slice to each Metal
array layer, checks byte-count overflow and publishes private shader-read
storage only after the upload command commits. The 16-bit `.cube` path retains
its precision; the decoded 8-bit image-strip path retains its component and
slice ordering. Native-device limits replace the GL maximum on Darwin.

The added physical regression reads every RGB8-to-RGBA8 voxel across two
layers, proves opaque alpha, reads exact RGBA16 values across layers and rejects
invalid dimensions, component counts, component sizes and null data. Failed
native creation cannot set loaded state.

No viewer was launched or driven. Strength, bypass, transition behavior and
color precision are queued for an owner-attended daily review rather than
inferred from deterministic tests.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and Metal compiler present.
- The required `tools/metal/build.sh` command exited 0 at exact source
  `e213e9ac01d24efbdb5b587e818b24b845a456cb`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68147` (`26.4.0.68147`).
- `cmake --build build-Darwin-ninja-os-arm64 --config RelWithDebInfo --target
  almetal_tests llrender_tests llrender_gl_fresh_tests
  llrender_gl_shared_tests newview_tests -j 10` exited 0 and linked the exact
  source RelWithDebInfo binaries.
- With `MTL_DEBUG_LAYER=1`, `almetal_tests` passed 52,272 assertions across 53
  cases with Metal API Validation enabled. The new physical case contributes
  51 assertions for exact RGB8 expansion, RGBA16 precision/layer preservation
  and invalid-input rejection.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- With `MTL_DEBUG_LAYER=1`, `llrender_gl_fresh_tests` passed 2,243 assertions
  across 94 cases with Metal API Validation enabled.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped as expected. Compiled and runtime build numbers
  both report 68147.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --config
  RelWithDebInfo -j 10` exited 0 with warnings as errors at build 68147.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `72bb699014c3e2b8eaf8dd882ccdd2a41ce5d9deb7cdcb83b6f93aa308704727`.
- Appearance-utility SHA-256:
  `3449cb8fb684d944cfb7f813f6379b6280b2060635ee6715ef452e01a1d951cd`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,259 findings and exits 1 as expected. The
  precommit report SHA-256 is
  `a8e681c2607c41595dc41da1329020ca29514676c8140daed419d3b73c6d9df9`;
  the exact-source report SHA-256 is
  `26f65a0e9a325d87dbf44ecac5b12d12cef4fc0ec3e5a475b1d204538adc0ab6`.
  Counts are unchanged, while matching source snippets differ. The ignored
  exact report is `artifacts/metal/gl-strict-native-color-lut.json`.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use the baked tone-curve LUT as the next bounded resource owner. Source
inspection confirms Darwin allocates an empty GL RGBA16 texture, bakes the same
samples into `mToneCurveLut` and `mMetalToneCurveLut`, gates activation on the
GL texture name and samples it in the retained GL post. Make activation
backend-neutral, allocate and update only the native texture on Darwin, retain
non-Darwin GL behavior and physically prove exact samples, state transitions,
identity handling and failure state.
