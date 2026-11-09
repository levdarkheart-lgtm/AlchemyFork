# Shader register experiment and probe-neighbor correction

Observed on the physical M1 Max/64 GiB, macOS26.5.2, Xcode26.6. Starting
source479ff989f4d21f2c7b5d3feba60408c9fd18759c. No live viewer FPS sample
was collected for the changes described here. The controlled baseline remains
68579 at9.608673FPS; latest normal68581 measured9.454639FPS.20FPS is unmet.

## Diagnostic compiler repair

479ff989 adds `-gline-tables-only` alongside `-frecord-sources` for the existing
default-OFF source-record option. Optimization and fast-math remain unchanged.
An actual compiler-options file is now a dependency, along with shader source
and CMakeLists, because Xcode otherwise reused stale output after flag changes.
Initial target44290 exited0 but its old AIR contained no line tables; that
result did not prove the new flags took effect. Generated AIR/metallib were
preserved before invalidation. Forced target11782 and both-generator check40829
exited0. Touching only the generated options stamp rebuilt the shader, actual
67709 exit0; output hash remained identical.

The bounded AIR verifier exited0:34,812 DILocations,749 DISubprograms, optimized
LineTablesOnly compile unit,839 geometry and76 sky debug instructions. AIR SHA
6bde712bb365e653844198cff2d5ea0038da7aabb04b129bdd387904be43760f.
Normal option-OFF shader SHA remains
ffd9e1d178988b0719c516cd1e87444907d59d063630d7fd9ac2d17cafaa9347.

Canonical diagnostic app68587 identifies source479ff989. Independent
`tools/metal/verify_app.sh <app>` and deep strict codesign each exited0;
349 ARM64 slices,0 invalid. Executable SHA
4c232817819488c815ac5b0ae956508898b09d06ff52ffd046f519f6f2f2e2ef;
packaged/built metallib SHA
6d9d343d85e7d5430ac61eaed9e67a740c8c9bb36c72d28519fe3166d27baa0a.
Saved full-build log ends BUILD SUCCEEDED; original session59140 and doctor
numeric exits were unavailable after context recovery and are UNKNOWN.
Independent report `/private/tmp/alchemy-metal-profile-line-info-app-verification-20261109.json`,
SHA9fbb233e43d095852e68ea51c0de5484472c072c19722ee3c229335e9fa93ddf.
No launch or runtime-performance claim.68584 was preserved separately before
this build. Xcode cache still has source recording ON; return OFF for normal runs.

## SH lifetime experiment rejected

Private `/private/tmp/alchemy-metal-base-sh-lifetimes-20261109.y16md3` changes
only packed-SH load/decode/consume scheduling. It preserves4 packed reads,
14 half2 conversions, exact float association, coefficient mapping, clamp,
ambiance and all probe selection. Combined shader SHA
6668015460cf9d282bae11d8beb4e66b670d2e42c6198b8ea29395f9d2aa9e9c.
Compile47173, API+GPU validation53423 and quiet timing57112 each exited0.
340 actual base bitwise pairs,85 ambient/walker and87 single-SH oracle pairs
passed. Two raw nonfinite fixtures produced only positive-zero final RGB in
both variants, so they do not prove survival of nonfinite payloads.

224 timing samples covered8 cases and7 counterbalanced blocks each. Whole
command-buffer durations divided by repeated clear/store passes are diagnostic,
not isolated draw timing. All observed command buffers were8–20ms. PBR median
change−0.1866% matches identical-PSO control−0.1859%. Legacy+1.3429%, cap32+2.4960%,
remap−1.8302%, box+0.1709%, sky+0.2851%, mixed−1.0775%; every paired range crosses
zero. No general gain. Independent report
`/private/tmp/alchemy-metal-base-sh-lifetime-review-20261109.z0m_6pbo/timing-independent-summary.json`,
SHA0c29e5a84f93931a3d7f308b4a9daa64379969dee3e4cc72661999183876721c.

A separate four-draw adapter used the same unchanged candidate shader, compiled
with26.5 deployment, line tables and recorded sources. Initial compile01's
shader steps completed but its host command named an absent source and failed;
correct adapter compile03 exited0. Actual capture execution exited0, with
strict original/candidate and final captured-output equality. Trace:
`/private/tmp/alchemy-sh-registers-cap32-y0kn4xvb/sh-lifetime-registers/four-base-draws.gputrace`.

Xcode replay and Profile completed. Actual summary:1 command buffer,4 render
encoders,4 draws,0 compute/dispatches,12 vertices,1024² RGBA16Float,33.64MiB.
Medium-state diagnostic duration36.54ms, original fragment cost49.98%, candidate
50.00%. BOTH fragment variants use216 registers/high register216 and0 spilled
bytes. No register reduction. Nonempty synthetic output was displayed. Source
editor opened, but source-line cost data still was not displayed; per-line
hotspots and occupancy remain UNKNOWN. Stop GPU workload produced Finished
running macOS App; CUA completion summary sent. No replay left running.
Observed record `/private/tmp/alchemy-metal-sh-register-profile-observed-20261109.json`.

Do not integrate this candidate or spend another viewer warmup on it. The older
predecoded-SH buffer experiment was also already rejected: see the
[compiler/SH record](CURRENT-67611-original-and-release-flags-2026-11-09.md).
Do not repeat predecode, named accumulators, specialization or material split
without a genuinely different measured cause. Current live Ultra SSR=0; private
SSR-sharing miss-path savings are not a current-scene FPS improvement.

## Confirmed independent parity issue: examined neighbors versus accepted taps

Exact original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45:
`llreflectionmapmanager.cpp:updateUniforms` packs up to64 valid neighbor IDs.
`class3/deferred/reflectionProbeF.glsl:sampleProbeTaps` uses that full count,
while limiting ACCEPTED taps to32. Rejected neighbors do not consume this cap.
Its comment saying up to REF_SAMPLE_COUNT−1 neighbors is misleading; executable
producer/consumer code is authoritative. Prior negative audits relying on that
comment or fixtures with only31 neighbors missed this discrepancy.

Native legacy, base and water walkers each incorrectly clamp the examined list
to31. A containing probe or manual box at neighbor32–64 can be omitted even
when fewer than32 contributions were accepted. This changes radiance, SH and
manual-box suppression. Class2 original uses a separate environment-map path;
its native bypass remains unchanged. Water must still reject nondefault
automatic probes, preserve its permanent first-owner neighbor switch and add
default0 only while the accepted cap permits it.

Root added an actual forward-renderer regression:31/63 valid-but-noncontaining
sphere entries before a containing manual box at neighbor32/64 must match the
complete image of the same box reached directly. Accepted duplicates separately
prove the32-tap cap still excludes a late box. Red Release build84399 exited0;
actual API+GPU run56383 exited42:2 expected late-neighbor image failures,
9,159 other assertions passed. Root changed ONLY the3 clamps31→64 and one
explanatory comment. Green build54169 and actual API+GPU run10932 each exited0; all9,161 assertions
passed. The independent all-walker fixture is prepared for review/execution.
No FPS improvement is claimed.

## Advancement and open gates

Measured register data rejects an ineffective change before a live viewer
trial. The confirmed64-neighbor discrepancy advances exact-original parity,
not FPS. Scalar neighbor addressing is a separate private hypothesis requiring
strict selection/order/output and actual timing proof; not admitted. Main-base
lighting is only about17ms of the roughly105ms viewer GPU interval union, so
optimizing it alone cannot credibly promise20FPS.

Viewer and Translator remain stopped; all Midday plans restored. No quality,
resolution, mirror-face count/cadence, caster, PCSS or gamma changes. Full pixel
and feature parity, multi-scene performance, prolonged stability, strict no-GL
and independent build/review evidence remain open. The goal remains ACTIVE.

## Repair validation completed and scalar cursor rejected

Code repair committed4b23c0b34589bc9deee3cec4a86ddee80f34e67c. Full Release
run46704 and RWDI build44278/run27591 actual0; both API+GPU suites passed
427,464 assertions/98 cases. Capture-specific startup flags remained disabled;
those tests explicitly warn that capture execution requires the startup flags.
No new capture-mode execution is claimed by these full normal suites.

Private all-walker fixture
`/private/tmp/alchemy-metal-probe-neighbor64-20261109.iulo3C` compiled62467:
metal3.1/ffast-math/min26.0, metallib and optimized host each0. API+GPU run66898
actual0:12 cases compare old31 and required64 walkers against independently
expected whole-image RGBA8 bytes and CPU membership. Base actual fragment,
legacy/water direct actual-helper wrappers cover late32/64, boxes/spheres,
zero-fade first owner, accepted32 cap, high buffer base and partial tail.
Root caught an incorrect private cube override face index during source review;
agent corrected layer→layer*6+face BEFORE compilation or execution. Final host
SHA1562264fb8de7a003d7dfb98dde808d525628bd772a8aa2c496da9caa21f4ff6;
combined918216a1e520f3962f9e1f13a3f2b85f72dc13ea5a9397050ec470017f8ca705.
No shader-selection quality reduction or FPS claim.

Separate scalar-addressing prototype
`/private/tmp/alchemy-metal-probe-scalar-cursor-20261109.g47atm_4` used the
corrected64-candidate baseline. Candidate changes only integer addressing and
normalizes a bounded span once per permanent owner; FP math/ABI unchanged.
Compile55854 all3 steps0; API+GPU26980 actual0:404 strict base image pairs,
101 walker/103 single-SH oracles and16 nonvacuous neighbor contract checks.
Quiet timing16269 actual0:252 samples/9 cases/7 ABBA/BAAB blocks, all CBs8–20ms
(observed10.889–17.776). MainPBR−0.55% versus same-PSO control−2.62%, legacy
+1.30%, mixed+0.09%; only64-rejected stress case consistently improves−3.91%.
Every other paired range crosses zero. REJECTED/private; no general gain.
Independent report
`/private/tmp/alchemy-metal-probe-scalar-cursor-review-20261109.y8ti61oe/timing-independent-summary.json`,
SHAfdebd3009a7a42740778db6891a1285a73768421891ad2fe06d34f732388b582.
No heavy build/profile/GPU work overlapped the timing window.

Root preserved68587 diagnostic app, ran doctor, configured source recording OFF
and ran tools/metal/build.sh build for4b23c0b345:11888 all4 steps actual0,
per-step record `/private/tmp/alchemy-metal-neighbor64-app-build-exits-20261109.json`.
Final normal app identity/signature/ARM64 checks pending. No viewer launch or
live FPS comparison. All viewers and Translator remain stopped.

Priority now shifts to actual fragment/depth-rejection evidence from one costly
hero forward face. A source-only diagnostic capture extension is being prepared;
no GPU hierarchy/occlusion optimization or new capture run is claimed. Proposed
culling would use current-frame, exact-face depth, preserve draw order/all six
faces, and retain uncertain/deformed/equality/near-plane cases. Do not enable
shared main-camera occlusion in cube views. Measure hardware early rejection
first; if already effective, reject this avenue before adding hierarchy cost.


Final normal app verification completed actual0, independently by shadow_contract:
app26.4.0.68588/full source4b23c0b34589bc9deee3cec4a86ddee80f34e67c embedded,
349 Mach-O ARM64 slices/0 invalid, deep strict codesign valid. Source recording
OFF, packaged metallib equals normal Release artifact. Executable SHA
44d83b1a7a162cb82b06fc901b3bb35f30d6ad95db9f501d612fde8f6ebecd97;
metallib33bd2d7f0e1f240354b92ca26289136373b27152992cba9214f4448844c6bbb1;
committed MSLd6adbee09cb33c7f9451892d3d896ab9ffc763b43a722e054353deb8146a0489.
Identity report `/private/tmp/alchemy-metal-68588-app-identity-20261109.bhf9c188/app-identity.json`,
SHA67a60a4eaf7025285289ba332744da5bdea0db2519c4ee44d46dcea3fe684b4f.
No viewer launch. All validation/build/replay/verification processes terminal;
GPU idle. Agents continue source-only preparation/review for the next capture.


## Next diagnostic implemented: selected hero-forward face

Root reviewed/applied the privately prepared six-file extension and parameterized
capture tests; independent draw_cpu_costs review approved caller/lifecycle
ownership. Final private patch SHA
a8d0e03d5c7b8afc3fbc120d231130567931ac2ee70489d42f128b2d0f188f98 in
`/private/tmp/alchemy-metal-hero-forward-capture-20261109.gztze8bu`.
Mode enum separates display/main-base/hero-forward. Face1 reservation carries
frame/owner/epoch and is rechecked against actual hero face/view/target state;
main/ordinary/snapshot/HUD consumers cannot start it. Only the reserved face
omits outer batching. Begin immediately before the owned forward call; finish
after its commit and before target publication, with RAII cancellation on error.
Default rendering, all six faces, shader math and cadence remain unchanged.

Release build12288 and actual capture test82486 exited0:3,059 assertions/3 cases
for both selected modes, with API validation and shader validation UNSET. RWDI
build55464 exited0. Remaining serial capture/normal GPU validation27224 is in
progress. Runtime newview routing, actual hero command counts, input replay,
profiling and hidden-work savings are NOT yet verified. No new viewer launch.


Hero diagnostic extension committed0d5d49c188f8bfee4dd866adbac757af750340d4.
Serial validation27224 actual0: RWDI actual capture3,059/3 passes; BOTH normal
capture-OFF API+GPU suites427,476 assertions/95 cases pass. Source routing is
independently reviewed; actual newview face routing still requires a live run.
Build20880 all steps0: preserve normal68588, doctor, configure source recordsON,
full app build. New diagnostic app68590/full source0d5d49; fresh pinned QA dry-run0.
Static verification underway; no launch, Midday plan or capture execution yet.


Diagnostic68590 independent verification45710 actual0:349 ARM64/0 invalid,
deep strict signature valid; exe matches QA pin44bd81abdbc6dedbd9a91e1a7fb950b070b4e4a65971f542a1a208dd10d5336f;
metallib e5ed5a90e76868e2ae141d2b63116cf806b544b349a07e196e52ba916e69e90a,
optimized source/line flags ON, deployment26.0. Identity packet
`/private/tmp/alchemy-metal-68590-diagnostic-identity-20261109.eb1x79xe/app-identity.json`,
SHA09e51c29978684a80ce6b503e94e977b4afc18bc20d8df2a7771b04cd91aac9d.

Installed fresh0d5d49 reversible Midday plan and QA launch each exited0.
LIVE PID80172, exact68590/source0d5d49. CUA CtrlSuperF fullscreen before saved-login
Return; LEAP reports logged-in. World AX recheck timed out/reset18sec; root did
not repeat the heavy query. CUA phase completion summary sent. Fresh start
observe/verify/gate each0 confirms43 controls/location/exact executable.
Runtime viewport3456×2168/Ultra6/Preset123, camera-change0. Probes still warming;
no samples/capture yet. New Midday plan remains ACTIVE until verified shutdown.
All older plans restored, Translator stopped, no competing build/GPU/profile.
This run is diagnostic, not a formal FPS comparison or scene-image parity proof.
