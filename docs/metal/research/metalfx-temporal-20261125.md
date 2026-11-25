# MetalFX Temporal review — implementation in progress

Parent: `41e90aaadb938d77955f7c7e752362aab875fff6`, fetched from the Spatial review branch unchanged.
Feature branch: `codex/metalfx-temporal-20261125`; separate worktree, preserved Spatial app69464 and evidence.
Owner authorizes local engineering, login when necessary, review-branch push and private iCloud packaging.
No usual-branch integration or release. Live quality/FPS acceptance remains owner review.

## Integration contract

Classic `MTLFXTemporalScaler` is new. Original Alchemy `1eb44bb` renderFinalize owns linear HDR world
color before exposure/DoF/bloom/tonemap, then AA/CAS/final lens/print and native UI. Preserve that ordering
except Temporal supplies AA on HDR before display transfer. Existing late perceptual Spatial remains separate.

- Input: reduced main-world RGBA16F linear HDR after world geometry/forward effects, with input-size
  depth, RG16F unjittered current-to-previous input-pixel motion and documented R8 reactive rejection.
  Exposure is not baked into radiance: MetalFX fixed exposure/preExposure1; existing viewer adaptation downstream.
- Reconstruction: existing main post command, before tone mapping. Preserve alpha/glow separately; MetalFX only
  reconstructs RGB. Depth consumers retain an explicit input-resolution contract or a separately resampled depth.
- History: one main-world serialized scaler, authoritative previous inputs advance only on successful encompassing
  transaction submission; uncommitted abandonment does not advance; GPU error invalidates later history.
- Jitter: texture-storage top-left/Ydown pixel offsets supplied independently from unjittered motion. Logical
  camera/picking/UI remain unjittered. Previous matrices/palettes refer to preceding submitted Temporal world.
- Early preflight chooses effective reconstruction/material policy. Expected unsupported views use existing
  unjittered fallback. A late failure with jitter cancels publication, retains old world and resets next history.
- Reset: first use, mode/descriptor changes, cuts/teleports/origin changes, missing generations, recovery.
  Ordinary object/camera/skeletal movement remains continuous. Probes/shadows/snapshots never advance history.
- Policy: stable IDs0–4 retained, Temporal50/67/75 use5/6/7. Temporal bias log2(actual input/output)-1, one
  method-specific policy. FXAA/SMAA/CAS preferences preserved but not stacked while Temporal executes.

This is the implementation map, not evidence of completed code or successful runtime tests.

## Focused inherited repairs

PPD599: successful ordinary no-post/legacy-sky linear fallback must publish effective status.
PPD600: remove the old quiet-session mip-disable override; preserve exact old executable/evidence.
PPD597: externally supplied hidden/manual probe cases must not be registered as an empty automatic suite.

## Evidence

Private root: `artifacts/metal/metalfx-temporal-20261125/`. Doctor passes on M1 Max/64GiB, macOS26.5.2,
Xcode26.6/SDK26.5, ARM64. Installed classic header provides input/output formats, scale limits,
reactive-mask enabled/format and usage queries; no jittered-motion switch. Tests/build/package pending.

Primary references: [Temporal API](https://developer.apple.com/documentation/metalfx/mtlfxtemporalscaler),
[reactive mask semantics](https://developer.apple.com/documentation/metalfx/mtlfxtemporalscalerbase/reactivemasktexture),
[Apple integration guidance](https://developer.apple.com/videos/play/wwdc2022/10103/).
