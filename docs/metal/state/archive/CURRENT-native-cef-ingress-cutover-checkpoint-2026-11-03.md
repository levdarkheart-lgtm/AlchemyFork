# Native CEF media ingress cutover checkpoint — 2026-11-03

## Observed source state

Commit `a33f57df9c70ce4cff5434f622437f78b54a801f` removes the Darwin
accelerated-CEF path's duplicate OpenGL IOSurface import and framebuffer copy.
The macOS implementation no longer creates a GL rectangle texture, calls
`CGLTexImageIOSurface2D`, owns an `ALTextureBlit`, or requires a current GL
context.

Darwin interop initialization now requires the published `ALMetalDevice` and a
native `ALMetalTextureResampler`; absence of either rejects accelerated paint so
the media plugin can retain its CPU paint path. A received +1 IOSurface is
imported as a shared BGRA8 Metal texture. The new source replaces the previous
one only after native import succeeds, so a failed import cannot publish a
stale or partially initialized source.

`LLViewerMediaImpl::updateAcceleratedTexture` now branches before consulting a
GL texture name. On Darwin it creates or reuses the render-target-capable native
media mirror, encodes the top-down to bottom-up Metal copy, and publishes the
new content generation only after that copy is committed. Copy or adoption
failure discards the old native mirror, returns false and does not mark the
media texture playing. Windows D3D/WGL and Linux dma-buf/EGL interop are
unchanged during this bounded cutover.

The existing native GPU regression constructs a real IOSurface, imports it
without CPU staging, flips its two distinct rows through the same resampler,
reads the result back byte-exactly and rejects a mismatched descriptor. Moving
CEF/VLC appearance remains queued for the owner-attended daily review; this
source/GPU checkpoint is not visual parity evidence.

## Validation evidence

- The required command
  `AL_METAL_PRESET=ninja-os-arm64 AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=10 tools/metal/build.sh build`
  exited 0 at exact source
  `a33f57df9c70ce4cff5434f622437f78b54a801f`, with warnings as errors,
  producing Development build `26.4.0d68127` (`26.4.0.68127`).
- With `MTL_DEBUG_LAYER=1`, the complete source-exact `almetal_tests` binary
  passed 52,221 assertions across 52 cases with Metal API Validation enabled.
  The focused IOSurface media-ingress case passed 16 assertions.
- The source-exact `llrender_gl_fresh_tests` binary passed 1,216 assertions
  across 85 cases with Metal API Validation enabled.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68127.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `970a08af785d28f1ca111188c2a41beca3aedc9adc1ebcb93cc6aafc5c04decb`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- The packaged viewer executable has no undefined
  `CGLTexImageIOSurface2D` dependency.
- Strict inventory is 2,408 files / 3,244 findings and exits 1 as expected, a
  reduction of five findings from the prior exact checkpoint. The scanner is
  preprocessor-blind and still inventories retained non-Darwin interop source.
  The ignored report is
  `artifacts/metal/gl-strict-native-cef-ingress-cutover.json`.
- `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer.
  The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Cut the ordinary CPU-painted media path over from its Darwin GL allocation and
sub-image upload plus Metal mirror to an explicit native-only media upload
contract. Preserve partial dirty rectangles, asynchronous publication and the
VLC/CEF byte format, and keep the existing moving-media question in the daily
owner-attended queue.
