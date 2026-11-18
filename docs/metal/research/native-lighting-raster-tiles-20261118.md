# Native lighting raster tile geometry —2026-11-18

This group implements an optional native16x16 raster tile request for ordinary
main-view BASE and forward lighting. Authored low- and high-overlap real-renderer
checks show a useful component GPU-cost signal while preserving exact stored
output. The control defaultsOFF; this is a provisional owner-checkpoint choice.
No whole-viewer FPS, register-spill, hardware-occupancy or full architecture
acceptance is established by these checks. The Home decline remains PPD319.

## Mechanism and preserved behavior

`ALMetalRenderPassConfig::raster_tile_size` defaultsautomatic. The native render
pass sets16x16 only on a requested Apple4-capable target; ordinary world owners
fall back toautomatic on older unsupported families. Invalid enum values remain
fail-closed. The encoder reports its actual native width/height without flushing.
The existing immutable BASE/forward scene inputs carry the request. No tile
shader, threadgroup cache, extra attachment, pass, queue, worker, readback or
lighting approximation is introduced.

`RenderMetalSmallLightingTiles` is a declared Boolean, defaultfalse/nonpersistent.
Only ordinary mainRT lighting requests16; ordinary probe/hero/HUD/cutout/snapshot
paths remainautomatic. BASE and all five forward-encoder begin sites propagate
the choice, including atmospheric/water haze, water and diagnostic-family
continuation. The per-owner census distinguishes original request, effective
capability policy and actual dimensions for every successful begin. It counts
begun encoders, not completed frames or FPS. Successful reload preserves all
15 cumulative/last-observation fields.

Original reference1eb44bbd deferred/alpha producer/consumer paths have no Metal
raster-tile API equivalent. Their light contributions, probe32 cap, default/
manual selection, alpha ordering, depth/stencil/query semantics and material/
water/sky math remain the behavior contract. Production MSL SHA256 remains
239144bbd96dae880ac2994221bf80fdc3a5cfab4e2183b7b7509a732c1f2a97;
active offline metallib remains
fae7a43b5b48b7375f3a47ae52b6c7c7d89dd6b5f6a7456562d9e99438e223d0.
No new shader variant or compiler-option change was selected.

## Controls and selection

Private packed-SH tile-cache pilots initially confounded geometry and caching.
Matched controls87strict/59release and3600retained observations separate the
geometry signal from nearly neutral cache-load effects. Packed-SH tile caching
is **unselected**; its private source/failures/positive and adverse pairs remain
under `tile-sh-consumer-pilot-01`. The scoped-clone characterization remains
FAILED on2one-component helper half differences;549actual production images
match, but this is not a blanket shader-parity waiver.

Independent PPD393 conversion proof separates language/CPU RNE from native
attachment rounding for ten finite normal-range inputs on this device/current
options; subnormal/NaN and universal conversion behavior are not established. Its auxiliary same-format
reference repair is source-only and does not erase the2strict A/B mismatches.
The compiler default/O2 audit found no demonstrated optimization-flag defect.
No compute threadgroup workaround is applied to raster shaders.

## Actual renderer quality and cost

API test passes697483assertions/2cases with API+GPU validation. Real production
offline-library suite passes1,249,016assertions/4cases: BASElegacy/PBR, generic/
prepared, reverse/HDR/infinite modes, simple/material/PBR alpha rigid/skinned,
ordered pre/post-water, High2, preserved depth/stencil, probe-response, atmosphere
restart with mandatory post-draw omission difference, water haze/surface with
normal-response control. This verifies authored deterministic output, not all
Second Life content or owner visuals.

Every listed cost cell runs in a separate process with API/shader validation,
HUD and capture disabled. Its same-factory/offline-library sentinel reports
actual shaderValidation2; this is not an exhaustive private world-PSO census.
Each cell performs immediate exact output/depth/stencil/nonvacuity and prepared
activation checks,10warmups/arm then150counterbalanced ABBA/BAAB blocks,
300observations/arm. All observations/adverse blocks are retained. No outlier
removal or between-process absolute-time subtraction.

GPU columns use the exact consuming-command completed timestamps; owner wall
includes current frame-arena/target acquisition, constant/resource preparation,
encoding, commit, waitUntilIdle barrier, completion/output receipt retirement.
Device/renderer/PSO creation, fixed authored G-buffer/textures/input seed, and
LL scene collection/culling/sorting are outside the interval. These are not
CPU-thread time, whole-frame latency or viewer FPS. Measurements were serialized
on the actual M1Max/64GiB/macOS26.5.2/Xcode26.6, AC100%, with no other viewer/
compiler/native GPU job. pmset recorded no thermal/performance warning; that
is not proof of thermal-state equality or absence of throttling.

All5400samples show automatic request0/effective0/actual32x32 versus explicit
request1/effective1/actual16x16 on this host. BASE/forward owner counts match
each actual pass. Prepared47 fixtures preserve all bindings/math.

|Cell and authored demand|GPU medians A / B (ms)|Paired GPU B/A|Adverse GPU blocks|Paired owner-wall B/A|
|---|---:|---:|---:|---:|
|0: BASE legacy, low overlap|1.482084 / 1.480834|1.00007080|76/150|1.00076464|
|1: BASE PBR, low overlap|1.443167 / 1.433250|0.99292739|47/150|0.99345164|
|2: 7 alpha layers, low overlap|9.048709 / 8.521812|0.94715784|2/150|0.94753625|
|3: Sparse alpha, low overlap|0.365625 / 0.218562|0.59734150|2/150|0.78370270|
|6: BASE, default probe only|0.221562 / 0.214208|0.97894568|48/150|0.99692990|
|7: 7 alpha layers, default only|1.737979 / 1.633854|0.96764675|25/150|0.96974501|
|8: BASE legacy, authored32 accepted|3.955916 / 3.713417|0.93841953|9/150|0.94383258|
|9: BASE PBR, authored32 accepted|3.476707 / 3.269833|0.94236187|11/150|0.95126604|
|10: 7 alpha layers, authored32 accepted|19.313937 / 17.884271|0.95823791|21/150|0.95803546|

Paired ratios are the median of150within-block mean(B)/mean(A) ratios, not a
ratio of two independently pooled medians. Do not substitute the latter when
there is process drift. Low-overlap BASElegacy is neutral; PBR's0.7% result
alone is weak. Layered alpha and the authored heavy cases support provisional
viewer evaluation without changing the product default.

The primary47configured low-overlap pack accepts only1–5SH entries/fragment
(ideal analytic eye-plane area mean~2.63 for BASE); configured count is not accepted work. Optional
8/9/10 deliberately place31local volumes around every authored receiver and
retain15bounded volumes. The unchanged accepted32cap or default fallback then
produces32semantic SH entries/covered fragment while retaining prepared-mask
eligibility. This is authored stress, not recovered Home per-probe demand or
a GPU instruction counter. The current Home accepted/positive-scale
distribution remains unknown.

## Exact source, failures and integrated repairs

Native API root6ad2ada19a, world consumer0ad5b408a8, appearance repaira84d8cf5
are local. Costs0/1/2/3/6/7 use build04/builder
e6b5619a7a653b690b585d247a5517974ab12eb5, indra
d22e2d74d6df1ab2cf442ebdc75a2d0e8bd81a81, native test binary
b62bafdf77ac8d9efb0420b2d18ca0296333b5dcb9cbf932e3298b074e4f4f44.
Heavy8/9/10 use build05/builder
b157a873a26d4fa0c28a17249910552d1c4ed322, indra
15c6dd139823fb822e0509783242d907ca6c1f97, binary
cc1c84823855cd6955bbc2e0da6ea4e920c42acdba13a48c72f51f0981e2708f.
The difference is test-only optional authored stress and metadata, with unchanged
production implementation. Both exact native test app bundles are preserved
before subsequent rebuilds. Per-process argv/environment/source/binary/library
hashes, rawstdout/stderr, power checks and reductions remain private below.

PPD400incorrect field placement and401/402census gaps were caught by source
review before native acceptance and corrected; no failure history is erased.
PPD403atmosphere draft was clipped by its authored plane, fixed with a required
omission control. PPD404material fixture failed closed at its first invalid
material declaration:804722assertions passed before1failure; remaining cases
were unrun. Failed bundle/binaryd31b384e is preserved. Three fixture declarations
fix the exact mode/UV contract; production validation/math/thresholds unchanged.
PPD405first reduction KeyError is retained; corrected metadata read reduces
unchanged native data without another GPU run. PPD406private warning writer
parse failure is retained; direct-file AST/execution0 repair regenerates the
inventory from unchanged logs. No failed/unrun check is called a pass.

The combined viewer also contains [PPD125 readiness repair](ppd125-startup-warning-cause-20261118.md),
[PPD241 current terrain height/preflight](terrain-shadow-startup-cause-69258-20261118.md)
and [PPD399 wearable diagnostic](appearance-warning-investigation-69258-20261118.md).
PPD399 exact3files match328fe89d, isolated117/5+883/18 and combined883/18 pass;
no avatar propagation/network/FPS claim. Terrain dirty/current publication and
probe preflight preserve completeness/admission guards; historical exact
startup operands remain unknown. All defects remain owner-pending.

Build03/04/05 warning audit retains20/5/5events:3Catch2 holder,1existing unused
sky constant and26duplicate static-archive link rows;0errors. Catch2 PPD398
attribution remains open; no suppression or legal-compliance completion claim.
Repeated archive paths/source owners and every full diagnostic remain retained.
No new viewer run/logs were produced by these native checks. Existing full
69258warning/replay history remains authoritative for that run.

## Evidence and next checkpoint

Private root:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/accepted-probe-lighting-20261118`.
Relevant directories: `raster-tile-api-native-01`, `world-raster-parity-01/02/03`,
`world-raster-cost-cell{0,1,2,3,6,7,8,9,10}-01`,
`tile-geometry-api-01/authored-world-demand-census-01`,
`combined-build-warning-review-02`, `ppd399-integration-01`,
`ppd393-conversion-reference-01`. Readiness69259control is independently
staged and preserved. No captures/private logs were uploaded or committed.

FinalReleasebuild06 passes0 in36.419s at builder25faff429e27aae824321e686fc75cda8bead272,
canonical implementatione94d9353622398ea96f472bca64183bd1e7ad9bb, indra
cf835598516daa65ffd47d66473cf449281c8d6a. Actual final world/main suite passes
1255075assertions/11cases, including compound BASE+sky+point/projector exact
parity and successful public reload preserving all15populated census fields.
Empty-forward reload setup proves counters only; separate alpha fixtures
prove fragment contributions. Focused llrender API/readiness/terrain passes
698443/32, terrain layout853/13 and full appearance883/18; API/GPU validation
banners are retained where native GPU work executes. Final current-source
strict gate passes3617runtime files/915informational/0blockers/0errors; doctor passes on M1Max. Separate combined69264 staging passes0:8273exact nodes including app root,
346ARM64/0invalid slices, deep strict signature and3owned direct-import checks.
Source/staged bytes/modes/symlinks and embedded25faff source identity match;
metallib remainsfae7. Viewer executable SHA256
46fb9270a18f79ec30868b34402709c6f576cd0849220e9406efe8581bf8ca63.
Exact app path:
`/Users/dekstroza/Programming/AI/Secondlife/Alchemy/artifacts/metal/accepted-probe-lighting-20261118/final-combined-checkpoint-69264/candidate-26.4.0.69264/Alchemy Metal Alpha G2G5.app`.
No viewer launch/capture or push. Engineering pauses here for the owner
Home/club checkpoint as explicitly requested. Compare
automatic and explicit tile16 with the same binary and unchanged graphics,
recording Home login→settlement and club teleport/loading→settlement with
replayable all-queue GPU frames, CPU/native completion/workload telemetry,
complete warning/error logs and normal-rendering controls. Qualify actual
coverage and capture pauses separately.

The shared shadow-publication gap was implemented and warm reuse observed in
69258, but churn/denial costs (PPD382), earlier streaming/upload feedback,
small-workload native visibility/commands and whole pass/resource liveness
remain partial. See [69258 exact acceptance](shared-shadow-main-admission-checkpoint-69258-20261118.md).
Further architecture must be justified by these boundaries' demonstrated cost.


Checkpoint qualification lives in `final-combined-checkpoint-69264/`:
`stage-verification/receipt.json` contains every command/path/node identity;
`owner-checkpoint-plan.json` specifies expected per-owner policy/capture scope.
It is not an executed or newly qualified capture-controller specification.
Before the owner run, qualify fresh exact app/control identity without changing
graphics, and verify actual retained coverage afterwards. Existing69259
readiness and previous69258capture controls remain untouched. This stage is
ready for owner evaluation, not release or performance acceptance.


Final incremental build06 additionally emits5known warnings/0errors; combined
03–06 total35 (4Catch2,1unused sky constant,30duplicate-link rows), with full
source/site/trigger mapping in combined-build-warning-review-02/build06-addendum.
Independent reduction checks all5400retained CSV rows against reported paired
medians/adverse counts. PPD407retains an initial documentation line-cap/EOF
guard failure; whitespace-only repair passes separately captured harness33 and
diff-check0. No source/app/performance evidence changed. The exact recorded
awake guard55971 is stopped at the owner checkpoint; no other process is stopped.
