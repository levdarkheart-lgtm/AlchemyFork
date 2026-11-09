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


Live diagnostic completed:300 samples, all256 probes ready/stable120, zero camera
changes. Fresh end observe/verify/gate each0. Environment and camera basis match
68579; camera Zdelta−30.5 micrometres. One selected hero-forward capture stopped
7506→7506,1 attempt/error0. Metadata SHA
e72fc019ee62ac6244e321ac4aca0326ed12c8653d59f039e554bb107dafd163.
File `artifacts/metal/private-0d5d49-hero-forward-capture-20261109/frame-capture/display-interval.gputrace`
exists,21,909 top-level entries (resources, NOT command count). Actual replay,
profiling, command census and early-depth work remain UNKNOWN until Xcode.
Exact PID80172 verified, graceful SIGTERM, Goodbye/absence19750 actual0; fresh
0d5d49 Midday plan restored actual0. All viewers closed/plans restored, Translator
stopped, GPU idle. No performance improvement or scene parity claim.


## Actual hero-forward profile and attended source inspection

The exact NEW0d5d49 face1 trace (full path in NEXT_SESSION) replayed a nonempty
raw cube scene. Xcode census:1 command buffer,4 render/4 blit encoders,833 draws,
154577 vertices,0 compute,1024²RGBA16F,9.22GiB resource total. Profile completed
15.74ms at MEDIUM PState: one diagnostic forward face, NOT live frame/FPS.

Shader execution cost/register/spilled bytes: simple-alpha shadow58.02%/245/304,
above-water20.39%/254/96, PBR alpha15.22%/254/80, legacy alpha1.21%/254/32;
haze fragment1.79%/64/0. Atmospheric-haze ENCODER3063 has589 draws and77.867%
cost because later alpha shares that encoder. Do not equate haze label with
haze shader or sum overlapping encoder/shader/function scopes.

Encoder fragment occupancy4.19% is actually observed, unlike older main-base
unknown occupancy.1,660,704 fragment invocations, average overdraw1.58, PreZ
rejects55.29%,1.4197248B ALU instructions:21.33%float/41.73%integerconditional/
8.06%integercomplex/28.88%inefficiency. ALU limiter9.72%, cull28.46%; measured
fragment-device bandwidth.753GiB/s is not saturation evidence. Static spill
bytes are compiler metadata, not per-fragment external-memory traffic. No late
Z rejection counter was displayed in the inspected PostFragment category.

User drove Xcode while root observed read-only. Source Files dropdown→Call Stack
revealed simple-shadow9.14ms, simple_lighting91.94%, sample_base_probes71.05%,
holds28.93%, base_probe_tap15.61%, SHirradiance8.29%, weight4.86%. Holds source
5518 marker28.93%, probe type branch5525=11.50%, boxmatrix5527=3.31%, bounds5528=.15%.
Source sphere-check line costs were not collected. Optimized inline/source
attribution nests directional_shadow under holds even though source holds calls
NO shadow helper. Therefore these are displayed/inclusive attributions, not a
literal callgraph or proof of instruction-level timing. Declaration97.32% marker
must not be described as a single instruction consuming97%.

User stopped replay; root actual full AX saw Finished running macOS App with
Replay landing. CUA completion Recorded hero shader profile breakdown occurred
BEFORE any subsequent compilation/GPU test. No viewer/profile/other GPU test
competed. Observation packet is ignored private-0d5d49-xcode-profile-observed-20261109.json.

## Distinct private probe-only shadow specialization — strict output verified

Frozen packet /private/tmp/alchemy-metal-probe-shadow-specialization-20261109.3dmkq674,
combined026b58407cd1cf0964a624775b4f6fae456bc723991c3ee6d1bc945e984b741e,
host50a15da7b8ed8cb74ae46665568bcfc8f5ee86ed03321860c138f704e469ad29;
source manifest96d800685bf91dd9b0d8d33c544f2786c21a10e99850ce401c7490ecf78b844a.
Root verified ALL frozen file hashes and byte-identical current d6ad production
prefix before execution. Independent shader reviews draw/shadow and host trace
approved the narrow contract. Exact original1eb class1/deferred/shadowUtil.glsl503
already branches cube_snapshot to its single tap; native generic forward PSOs
retain runtime branch/unreachable Gaussian/PCSS code. Candidate forces ONLY the
existing bit0x4 taken path; same quality/Z/cascade/fade/bias/sampler behavior.
Main unflagged PSO remains original. Source alone does not prove unused code
causes245 registers/304 spill bytes.

First compiler attempt exited1 because sandbox user clang module cache was
unwritable. Retry uses -fmodules-cache-path=PRIVATE/module-cache. Actual33790
metal/metallib/clang++ each exited0. Shader flags stdmetal3.1/ffast-math/
min26.5/gline-tables-only/frecord-sources (fixture SDK target, app remains26.0).
Initial sandbox strict run exited1: Actual Metal device unavailable; not shader
failure. Authorized native GPU run35852 exited0 with API+GPU validation enabled:
200 strict full-color/rawD32 bitwise pairs across50 cases/four output formats,
shadow and local-light positive controls, rigid/skinned, alpha/clip/coverage,
read-only depth,2/4 cascades/quality0..3/F/RZ/bounds/fades/unbuilt maps; thermal0.
Exact argv/exits in compile-exits.json, compile-retry-exits.json and
strict-validation-native-exit.json; full validation log retained in packet.
No performance/occupancy/register gain is yet measured or integrated. Derived
quiet A/B/control timing adapter is source-only preparation; capture only if
merited. Production shader/PSOs unchanged. Ambient-only simple walker is a
separate source hypothesis preserving dynamic family arrays/exact traversal;
compiler may already eliminate its dead gloss/legacy state. No gain assumed.


Distinct ambient-only SIMPLE packet gtmapgbm also independently source-reviewed:
actual caller false/0/0 cannot sample gloss/legacy. It keeps dynamic family arrays
and unchanged cursor/order/64 candidates/32 accepted/default, SH/weight/mix,
shadows and fragment math. Prior rejected named-accumulator rewrite instead
changed representation/branching. Combined shaderfee6e71420eef5a8c47c69a3203706d6e8633ecbe9950624242042f99cd03590;
nominal sum112→48 bytes is not physical-register proof. Root derived validation
host7045921ccb6269ae4e2635f2599367bece80f10a5ccaded3b9023ac68a8f9a90 from
frozen probe-shadow host, selecting ambient candidate for BOTH probe and actual
unflagged-main case; only identical-PSO control aliases baseline. Compile60428
metal/metallib/host all0; native GPU53042 API+GPU validation actual0,200 bitwise
color/rawD32 pairs50×4, positive/coverage checks, thermal0. All inputs/hash/prefix
verified. Production source remains unchanged.

Independent timing adapter8bdf6dfd16671e44fcb22a262d6ef7afd3a95336d88759d2ca231529a39a4b19
and root ambient derivative98f34d6428fb6e1535ef536d7893e7629576d3090ced3c9a29dfa5aff39d1705
compile32525 each0. Six cached setups/7 counterbalanced ABBA blocks/168samples,
same actual-PSO control and shared within-block physical targets, common8–20ms
commands; validation/HUD/capture OFF and thermal nominal required. Await source
review before GPU timing. No performance or register conclusion yet.
Doctor post-profile native run exited0 (sandbox first run1 at hardware sysctl);
M1 Max/64GiB/ARM64/macOS26.5.2/Xcode26.6, unified memory, recommended working
set62277025792 bytes/Apple7supported. Full log retained privately in/tmp.


## Actual specialization timing and production implementation

Initial forced-clone168-sample/7-block timing21311 actual0, all8–20ms/thermal0,
found accepted rigid/skinned−24.61/−22.43% paired medians and rejected-neighbor
rigid/skinned−29.27/−31.65%, ALL7/both targets negative. Default-only+5.34%
[+.31,+18.74] regressed; same actual-PSO median+1.06% range crosses zero.
Ambient-only same serial run0,168samples, every paired range crosses zero:
REJECTED/no production change. Independent parser/hash/schedule checks pass.
Density followup76515=0/168 samples:2accepted−16.79%,2outside−6.39%,8accepted
−32.79%,8rejected−21.53%,8manual−25.39%, all7 negative. Control+1.92% variable;
smaller effect is qualified. This motivated actual probe-count>1 admission.

ROOT now implements optional Metal bool function constant0 in the existing
filter guard, default false. True folds the already-taken raw-depth single-tap
branch; no math/caster/quality changes. Stock simple-alpha three-mode PSOs and
custom blend cache mode use ACTUAL validated bound shadowbit4 andactualprobe
count>1. Main/default-only/legacy/avatar keep runtime; constant list cleared
before other materials. Existing native function-constant wrapper reused.
Production MSL920d2e2d927b94a43cd396478bfa3776bc2edccaf5e5dd3ef8eb10bbbfcb1fa1.
Two independent source reviews approve cache/index/null/constant-reset semantics.

Actual-functionconstant packet at/var/folders/ds/8jjw6_7x0kzd98p2w2z16r3w0000gn/T/alchemy-metal-probe-function-constant-20261109.ad1tpate
(pointer /private/tmp/alchemy-metal-probe-function-constant-latest-path-20261109.txt).
Compile31725 metal/metallib/host0/0/0; API+GPU10157=0 ALL200 strict color/rawD32
pairs50×4formats against unchanged old d6ad library, including actual newsource
main/default/legacy fallback, thermal0. Guarded quiet timing33685=0/168 samples:
accepted rigid−23.2226%[−30.17,−21.75],skinned−25.4957%[−31.50,−20.11]; rejected
rigid−30.6883%[−35.90,−26.55],skinned−31.8305%[−34.45,−27.60], ALL7/both targets.
Default-only−2.08%[−9.60,+10.08] and samePSO−2.00%[−5.65,+1.76] have no isolated
benefit. No competing build/GPU/profile; all nominal/inrange. Independent parser
checks all168/schedule/normalization/hashguards; summarySHA
dd2b60e251c485df3726bbba296d965cacddddfe90ca3ab7abbb12cad6af0a62.
These are mechanism timings, NOT viewer FPS or proved spill causality.

Small actual-functionconstant A/B/B/A capture producednative0/preflight+finalA
strict color/depth equality, at/private/tmp/alchemy-metal-function-constant-registers-20261109.8jh1vz8a/four-simple-shadow-draws.gputrace.
Hostec5e1f3ba900454e34aaffa1aa0c466638f99bf2380b98304655dccb1dff9676;
planned1CB/4clear-storeencoders/4draws/1024²RGBA16F/readonlyD32, commit→stop→wait.
Actual Xcode census/replay/profile/registers/spills NOT yet observed. User has
explicitly handed Xcode control back to root. Replay remains stopped until all
GPU tests complete; those have now completed. All viewers closed/plans restored.

Root applied independently reviewed public-renderer patchfebff14885bc3d45083e698276d89215515d3540310c2108748f1353605f0111:
local checkerboardR32 plus unchangedSH, fullRGBA16/rawdepth count1vs2 equality,
rigid/skinned/standard/custom blend, specialized custom FIRST thenmain→probe→main
and PCSS-vs-singleTap covered-pixel positive controls. Existing state/maps remain
local/unchanged. First test build76595 exited65: Xcode app tree has tests disabled,
not compiler failure. Corrected verified Ninja Releasebuild72719=0 (57.25s),
fullAPI+GPU95754=0 (129.69s),427529 assertions/95 cases PASS. RWDI70294 build0
(52.35s)/fullAPI+GPU0 (37.20s), SAME427529/95 PASS. Capturetests excluded asbefore;
no capture lifecycle code changed. Exact argv/exits/source hashes and full logs
are in ad1tpate. No new normal app or live FPS measurement yet; code uncommitted.


## Small actual function-constant capture verified and code committed

Root took back Xcode as explicitly requested, opened exact8jh1vz8a capture via
Mac Open/GoTo shortcuts (paste did not update GoTo field; AXsetValue corrected
and exact URL verified before Replay). Actual Summary1CB/4render/0compute/
4draws/0dispatch/1024²RGBA16F,49.45MiB resources. Replay then Profile completed
29.09ms at MEDIUM PState, four synthetic mechanism draws, NOT live frame/FPS.
Shader rows: baseline245 allocated/high registers/304 spilled bytes/cost55.69%;
guarded functionconstant candidate224/224/304/cost44.22%. Static spill allocation
is UNCHANGED, so no spill-elimination claim. Register reduction alone does not
prove occupancy/performance causality. Encoder draw0A25.17/draw1B22.09/draw2B22.18/
draw3A30.56%; single-profile variability is not substituted for quiet paired tests.
Root clicked Stop, observed Finished running macOS App/Replay landing, then
next CUAcall completion Measured lower probe shader registers before other tools.
Rawprivatead1tpate/xcode-register-profile-observed.json records this observation.

Committed three source/test files as735a407bb7da33a95bf14f7cbae995e1d9a91d4b,
metal: specialize probe simple-alpha shadow pipelines.173 insertions/10 deletions.
Publicsource/C++test exacthashes in buildrecords. Both optimized fullAPI+GPU
427529/95 and private200strict/168timings pass asabove. No new normal app/liveFPS
result yet. Next normal source-recordOFF doctor/build/verify, preserving old68590,
then fixedlocation/Ultra/Midday/Preset123 controlled300frame comparison. No other
families combined. Independently reviewed source-only PBRt54uaujb and flat-water
qfeQtB fixtures remain UNCOMPILED/UNRUN; no production admissions or gains claimed.


## Normal app built/verified and controlled viewer comparison started

Clean fullsourceb299d5c99afd519da82f0db11b5b9ee60f88b283 (renderer735a), normal
68594. Build13489 actual0: preserve68590/doctor/configure SOURCE_RECORDOFF/full
toolsbuild each0 (build45.56s). Packet/private/tmp/alchemy-metal-b299-normal-build-20261109.wpzx41rj
holds argv/exits/logs and preserved68590. Canonical app at usual Release path;
exeac27e181622600b2c561c681c8734778a3f50a34e6c07912fb6b60e10e8f058f;
metallib61c5dcb3fa1677818cd3ef6b069837979a00cf9cf4d56cf1594e46d011d41585.
Rootverify_app87922=0. Independentstatic96763=0:349MachO/0nonARM64/deepstrict
signature0, embeddedfullsource/HEAD/headerb299, packagedmetal=normalartifact,
cacheOFF/exactstd3.1/fastmath/min26.0/no debugsources, freshAIR/currentMSL.
Identityreport/private/tmp/alchemy-metal-68594-normal-identity-20261109.zgkX3Z/app-identity.json,
SHAcfe672bab98752f763c27f9f62db7bbfedfee16120cba6bc981648c427a0004c.

Pinned fresh normal QAconfig/launcher dryrun0. NEWb299Middayinstall0/QAlaunch0;
exactlivePID14587 verifiedvia libproc/path/EXESHA beforeUI. Cua boundexact68594,
CtrlSuperF fullscreenBEFOREloginReturn, progress seen; freshLEAPloggedintrue.
Cua completion Logged into verified fullscreen viewer recorded. Freshstart
observe/verify/gate each0:43controls/location/exactbinary, Ultra6/3456×2168/
Preset123/aniso4. No validation/capture/split/probe workloadflags inrun. World
screenshot aftergates timedout15s/resetCua; no image/visual-smoke claim. Cua
completion Recorded viewer screenshot timeout endedattempt; no repeatedworldUI.

BenchmarkCSV private-b299-normal-probe-shadow-20261109-01 isstillwaiting for256
ready/stable120 then300samples, minimumwarmup300s/120frames. Earlycamera Z−5.8cm
settled byframe5172 to+34.3micrometres vsprior0d5diag; camera basis/projection/
EEPidentical, camera_changes0. This iswarmup, notformalposesampling. No liveFPS
result yet. NEWb299setupplan remainsACTIVE untilverifiedshutdown; alloldplans
restored. TranslatorstaysSTOPPED, XcodeSTOPPED, no otherbuild/GPU/profile. Root
ownsmeasurement. AgentsfinishedPBR/water source-onlypackets, neithercompiled/run.


## Completed normal68594 comparison and startup observation — 2026-11-09

Normal sourceb299/renderer735a, full source/library/executable identity verified.
Formal300ready native-exact frames7949–8248 at Ultra3456×2168/bias1/256ready/
stable120: mean103.782010697ms, median103.661375, nearest-rankp95108.9555,
9.635581285FPS. Best68579 was9.608672762FPS: +0.28%, effectively flat. Earlier
build lacks the64-neighbor repair; physical draws differ~3.8%, so this is not
an isolated shader A/B. Fresh start/end43-control/location/binary gates exit0;
pose changes are warmup-only. Independent report:
/private/tmp/alchemy-metal-68594-formal-independent-20261109.p5xjUcY1/independent-formal-summary.json
SHA602fd7673d857cf3122dc697d8fb5978e885b958c3e37a9b83a99bccdeff7ab0.
Raw artifacts/metal/private-b299-normal-probe-shadow-20261109-01.csv and sidecar.
Exact14587 graceful shutdown/Goodbye/absence75320=0, oldnormal plan restored0.

Owner then requested CURRENT68594 restart and direct FPS-widget observation.
LIVE17022/path/executable hash verified; Cua fullscreen before saved login.
NEW artifacts/metal/private-b299-startup-readout-setup-plan-20261109.json ACTIVE;
restore only after exact17022 exit. First launch failed1 before viewer start
because private wrapper/inner logfile paths collided; corrected launch0, no
plan overwrite. Current launcher/private/tmp/alchemy-metal-b299-startup-launch-20261109.py.
Installed skill files unchanged; private copied LEAP observer reads actual
FPSText through official LLWindow.getInfo. Cua completion Confirmed live FPS
display changes recorded; no new world screenshot or visual-parity claim.
Fresh43-control/location/binary observe/verify/gate each0. Widget log:
artifacts/metal/qa-runs/candidate-b299-startup-readout-20261109-01/fps-widget-observed.jsonl.
Observed32→11 over22.44s, later10, matching user11/12. Brief160 was not captured;
initial polling gaps up8.82s mean it cannot be excluded.240/0 initialization
values are not measured FPS. Source widget averages199 completed frame rates,
rounded integer; WORLD_INIT resets login history. It can lag real workload.

Startup raw6000frames/558.45s/allnative1 completed in
artifacts/metal/private-b299-startup-readout-20261109-01.csv +provenance. This is
DIAGNOSTIC: warmup0/readiness observer and admission disabled, so false readiness
fields do not prove missing/unbuilt probes. Actualfirst10s12.257585FPS/median
44,631encoded draws; next10s11.038430FPS/50,697draws, unchanged camera. Real
workload growth accompanies display smoothing. Later drawcounts plateau52–54k
while throughput declines toward10; no complete causal attribution from counts.
Small camera-Z settling at300–302s prevents identical-pose startup-vs-late claim.
Independent analysis/private/tmp/alchemy-metal-b299-startup-readout-analysis-20261109.EZPDu5/startup-readout-analysis.json
SHA813732a14f4e310262427637ba3e2abf6f2e19bfb39ffea40c6a12c460f3a53f.

No20FPS/live speedup/infeasibility claim. Translator remains STOPPED, Xcode
replay STOPPED, no concurrent GPU/build work. Next root: attribute available
startup counters; then exact17022 shutdown/currentplan restoration before
corrected64-parent control or actual-selected-PSO census patch/test/build.


### Startup pass population cross-check

Independent source-frame-aligned98post snapshots and56numeric pressure samples:
/private/tmp/alchemy-metal-b299-startup-readout-analysis-20261109.EZPDu5/startup-pass-growth.json
SHAcba4c42f64463719f974a3e6dfca742c9ba252b0173177f1b6751e0deaaf047b;
analysis exit0, root reread/hash matches. Main view probe pack2at10.2s,
39at119.8s,74at291.4s,98at427.8s. Hero face1 keeps832post batches/223initial
and identical recorded family/index counts as pack105at163.6s→237at389.7s;
other fixedfaces similarly reach256. This is evidence of growing lighting
population with stable encoded-family counts, not literal sampled-probe counts
or causal milliseconds. Current walker uses depth buckets, minDepth break,
first-owner ordered max64neighbor chain, max32accepted, conditional default.
From60–120s to240–300s mean frame time85.0→93.8ms while draw/submission totals
broadly plateau. No per-pass CPU/GPU timing in this dataset. CSVmetal_gpu_ms
sums overlapping completed command-buffer intervals (with delayed reporting);
do not treat it as active GPU union or add it to frame/CPU times.
Pressureimage1293→7761MiB/device3133→9730MiB; all56system_low0 and all6000 finite
discard-bias samples1. This shows residency growth, no measured cache churn or
pressure-driven bias escalation. Root authorized a separate10s nativeCPU sample
of exact17022 to distinguish main-thread execution/wait. Actual exit0 but only
TWOmain-thread observations,35.156s invocation and DWARF-analysis warnings: unusable
for hotspot attribution. No font-shaping CPU claim from these two observations.
Private raw/private/tmp/alchemy-metal-68594-startup-cpu-sample-20261109.zNbsrCJC/sample.txt
SHA4ad93a824e26e0c4853949b2b91467841a9684a8c5618e48e36a29fe1b827d64.
Wrapper preliminary CPU seconds mistakenly treat Mach ticks as nanoseconds;
do not use those derived fields. Raw rusage preserved; agent corrects units in
separate record. No rerun queued. Existing watcher fresh10/loggedin/available at
21:48:18 after sampling; viewer remains running/current plan ACTIVE.


## Actual-selected-PSO census integrated — 2026-11-09

Renderer commit 4e833fa29aadabee3a66023fb67da0975f6b6c7f. Applied reviewed private patch1bc731; allfour source
hashes match manifest. NativePSOdiagnostics have no exactOpenGL equivalent;
nearest1eb cube_snapshot producer/shadowguard and Metal alpha owner unchanged.
Counters only: no shader/quality/ordering/fallback change. Observe actualchosen
PSO AFTERsuccessfuldraw,13 independentlydatedviews/12disjointbuckets, publish
afterwholerenderfinishsuccess. Encoded draw/declaredranges, notGPUtime/invocations
or laterouterbatchcommit. Defaultunarmed skipsinspection/counters. Independent
shadow_contract re-review approved f7+patch; rootverified allsourcehashes.
Exact68594/PID17022 path/hash/SIGTERM/Goodbye/absence71674=0 in2.01s; startupplan
restore0. Allviewers closed/allplans restored/Translator STOPPED. Normal68594
copied/private/tmp/alchemy-metal-68594-normal-preserved.app, originalSHAac27...
verifiedbefore/after. Owner clarified thisstart~38ratherthan160, numbervaries.
NinjaRelease82672 terminal/noopcompletioncheck0; RWDI49590=0. Initialsandbox
suite42/noMetaldevice retained separately. NativeRelease28560=0 andRWDI32381=0,
BOTH428004assertions/95cases. Packet/private/tmp/alchemy-metal-selected-pso-validation-20261109.bh6k72uq.
GPU/APIvalidationON, no viewer/profile/build competed. FailedCPU samplequality/
Machunitcorrectione0656051 saved; no CPU hotspot/normaltimeclaim.
Next normalappwrapper/private/tmp/alchemy-metal-selected-pso-app-build-20261109.py,
thenexactnewidentity/privateQAconfig/freshMiddayplan. Corrected64parent68588
control deferred toprioritizeactualselectedPSOs.20FPSgoal ACTIVE/unmet.


## Continuous startup CPU capture — 2026-11-09

The owner asked for CPU profiling from before login through the visible decline.
The same normal68597 executable/source6f620/renderer4e833 was verified by exact
PID/path/SHA before fullscreen and saved login. The initial/end43-control,
location and binary gates all exited0. No shader or quality change was made.
Prior48160 diagnostic300 readyframes gave8.159294607FPS, separately labeled;
its exact shutdown92003/Goodbye/absence and environment restoration exited0.

The new viewer50766 was profiled continuously from the login window BEFORE
login for720.726011s,22:20:40.373–22:32:41.099UTC. xctrace TimeProfiler14305
exited0; TOC33140/time-profile16452/thermal60378 exports each0. The private
packet is/private/tmp/alchemy-metal-continuous-startup-profile-20261109.3a9on15l.
CPU export274330512bytes SHA8d7cc7bcc4b3b563385cb6facfb22bcb5bf5c5726c2c4b3b2ae2445afe92d5ce.
Streaming analysis ran once/exit0/22.99s:920867 exact-target Running samples,
596968main,596.968 sampled mainCPU-s, no reference errors. Fullanalysis SHA
e50ce5dfdcef41b7de69acbcb2c7dbcac1975f9a52fcddb1c2db1c75fbb59ada;
findings8d1a48fcd78db2a11ed4b5673495a825ba1087cb767277826200f98b924a47c3.
The parser was validated once against the earlier11076/8417 fixture and uses
compact references without retaining XML; it was not added to production tools.

Main-thread CPU-active weight declined27.4→23.7s per30s (roughly92%→79% of one
core) as the visible counter fell12→11→10. World rendering stayed on Pcores,
with only.001 sampled Ecore-second after the first30s. Hero work remained about
half main activity; shadow/Gbuffer/bindings/draws stayed prominent without an
upward CPU ramp. Root's approximate rawframe alignment yields77.558 sampled
mainCPU-ms/frame at11.782FPS (trace120–150s), versus76.463 at10.751FPS(510–540s).
These are30s statistical proxies, not an exact CPU floor. Alignment uses CSV
birthtime and2391Hz frameflush observations, lag.087–1.094s/median.610s. Report
startup-correlated-phases.json SHA4e46f5c4afbe2ad4516b5a428d2d23f4a726151fd7d00e13df2ff1220e0f9c55.

The raw startup log contains6000 consecutive frames199–6198/527.46s, allnative
exact1/fallback0, Ultra3456×2168/bias1. First10s12.347FPS/43,506.5 median draws,
next10s12.067/47,085; later~52k draws and10.674FPS last300. Raw stops before
12min CPU trace; remaining CPU bins have widget/process/probelog observations.
The displayed peak was41, compatible with the owner's variable~38 startup;
no repeatable160 value assumed. All frames are diagnostic (warmup0/readyobserver
OFF/exactgateOFF), not a formal gain comparison. False readiness fields prove
nothing about probe completion. Actual view packs grew toward256; GPU completed
CB interval sums increased~226→473ms/frame in matched bins while CPU/frame stayed
flat. Sums overlap and completion lag applies: this supports investigating GPU
work/backpressure but does not prove GPU active time or causal milliseconds.

Thermal export reportsNominal for the entire720.726s. The owner heard louder
fans; that observation does not establish throttling. CPU/GPU clocks remain
unknown: powermetrics exited1/requiresroot; sudo-n preflight1/password required.
No privileged sampler ran. Correct Mach125/3 process CPU/RSS/disk metrics were
recorded at1Hz. The12min recording had0DWARF warnings with cached symbols; this
does not prove the proposed CAS metadata cause/fix. No compiler flags changed.

Exact50766 shutdown29248 exited0 with Goodbye/PID absence; continuous setup
plan restored0/allolderplans restored. Translator remains fullySTOPPED; Xcode
GPU replaySTOPPED. No runtime jobs remain.20FPS goal remains ACTIVE/unmet.
Next: measured GPU probe-work growth plus broader scalar covered-shadow draw
reduction; staticbulk-only optimization deferred because it is only2.22% of
main CPU weight. A private source-only covered-coalescing proposal is pending.


## Covered-shadow draw coalescing and Apple-primary research — 2026-11-09

Production commit5e1ecb76459de8a0bc90dc16da56aec75f1fc02d, test-only native-counter
strengthening96af75c0d978b1c61c64b2b74ef9aa239ba376bc. Whole-scene validation
remains before encoding. Compatible adjacent static covered complete triangles
merge without sorting, remapping, shader or coverage changes. Geometry/offset/
range/index type, coverage/count/cull, exact matrices, cutoffs/transforms and
used texture identities must match. Avatar/rigged/terrain/ICB do not merge.
Exact1eb shadowAlphaMask/PBR paths inspected;1/512 mask bias/checker stipple and
rawR32/reverseZ/clamp behavior preserved. No actual live gain claimed yet.
Private source proposal ebf20cf6 and tests e4ac7988 received two independent
reviews; root added retained-output and native encoder draw/commit assertions.
Focused firstGPU1573/1=0; fullRelease47183=0/RWDI47338=0,428946/95 each. Final
native-counter focused1747/1 passes BOTH configs. Native calls6→1/fieldbarrier3
and no draw/commit/retained change on later invalid inputs are verified. All
API/GPUvalidationON; no viewer/profile/competingGPU work. Packet
/private/tmp/alchemy-metal-covered-shadow-validation-20261109.ainfndy_.
Normal68597 baseline preserved/private/tmp/alchemy-metal-68597-normal-preserved.app,
executable47670056... verified before/after copy. Next normal candidate build
wrapper/private/tmp/alchemy-metal-covered-shadow-app-build-20261109.py.

Apple-primary research found no matching published version-specific driver bug.
Apple documents CPU overhead from individual resource bindings and explains
how argument buffers reduce bookkeeping; this matches the type of work in our
profile. [Argument-buffer guidance](https://developer.apple.com/documentation/metal/improving-cpu-performance-by-using-argument-buffers).
Metal4 core supports M1 and later and permits incremental command-encoding
adoption. Explicit residency, synchronization and resource lifetime ownership
must accompany it. [WWDC25](https://developer.apple.com/videos/play/wwdc2025/205/),
[core API](https://developer.apple.com/documentation/metal/understanding-the-metal-4-core-api).
Exact26.5.2 advisory unknown; inspected Xcode26.6/macOS26.5 notes supply no
matching runtime fix. No cache deletion/toolchain switch/system change made.
Research/private/tmp/alchemy-metal-primary-performance-research-20261109.md
SHAbb30ee13b4538aa92d5f4c354bc4aa37aead929338560534111436c22d019b3c.

PRIVATE M1Max feasibility packet:
/private/tmp/alchemy-metal3-metal4-covered-shadow-20261109.25axBX.
HostSHAcb19ab4f82335ae694c190261605ba7a5d6f2177ed8afc260bcd160215054fa1;
clang++ C++20/O2/ARC/min26.0/Foundation/Metal compile0; native API+GPUrun0.
ActualexistingMSL3.1 library61c5dcb3.../SAMEoldnativePSO/sharedsampler,608B ABI,
two distinct immutable768B-offset transforms, singletexture/64²R32+D32. All12
coverage/Z pairs plus2cutoff positive controls pass full rawBITWISE parity with
visible pixels and same-target poison. M3cache/offset setters match current
issued-work model; M4persistent argument tables/explicit residency/owners/
sharedEvents/queuebarriers/feedback/GPUcompletion-before-allocator-reset are
present. Sampler supportArgumentBuffersYES is shared by both test backends.
No performance,8/32texture,rigged/terrain/tessellation/clamp/perspective or full
viewer Metal4 claim. Future quietCPUbenchmark is a separate source-only packet;
actual timing and production adoption remain unexecuted.20FPSgoal ACTIVE/unmet.


### Normal covered-shadow candidate68601 built

Source77977290c9bd2e3712d547f7b8e75b20c4191eca, renderer5e1ecb/test96af75.
Normal doctor/configRECORDOFF/fullbuild/verify91206 all0. Packet
/private/tmp/alchemy-metal-covered-shadow-app-build-20261109.uiuvk2ho.
ExeSHAf277ea10cbf98cb048e58f6644a6641f3af16f1120f26832c8043199f883a022;
metallib61c5dcb3fa1677818cd3ef6b069837979a00cf9cf4d56cf1594e46d011d41585
identical68597. Independent narrow review includes actualdeepstrictcodesign0,
source/header/version/exe/library/normalflags; root349ARM64/0bad reviewed, not
rescanned. Report/private/tmp/alchemy-metal-68601-identity-review-20261109.9jc3ukr2/identity-review.json
SHAcb0759d5795881d7d4c58edc9a29f165cfabf6383be85241b20ac9657d4fc234.
Pinned privateQAconfig and launcher dryrun0 are ready; no viewerlaunch/setupplan
installed. Allplansrestored/TranslatorSTOPPED/XcodeGPUreplaySTOPPED/no jobs.
Metal3/4 quietCPUbenchmark prepared separately/sourceonly atxitJjX, explicit
validation-only and quiet-timing modes; no timing/speedup claim. Next measure
CPUcommand cost and actual coveredmergepopulation in separatequiet windows.

### Fixed-material native Metal3/4 CPU experiment executed; no win

Frozen packet /private/tmp/alchemy-metal3-metal4-shadow-encoding-20261109.xitJjX,
hostSHA5a36d4e0991da61ea6f6a77d03d7034d019f9f49da9c13b48b9c437270747784,
manifest8397ee31784f20359381bbab82633f95c1eae50a3353cf557ff17146ac5645a1.
Actual O3/ARC/min26 compile0, nativeAPI+GPUvalidation-only0 (4strict fullR32/D32
pairs and0timings); separatequiet --timing0/168samples at23:27:41.789–42.060UTC,
before viewer launch. M1Max, thermal0 throughout, competingknownprocesslistempty.
Existing currentMSL3.1/PSO, constant-material singletexture, N1024/4096 distinct
MVP/cutoff records at768Bstride,2physical targets,7rotatingABBA/BAABblocks and
genuine same3/same4 controls. NativeM3cache+offset-only setters/currentinstance1
draw selector vs persistentM4tables/constantaddress updates; ownership/residency/
barriers/crossqueueevents/GPUcompletion-before-allocator-reset retained.

Pooledprecommit medians1024draw107.459us M3 vs113.894us M4(+5.99%),4096draw
387.270vs404.291us(+4.40%). Sevenpairedranges−.85..+11.77% and+4.86..+7.33%;
M4faster1/7 and0/7. Summed measured host scopes excludingGPUwait113.437→120.809us
(+6.50%) and393.685→412.437us(+4.76%). These are sums of instrumented host wall
scopes, not threadCPU or full contiguous lifecycle. Setup81.6ms and GPUwait
are reported separately. Controls1024M3+.70%/M4−1.37%,4096M3−1.70%/M4+.68%;
short .1/.4ms encoding scopes limit small-effect precision. No Metal4 performance
benefit to promote from this case, and no fullrenderer/FPS conclusion.

Independent source/schedule/lifetime review and strict168row parser:
/private/tmp/alchemy-metal3-metal4-shadow-parser-20261109.shTjM2Y0/REVIEW.md,
timing-summary.json SHA03f54b3597e2067aac170e707ad1869c9e687f6d3eed732a7fd0a195801c748d.
RawtimingSHA9b98cbc5cddbf126ddd5ae9c8b7da76329b451450bac23587d2a199560de1b24.
Exact source/executable/library/env/exit provenance root-quiet-timing-exit.json.
Next experiment is a separate changing-material1/8/32-slot source fixture;
fixedtexture does not exercise ResourceList costs from changing textures.

### Covered-shadow live comparison admitted to warmup

LIVE68601 PID73462/source77977290/exe f277ea10... normal library61c5... unchanged.
RootinstalledFRESHplan artifacts/metal/private-covered-shadow-setup-plan-20261109.json
(ACTIVE), QAlauncheractual0, runtime path/hash independentlymatched. Fullscreen
shortcut BEFORE savedloginReturn; loginobserved and CUAcompletionrecorded.
Freshobserve/verify/gate each0 verifies43controls/location/exactbinary. No
worldimage-parity claim. Currentrun candidate-covered-shadow-20261109-01;
CSV private-covered-shadow-20261109-01.csv waits forready/stable probes, with
provenance sidecar active. Atframe3918:256assigned/151incomplete, camera same3721.
Recent120-frame numericlog interval: main216720/1332141 source-family static
covered commands removed(16.27%), probe300274/1600111(18.77%). Denominator is
source family rather than fullymergeable inputs. Commands removed are not
milliseconds or measuredFPS; counter audit and formal>=300frames pending.
No competingGPU/profile/build. RestoreONLYthisplan afterexact73462gracefulexit/
Goodbye/PIDabsence. TranslatorfullySTOPPED/XcodeGPUreplaySTOPPED. GoalACTIVE,
20FPSunmet/noinfeasibilityproof. Finishcapture/endgates andpreserved68597control
before running the new private experiment; all agents text/source-only meanwhile.
