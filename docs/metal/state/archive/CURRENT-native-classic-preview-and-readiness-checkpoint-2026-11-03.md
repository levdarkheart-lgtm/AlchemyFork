# Native classic material preview and readiness cutover checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Classic renderer commit:** `24b50cc382f04ed8d598b25bbfe388cbc07e35fd`  
**Readiness cutover commit:** `ab90edefd033abc24d6d535740f422ee4589ed0a`

## Product movement

The native material-preview owner now covers classic as well as modern sky
lighting. Its fragment path preserves the retained `CLASSIC_MODE` contract:

- atmosphere ambient remains sRGB until classic recombination;
- the preview's white sun takes the selected sky sunlight scale and classic
  1.35 boost;
- the legacy 0.7/PI directional reconstruction and two 1.1 scales occur in
  the retained order;
- diffuse SH probe irradiance is suppressed in classic mode while radiance
  remains available to specular IBL;
- the current environment block supplies the exact HDR/non-HDR sunlight scale.

The scheduler now distinguishes normal producer readiness from failure.
Missing in-flight material texture mirrors or a not-yet-published coherent
default probe remain `PENDING` and retry targetlessly. They do not increment
the retained-preview demand or allocate the GL probe/post pack. Device,
destination, matrix, geometry, native renderer, encode and logical-texture
publication failures remain explicit one-shot retained recovery conditions.
No supported preview configuration selects GL as its production owner.

This advances M-008 and M-011 but is not a no-context viewer claim. Actual
preview failure recovery, other dynamic textures and broad world traversal
still contain retained OpenGL authority.

## Physical and build evidence

- `tools/metal/doctor.sh` exited 0 on macOS 26.5.2, arm64
  `MacBookPro18,2`, Apple M1 Max, 64 GiB unified memory, Apple7 support and
  `recommendedMaxWorkingSetSize=62277025792`.
- `cmake --build build-Darwin-ninja-os-arm64 --config Release --target
  almetal_tests` compiled the built-in MSL, native preview renderer and GPU
  fixture warnings-as-errors.
- With `MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1`, load-action validation
  and `MTL_DEBUG_LAYER_ERROR_MODE=nslog`, focused `*preview*` coverage passed
  459 assertions across two cases. The complete physical Metal suite passed
  52,414 assertions across 55 cases.
- The new classic cases independently pin the 68.48-byte encoded direct-light
  reference at normal incidence and inject a strong green L0 SH coefficient
  while proving classic diffuse still follows the atmosphere-authored RGB
  ambient rather than probe irradiance.
- Exact RelWithDebInfo `llgltfmaterialpreviewmgr.cpp` and `pipeline.cpp`
  objects compiled warnings-as-errors after the readiness split.
- `build-Darwin-ninja-os-arm64/alrender/Release/alrender_tests
  '[alrenderresourceplan]'` passed 3,376 assertions across 37 cases. The full
  renderer suite passed 557,312 assertions across 137 cases.
- The full `newview_tests` binary passed 98,973 assertions across 743 cases:
  742 passed and the established CRT generator case skipped.
- `tools/metal/build.sh` exited 0 at exact head `ab90edefd0`, producing
  Development build `26.4.0d68188` (`26.4.0.68188`).
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/
  Alchemy Metal Development.app"` checked 349/349 bundled Mach-O slices as
  ARM64.
- Viewer executable SHA-256:
  `e23aed414f4433469260bb024c230ff9d0badc1081800738c336e0d2a4d7ea8d`.
- Source and packaged MSL are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- `file`, `otool -L` and `nm -u` confirm the viewer is arm64, has no undefined
  `CGLTexImageIOSurface2D`, and still links both Apple OpenGL and Metal.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-preview-readiness-cutover.json` exited 1 as
  expected at 2,408 files / 3,262 findings. Report SHA-256 is
  `462ca6658bdddfa0a760200742e3ac75b22166b7a1ea12cd5a8096c3764b9c07`.

## Visual and performance status

No viewer executable was launched, driven, photographed or compared. Queue row
17 now names build 68188 and includes classic lighting plus mode/texture-load
refresh. It remains open for an owner-attended daily review. No paired
quality-equivalent performance samples were collected and no performance claim
is made.

## Next executable boundary

Remove the now-isolated Darwin preview failure-recovery sphere/post transaction
and its retained resource demand while preserving fail-closed native retries
and diagnostics. Broader native pipeline lifetime under GL teardown remains a
separate open boundary.
