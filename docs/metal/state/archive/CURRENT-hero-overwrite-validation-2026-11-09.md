# Shiny full-hero overwrite optimization —2026-11-09

Physical M1Max/64GiB/macOS26.5.2/Xcode26.6. Active20FPS goal; no quality,
view count, cadence, culling or resolution changes. Sourced1288129991e72b425c7dd60b2932f9fb1214f7b committed; live acceptance pending.

## Source contract and narrow implementation

Exact original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45:
class3/deferred/fullbrightShinyF.glsl requests the legacy environment, applies
applyLegacyEnv and clamps final color. reflectionProbeF.glsl legacyProbeFinish
applies tapHeroProbe to legacyenv with glossiness1; tapHeroProbe computes the
box/sphere/clip weight, then mixes ordinary radiance with hero radiance.
The native shiny fragment previously always completed the ordinary probe walk
before that final blend, even when weight was exactly1.

Candidate computes the identical weight first. At exactly1 it samples hero
radiance directly; otherwise it performs the original ordinary walk and blend.
Legacy sky remains separate. Three shiny-only helpers preserve shared PBR,
water/alpha finish and ordinary selection/tap bodies byte-for-byte. No epsilon,
probe filtering, cap/default/neighbor ordering change, or CPU admission heuristic.
Source review independently approved the measured private-prototype shape.

Under production fastmath/no-signed-zeros, a runtime mix(x,y,1) test of258112
finite pairs found one bit difference: +0 versus -0 (no numeric differences).
This is explicitly retained in the private strict initial-failure log; final
shiny max canonicalizes zero. It is not a proof over all possible float inputs.
15 private production-shiny images including signedzero/negativeHDR matched
bitwise;882 finish pairs also matched. No arithmetic tolerance was introduced.

## Performance mechanism, not viewer FPS

Private prototype /private/tmp/alchemy-metal-hero-overwrite-20261109.b3NS61
contains exact source baseline SHAe9345c0dd8f4a1b02624a2f5194144006f40e61ac9b37561d918021d9dcc8d56,
manifest, compiled variants and raw counterbalanced samples. All processes exited;
thermal state0.3456x2168 fullscreen synthetic full-hero cases skip almost all
ordinary search/sampling. Mixed cases benefit in proportion to full-weight pixels.
Disabled/rejected cases show small overhead (about2–3% in the sampled shader),
so no blanket improvement claim or single-scene quality workaround is justified.
These synthetic fullscreen figures are not projected live draw times or FPS.

## Regression and validation

New tests/almetal_hero_overwrite_test.mm loads actual production shiny vertex
and fragment from the normal built-in metallib. Its reference appends frozen
pre-change shiny+hero-finish functions to current source; the frozen fragment
never calls new hero logic. The unchanged ordinary traversal is shared, so this
test does not independently re-prove ordinary selection correctness.

24 cases compare raw output bytes across RGBA32F/RGBA16F/RGBA8/RG11B10F.
Cases include full/mixed/pure-partial/off/clipped hero, old-owner packs, negative
ordinaryHDR, heroHDR, zero diffuse with intensity0/1 and signedzero/negativehero,
nonuniform cube faces/texels/mips and a rotated environment matrix. A frozen GPU
weight census asserts full/mixed/pure-partial/zero populations. Alpha+finite
checks reject equal clear-only output; no numeric tolerance is used.

Required doctor initially hit sandbox sysctl denial; host retry exits0 with
M1Max/Metal capability. Shader compile and existing full Release API+GPU suite
exit0 (121227assertions/86cases). Initial focused fixture268727/1 passes.
Final Release focused268745/1 and fullRelWithDebInfo389972/87 pass with API+GPU
validation. Independent frozen-fixture review approved; full coverage assertions
added after review. Tests ran serial GPU windows; all processes exited0.
Logs /private/tmp/alchemy-metal-hero-overwrite-*20261109.log.

## Next acceptance boundary

Finish both configurations, build/verify/sign an exact committed normal viewer
with source recordsOFF and capture/API/shader-validation environmentunset.
Repeat fixed-location fullscreen Ultra/Midday/Preset123,3456x2168,120frames+
300seconds warmup and300consecutive exact-native frames. Compare corrected
68559 baseline8.121FPS, not stale-face68558. Preserve user environment state.
20FPS and original-vs-candidate visual/functional/release gates remain open.
