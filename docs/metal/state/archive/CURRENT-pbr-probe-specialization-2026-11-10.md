# PBR alpha probe-shadow specialization — native gates passed

Observed2026-11-10 on physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
Code checkpoint765c305b5ac4c7b897d6581ea2990de49d1e09f6. This admits a normal
viewer build and controlled trial; no new viewer FPS improvement is established.

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
No build/GPU/profiler/viewer process remains from these gates. Complete commands,
logs, source, failures and raw samples are preserved locally under ignored
artifacts/metal/private-pbr-alpha-fc0-20261110/; no private upload.

## Next gate and product scope

Build normal source-exact app with doctor/configure/build/verify/signature checks,
independently verify actual flags/identity, then repeat fullscreen/Midday/Preset123
Ultra3456×2168 at the SAME location. Compare with preserved68610 water baseline.
Native strict/micro cost admits that trial, not a claimed FPS gain or20FPS success.
Full pixel/feature parity, multi-scene performance, prolonged stability, strict
no-GL and independent fresh-build release review remain open. Goal20FPS ACTIVE.
