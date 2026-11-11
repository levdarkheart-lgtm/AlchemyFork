# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `0b219b9728b7dda9864357e1546a7e7337ce274c`.
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
Last full registered llrender at f36: **88827assertions/375cases**,374passed/one
known CPAL multi-palette skip; separate native Font/UI **1862/61**0, API+shader
validation. Current0b final test build/run is the next root gate; historical
overlapping filters are not a current aggregate.
Integrated Scene Monitor **513/7**0 and buffer **108/4**0. Raw UI/target child
**8007/9**0, real Scene Monitor result/UI child **80/1**0. RenderInfo/join/
indirect/timing child **11455/16**0; actual source/Release/link checks0 and peer
reviews clean. Exact reds/precision limits are in the latest note. Root viewer
Scene Monitor/shader-manager/target Release syntax0.
Latest root strict source audit:3507files/0blockers/0errors/1186informational
before Scene Monitor integration; lexical source-zero does not prove runtime,
linking, subprocess safety or whole-feature parity. Harness32files0.

**Last verified canonical app remains source d28b0477a038e42b5a593d0d5350588d48c62343,
version26.4.0d68712**, older than current code. Doctor/configure/build0,
349Mach-O/zero invalid ARM64 slices, deep strict codesign0, forbidden framework/
GL/CGL/SDL_GL imports0. Optional appearance Release build/ARM64/signature/imports0.
Exact hashes/source: final-feature-identities.json/final-feature-build-source.json.
Current0b needs a frozen canonical and appearance build/verification. Neither
executable was launched. No performance or release-readiness claim.

## Exact next actions and ownership

Root owns integration/CMake/docs and frozen canonical verification. Finish0b
registered native suites, then doctor/configure/build/verify/import/signature
and optional appearance checks. Keep source frozen during those builds;
agents continue only in isolated worktrees. Integrate their new source after
frozen-build receipts. Do not stop at that bounded green gate.

menu_port: `/private/tmp/alchemy-native-rlva-feature-admission-20261111`;
fresh native silhouette work based0b. Exclusive pipeline.cpp/h, almetalworld.cpp/h
and minimal coverage MSL/test. Source confirms original @setcam_avdist/muted/
complexity impostor stamp can write CPU fill RGB (.05 silhouette), while native
stamp writes alpha only and skips zero-forward-batch scenes. Prove/fix exact
original stamp boundary, bothZ/edges/attachments; no invented replay tint.
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
Own ALFrameFences.cpp/h and old/new fence tests; map all12 original cases to
actual scheduler/gated GPU completion/pacing/release. Possible missing native
query-call diagnostic requires red proof before fix; preserve timeline behavior.

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
