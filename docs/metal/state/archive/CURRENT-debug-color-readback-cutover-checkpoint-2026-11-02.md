# Debug-color native readback cutover checkpoint — 2026-11-02

## Observed source state

- Branch: `codex/apple-silicon-metal`.
- Renderer checkpoint: `a9a0edd4e2` (`refactor(metal): cut over debug color
  readback`).
- Full source SHA: `a9a0edd4e225681e49ae5b06f47284af31b70594`.
- Host: physical M1 Max, 64 GiB unified memory, macOS 26.5.2, Xcode 26.6;
  `tools/metal/doctor.sh` exited 0 and reported Apple7-family support.
- No viewer was launched, driven, photographed or compared. Visual questions
  remain in the owner-attended daily review queue.

## Source-derived contract and implementation

`LLDebugText::update()` samples `DebugShowColor` after ordinary HUD/interface
drawing and before it draws the debug-text overlay. Sampling the final retained
world/UI texture would therefore be semantically late. The native replacement
preserves that precise ordered boundary:

- `ALMetalUIRenderer::copyFrameColorPixel` requires an offscreen retained
  frame, ends the current encoder, copies one selected pixel to a private 1x1
  BGRA8 texture on the still-open frame command buffer, then resumes the same
  color/depth target with load/store actions.
- `LLRender` flushes preceding immediate vertices before taking the snapshot.
  It owns a bounded three-slot `ALMetalTextureReadbackRing`, queues the 1x1
  readback only after the exact source transaction is submitted, and collects
  completed samples without waiting. Same-device queue ordering makes the
  source copy visible to the later readback command. Historical lower-left
  coordinates and BGRA-to-RGBA presentation are preserved.
- The pipeline requests a fresh exact world/UI transaction each enabled frame
  without spending or latching the bounded presentation-probe budget. An
  incomplete transaction publishes no sample.
- `llviewerwindow.cpp` no longer owns an `ALGPUReadback` or any GL/PBO fallback.
  With no remaining production consumer, the entire `ALGPUReadback` class,
  implementation, stale comments and two GL-only test cases were removed.

The physical regression renders one opaque UI color, snapshots the center at
that boundary, then renders a second opaque color from a distinct shared
buffer. The 1x1 texture contains the first draw while the final retained target
contains the second, proving order rather than merely final-frame readback.

## Static and linked audit

```sh
python3 tools/metal/audit_gl.py --mode strict --json \
  --output artifacts/metal/gl-strict-debug-color-native-cutover.json
```

Expected exit status is 1. At committed source it scanned 2,408 files and
reported 3,266 findings, exactly three fewer than the preceding 3,269
checkpoint. Source search returns no `ALGPUReadback` reference. The remaining
production `gGL.readPixels` calls are three snapshot fallback sites in
`llviewerwindow.cpp`.

`otool -L` still reports both `OpenGL.framework` and `Metal.framework` in the
main executable, so strict native-only is not met.

## Build and physical validation

The implementation first forced and passed a broad 1,392-action dependency
rebuild with warnings as errors. After committing, the required wrapper rebuilt
the source identity and produced Development build `26.4.0d68114`
(`26.4.0.68114`):

```sh
AL_METAL_PRESET=ninja-os-arm64 \
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 \
AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=8 \
tools/metal/build.sh build
```

- `tools/metal/verify_app.sh` exited 0: 349/349 Mach-O slices are ARM64.
- Main executable file type is Mach-O 64-bit executable arm64.
- Executable SHA-256:
  `fcb6a3669e822170e408ac05d8343175dc51f121ce68ce5403f3c6b62f9f733b`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
- Packaged metallib SHA-256:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- With `MTL_DEBUG_LAYER=1`, shader validation disabled, load-action validation
  enabled and errors sent to `nslog`, the complete source-exact RelWithDebInfo
  physical Metal suite passed 52,209 assertions across 52 cases in 2.49
  seconds.
- Focused `llrender_gl_shared_tests/algpubuffer_gl_test` passed 108 assertions
  across nine surviving cases in 0.19 seconds. The removed cases tested only
  the now-dead GL readback abstraction.
- `git diff --check` passed before the source commit.

The prior unfiltered 574-entry CTest result remains 568 passes, six legacy
GL-host failures and five skips. It was not rerun for this bounded cutover and
is not represented as green.

## Next executable boundary

The only remaining production `gGL.readPixels` calls are the color/depth row
fallbacks in `LLViewerWindow::rawSnapshot` and the face fallback in
`simpleSnapshot`. Inventory every supported color/depth/depth24, RGB/RGBA,
cutout, UI/HUD, tiled and 360 contract against the existing native snapshot
owners. Add the uncovered native variants before removing GL fallback; do not
turn a retained feature into a hard failure. Continue to queue visual questions
instead of launching viewers.
