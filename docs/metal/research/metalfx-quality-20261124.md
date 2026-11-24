# Spatial scales and material detail — PPD596

The optional Spatial prototype now offers 50%, exact two-thirds (labelled 67%) and
75%. Native/Standard remains the default. This is a reconstruction quality/performance
trade-off; neither exact native quality nor a new live throughput result is claimed.

## Build and source

- Worktree: `worktrees/metalfx-spatial`, branch `codex/metalfx-spatial-20261124`.
- Starting checkpoint: `091243d236`, retaining code `12fb05f` and build 69462 repairs.
- Application checkpoint: `a5142ae5c2c6dd1fe8b2fa89c9479eb213cff0f5`, build **69464**.
- App: `build-metalfx-spatial/newview/Release/Alchemy MetalFX Review.app`.
- Executable SHA256: `6c498099bb4acde29e0cc9033dc2faa81c978df4fe3f9b35a37e30190518b05d`.
- Library SHA256: `087705e4082de00098f9e52fd2a95b5d10688151fde0cceae71d84e35b3ef607`.
- Later test-fixture/documentation changes do not change this application's identity.

Private evidence is under `artifacts/metal/metalfx-quality-20261124/`; it includes
source diff, compiler inputs, shader binding census, capability check, build/test
logs, launch identity and package verification. Nothing has been pushed.

## Dimension policy

`ALRenderResourceInputs` maps stable saved choices to reconstruction method and an
exact rational scale. IDs 0/1/2 retain Native, linear 50% and Spatial 50%; IDs 3/4
add Spatial two-thirds/three-quarters. One overflow-safe calculation rounds each
scene dimension upward. For output 3456×2168, expected scenes are 1728×1084,
2304×1446 and 2592×1626, respectively. Actual bound descriptors must qualify the run.

All explicit scaled choices retain output-based shadow references and native UI.
Independent probe/hero sizing and the existing bounded scaler cache/command ownership
remain. Ordinary same-size snapshots still request independent native scene detail;
custom-size/cutout contracts retain their previous path. Live-window captures, not
that snapshot route, are required for Spatial quality comparison.

## Material sampling and fallback

Before main material encoding, the view freezes additional bias as
`log2(actual_scene_width / actual_output_width)`: nominally −1, −0.585 and −0.415.
This follows Apple's [Spatial guidance](https://developer.apple.com/videos/play/wwdc2022/10103/),
without Temporal's additional minus one.

A dedicated fragment uniform at slot 10 leaves immutable cached material packets
unchanged. Relevant legacy/PBR material samples use shader bias; terrain detail
samples scale their existing gradients once by `exp2(bias)`. Main alpha color and
depth draws receive the same policy and retain their original thresholds. Explicit
probe/environment LODs, water, control maps, UI and shadow generation are unchanged.
Native, saved linear 50%, snapshots, impostors and independent probe views use zero.

Late AA/scaler preparation fallback retains the bias already used for materials and
reports linear reconstruction, reason, actual dimensions and bias. It does not
retroactively change texture sampling. The UI shows the effective path/dimensions;
the tooltip includes the material adjustment. MetalFX retains its existing sharpening
policy; stored CAS applies to Native/linear paths. Combined lighting remains off.

The newer sampler-descriptor bias is listed for Apple10 in Apple's
[feature table](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf).
On this M1 Max/Apple7, the validated colored-mip check observes no descriptor-bias
effect, while shader bias selects the finer mip. Texture demand still uses output
pixel height; there is no existing scale bias to apply twice. Native mip zero starts
at the resident J2K discard, so finer unavailable source data cannot be recovered.
No texture-streaming policy was changed.

A review-only, nonpersistent control exists only when quiet diagnostics are enabled
to disable this new bias for the requested A0/A1 arms. It is not a graphics preference.

## Focused correctness

Release build, app verification, deep/strict signing and 346 ARM64 inspections pass.
Three CPU suites cover resource dimensions/odd sizes/mode transitions, actual graphics
UI bodies, and real temporary preset XML save/change/load for all saved modes. Older
presets without the key still select Standard; existing values retain their meanings.

Fourteen relevant Metal suites pass with API and GPU Validation. Three new cases cover
legacy/PBR colored mips and material maps, alpha color/depth coverage, terrain gradients,
owned-input parity and zero-bias recovery. Existing scaler reuse, presentation failure,
cutout, AA, resource-format and reload coverage remains. The new alpha fixture initially
omitted a required environment source; only the fixture was corrected, preserving
production validation and assertions. The old hidden probe-accumulator suite requires
external source inputs and is excluded; no probe performance experiment is inferred.

## Remaining qualification

The M1 Max viewer has not been logged in for this task. Account availability on the
owner's other machine is pending. Therefore no six-arm throughput table, new PNG/motion
quality conclusion, preferred Spatial mode or recommendation to implement Temporal is
established yet. The review app is available; the remaining work is one requested
N/A0/B/C/D/A1 Home comparison and lossless image/motion inspection, followed by restoring
starting controls. This is an explicit incomplete qualification boundary.
