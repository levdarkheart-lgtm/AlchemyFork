# Lighting rejection qualification — 2026-11-17

The proposed point/projector shortcuts are **unselected**. Image preservation alone did not establish a useful optimization: representative accepted-light cost regressed. The separately staged69231 viewer, its source7060967e3069f85d808d7b7ac6cca2bfef836dd2 and all production renderer code remain unchanged. No viewer was launched for these contained checks.

## Contract and actual workload

PPD-344 records that native bounded lights use screen rectangles while original1eb44 uses bounded volume/depth coverage. In the saved69228 main frame, ten bounded count1 points cover41,921,469rectangle pixels;60projectors cover248,358,927. Together with one fullscreen grid they predict296,915,916fragments, within428of the recorded fused count. These are primarily repeated local-light rectangles, not296.9million base-probe evaluations. The ten points account for14.12%of that proxy and4.80%of late serialized shader-cost attribution; neither percentage is an available FPS gain. Healthy A2/B2 retain the same point area and47projectors, only two shadowed.

Original pointLightF checks radius before GBuffer reads. The first private variant moved the exact native length/radius >1 test ahead of material/BRDF reads for count1 and returned the existing additive zero. It preserved sky rejection, equality/NaN comparison forms, resources, depth and query semantics. The projector variant kept both raw position derivatives unconditional, then moved the existing sky/radius rejection before normal decoding and unchanged receiver-plane adjustment. All later cookie/shadow and outside-cookie legacy shiny behavior stayed intact. Source and derivative audits are retained privately.

Both compile with the exact current offline options: Metal3.1, fast math, macOS26.0 deployment, line tables and recorded sources. No precision, quality, pass, resource or threadgroup-policy change was introduced. Host doctor passes on M1 Max/64GiB/macOS26.5.2/Xcode26.6.

## Point runtime-branch variant

Actual baseline library SHA256`a5330e85acf8dc6a3624b054573b3c3445ccee95f905c6553bb4f0f48eeaac42`; candidate`dde699550867113b83cc45cbbcd3be7a1f13c6f04f459d63804765d12649e3b6`, source`4d2d4ae284b736b19107780327ef9e927af5923a47c7372ec91a9e2895941717`.

After PPD-350's fixture repair,216finite cases pass exactRGBA16F/depth/stencil/alpha/visibility comparisons under API/GPU validation, including80positive and136intentional zero-only cases. Coverage includes four material classes, forward/reverse depth, attached/standalone targets, nonzero light-buffer offset, mixed/inside/outside and exact/near-radius inputs, classic scaling and count0/2/16controls. It is not full-viewer parity or nonfinite-input qualification.

Six1024×576cost cells pass exact large-image prechecks with queries disabled, matching production light-pass scope. Each retains100alternating ABBA/BAAB blocks,200samples per arm, eight warmup blocks and actual completed-command GPU intervals. Actual shader validation is disabled(enum2), stderr is empty and no intervals overlap. Paired median GPU changes:

| Cell | Change | Adverse pairs /100 |
| --- | ---: | ---: |
| PBR mixed |−31.15%|0|
| PBR all inside |+2.16%|81|
| PBR all outside |−44.96%|0|
| Legacy specular mixed |−10.41%|2|
| Legacy specular all inside |+2.44%|72|
| Generic PBR count16 |−0.38%, neutral|47|

The fully accepted cases demonstrate a regression. The runtime-branch variant is unselected; rejection-heavy fixture incidence is not the measured incidence in Home.

## Finite count1 specialization

A separate private optional Bool5, namedalmetal_single_point_light, makes the loop count a literal1 only when that PSO is selected for actual count1. Unset leaves the generic loop and removes the added early test. Candidate library`8554b7a6c7c6d49060089077022dd710c049ae4c075237051efac2b54f59d34c`, source`27091e46b8a9999948bdedbd031f98e14e691bf21a4c4e29fd2c865363eabc88`.

It passes the216small finite cases with validation enabled, but fails the first1024×576cost precheck: two blue half components change by minus1ULP, absolute3.0517578125e-5, at609,370 and438,376. Other2,359,294components and depth/stencil/alpha remain exact. Both points are well inside the light radius; code-generation rounding is consistent with the result but unproved. Validation state and pixel coverage also differ between the small and large checks. PPD-351 retains the strict failure; no threshold was relaxed, no timings followed, and the variant remains unselected.

## Projector reorder

Candidate library`bf91d98ecee27726d6b89a9a4760f3a367d6f0b040bdb4de4ed90bb5e4f39ca4`, source`a0607cb433f9cbf04a7164a13fc91709af9295c587b9c55eb7d94661cd62ca35`. All122finite parity cases pass under API/GPU validation, including both normal formats/depth conventions, shadow channels, mixed2×2depth/normal derivatives, quality0/2/3 and positive legacy shiny outside the primary cookie.

Eight1024×576cost cells also pass strict full-image/depth/stencil/alpha checks. They retain3,200measured observations/800paired blocks,256warmups, actual validation disabled2, empty stderr and zero observed GPU-envelope overlap across all processes.

| Cell | Paired median change | Adverse pairs /100 |
| --- | ---: | ---: |
| PBR mixed, shadowed |+4.12%|92|
| Legacy shiny inside, shadowed |+5.70%|100|
| PBR outside, unshadowed |−4.63%|18|
| PBR inside, shadowed |+5.50%|100|
| PBR mixed, unshadowed |+7.16%|100|
| PBR inside, unshadowed |+9.37%|100|
| Legacy shiny mixed, unshadowed |+7.13%|98|
| Legacy shiny inside, unshadowed |+9.05%|100|

The representative lit cells regress, including the predominantly unshadowed workload. This proposal is rejected. Source order did not establish lower executed cost; the compiler's scheduling/register effects were not separately measured.

## Fixture incident and retained evidence

PPD-350 preserves two initial failures: both arm encoders shared one query-result buffer, and the later encoder reset the earlier result. Apple's [visibility reset definition](https://developer.apple.com/documentation/metal/mtlvisibilityresulttype/reset) and the actual SDK26header describe this reset at encoder creation. Independent per-arm buffers produce531/531in the first case and pass all216without weakening any assertion. Production already allocates fresh result buffers per eligible GBuffer/shadow pass; point/projector passes have no active query. No production query repair is needed.

Private evidence is under `artifacts/metal/ppd344-point-rejection-20261117/`: frozen variants/compiler receipts, failed and successful parity inputs, complete raw planes, cost helpers, observations/adverse pairs, `point-cost-analysis/`, `point-specialized-failure-analysis/`, `projector-cost-analysis/` and `workload-audit/`. Additional original/native/derivative/query-store maps are in `ppd344-point-radius-source-audit-20261117/`. No captures, account data or raw private evidence were uploaded.

Owner steering now requires a whole-frame inventory of other shader spills and the remaining Home FPS decline before further local variants. Compare early and settled work, distinguish numeric allocation from blank compiler fields and actual traffic/time, and use those results to choose the next change. Existing [69231 benefit](exposure-simd-histogram-20261117.md) remains qualified; original architecture and whole-viewer performance acceptance remain partial.
