# Native parcel-overlay checkpoint — 2026-11-03

## Observed source state

Commit `6bc7395fe134d09cada72f0b08d57007e031caa6` removes OpenGL
ownership and GL-lifecycle dependence from the Darwin
`LLViewerParcelOverlay` texture. Construction now preserves the established
`LLViewerTexture` identity without invoking the raw-image overload that creates
a GL texture. Completed RGBA CPU maps create native storage on first
publication and use native full-image updates thereafter.

The overlay no longer checks `gGLManager.mIsDisabled` or uses
`gPipeline.markGLRebuild` to advance rows on Darwin. Idle processing progresses
the bounded CPU rebuild directly, so a future disabled/absent GL manager does
not stall this resource. Existing retained terrain binding and explicit native
terrain/PBR-terrain captures continue to receive the same logical texture;
native capture remains fail-closed if a requested overlay lacks Metal storage.

Publication is fail-closed. A native allocation/update failure leaves the
previous complete texture visible, resets row progress to zero and clears the
native-ready flag. Idle processing rebuilds all rows, and the next publication
recreates native storage rather than repeatedly updating a missing image.

The existing physical RGBA8 native-creation regression now also writes a 6x4
subregion of a 12x10 image, reads the texture back exactly, proves all four
changed channels, proves every outside texel unchanged and rechecks that no GL
name exists. No viewer was launched or driven. Parcel-overlay appearance is
queued for owner-attended daily review rather than inferred from deterministic
validation.

## Validation evidence

- The required `tools/metal/build.sh` command exited 0 at exact source
  `6bc7395fe134d09cada72f0b08d57007e031caa6`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68137` (`26.4.0.68137`).
- `cmake --build build-Darwin-ninja-os-arm64 --target almetal_tests
  llrender_gl_fresh_tests llrender_gl_shared_tests newview_tests -j 10`
  exited 0 and linked the source-exact RelWithDebInfo physical test binaries.
- With `MTL_DEBUG_LAYER=1`, the complete source-exact `almetal_tests` binary
  passed 52,221 assertions across 52 cases with Metal API Validation enabled.
- With `MTL_DEBUG_LAYER=1`, the source-exact `llrender_gl_fresh_tests` binary
  passed 2,187 assertions across 92 cases with Metal API Validation enabled.
  This includes exact RGBA8 native creation/update/outside-region preservation
  and the prior typed allocation, partial update and mip-chain regressions.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68137.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --target
  appearance-utility-bin -j 10` exited 0 with warnings as errors at source
  build 68137.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `1f3af01e9078e0b2f4b65cd97c0aa9639042da4d68d890677fd32e8267393070`.
- Appearance-utility SHA-256:
  `5769d937be848ee0b6edf9c153eefb0b0107898f90bc42fb377399e2ff7b337e`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/validate_harness.py` passed all 32 required files, and
  `git diff --check` passed before this documentation update.
- Strict inventory remains 2,408 files / 3,258 findings and exits 1 as
  expected. The precommit and exact-source reports are byte-identical to each
  other and to the prior water report at SHA-256
  `87e5bec2875149fdb44878bad14add7bafa1651ecdf53dedcce2cbb63e1ebf44`.
  The owner cutover adds no static finding. The ignored exact report is
  `artifacts/metal/gl-strict-native-parcel-overlay.json`.
- The packaged viewer has no undefined `CGLTexImageIOSurface2D` dependency.
  `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer
  and source-exact appearance utility. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use `LLSkyTex` as the next bounded resource owner. Its CPU-generated sky and
shiny face pairs retain logical `LLViewerTexture` consumers and feed the legacy
sky/cubemap path, but `createGLImage` still publishes every completed RGBA face
with `createGLTexture`. Convert Darwin publication and restore behavior to
native storage while preserving double-buffer selection and explicit RGBA8
format. Do not launch either viewer; queue any sky appearance question for the
owner-attended daily review.
