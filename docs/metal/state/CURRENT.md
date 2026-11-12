# Current engineering state — remaining native feature completion

**Observed:** 2026-11-12 UTC, Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Renderer code checkpoint:** `b7bc118d68`; matching source cb840/68841 built/checked/launched.
**Owner-passed viewer:** `cb840a5b9d`,26.4.0.68841; reported beard-light cutoff fixed.
Alpha4 built/tested/packaged/published; [profile](../research/busy-region-profile-2026-11-12.md)301.302sec+11.912sec; exact68841 PID91442/92651 owner login/settings/logout, no source change.
[Shadow queries](../work/active/shadow-camera-occlusion.md):68849/tests pass;181.281s recovered; [hotspots](../research/three-profile-hotspots-2026-11-12.md) compared. [Apple-first study](../work/active/apple-first-architecture-study.md): M1Metal4 core objects verified, retained-data/parallel/GPU-indexed direction; no implementation or gain claim. Utility unexecuted/stopped20FPS deferred.
Local [OpenHTJ2K experiment](../work/active/openhtj2k-local-experiment.md), PPD-005:
db398/68860 build/ARM64/signature+76tools+736image assertions pass; PID67814/4OpenHTworkers observed,181.755s [profile](../research/openhtj2k-profile-2026-11-12.md) saved. Owner accepted decoder experiment and scoped decoder push completed at4842cb9460 on codex/apple-silicon-metal; unrelated work stays local, no causal FPS claim.
## Current implementation

[Adaptive decoder admission](../work/active/adaptive-decoder-concurrency.md):[eight-burst trial](../work/active/eight-burst-decoder-experiment.md) owner-rejected for slower textures/FPS; both captures saved and source restored exactly98f41(localrevert59e50). Prior68880 PID63726 normal4/max6/adaptive1 relaunched, cache retained/recording off. Owner explicitly approves4/6 plus named shadow-occlusion/projector repairs/tests; published4976f233b8 on codex/apple-silicon-metal. Restored44cases1222assertions+76tools pass; five untracked notes/raw captures excluded.
PPD-001 [projector admission](../work/active/projector-per-light-admission.md):6404/68869; [warm analysis](../research/warm-cache-profile-analysis-2026-11-12.md):142.911decodeCPU-s,42.43%main shadows; late decode0.645s/22.377FPS, GPUinflight69.823ms(avg overlapping spans). Push explicitly approved; scene verification pending, PPD-002 cause unproved. Next palette reuse + GPUencoder attribution.
Mandatory [post-port defects](../POST_PORT_DEFECTS.md): PPD-001–004/006–007/009–010/014–015 unresolved;005/008/011–013 owner-verified. PPD-008 [black login](../work/active/login-black-world-investigation.md): verifiedb0585e/68894;133source+31,825GPU assertions pass; PID8114 prepares beforelogin, firstworld/fade20:23:29 and owner confirms resolved. Owner explicitly approves scoped push2026-11-12; native-music work excluded. PPD-007 [font audit](../work/active/font-position-cache-audit.md) separate. PPD-009 [music indicator](../work/active/music-startup-indicator.md): local9f8d175/68889 build/static+22checks pass, exactPID77975 launched/cache retained; owner explicitly approves push. PPD-010–012 [native music](../work/active/native-parcel-music.md): exactf826/68898/PID26265 owner-verified;271checks/14cases+build/ARM64/signature/source pass, WebRTC preserved; music scope published8f11241c9 after explicit approval. Prior approved scope published802250e82c. PPD-013 [water angles](../work/active/water-angle-artifacts.md): exacta6f4/Water Haze Fix68894/PID49892 owner-tested and confirms fixed; GPU171318pass versus8baselinefails+32197existing checks/build/static pass. Explicit scoped push approved. PPD-015 separateeye/agentsteepness mismatch open/unfixed. Independent remoteea1manualrelease work not integrated.
Unapproved2c11/5ed push reverted as0f7; original beard-light68841 remains owner-passed.
Keep future fixes local until explicit owner approval to push after testing.

[Latest finalization/preview/media closure](../work/active/native-finalization-and-media-closure.md)
and [settings/resource tail](../work/active/native-settings-and-resource-tail.md)
plus [post-launch corrections](../work/active/native-post-launch-corrections.md)
and [HUD selection/preview followup](../work/active/native-hud-selection-and-preview-retry.md)
extends [raster/float/reflection and fixture admission](../work/active/native-raster-and-legacy-fixture-admission.md),
[Scene Monitor/UI/RenderInfo](../work/active/native-scene-ui-and-render-info.md),
[target/query/font](../work/active/native-target-query-invariants.md) and
[cache/coverage](../work/active/native-pipeline-cache-and-coverage.md).
Original source `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` is authoritative.

Preserve prior RLVa sphere/silhouette/current-camera, viewport/HUD, diagnostic,
float-Screen, reflection-level-zero, native UI/raster/font, resource/lease and
developer-menu repairs. Existing non-rendering features remain in scope.
120GLSL assets, orphan fixtures/backends/programs/metadata and unused buffers
were retired after original invariant/reader mapping; native notices are staged.

Native finalization retains lens preflight, SDR DoF/scopes, one-shot reference/
callbacks and exposure epochs without the displaced GL post traversal. Bloom
works with Scopes closed; native SSR isolation retires history while valid probe
lighting remains presentable. Reference stills always store RGB10_A2 and survive
HDR/SDR changes. Preview first-allocation retry and VLC lifecycle/order are fixed.
Eight fixed targets and the prewarm-only transient pool are retired. Native
Bloom shape uses actual post-source dimensions, including snapshot overrides.

SMAA bfff8/dd796 restores original stencil and output-sized sample metrics.
Alpha5fa7/b20 restores HDR10/SDR8 color/rawF16 final coverage with BilinearMirror
and one quantization; early-U8 workaround retired. Root focused/world/reload pass.
Cutout3b5 retires duplicate graphs; exact stream boundaries/live gDebug remain.

Phase d78/c65 retires34 skipped descriptions/three dead copies/unused helper;
physical plans/native owners/full inputs remain. Snapshot4f5 restores forced-
failure admission/recovery without mFullSize guard. Preview7f66/Heroce65 retire
proven dead branches/wrapper; live specializations remain. Water6473/8356 restores
normal filtering/optional Compute culling;2c16 removes only redundant input copy.

Post-launch source audit found additional issues; earlier completion assessment
was premature. Integrated9838 fixes inherited feature-mask off clearing all bits;
65b5 restores dirty1 avatar-preview morph refresh;158f retries/current-qualifies
preview vertex/index stores; a380/8a14 fixes RGB8 media source stride/opaque alpha.
Child CPU/GPU/source peers and matching combined root build/tests pass.

## Observed validation

Evidence: `artifacts/metal/feature-completion-20261111/`.
Frozen clean f254 doctor/configure/full build1447 and static69509 exit0.
Version26.4.0d68817:346ARM64 Mach-O/zero invalid slices, strict deep signature0,
main viewer and both owned media hosts forbidden direct imports/frameworks0.
Fresh332VLC plugin names/bytes exactly match old335 minus the three exclusions.
Keep screen/VT/CVPX/vmem and complete CEF capabilities. Foreign links/runtime
remain separate; no whole-bundle native-only claim. ZeroGLSL/no old shader tree;
exact native MSL/metallib/notices. Previous stages preserved by rename.
Receipt: native-settings-phase-canonical-{build-state,validation}.json.
Viewer SHA256285c561302672cb9fab2fe4ab4ef402297796ca2272b579765d57b3abd4ab1eb.
Prior Info.plist staging defect and repair evidence remain in the linked note.
Keep owner-passed f254 app untouched.

Matching root77976: policy652/20, full alrender563966/115, focused water/cutout/
SMAA11054/19, world105890/23, water-cull250/1 and reload2304/8 all pass. GPU
logs confirm API/GPU validation. Exact argv/hashes: native-settings-phase-
registered-validation.json. Tested binaries unchanged after packaging repair.
Snapshot actual-body184/0fail passes again at clean f254 after16 baseline fails;
real water caller policy352/1 passes after6 baseline flags. Normal389/2 red17→0
and independent source/physical-plan/preview/hero peers remain source-pinned.
Counts overlap; earlier full LLRender335391/442(one CPAL skip) is older evidence.

Production f254 strict3504files/909info/0blockers/0errors passed. Earlier tests
4094files flags two negative SDL_WINDOW_OPENGL==0 assertions; retain both, no
all-source strict pass or manual window execution. Tools49/four content models
passed at unchanged-tool state. Root harness32files last passed; rerun after docs.
Optional utility f254 build/static83133 exits0: ARM64/signature/direct imports,
exact same native MSL/metallib/notices,96character names/bytes, zeroGLSL. Receipt:
native-settings-phase-appearance-validation.json. Utility not executed.
Launch31758 exits0; exact process90460/new verified binary alive after launch.
native-settings-phase-viewer-launch.json. Actual viewer parity not inferred.
Ordinary exhausted-plan audit finds no concrete ownership/freshness defect:
independent exact typed pool recovery clears storage and retains commit/current-
frame publication gates. No !mFullSize guard. Forced pool-failure injection is a
separate unexecuted diagnostic question, not a proved rendering bug.
/private/tmp/alchemy-native-exhausted-plan-audit-20261111.json.
Owner subsequently provisionally passed68817. Exact7,840file/link/byte baseline
copy and strict signature0: manual-pass-baseline-receipt.json. App path untouched.
New fixes: mask80checks red44→0; normal preview20red3→0; realLLVB/publication
combined1860/6 API/GPU pass; media CPU831/2pass after same-source599/2red112.
Tools d053 honor isolated configure -B; all50tools tests/shell syntax pass.
Real new followup directory bootstrap/wrapper configure75939 exit0, existing
dependencies read with installOFF, passed binary hash unchanged. No new app run.
Combined clean114fedf245: doctor/configure/full build98310 exit0 in followup tree.
Root55175: policy652/20; LLRender336356assertions/445passed+one known CPAL skip;
alrender563966/115 pass, device suites confirm API/GPU validation. Static44362
passes346ARM64/332exact VLC/signature/import/resource/zeroGLSL checks. Strict
source3505files/911info/0blockers/0errors. Receipts: followup-{canonical-build-state,
canonical-validation,registered-validation,strict-source}.json.
Same source staged as Alchemy Metal Alpha4 in Release, LTO OFF;83415/7544 exit0.
Renderer test hashes unchanged; identity+policy662/21 pass. ZIP84942 exit0:
319936679bytes, all7840files/links/modes exact after extraction, strict signature0.
artifacts/metal/releases/26.4.0.68825-alpha4/ contains ZIP and ZIP.sha256.
manual-alpha-{build-state,validation,zip-validation}.json records exact commands,
hashes and identity. No viewer launch or upload. Passed f254 binary unchanged.

## Exact next actions

All identified source fixes integrated: preview/HUD duties39/208/578,
per-owner current-buffer preparation4a6/cc788/ef0/175 and final8ce7 recovery for
four preview destinations/early bake metadata. Source peers/original projections
and red/green controls are recorded in the linked followup note. No identified
code fix pending; full scenes/physical OOM remain unexecuted.

Combined9f42/26.4.0.68836 Release/LTO OFF: doctor/configure/build0/0/0;
matching policy652/20, LLRender453086assertions/474passed+one known CPAL skip,
alrender563966/115 pass. Both renderer suites confirm API/GPU validation. Static
346ARM64/332exact VLC/signature/viewer+owned-host imports/resources/zeroGLSL pass.
Strict3514files/912info/0blockers/0errors. ZIP319934684bytes; all7840files/links
and modes exact after extraction; source/extracted signature and checksum0.
SHA2567e74d96fb3c991286fd4bbb44ac56fae3f21a72f9aed0b7e58d2f77535d2314e.
Receipts: hud-preview-alpha-{foreground-build-state,registered-validation,
validation,strict-source,zip-validation,checks-state}.json. Original background
attempt intentionally interrupted130; unchanged source/objects resumed at normal
priority. Temporary job wrapper restart was blocked by receipt guard; removed.
All build/check jobs now terminal/removed; original f254 and older ZIP unchanged.

Owner-authorized release442275652 replaced manually:26.4.0.68841-alpha4.
https://github.com/levdarkheart-lgtm/AlchemyFork/releases/tag/26.4.0.68841-alpha4
ZIP319935071bytes SHA85a19e063a5a8d3504e7c657075d84ab7f1e05dabfe6ebd7741e6c4fc0b62580;
new ZIP+SHA server digests/sizes verified; tag exactcb840. Oldtag9f42 unchanged.
See facelight-cutoff-20261111/release-replacement.json. No Actions/billing changes.
Prior port/release request and this scoped correction are complete.
No viewer operation during upload; prior requested exact launch remains recorded.
Concurrent research files untouched/excluded. Full-product gates remain open.

## Complete-product advancement assessment

| ID | State | Evidence / next boundary |
| --- | --- | --- |
| M-008 | doing | Original SMAA/cutout/optical duties advance; broader parity remains. |
| M-011 | doing | Source/static/build pass; owner confirmation/foreign/runtime remain. |
| M-012 | complete | Prior tooling retained; requested exact-build CLI launch observed. |
| M-013 | doing | Original-mapped RLVa repairs retained; full behavior unverified. |
| M-014 | deferred | Owner-stopped20FPS campaign remains deferred. |

Full original feature parity, prolonged stability, strict no-GL runtime,
independent build/review evidence and measured multi-scene performance remain open.
Preserve earlier lighting, self-turn, camera and voice issues in their notes.
Build/test/source success alone does not establish product completion.
