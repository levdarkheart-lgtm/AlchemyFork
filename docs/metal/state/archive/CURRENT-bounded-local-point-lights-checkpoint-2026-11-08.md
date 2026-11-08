# Bounded ordinary deferred point lights checkpoint — 2026-11-08

## Finding classification

**Confirmed.** Before commit `8247d09c5d77f6fcb82a8f1451aaba40953230fe`,
the native deferred path collected every qualifying ordinary point light into
one array, issued one full-target triangle, and looped over the complete array
inside each covered geometry fragment. The distance rejection in the fragment
shader preserved lighting appearance but did not restore the original
screen-space coverage or per-draw light-count bound.

This classification does not claim that ordinary lights caused the recorded
Ultra frame rate. The settled Release `68521` trace labelled local-light work at
`3.566 ms/present`, but its live scene, light population and capture do not
isolate this defect.

## Exact-original contract

The required reference is commit
`1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, not a current upstream viewer.
Its `indra/newview/pipeline.cpp` blob is
`5d41e915d509d95b454762f9a8528c21fba6b98f` (content SHA-256
`bb52adcb83cf493d57246b6410294bb8579f429454b024aeae1ae66efa23231b`).
The ordinary-light branch at lines 11981–12006 establishes two paths:

- if the camera is outside the light's radius-expanded axis-aligned bounds,
  draw the light's cube volume immediately; and
- if the camera is inside those bounds, append the light to a full-screen
  batch capped at `LL_DEFERRED_MULTI_LIGHT_COUNT` (`16`).

The corresponding point- and multi-point-light fragment shader blobs are
`9138570bcf02043d424699932e2d9461f62d42a4` and
`a8e0c49cfdd044c77ab22034f3a335565da13f3b`. The original therefore rejects
most off-volume pixels by raster coverage and bounds only the camera-inside
full-screen loop.

## Native repair

Commit `8247d09c5d77f6fcb82a8f1451aaba40953230fe` implements the smallest direct
Metal equivalent at the existing local-light owner:

- `pipeline.cpp` preserves the exact original camera-inside/outside predicate,
  including its `0.2` margin, and computes a conservative projected cube
  rectangle with the projector path's already-proven eye-plane fallback;
- camera-outside ordinary lights are stored first with that rectangle, while
  camera-inside lights follow and retain full-target coverage;
- `ALMetalWorldRenderer` encodes one scissored draw per bounded light, using a
  one-light buffer range, then full-target batches of at most 16 inside lights;
- standalone accumulation preserves first-draw replacement followed by
  additive blending; production deferred accumulation remains additive; and
- the Metal ABI carries an explicit `uint4 screen_rect`; fragment lighting
  equations and distance attenuation are unchanged.

This deliberately uses conservative scissors rather than a speculative tiled
or clustered rewrite. A pixel outside a bounded light's projected volume now
runs no fragment work for that light; a covered bounded pixel examines one
light; only camera-inside lights use the original maximum-16 full-screen batch.

## Regression proof

The focused real-device fixture now includes a camera-outside bounded light and
a following camera-inside light. It proves that the bounded receiver is
unchanged, a receiver outside the bounded rectangle remains unlit, and the
full-screen light contributes exactly once after the scissored draw.

Commands and results:

- Release and RelWithDebInfo `almetal_tests` targets build successfully.
- With `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`, the focused
  `almetal: native world G-buffer feeds exact deferred point lights` case
  passes `1,990/1,990` assertions in both configurations.
- The full validated Release real-device Metal suite passes
  `52,861/52,861` assertions across 59 cases.
- Ninja Release `alchemy-bin` builds and stages; `verify_app.sh` passes
  349/349 ARM64 slices.
- `tools/metal/build.sh configure` and
  `AL_METAL_JOBS=10 tools/metal/build.sh build` both exit 0; the latter reports
  `** BUILD SUCCEEDED **` with warnings as errors.
- The source-exact Xcode Release bundle passes deep strict signing and 349/349
  ARM64 verification.

## Source-exact artifact

- App: `build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app`
- Version: `26.4.0.68528`
- Embedded revision: `8247d09c5d77f6fcb82a8f1451aaba40953230fe`
- Executable SHA-256:
  `6382a4758a13b8f7fc725bd39184704a5c3515b7bf131e8166131b3fe8987547`
- Packaged `almetal_builtin.metallib` SHA-256:
  `4f4778e6d6558654b52986ac4caa69b42add1ec1fe940bed331844a9f1ad97bb`

No viewer was launched, logged in or captured in this checkpoint. Visual
no-popping coverage and a fully resident, matched local-light performance trace
remain owner-attended work. No FPS improvement is claimed from build or GPU
test success.
