# Native diagnostic-readback cutover checkpoint — 2026-11-02

## Observed source state

- Branch: `codex/apple-silicon-metal`.
- Renderer checkpoint: `2a32759e7c` (`refactor(metal): require native
  diagnostic readback`).
- Host: physical M1 Max, 64 GiB unified memory, macOS 26.5.2, Xcode 26.6;
  `tools/metal/doctor.sh` exited 0 and reported Apple7-family support.
- No viewer was launched, driven, photographed or compared. Visual questions
  remain in the owner-attended daily review queue.

## Implemented cutover

`pipeline.cpp` no longer has a GL fallback for either diagnostic readback:

- A one-shot scene-pixel request now requires the same-frame,
  dimension-matched native `opaque_post_deferred_target` and calls
  `readSceneColorPixel`. A missing, stale or failed native source drops the
  request instead of calling synchronous `gGL.readPixels`.
- Scopes now require the exact same-frame native display-post source, including
  the established post-owner census. Metal point-decimates that source and an
  `ALMetalTextureReadbackRing` collects it asynchronously. The GL scope target,
  GL blit, `ALGPUReadback` ring and `requestPixels` fallback are deleted.
- Scope resize now resets native slots and stale CPU bytes explicitly. A native
  allocation failure disables scopes; an inexact frame publishes no sample.

The change deletes 114 lines and adds 47 across `pipeline.cpp` and `pipeline.h`,
plus removes the pipeline's dependency on `algpubuffer.h`. No broad traversal
suppression or feature waiver was introduced.

## Static and linked audit

```sh
python3 tools/metal/audit_gl.py --mode strict --json \
  --output artifacts/metal/gl-strict-native-readback-cutover.json
```

Expected exit status is 1. The audit scanned 2,408 files and reported 3,269
findings, unchanged from the prior checkpoint. That does not mean the removed
fallbacks survived: their PBO calls were hidden behind the shared
`ALGPUReadback::requestPixels` implementation, which remains audited because
`llviewerwindow.cpp` still uses it for `DebugShowColor`. Source search confirms
that this is now its only production caller; the remaining references are the
helper implementation and GL tests.

`otool -L` still reports both `OpenGL.framework` and `Metal.framework` in the
main executable, so strict native-only is not met.

## Source-exact build and physical validation

At committed source `2a32759e7c7faa1fd33bad56c622fc66923231c9`:

```sh
AL_METAL_PRESET=ninja-os-arm64 \
AL_METAL_BUILD_DIR=build-Darwin-ninja-os-arm64 \
AL_METAL_CONFIG=RelWithDebInfo AL_METAL_JOBS=8 \
tools/metal/build.sh build
```

The warnings-as-errors build and the subsequent source-exact no-op check exited
0 and produced Development build `26.4.0d68112` (`26.4.0.68112`).

- `tools/metal/verify_app.sh` exited 0: 349/349 Mach-O slices are ARM64.
- Executable SHA-256:
  `e9a1ab921ddac8ce088ebab03c5ff7c5ad65cc228ea174036c17b4adb888d390`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`.
- Packaged metallib SHA-256:
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- With `MTL_DEBUG_LAYER=1`, shader validation disabled, load-action validation
  enabled and errors sent to `nslog`, the complete RelWithDebInfo physical
  Metal suite passed 52,190 assertions across 52 cases in 2.71 seconds.
- `git diff --check` passed before the source commit.

The earlier unfiltered 574-entry CTest result remains 568 passes, six legacy
GL-host failures and five skips. It was not rerun for this Darwin-only pipeline
cutover and is not represented as green.

## Next executable boundary

Trace `DebugShowColor` in `llviewerwindow.cpp`: establish whether its current
framebuffer sample includes world UI, its exact call timing and coordinate
origin, then match that contract to the native presentation/world-UI producer.
Only after exactness is proved should its GL/PBO request and the production
pixel-readback helper be removed. Continue to queue visual questions instead of
launching viewers.

