# Native pipeline cache and invariant coverage — 2026-11-11

Frozen source d28b0477a038e42b5a593d0d5350588d48c62343, branch
codex/apple-silicon-metal, physical M1 Max64GiB/macOS26.5.2/Xcode26.6.
Goal stays active. Original reference1eb44bbd1bdc76d0100c75699bad1fb337e8cb45.
No viewer/appearance utility/SDL window/menu/screenshot/user-cache action.
GPU checks below use real offscreen devices and commands.

## Implementation and original contracts

Original RenderShaderCacheEnabled controlled shader-binary reuse with app/source
identity. dd9372cd69 restores the consumer through device-owned Metal archives;
53e3599a36 configures actual app/version/LL_PATH_CACHE/shader_cache and actual
second-instance/read-only policy before first PSO. Reload configures before
rebuild; shutdown flushes with device alive; clear invalidates live writers
before legacy directory cleanup. Displaced cache version setting removed.
a37f65ca0f registers native code. Unconfigured/disabled devices do no cache IO.

Bounded regular app-prefix files, canonical physical aliases and shared
purge/owner synchronization prevent stale writer resurrection. Never delete
system Metal caches or bundled metallib. Identity includes app/build, exact
loaded artifact SHA256, OS/device/family and diagnostic shader namespace.
Loader precedence stays actual bundled metallib then source fallback.

Actual M1 Max controls found loaded mixed render/compute archive extension
fails driver stage packing; separate archives pass. Failed serialization also
consumes pending driver state: retries reopen durable/new state and re-add
only retained dirty descriptors with binaryArchives cleared. Successful flush,
disable and purge release the catalog. Initial failures remain preserved.

Apple documents Shader Validation/binary archive incompatibility in
[its validation guide](https://developer.apple.com/documentation/xcode/validating-your-apps-metal-shader-usage.md).
Inspect actual PSO shaderValidationEnabled and bypass when instrumented without
disabling validation. API-only persistence and instrumented bypass are separate
real tests. Typed compute preserves ordinary computeWithFunction label behavior
in both modes; selective raw-label positive control is actually instrumented.
Registry retains the existing function-static owner after independent review.

## Native invariant admission

3caf658f78 maps old atlas cases to actual native textures:118assertions/16cases0;
only obsolete dual-backend behavior removed.4513038bcb/9831d0208d preserve old
vertex selective/interleave/resize/publication invariants:5212/2native0.
A first wrong filter ran zero/exit2; corrected [native-publication] is the real
result, with separate receipts.94fcc9ebac ports all11FreeType cases with160
assertions and actual glyph alpha/pixel/metric/GC/weight checks.
397860e2bf/d28b0477a0 extract29native image cases from62:3037assertions0;
other33legacy bodies remain byte-identical. Native workers drain/join and
command lifetimes remain owned; no hidden window fixture.

## Frozen canonical validation

Receipts: artifacts/metal/feature-completion-20261111/. Named commands/logs
exit0 unless explicitly described. Overlapping filters are never additive.

| Command or log | Observed result |
| --- | --- |
| tools/metal/doctor.sh; tools/metal/build.sh configure | Host/environment and configure0 |
| AL_METAL_JOBS=8 tools/metal/build.sh build | Release26.4.0d68712 build0 |
| tools/metal/verify_app.sh 'build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app' | 349Mach-O, zero invalid ARM64 slices |
| codesign --verify --deep --strict on canonical bundle | 0 |
| Viewer forbidden framework/import checks | No OpenGL/Vulkan/MoltenVK/ANGLE frameworks or GL/CGL/SDL_GL undefineds |
| Optional appearance Release build/signature/ARM64/import checks | All0; not launched |
| final-feature-native-test-build.log | almetal_tests,llrender_tests,alrender_tests built0 |
| final-feature-llrender-tests.log; API+shader validation | 77929assertions/312cases;311passed/one CPAL multi-palette skip |
| final-feature-native-gpu-tests.log; API+shader validation | 93869assertions/22cases0: benchmark/profile/reload/timing/indirect/G-buffer/compute/water |
| final-feature-alrender-tests.log | 556639assertions/132cases0 |
| final-feature-cache-api-tests.log | 5905assertions/6cases0; actual disk hits/misses/purge/retry/lifecycle |
| final-feature-cache-instrumented-tests.log | 882assertions/1case0; actual instrumented PSOs bypass/no writes |
| final-feature-cache-selective-tests.log | 98assertions/1case0; actual selection controls |
| python3 -m unittest discover -s tools/metal/tests -v | 39tests0, final-feature-tools-tests.log |
| python3 tools/metal/validate_harness.py | 32required files0, final-feature-harness.log |
| Strict source audit | 3509files/0blockers/0scanerrors/1188informational, final-feature-source-audit.json/log |

Source receipt final-feature-build-source.json; hashes final-feature-identities.json:
viewer015b01628ad55fd378d3ec8f7e676c5a3a69d6f7222c5881d2afbef4e75ecb3a;
metallibb8da56415b3546dedc50882109b20568ab4a8ec6f6bf8d3885e6165ea18e3f89;
appearance698376ace0b03ef095090d57456fff74bf8a3031ea70cc34b63ead752493ff75.
One font skip: installed Noto-COLRv1 has one CPAL palette, no multi-palette
data. This is not a rendering failure. Earlier failing receipts remain.

## Production-owner cache probe

Temporary /private/tmp/alchemy-production-cache-probe-20261111.mm uses canonical
libalmetal (SHA33d4d43f… unchanged before/after) and actual world/UI/resampler
owners. Cold factories504 include real duplicate memory hits. Recreated
device/owners load two disk archives,492render+12compute actual
FailOnBinaryArchiveMiss hits,0misses/errors. World debug16bytes/UI4x4/resample2x2
RGBA8 and exposure-meter128x72RGBA16F(.25,0,0,1) match exact actual outputs.
Receipt /private/tmp/alchemy-production-cache-probe-20261111/receipt.json;
cache deleted afterward. API validation1; four shader-selection variables unset.
This proves tested descriptor coverage, not performance or all descriptors.

## Continuing work

Native target admission finds shared-depth color resource/content loss and
stale actual labels; menu_port owns original-mapped repairs and GPU oracles.
FontGL/TextCache61 and registry/COLRv1/shaping/UIImage24 invariants are being
admitted independently. Root removes dead ALImageSlot recording state only
after the live format predicate stays in target allocation. Old generic
query/GL mechanics need consumer review, not superficial renaming.
Source-zero is lexical evidence, not runtime context/link/subprocess proof,
whole-feature parity, prolonged stability, independent release review or
measured multi-scene performance. Goal/product remain incomplete.
