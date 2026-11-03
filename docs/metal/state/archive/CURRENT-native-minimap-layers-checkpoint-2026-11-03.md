# Native minimap-layer checkpoint — 2026-11-03

## Observed source state

Commit `afda818d2d428c3b8372a53837bb74a85a3917da` removes OpenGL
ownership from the Darwin `LLNetMap` object and parcel layers. A resize now
creates the established logical `LLViewerTexture` objects without generating GL
storage. The first complete CPU-painted image creates explicit linear RGBA8
Metal storage; later complete images update the native texture in place.

Existing dynamic power-of-two sizing, scale calculation, object refresh timer,
parcel displacement/update trigger and sampled-texture call sites are retained.
The native UI renderer already consumes the same logical texture identity.

Publication is non-destructive. If a native create/update fails, the prior
complete native texture remains the sampled image and its prior global center
is restored. The update flag remains set, so the next draw repaints and retries
instead of showing old pixels at a newly advanced center. A successful upload
alone advances the visible center and object timer.

No viewer was launched or driven. Object/parcel minimap appearance, panning,
recentering, update and resize behavior are queued for owner-attended daily
review rather than inferred from deterministic validation.

## Validation evidence

- The required `tools/metal/build.sh` command exited 0 at exact source
  `afda818d2d428c3b8372a53837bb74a85a3917da`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68141` (`26.4.0.68141`).
- `cmake --build build-Darwin-ninja-os-arm64 --target almetal_tests
  llrender_gl_fresh_tests llrender_gl_shared_tests newview_tests -j 10`
  exited 0 and linked the source-exact RelWithDebInfo physical test binaries.
- With `MTL_DEBUG_LAYER=1`, the complete source-exact `almetal_tests` binary
  passed 52,221 assertions across 52 cases with Metal API Validation enabled.
- With `MTL_DEBUG_LAYER=1`, the source-exact `llrender_gl_fresh_tests` binary
  passed 2,191 assertions across 92 cases with Metal API Validation enabled.
  Existing physical coverage proves explicit linear RGBA8 native descriptors,
  exact all-channel subimage update/outside preservation and no GL name.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68141.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --target
  appearance-utility-bin -j 10` exited 0 with warnings as errors at source
  build 68141.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `96187c4eb4f7969ff64b917839064dbacdad5ab9d116841db0e1632a475d6d1c`.
- Appearance-utility SHA-256:
  `5769d937be848ee0b6edf9c153eefb0b0107898f90bc42fb377399e2ff7b337e`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/validate_harness.py` passed all 32 required files,
  `python3 -m unittest discover -s tools/metal/tests -v` passed 27/27 and
  `git diff --check` passed before this documentation update.
- Strict inventory is 2,408 files / 3,259 findings and exits 1 as expected.
  The minimap owner adds no finding. Precommit and exact-source reports are
  byte-identical at SHA-256
  `1b1c1c444707ce6b14f60326a6eb10d322658380a189dfc9bb2a74ea103fdbce`.
  The ignored exact report is
  `artifacts/metal/gl-strict-native-minimap-layers.json`.
- The packaged viewer has no undefined `CGLTexImageIOSurface2D` dependency.
  `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer
  and source-exact appearance utility. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use `LLFontBitmapCache`/`LLFontFreetype` as the next bounded UI resource owner.
Native UI already consumes each atlas `LLImageGL`, but a new sheet is still
constructed from its raw image through GL and each rasterized glyph uses GL
`setSubImage`. Convert Darwin sheet creation and dirty-rectangle update to
native storage while preserving grayscale luminance-alpha swizzle, color BGRA
bytes, page eviction/recycling, CPU-only test mode and retry semantics. Do not
launch either viewer; queue font appearance only after deterministic source and
physical validation exists.
