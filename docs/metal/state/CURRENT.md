# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `f36e8aabdb`.
Benchmark registration and displaced shader cleanup are validated.
[Native source-tail evidence](../work/active/native-opengl-tail-removal.md)
records removals, exact CPU vocabulary and source-gate limits.
[Ongoing target/query/font admission](../work/active/native-target-query-invariants.md)
records newer implementation and actual regressions; goal stays active.
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

**Last verified canonical app is source `d28b0477a038e42b5a593d0d5350588d48c62343`, version
26.4.0d68712**, older than current source. Doctor/configure/build0,349 Mach-O files/zero
invalid ARM64 slices, deep strict codesign0. Viewer/utility imports contain no
OpenGL framework or GL/CGL/SDL_GL undefined imports. The earlier failed link65
and restoration of native `LLViewerShaderMgr::built` are retained.
Exact hashes: `artifacts/metal/feature-completion-20261111/final-feature-identities.json`.
Optional appearance Release build/ARM64/signature/forbidden-import checks0.
Neither executable was launched. [Latest cache/coverage receipts](../work/active/native-pipeline-cache-and-coverage.md)
supersede historical partial-test/build counts above.
All receipts are under `artifacts/metal/feature-completion-20261111/`.

Expanded root strict source audit0:3509files/0blocking/0errors/1188informational.
Root39tools tests/harness0. All first-party runtime directories are scanned,
with explicit vendor/submodule/test exclusions and lexical limits. Native names
and CPU data are informational; actual APIs still block. This does not prove
runtime contexts, linking or foreign subprocess behavior.

## Exact next actions and ownership

Root owns integration, CMake, documentation, tests and canonical builds. Earlier
image/label/profiler/wrapper/Win32/water/cache/policy changes are integrated.
Native cache dd9372cd69 + policy53e3599a36 include actual persistence/retry/
disable/purge/instrumentation proof. Frozen final native builds0; full llrender
77929assertions/312cases:311pass/one CPAL multi-palette skip; alrender556639/1320,
targeted native GPU93869/220. Cache API5905/60, global instrumentation882/10,
selective98/10. Historical filters overlap. No launches or user cache actions.

Native font registry/COLRv1/shaping/UIImage24 integrated eb0d2a9618/154165dfc8,
276assertions0. FontGL/TextCache61 integrated bdfc3f86c8/fb7c4873a3 in separate
llrender_font_ui_tests:1862assertions0. Original2D helpers four native cases:
76assertions0; exact57600-pixel outline oracle retained. Dead ALImageSlot graph
removed bd13164b70 after exact live format predicate moved657da5ce23;
target-usage/font filter447/340. Timer ef34b9c424 native5666/30 and dead
attachment-span storage f2db7c729e removed; actual native profiles preserved.
Latest integrated f36 native build0: full llrender **88827assertions/375cases**,
374passed/one known CPAL palette skip; separate Font/UI **1862/61**0 under
Metal API/shader validation. Includes target/query and accepted UI diagnostics
fixes below. Prior targeted filters overlap; do not sum them.

Target/depth f75461d142 integrated: preserve attachments/rollback/native labels,
retain shared-depth lifetime, atomically build depth-only sharers and restore
original integer-clear rounding. Child3867/26 actualGPU0; full integrated suite0.
Agent menu_port now owns isolated ALGPUBuffer storage invariant admission and
dead test-only stream/getGLName cleanup; root retains uniform/frame ownership.

Query ac4a125458 integrated after CPU3/GPU15 red assertions: generation
invalidation restored; visibility/diagnostics retained, dead driver APIs removed.
Child1291/10GPU0; full integrated suite0. Agent gl_tail_audit now read-only
audits actual typed-world RenderInfo counters before proposing an owner.
Agent `rlva_port`: `/private/tmp/alchemy-native-scene-monitor-20261111`,
basebd13164b70 + root frame-owner1c77fa745a/7162dd92f8. Own SceneMonitor,
bounded native service/test and additive MSL. Preserve original byte/dither/
filter/normalization/state semantics with actual retained pre-UI leases.
Root owns pipeline/display/CMake/docs/LLRender. Native sphere/recording/matrix
replay counts now owned at successful UI batch admission, avoiding double counts;
full integrated native suite0. Remove unreachable fetched GL marker395510e792.
No current source full-app verification yet; rebuild after these repairs.
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
