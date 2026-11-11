# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `5d55f9a202`.
Benchmark registration and displaced shader cleanup are validated.
[Native source-tail evidence](../work/active/native-opengl-tail-removal.md)
records removals, exact CPU vocabulary and source-gate limits.
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

Integrated native image/label/profiler/direct draw tests pass **67773 assertions/47 cases**, including
texture/index/offset/alpha/order, failed upload and independent publication
readiness, under Metal API/shader validation. Integrated Frame Profile and
utility/preview reload tests pass **60425 assertions/12 cases**. These filters
overlap earlier test receipts; do not sum historical totals. The isolated
Developer benchmark passes **103 assertions/2 cases**; canonical integration
test build and combined GPU run exit0,19123 assertions/10 cases.
Harness validation0. Frozen696c43ba78 doctor/configure/build all0;
feature-native-*.log receipts. Image/label/profile and wrapper builds0.
Wrapper regressions:66108/18 llrender,552467/29CPUwater,33323/8nativeGPU.
Water/window build0; native water count regression31516assertions/1case0.

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

Expanded root strict source audit0:3507files/0blocking/0errors/1188informational.
Root39tools tests/harness0. All first-party runtime directories are scanned,
with explicit vendor/submodule/test exclusions and lexical limits. Native names
and CPU data are informational; actual APIs still block. This does not prove
runtime contexts, linking or foreign subprocess behavior.

## Exact next actions and ownership

Root owns integration, CMake, documentation, tests and canonical builds. Current
image/label/profiler/wrapper/Win32 removal and native water count changes are
integrated. [Menu audit](../work/active/developer-menu-admission.md) fixes two
inherited timer controls and identifies orphaned Enable Shader Cache. Root
policy6cc38ac11b in `/private/tmp/alchemy-native-cache-policy-20261111` is not yet
integrated; startup/reload/clear/shutdown wiring awaits the native cache owner.
CPU vocabulary and audit integrated; native builds0,67793assertions/49render
cases and93869/22GPUcases pass. Freeze after cache/policy integration, then
full builds/verification. No launches or cache/menu actions.

Agent `menu_port`: `/private/tmp/alchemy-native-pipeline-cache-20261111`, branch
`codex/native-pipeline-cache`, basec5261cecae. Own new native binary archive/test,
device-owned lifetime and synchronous PSO factory hooks, exact builtin identity.
Default disabled/no IO; root maps actual setting/path/version/second instance.

Agent `gl_tail_audit`: CPU vocabulary integratedfe9cf22c68; current task is
independent native-cache lifecycle/persistence/default-path review.
Agent `rlva_port`: audit integrated5d55f9a202; current task retires excluded
llvertexbuffer_gl_test.cpp only after native invariant census, porting unique
meaningful checks into a dedicated new native file. Root owns shared CMake.
CURRENT/linked work notes retain prior committed worktree provenance.

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
