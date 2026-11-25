# Spatial quality review — PPD596 / build 69464

Keep developing the optional Spatial path, with **67% as the balanced review choice**.
75% gives the best scaled still-image quality; 50% prioritizes throughput and retains
obvious thin-edge artifacts. The mip policy modestly improves material detail, but
cannot recover missing raster samples. Native remains the default. A bounded Temporal
prototype is the recommended next major quality task, aimed at the remaining fine-detail
reconstruction limit; no Temporal implementation was started here.

## Exact build and implementation

Worktree `worktrees/metalfx-spatial`, branch `codex/metalfx-spatial-20261124`, starts at
`091243d236`, retaining the preset/fallback/snapshot fixes since 69457. Application
source is **a5142ae5c2c6dd1fe8b2fa89c9479eb213cff0f5**, version **26.4.0.69464**.
Later test-fixture and documentation commits do not change that executable's identity.

- App: `build-metalfx-spatial/newview/Release/Alchemy MetalFX Review.app`.
- CPU SHA256: `6c498099bb4acde29e0cc9033dc2faa81c978df4fe3f9b35a37e30190518b05d`.
- Library SHA256: `087705e4082de00098f9e52fd2a95b5d10688151fde0cceae71d84e35b3ef607`.
- Private evidence: `artifacts/metal/metalfx-quality-20261124/`.
- Owner authorized publication to `codex/metalfx-spatial-20261124` on 2026-11-25 for AI review.
  Private captures/bundles stay local. Review packaging is ad-hoc, Apple Silicon/macOS26+, not notarized.

`ALRenderResourceInputs` centrally maps stable saved choices to reconstruction method
and exact rational scale. IDs 0/1/2 retain Native, linear50 and Spatial50; IDs 3/4 add
Spatial2/3 and3/4. One overflow-safe calculation rounds each scene dimension upward.
Explicit scaled modes retain output-based shadow references, native-resolution UI,
independent probe/hero sizing and the existing bounded scaler/completion ownership.
Normal settings, control callbacks and preset save/load use the same saved choice.
Older presets without the key select Standard. No compensating shadow setting writes.

Before material draws, an ordinary main view freezes
`log2(actual_scene_width / actual_output_width)`, following Apple's
[Spatial recommendation](https://developer.apple.com/videos/play/wwdc2022/10103/).
It does not use Temporal's additional minus one. A dedicated fragment float4 at slot10
keeps cached material packets immutable. Relevant legacy/PBR samples use shader bias;
terrain detail gradients are scaled once by `exp2(bias)`. Alpha color/depth draws share
the policy and keep their existing thresholds. Explicit environment/probe LODs, water,
control maps, UI and shadow generation are unchanged. Independent captures, snapshots,
impostors, debug views, Native and the saved linear50 mode remain unbiased.

The validated M1 Max colored-mip check confirms shader bias works, while descriptor
bias has no effect on this Apple7 device; Apple lists that newer descriptor feature
for Apple10 in its [feature table](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf).
CPU demand remains output-based. Resident mip0 already starts at the J2K discard;
negative bias cannot recover unloaded levels. The observed pressure log reports the
neutral discard bias1, image6887MiB versus target39643MiB, no system/OS low-memory flag.
No streaming policy was changed.

Late AA/scaler fallback retains and reports the material bias already used, plus the
actual linear method, dimensions and reason. It cannot retroactively select mips.
MetalFX keeps its existing sharpening policy; stored CAS applies to Native/linear paths.
Ordinary same-size snapshots still render independent native scene detail, so these
quality captures use the live window. Custom-size/cutout contracts are retained.
Combined lighting stays OFF. A nonpersistent, quiet-session-only override permits the
same-executable unbiased comparison; it is not a normal graphics preference.

## Correctness

Full Release build, deep/strict signing and 346 ARM64 inspections pass on M1 Max.
Three CPU suites cover resource dimensions/odd sizes/transitions, actual graphics UI
bodies, and temporary real preset XML save/change/load for all choices. Fourteen relevant
Metal suites pass with API/GPU Validation: real legacy/PBR mip samples, alpha color/depth
coverage, terrain gradients, owned-input parity, zero-bias recovery, scaler reuse,
presentation failure, cutout, AA, formats and reload. New alpha-fixture environment
omission was corrected without weakening production validation or assertions. The old
hidden probe suite requires external source inputs and is excluded, not a new experiment.
The private qualification helpers pass38 synthetic tests, including stale-policy endpoints.

## Bounded live results

Same executable/library, fullscreen Home camera, output **3456×2168**, original planned
shadows, SMAA/other controls fixed, combined lighting OFF; Instruments/manual profiling OFF.
Actual decoder OpenHTJ2K0.27 verified. Every BASE census in the rows below is53 probes.
Rates count identifiable successful source-policy-matched GPU world completions, not
application iterations or physical scanout. All source-frame gaps remain in interval tails.

| Arm | Scene | Reconstruction / bias | Completions / seconds | Observed worlds/s | Median / p95 ms |
| --- | --- | --- | --- | --- | --- |
| Native N2 | 3456×2168 | Native /0 |380 /25.0937 |15.14 |65.67 /69.71 |
|50% control A02 |1728×1084 |MetalFX /0 |894 /25.0537 |35.68 |27.16 /33.21 |
|50% new policy B2 |1728×1084 |MetalFX /−1 |893 /25.0972 |35.58 |27.47 /32.90 |
|67% C |2304×1446 |MetalFX /−0.584962 |637 /25.0719 |25.41 |39.03 /42.81 |
|75% D |2592×1626 |MetalFX /−0.415038 |529 /25.1012 |21.07 |47.26 /50.42 |
|Restored50% A12 prefix |1728×1084 |MetalFX /0 |718 /20.0000 |35.90 |27.07 /32.95 |

The first N/A0 windows were still discovering probes (25→32 and46→53), so they are
excluded as matched controls. Only Native and the fixed50 mip comparison were repeated
after53-probe settling. C/D remain identified as PID20126 windows; the repeated controls
are PID21327, not one uninterrupted or perfectly identical scene realization. Camera,
original66 controls, CAS, output dimensions and source identity match. Selected workload
facts remain17 shadowed simple-alpha batches/13452 vertices/24552 indices,9 local lights,
41 projectors(two shadowed); probe updates and uploads vary. Equal probe count is not
proof of an identical entire scene.

The new policy's observed rate is **0.285% below A02 and0.887% below restored control**;
the two disabled controls differ0.607%. This limited ordered comparison does not establish
precise mip overhead or zero cost. It does not justify assigning the higher-resolution
choices the50% rate, or claiming zero Native regression against another executable.

**Loss limits remain explicit.** Retained endpoints all pass exact requested/effective
mode, descriptor and bias checks. Only B2 passes every local population gate; missing
lifecycle/UI rows leave the other populations incomplete. These are bounded observations,
not a lossless full-population qualification. Named frames/gates/denominators are preserved
in `quality-results-01.json`. Old/new recorders report466/471 contention drops and31/75
budget drops; their precise per-phase distribution is unknown. The new byte cap ended
3.209s before full A12's intended end. That full row remains invalid. The explicitly
parent-linked20s prefix ends1.891s before cutoff and retains its named holes. No further
runs were collected to seek a favorable result. CPU-analysis completion precedes N2 by
18.139s, independently corroborated; it did not overlap that window.

## Image and motion evidence

Five original native macOS Screenshot PNGs are3456×2234, with lossless2168-high window
and1:1 foliage/glass, fence, hair/clothing, rock/grass and UI crops. This is not the
viewer's no-post snapshot path. All five selected UI crops are byte-identical
(`7b040c8f2ab08c728003736f8f8ccf2e92ffc34f6785b1e54141efa95b0cc314`).

Bias−1 gives modest material/bark/surface clarity over the same50% control. Both50%
images still merge fine grass/leaf gaps and show wavy fence contours/bright palm edges.
67% and75% restore more separation and reduce that edge wobble;75% is the best scaled
still. Native resolves the finest separation. No new alpha holes or depth/color coverage
failure was observed in these crops; the GPU fixtures independently exercise that contract.
Avatar poses, foliage and illumination/shadows change, limiting exact pixel comparisons.
No useful world signage was available in the fixed view; that material category is unqualified.

The original168s H264 screen movie and four review clips retain the same submitted25-point
±0.65m camera path for Native/50/67/75. The path takes14–18s depending on API acknowledgement;
it is stepped, not frame-locked, and supplies qualitative review, not a temporal-stability
score or FPS measurement. The review videos contain1:1 unoccluded foliage/glass and avatar
crops, preserve variable timestamps, and are lossy H264 derivatives. No audio was recorded.
The source movie remains private. A separate bias-off/on motion pair also exists at50% with actual0→−1 policy and advancing scaler
encodes verified before cutoff. Its main-view probe pack varies53–56/53–57 during camera movement;
it is not the53-entry fixed-camera timing population. Sampled poses are not pixel matched (pose0 has
a substantial framing difference), so it supplies qualitative detail/coverage evidence, not an
isolated continuous-shimmer result. No gross new holes or coarse moiré bands were seen in the
sampled unoccluded views; absence of unacceptable continuous instability is not established.

Full physical captures retain stale macOS dialogs for older69454, not candidate69464.
Those dialogs are excluded from labelled detail crops. Later visual observations outlive
the quiet recorder cap: active method/dimensions/bias are corroborated by successful scaler-encoding logs, visibly changing
world views and fresh69-control/camera/identity gates, not fresh GPU completion rows or
a newly captured UI status field. Frame-atomic screenshot/command association is not claimed.

## Separate maintenance finding

PPD597 records an existing hidden-only manual probe fixture incorrectly registered as an ordinary
CTest suite. Default invocation matches no cases; explicit manual invocation requires external
source inputs. It is excluded, never counted as passing. A coherent manual/default-test disposition
is retained for separate maintenance; no probe optimization or unrelated harness rewrite is included.

## Decision and stopping point

Keep these optional choices and the scoped mip policy. Prefer67% for this observed balance;
choose75% for scaled image quality or50% for speed with its visible reconstruction limit.
The remaining fine-edge/detail deficit justifies a bounded Temporal evaluation next, not
an assumption of native-quality reconstruction or guaranteed further throughput. Motion
stability and broader material/scene acceptance remain owner review work, not silently
closed gates. No Temporal code or additional shader experiments were started. The quiet-session bias override is
review-only: retire it before normal-branch production integration and rebuild, retaining this exact
review executable/evidence for reproducibility.

Starting Native mode, original66 controls and CAS are restored; the review-only override is
false. The camera returns to its fixed starting position, capture utility returns to still
mode, and final viewer PID22729 is closed. Review app/DMG, focused diff, exact results, PNGs/crops
and clips are on iCloud Desktop. PPD596 is ready for owner review. The owner authorized pushing the
feature branch for AI review on 2026-11-25; usual-branch integration and release remain unapproved.
