# MetalFX Temporal review — corrected69501 ready for owner review

Current corrective scope: **PPD608/609**, source **64023b8b1656dec297a2658ff546f35ad06efe36**.
The final section records its allocation/cancellation evidence. The original69494 identity and results below
are preserved historical provenance; they are not measurements of the corrected viewer.

## Preserved69494 identity and scope

Parent `41e90aaadb938d77955f7c7e752362aab875fff6` was fetched from
`codex/metalfx-spatial-20261124` unchanged. Implementation uses the separate
`worktrees/metalfx-temporal` worktree and `codex/metalfx-temporal-20261125` branch.
The Spatial branch, its 69464 executable/evidence and the usual branch are preserved.
The owner authorizes this review branch's commits/push and private iCloud delivery;
this is not usual-branch integration, product acceptance or a release.

Review app **26.4.0.69494**, application source **663a591f2c543158d4e0bf5a68e4e7236f20a559**.
The subsequent `841358a5c8` changes only the new material-test oracle; documentation/register
commits likewise do not change the packaged application. Native remains the default and
saved Native/Spatial identifiers retain their meanings. Combined main lighting stays OFF.
No Home/Club login, Temporal FPS, live image-quality or real-scene motion-stability result is claimed.

| Artifact | SHA-256 |
| --- | --- |
| Executable | `d2efebe301410c19bfa06165dd913492c241c7864450bdd737cda274418a689d` |
| `almetal_builtin.metallib` | `b8662ac1995b4e7df0242d975269521c4580b74ab53dff9ab1cc556ee3ec4109` |
| `Alchemy-MetalFX-Temporal-69494-AppleSilicon.dmg` | `3ce21060fe6c2133126b1ad9c54fd1f56025641e70a8e5e093649bf5251d5047` |

Build host: Apple M1 Max/64 GiB, macOS26.5.2, Xcode26.6/SDK26.5. Deployment target26.0,
ARM64. Shader compiler options remain `-std=metal3.1 -ffast-math -fpreserve-invariance
-mmacosx-version-min=26.0`; exact inputs/options and receipts are in the private bundle.
The running login screen confirms **OpenHTJ2K0.27.0 pixel decoding**. OpenJPEG's separately
retained parsing/encoding runtime does not supply the pixel decoder.

## Integration map and decisions

- `indra/alrender/alrenderresourceinputs.h` centrally maps IDs0–4 unchanged and
  Temporal50/67/75 to IDs5/6/7. Exact rational scales use upward integer rounding,
  output-sized shadow references and native UI. Probes and independent captures keep their sizing.
- `indra/almetal/almetaltemporalscaler.{h,mm}` wraps classic `MTLFXTemporalScaler`.
  Device support, output/input scale limits, descriptor formats and queried usage requirements
  are preflighted. One stateful scaler serves a continuous main-world stream and is reused
  for compatible descriptors; it is not a round-robin Spatial cache.
- `pipeline_temporal_state_native.inl` selects reconstruction, resources and material policy
  before world rasterization. Unsupported underwater, RLVa sphere, no-post/legacy-sky,
  wireframe/debug and independent views use the existing unjittered fallback. No late
  material-policy switch is attempted after sampling.
- `pipeline_temporal_reconstruction_native.inl` reconstructs **linear RGBA16F HDR** inside the
  existing main post transaction, before bloom/tone mapping and native UI. Reduced-resolution
  DoF runs first with its matching authoritative depth. MetalFX does not supply reconstructed
  depth. Viewer exposure remains downstream; scaler auto-exposure is OFF, exposure/preExposure1.
  Motion/depth/reactive inputs are RG16F/R32F/R8Unorm.
- `almetalworld_temporal.inl` preserves current glow alpha separately using jitter-corrected
  sampling; vendor output alpha is not treated as viewer glow/coverage. Downstream lens-dirt
  dimensions follow the actual post source, including fallback. Temporal replaces FXAA/SMAA;
  CAS is also skipped to avoid stacking sharpening. Saved AA/sharpening preferences remain.
- Existing native commands, dependencies, resource generations and completion retirement retain
  scaler/resources. No new submission queue, admission policy, routine CPU readback or
  `waitUntilCompleted` is introduced.

The original Alchemy `1eb44bb` renderFinalize ordering was inspected as the external reference.
Native and late tone-mapped Spatial placement remain separate from the new HDR Temporal path.
Apple references: [Temporal API](https://developer.apple.com/documentation/metalfx/mtlfxtemporalscaler),
[reactive-mask semantics](https://developer.apple.com/documentation/metalfx/mtlfxtemporalscalerbase/reactivemasktexture),
[integration guidance](https://developer.apple.com/videos/play/wwdc2022/10103/).
Installed SDK declarations and actual M1 Max execution determine supported capabilities.

## Jitter, motion, history and fallback

Jitter uses texture-storage top-left/Y-down input pixels, applied only to copied raster matrices.
Logical camera/picking/UI remain unjittered; a one-input-pixel conservative culling guard covers
edge jitter. Halton2/3 cycles depend on actual scale:32/18/15 phases at50/67/75%. Motion is
**previous minus current**, unjittered input-pixel displacement, with MetalFX scale1/1 and
matching reversed-depth configuration. Invalid/behind-camera/nonfinite vectors reject history.

`almetaltemporalworldhistory.{h,cpp}` owns deep previous matrices and joint palettes from the
preceding successfully submitted Temporal world. Pending preparation does not mutate immutable
material publications. Stable producer and exact resource identities, vertex publication Signal
revision, geometry layout/ranges and UV changes prevent unrelated slots inheriting poses.
Ambiguous duplicate keys reject history instead of guessing. Pending/committed pose storage is
bounded to65536 draws/64 MiB each.

An abandoned transaction does not advance authoritative history/jitter. Successful encompassing
main-post submission advances it without waiting for CPU GPU-completion notification; existing
queue ordering preserves successive Temporal dependencies. First use, mode/descriptor changes,
teleport/camera cut, world-origin discontinuity, reload, interrupted history and GPU failure reset
history. UI-only redraws/repeated presentation and independent views do not advance it.
A late failure after jitter poisons/cancels publication, retains the previous valid world and resets
next use; it does not publish a jittered image through an ordinary scaler.

| Content family | Implemented treatment and limitation |
| --- | --- |
| Camera, rigid objects, attachments | Camera reprojection plus previous submitted object transforms; ambiguous instances reject. |
| Animated/skinned avatars and cloth | Previous joint palettes and procedural cloth parameters feed production deformation motion. Live animation stability awaits review. |
| Legacy/PBR alpha tests | Same UV/mip policy, alpha thresholds and current depth in color/motion coverage; production fixtures cover the principal families. |
| Blended hair, glass, particles | Covered pixels favor current color with reactive1 and retain opaque motion/depth. Transparent layers do not independently accumulate history. |
| Water | Conservative wave-height slab rejects history; camera inside the slab rejects the view. No explicit wave velocity; rejection can be excessive. |
| Sky/clouds/impostors | Reactive rejection; reconstruction without useful accumulation in those regions. Impostor generation is not mutated by the motion replay. |
| UV animation and CPU morph/topology changes | UV changes and geometry publication revisions invalidate previous poses. Texture-content-only animation is not explicitly classified. |
| Terrain changes | Current-depth camera reprojection and vendor depth rejection; no explicit terrain-edit motion or generation-based local mask. |

Temporal material bias is `log2(actual input width/output width)-1`, selected once per view
rather than added to Spatial bias. Native adds zero, Spatial keeps its original formula.
Matching alpha depth/color coverage uses the same policy; intentional environment/probe LOD,
UI/shadows and independent captures are preserved. No conservative bias adjustment was made
without live evidence. Existing streaming may not have finer mips resident; authored offline
mip fixtures do not establish top-mip residency or absence of live shimmer/moiré.

Same-size ordinary snapshots continue to render independently at native scene detail. Explicit-size,
cutout and other independent captures retain their existing contracts and never advance main history.

## Focused verification

Private root: `artifacts/metal/metalfx-temporal-20261125/`. Selected CPU receipts were copied from
the canonical worktree's same-named private root; each retains its original absolute provenance.
No account settings, credential stores, raw viewer log or captures are included in the review ZIP.

| Executed check | Result/evidence |
| --- | --- |
| Doctor; full Release build | Pass; `doctor.log`, `build-viewer-03.log`. |
| App architecture/dependencies | `verify_app.sh`:346 ARM64 images, no bad slices; `verify-app-01.log`. |
| Temporal scaler, post and material suites |18 cases/2,221,543 assertions pass with API/GPU validation; `temporal-agent-final-checks.json`, integrated03 log. |
| Production motion including vendor reconstruction |10 cases/51,365 assertions pass with API/GPU validation; `motion-final-02-receipt.json`. Real moving rigid/skinned inputs across submitted worlds, analytical camera/cloth vectors, alpha coverage, water, reversedZ, invalid vectors. |
| History/resource planning |13 cases/1289 assertions and34 cases/3356 assertions pass; `cpu-qualification-03/`. |
| Real UI/preset manager/XML |398 UI checks and116 preset checks pass; `cpu-fixtures-final-01/`. Saved scales/methods and older-preset compatibility retained. |
| Capture publication fixtures | Strict production stamps compile/run; tree ASan/UBSan7 cases/300 assertions; `capture-fixtures-01/`. |
| Existing regression suites |15/15 pass, API/GPU validation: Spatial/presentation/AA/cutout/reload/material/owned inputs, publication and tree; `regression-gpu-01.log`. |
| Manual external probe fixture | Absent from804 default CTests; explicitly listable hidden case. Not executed or counted as passing. |
| Login smoke | Exact69494 executable/PID, AutoLoginfalse, fullscreen login observed, no login; clean quit and17 original settings files byte-exact restored. `login-smoke/receipt-corrected.json`. |
| Package | Ad-hoc deep/strict codesign, DMG checksum/readonly mount, mounted executable/library SHA match; `dmg-verification.json`. Not notarized. |

Important retained failures: native scale-limit direction was initially misinterpreted; corrected
against real runtime limits. PPD601 caught a Temporal prefix surviving final validation failure,
602 an MRT attachment-count mismatch,603 glow jitter alignment,604 in-place vertex revisions,
605 invalid vectors,606 an unsafe impostor-generation route,607 lens-dirt dimensions. Their
source repairs and evidence remain in the register. Motion-final01's illegal new legacy fixture
was corrected to production routing without weakening validation. Two new fractional mip tests
initially used an invalid derivative-rescaling oracle: a13-pass native calibration showed bias
matches explicitLOD2+bias exactly, while native derivative/filter precision differs. Fixture-only
841358 replaced that oracle, retained tolerances and original assertions, and all18 cases pass.
A first smoke attempt lacked isolated-launch provenance; it is explicitly unqualified and was
replaced with the verified PID/arguments/fullscreen smoke. No live acceptance is inferred.

The12-world128x96 ownership fixture creates24 textures, with0 pool reuses/132 dependency deferrals
and1.125 MiB cached. Conservative opaque native closures defer post-target reuse; the existing pool
bounds retained storage. This exposes allocation churn, not a measured Home cost or zero Native
regression claim. Existing third-party media/plugin release gaps (including inherited PPD592) remain
separate. Builds/fixtures do not satisfy the whole product's release gates.

## Owner handoff

Install **Alchemy-MetalFX-Temporal-69494-AppleSilicon.dmg** from iCloud Desktop; the separate
**MetalFX-Temporal-69494-Review.zip** contains this report, source diff and compact verification.
The local app is `artifacts/metal/metalfx-temporal-20261125/review-69494/Alchemy MetalFX Temporal Review.app`.

Select Preferences → Graphics → Advanced → World resolution → **67% (MetalFX Temporal)**.
The effective status below the selector reports the successfully committed method, dimensions,
bias and fallback reason. A selection alone, or the login screen, does not prove Temporal ran.
Return to **67% (MetalFX Spatial)** or **Standard** for the existing paths. Keep combined main lighting
OFF (`RenderMetalMainLightingRenderPass=false`, existing default); no permanent owner settings changed.

Review stationary textures/thin edges, camera pan/rotation, independently moving and animated avatars,
foliage/hair disocclusion, glass/particles/water, native UI, resize/mode transitions and snapshots.
Watch for ghosting, shimmer, over-rejection and texture sharpness. This build is ready for that owner
live review; image-quality acceptance, scene-motion stability and FPS remain explicitly open under PPD598.


## Corrective review69501 — allocation ordering and pre-display cancellation

Fetched review head `dbff1c4f1ee78c3a6386a7075d27393f75025498` was unchanged and the Temporal worktree
was clean before correction. Application source is **64023b8b1656dec297a2658ff546f35ad06efe36**.
Private evidence: `artifacts/metal/metalfx-temporal-corrections-20261125/`.

PPD608's cause was resource acquisition after `nativeHandle()` correctly marked the command opaque.
All active downstream pooled targets are now acquired beforehand and carried as exact move-owned targets:
bloom levels, optional cross-filter targets, display transfer, final presentation and requested reference still.
Shared descriptor builders preserve the old bloom rounding/formats. Cached procedural lens dirt is prepared
before Temporal; its inputs need no reconstructed scene. Reference request consumption remains after valid
display. Exposure, flare and reduced-depth DoF are already before Temporal; SSR-history update and UI/scopes
are after settlement. Temporal-ineligible AA/CAS/legacy-glow/RLVa paths retain their existing behavior.
Partial preparation, mismatches and abandonment use existing RAII, pool and completion ownership. No opaque
or generation checks were weakened, and no production waits, queue or separate cache was added.

Settled12-world fixture at128x96, with a4-world warm-up (native API/GPU validation):

| Configuration | New textures | Pool reuses | Dependency deferrals | Cached bytes | New after warm4 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Historical69494, display/final only |24|0|132|1179648|16|
| Corrected, display/final only |2|22|0|98304|0|
| Corrected, bloom |6|66|0|114624|0|
| Corrected, halation/cross-filter/reference |10|110|0|198528|0|

The historical row comes from `temporal-agent-final-checks.json`, production663a591f2c/fixture841358a;
that exact preserved69494 configuration matches only the corrected minimal row. The optional-effect rows
have no historical comparison. Continued worlds require positive reuse and an exact allocation plateau,
not merely bounded storage. Two pending worlds retain distinct targets; odd/changed descriptors, abandonment
reuse, strict mismatches without replacement allocations and zero leaked texture/buffer storage also pass.
These are allocation/lifetime results, not FPS measurements.

PPD609's cause was cancellation nested inside `if(exact_base)`: missing dirt/grading skipped it and generic
cleanup could commit an unfinished Temporal batch. The owning viewer now checks the same Temporal-only
finalization rule before both commit routes. Flags are cleared before cancellation, making repeated cleanup
inert. It abandons pending pose/jitter work, invalidates the next history, clears pending current outputs,
reference and mode status, and leaves the existing `ALMetalRetainedBackground` authority intact.
Native/Spatial partial commits and the settlement observer's current-presentation requirement remain.

The new3-case orchestration fixture uses the production eligibility and cancellation functions with real
native commands/scaler/history. Dirt and grade are each unavailable **before the display consumer is called**.
It asserts no affected command submission, no authoritative pose/jitter advancement, exactly one rejection
settlement, no successful mode publication, identical retained-world lease/frame/generation/pixels, clean
teardown and reset/recovered submission. This extends rather than repeats late renderer-failure coverage.

Focused verification passes15 suites, including Temporal presentation7cases/362284 assertions,
orchestration3/264, scaler6/1041531, motion10/51365, history13/1289, resource plans, Native/Spatial post/lighting
admission, and actual finalize policy16/469. Exact command, environment, executable/library hashes and exit0
are retained in `focused-01-receipt.json` and `focused-01.log`. Metal API/GPU validation is enabled.
A first build invocation used the wrong test-target name and failed before compilation; the corrected build02
passes. No product defect is inferred from that setup error.

Release build69501 passes;346 ARM64 images, three owned executables' imports, and deep/strict signing pass.
Fullscreen login-only smoke uses the verified69501 executable/PID with AutoLoginfalse, then quits cleanly;
all17 original settings files are restored byte-exact. Original69494 artifacts and Spatial/usual branches remain preserved. All disclosed content-motion,
snapshot, exposure, mip, AA and sharpening policies are unchanged. Home/Club FPS and live quality remain
owner review; no new live traces or login are part of this correction.

For live comparison use macOS window screenshots or screen recording. The viewer's independent native-detail
snapshot path does not reliably represent the displayed Temporal reconstruction.


Corrected executable SHA-256: `e0eb97ac31bf08a354e7ad6cf7fd6a33197c5ff514b718c1df69488725bbf0d9`.
Shader-library SHA-256: `b8662ac1995b4e7df0242d975269521c4580b74ab53dff9ab1cc556ee3ec4109`, byte-identical
to69494. New DMG: `Alchemy-MetalFX-Temporal-69501-AppleSilicon.dmg`; compact source/verification ZIP:
`MetalFX-Temporal-69501-Review.zip`; checksums: `SHA256SUMS-Temporal-69501.txt`, on iCloud Desktop.
Deployment remains Apple Silicon/macOS26+, ad-hoc signed and not notarized. The bundle records the final
review-branch commit separately from application64023b8b16 and includes package hash/mounted-file verification.
Native remains default; select67% (MetalFX Temporal) under Advanced Graphics → World resolution and check
the committed effective status/reason after entering the world. Keep combined main lighting OFF. No remaining
blocker to owner review is known; live acceptance, broader content limitations and inherited PPD592 remain open.

Corrected DMG SHA-256: `5f10e12ce4738be5727ee70c7e1df61940d6405f852a422b5872cdd2fb76cb1e`.
Readonly-mounted executable/library hashes and deep/strict signature match the frozen69501 app.


## Active stationary foliage investigation — PPD610

Starting HEAD949944f7a9eb33da50d488a668d01622c8a523ac fetched unchanged; clean before edits.
Compared with69501 application64023b8b16, only four documentation files differ. Primary owner reference is
Untitled.mov SHA6065598f44f2cbce37ae1693bb716ff4718402a38a4effb1cf075a4bde82da99. Private evidence root:
`artifacts/metal/temporal-foliage-20261125/`. Native/Spatial/usual branches and69501 artifacts are preserved.

The5.826667s3440x1306 H264 MOV contains fine-change evidence in grass, dense leaves and palm silhouettes.
Narrow fence/pillar edges also fluctuate, while broad pillar faces and trunk texture are much steadier.
Registered trunk/fence framing stays within0.08pixel p95 approximate decoded-image translation. The215 declared
versus213 displayed frames are explained by the MOV edit list, not established recording loss. Compression and
lack of renderer metadata prohibit identifying draw paths, actual Temporal activation, or shader causality from
this recording. Private ROI notes and consecutive decoded-frame strips preserve those boundaries.

Source audit: built-in Linden tree and grass routes are opaque alpha tests; mesh foliage can use other paths.
Actual objects in the recording remain unidentified. Sky pixels and all forward draws receive full rejection;
Temporal67 bias is log2(input/output)-1. Classic/legacy/PBR color and motion coverage inspected so far agrees.
The suspected extra PBR alpha-factor mismatch was retracted because vertex colors already carry that factor.
Apple WWDC22/10103's sampling-offset example and the existing authored analytic fixture support the current
positive apparent-feature-translation jitter convention; internal consistency alone is not a stability test.
No jitter, mip or rejection change is accepted as a repair yet. Shared-buffer publication revisions are a measured
history-validity candidate, not permission to weaken PPD604's generation safeguard.

Next: production-path rendered Temporal67 stability fixture across complete jitter cycles, one-variable
rejection/mip/frozen-jitter diagnostics, and a prepared private build for exact-scene input/mask/history/pick
attribution. No broad FPS profiling, automatic live login, or claim of correction from encoding suites alone.

### PPD610 rendered causal checkpoint

Frozen baseline04 fixture exercises actual G-buffer alpha/depth, forward color, production motion/rejection,
MetalFX and resolve at256x128→384x192 with18 jitter phases,36 warm-up/54 stationary/18 move/36 stop frames.
Previous poses remain valid, stationary motion is below1.9e-6 input pixels, and color/motion depth agrees.
Sky rejection alone changes the mean adjacent RGB variation from0.03405→0.002826 in masked sky-backed leaves
and0.03167→0.002377 in grass; fixed straight-edge bands improve0.06969→0.003095. Recorded input PPMs and raw
depth/motion are byte-identical over18 phases, while masks change. Detail energy increases, so this is not blur.
All-forward rejection is unchanged and blended leaves remain0.01646→0.01648. The diagnostic is not a production
fix: proper infinity/background motion and dynamic sky masks are being implemented separately.

A one-variable coarser mip diagnostic (-1.585→-0.585) leaves masked sky/straight-edge crawl nearly unchanged;
blended variation improves but detail energy and recall worsen. No mip-policy change is justified by that test.
The test's original coverage oracle mixed effective LOD and binary AA fringes; a qualified supersampled matched-
LOD coverage reference is being added, without relaxing strict flicker/edge/detail assertions. Initial authoring
failures remain retained and do not count as production defects or successful measurements.

Private diagnostic69502 retains original69501 shader bytes and adds one-shot bounded ROI readback/picks only.
It is open fullscreen at the login screen, AutoLoginfalse, Temporal67, at the privately recorded scene destination. Owner login/camera
assistance has been requested; actual palms/grass paths remain unknown until that step. Diagnostic hooks are
private and must be removed before the review candidate. No Home FPS experiment or push occurs in this scope.

### PPD610 scoped corrections and remaining gate

The sky correction now uses direction-at-infinity camera reprojection and the actual composited modern-sky
inputs. Shared layer evaluation preserves color/glow alpha while deriving scoped dynamic-layer rejection;
only the final rejection union evaluates it, with coherent quad derivatives. A submit-owned sky identity
rejects discontinuous active map/layer/HDRI changes without treating camera movement/cloud scroll as resets.

The independent generic blended-leaf correction preserves pristine opaque depth in one pooled R32 target
before the forward continuation can mutate borrowed depth. Valid standard-alpha generic layers retain history
only when their motion agrees with underlying opaque motion within0.025input pixel. Invalid poses, UV
animation, missing snapshot, differing motion, particles and unsupported material families remain rejected.
MAX rejection is preserved; final visible depth still goes to MetalFX. No normal-frame CPU wait is added.
Avatar-only depth rows retain their prior conservative behavior; no broader avatar policy is inferred.

The matched sky-aware forward-reject control versus candidate comparison changes only the new acceptance
block. All18 HDR, motion, final-depth and pristine-depth phases are byte-identical; masks differ. Blended edge
variation falls0.0240313→0.0026645 (88.9%), large coverage toggles22.59%→0.0034%, and detail energy rises
0.0848321→0.142943 (Native0.15044). This is rendered fixture evidence, not a live-scene or FPS result.

Metal API/GPU validation passes26motion/sky cases and294703assertions, including a moving opaque object
behind depth-writing stationary glass: the preserved motion disagrees and still rejects history. Stationary
matching layers accept, UV animation/missing provenance reject, and final device depth remains unchanged.
The authored full quality regression remains failed at20strict bounds: thin coverage retention, small residual
variation and phase-matched settling. No thresholds were relaxed. Lossless output and labelled stationary/
move-stop sequences, exact source/library identities, failure lists and controls remain private.

Actual owner-recorded palm/grass material paths and live correction are still gated on the prepared exact-
scene login/camera step. The current code is a scoped correction candidate, not a completed PPD610 fix.
Do not substitute these offline improvements or passing ownership checks for that missing qualification.

Offline straight-edge localization from18 raw output phases finds only0.063pixel right-edge position span;
its regression against input jitter.x is-0.00747outputpixel/inputpixel with R²0.0138 (center R²0.0005).
Together with the analytical motion tests this weakens a shared jitter-sign/magnitude error; it does not prove
every material path. Remaining leaf modulation repeats by phase, with little same-phase drift, and is not
explained by repeated history resets in this fixture. Local foliage full-cycle display spans remain~10–14code
levels; the remaining coverage/settling failures are real qualification work, not discarded threshold noise.

Diagnostic69503 is built from949944f7a9 plus the private capture hook and local correction, using an explicit
version override (not a claim of a new Git commit). Full tools/metal/build.sh Release build and346ARM64
inspections pass. It is ad-hoc signed, open fullscreen at login with AutoLoginfalse; no live login or capture
has occurred. The initial library preserves69501 shader policy plus the new copy entry; candidate and source
hashes/diff are frozen alongside it. Original69501 and earlier69502 diagnostic remain intact.

The expanded snapshot lifecycle verification initially passes14/15selected suites; its new pending-world test
incorrectly assumed different native handles were necessary after submission. Resource planning explicitly
permits ordered generation reuse after commit. That test is being strengthened to check per-generation depth
results under pending GPU work. No production guard is being weakened. Settled snapshots already plateau at
three textures/reuse33/zero deferrals/110592cached bytes; optional arms plateau at7/11textures. Historical
no-snapshot configuration still reports2textures/reuse22. The first failed run is retained, not called passed.

The pending-generation ownership test is repaired and passes381193assertions/7cases with API/GPU validation.
The same R32 allocation advances generation1→2 while pending; retained per-world outputs preserve separate
0.625/0.375depth and analytically expected2.5/1.5pixel snapshot-derived motion. This verifies ordered reuse
rather than requiring unnecessary allocation. The other14selected regressions passed the earlier run unchanged.
No production code changed for this test repair. The authored quality fixture remains failed at20bounds.

Updated diagnostic69503 is PID7230, fullscreen at the login screen, with actual OpenHTJ2K0.27.0 decode logged.
Executable SHA4395d9bf88da90382d1dffeca353206cf4e86bcfbb4ad5de93dd8db881b0047d; initial compatible-original
library bdfba6aa36c88774ad5f6da62dc1219fa1dc7b2a2ac7f6228a7bd59ab301de26; candidate library
3302ce826403ddc4fb98864f1550633e4b52c9db43104fe5aaccf17f32b16f9f. Actual live paths, masks, pose stability
and scene correction remain unobserved pending owner login/camera assistance. Normal graphics/login controls
match the starting file; full17file restoration is deferred until this private-settings session exits.
No commit, push, finished review package or defect closure is claimed at this checkpoint.

Autonomous front-yard checkpoint (22:55UTC): the owner closed the viewer and authorized saved login.
Fresh QA-gated69503 baseline matches the owner’s house/central palm/grass/pillars/fence; the earlier pool-view
preflight is excluded from affected-pixel attribution.36consecutive Temporal worlds cover two18phase cycles,
with unchanged camera, no resets and maximum ROI motion component<0.000081inputpixel. Grass-right is generic
blended post-water mesh; grass-left also includes PBR BLEND. Their poses/MVP/UV remain valid/static36/36, yet
79.82%/94.02%of left/right ROI samples reject history. Palm/dense leaves are PBR MASK; surrounding sky is
fully rejected. Right-grass fixed-edge adjacent finalRGB MAE is0.02237232 (5.705/255), with frozen masks retained.
Actual scene/output2304x1278→3456x1916 is a separate display population from the earlier3456x2168 setup.
Private evidence lives under temporal-foliage-20261125/live/owner-view-original-stationary-1795646856960955.
The first candidate launch failed before rendering: a premature private LLPanelLogin helper request created
a duplicate event-pump name during panel construction. This is neither an activated shader comparison nor
a rendering result. Retry only after actual login-panel readiness. No quality closure or new push approval.

Verified actual-view3302 diagnostic: right-grass fixed-edge finalRGB variation falls58.39%, palm/sky81.44%,
dense47.28%, left-grass27.88%; mean-image edge energy increases. Across all7ROIs/36same-phase pairs,
G-bufferRGBA and pristine depth are byte-identical. This proves a real response to the rejection policy in
the reported view. However all grass Temporal depth samples still equal background opaque depth. Treat
3302 as causal/partial, not a finished transparent-input correction. The next local repair supplies nearest
qualifying generic/one-slotPBR-alpha motion and depth in the existing Temporal target, keeping the actual
viewer depth unchanged. A first mask replay compares all layers to fixed opaque motion; a second proxy
replay has mask writes disabled and selects strict nearest depth. Separate replay avoids rolling-reference
acceptance chains and epsilon depth drift found by independent review. Mip/jitter/quality policies stay unchanged.
The preserved69503 bundle is restored to its original-compatiblebdf library after both processes exit.

69504 verification:52focused motion/history/presentation/orchestration/sky cases pass678465assertions with
API/GPU validation. Separate mask/proxy replays use the same3attachments and add no textures; snapshot pool
plateaus remain3/7/11created,33/77/121reused,zero after warm4. The corrected strict fixture independently
checks98304blended foreground and69876background samples with zero depth-contract errors; all20original
image-quality bounds remain failed. The one coarser-mip counterfactual reduces blended detail energy95%→67%
ofNative for modest stability gain, so production sampling stays unchanged.

Actual69504raw inputs prove foreground proxies over78%/94%ofleft/right grass and rejection~2.45%/.88%,
versus~79.82%/94.02%original rejection. Original/corrected108world move-stop sequences adopt the same25camera
matrices exactly; final adoption differs2worlds. Grass and palm variation is lower over matched33stopped
worlds, with no obvious displaced duplicates in retained stills, but detail settling remains measurable.
Cross-session HDR illumination changes materially, so live MAE/gradient ratios are descriptive, not perfectly
isolated photometric quality gains. Controlled fixtures provide the mechanism isolation. Dense masked detail
remains a limitation; sparse opaque rejection spikes are not proof that all residual variation is vendor-only.

PPD611 source audit finds native_material_source.get() in the Temporal key, although publish() may create
distinct immutable wrappers for equal values. Earlier static producer80293 invalidates22/36times correlated
with address changes and otherwise unchanged recorded fields; older metadata lacks full semantic contents.
The scoped repair replaces address significance with canonical active material/texture tokens and exact
equality, preserving producer/native-generation/layout/duplicate guards and charging retained capacity to
the64MiBframe bound. Full semantic diagnostic capture and focused regression verification are in progress.


### 2026-11-26 corrective source checkpoint (PPD610/611)

The current code has three scoped corrections: directional sky motion with local dynamic-layer rejection;
separate nearest foreground alpha depth/motion after fixed-anchor MAX rejection; and exact material/resource
semantic history identity, independent of immutable wrapper addresses. No mip/sharpening/quality settings change.
The actual grass is generic/PBR BLEND; palms/dense leaves are PBR MASK meshes, not appearance-inferred trees.

Targeted54-world semantic proof retains36/24wrapper replacements with exactly equal named inputs/texture
generations and54/54valid poses. Four opaque centers reject44/41/53/53→0/0/0/0worlds. All72GBuffer crop files
and72pristine opaque-depth files are byte-identical at matched jitter phases. This validates PPD611, separately
from every remaining leaf-quality question. Reserved PBR UV lanes3/6/7 are excluded; used0/1/2/4/5 remain guarded.

Final correctness checks:14affected CTest suites pass API/GPU validation; focused history19cases/1851assertions
pass. Existing12-world pool configurations create2/3/7/11textures and reuse22/33/77/121, with0newtextures after
warm4 and0deferrals. The new snapshot is pooled; opaque generation guards and cancellation remain enforced.
Strict rendered foliage regression retains20failures among6201assertions (exit42), including thin detail,
phase variation and move-stop settling. It is a failing quality gate, not a passing encoding test or hidden skip.
The rejected coarser-mip counterfactual and all private capture hooks are absent from production source.

Earlier live records were windowed2304x1278→3456x1916 because macOS startup ignores the FullScreen control.
Several earlier arms also changed illumination; their absolute image metrics are descriptive, not isolated
final quality/FPS effects. Final original/corrected records use the actual native fullscreen window and fixed
Midday at2304x1446→3456x2168, same camera/mode/mip policy.36stationary worlds cover two18phase cycles;108world
move-stop arms adopt the same25actual camera states. Exact matched quality analysis and clean review package
identities will follow in the handoff. No claim of fully fixed foliage, native parity, zero ghosting or FPS gain.
Private source/video/metrics remain under artifacts/metal/temporal-foliage-20261125; no push authorization.


### Review69506: demonstrated correction and remaining quality boundary

Application source is `f969bd53245a26b79df3abcb41c047f28558f455`, parent949944f7a9, local review branch only.
The clean Release app69506 removes all private readback/mip-counterfactual hooks. ExecutableSHA256
`4a95d21a6ed38594346bf5ae81d6edc708655e6ce747030d9629a326c3b14e2e`; shader librarySHA256
`f9f6735cf82b2a8a9a1232b30e122ce992ceb341f65c97dfab9b4158d832297b`. Native remains default.

Final actualfullscreen/fixedMidday36-world comparison (2304x1446→3456x2168, Temporal67, originalbias/jitter):

| Region | Original→corrected adjacent-world luma change,8bit levels |
| --- | --- |
| Left/right grass |3.818/3.766→.588/.581|
| Dense foliage |2.967→1.144|
| Palm/sky |6.233→1.413|
| Pillar/fence controls |.806/.625→.475/.613|

SixROI GBuffer/depth crop files match exactly across36phase-pairs; trunk has small genuine foreground
differences and is excluded from exact-input claims. HDR brightness differs, particularly dense/palm
foreground−6.8%/−6.6%. Mean-normalized source variation differs≤1.7%; normalized output reductions remain
grass≈85%, dense60%, palm77%. Mean-image gradients increase9–46%in foliage, opposing a simple blur explanation.
These are scene-specific phase-balanced image differences, not independent statistical trials or native parity.
No corrected opaque samples are fully rejected in any of sevenROIs; changed/dynamic background remains scoped.

Actual-output clips include stationary controls and108-world move-stop arms with the same25camera matrices.
Stop-relative comparison uses30common static worlds; no reset occurs. Selected stills show no obvious
persistent displaced duplicate, but grass is initially softer and fine detail continues settling. Strict
fixture08 still fails20/6201assertions, principally detail/coverage/variance/settling. It detects the original
failure and is retained unchanged as an open quality gate. Native is the concrete alternative when these
Temporal limits are unacceptable; no claim that the remaining effect is an intrinsic vendor defect.

Quiet ordinary20s sanity (readbacks/profilers off, same fullscreen camera/fixedMidday) observes433/398joined
successful worlds:21.64→19.90/s, median45.61→49.52ms,p9549.48→55.49ms. This≈8%lower observed rate is a potential
cost, not an FPS improvement or isolated regression proof: no restored control,1/11enclosed sequence holes
and differing probe/update/upload observations. Full joins and exclusions remain private; do not transfer
windowed results or synchronous capture cadence into this observation. No additional performance campaign.

Build,14affected correctness suites with API/GPU validation,346ARM64 slices, owned imports and deep/strict
ad-hoc signature pass. Deployment remains macOS26.0; no notarization. Login-screen smoke has AutoLoginfalse;
explicit authorized saved login supplies the short cost check. Shared environment and original settings are
restored; viewer closes normally. PPD611 is ready for owner review; PPD610 remains open for residual quality.
Private review package/COMPARISON/source patch/stills/clips/checksums are under temporal-foliage-20261125.
The independent snapshot path is unsuitable for judging displayed Temporal quality; use macOS recordings.

Review69506DMG read-only verification matches all8272app entries (7839files,2symlinks,431directories),
with346ARM64Mach-O checks and deep/strict signature passing. Native fullscreen login smoke and the authorized
Home sanity session both finish normally. Original normal/custom graphics settings are byte-restored and
shared environment is restored; no viewer remains running. The correction source commit above differs from
the later documentation-only handoff commit. No push or usual/Spatial branch modification is performed.

Delivery metadata confirms all three privately copied iCloudDesktop files uploaded with no uploaderror at
2026-11-26T00:20:24UTC. The initial4355account-access error recovered without account/settings changes.
Files: Alchemy-MetalFX-Temporal-Foliage-69506-AppleSilicon.dmg, MetalFX-Temporal-Foliage-69506-Review.zip and
MetalFX-Temporal-Foliage-69506-SHA256SUMS.txt. Checksums/final delivery receipt remain beside private evidence.
Ready for owner review of the demonstrated correction and residual limits; no new push approval or FPS claim.


### Current publication authorization — 2026-11-26

The owner explicitly requests publication of the foliage corrections, tests and documentation to
`codex/metalfx-temporal-20261125` so another AI can review. This supersedes earlier no-push checkpoint
statements, without changing pending quality acceptance or the20strict rendered failures. Private media,
raw evidence and packages remain local/iCloud; usual and Spatial branches are unchanged. Review69506 still
uses application sourcef969bd5324; later commits are documentation only. No rebuild or new measurements.
