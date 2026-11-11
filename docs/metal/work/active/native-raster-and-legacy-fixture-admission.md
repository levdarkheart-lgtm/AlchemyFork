# Native raster, float scene and remaining legacy fixture admission

Observed 2026-11-11, Apple M1 Max64GiB, macOS26.5.2, Xcode26.6.
Original behavioral reference: `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
Branch: `codex/apple-silicon-metal`; root checkpoint `0fe112ee90`.
No viewer, appearance utility, SDL/native window or screenshot execution.
Device-only native GPU tests are distinct from feature/menu invocation.
This note extends [the previous implementation record](native-scene-ui-and-render-info.md).

## Original mapping and integrated changes

| Root commit | Original invariant / actual native change |
| --- | --- |
| 7b8114824b | Native pool/VB/FXAA/morph controls replace displaced GL owners; orphan ALOnDemandTarget has no production readers and is retired. |
| 8f19f5689d | Preserve28LLUI text/surface/diff cases and117 static assertion sites; native font/UI fixture owns process-static metadata and actual texture lifetimes. |
| d37389e838 | Original polygon offset updates after flushing queued batches; UI honors logical slope/units and reverse-Z sign, indexed HUD sign corrected. Known retained color/depth backing defines actual target extent, then resets for no-backing draws. |
| 5fd77a8f36 | Original BatchSize(-1,+1), flat slope0 moves forward-Z depth away. Correct inherited wrong-sign test; no tolerance change. |
| 2066d5c507 | Original alrenderresourceplan137–139 Screen alwaysRGBA16F. Preserve21 native screen families independently of HDR, explicit SSR/probe HDR intent and original conversion order; remove three zero-reader alternative lighting APIs and their dead PSOs/resources/MSL. |
| 1cd708bb73 | Independent CPU original9-tap Gaussian oracle plus GPU format-store controls verify exact SSR/hero/ordinary probe publication bytes. |
| 67963a18d9 /929cdb5656 | Preserve9 exact algorithm/citation spans and21 deduplicated LGPL spans mapping240 original shader instances; exhaustive1980 marker lines accounted for. |
| e99e4e4b4c | Original REFMAP_LEVEL0 suppresses only ordinary nondefault shading traversal. Project shading count only; preserve full manager pack/debug/exposure/allocation, default/SSR/hero. |
| 459bb2d9e5 | Remove13 shader-program globals with zero external executable readers; preserve shared material assignment, surviving loader bodies, settings and final unbind. |
| e3e8ed5b35 | Native base PBR/legacy/classic warm3→1→3 policy, exact independent default pack, actual SSR→hero order and exact builtin water-helper dispatch controls. |

Raster original call sites include manipulator showAxes(-2,-2), HUD emboss(-1,-1),
and llspatialpartition2696–2713 BatchSize(-1,+1). Immediate ALMetalUIBatch has
two additional logical bias floats; root rebuilt matching headers/archives.
Isolated float scene tests used an older pinned UI cohort and therefore do not
alone verify the integrated raster ABI. Root matching rebuild/testing is separate.

Root raster changes preserve finite-input validation, explicit zero bias when
inactive, retained-depth-only triangle-family offset eligibility and original
sign in both Z conventions. Color-only/depth-backed viewport regressions and
a subsequent no-backing8x6 reset pass. Old clear/state mechanics map to actual
native owners; generic LLRenderTarget scissored clear is neither implemented nor
claimed. Actual HUD bracket migration removed its only relevant product consumer.

Always-float scene does not make every attachment float. GBuffer/emissive,
refraction, sky-behind, display AA/UI and probe/history storage preserve original
HDR policy. Ordinary probes retain H→HDRR11/SDR8 snapshot, V→float Screen then
mip0→probe storage; supersample1 converts at mip0. Hero first mip uses HDR16/SDR8,
probe alpha0. SSR SDR history preserves RGBA8 alpha plus independent raw R32depth.
All three publication APIs take explicit HDR intent. Hero sampling rejects
render-only input before changing published cube identity/valid mask.

Original colorCorrectF135–189 applies flare and flare-lit dirt before HDR-only
exposure. Native nonHDR world RGB4 now survives the actual lighting/opaque owners
and exceeds unchanged gate2. RGBA8 max1 remains an exact-black negative control.
Six HDR-only gates removed while main/current epoch/frame/device/noPost/cutout/
hold/sky policies remain. Alternative standalone lighting path differed823 halfs
by at most1ULP; no tolerance waiver was introduced. The zero-reader alternative
was retired; actual in-place and batched live lighting remain bit-identical.

Level0 projection copies a validated full manager snapshot and changes only
config[0] to min(count,1) at exactly0. Readiness uses the same projected range.
Count0 bootstrap and invalid count failure policy remain. Actual refresh/
snapshot/projection bodies are extracted byte-identically for a narrow CPU facade
with real controls/schema and named camera input. GPU test supplies constants
directly and does not pretend to execute manager/UI callbacks. Water policy is a
wrapper calling the exact production builtin helper, not a copied algorithm or
full-water invocation; actual full-water tests remain separately mapped.

## Root registered validation

Evidence prefix: `artifacts/metal/feature-completion-20261111/`.
All completed commands below exited0 except explicitly retained reds.
Tests run as CLI binaries, never through LaunchServices or a product executable.

| Evidence receipt/log | Observed result |
| --- | --- |
| resource-morph-registered-validation.json | Native resource+FXAA9140assertions/5cases; morph13203/3, API/shader validation0. |
| llui-registered-validation.json | Native LLUI73292/28 and CPU LLUI4821716/1516 passed. |
| raster-registered-validation.json | llrender333661/438(437pass/one known CPAL font skip), Font/UI1862/61, native LLUI73292/28, morph13203/3 passed. |
| raster-ui-original-sign-validation.json | Affected Metal UI/selection/retained-background/RenderInfo36441/11 passed. Initial old wrong-sign expectation1/36440failure retained and corrected, not waived. |
| float-policy-registered-build.log | cmake --build build-Darwin-ninja-os-arm64 --config Release --target almetal_tests newview_tests --parallel8 exited0. |
| reflection-policy-registered-build.log | Same registered Release targets with new reflection GPU file, exit0. |
| float-reflection-registered-validation.json | CPU native lens73/3 plus reflection117/2 passed190/5. Matching-header root float105890/23, conversion3462/3, reflection1964/3 and UI36441/11 passed API/shader validation. |
| raster-native-doctor-host.log | Required host doctor0 after sandbox sysctl denial; Apple7 true, Apple8/9 false, actual recommended working set62,277,025,792bytes. |

Root test commands use the registered Release paths:
`build-Darwin-ninja-os-arm64/almetal/Release/almetal_tests.app/Contents/MacOS/almetal_tests`,
`newview/Release/newview_tests`, `llrender/Release/llrender_tests`,
`llui/Release/llui_metal_tests`, `llui/Release/llui_tests`,
`llappearance/Release/llappearance_metal_tests`, and alrender native target.
Use actual corresponding build-directory prefix, never execute a window-tagged
case. Each receipt preserves exact argv, source/WIP/binary hashes and log path.
GPU environment: MTL_DEBUG_LAYER=1, MTL_SHADER_VALIDATION=1,
MTL_SHADER_VALIDATION_REPORT_TO_STDERR=1.
Current root GPU filters: original23 persistent world/final-color/hero/ordinary/
base/fullbright/nonHDR lens cases, [linear-screen-boundaries], [reflection-policy],
and exact UI/selection/[retained-background]/[render-info] cohort.

## Isolated proof and superseded failures

Frozen receipts under /private/tmp (not product runtime):
- alchemy-native-alrender-fixture-validation-20261111: original assertion map,
  native pool/VB147/4, FXAA8993/1 and morph13203/3.
- alchemy-native-llui-validation-20261111:28cases/native fixture assertion/lifetime
  proof. Missing semantic blend, wrong asset root/fallback fonts and original
  wave-cache process lifetime were test setup corrections, not product bugs.
- alchemy-native-state-validation-20261111: baseline5472fail/17274assert4cases;
  fixed30858/12. Old-order-only mutant768fail/1570assert; original viewport
  inference1fail/13assert. Final native/scoped/clear/restore controls green.
- alchemy-native-float-screen-validation-20261111.json: persistent105890/23,
  offline Metal3.1 compile0, six Release/syntax units0; temporary resource identity
  case excluded from permanent coverage. Original scene format baseline retained.
- alchemy-native-linear-screen-boundary-validation-20261111:3462/3 exact bytes.
  Wrong initial SSR alpha0 assumption corrected to original preserved alpha;
  no production SSR alpha bug or tolerance expansion.
- alchemy-reflection-policy-validation-20261111.json: actual CPU baseline4fail/
  117assert2cases, fixed117/2. Full pack/debug payload unchanged.
- alchemy-native-reflection-policy-gpu-validation-20261111:1964/3, API/shader0.
  Supplemental authored float equality initially failed4asserts by1FP32ULP due
  original weight/reciprocal normalization; exact independent-pack equality
  remains primary. No production precision/math change.
- alchemy-native-unused-shader-validation-20261111: exact authorized projection,
  eight surviving loader bodies and41 surviving program lines byte-identical.
- alchemy-native-glsl-retirement-validation-20261111: manifest hashes/notice map
  and conditional120asset deletion, not canonical absence.

## Native shader and notice staging

Tracked notice SHA256:
`17bacda8956823fc0f740ff8bffa26b6215363eb69f2b50f5dff7f979bfeea75`.
Builtin source points to the source-tree notice and adjacent installed notice.
Viewer root WIP stages builtin MSL/metallib/notices in Resources; dependency
edges include generated metallib as well as source/notices. Optional utility
stages adjacent native artifacts and retains character data, removing the
obsolete GLSL copy. A plain CLI NSBundle resourcePath is its executable directory,
verified using a standalone Foundation-only probe (not the utility).
/private/tmp/alchemy-utility-resource-probe-20261111 contains before/after receipts.
Optional actual utility Release build0 and final frozen-source refresh0, fresh
stage zeroGLSL/app_settings, exact MSL/metallib/notices+all96character files,
ARM64/strict signature/imports0. Source drift during first build is explicit:
root notice wording changed; final refresh pinned066+WIP exact hashes.
Final /private/tmp/alchemy-native-appearance-resource-validation-20261111/
validation-receipt.json includes actual generated Ninja dependency edge dryruns:
clean baseline no-op, newer task-local MSL recompiles+relinks/poststages, newer
notice only relinks/poststages. No live shader timestamp/content modification.
Foundation-only adjacent probe finds all four resource paths; two sandbox Metal
compiler-cache failures retained and standard escalation build succeeded.
Prior stage with126GLSL files is preserved
as stale evidence. Install does not automatically prune previously staged assets.

## Conditional cleanup and next work

Integrated97d0b8a20e (childfce3ee8218) removes reflection3files +headlessGLfixture and32 dormant
listener lines; exact prior Darwin listener projection/all10guards and native
manual-window owners preserved, Release syntax0, independent review0. Integrate
after registered CPU117/2 and GPU1964/3 passed. Root removed stale alrender
GL target/AL_TEST_SHADER_DIR in WIP; menu owns obsolete GL mode/CI/docs closure.

Integrated066d9d4e53 (child6233c90f79) removes exactly120GLSL assets878,091bytes/
21,115lines.108match original byte-for-byte,12are modified. Root independently
verified exact120 manifest hashes/deletion change set and notice SHA before
integration; glsl-root-retirement-gates.json records reader/native replacement/
package gates. Root removed viewer IDE/install boundaries in WIP; fresh stage
verification remains pending.
Semantic shader filenames in native loader managers are identities, not file I/O.

Integrated pipeline dead-leaf49fa6b4401 (childc82a35e169): -612lines in pipeline.cpp/h,
native encode/DoF config/focus byte-identical, Release syntax0 and independent
review0. Live finalization still owns reference one-shot availability, lens-fit
solver, callbacks/statistics/scopes, exposure epochs and SSR eligibility.
Move those actual duties to their native owner before retiring traversal/storage.
Original-mapped nonHDR DoF/scope gates are next under exclusive RLVa owner.

Foreign audit /private/tmp/alchemy-foreign-media-audit-20261111/audit.json:
339VLC modules/import/disassembly census supports three windowed GL plugin
exclusion candidates only. Keep actual screen input, VT decoder and complete
required CEF capability payloads. Plugin enumeration can load unused modules;
callback vmem/window-none does not alone establish no GL loading. No foreign
runtime backend claim or whole-bundle native-only result.

Root committed all registrations/native notice/staging/IDE/install cleanup0fe.
For a genuinely fresh app stage, preserved the entire old app by rename before
configure:273stale GLSL files, receipt native-fixture-fresh-stage.json. Source
was clean0fe at freeze. Required doctor/configure0; full tools/metal/build.sh
currently runs as73684,1302actions, native-fixture-canonical-build.log. Root
tools49passed; content models12622/source audit79931 in progress. Keep source
frozen, then verify actual new app slices/deepcodesign/imports/native resources
and absence of GLSL. These are pending, not a successful app result.

Latest verified full canonical viewer remains d9f624ea46, Release26.4.0d68755:
doctor/configure/build/verify0,349ARM64/zero invalid, deepcodesign0 and main-viewer
forbidden imports0. Exact viewport canonical receipts remain authoritative.
New code requires a fresh frozen full build/static verification. Stability,
complete feature parity, runtime no-GL, independent release evidence and measured
multi-scene performance remain open. No product completion or speedup claim.
