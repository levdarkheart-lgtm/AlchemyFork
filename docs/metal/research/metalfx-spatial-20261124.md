# Optional MetalFX spatial mode — 2026-11-24

Implemented locally from usual-branch7911e382ce; PPD591 owns qualification.
The initial measured prototype is68e59fc7b6a538f092d03f1869e06395ecae16c7, version69457.
Matched source control isf384e2c220c68518bf74da7fb8e6fabe9d6a86d3.
No source push or production-default change is authorized. Standard remains0.
The focused integration follow-up below is12fb05f43e7a8d7ad41913d3ce1a65110cb53221,
version69462. Its functional verification is separate from the69457 performance cohort.

Initial69457 executable SHA256:
`e3b544d3dafa334e000ec5fcd300d02fd0871f1c6bebdc0652c48528cbb31562`.
Matched69456 control executable SHA256:
`7d9af496904e77854261173d1114259d436d8d7968b69b91aa7dc11a68973bdd`.
Both bundled libraries are
`6fe71e2833331703acc4912f83f21c9e6ff0a14b7ef5fad70b1f75dccd688793`.
These are new builds; earlier private Review69454 measurements supported the
choice of image/shadow policy but do not substitute for this cohort.

## Initial69457 behavior

The saved graphics selector has Standard0, 50% Linear1 and 50% MetalFX Spatial2.
Standard preserves legacy divisor/multiplier/shadow coupling. Fixed modes use
ceil(output/2) scene dimensions, output-sized shadow reference and native UI.
Existing shadow scale/cap/filter/detail values are preserved, not compensated.
Probe/hero allocations remain independent; cube lighting still shares main spots.

Classic `MTLFXSpatialScaler` consumes post-tonemap perceptual color. FXAA/SMAA,
legacy glow and RLVa keep their ordering; MetalFX replaces CAS sharpening only
when preflight succeeds, preserving its saved value. AA-off or unavailable-device/
format creation uses Half linear with normal CAS. Debug/special main views also
use Half linear; explicit snapshots retain the existing independent size override.
A same-size snapshot exercised Half linear fallback. Native UI stays full size.

The internal post target remains the scopes/reference source. A separate upscaled
texture feeds final lens/print/dither/reference composition before UI. Ordinary FX
presentation explicitly writes opaque alpha; cutout keeps its existing path.
Cached descriptors, exact queue/resource ownership and completion leases preserve
pending images. Failed encoding poisons the existing batch rather than replaying it.
The original1eb44 finalization order is the reference; MetalFX itself is new.
Apple guidance: [spatial scaling](https://developer.apple.com/videos/play/wwdc2022/10103/),
[perceptual mode](https://developer.apple.com/documentation/metalfx/mtlfxspatialscalercolorprocessingmode/perceptual).

## Initial69457 build and correctness evidence

Both clean Release builds and app checks exit0;346bundled Mach-O components are
ARM64. Selected actual core recipes match after path normalization: `-O3`, C++23,
ARM64, deployment26.0, SDK26.5, AppleClang21/Xcode26.6; shader source metadataOFF.
Metal3.1/fast-math/invariance options and library bytes match. Candidate adds the
MetalFX framework/helper. Both link the same pinned clean OpenHTJ2K0.27 archive;
actual runtime decoder identity was required by the runtime qualifier.

From the candidate worktree, the build/verification commands are:

```sh
cmake --build build-metalfx-spatial --config Release
tools/metal/verify_app.sh 'build-metalfx-spatial/newview/Release/Alchemy MetalFX Review.app'
ctest --test-dir build-metalfx-spatial -C Release --output-on-failure -R 'alrender_tests/alrenderresourceplan_test$|newview/native_graphics_preferences$'
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 ctest --test-dir build-metalfx-spatial -C Release --output-on-failure -R '^almetal_tests/almetal_(spatial_scaler|spatial_presentation|retained_background|postchain_parity|postchain_legacy_admission|finalize_admission|reference_format|cutout_coverage|smaa_stencil|cutout_alpha_resample|shader_reload)_test$'
```

CPU scopes pass2/2. GPU scopes pass11/11 with API+Shader Validation, covering
MetalFX caching/formats/pending-command lifetime, perceptual transfer/opaque alpha,
references, scopes, CAS, cutout, SMAA, retained UI/depth, cancellation and reload.
Six additional exact display-post/CAS/FXAA/SMAA/final-print/RLVa cases pass11979
assertions. Their exact Catch filters are retained in `gpu-core-tests-01.log`.

Private campaign root: `artifacts/metal/metalfx-spatial-20261124/`.
Build/app results: `control-build-01.log`, `candidate-build-01.log`,
`control-verify-01.log`, `candidate-verify-01.log`, `build-config-comparison.json`.
Tests: `cpu-tests-01.log`, `gpu-tests-01.log`, `gpu-tests-01.xml`,
`gpu-core-tests-01.log`. Launch identities and source-pin receipts are separate.
These fixtures do not establish whole-scene image-quality parity or release status.

## Corrected69457 closed-run observations

`analysis03-corrected01/{summary.json,qualification-receipt.json,REPORT.txt}` owns
one ordered Native/Linear/FX sequence on the candidate, with10s settling and25s
windows. Observed main probes55, scene3456×2168 or1728×1084, output3456×2168.
Rates count successful joined main-post GPU completions, not physical display FPS.

| Mode | Positive GPU ends / seconds | Observed worlds/s | Interval N | p50 / p95 ms |
| --- | ---: | ---: | ---: | ---: |
| Standard |379 /25.041244917|15.135030|378|65.857834 /70.077644|
| Half Linear |892 /25.077357958|35.569935|891|26.652917 /38.166167|
| MetalFX |877 /25.038544208|35.025998|876|27.443979 /36.217906|

Native/Linear pass all five per-measure gates. FX retains one missing
`command_create_end` at source frame10081: full output/UI/scaler-population gates
remain false; scene dimensions and phase coverage pass. Positive matches remain.
FX primary intervals include10080→10082; dense adjacent-source interval N is875.
Global392contention drops and18pending commands at duration cutoff remain explicit.
All three windows are enclosed by native coverage; A1 has no timed rate.

Observed rates are approximately2.35×/2.31× Native; FX is approximately1.5% below
Linear here. This is one quality-changing ordered sequence, without timed restored
Native or an active timed source-control cohort. It is not a repeated-pair causal
production gain or demonstrated MetalFX advantage over Linear. No per-pass cost,
physical traffic, GPU occupancy or scanout result is inferred.

All retained main inputs have55probes and SSR1/25/4; local lights9 and projectors41
(two shadowed). Alpha census snapshots retain17batches/13452vertices/24552indices,
a subset rather than total draws or fragments. Probe BASE stays256², but A0 face
packs vary123–128 while later arms show128. Upload/probe work differs; no identical
workload or causal normalization is claimed. Joined raw facts remain in A0/L/FX JSON.

The first parser omitted an existing BASE schema entry. Registered correction and
two actual reader regressions plus18join tests pass; original `analysis03` remains.
Raw hash, positive counts/rates/quantiles and the genuine FX exclusion are unchanged.

## Initial visual evidence and remaining acceptance

Verified clean04 source JPEGs and `quality/*comparison.png` retain Native/Linear/FX
left-to-right crops. Root inspection finds FX sharper than Linear, with less fine
foliage/texture detail than Native; UI remains sharp. Animated poses differ, so
these are qualitative observations, not pixel parity. Mode switching preserves CAS.
Fullscreen resizing exercised1728×1084→1728×958 scene and3456×2168→3456×1916 output;
root verified reuse of the two scaler sizes on return. `resize-qa.json` retains the
observed creation count1→2 and unchanged CAS value.

`quality/screen-motion-02.mov` retains one real screen recording, with the same
25-position small lateral camera sweep under each mode. Fresh installed visual
gates precede Native/Linear/FX arms. UTC action receipts map the sweeps to the
recording; API submission does not promise frame-locked camera updates. Exported
`motion-foliage-fence.mp4` and `motion-hair-texture.mp4` compare Native/Linear/FX
left to right without spatial resizing. Stale macOS notices for the older69454
app obscure the raw recording's center; exported crops exclude that region rather
than retouch it. They are not a candidate crash. Clean window-only stills omit them.

Sampled motion frames confirm camera movement and retained geometry. Compression,
animated poses and unequal action timing prohibit pixel parity; real-time shimmer/
edge-crawl acceptability remains for owner review. This bounded Home sweep is not
coverage of every transparent material, fast motion or scene. The review artifacts
preserve these limits rather than claiming complete temporal quality.

Final qualifier verifies all original66 controls, fixed camera and Standard0;
CAS remains0.4000000059604645. Normal viewer shutdown completes16:00:12 UTC. The
optional selector's owner usability/layout review, broader special-view coverage,
stability and still/moving-quality acceptance remain open. The implementation is
suitable to keep developing as an optional performance/quality choice. Keep
Standard default and all prior negative shader/queue experiments closed. No push
or production-default selection is implied.

## Initial69457 private package for owner testing

`Alchemy-MetalFX-Review-69457-AppleSilicon.dmg` is copied to the owner's iCloud
Desktop (374018189bytes; SHA256
`d6cda92f4fe01e338a68a15210aca38fb975cc781beac15f6187d123045e6c6a`).
It includes the exact tested app, an Applications link and mode-selection guidance.
Mounted file bytes/modes/symlinks match all7841 source entries; deep/strict signing,
346 ARM64 checks and disk-image checksums pass. Main Mach-O minimum is macOS26.0.
The existing ad-hoc signature is unchanged; this is not notarized or a release.
`package-69457-receipt.json` records verified iCloud-copy hashes and Foundation's
uploaded=true/uploading=false/no-error metadata. Download and external-machine
execution remain unverified. Native default and source remain unchanged.

PPD592 separately records two inherited VLC plugins referencing unbundled Growl/
Sparkle frameworks. Both have identical hashes in the native control and candidate;
actual selection and media impact are unknown. No speculative package removal or
MetalFX attribution is made. This remains a production media-packaging follow-up.

## Focused integration follow-up — PPD593–595

Graphics presets now save/watch RenderMetalMainResolutionMode. The actual manager
save/load methods, real LLControlGroup and XML I/O verify save2→select0→load2.
Older graphics presets lacking the key select Standard0; explicit values through
Include keep the normal loader's precedence. A failed load restores the prior mode.
Generated Default serializes the declared Native default without changing the
current optional mode or falsely marking Default active. Looks/camera behavior is
unchanged. Original1eb44 whitelist/save and merge/load boundaries are retained.

Advanced Graphics now shows the last committed ordinary main-view effective mode
beside the selector. AA-off and unavailable scaler preparation report Linear with
the relevant reason. Input application/startup invalidates stale status; snapshots,
probes, HUD and debug/special views cannot overwrite it. The UI reads cached status
and never prepares a scaler. A failed encode or enclosing commit cannot publish a
false successful fallback. Actual refresh/draw bodies pass the ASan/UBSan UI fixture.

Same-size ordinary snapshots explicitly request independent native-size scene
capture in either50% mode. They follow normal native AA/CAS/final effects, rather
than attempting pixel identity with live MetalFX. Native/Standard keeps legacy
divisor/multiplier semantics. Custom sizes, explicit overrides, cutouts,360 faces
and a larger request's tiled allocation-failure fallback retain their existing
paths. Request intent lives only in the existing snapshot transaction and is cleared
on cancellation; no global setting, admission limit or scheduler changes. The input
policy tests cover classification/dimensions and unchanged live plans/shadow references.

One new spatial transaction case first encodes real MetalFX inside a borrowed main
batch, then rejects invalid final lens values or an incompatible prepared target.
Both branches reject outer commit, grant no settlement/publication permission,
release the reservation, retain the previous BGRA image and permit a valid retry.
Actual drawable/UI publication remains the existing caller gate, not a new test hook.

Final69462/source12fb05f Release build passes. All14selected CTest suites pass:
3CPU(settings/presets/dimensions) and11Metal suites with API+GPU Validation,
including the new failure case and retained/cutout/SMAA/reference/reload coverage.
Deep/strict signature and346 ARM64 components pass. CPU SHA256 is
`85d4b87227a39fb61e42cb439c92d908c71228d406e1ae76ccc03cbdc3208829`;
the baseline metallib remains byte-identical. Exact receipts/logs are under
`artifacts/metal/metalfx-integration-20261124/` (final-tests-01.xml/log,
viewer-build-03.log, verify-app-03.log, codesign-03.log, candidate-69462-identity.json).

No viewer login, live snapshot/UI smoke check or new timing comparison was run in
this follow-up. The owner is testing on another machine; these results are source,
real settings-I/O and native component verification. The ordered69457 comparison
does not establish precise MetalFX overhead or zero Native regression for69462.
Native default and combined lighting OFF are preserved; no push is authorized.
The separate69462 DMG on iCloud Desktop preserves every application byte/mode/
symlink and passes mounted signature/image checks. The69457 package is unchanged.
The focused integration review bundle carries the final diff, selected source,
test receipts and these verification limits; neither package is a release claim.
