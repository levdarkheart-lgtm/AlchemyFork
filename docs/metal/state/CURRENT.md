# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `815416675e`.
Benchmark registration and displaced shader cleanup are validated.
The remaining-feature/OpenGL-removal goal stays active. No viewer or appearance
utility launch, menu action, screenshot or signed-in state change. Offscreen
test executables create Metal devices and submit GPU work. The separate20FPS
campaign remains deferred.

## Current implementation

[Latest feature/resource work](../work/active/native-feature-resource-completion.md)
extends [shader ownership cleanup](../work/active/native-shader-owner-cleanup.md).
Forced shader rebuild announces a new builtin asset revision only after
successful world/UI and semantic configuration. Utilities, retained background
scaling, lit/sculpt/model/avatar and PBR previews refresh their actual native
shader owners lazily; PBR preview regenerates its internal BRDF LUT. Generated
outputs, external targets and in-flight leases remain owned.

Developer Frame Profile now collects actual Metal encoder timestamps and bound
pipeline/MSL identities, with deferred UI submission and asynchronous completion.
The physical M1 Max probe supports stage-boundary timestamp counters only;
per-draw/dispatch timing and hardware statistics remain unavailable. Developer
texture benchmark now uses the original fixed sampling workload with actual
passing-sample counts and completed GPU command timing. Sampled throughput is
not physical DRAM bandwidth. Neither menu action was executed.

Vertex storage/upload and direct member draws now use native owners. UI
admission checks independently published vertex/index generations, honors
indexed offsets and SoA vertex offsets, flushes preceding immediate draws and
avoids duplicate sphere capture. Volume/grass merging uses real native buffer
bounds and adjacency instead of zero GL driver recommendations. Remove
write-only GLTF upload sinks while retaining real node/skin/material CPU data.
Dormant GL manager loading and physical GL state/error calls are removed;
CPU projection/state/flush contracts and truthful diagnostics remain.

Keep [native buffer views](../work/active/debug-buffer-visualization.md),
[appearance loader](../work/active/appearance-native-loader.md),
[RLVa/Developer/settings repairs](../work/active/remaining-feature-port.md) and
all earlier world/avatar/editing/travel/music/HUD/upload fixes.

## Observed validation

Integrated native direct draw tests pass **67598 assertions/42 cases**, including
texture/index/offset/alpha/order, failed upload and independent publication
readiness, under Metal API/shader validation. Integrated Frame Profile and
utility/preview reload tests pass **60425 assertions/12 cases**. These filters
overlap earlier test receipts; do not sum historical totals. The isolated
Developer benchmark passes **103 assertions/2 cases**; canonical integration
test build and combined GPU run exit0,19123 assertions/10 cases.
Harness validation0. Frozen696c43ba78 doctor/configure/build all0;
feature-native-*.log receipts. New image/label/profile batch test build is
running, handle79692; image-label-profile-native-test-build.log.

The first Frame Profile GPU test aborted134 on an absent optional pipeline
label; production setters now skip absent/nonconvertible labels, and the
unlabelled regression passes. Preserve the initial failure receipt.
Optional appearance Release build after utility/vertex changes exits0.

**Last verified canonical app is source `696c43ba78`, version
26.4.0d68677**, not current source. Configure/build0,349 Mach-O files/zero
invalid ARM64 slices, deep strict codesign0. Viewer/utility imports contain no
OpenGL framework or GL/CGL/SDL_GL undefined imports. The earlier failed link65
and restoration of native `LLViewerShaderMgr::built` are retained.
Exact hashes: `artifacts/metal/feature-completion-20261111/feature-native-identities.json`.
Current source still requires a frozen canonical build and verification.
All receipts are under `artifacts/metal/feature-completion-20261111/`.

Latest whole-tree audit:1257 findings/2406 files,36call-syntax matches; strict
exit1, image-label-native-gl-audit.json/log. Physical matches remain in excluded
Win32 source; CPU helper names/glTF strings and native class names also match.
Names/comments/excluded code contribute; this is not a live call count, and the
inventory omits some owned directories. Runtime context proof remains open.

## Exact next actions and ownership

Root owns integration, CMake, documentation, tests and canonical builds. Resume
native target rebuild79692, then execute image publication/label/profiler and
draw regressions with Metal validation. Current image/layout, label, profiler,
SDL context removal, dependency removal and native-only build gate are integrated.
Integrate frozen wrapper cleanup, then freeze a new canonical build. No launches.

Agent `menu_port`: frozen `/private/tmp/alchemy-image-publication-cleanup-20261111`,
branch `codex/native-image-publication-cleanup`. Own llimagegl.cpp/.h and new
native readback test only. Fix published-versus-pending format mixing in
readBackRaw and audited dead image helpers are integrated b91d739a46/31db370a3d.
Baseline fails2 cases; fixed offscreen tests pass83assertions/2cases before
layout cleanup. Final rebuilt-target GPU run remains. Full menu audit is now
read-only, including registered callbacks and native owners.

Agent `gl_tail_audit`: labels integrated2e2b542800; new isolated
`/private/tmp/alchemy-render-wrapper-cleanup-20261111`,
branch `codex/native-render-wrapper-cleanup`, base62d99552aa. Own dead generic
LLRender instanced/indirect/compute/barrier wrappers, ALWaterSurface bind/draw,
exact displaced excluded fixture sections and stale native failure text.
Agent `rlva_port`: isolated `/private/tmp/alchemy-shader-profiler-cleanup-20261111`,
branch `codex/native-shader-profiler-cleanup`, base325354e4da. Own old
llglslshader profiling deletion, exact dead Win32 caller/excluded test sections
and ALPrimitivePlot unsupported-counter repair integrated d0f6fcf6f0/d397b548ca.
Current task: independent label review and native water draw diagnostic census.
Root owns shared CMake.

Prior utility/preview/vertex/FrameProfile/manager/batching/GLTF worktrees are
committed and integrated; do not reintegrate their foundation commits. Exact
implementation/receipt references are in the linked latest work note.

## Complete-product advancement assessment

| ID | State | Evidence / next boundary |
| --- | --- | --- |
| M-008 | doing | Native views/effects retained; whole-feature parity open. |
| M-011 | doing | Native authority advances; remaining source/runtime no-GL gates open. |
| M-012 | complete | Prior native UI automation evidence retained; no launch this goal. |
| M-013 | doing | RLVa sphere repaired; full feature verification remains. |
| M-014 | deferred | Owner-stopped20FPS campaign remains deferred. |

Full original feature parity, prolonged stability, strict no-GL source/runtime,
independent build/review evidence and measured multi-scene performance remain open.
Preserve unresolved lighting, self-turn, camera and voice issues in their prior
notes. No release-readiness or performance-improvement claim.
