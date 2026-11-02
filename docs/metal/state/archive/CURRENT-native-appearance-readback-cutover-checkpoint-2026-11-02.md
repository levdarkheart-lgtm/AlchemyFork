# Native appearance readback cutover checkpoint — 2026-11-02

## Observed source state

Commit `6d82ef5405096cf108fa8a1419f72d16599697c0` removes the remaining
direct framebuffer reads from retained appearance behavior. Main-viewer avatar
morph-mask generation and the separate appearance-baking executable now use an
ordered native RGBA8 readback checkpoint; neither has an OpenGL fallback.

`ALMetalUIRenderer::readFrameColorRGBA8` is valid only for an active offscreen
RGBA8 capture. It finishes and submits the current ordered output, synchronously
reads the requested region, retains the completed output as the next draw's
background and allocates a distinct destination before drawing resumes. The
checkpoint never aliases the sampled background and render target. Frame-end
paths release its internal retained owner.

`LLRender::readMetalUITextureCaptureRGBA` flushes immediate-mode draws before
delegating to that checkpoint. A failed or undersized request marks the capture
incomplete. `LLTexLayer::readBackAlpha` reads exact `width * height * 4` native
storage and extracts alpha; failures now propagate through morph-mask render and
gather calls, and no failed result is published to the cache or bake output.

`LLBakingTexLayerSetBuffer` begins a native RGBA8 capture, reads the complete
color result at the exact ordered boundary, gathers masks with propagated
failure and requires the capture to finish complete. The Darwin headless window
owns a Metal device and scheduler without a drawable surface, and the appearance
utility publishes those owners to the render and image systems for its lifetime.
The utility links `almetal` explicitly.

Exact production searches show no `glReadPixels`, `glGetTexImage` or
`glGetTexLevelParameter*` in `indra/llappearance` or
`indra/llappearanceutility`. Repository-wide production source has no direct
`glReadPixels` call; the only textual match is an explanatory comment in
`llviewerwindow.cpp`. Direct texture-image reads remain in `LLImageGL` and
`LLCubeMapArray` and are the next bounded owners.

## Validation evidence

- `tools/metal/doctor.sh` passed on macOS 26.5.2, arm64, MacBookPro18,2 with an
  M1 Max, 64 GiB unified memory, Apple7-family support, Xcode 26.6 and the Metal
  toolchain present.
- The required command
  `AL_METAL_PRESET=ninja-os-arm64 AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=10 tools/metal/build.sh build`
  exited 0 at exact source
  `6d82ef5405096cf108fa8a1419f72d16599697c0`, with warnings as errors,
  producing Development build `26.4.0d68121` (`26.4.0.68121`).
- With `MTL_DEBUG_LAYER=1`, the source-exact physical Metal suite passed 52,221
  assertions across 52 cases. The focused hidden-window `lltexlayer_gl` suite
  passed 449 assertions across two cases with Metal API Validation enabled.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68121.
- A separate viewer-disabled, warnings-as-errors appearance-utility
  configuration compiled and linked `appearance-utility-bin`. `file` and
  `lipo -archs` report arm64 for both it and the packaged viewer executable. No
  complete external bake fixture was available or claimed at this checkpoint.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `75fda49173bf127566b0f1e0698222128c3ca8e00e72d378618188b2901e30b1`.
- Appearance utility SHA-256:
  `9ea644c412a34cd27cc80f6360a27c3f514d5656a3faaedf6a2122303ac7692d`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- Strict inventory remains 2,408 files / 3,265 findings and exits 1 as expected.
  The count is unchanged because the scanner's production roots do not include
  the appearance directories. The ignored exact report is
  `artifacts/metal/gl-strict-native-appearance-readback-cutover.json`.
- `otool -L` still reports both Apple OpenGL and Metal in the viewer and
  appearance utility. The strict no-GL gate is not met.
- The content-equivalent broad `llrender_gl_shared_tests` run passed
  8,335/8,340 assertions and 181/224 cases, with four failures and 39 skips. The
  failures are the existing frame-fence pacing/budget and depth-format families
  already recorded in the native SSR checkpoint; this run is recorded as
  failed, not promoted to green.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal,
resources and direct texture-image read/copy paths remain; complete parity,
prolonged stability, independent build/review evidence, strict no-GL and
measured performance gates remain open. Appearance-utility compile/link and
hidden-backend tests do not substitute for a full out-of-process bake fixture.

## Next executable boundary

Trace all `LLImageGL::readBackRaw` callers and accepted formats. Its native
mirror path already covers uncompressed byte textures, but GL metadata,
compressed-image and fallback readback remain. Make supported Darwin behavior
require the exact native mirror while preserving fetched/sculpt saved-raw,
discard-level and failure semantics. Then replace cube-map-array growth's
all-layer `glGetTexImage`/CPU scale/`glTexSubImage3D` path with an explicit
native six-face-per-probe copy/scale.
