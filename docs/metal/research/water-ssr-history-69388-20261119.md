# Water band and SSR history coordinates — 69388

PPD-013 is reopened for the owner's camera-relative dark/grainy water band.
The previously accepted homogeneous haze interpolation fix remains present;
no regression in that correction has been proved. PPD-015 remains a separate
eye/agent steepness mismatch. PPD-497 records the newly confirmed SSR history
coordinate inconsistency before its repair. The checkpoint merge/push is held.

## Observed runs and capture identity

The first functional run used the preserved Texture Fixes69388 app, source
97cb7ff705a17fb2dde73da737230f3cf97d91c4, indra tree
65fb19d0eafb8471e60c89dfaf634aebf93603b5, executable SHA256
a3051a443a6afd892e5b73eb5c7c645aa4990d8b7f6de456fd7d6fbbabbbebd8.
Its414warnings/44sites and normal shutdown are covered in the separate
[closed functional log audit](warning-runtime-69388-source-audit-20261119.md).
The owner initially reports no functional issues, then supplies three water
images and confirms the band moves with the camera and affects only water.

The subsequent capture-enabled restart uses the exact same executable.
Owner confirms the artifact and Midday setting; effective EEP controls were
not independently exported. Viewer PID34653 exits normally with code0 at
2026-11-19T14:08:18.616504Z. Its distinct final log is3,483,340bytes,
120warnings/26sites, SHA256
bd32162f0f1bb86a05978c2084d6091b566c4eb75a223cbff7521d9acf8dcd6b.
Do not substitute these counts for the first run's414/44. Cache/account files
are retained; no change to the OpenHTJ2K-only decoder policy.

Private run Alchemy-water-69388-q7dnx5l4 preserves:

- Early device-all-queues display capture, frames258→259, stopped/error0,
  5582files/4,553,353,981bytes. CPU capture-stop interval3.549656084s.
- Late device-all-queues display capture, frames2057→2058, stopped/error0,
  7800files/6,027,529,924bytes. CPU capture-stop interval4.457072875s.
- Final diagnostics stopped by request below the256MiB cap; complete185743,
  pending0, I/O0, budget drops0. Capture pauses are not normal pacing evidence.

Original traces remain immutable. A separate APFS clone of the late trace
replays successfully in Xcode with profiling unchecked. File names and sizes
match the original; full trace content hashes were not calculated. Its final
captured image contains the band. The two captures are not yet proved to have
different camera matrices and are not an asserted two-angle comparison.

## Captured water pass and bounded localization

Late main water encoder contains wave draw6493 followed by four flat groups:
6496cells,6498rings,6500zippers and6502fringe. Wave and flat PSOs share
almetal_world_above_water_fragment. Color0 is3456×2168RGBA16Float; depth is
Depth32Float_Stencil8. Actual reverse-Z depth state is GreaterEqual/writeYes,
with stencil bit0 test/bit1 replacement. The state's underwater label does
not establish that the above-water fragment is wrong.

After-wave Color0 exports as KTX1,59,940,960bytes, SHA256
a103af8da49245d960b369904a883ebc0582bbc97ada010083276beed8f48b26.
Its59,940,864byteRGBA16Float payload contains zero NaN/Inf components. The band
is already present after the wave draw. The oriented human preview is an
explicit derivative; its Y flip is not itself a renderer defect. Additional
before-water/after-flat exports remain unavailable: Xcode's save controls
stay disabled despite valid local destinations. Pixel debugging reports
"No source found" for the optimized captured shader; import did not complete.
No successful additional exports or shader debug session are claimed.

Direct typed-buffer inspection supplies rounded display values, not raw
exported constants. Actual SSR config=(1,1,0,0), trace=(25,.1,.015,.001),
sampling=(4,1.6,0,0); water roughness≈.350 and optics.z≈.080. SSR is enabled
and the glossiness gate passes. Projection/inverse, modelview, field layout,
water plane and environment values are retained privately with their limits.

CPU characterization of after-wave pixels locates a strong fine-scale change
near upright y1087–1093, x1100–3100. Around that boundary alpha is exactly0.
With actual rounded sunlit/roughness, the water glint/shadow RGB contribution
is conservatively below4.1e−6/channel under ordinary half conversion, whereas
the sampled mean luma difference is≈.0371. This bounds that specific glint
contribution; it does not exclude upstream shadows or identify SSR as cause.

An explicit conservative rounding/displacement envelope places that ROI at
eye-depth14.5–16.2m, inside field cell(3,3), more than412m from the exterior
wave rectangle, and below24.1m horizontal radius versus fade start≥299m.
It is not an external wave/flat, reach-fade or ring boundary under those
assumptions. Internal tessellation and exact raw constants remain unverified.
The SSR128m fade is smooth and not at this boundary. No geometry offset,
quality disable or different water height is introduced.

## Original contract and PPD-497 source proof

Reference1eb44bbd1bdc76d0100c75699bad1fb337e8cb45:

- LLPipeline::copyScreenSpaceReflections copies sharp screen color with equal
  source/destination rectangle orientation and copies scene depth likewise.
- class3/deferred/screenSpaceReflUtil.glsl projects clip.xy/w to normalized
  coordinates directly. Depth reconstruction uses those same coordinates.

Native source at the tested69388 engine:

- almetal_world_water_surface_output negates GL clip Y before rasterization.
  A positive Metal viewport therefore writes row fraction(1+GLndcY)/2.
- almetal_present_gl_rows_fragment explicitly documents retained GL lower-row
  world/UI storage and performs the sole display-origin conversion.
- almetal_ssr_project returns(clip.xy/w)*.5+.5; source_uv retains that Y for
  both history depth and color sampling.
- Pipeline passes the sharp renderOpaquePostDeferred target directly to
  updateScreenSpaceReflectionHistory. Separate display/DoF/presentation
  products do not replace or flip this source.
- The history fragment nevertheless reads source(height−1−targetY), reversing
  both color and depth. A landmark at GLuvY=.75 is stored at row.75, published
  at row.25 and then sampled at row.75 by SSR. This is a producer/consumer
  contradiction independent of an exported screenshot's orientation.

Two independent source reviews find the same mismatch. All production history
callers and consumers must be reviewed before integration. Existing2×2 test
uploads arbitrary rows and asserts their reversal; it misses the actual
rendered-geometry contract. Existing SSR consumer fixtures manually populate
lower-row history and do not exercise the publisher. Neither establishes the
correctness of this combined chain.

## Corrective work and acceptance boundary

The proposed minimal correction preserves sharp world color/depth rows at
publication and updates the stale explanatory comment. SSR projection,
tracing, quality, sample count, history ownership/atomic publication, depth
convention and presentation remain unchanged. PPD-015 is not combined.

Before acceptance, an actual rendered asymmetric geometry→history regression
must fail against the frozen old library and pass with the correction, using
one unchanged test executable and expectations. Cover differing landmark
depth/color and forward/reverse-Z plus SDR/HDR. Exercise the above-water
consumer where practical. Retain existing haze, water, base and alpha checks.
Then build and verify a separate candidate and ask the owner to repeat the
waterfront camera turns. A physical source regression does not by itself
close the reported water band; no owner retest or push approval is claimed.

## Executed physical producer regression

M1Max64GiB/macOS26.5.2/Xcode26.6. Doctor exits0; incremental shader and
almetal_tests builds exit0. Initial test executable SHA256
c72aa5cd60d176652a3c72f36fb9b50417db90ac5c583d9521fd8df04a2beb0c
runs the identical [ssr-history] selector and seed with Metal API and GPU
validation explicitly observed in both logs. It renders actual asymmetric
opaque fullbright red/blue meshes and verifies source landmark color/depth
before the history call. Four combinations cover SDR/HDR and forward/reverse-Z.

Frozen69388 metallib6b77dbe5b37ae17be60bede083de51a3fc7505b8f272f2559760bb16ae42bf98
fails17of158assertions across both tests, native exit42. Each geometry variant
has896of2048color and depth pixels in incorrect rows. Corrected library
bf2a3bb4d977bd1768c3b62f6cfdd207c3dae03e47b973d2b4a167d72a91210c
passes158assertions/2cases, native exit0; restored rerun also exits0. Original
library bytes and production source remain privately preserved. No test
expectations change between these negative/positive runs.

The private runner then exits1 in postprocessing because Catch's byte-array
failure output contains invalidUTF8. Both native results and exact corrected
library restoration had already been recorded. A CPU-only tolerant decode
verifies the original raw logs/validation markers/native42→0; it does not
repeat or replace the GPU runs. producer-verified.json preserves this limit.

Broader world/water follow-up initially fails1of36424assertions: an existing
far-depth landmark at row31 still assumes reversed history. The rendered source
puts that far landmark at row0. Follow-up explicitly reads/requires source
row0depth1 before comparing history at the same coordinate; production shader
is unchanged. Initial failing log is retained. Rebuild/retest is pending.

Independent source reviews of the minimal publisher, all production callers/
consumers and actual-geometry fixture find no actionable issues. They are
source reviews, not additional executed tests. No actual above-water SSR
consumer-chain fixture or owner water-band retest is yet claimed.

Final fixture rebuild exits0. Broader actual world/water case now passes36426assertions/1case; four registered CTests (SSR isolation, water normal filtering, original haze projection and base-light fastpath) all pass. Final executableSHA256d4376a92aa9f937753e9c54111602a0a392baf30e7b2e1a370add8135be5c9fe again fails17/158 with frozen old library/native42 and passes158/2 with corrected library/native0, same seed/expectations; exact corrected library restored. API/GPU validation is observed in every final native log. final-native-validation.json retains exact argv/exits/hashes. This proves the coordinate repair and bounded compatibility, not the live water-band result. Separate full viewer build/verification and owner retest follow.

## Qualified separate candidate69390

Committed source dc781c61db97ec8bff51c0f1605fa57986ba0ac5, indra tree
326ad879fe933432fc6e03f0e1e1437345098d3e. Required wrapper Release build
exits0, then copied candidate passes verify_app (346ARM64/zero invalid), deep
strict signature and strict source3645files/0blocking/0errors/925informational.
Three owned main/CEF/VLC executables have zero forbidden direct graphics imports;
foreign dependency and runtime context gates remain separate. Compiled,
test-bundle and candidate metallibs all equal correctedbf2a3bb4d977… above.
Final GPU test executable still equals the red/green-tested d4376a92aa9f….

Separate app is Alchemy Metal Water SSR Fix69390.app, version26.4.0.69390,
executable SHA256
42fcf45f88e8b8ad9835d89fe7cf1f632e300414c087a06ed4914348e2460cb3.
Embedded source SHA is verified. Stable69388 executable still equalsa3051a44…;
69384/69386/69388 apps are preserved. qualified-candidate.json retains command
statuses, exact source/binary/library identities, test receipts and limits.

Next owner run is prepared in private Alchemy-waterfix-69390-memw12uh with
manual login, the previously observed waterfront destination, retained current
graphics/Looks/cache and bounded warning metadata. No capture, validation,
sampler, Instruments or performance collection is enabled. Actual region,
settings and disappearance of the band remain unverified until owner login.
No merge/push or PPD-013/015 owner acceptance is claimed.

Ordinary harness first rejects the missing next-ID counter advance when497 is
registered; counter is corrected to498 and the subsequent33-file checks pass.
The first producer-runner Unicode failure and broader far-row fixture failure
remain in the preceding history. No assertions or policy are silently waived.

## Owner publication authorization before scene retest

On2026-11-19 the owner states: “I’m not home so I will test it in a while,
mean while commit this and push it.” This explicitly authorizes the current
checkpoint before the pending69390 water test and supersedes the prior push
hold for that existing scope. It does not supply a visual result or authorize
future fixes, default-branch changes, release publication or capture uploads.
PPD-013 remains investigating/owner result failed for69388; PPD-497 remains
ready_for_owner/pending for69390. OpenHTJ2K and all retained unknowns remain.
The normal publication target is the established codex/apple-silicon-metal
branch. Preserve the separately published69277 checkpoint history and require
final indra tree326ad879fe933432fc6e03f0e1e1437345098d3e, exactly candidate69390.
