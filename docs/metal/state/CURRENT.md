# Current engineering state — native buffer views and baking loader

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `a5425524eeb85e340d491a818a91d4b0f375c105`.
The remaining-feature/OpenGL-removal goal is active, including RLVa and Debug/
Developer behavior. No viewer launch or screenshot comparison is authorized.
Neither viewer nor appearance utility was launched. No account/cache/settings/
camera/location/simulator state changes. Separate20FPS campaign remains deferred.

## Current implementation

[Native buffer views](../work/active/debug-buffer-visualization.md) replace the
old presentation rejection and GL-only visualization path. Modes0–3 copy real
G-buffer data; mode4 consumes the actual spatial luminance grid; modes5/6 retain
actual SMAA intermediate owners. Preserve original sampling, alpha, quantization,
cutout/optional-source policy and placement after RLVa/before scopes/reference.
Meter-only execution restores original diagnostics with auto-exposure disabled.
The last meter survives temporal-history/HDR toggles; device/renderer failures
clear it. Remove the old program/GLSL fragment and unused GL checkTexSize method.

[Appearance utility repair](../work/active/appearance-native-loader.md) replaces
its still-live GL shader compilation with native AlphaMask metadata, retaining
original layout/math. Checked initialization and shared cleanup prevent failed
native setup from leaving borrowed backend pointers/window ownership behind.
Remove unused utility shader bookkeeping and dead non-Darwin fallback.

Keep [prior RLVa/Developer/settings fixes](../work/active/remaining-feature-port.md)
and all earlier renderer, avatar, editing, travel, music and HUD/upload repairs.
Deleted source remains recoverable through Git.

## Observed validation

Doctor/configure and native test builds0. Full and final incremental canonical
Release builds0, bundle26.4.0d68651. Source content matches a5425524ee; configuration/
build began at8de5f88ce9 plus these changes. ARM64 inspection passes349 Mach-O
files/zero invalid slices; deep strict codesign verification0. Work notes pin
executable/metallib/utility hashes and exact command/evidence paths.

Under Metal API and shader validation, visualization/ownership plus existing
exposure/SMAA/display regressions pass59386assertions/11cases. Actual utility
loader plus native renderer regressions pass959assertions/18cases:60345total/29.
The loader case proves metadata readiness with unset GL entrypoints and zero
GPU submissions. Optional appearance Release target builds0 and is ARM64.
Both final executables have no OpenGL framework or GL/CGL/SDL_GL imports.

Initial pixel tests exposed overly ideal filtering precision assumptions:
6 fractional RGB10 failures, then3 float-control midpoint failures. Independent
compute control reproduced the hardware precision before packing. Final bounded
filter checks retain strict authored/mip/alpha/endpoint/midpoint output checks
and require production/control agreement. Failed test/compile receipts retained.
No production sampling math was weakened or tuned to pass an image comparison.

These checks do not establish live menu interaction, RLVa command/touch/tween/
restart parity, complete bake output or constructor-failure behavior at runtime.
No viewer/utility launch, no screenshot, no performance claim.

Ignored evidence: `artifacts/metal/feature-completion-20261111/`.
Strict source audit exits1:1783findings/2406files,333GL-call-syntax matches.
Many names are native facades; real dormant GL implementations/pointers remain.
The inventory also omits some owned areas such as appearance utility, so direct
source/target/import auditing supplements it. Runtime context proof stays open.

## Complete-product advancement assessment

M-008/M-011 advance through actual native menu consumers, corrected producer/
intermediate ownership and removal of a live utility GL compiler dependency.
Full parity, strict no-GL, prolonged stability and performance remain open.
Independent build/review evidence remains open. M-013 is doing; M-014 deferred.

| Milestone | State | Evidence / remaining requirement |
| --- | --- | --- |
| M-008 Advanced views/effects | doing | Native buffer views have physical semantics/ownership tests; live feature parity remains. |
| M-012 Native UI automation | complete | Prior exact-build flow retained; no new launch. |
| M-013 RLVa parity | doing | Sole native sphere/current-view state preserved; broad live policy parity unverified. |

## Exact next actions

Trace original source, then remove the dormant GL backend from
`LLViewerShaderMgr::setShaders`, `LLGLSLShader::createShader/linkProgram` and
`LLShaderMgr::loadShaderFile/compileShaderFile`. The optional baking loader no
longer consumes shared GL compilation. Preserve native pass identity, variant
graphs, attributes, CPU uniform state, minimum-alpha routing and cache-purge/
shader-reload behavior. Enumerate all consumers before deleting common APIs;
retire displaced shader-only GL pointers only after their last users are gone.

Re-run native metadata/UI/world regressions, build both viewer and utility,
inspect imports/slices, and rerun strict source audit. Do not start either app.
Keep broader lighting, self-turn, camera/voice issues recorded in
[display](../work/active/display-settings-lighting-audit.md),
[crowd](../work/active/crowd-live-68643.md) and
[camera/voice](../work/active/teleport-camera-voice-audit.md) records.
No speculative brightness/camera/voice policy changes or release-readiness claim.
