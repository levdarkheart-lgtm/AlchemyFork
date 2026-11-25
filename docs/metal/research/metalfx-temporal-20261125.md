# MetalFX Temporal review — ready for owner live review

## Identity and scope

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
