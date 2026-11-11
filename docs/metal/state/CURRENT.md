# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `8bb61bb34d` (newer than the verified app below).
The remaining-feature/OpenGL-removal goal stays active. No viewer or appearance
utility launch, menu invocation, screenshot, signed-in state or user-cache
operation. Offscreen tests submit actual Metal commands. The owner-stopped
20FPS campaign remains deferred.

## Current implementation

[Latest Scene Monitor/UI/RenderInfo evidence](../work/active/native-scene-ui-and-render-info.md)
extends [target/query/font invariants](../work/active/native-target-query-invariants.md)
and [native cache/coverage](../work/active/native-pipeline-cache-and-coverage.md).
Native Scene Monitor uses exact current normalized pre-UI color/depth leases,
original RGB8 crop/Bayer/quantization/BilinearMirror/threshold/.5 legacy
normalization, asynchronous count ownership and actual filtered UI output.
Restore only owned freeze state and refreeze an active monitor after reset.
The viewer/menu itself remains unexecuted.

Raw native and LLRenderTarget sampled bindings now reach authoritative UI with
exact default/custom sampler policy. Old bindings flush first; unsupported
resources and unowned raw display lists fail closed. Actual computed-result UI
test proves colored pixels/blending/depth/lifetime. Displaced two shader
managers/four GLSL files and uncalled window-copy stub removed.

RenderInfo counts actual native encoder emissions independently of timing,
including UI background/foreground/resume/HUD. Native totals stand alone;
accepted UI batches are separately labeled. ICB range/slot metadata does not
fabricate exact draw/element totals. Joined/direct/instanced/patch inputs use
actual emission sites; generation reset excludes old encoder publication.

Preserve cache, RLVa sphere/current-view policy, shader/utility/preview reload,
readback/publication, vertex resources/batching, diagnostics/benchmark, water
counts, SDL/dependency and CPU-vocabulary repairs. Target growth/rebase retains
attachment identity/data/depth lifetime, integer clears round correctly, labels
follow native resources and release invalidates query generations. Dead ambient
image slots, generic query/timer/span APIs and unconsumed GPU stream removed.
RLVa silhouette b741/a922 restores conditional original fill/pin/order and exact
profile join eligibility. Native final viewport65c2 fixes scaled lens masks and
inverted print-noise Y; independent color/depth normalization preserves snapshot
depth through readback. Unconsumed uniform6a405 and generic bufferc69 graphs,
empty storage metadata and dead stencil scopes/fields are removed. Actual CPU
environment/deferred/shadow/matrix layouts/math and typed native owners remain.
TextureView reports tracked Metal buffer wrapper bytes, not driver memory.

## Observed validation

Receipts: `artifacts/metal/feature-completion-20261111/`. Native fences2157/9
and packaged indexed-join7969/1 pass API+shader validation. almetal_tests now
owns its offline metallib in a target-local bundle; CTest directly executes
the CLI binary. Generated resource rules also track shader-only updates.
Matching-header frozen d9/renderer1f full registered Release suites passed:
llrender98965assertions/389cases(one known CPAL skip), Font/UI1862/61,
alrender556641/132 and affected Metal106377/50, API+shader validation.
See viewport-test-validation.json. Newer HUD/lens aggregate results are below.
Earlier cache API5905/6, instrumented882/1, selective98/1 and silhouette/
uniform/buffer/stencil/viewport source proofs remain in the latest work note.
Original viewport mask red1080/9249 and raw-row red238/322 repaired; superseded
inactive-grain/inverted-oracle fixtures are not final evidence. Stale build/
missing bundle-resource results were corrected and explicitly excluded.
Lexical source-zero does not prove runtime or whole-feature parity. Root
tools49/harness32files0; exact old checkpoints remain in the work notes.

**Last verified canonical source d9f624ea464d3ba3c305d7656bf9533f6e65a8dd,
version26.4.0d68755**. Doctor/configure/build0,
349Mach-O/zero invalid ARM64 slices, deep strict codesign0, forbidden framework/
GL/CGL/SDL_GL imports0. Optional appearance Release build/ARM64/signature/imports0.
Exact hashes/source: viewport-canonical-identities.json/viewport-canonical-source.json.
All349 Mach-O imports audited: foreign VLC screen/vout/caopengllayer/glconv and
CEF contain GL links; videotoolbox has a CV compatibility-key import. These
are recorded separately, not a whole-bundle no-GL pass. Neither executable launched.
CEF Libraries/libvulkan.dylib is also shipped; its own LC_ID_DYLIB is not an
import. Dynamic third-party renderer selection remains unobserved.
Newer finalizer/test/tool admissions are not represented by that app.
No performance or release-readiness claim.

## Exact next actions and ownership

Root owns CMake/docs/integration. Integrated8bb61bb34d (child5940153141) restores
the HUD bridge in alhuddepth.h, llrender.cpp and llviewerdisplay.cpp plus a new
native fixture, retiring the old seven-case GL fixture. Actual original
clear/restore callers still used always-false isComplete; availability-only
fix then exposes inverted rectangle rows (100/803fail). Native clear/restore,
correct raw lower-row coordinates and explicit pre-operation flush pass9595/9
actual GPU tests, mapping all original seven-case invariants. Removing only
the flush fails100/818 assertions in the corrected producer-flush control.
Initial no-order control omitted final producer flush and stayed green; it is
superseded, never final ordering proof. Viewer textual Release syntax0 and
independent source/lifetime review0. Root registered Release rebuild0;
llrender302803/426(425pass/one known CPAL skip), Font/UI1862/61 and
alrender556674/132 all0 under API+shader validation. Exact commands and binary
hashes: hud-lens-render-validation.json. Latest strict production source audit:
3505files/0blockers/0errors/1141informational. Including excluded old tests:
4099files/232blocking findings, not a strict pass or production runtime result.
No viewer/window execution.

rlva_port exclusively owns isolated pipeline.cpp/h and new lens policy tests.
Two actual CPU config methods also used always-false isComplete: original
positive controls fail10/36. Integrateda54314153d (child2707f854c0) corrects native
availability and retires the displaced generator, preserving epoch/hold/sky/
reset history; actual registered CPU73/3passes, existing HDR GPU1846/2passes.
NonHDR admission remains consistently excluded pending its original-mapped
float linear screen closure. Original Screen is alwaysRGBA16F; native8bit
screen cannot exceed original luminance threshold2. Native display also rejects
nonHDR flare; actual28assertion probe exposed both issues. RLVa owns isolated
pipeline, almetalworld.cpp/h, exact llreflectionmapmanager.cpp callsites and
two existing native test files. Preserve explicit HDR-dependent SSR/probe/
refraction/display formats instead of inferring HDR from a float input.

gl_tail_audit owns isolated remaining alrender legacy fixture admission except
HUD, plus appearance's three ordered morph-alpha cases and the proven orphan
ALOnDemandTarget graph. Native pool/VB122/4, FXAA8993/1 and morph13203/3 are
initial child passes; final strengthened source/proofs/retirement pending.
menu_port owns isolated migration of28LLUI text/surface/diff native cases,
with shared process-lifetime offscreen font/UI fixture; root owns CMake.
Root next maps the last nine llrender GL-state cases against original/live
native owners. General target scissored clears are not proven: encodeClear
currently uses whole-attachment load actions. Trace current consumers before
mapping a displaced architecture or implementing missing live behavior.
All canonical shared-file edits remain root-owned; agents use separate trees.

Integrated88ff446002 removes four uncalled finalizers and CPU CAS include;
ab128e02e5 admits five native CAS/DoF legacy-invariant cases;9ac375d389 admits
24image/four hardware tessellation cases and a compile-only manual hidden-window
fixture. Root registration/slot identity assertions and removal of obsolete
timing-only SSR benchmark/helper committed8dcbbced4d. Registered build0;
image/tessellation194243/28 and postchain170713/5 pass API+shader validation.
a272c2b365 admits eight CPU GLSL readers and both
lens content tools against native source; exact default metric/preset preservation,
49tool tests0 and two original-mapped Python model corrections are proven;
root CPU364/9, tools49/harness32files0. Foreign audit complete/read-only:
/private/tmp/alchemy-foreign-media-audit-20261111/audit.json identifies three
windowed VLC GL output/converter plugin exclusion candidates, backed by actual
ARM64 callback disassembly forcing vmem/window-none and339module importer census.
Keep VT decoder/CV compatibility key, screen input(CoreGraphics-only actual
symbols but stale framework link), and required complete CEF capability assets.
Runtime selection/loading remains unknown; no whole-bundle no-GL claim.
Root packaging changes require fresh staging and callback regression validation.

Finished trees (silhouette8c/75,uniform81fd,buffer8818,stencil9720,viewport7ee)
must not be reapplied. Receipts and exact original maps are in the latest note.
Foreign VLC/CEF import consumer/policy audit and remaining GLSL assets follow.

## Complete-product advancement assessment

| ID | State | Evidence / next boundary |
| --- | --- | --- |
| M-008 | doing | Native views/effects/resources advance; broader parity remains. |
| M-011 | doing | Source/import gates advance; dead native emulation and runtime gates remain. |
| M-012 | complete | Prior native UI automation evidence retained; no launch this goal. |
| M-013 | doing | Native sphere and silhouette repaired; broader original parity remains. |
| M-014 | deferred | Owner-stopped20FPS campaign remains deferred. |

Full original feature parity, prolonged stability, strict no-GL source/runtime,
independent build/review evidence and measured multi-scene performance remain
open. Preserve earlier lighting, self-turn, camera and voice issues in their
notes. Build/test/source success alone never establishes product completion.
