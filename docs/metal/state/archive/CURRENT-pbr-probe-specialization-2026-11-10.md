# PBR alpha probe-shadow specialization — isolated gain, live FPS flat

Observed2026-11-10 on physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
Code checkpoint765c305b5ac4c7b897d6581ea2990de49d1e09f6. Normal68614 and its controlled live trial completed; no new viewer FPS improvement
is established.

## Original behavior and implementation

Exact original1eb44bb class2/deferred/pbralphaF.glsl143–146/237–239 computes
receiver derivatives before discards and calls sampleDirectionalShadowGrad.
Class1/deferred/shadowUtil.glsl503 returns single-tap for cube_snapshot. The
native shader already makes that return for actual pack bit4. SIMPLE and water
PSOs specialize it, while both standard/custom PBR color-alpha factories omitted
the existing optional function constant0. Existing stages/census favored PBR
alpha over the very short probe light-map pass; these were not exclusive PBR costs.

Native scene.probe_pbr_shadow_specialization defaultsfalse for direct callers;
the generic pipeline producer now enables it. Backend rechecks shadowed PBR
color, non-avatar/non-depth-only, validated probe count>1 and bit4 in the actual
bound directional pack. Main/default/no-shadow/protected depth paths stay on
original PSOs. Standard HDR/coverage/rigid-skinned clones are lazy, at most8 per
renderer, copy every descriptor value and restore the owned library pointer.
Custom PBR uses the existing shadow-mode cache key; unqualified keys are unchanged.
No shader, binding/resource ABI, light/probe selection, math, clipping, sampler,
texture view, cascade, cadence or quality change was made.

Timing-opt-in actual-descriptor census has13 independent view tuples and12
rigid/skinned × standard/custom × undefined/false/true bins. Failed/unarmed
renders preserve previous successful observations. The producer logs actual true
bins once per possible probe view during tracing, with matching frame/view;
ordinary unarmed play never reads/logs the snapshot. This proves encoded
selection, not GPU completion or fragment counts. Core and producer were
independently source-reviewed. Native factory failure stays a hard render failure.

## Actual correctness and cost

Integration /private/tmp/alchemy-metal-pbr-probe-fc0-integration-20261110.4savz8k7.
Core patchb809d9e0; initial fixture14cb1b33. Both arms use original source080fba9e
and exactly the SAME offline normal metallib3932018186338255b0523028441e5376393a51c2b3ddbad2ad538f1c530376c2.
All run/build records pin source and diff hashes; fresh test.app bundles contain
only the compiled library, no MSL fallback source. Environment is scrubbed.

Initial test build1 was Catch's required parentheses around an unchanged OR
predicate. Initial validated preflight42 stopped on an invalid fixture cutoff1,
not an output mismatch. An intermediate.99 correction compiled but NEVER ran on
GPU; source inspection showed the owner admits fixed original cutoffs, not a
range. Final edge uses legal.004 plus the existing zero-alpha textures, accepted
pre-water geometry with depth writes, and checks exact background/far-D32 bits.
Both failed attempts and the unused intermediate source remain preserved.

Final measured fixture9a4dd85e passed actual API+GPU25379assertions/1case:
68 strict pairs plus six1024-square preflights. Mixed1/2/8 slots, rigid/skinned,
standard/custom blends, coverage, main→probe→main PCSS contrast, default/no-shadow,
water/mirror/cascade edges, all-source attachments, rawRGBA16/D32/S8 and selected
PSO receipts are checked. Source stencil3 is unchanged; post output stencil0 is
its existing contract. Unarmed/invalid-scene census retention is also checked.

Separate quiet run28122/1 passed with168 rows, six cases, seven counterbalanced
ABBA/BAAB blocks and fixed R2. No competing viewer/profiler/test was running;
validation/HUD/capture/stage flags were unset, thermals nominal. GPU whole-CB
elapsed includes complete renderer work. CPU preparation, commit, wait, callbacks
and diagnostic drain are separate; this is not a whole-viewer FPS measurement.

| Dense probe case | Runtime GPU ms/invocation | Specialized GPU ms/invocation | Mean change | Seven paired blocks |
| --- | ---: | ---: | ---: | --- |
| Rigid |5.519929|4.771719|−13.555%|all faster,−15.383% to−11.352%|
| Skinned |5.595585|4.602552|−17.747%|all faster,−23.142% to−11.438%|

SAME-runtime mean−0.795%, SAME-specialized+0.774%; individual control blocks
range−5.228% to+5.856%. Main alias mean+1.151% and count1 alias−2.490% are
controls, not code improvements. All28 out-of8–20ms rows are short count1 cases
and are retained. Eligible/SAME dense commands were within8–20ms. No consistent
CPU gain is proved. Logical256 probes share one physical cube/SH row; physical
target identity and generic owner retirement are not fully controlled.
Independent audit actual0 SHAda5b8bb6f76db2c8637587627c12e24593e45df59c2a8500389f0422c9bbde8c
and findings0f27e9d287a655b423b7928703a12d0b9989e776976ea76b769c40455bbb3965
recommend full suites/viewer trial. Quiet executable9c257e7dfbf68fc397597acd97f25632abe843837a8ae3e7e16a123de425c74c.

Supplementecbb29ab adds16 rawRGBA8 forward/reverse/rigid/skinned/blend/coverage
pairs and4 protected depth-only pairs with valid preceding offscreen fullbright
phase, zero PBR-color bins, unchanged color and actual depth writes. Actual
API+GPU expanded preflight passed27593/1 (88 pairs plus six large preflights).

## Final source and optimized regression gate

The private benchmark31a41fa5 and seven private imports were removed; one comment
was renamed. Root and independent review reconstructed the entire remaining
strict file byte-for-byte: permanent fixture7ccb823c7313e0a64ed5d2d3fba6254f953fc0295442306957f39699c35aa2e3.
Renderer/shader code is identical to the measured mechanism; the producer change
has independent source approval, pipeline SHAe4786d49e11b75e92bd0e74820cc1201ee51a1d66e3cc4c8a00a34df33b1fe2b.

Admission /private/tmp/alchemy-metal-pbr-fc0-admission-20261110.ln3y_bxk/run.py:
build-release/full-release/build-rwdi/full-rwdi EACH actual0. Release and
RelWithDebInfo EACH passed494211 assertions/104cases with both actual validation
preambles, using [almetal]~[frame-capture-gpu]. All15 recorded final source hashes
were rechecked against current files before the coherent code commit765c305.
Complete commands, logs, source, failures and raw samples are preserved locally
under ignored artifacts/metal/private-pbr-alpha-fc0-20261110/; the later normal
build, independent identity and admission supplements were also copied there.
No private upload.

## Source-exact normal viewer build

Normal68614/source271eab19d538a3ee2c8ed0fe93d792247a35ab76 completed actual
doctor/configure/build/verify/deep-strict signature ALL0,349ARM64/0bad.
Canonical build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app;
exe ddff27521a7a83d7279654e64d266cc2a3fd3e6113e1c45c54287828f459d2e8,
unchanged normal metallib39320181. Actual normal commands/exits are in
/private/tmp/alchemy-metal-pbr-fc0-normal-build-20261110.cxu7qwy0, copied to the
private evidence directory. Independent identity5722b400 verifies generated and
embedded source,8actual O3/NDEBUG/C++23/ARM64 compile recipes,3referenced response
files, MSL3.1/fast-math/source-recording OFF and signature0. One initial identity
script path error was corrected; the completed audit0 is retained.
Preserved /private/tmp/alchemy-metal-68614-pbr-specialization-preserved.app;
copy/executable hash/deep-strict signature0.

## Controlled live comparison — no demonstrated gain

Both runs used private-ultra-pbr-fc0-qa-20261110.json SHA
2da3db3c72c766d5901f09c1324f63515d30a5a798ce7e4ea676105f1b57d777,
fullscreen BEFORE login, Midday/Preset123, Ultra3456x2168,300s warmup and120
ready frames before300formal consecutive samples. No profiler, GPU test, build,
family/stage/workload capture or validation competed with formal collection.
Translator remained stopped and Xcode replay stopped. Both start/end fresh
observe/verify/gate operations passed0 and43active controls matched.

| Normal build | Source | Formal frames | FPS | Mean ms | p50 ms | Nearest p95 ms |
| --- | --- | --- | ---: | ---: | ---: | ---: |
|68610 water control|8eea2c6895c1c907ce513a5c83872a631874a3bc|4993–5292|9.581135|104.371773|104.685521|115.578083|
|68614 PBR candidate|271eab19d538a3ee2c8ed0fe93d792247a35ab76|5016–5315|9.566138|104.535388|104.361208|114.900709|

Candidate mean FPS−0.1565%; p50−0.310% and p95−0.586%. This is effectively flat,
not a live performance improvement or proof of regression. Main/end camera and
EEP/sun are exactly equal; all300samples native requested/attempted/exact1 and
fallback0, bias1,256ready/stable120, cameraChangeDelta0 and restoration flags true.
Mean draws50808.9733→51014.41 (+0.4043%); main-visible probes98→97. These small
workload differences and sequential-run drift limit causal attribution. The fresh
68610 control was itself2.087% below the earlier9.7853 water run. Completion-window
GPU sums overlap and are not additive GPU frame time.

Actual11bounded encoded PBR-specialized probe receipts confirm selection, not
GPU completion or fragment coverage. They are startupframes232–303/count2or5,
hero faces0–4 and all6ordinaryfaces; the once/view logger does not measure
settled formal-window population. An independent parser initially used the wrong
log label; corrected exact-source label finds all11 and preserves old reports. Sparse census does not establish the
population of every expensive hero face. Actual window screenshots were observed
only after fresh gates; the same patio/forest/avatar/chair/mirror arrangement and
10FPS HUD were visible. This qualitative check is not full pixel parity. No
standalone screenshot file was returned. One baseline CUA screenshot timed out at
30s; retry with60s succeeded. All CUA phases received completion summaries.

Raw runs: artifacts/metal/qa-runs/reference-pbr-fc0-20261110-01 and
candidate-pbr-fc0-20261110-01. CSVs:
artifacts/metal/private-68610-pbr-control-before-20261110-01.csv and
private-68614-pbr-fc0-20261110-01.csv plus sibling provenance. Fresh start/end
observed/verified JSON and actual exit records are preserved in each run.
Official perf_compare.py --enforce --require-provenance completed exit1:
provenance passes, no p95 regression, but p50 target and three-scene requirement
are unmet. Output artifacts/metal/private-pbr-fc0-live-comparison-20261110.json.

Baseline exactPID18036 and candidate exactPID18623 each received SIGTERM only
after executable verification, followed by actual PID absence and Goodbye proof.
Their actual OS exit codes were not observed. Exit operation receipts are0.
Each OWN environment plan was restored0 AFTER exit; no plan remains active.
Never restore these completed plans again. No viewer/profiler/build/GPUtest is live.

Independent completed audit actual0: JSON120742d331b3931c443fbb1e6554fb97f19c4b18773a25a5bcdeac2c73f30aa4,
findings e9735b0434a0c770cbd21d4d64db9c61af967e2f4d6e2d27ab51f2fcf38d347e,
parser39ef5dbc; private copy retained. Parser also adapted to the actual candidate
end-exit envelope while requiring exactly allthree zero exits; no tolerance
relaxation or native gate failure. Audit confirms flat result.

## Decision and product scope

Keep the bounded, strict-equivalent specialization for its repeated affected-GPU
benefit; do not report it as a viewer FPS gain. New code remains local and is not
in pushed stable7772/water. Further work needs a substantial measured mechanism;
an isolated shader improvement alone has not solved this workload.
Full pixel/feature parity, multi-scene performance, prolonged stability, strict
no-GL and independent fresh-build release review remain open. Goal20FPS ACTIVE,
unmet and not proved infeasible.
