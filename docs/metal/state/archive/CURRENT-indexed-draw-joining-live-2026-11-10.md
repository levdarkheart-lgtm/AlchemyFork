# Indexed draw joining: completed native cost and live rejection

Observed on the physical Apple M1 Max/64 GiB/macOS 26.5.2/Xcode 26.6.
Branch `codex/apple-silicon-metal`; normal trial source
`88528d27a5d0836faddfcbcb0b715fe0c6280ebe`, build 26.4.0.68619.
Goal 20 FPS Ultra remains ACTIVE and unmet; infeasibility is not established.

## Outcome

Do not promote draw joining to default-on. The controlled live observation was
slower despite lower encoded draw counts. The nonpersistent Boolean control
`RenderMetalJoinStaticDraws` remains registered default false. Existing binding
caches and earlier correctness/performance repairs remain retained.

| Same verified normal executable | Frames | FPS (1000/mean) | Mean ms | p50 ms | p95 ms | Mean draw commands |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| OFF02 | 5003–5302 | 8.639418740 | 115.748527780 | 115.750562 | 126.404291 | 50038.836667 |
| ON03 | 8887–9186 | 5.513777899 | 181.363852223 | 182.358896 | 194.354416 | 45954.666667 |

Observed FPS −36.178833%, mean frame time +56.687826%, draws −8.162000%.
This is not a precise code-only regression estimate: the source populations
changed materially despite the fixed camera, environment, quality and location.
Main G-buffer sources averaged 4990→4616; hero2 1409→1173, hero4 277→134;
ordinary faces represented substantially different probe owners. No live benefit
was demonstrated, which is enough to reject default-on admission.

Both modes have exactly 300 consecutive native-exact/fallback-zero frames,
Release/Ultra6/3456×2168, discard bias 1, 256 ready probes stable for 120 frames,
and main-visible probe count 98. Main/end camera, EEP/sun and private start/end
position compare exactly; camera-change delta is zero and restoration checks pass.
All 44 raw live controls and saved start/end observe/verify/gate operations passed
with actual exit 0. Only the requested joining control differs.

There are 26 formal-only command receipts in each run: two for each of 13 views.
ON hero1 admits 3511–3512 source batches and emits 2998 physical commands;
hero0 1890→1590 and hero3 1971→1606 also establish actual joining.
Main remains query-gated: ON 634–635 visibility queries versus OFF 658–659.
Counts are CPU-emitted native commands, not GPU completion, fragments or time.
Do not subtract auxiliary queries blindly or sum sparse per-view means.

## Source, build and native correctness

Core c2804f88 adds default-false encoder-local adjacent indexed joining. Complete
single-instance triangles may join only for the same immutable native IBO/type,
exact contiguous offsets and checked count. All native mutations flush pending
work; native interop/cache invalidation also disables joining. World eligibility
excludes queried, wireframe, stamped, skinned and special draw families. Primitive
order, buffers, shader math, shadows and caster behavior are unchanged. Exact
original 1eb ordering and all required guards were source-reviewed independently.

Focused actual API+GPU gates: wrapper 7969 assertions/1 case (58 invocations),
production 2120/1 (67 strict pairs and two special-family positive controls), exit 0.
Both complete optimized Release and RelWithDebInfo suites passed actual API+GPU
validation: 504300 assertions/106 cases EACH, exit 0. Private benchmark insertion
was stripped byte-exactly back to strict production test ee089f4b before the trial.

Source885 adds only the nonpersistent default-off producer control and bounded
physical command receipts. Source review approved before the normal build.
Normal doctor/configure/build/verify/deep-strict signature all exited 0, 349 ARM64
images/zero bad. Independent identity a1657b93 checked source stamp, plist,
executable, actual optimized ARM64 recipes, settings and offline shader flags.
Executable SHA256 ff3dec85f76f5f99b3b68738d97be68a52005568256cd3a709ed39c6af0055e9;
metallib3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2.
Canonical app is build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app.
OFF uses byte-identical /private/tmp/alchemy-metal-68619-join-off-control.app
because the QA helper requires distinct executable paths. Neither is the Alpha3
release; that remains stable source7772.

## Complete private cost: limited trial admission, no FPS promise

Private patch813fa308/test4024883b was measured at source HEAD58bc12fa and frozen
before stripping. Build5473/validation17740/R2quiet96034/R32quiet53142 all terminal0.
Validation2341/1 had actual API+GPU preambles, 67 small and six large strict pairs.
Each quiet run4538/1 contains six preflights and 168 samples, nominal thermal state,
no validation preambles. R32 eligible CPU means save only .0177/.0177/.0837 ms per
simple/PBR/mixed invocation; all seven paired CPU blocks are faster. No-match
mean +.0163 ms is not zero-overhead proof. GPU directions are inconsistent and
reverse R2 for simple/PBR; dense mixed stays favorable but SAME controls vary.
All out-of-band rows retained, no linear per-view scaling or FPS inference.
Independent source/compiler/statistical audits approved only a controlled trial.

Preserved ignored packet artifacts/metal/private-indexed-draw-joining-cost-20261110:
197 indexed files, all SHA-verified, including raw runs/apps/source freeze,
compiler identity and analyses. COST-SYNOPSIS.md SHA13466b56; INDEX.md21badcc3;
SHA256SUMSac7ef43d. Original private packet paths remain in that index.

## Runtime protocol and rejected setup attempts

Accepted OFF02 configv2 SHA38b0c5ceff5501a96ad19b8c6f239bc4dcd3f4b7cd8ea44297fd33cc9ea954fe;
ON03 configv3 SHA419dc37ff2864f0bd38059697ae75471423a2507362d77b487addf90b4b6f660.
Both launch/dry-run operations actually exited0. Fullscreen before saved login,
Midday shortcut and observed Preset123; camera panel closed before formal sampling.
Only standard exact/provenance/ready collector enabled: 300s warmup, 120 consecutive
ready frames, then300. No profiler, build or GPU test during formal collection.
ON CUA reads timed out twice at60s, recovered at65–66s; these setup events occurred
well before formal sampling. Actual gated end images were observed: expected scene,
OFF top9FPS and ON top5FPS, qualitative only. Every CUA phase was completed.

OFF01 failed only expected false versus LEAP integer0; ON02 failed expected
integer1 versus LEAP true. The helper obtains Boolean declarations only from
session XML, where role-only controls are absent. Its strict comparator was not
relaxed. New immutable configs use OFF0 and ONtrue; accepted raw live values match.
Failed runs and their aborted exits remain preserved, excluded from the comparison.

Exact PIDs61823/64019 (rejected) and62932/64552 (accepted) were SIGTERM-stopped after
exact executable checks; Goodbye and PID absence verified, operation exit0.
OS exit codes are unknown/null. Each OWN environment plan was restored only after
its exact viewer exit; actual restore0. No live viewer or active trial plan remains.
Translator stays fully stopped; Xcode replay stopped. No location change or quality cut.

## Analysis and limitations

Official tools/metal/perf_compare.py --require-provenance --enforce actual exit1:
provenance verified, performance/scenario target not met. Raw output/exit saved at
artifacts/metal/private-68619-join-off-on-official-20261110-01.
Independent approved parser88a7cfe9 actual exit0 in .103s; completed audit
/private/tmp/alchemy-metal-indexed-join-live-audit-20261110/completed-off02-on03-audit.json
SHAe559cdb32f6df4a876750347fa4dc0cfe1cde2a33a8b6cc24798fa4ef57e178f;
findings79bc1eb7. Raw CSV/provenance/QA inputs and hashes retained locally.

Foreground statistics bracket both runs and advance ~300s per300s wall time;
this opposes a sustained40ms background sleep explanation. Source/settings/argv
pacing audit0a0f6476 also found no active limiter; MaxFPS is obsolete/unread.
No per-frame OS-focus or actual sleep trace is claimed. Read-only powermetrics
attempt during ON warmup exited1: sudo password required, no prompt issued.
Current clocks/thermal throttling remain unknown. Completed GPU duration sums
can overlap and lag; they are not per-frame busy time or a hardware ceiling.

The separate shadow single-map-walk patchd4b12898 is source-approved but private,
unapplied and unmeasured. Hero shadow transactions already batch per face;
ordinary-probe total sampled CPU5–7% is not removable batching overhead. No new
high-leverage shadow implementation is justified by the saved inclusive scopes.
Historical current64 probe-selection replay is being reviewed separately; it
cannot establish current alpha-fragment cost or admit the old31-neighbor prototype.
Full parity, multi-scene performance, stability and strict no-GL release gates remain open.

## Historical current64 population follow-up

Root read the complete peer-reviewed walker and final findings after both live
runs ended. Sourcebe3e2c6c was approved for a historical CPU model, not GPU
floating-point equivalence. Both all13-view walks actually exited0 in2.118/1.991s;
33,280 grid samples each, existing V2 GPU-stored positions. Saved source identity
is externally supplied, not embedded. This is a counterfactual current64 selector
on data captured before the64 repair and newer specializations.

Main ideal initial-rejection removal accounts for4.33% of modeled containment
checks in both packets; hero19.35/21.47%; ordinary23.85/78.04% varies with captured
owners. Hero neighbor examinations remain51–53/sample on average. Three hero grids
have no base geometry and hero5 onlyeight samples. These are neither GPU savings
nor affected alpha/water cost; heuristic float-boundary flags do not certify
unflagged GPU equivalence. No accelerator admitted. Next useful measurement is a
fresh current post-formal workload packet with the existing sampler, before any
shader rewrite. Source-only current-profile plan assigned separately; not executed.
Findings42412e4f, summaryb6c8cde6, manifest68451c95 in
/private/tmp/alchemy-metal-historical-current64-replay-20261110.

A local ignored live evidence copy is complete:
artifacts/metal/private-indexed-draw-joining-live-20261110,114 copied files checked
source/copy SHA-equal,116 indexed files/6,454,805bytes. COPY-MANIFEST.json
SHAd8aa396dad4288f7c68c5d3ac08f8439c31944067b02b5068000c3731baa13c5.
Contains accepted/rejected raw QA/CSV/provenance, configs/launchers, official and
independent results, historical source/results/dependencies and clock-check failure.
Historical binary inputs remain in their original ignored paths. No private upload.
