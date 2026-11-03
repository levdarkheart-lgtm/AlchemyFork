# Native CPU media upload checkpoint — 2026-11-03

## Observed source state

Commit `026a97804bb361c173e2b473e7a61f2e6bf02d82` cuts the ordinary
CPU-painted CEF/VLC media update path off OpenGL on Darwin. The media worker now
wraps only the plugin's valid `data_width x data_height` shared-memory extent,
uploads that complete frame into a pending private Metal mirror, and never
reads the larger power-of-two destination dimensions from the plugin buffer.

The Darwin path no longer performs per-frame GL allocation, `setSubImage`, GL
texture-name handoff or GL-fence publication. It deliberately ignores the
plugin dirty rectangle because CEF and VLC expose a complete valid CPU frame;
the exact valid extent is uploaded while any larger destination padding is
left outside the upload region. Windows and Linux retain their existing GL
upload branch during this bounded cutover.

`LLImageGL` now supports a native-only worker publication with no GL name or
fence. Publication waits for the shared Metal batch's completion token, and its
success-aware callback aggregates batch commit status before media playing
state is advanced. Encode, work-queue post or batch-commit failure therefore
cannot publish a successful frame. The old no-argument callback remains for
existing callers.

Two physical renderer regressions cover the new boundary. One uploads a smaller
valid frame into a larger native texture and verifies exact bytes, untouched
padding, unchanged GL bytes/name and separate content authority. The other
holds the main and worker queues deterministically, proving that a native mirror
cannot publish before its shared Metal batch commits and that publication
success is reported.

Moving CEF/VLC appearance remains in the existing owner-attended daily review
queue. This checkpoint is source/build/GPU evidence, not visual parity evidence.

## Validation evidence

- The required command `tools/metal/build.sh` exited 0 at exact source
  `026a97804bb361c173e2b473e7a61f2e6bf02d82`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68129` (`26.4.0.68129`).
- `cmake --build build-Darwin-ninja-os-arm64 --target almetal_tests
  llrender_gl_fresh_tests llrender_gl_shared_tests newview_tests -j 10`
  exited 0 and linked the source-exact RelWithDebInfo physical test binaries.
- With `MTL_DEBUG_LAYER=1`, the complete source-exact `almetal_tests` binary
  passed 52,221 assertions across 52 cases with Metal API Validation enabled.
- With `MTL_DEBUG_LAYER=1`, the source-exact `llrender_gl_fresh_tests` binary
  passed 1,499 assertions across 87 cases with Metal API Validation enabled.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68129.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --target
  appearance-utility-bin -j 10` exited 0 with warnings as errors at source
  build 68129.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `1de4c5c6cdf278c44949b6b409593b62e4c3928f042c68749eab38c7ac693dc7`.
- Appearance-utility SHA-256:
  `a53d96036859d90452b133820a11ba1038312c1dd0eefdcc031af3694e824000`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory is 2,408 files / 3,248 findings and exits 1 as expected. Its
  increase of four is exactly three new `LLImageGL` method definitions plus one
  `LLImageGL*` typed use classified by the scanner's symbol heuristic; no GL
  call was added. The ignored exact report is
  `artifacts/metal/gl-strict-native-cpu-media-upload.json`.
- The packaged viewer has no undefined `CGLTexImageIOSurface2D` dependency.
  `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer
  and source-exact appearance utility. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Inventory production Darwin callers of `LLImageGL::createGLTexture` and
`setImage`, beginning with `LLViewerFetchedTexture::createTexture`, and classify
their retained GL consumers versus native sampling consumers from source. Give
the first fully native-consumed owner an explicit Metal allocation/upload/
publication contract, remove its duplicate Darwin GL authority, and add exact
byte/ordering regressions without launching either viewer.
