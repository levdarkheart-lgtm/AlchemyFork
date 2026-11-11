# Remaining feature port — 2026-11-11

**Source base:** `bccbb7be48000fab91b43688bc585f2f9ab474cf`.
**Implementation checkpoint:** `71f0a584038012fc6b03c29336936a5735fba692`.
**Host:** M1 Max / 64 GiB / macOS26.5.2 / Xcode26.6; ARM64 native.
The owner resumed remaining-feature and OpenGL-removal work, explicitly including
RLVa and Debug/Developer items. Earlier deferral of this work is superseded.
No viewer launches, screenshot comparisons or signed-in state changes are allowed.

## Original behavior mapped before implementation

- [RLVa ownership](rlva-native-owner-cleanup.md): original `dd9aee0de1^` samples
  effect parameters and view-space origin per execution. Preserve enabled-effect
  priority order, retained depth, five modes and separable horizontal/vertical
  blur. Remove the displaced shader/target path and its GLSL sources; native
  execution alone owns sphere output. Remove the erroneous frame-only cache.
- [Developer diagnostics](developer-metal-diagnostics.md): original `1eb44bbd1b`
  consumes the next-session debug switch before window creation. Request Metal
  API validation before the first device, consume old saved requests, and retain
  `--debugsession`. Explicit driver-crash testing now enters Metal with invalid
  CPU client memory and an abort fallback; no GPU work is submitted. This crash
  action was not executed.
- Emissive/Glow: original `1eb44bbd1b` `gbufferUtil.glsl` initializes emission to
  zero and samples it only under `HAS_EMISSIVE`; `softenLightF.glsl` adds this to
  PBR deferred lighting. Alpha and HUD shaders add emission inline irrespective
  of this setting. Native base lighting now carries independent suppression in
  the previously reserved `environment_config.y`; HDR storage is independent.
  Keep emission data resident, including cached impostors, so off/on toggles
  cannot discard information needed when emission is reenabled. The native
  `getGBuffer` equivalent consumes zero while disabled. Alpha/HUD ABI and shader
  paths are untouched. Glow's callback uses native-aware `isAvailable()` for
  both metadata programs rather than GL-only program completeness.
- Build No Post-processing: original `1eb44bbd1b` final display and bloom use
  the option only with an available Build floater. The native post owner
  already applies that condition; remove its stale unconditional rejection in
  presentation preflight so the native tonemap-bypass variant can execute.
- Dead depth-bounds owner: original `211a6faec3` / upstream `65c39b7bad` used
  `ALDrawnOnly`/`ALDepthBounds` in full-screen haze and light volumes. Actual
  consumers were replaced in `1f453941fb`, `e6f54ce64d`, `1f3cfeefb6`. Native
  depth/stencil classification, sky rejection and light-radius checks remain.
  Delete the unused GL unit, its GL-only test, CMake entries and obsolete
  setting. Removed files remain recoverable through Git.

## Executed checks

Ignored evidence directory: `artifacts/metal/feature-completion-20261111/`.

| Command / check | Observed result |
| --- | --- |
| `tools/metal/doctor.sh` | exit0 outside sandbox; actual M1 Max and toolchain confirmed. Initial sandbox attempt denied hardware sysctl. |
| `tools/metal/build.sh configure` | exit0; `configure.log` |
| `tools/metal/build.sh build` | final exit0 / BUILD SUCCEEDED; `build-final.log`; earlier compile failure preserved in `build.log` |
| `tools/metal/verify_app.sh 'build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app'` | exit0;349 Mach-O files /zero invalid slices; `verify-app.log` |
| `codesign --verify --deep --strict --verbose=2 <same-app>` | exit0; `codesign.log` |
| `cmake --build build-Darwin-ninja-os-arm64 --config Release --target almetal_tests llrender_tests --parallel 4` | exit0; `test-build.log`; updated HDR/SDR fixture rebuild also0 in `test-rebuild.log` |
| `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 .../llrender/Release/llrender_tests '*native*'` | exit0;910 assertions/17cases; `llrender-tests.log` |
| Same validation flags with `.../almetal/Release/almetal_tests '[emissive-toggle],almetal: RLVa vision sphere owns every post mode,almetal: world legacy glow owns non-HDR extract blur and combine,almetal: world simple pass writes the native four-attachment G-buffer,almetal: production UI renderer consumes the viewer SoA layout'` | exit0;38307 assertions/5cases; `almetal-tests.log` |
| Same flags with `.../almetal/Release/almetal_tests 'almetal: world display post owns the real post target formats'` | exit0;1824 assertions/1case including tonemap bypass; `display-post-tests.log` |
| Standalone actual-device diagnostics probe | API-validation arm creates MTLDebugDevice; control creates AGXG13XDevice; both0; details in linked diagnostics note |
| XML parsing and independent source reviews | exit0 / no blocking defects; one misplaced ABI comment corrected |
| `python3 tools/metal/audit_gl.py --strict --output .../gl-inventory-final.json` | exit1;1792 findings/2406 files, down from1815/2408; strict gate remains open |

Initial sandbox app/test builds failed on Xcode/package and Metal module-cache
access; approved outside-sandbox retries were used. These were environmental
failures, not successful builds. No tool sources changed.
Root independently reran both safe device-probe arms: both exited0, confirming
the child's validation-enabled/normal-device observations. No viewer/window.
The first full app compile then failed because RLVa's camera type had depended
on the removed pipeline include. Add the explicit `llviewercamera.h` include;
the final app rebuild succeeds and is recorded separately rather than hiding
that failure. Final source content matches implementation commit71f0a58403;
configuration/build began at basebccbb7be48 plus those changes. App bundle
version is26.4.0d68649, never launched. Main executable SHA256:
`6c2e77a1f921630905a7bd0f0439aa15c57c589eda82f9d5ee67ea5e90565c41`.
Bundled metallib SHA256:
`2fad1e9530dfa8075bf28e073923565202cd0ec0ec89c108ca46bf8f157efe69`.
The final executable's `otool -L` and `nm -u` receipts show zero matching
OpenGL/Vulkan/MoltenVK/ANGLE framework imports and zero GL/CGL/SDL_GL imports.
This does not rule out defined dormant GL pointers, foreign subprocess graphics
or runtime contexts; the strict source gate remains failed.

The extended emissive fixture reads actual GPU output against analytic values
for HDR/SDR, forward/reverse depth, classic/nonclassic lighting, switch off/on,
legacy fullbright/environment guards and clear-depth rejection. Existing GPU
fixtures cover five RLVa modes and non-HDR Glow math. These do not execute
viewer menu clicks, RLVa commands or settings persistence. Live parity remains
unverified under the owner's no-launch constraint. No performance claim.

## Product advancement and next implementation

M-008/M-011/M-013 advance through corrected native policy consumption and removal
of displaced GL execution. M-009 core parity and strict native-only release
gates remain open. Current executable import audit found no linked OpenGL
framework or unresolved GL API imports, but `llgl.cpp` still defines dormant GL
function pointers and owned sources retain real dead GL implementations. This
does not establish source removal or runtime context proof.

Next implement `RenderBufferVisualization`: `canCaptureMetalWorldPresentation`
still rejects every selected buffer. The old `visualizeBuffers` and GLSL
program are the sole consumer; native presentation can retain old world output
instead of showing the requested view. Trace original `1eb44bbd1b` finalization,
all attachment/luminance/SMAA modes and scope/reference/UI order; replace this
with explicit native resource consumption and independent GPU oracles. Then
continue dormant GLSL compiler/linker/binary-cache removal in
`llglslshader.cpp`/`llshadermgr.cpp`, preserving native metadata/uniform contracts.
Preserve native shader-cache purge/rebuild behavior for Developer, mirror-reset
and purge-all callers; account for the disabled optional appearance utility's
`LLBakingShaderMgr` dependency before removing shared compiler APIs. The unused
`LLImageGL::checkTexSize` still contains GL introspection with no call sites in
the inspected source; remove that dead method in the same source-tail cleanup.
Do not declare all Debug/Developer or RLVa behavior complete from these checks.
