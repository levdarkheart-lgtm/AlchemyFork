# Native legacy sky-cube checkpoint — 2026-11-03

## Observed source state

Commit `4a005b8ef950f1872c0ad9a86cfdc575dff2030c` removes OpenGL
ownership from the Darwin legacy sky `LLCubeMap`. `initRawData()` prepares the
six transformed CPU faces independently of GL allocation. On Darwin,
`LLCubeMap::init()` publishes those faces directly to the established native
cube, while `initGL()` is inert and cannot recreate the duplicate GL cube or
its six `LLImageGL` face wrappers. Non-Darwin retains the GL implementation for
later platform pruning.

The native texture is a 64x64, one-mip, linear `rgba8_unorm` `texture_cube`.
It preserves all historical per-face flips/transposes and maps the legacy
`-X,+X,-Y,+Y,-Z,+Z` source order to Metal cube slices exactly. Retained Darwin
GL traversal that encounters the native-only cube clears any previous cube
binding instead of sampling stale storage. `getGLName()` is zero and resolution
queries fall back to the CPU faces.

Publication is fail-closed by generation. A cube prepared without a Metal
device does not expose the preceding native generation; restoring the device
publishes the pending generation on retry. Physical validation reads all six
slices back byte-for-byte, changes a source byte across a missing-device retry
and proves the precise changed output byte after recovery.

No viewer was launched or driven. Seams, shiny/water response and environment
changes are queued for an owner-attended daily review rather than inferred from
deterministic tests.

## Validation evidence

- `tools/metal/doctor.sh` exited 0 on the physical MacBookPro18,2 acceptance
  host: M1 Max, 64 GiB unified memory, arm64 macOS 26.5.2, Xcode 26.6,
  Apple7-family support and Metal compiler present.
- The required `tools/metal/build.sh` command exited 0 at exact source
  `4a005b8ef950f1872c0ad9a86cfdc575dff2030c`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68145` (`26.4.0.68145`).
- `cmake --build build-Darwin-ninja-os-arm64 --config RelWithDebInfo --target
  almetal_tests llrender_tests llrender_gl_fresh_tests
  llrender_gl_shared_tests newview_tests -j 10` exited 0 and linked the exact
  source RelWithDebInfo binaries.
- With `MTL_DEBUG_LAYER=1`, `almetal_tests` passed 52,221 assertions across 52
  cases with Metal API Validation enabled.
- The CPU-only `llrender_tests` binary passed all 1,513 assertions across 203
  cases: 202 passed and the established single-palette font case skipped.
- With `MTL_DEBUG_LAYER=1`, `llrender_gl_fresh_tests` passed 2,243 assertions
  across 94 cases with Metal API Validation enabled. The added physical case
  proves a 64x64 one-mip linear-RGBA8 cube, every historical face transform and
  source-to-slice mapping byte-for-byte, zero GL name, fail-closed missing-device
  state and exact changed-byte publication after device recovery.
- `newview_tests` passed all 98,973 assertions across 743 cases: 742 passed and
  one platform CRT case skipped as expected. Compiled and runtime build numbers
  both report 68145.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --config
  RelWithDebInfo -j 10` exited 0 with warnings as errors at build 68145.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `9277b87848d3723b04baa3c90dd123a23cd6faca1c991122dfb4bd35bc0870c6`.
- Appearance-utility SHA-256:
  `bf70df75140711cd0d034d5562d3ec567d9814b252f617d1202e4f3833e0cf10`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,259 findings and exits 1 as expected.
  Precommit and exact-source reports are byte-identical at SHA-256
  `a8e681c2607c41595dc41da1329020ca29514676c8140daed419d3b73c6d9df9`.
  The ignored exact report is
  `artifacts/metal/gl-strict-native-legacy-cube.json`.
- The packaged viewer and appearance utility have no undefined
  `CGLTexImageIOSurface2D` dependency. `otool -L` still reports both Apple
  OpenGL and Metal in each binary. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use the color-grading LUT as the next bounded resource owner. Source inspection
finds pipeline `mCGLut` as the only live production `ALTexture3D` user. Darwin
currently uploads decoded `.cube` or image-strip data into both a GL 3D texture
and the native 2D-array texture already consumed by Metal world color grading.
Make loaded state backend-neutral, allocate only the native texture on Darwin,
preserve non-Darwin `ALTexture3D`, and physically prove 16-bit/8-bit format,
slice order, component expansion, precision and failure-state behavior.
