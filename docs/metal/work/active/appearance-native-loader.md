# Appearance utility native loader and failure cleanup

**Source base:** `8de5f88ce9d76030a9d4c4b26b073ddb8c7599a8`, 2026-11-11.
This optional open-source executable is retained functionality, not dead code.
`AL_BUILD_APPEARANCE_UTILITY` is off in the canonical viewer build, on in the
existing `build-Darwin-ninja-os-arm64-appearance` tree. It was not launched.

Original `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` utility loader/window and
`interface/alphamaskV.glsl`/`alphamaskF.glsl` define one interface-level2 program:
position/UV/color input, vertex-color times diffuse texture, discard below
minimum alpha, nonnegative output. Shader-ready initialization precedes baking.
The prior native window/capture/readback ports preserve this math, but the
utility loader still called `loadShaderFile`/`compileShaderFile`, reaching null
GL entrypoints after native window initialization.

Replace only that loader with native program metadata and the existing native
alpha-mask implementation. Keep reserved attribute/uniform initialization,
source provenance, explicit layout, single sampled diffuse slot and alpha-cutoff
policy. No GLSL file is read or compiled. Return success explicitly; the window
rejects failure instead of proceeding to a texture bake with no ready shader.
Remove unused shader-class bookkeeping, empty iterator/list/skip machinery,
GL shared-object requests, OpenGL include and obsolete non-Darwin window branch.

All post-window constructor failures now share destructor cleanup: unload the
program while its backend is valid, clear borrowed renderer/image/buffer device
references, then destroy the window. Normal successful initialization keeps
existing native capture/bake/readback ownership. Failure cleanup is source-
reviewed and compiled, not exercised by opening a window.

## Executed validation

M1 Max/64GiB/macOS26.5.2/Xcode26.6. Ignored evidence directory:
`artifacts/metal/feature-completion-20261111/`.

- `cmake --build build-Darwin-ninja-os-arm64-appearance --config Release --target appearance-utility-bin --parallel 4`
  exits0 for the full and final incremental source; `appearance-native-build.log`
  and `appearance-native-build-final.log`.
- `lipo -archs .../llappearanceutility/Release/appearance-utility-bin` reports
  arm64. `otool -L` and `nm -u` show no OpenGL framework or GL/CGL/SDL_GL imports.
- `cmake --build build-Darwin-ninja-os-arm64 --config Release --target llrender_tests --parallel 4`
  exits0; the test binary links the actual production baking shader loader.
- `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/llrender/Release/llrender_tests '[baking-loader],*native*'`
  exits0:959assertions/18cases; `baking-loader-native-tests.log`. The new loader
  case accounts for49assertions and configures the real loader with unset GL
  entrypoints, no GL context, a real Metal device and zero GPU submissions.
  It checks backend/initialization guards, exact native layout/provenance,
  repeated configuration, unloading/reloading and restoration of global state.
- Source diff checks and independent original-contract reviews pass.

Utility executable SHA256:
`b6995351260f120ff67feed4e06c49fbb6d21c56428ce390604b682299eb3b26`.
Verified again after the final guard-only source rebuild. Neither utility nor
viewer was launched.
No saved bake, batch bake, avatar construction, window-failure injection or
end-to-end appearance parity claim. Those remain separate unexecuted checks.

This closes the live utility dependency on shared GL compilation before its
common implementation is removed. Shared native metadata and cache/reload
consumers still require preservation in the next source-removal step.
