# Native shader ownership and Developer reload — 2026-11-11

Follow-up: [native feature/resource completion](native-feature-resource-completion.md)
now integrates utility/preview reload and native Developer measurements, vertex
and manager cleanup. Its receipts and CURRENT supersede the older next actions
below. The app identity in this note remains exact for30ef2ccb69 only.

**Source base:** `a2f8b2f4c2cd3b2c902c2895014be62bcdb91595`.
**Implementation:** `a05a38d31054a4c98da2af9239e372bab0c9cfae`, corrected native
loader helper in `30ef2ccb694ee3030ff059ec00f084de3fe7c748`.
**Host:** M1 Max /64GiB /macOS26.5.2 /Xcode26.6, native ARM64.
Remaining-feature/OpenGL-removal goal stays active. No viewer or appearance
utility launch, screenshot, login, cache purge or crash action was executed.
Offscreen test processes do create real Metal devices and submit GPU work.

## Original source contracts checked before changes

Reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` establishes the shader
configuration/hash/variant graph and explicit cache purge behavior. The later
native branch introduced by `3087308155` separates semantic pass identity from
MSL libraries and pipelines; compare the immediate pre-change native path too.

`LLShaderMgr::clearShaderCache` deletes the historical app-owned `shader_cache`
directory contents. Developer menu and hero-probe workaround separately request
`Change::Programs`; app cache purge also calls the disk cleanup before loading.
Do not unload live programs from that disk-only method. No native persistent
pipeline archive exists here. Bundled metallib and system Metal caches are not
app cache files and are not deleted.

The retained traversal depends on shader identity, eight rigged/classic/mirror
corners, generations, attributes, indexed batch widths, minimum-alpha capture
and CPU environment vectors. GL compilation, reflection and uniform uploading
were already bypassed on the native path. Removing those implementations does
not implement any previously missing native uniform consumer.

Original query capabilities only selected GL query/compute implementations.
The native query pool supports visibility, not timestamp/statistics queries.
Native world/HUD texture layout widths and Apple Silicon baseline compute/depth
capabilities must not inherit zeroed GL limits.

## Implementation

Remove shared GL shader compiler/linker, object ownership, binary cache,
stand-in program, reflection, uniform upload/cache, unused link callbacks and
shader-only GL entrypoints. Remove the viewer's dormant GL loading branch and
the baking manager's displaced shader-directory override. Keep the native pass
configuration and failed-pass admission behavior, CPU uniform values and exact
14-attribute/506-uniform table order from the immediate native checkpoint.
The original reference had 547 uniform entries; earlier removed features account
for that difference, not this cleanup. Keep the legacy `isComplete()` gate false
until its remaining old post-pass callers are removed or ported; native consumers
use `isAvailable()`/`isNativeConfigured()`.

Delete the obsolete GL compiler test suite after moving meaningful native
hash/generation/variant/alpha/registry checks and CPU table checks into the native
suite. Delete the write-only engine-block registry and its five static
registration groups. Its sole reader was removed GL reflection validation.
Keep real producer structs, packing tables, CPU data and native ABI checks.
Delete the unused storage-buffer name cache: it had no production callers or
native encoder consumer, and native `getGLName()` always returned zero.

Remove dormant query/compute capability initialization, unused flags/limits and
their remaining query/resource pointers. Native diagnostics now report actual
`gRenderCapabilities` identity, budgets and dimensions, or truthful unavailable
state. They never query GL or present absent GL limits as Metal measurements.
The direct `llappviewer.cpp` getGLInfo call is under `!LL_DARWIN`; actual native
consumers include renderer error text, statistics raw identity and `asLLSD`.

Forced program rebuild now refreshes the retained world/probe-world and UI
renderers' actual Metal libraries and pipelines. Each renderer stages replacements
before publication, rejects active command transactions, and preserves external
targets, histories, pending command ownership, sampling policy and UI backing.
Rebuild already-used lazy PSOs too. Refresh the frame-data material argument
encoder cache against the new library without resetting its arena or invalidating
old pass/command leases. Failures requeue the request and withhold world
presentation until a complete frame can be produced. Per-owner staging does not
claim a global atomic transaction across every owner.

## Observed validation

Ignored receipts: `artifacts/metal/feature-completion-20261111/`.

| Command/check | Observed result |
| --- | --- |
| `tools/metal/doctor.sh` | exit0 outside sandbox; `shader-native-doctor.log` |
| `tools/metal/build.sh configure` | exit0; `shader-native-configure.log` |
| `cmake --build build-Darwin-ninja-os-arm64 --config Release --target llrender_tests almetal_tests --parallel 4` | initial exit0; `shader-native-test-build.log` |
| `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/llrender/Release/llrender_tests '[shader-lifecycle],[baking-loader],*native*'` | initial exit0;1165 assertions/21cases; `shader-native-llrender-tests.log` |
| Same validation flags, `build-Darwin-ninja-os-arm64/almetal/Release/almetal_tests '[shader-reload]'` | exit0;74 assertions/2cases; `shader-native-reload-tests.log` |
| Optional appearance utility Release build | initial exit0; `shader-native-appearance-build.log`; not launched |
| `python3 tools/metal/audit_gl.py --mode strict --output .../diagnostics-native-gl-audit.json` | exit1;1441findings/2406files,159GL-call-syntax matches; release gate remains open |
| Final native test build | exit0; `shader-native-test-build-final.log` |
| Final metadata/diagnostics/baking/native filter, adding `[alrendercapabilities]` | exit0;1347assertions/26cases; `shader-native-llrender-tests-final.log` |
| Metal-validated original world G-buffer/UI/RLVa sphere/display-post regressions | exit0;34129assertions/4cases; `shader-native-world-ui-tests.log` |
| Final optional utility Release build | exit0; `shader-native-appearance-build-final.log`; ARM64; not launched |
| `python3 tools/metal/validate_harness.py` | exit0;32 required files; `shader-native-harness.log` |
| Final strict source audit | exit1;1441findings/2406files,1282class-name and159call-syntax findings; `shader-native-final-gl-audit.{json,log}` |
| Corrected frozen `tools/metal/build.sh configure` | exit0; `shader-native-configure-final.log` |
| `AL_METAL_JOBS=8 tools/metal/build.sh build` | exit0/BUILD SUCCEEDED; source30ef2ccb69; `shader-native-app-build-final.log` |
| `tools/metal/verify_app.sh 'build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app'` | exit0;349Mach-O/zero invalid slices; `shader-native-verify-app.log` |
| `codesign --verify --deep --strict --verbose=2 <same-app>` | exit0; `shader-native-codesign.log` |

The final offscreen set totals35550assertions/32cases including reload tests;
do not add overlapping initial/final filters again. The optional utility has no
OpenGL framework or GL/CGL/SDL_GL undefined imports in its final receipts.
SHA256: `336570864d723317f0046b992df687728acc96d701e2acf830642b1475b3bc50`.
First canonical build exited65 at link: root removed the still-needed
`LLViewerShaderMgr::built` helper with the old loader range. The original
reference and immediate base both retain that CPU failure tracker. Restore it
in30ef2ccb69; preserve the failed receipt `shader-native-app-build.log`.
Final frozen configure/build0 against30ef2ccb69; Release26.4.0d68655.
ARM64 and deep strict codesign pass. Final executable imports contain no OpenGL
framework or GL/CGL/SDL_GL undefined imports. Exact identities are saved in
`shader-native-identities.json`. Viewer SHA256:
`2f7cf7417b0dfea8a30370a5817500bb9cd89a8ea656f5b6d72a44259721c9a4`.
Bundled metallib SHA256:
`58dc6b62a7450b0923096378485ecb4597d61932d157ba48a610de6838c924ea`.
These static checks do not prove runtime contexts or full native feature parity.
The reload fixture reads real GPU output before and after replacement, retains
an already submitted target, commits an old material command after replacement,
and checks an active UI/world transaction rejects reload without changing its
generation. It does not inject library/PSO compilation failure or exercise a
live Developer menu. No runtime viewer context or visual-parity proof is claimed.

## Remaining executable work

Integrate reviewed isolated utility/preview reload and vertex upload cleanup
commits pinned in CURRENT, wire revision announcement/register tests, execute
their actual GPU tests and rebuild the resulting coherent source. The verified
30ef2ccb69 checkpoint above precedes those independent changes.

The cache-purge reload still misses persistent utility/preview owners; do not
claim every application shader is refreshed. Original-reference mapping confirms
terrain bake (`llterrainpaintmap.cpp`), normal generation (`lldrawpoolbump.cpp`),
five lit/sculpt/avatar/model preview owners and PBR material preview use global
programs reloaded by the original viewer. Their independent cached native
libraries currently survive the forced rebuild. Replace their executable owners
without discarding generated images or external preview textures; recompute the
PBR preview's shader-generated BRDF LUT. Also handle retained background scaling,
LLImageGL and CEF resampler caches. Per-call render-target resamplers are already
fresh. Optional literal-source probe-workload diagnostics are independent and
preserved; shadow indirect commands own no library/PSO and use the rebuilt world
encoder pipeline. Frame-data shadow material caches are refreshed explicitly.

Developer Frame Profile remains a real missing feature: native timing/statistics
query spans are unsupported, so old reports produce zero measurements and can
divide by zero. Do not activate metadata-bind hooks or substitute CPU timing.
Trace original display scope (main, hero probes, UI; ordinary probes update after
display) and collect at actual Metal encoder/pipeline boundaries, reusing native
stage timestamp ownership and explicit unavailable-counter states. Formal
benchmark warmup gates do not belong in a developer one-shot action.
Root independently compiled/ran a device-only capability probe: M1 Max exposes
only timestamp/GPUTimestamp and stage-boundary sampling; draw/dispatch/blit/tile
sampling is false, no statistics set exists. Evidence/source:
`frame-profile-counter-capabilities.log`, `frame-profile-counter-probe.mm`.
The isolated Frame Profile worktree is implementing actual physical render/
compute intervals with explicit unavailable per-pipeline/statistics fields.

Continue dormant GL manager/loader, vertex-buffer and image tails using actual
callers and the original reference. The strict inventory includes compatibility
names, comments and excluded platform code; it is not a live API count and does
not cover every owned directory. Full feature parity, strict source/runtime
no-GL, prolonged stability and performance release gates remain open.
