# Native Developer buffer visualization

**Source base:** `8de5f88ce9d76030a9d4c4b26b073ddb8c7599a8`, 2026-11-11.
**Implementation checkpoint:** `a5425524eeb85e340d491a818a91d4b0f375c105`.
Owner goal remains active. No viewer launches or screenshots are authorized.

## Original contract inspected before implementation

Reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`, `pipeline.cpp`
`renderFinalize`/`visualizeBuffers`, `postDeferredVisualizeBuffers.glsl`,
`alsamplerstate.cpp` and the exposure-meter resource descriptor:

- Order: tonemap/grade, AA, CAS, legacy Glow, RLVa sphere, selected-buffer
  replacement, scopes/reference capture, final lens/print transfer, UI.
- Modes0–3 sample raw albedo/specular-or-ORM/normal/emissive RGBA. The bilinear
  mirrored sampler decodes sRGB albedo; ordinary data formats remain raw. No
  normal interpretation, grayscale mapping or added gamma transfer.
- Mode4 samples the128x72RGBA16F spatial grid: ground luminance, sky luminance,
  sky fraction, one. Requested LOD8 clamps to the single allocated level0.
  Meter generation precedes and is independent of automatic adaptation and
  classic-sky fixed exposure. HDR-off retains previously produced grid data;
  initial non-HDR content has no defined original pixel contract.
- Modes5/6 consume edges/weights only if SMAA actually produced them. FXAA and
  AA-off do not force SMAA. Cutout skips all visualization; No Post-processing
  does not separately suppress it. Negative/unknown modes leave normal output.
- The normal post target is UNORM (HDR10-bit, otherwise8-bit), so raw values
  clamp/quantize there before final effects. Preserve alpha and texture rows.
  No additional vertical flip or display sRGB encoding belongs in this copy.
- Emissive-disabled mode3 has no valid original attachment/output contract.
  An absent optional source leaves the native completed frame intact instead
  of retaining yesterday's whole world or inventing a diagnostic source.

## Native ownership and changes

`ALMetalWorldGBufferRenderer::renderDebugBuffer` encodes the direct textureLod
copy with explicit output format, bilinear mirrored nearest-mip sampler and
finite/format/shape validation. It joins the existing world command batch;
encoding failure withholds publication. The native pipeline removes the stale
selected-buffer preflight exclusion and inserts the copy after RLVa, before
reference/scopes. Final presentation and world depth remain with their owners.

SMAA can return its actual intermediate owners only after the entire three-pass
chain succeeds. Failed calls clear stale diagnostic output. Exposure can return
its actual spatial meter alongside adapted EV. A shared meter-only path runs
when adaptation is disabled; no extra EV history is created or advanced.
The retained grid survives temporal-history/HDR state changes like the original,
but device/renderer replacement and failed command batches clear it. Independent
review caught and removed an overbroad temporal-history reset before handoff.

Remove the displaced GLSL visualization program, fragment file and old draw
path. Also remove unused `LLImageGL::checkTexSize`: original and current source
have no executable callers; it contained two GL introspection calls. Keep actual
storage/allocation/default-image/texture-audit state. Deleted code is recoverable
from Git; no called original diagnostic feature was deleted.

## Validation and limits

M1 Max/64GiB/macOS26.5.2/Xcode26.6. Ignored evidence:
`artifacts/metal/feature-completion-20261111/debug-buffer-*`.
Doctor/configure, native test builds and both full/final incremental app builds
exit0. Release bundle26.4.0d68651 is built from base8de5f88ce9 plus this checkpoint's
changes, never launched. Deep strict codesign verification exits0. ARM64 slice
inspection exits0 with349 Mach-O files/zero invalid slices, recorded in
`debug-buffer-verify-app.log`.

With `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`, the six new cases plus existing
exposure/SMAA/display-post regressions pass59386assertions/11cases in
`debug-buffer-regressions.log`. Commands:

```sh
cmake --build build-Darwin-ninja-os-arm64 --config Release --target almetal_tests llrender_tests --parallel 4
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/almetal/Release/almetal_tests '[debug-buffer],[debug-owner],almetal: world exposure matches the production meter and history,almetal: world SMAA edge and complete transaction match reference,almetal: world SMAA blending weights match lookup reference,almetal: world SMAA neighborhood blend matches linear-light reference,almetal: world display post owns the real post target formats'
tools/metal/build.sh configure
tools/metal/build.sh build
tools/metal/verify_app.sh 'build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app'
```

Initial test run exits42 with6 fractional RGB10 interpolation failures/37659
assertions. An independent compute kernel bypassing production vertex/fragment
and output packing reproduces the sampling precision; its initial overstrict
float midpoint check exits42 with3 failures/37726assertions (RGB0.50012213 for
ideal0.5). Control compilation also exposed two unparenthesized Catch INFO
ternaries, corrected in the test only. Preserve all failed receipts. The final
test bounds only fractional RGB10 analytic interpolation (3codes) and observed
float-control precision; actual output must still match the independent control
within1code. Raw texel/mip/alpha/endpoint/midpoint output checks remain strict.
These are measured M1 sampling observations, not a universal precision claim.

Final executable SHA256:
`2cf230b3604447ae9ed3dad8ec774d3300343b93bfaebc7a9c8fe18d2a7cd5d8`.
Bundled metallib SHA256:
`58dc6b62a7450b0923096378485ecb4597d61932d157ba48a610de6838c924ea`.
Final executable framework/undefined-symbol receipts find no OpenGL framework
or GL/CGL/SDL_GL imports. Strict source audit still exits1:1783findings/2406files,
including333call-syntax findings; names and dormant GL code both remain.

The new fixture uses authored pixels/analytic expectations for sRGB decode before
filtering, raw normals, clipping, alpha, row orientation, nearest/clamped mip
selection, both post formats and invalid-input rejection without GPU submission.
Ownership cases check the full spatial ground/sky grid, adapted EV invariance,
SMAA diagnostic bytes against standalone stages, unchanged final output and
failure clearing. They do not execute menu clicks or viewer presentation.
Source reviews cover the original selection/order and lifecycle bridge.
No live feature parity or performance improvement claim.

The [appearance utility loader](appearance-native-loader.md) was also corrected
when this dependency audit found its remaining live GL compiler calls.
Next continue dormant GLSL compiler/linker and binary-cache removal, preserving
native metadata/uniform and shader-reload/cache-purge contracts.
Strict source/runtime no-GL and all other product gates remain open.
