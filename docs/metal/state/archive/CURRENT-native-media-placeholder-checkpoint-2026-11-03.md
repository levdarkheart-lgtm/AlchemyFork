# Native media placeholder checkpoint — 2026-11-03

## Observed source state

Commit `26801e499ffdf5f895daae4b6659156bfb2e6588` removes the Darwin
media texture's initial OpenGL placeholder allocation. `LLImageGL` and
`LLGLTexture` now expose a main-thread native allocation/upload contract that
creates a populated, non-mipmapped Metal image, records its dimensions, format,
category and memory accounting, and owns no GL texture name.

The media owner uses that contract before either accelerated IOSurface ingress
or ordinary CPU-painted CEF/VLC frame publication. A native-only image remains
the logical texture bound in `ALTextureSlot`; while retained GL traversal still
runs, the physical GL slot uses the established white/default placeholder. This
preserves the native image identity for Metal encoding without inventing a
duplicate GL copy. A change of native logical identity flushes queued geometry
before slot bookkeeping changes. Raw `LLImageGL`, wrapped `LLTexture`, slow-bind
and fast-bind paths are physically regressed.

The implementation added no GL call site. During review, two existing
`glBindTexture` sites were factored to accept the selected fallback name; the
strict scanner therefore shows those two old snippets removed and two
equivalent snippets reworded. Its net increase of six findings consists only of
`LLImageGL`/native-binding symbol matches, including the new Metal creation
method, not new GL calls.

Moving CEF/VLC appearance remains in the existing owner-attended daily review
queue. No viewer was launched or driven for this checkpoint.

## Validation evidence

- The required `tools/metal/build.sh` command exited 0 at exact source
  `26801e499ffdf5f895daae4b6659156bfb2e6588`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68131` (`26.4.0.68131`).
- `cmake --build build-Darwin-ninja-os-arm64 --target almetal_tests
  llrender_gl_fresh_tests llrender_gl_shared_tests newview_tests -j 10`
  exited 0 and linked the source-exact RelWithDebInfo physical test binaries.
- With `MTL_DEBUG_LAYER=1`, the complete source-exact `almetal_tests` binary
  passed 52,221 assertions across 52 cases with Metal API Validation enabled.
- With `MTL_DEBUG_LAYER=1`, the source-exact `llrender_gl_fresh_tests` binary
  passed 1,527 assertions across 90 cases with Metal API Validation enabled.
  This includes native allocation, byte readback and raw/wrapped slow/fast
  sampled-binding regressions.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68131.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --target
  appearance-utility-bin -j 10` exited 0 with warnings as errors at source
  build 68131.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `ad62e9b12d9c929e213481015ff24cc5610d60e1c9a4342e81d7f5c42d493fd5`.
- Appearance-utility SHA-256:
  `4c0a61cfff3e1ddbd8f3307fb75da958b9cc89ea83e62e087fc4400297b4e9fb`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/validate_harness.py` passed all 32 required files, and
  `git diff --check` passed.
- Strict inventory is 2,408 files / 3,254 findings and exits 1 as expected. As
  described above, the six-finding increase is symbol-heuristic bookkeeping;
  the two affected GL binding calls are semantically unchanged. The ignored
  exact report is
  `artifacts/metal/gl-strict-native-media-placeholder.json`.
- The packaged viewer has no undefined `CGLTexImageIOSurface2D` dependency.
  `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer
  and source-exact appearance utility. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use `ALTerrainSurfaceMaps` as the next bounded resource owner after confirming
all live consumers. Its height and composition maps already expose native
Metal textures, but their upload path still creates GL storage and generic
`setSubImage` rejects a Metal-only image before reaching its Darwin update
logic. Give this owner an explicit native region-allocation/update contract,
remove its Darwin duplicate GL authority, and physically prove full and partial
updates without launching either viewer.
