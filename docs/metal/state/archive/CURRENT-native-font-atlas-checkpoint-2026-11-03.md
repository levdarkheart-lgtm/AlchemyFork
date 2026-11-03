# Native font-atlas checkpoint — 2026-11-03

## Observed source state

Commit `b79cb6a6ca3ef3f4b879ca5e8d62cd5ff00abb80` removes OpenGL
ownership from Darwin grayscale and color font-atlas pages. Viewer font startup
explicitly selects the Metal backend on Darwin. Non-Darwin production and
retained GL tests explicitly select GL, while CPU-only tests select neither.

New grayscale pages publish as `rg8_unorm` with the established RRRG swizzle;
new color pages publish as `rgba8_unorm_srgb`. Rasterized glyphs publish only
their exact dirty rectangles. If a native subimage update fails, publication
retries the complete CPU page. If that also fails, glyph metadata is not cached,
so later layout cannot point at pixels which never reached the selected GPU
backend. Existing page reuse, eviction and logical-image identity are retained.

Physical validation reads both native page types back after dirty updates and
proves exact CPU/GPU byte identity, descriptor semantics and absence of a GL
texture. No viewer was launched or driven. Grayscale/subpixel text, color emoji,
shadows, reload, DPI/UI-scale and eviction appearance are queued for an
owner-attended daily review rather than inferred from deterministic tests.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and Metal compiler present.
- The required `tools/metal/build.sh` command exited 0 at exact source
  `b79cb6a6ca3ef3f4b879ca5e8d62cd5ff00abb80`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68143` (`26.4.0.68143`).
- `cmake --build build-Darwin-ninja-os-arm64 --config RelWithDebInfo --target
  almetal_tests llrender_tests llrender_gl_fresh_tests
  llrender_gl_shared_tests newview_tests -j 10` exited 0 and linked the exact
  source RelWithDebInfo binaries.
- With `MTL_DEBUG_LAYER=1`, `almetal_tests` passed 52,221 assertions across 52
  cases with Metal API Validation enabled.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- With `MTL_DEBUG_LAYER=1`, `llrender_gl_fresh_tests` passed 2,216 assertions
  across 93 cases with Metal API Validation enabled. The added physical case
  verifies grayscale `rg8_unorm` plus RRRG swizzle, color
  `rgba8_unorm_srgb`, exact dirty-rectangle updates and no GL name.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped as expected. Compiled and runtime build numbers
  both report 68143.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --config
  RelWithDebInfo -j 10` exited 0 with warnings as errors at build 68143.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `8e14e74c38ff6de8dfbbf4ab889cc55a24b478fce69da92d2262ffc003e8e723`.
- Appearance-utility SHA-256:
  `89908a96bfef57e5f62ef17cf515b96d067f7e381c01c887fffe0ce5b2708dd5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,259 findings and exits 1 as expected.
  The font-atlas cutover replaces structural `LLImageGL` matches without adding
  a GL call. Precommit and exact-source reports are byte-identical at SHA-256
  `3a13c17d3f58fe2bd9de2c92767b46106dba56b53e66b50312fd0a03467d73af`.
  The ignored exact report is
  `artifacts/metal/gl-strict-native-font-atlas.json`.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use the legacy sky `LLCubeMap` as the next bounded resource owner. Darwin still
allocates one GL cube and six GL face wrappers, publishes transformed CPU face
bytes to that cube and then builds a separate native cube from the same bytes.
The native Metal world-lighting and forward paths already consume
`getMetalTexture()`. Split CPU face preparation from GL allocation so Darwin
publishes only the native linear-RGBA8 cube, while preserving the historical
-X,+X,-Y,+Y,-Z,+Z transforms, face-to-slice mapping, lifecycle retry and the
non-Darwin GL path. Add physical descriptor, face-order and exact-byte tests.
