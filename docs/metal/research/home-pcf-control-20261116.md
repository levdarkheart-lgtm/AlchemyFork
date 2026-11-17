# Home High2 PCF representation control

PPD-319/327 remain open. Runtime source819f98d71c/build69224 is unchanged.
This is a private shader hypothesis, not an accepted engine or FPS change.

Private evidence: `artifacts/metal/quiet-home-pcf-20261116T221358Z`.
The baseline sourceSHA256 is
`f05d83a1b4a3f96159d7a1750c035afab67db423f0d239c1cd496c1e1143f33d`;
candidateSHA256 is
`338a071f9c9066bfad1842545de89409fac47fea68c28d32edfce95d67c4b55e`.
Both use the same Metal3.1/fast-math/target26.0 compiler flags.

## Behavior and scope

Original1eb44's High-quality Gaussian filter fixes kernel6 before GLSL
compilation. Current High2 similarly specializes quality2 before the final PSO.
Retained intermediate AIR has dynamically indexed local weight arrays, but
their survival in final High2 GPU code and contribution to304B are unproved.

The prototype replaces only quality2 PCF with12named weights and9explicit
2×2blocks, preserving the original by/bx accumulation order. All36depth
reads/comparisons, Gaussian weights, receiver-plane bias, UV clamping,
normalization and float precision remain. Generic quality0/1/3, PCSS and the
probe single-tap policy are unchanged. This does not reduce lighting quality.

## Contained observations

The existing production-fragment parity and Home gates pass7584assertions in
2cases.525image comparisons report maximum absolute error0.0000610351562,
zero failed/nonfinite components under the existing criteria. These are
numerically qualified images, not bit-identical output. Existing gates do not
cover every receiver slope, edge, reverse-Z or probe-specialization case.

The actual shadowed-alpha fixture uses20probes,7blended layers,1024×576,
10alternating warmups and30alternating measured pairs per family. Both arm
orders are retained; signs below are candidate versus currentHigh2.

| Family | Normal-order paired median | Reversed-order paired median |
| --- | ---: | ---: |
| Automatic |−3.22% |−1.39% |
| Mixed |−4.87% |−3.00% |

All240observations remain preserved, including adverse pairs. This is modest
fixture evidence that justifies broader qualification, not actual-scene GPU
timing, register/spill reduction, occupancy, live FPS or a solved Home decline.
Native parity extensions and strict fused/separate RG8 qualification are
independent private tests; no criteria are relaxed.

The private clone of the source819 test bundle passes454nonbenchmark default
cases/2375783assertions with API/GPU validation, including strict31×31
reverse-Z/HDR/quality2 fused-versus-separate RG8 image equality. Six archive
cases/5905assertions pass API-only under their required policy; selective
archive98assertions/1case passes. Baseline strict tile1526assertions/3cases
also passes. Three actual benchmark/cost cases were excluded. An initial
candidate-only strict run was deliberately interrupted to correct the planned
broad selector before a large benchmark started; its SIGTERM failure remains
retained and is not a shader failure or successful execution. No benchmark
started in this qualification. One entrypoint-inventory case explicitly loads
the pinned build-path library rather than the private candidate; renderer
fixtures use the verified candidate bundle resource. Exact receipts and this
coverage limit are in `full-renderer-parity/`.

Independent pre-run review found the proposed extended fixture's inherited
water plane would discard deep receivers, while positive clear color defeated
its nonzero-image counter (PPD-335). Those unexecuted cases are not credited.
Retain the initial source; correct only synthetic setup and require actual
non-clear coverage per case before trusting the added boundary checks.

Corrected extended gate exits0:39703assertions/1Catchcase,1050A/Bimage cases,
Q0/Q1/Q3=210each,Q2=420,525perZdirection. All output half bits match exactly
(maxerror0, zero failed/nonfinite/unequal components); each arm draws2560/2560
pixels beyond its measured zero-draw half-format clear background. Ten ordinary
shadow contrast controls differ,350runtime-probe/bool0true pairs match and210
generic-Q2/High2 pairs match. Raw21504000byte outputs both hash
`63715ebf3c745401b8efa2da140c14cd249c8a7b4e62920fdeb570354a0fad56`.
Initial unexecuted sources and the PPD335 setup repair are retained; production
water discard and existing pixel thresholds were not changed. Synthetic finite
inputs are still not whole-scene visual acceptance.

A fresh disposable late-Home capture copy was made from the original owner
input for any further saved-scene replay. It has15698files; copy exit0.
The older Xcode-crash working copy remains retained separately (PPD-334).
No viewer launch, new recording, graphics-setting change or push occurred.

## Actual saved-scene check: unselected

The fresh private copy replays the owner's old69200lateHome capture on
M1Max/macOS26.5.2/Xcode26.6. Medium/Overlapping was verified in the UI.
All20command buffers/41render encoders/5104draws/10dispatches remain present.
Shared shadow quality2 is held in both arms by the same source intervention
used for the earlier High2 control. This is recompiled saved work, not the
installed69224binary, a new owner recording or isolated shader execution time.

| Arm | GPU workload | Simple-shadow registers | Spilled bytes |
| --- | ---: | ---: | ---: |
| A1, current High2 helper |121.27ms |not reread |not reread |
| B, scalar/unrolled PCF |116.97ms |222 |304 |
| A2, original helper restored |116.32ms |224 |304 |

Candidate is3.55% shorter than A1 but0.56% longer than A2. The4.95ms
control drift exceeds this small candidate difference. Untouched-session
153.39ms is retained separately and is not a matched recompiled control.
Candidate/restored simple-shadow cost shares23.13/23.53% are replay shares,
not absolute per-draw GPU time. Occupancy/per-line spill traffic was unavailable.

**Do not select this prototype:** correctness is qualified and fixture timing
is modestly favorable, but this scene check does not establish a reliable
whole-workload gain. The measured spill remains304B. Two fewer registers do
not prove an occupancy change, spill elimination or a Home FPS repair.
This is an inconclusive scene result, not a demonstrated large regression.

Two source undos removed the helper and guard; visible original PCF was
verified. The final undo restored the captured runtime-quality expression.
GPU work stopped, Don't Save was selected, and Xcode returned to Replay /
Finished running. A reload-call timeout was followed by observed background
profiling and the completed result; no duplicate reload was issued. Engine
source, original captures and the69224viewer remain unchanged. Exact controls,
statistics, UI limits and restoration are retained in `recording-replay.json`.

## Owner's raster advice: verified limits

Xcode's retained304B is an observed compiler statistic for the recompiled
simple-shadow fragment, not an inference from low occupancy. It does not
establish304B external-memory traffic per fragment or identify SH, PCF arrays
or a source aggregate as the cause. Apple documents dynamic local-array
spilling and recommends Pipeline Statistics/compiler remarks; actual remarks
and candidate counters remain needed for attribution.
[Apple shader guidance](https://developer.apple.com/videos/play/wwdc2020/10632/?time=2404).

M1 Max supports legacy per-line shader metrics, counters and compiler remarks.
Newer shader-cost graphs/heat maps require M3/A17 Pro or later. Current Apple
documentation describes `gpudebug` saved-trace profiling, but that executable
is absent in the installed toolchain; the available route remains Xcode's
saved-capture replay. Eligibility does not prove collection succeeded here.
[Inspecting shaders](https://developer.apple.com/documentation/xcode/inspecting-shaders),
[shader-cost graph availability](https://developer.apple.com/documentation/xcode/analyzing-apple-gpu-performance-using-shader-cost-graph-a17-m3),
[interactive command-line tools](https://developer.apple.com/documentation/xcode/debugging-with-interactive-command-line-tools).

Do not add `early_fragment_tests` globally: the shared entry also serves
depth-writing rigged/pre-water/avatar consumers and required water/alpha
discards. Unrigged post-water alpha with verified read-only depth/stencil is
a narrower candidate. No actual missing automatic early rejection or benefit
is established. Visible translucent layers still require blending.
Exact source predicates/reference mapping and public documentation are in
private `analysis-source/early-fragment-test-audit.md` and
`raster-apple-advice-verification.md` under the69224Home checkpoint.

## Remaining attribution

This bounded control is complete and unselected. Actual compiler remarks and
per-line/runtime memory cost remain the higher-value attribution evidence for
304B. The full observed UI banner says missing debug info **or** deployment
target below device OS; it does not identify which condition caused unavailable
per-line data. No successful retargeting remedy was established. Keep the
product deployment minimum unchanged. Further source changes need their own
quality and cost evidence; neither smaller source nor lower register counts
close PPD319/327 or resume broad architecture acceptance.
