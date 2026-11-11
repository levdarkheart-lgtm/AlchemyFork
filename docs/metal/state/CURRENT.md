# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `f75d9e306ffd1371582ff40b8662bf7944aabee6`.
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
Native buffer storage bodies remain unchanged with stronger tests.

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
Integrated Scene Monitor **513/7**0 and buffer **108/4**0. Raw UI/target child
**8007/9**0, real Scene Monitor result/UI child **80/1**0. RenderInfo/join/
indirect/timing child **11455/16**0; actual source/Release/link checks0 and peer
reviews clean. Exact reds/precision limits are in the latest note. Root viewer
Scene Monitor/shader-manager/target Release syntax0.
Latest root strict production source audit:3509files/0blockers/0errors/
1177informational. Including excluded legacy tests:4109files/486blocking/
0errors; also includes two deliberate SDL no-OpenGL negative assertions.
Lexical source-zero does not prove runtime or whole-feature parity. Tools39/
harness32files0. Stale configuration and missing test-bundle resource results
are excluded explicitly in scene-ui-test-validation.json, then corrected.

**Verified canonical source9918621da8fc7bbc3de3edbd63d33150c8e00d1a,
version26.4.0d68744**. Doctor/configure/build0,
349Mach-O/zero invalid ARM64 slices, deep strict codesign0, forbidden framework/
GL/CGL/SDL_GL imports0. Optional appearance Release build/ARM64/signature/imports0.
Exact hashes/source: scene-ui-canonical-identities.json/scene-ui-canonical-source.json.
All349 Mach-O imports audited: foreign VLC screen/vout/caopengllayer/glconv and
CEF contain GL links; videotoolbox has a CV compatibility-key import. These
are recorded separately, not a whole-bundle no-GL pass. Trace actual media
consumer/selection policy before removal. Neither executable was launched.
No performance or release-readiness claim.

## Exact next actions and ownership

Root owns integration/CMake/docs. Frozen9918 verification is complete; fence
and test-bundle packaging integrated. Review silhouette8c40fc6 then release
pipeline for uniform closure. Root isolated test-only tree
/private/tmp/alchemy-native-postchain-admission-20261111 proves a new final
viewport defect: original hard lens mask at32x24 passes at1x but native world
post then resize fails1080/9249assertions at reduced/oversampled sizes. Preserve
the exact oracle and fix final window effects after shared-owner release.

menu_port: `/private/tmp/alchemy-native-rlva-feature-admission-20261111`;
fresh native silhouette work based0b. Exclusive pipeline.cpp/h, almetalworld.cpp/h
and minimal coverage MSL/test. Source confirms original @setcam_avdist/muted/
complexity impostor stamp can write CPU fill RGB (.05 silhouette), while native
stamp loses RGB fill. Initial G-buffer stamp already covers empty forward
scenes; zero-batch composite admission is required to move stamping to the
original policy point, not a separately observed lost-alpha defect. Child
21566assertions/fourcases0; review/integration pending. No invented replay tint.
Other audited RLVa camera/texture/privacy/overlay policies retain original
consumers; inherited PBR limitation is not a new native regression.

rlva_port: `/private/tmp/alchemy-native-uniform-upload-retirement-20261111`,
base32894 plus RenderInfo ancestor. Exclusive LLRender, ALUniformBuffer,
environment/control/settings/shader-manager and viewer-window uniform hooks.
Retire unconsumed GPU binding/rings after zero-reader proof; preserve actual
CPU environment/deferred/shadow data, matrix ABI/stacks/math and live light state.
Five exact obsolete setting feature-table rows approved. **Pipeline files held
for menu_port**; no uniform pipeline edits until silhouette owner freezes.
ALGPUBuffer full retirement is a separately mapped follow-up, not yet done.

gl_tail_audit: `/private/tmp/alchemy-native-frame-fences-coverage-20261111`.
Frame-fence495efc818c ready: original12cases mapped to native9cases; actual
2318/20with scheduler/completion/slack0. Red2157/9 found one2vs3 query-call
undercount; two increments only, timeline unchanged. Next own ALGPUBuffer
aftermath in isolated9918 tree; coordinate LLRender hooks with uniform owner.
Preserve CPU uniform schema; replace Buffers diagnostic with accurately labeled
tracked native wrapper bytes, never device-wide allocated bytes.

Dead ALGLStencil/ALWaterStencil recording graph has zero production consumers;
native typed water/exclusion/SMAA owners remain. Foundation cleanup deferred
behind live silhouette repair; coordinate LLRender removal with uniform owner.
Earlier integrated worktrees must not be reapplied. Exact implementation and
evidence provenance is in the linked notes.

## Complete-product advancement assessment

| ID | State | Evidence / next boundary |
| --- | --- | --- |
| M-008 | doing | Native views/effects/resources advance; broader parity remains. |
| M-011 | doing | Source/import gates advance; dead native emulation and runtime gates remain. |
| M-012 | complete | Prior native UI automation evidence retained; no launch this goal. |
| M-013 | doing | Native sphere repaired; live silhouette stamp gap under repair. |
| M-014 | deferred | Owner-stopped20FPS campaign remains deferred. |

Full original feature parity, prolonged stability, strict no-GL source/runtime,
independent build/review evidence and measured multi-scene performance remain
open. Preserve earlier lighting, self-turn, camera and voice issues in their
notes. Build/test/source success alone never establishes product completion.
