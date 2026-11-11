# Current engineering state — remaining native feature completion

**Observed:** 2026-11-11 UTC, Apple M1 Max /64GiB /macOS26.5.2 /Xcode26.6.
**Branch:** `codex/apple-silicon-metal`.
**Renderer code checkpoint:** `8ce7e33f71` (identified fixes integrated; combined build pending).
**Owner-passed viewer:** `f254feb048`,26.4.0d68817; provisional “pass...for now”.
Alpha4 source114 is built/tested/packaged; newer previews are source-tested. Owner superseded the earlier
no-launch instruction and requested the ready viewer for manual confirmation.
Fresh26.4.0d68817 launched with open -na; exact new executable PID90460 was
confirmed alive. No login action or screenshots by the agent; utility unexecuted.
Device-only Metal tests and separate CPU/Foundation probes are bounded evidence.
The owner-stopped20FPS campaign remains deferred.

## Current implementation

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

Root owns canonical integration/docs; child source released and GPU jobs closed.
114 and later handoff6bf pushed normally to private origin feature branch; new
fixes await combined validation/push. CLI/keyring works. Tag a Build43094454819
failed before steps for account payments/spending limit; no tag/build/release
created or billing changes. Owner requested manual ZIP+SHA instead. Delivered
68825/114 stays immutable; no further CI dispatch/upload/publishing.

HUD selected faces208 and diagnostics578 restore original native capture/blend/
mask duties. Actual6case18150 and3case61654 checks pass after813/15baseline
failures, with independent peers/production syntax checks. Diagnostic shader-
validation property was not directly probed; full selectedTE/HUD scenes unrun.
Preview39 restores3current-store gates: actual helper1494/1 and combined3354/7
API/GPU pass after13baseline failures. LLVB4a6 adds per-buffer prepare, preserving
vertex-only draws and independent buffers; actual19875/4API/GPU pass. Consumers
cc788/ef0/175 cover previews, UI/HUD/outline and31pipeline owners (5vertex-only).
Actual cached-UI82335/5pass after2baseline failures; pipeline306/5pass after35.
Final8ce7 integrates child8bfb:4preview destination retries and early bake metadata
bootstrap. Actual old bodies2123/10 expose5failures; fixed3991/10 and combined
7357/18API/GPU pass. CPU reset12/1,6productionReleaseTUs and independent peer
pass. Healthy images/metadata-valid bake adoption remain allocation-independent.
Full scene/physical OOM testing unexecuted; no new identified code fix pending.

Next: freeze docs, doctor/configure/full build as Alpha4 in isolated followup
tree; preserve old Alpha4 before configure and keep owner-passed f254 untouched.
Run matching full LLRender/alrender, affected policy/static/strict source checks,
then normal feature-branch push and new versioned ZIP+SHA with extraction/signature
verification. No viewer/window/utility launch or screenshots. Research files
under docs/metal/research are untracked concurrent work; preserve/exclude them.
All detailed source/peer/failure receipts are in the linked followup note.

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
