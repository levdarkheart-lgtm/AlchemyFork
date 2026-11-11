# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `a272c2b365` (newer than the verified app below).
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

Root receipts: `artifacts/metal/feature-completion-20261111/`.
Frame-fence fix integrated f75d9e306f. Registered CTest native fences2157/9
and packaged indexed-join7969/1 pass API+shader validation. almetal_tests now
owns its offline metallib in a target-local bundle; CTest directly executes
the CLI binary. Generated resource rules also track shader-only updates.
Frozen source9918621da8 (renderer0b) registered Release llrender:
**96864assertions/384cases**,383passed/one known CPAL multi-palette skip;
separate native Font/UI **1862/61**0, alrender **556641/132**0, affected native
Metal **73523/38**0, API+shader validation. API-only cache **5905/6**0,
instrumented **882/1**0 and selective **98/1**0. Existing native CAS/DoF/final
presentation/SSR-history **4487/4**0 is an overlapping admission filter.
New integrated silhouette registered21566/4passes API+shader validation; child
38636/5with unchanged impostor replay passes. Uniform8130/19CPU/native UI and
378/4actual-owner tests pass; exact CPU/layout proofs, eight Release checks0.
Buffer typed119/4 and stencil167/10pass with native validation. Viewport child
49366/9passes with existing post/retained-background owners, matching scene
headers and textual viewer syntax0. Original mask red1080/9249; raw-row red
238/322. Superseded inactive-grain/inverted-oracle fixtures are recorded, not
used as final evidence. Matching-header frozen d9/renderer1f full registered
Release suites passed: llrender98965assertions/389cases (388pass/one known CPAL
skip), Font/UI1862/61, alrender556641/132 and affected Metal106377/50, all0
with API+shader validation. See viewport-test-validation.json.
Latest root strict production source audit:3504files/0blockers/0errors/
1170informational. Frozen9918 including excluded legacy tests had486blockers;
also two deliberate SDL no-OpenGL negative assertions.
Lexical source-zero does not prove runtime or whole-feature parity. Tools39/
harness32files0. Stale configuration and missing test-bundle resource results
are excluded explicitly in scene-ui-test-validation.json, then corrected.

**Last verified canonical source d9f624ea464d3ba3c305d7656bf9533f6e65a8dd,
version26.4.0d68755**. Doctor/configure/build0,
349Mach-O/zero invalid ARM64 slices, deep strict codesign0, forbidden framework/
GL/CGL/SDL_GL imports0. Optional appearance Release build/ARM64/signature/imports0.
Exact hashes/source: viewport-canonical-identities.json/viewport-canonical-source.json.
All349 Mach-O imports audited: foreign VLC screen/vout/caopengllayer/glconv and
CEF contain GL links; videotoolbox has a CV compatibility-key import. These
are recorded separately, not a whole-bundle no-GL pass. Trace actual media
consumer/selection policy before removal. Neither executable was launched.
CEF Libraries/libvulkan.dylib is also shipped; its own LC_ID_DYLIB is not an
import. Dynamic third-party renderer selection remains unobserved.
Newer finalizer/test/tool admissions are not represented by that app.
No performance or release-readiness claim.

## Exact next actions and ownership

Root owns CMake/docs/integration and the isolated HUD bridge at
`/private/tmp/alchemy-native-hud-depth-admission-20261111`: alhuddepth.h,
llrender.cpp and llviewerdisplay.cpp plus a new native fixture. Actual original
clear/restore callers still used always-false isComplete; availability-only
fix then exposes inverted rectangle rows (100/803fail). Native clear/restore,
correct raw lower-row coordinates and explicit pre-operation flush pass the
initial803/2 actual GPU tests. Expand original seven-case invariants before
integration. No viewer/window execution.

rlva_port exclusively owns isolated pipeline.cpp/h and new lens policy tests.
Two actual CPU config methods also used always-false isComplete: original
positive controls fail10/36, availability fix passes69/3. Retire the displaced
legacy generator before enabling config; preserve native epoch/hold/sky/reset
history. Verify original-supported nonHDR flare/dirt through real GPU owners.

gl_tail_audit owns isolated remaining alrender legacy fixture admission except
HUD. menu_port audits foreign media imports/consumer policy read-only.
All canonical shared-file edits remain root-owned; agents use separate trees.

Integrated88ff446002 removes four uncalled finalizers and CPU CAS include;
ab128e02e5 admits five native CAS/DoF legacy-invariant cases;9ac375d389 admits
24image/four hardware tessellation cases and a compile-only manual hidden-window
fixture. Root registration/slot identity assertions and removal of obsolete
timing-only SSR benchmark/helper are pending commit. Registered build0;
image/tessellation194243/28 and postchain170713/5 pass API+shader validation.
a272c2b365 admits eight CPU GLSL readers and both
lens content tools against native source; exact default metric/preset preservation,
49tool tests0 and two original-mapped Python model corrections are child-proven.
Root tools/harness rerun follows documentation update.

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
