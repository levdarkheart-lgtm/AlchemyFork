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
