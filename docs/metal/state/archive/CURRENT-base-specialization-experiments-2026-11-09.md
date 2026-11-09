# Base-light specialization experiments — 2026-11-09

Physical M1 Max/64GiB/macOS26.5.2/Xcode26.6. Branch
`codex/apple-silicon-metal`; source checkpoint
`74ee5d48d97368d92e83ae40ba580516cef1e876`. The20FPS Ultra goal remains active.
This checkpoint does **not** establish an FPS improvement. Last measured viewer
remains68572/source46d622:9.382155FPS/106.585321ms mean, desired bias1 throughout
300resident frames. See [budget evidence](CURRENT-resource-aware-texture-budget-2026-11-09.md).

## Retained native API change

74ee5d adds explicit fragment-bool function constants to the existing pipeline
descriptor/factory, not a rendering framework. Values are copied and validated
against reflection; unknown/duplicate/wrong-type indices and unsupported vertex
constants fail. Optional fragment constants require a specialization call even
for an empty value list. The initial focused test aborted134 on that contract;
the corrected focused test passes425assertions/1case with API+GPU validation.
No production shader or world renderer consumes constants yet.

Full `almetal_tests` Release and RelWithDebInfo each pass391587assertions/90cases
with actual exit0 and Metal API+GPU validation. Both Ninja binaries reflect the
current API source; earlier recovery warnings about stale RWDI are superseded.
Logs under `/private/tmp/`:

- `alchemy-metal-function-constants-release-focused-20261109.log`: initial134.
- `alchemy-metal-function-constants-release-focused-retry-20261109.log`:0.
- `alchemy-metal-function-constants-release-full-20261109.log`:0.
- `alchemy-metal-function-constants-rwdi-full-20261109.log`:0.

`tools/metal/doctor.sh`, `tools/metal/build.sh configure`,
`tools/metal/build.sh build`, `tools/metal/verify_app.sh` on the canonical app,
and `codesign --verify --deep --strict --verbose=2` all completed0.
Build/verification logs are named
`/private/tmp/alchemy-metal-function-constants-{doctor,product-configure,product-build,verify-app,signature}-20261109.log`.
App68574 has349ARM64 slices, zero invalid slices; executable
`c19ea644343a61a4219771dd7beec351c59fc12903da511a21092bf75c7f5bbd`,
metallib `8bbfe4c6f3ce45eabd4fb58049cec413178213da480b23020447f01ef864b943`.
Identity: `artifacts/metal/private-74ee5d-build-identity-20261109.json`.
It has not been launched or measured. All viewers are closed and user-state
plans restored. The preserved68572 app remains the bias1 comparison baseline.

## Original contract and specialization boundary

Exact reference `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`:
`indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl`52–59
selects classic lighting per compiled program; line243 selects PBR per pixel
from the G-buffer. Original shader-manager SSR availability also combines a
global compiled feature with runtime cube-snapshot suppression. Native cubes
already disable SSR correctly; no cube raymarch bug was established.

Native `almetal_builtin.metal`12855 classifies stored normal alpha with
`abs(normal_sample.a-.67)<.1`. Mixed impostor pixels preclude reliable CPU
draw-family tagging. Original shared math/probe behavior must survive any split.
The current base HDR output is RGBA16Float; later surface RG11B10 is not the
correct base-pass timing target.

## Private classic/SSR experiment — not admitted

Directory `/private/tmp/alchemy-metal-base-function-constants-20261109.CM5KIW`.
One library preserves the complete original production source as its prefix,
SHA `5dea7fbfa0bb16c28cccd4e94a40f2b0cd131459b209cf6af9cd07f5231a5b3c`.
Named clone specializes classic/SSR only; shared math and zero-weight mixes
remain unchanged. Combined shader
`108bcc025f1508c3c6860f5ccdfac65afafde8ed50db8792c73573f89c52736a`.
Two independent source reviews preceded compilation. Shader/host compilation0;
first strict GPU comparison1. A reviewed host-only diagnostic completed0:
this means completion, not parity approval.

81fixtures×4formats:269strict byte passes/55failures. RGBA16Float66/81pass,
15failures of exactly one changed channel/one half unit each; max absolute
0.0001220703125. RGBA32Float41/81pass, max8ULP/max absolute0.00048828125.
RGBA8 and RG11B10 each81/81byte-exact. No nonfinite-class, signed-zero or
coverage discrepancies; all5 SSR positive controls pass both paths. The first
half mismatch is not proved to be a simple mathematical midpoint crossing;
target-dependent lowering/store behavior remains an inference.

Diagnostic summary SHA
`79a562cbc842d046996398ef221b082d3904f7146bf01c2f6257cfe2544e4a03`.
No epsilon was accepted. Strict failure status remains in all timing reports.

Separately authorized exploratory timing used the same target/library, a SAME
actual PSO control,8–20ms calibrated command buffers and7counterbalanced blocks
(14samples/variant), validation/HUD/capture unset, thermal0, actual exit0.
Modern medians were2.68–3.49%lower, near control block variation−3.39..+3.57%.
Classic fixtures were17.91–19.76%lower, all7blocks faster. Both feature flags
changed together, so component attribution is not isolated. The current viewer
profile is modern. These are synthetic pass costs, not actual FPS.

`timing-exploratory-summary-01.json` SHA
`6248688e25dbbead73e28e75de6d56a1db325f75c8ac9dab195a7dba961dfba0`;
raw log `c8ec251f87ede71709ac32da451f70b2a244ce7fffcf0ca36653b36e9145ac2a`.
The combined candidate is not admitted: modern benefit is small and downstream
precision parity is unverified. Preserve classic evidence for future work.

## Private material split — structural checks pass, mixed performance

Directory `/private/tmp/alchemy-metal-base-material-split-20261109.MxAxT0`.
Combined shader `2d66805daa30df4b91f4eb65f50b36fbd6d6c81f8b2e919278f5a40968baeaa3`;
host `5b2aefb08505bb8aa6c43544ab1ad7bdd558d4ca360b74d867493aa1bd52772c`.
Original prefix unchanged; named base changes only PBR selection to a constant.
Classic/SSR/shared math remain dynamic and original. Both shader and host were
independently reviewed before root compiled/executed. No production edits.

The existing sampled D32FloatStencil8 attachment is reused. One encoder masks
reset/classification to bit4, preserving water bits1/2, then draws two static
lighting programs. The classifier uses actual stored normal alpha; baseline
has only its original one draw. Net candidate timing includes all four draws.
Actual-depth initialization/readback is outside timing. No stencil fragment
export capability or new render target is assumed.

Commands from the private directory, each actual exit0:

```sh
xcrun -sdk macosx metal -std=metal3.1 -ffast-math -mmacosx-version-min=26.0 -fmodules-cache-path=modules-cache -c combined.metal -o combined.air
xcrun -sdk macosx metallib combined.air -o combined.metallib
xcrun clang++ -std=c++20 -O2 -fobjc-arc -mmacosx-version-min=26.0 -I/Users/dekstroza/Programming/AI/Secondlife/Alchemy/indra/almetal base_material_split.mm -framework Foundation -framework Metal -o base_material_split
env -u MTL_CAPTURE_ENABLED -u MTL_HUD_ENABLED -u MTL_SHADER_VALIDATION_REPORT_TO_STDERR MTL_DEBUG_LAYER=1 MTL_DEBUG_LAYER_ERROR_MODE=assert MTL_SHADER_VALIDATION=1 ./base_material_split
```

Actual GPU session78056 completed0 with API+GPU validation.97fixtures×4formats:
319strict passes/69failures; RGBA32Float43pass/54fail, RGBA16Float82pass/15fail,
RGBA8 and RG11B10 each97/97exact. Zero coverage/nonfinite-class discrepancies;
all5SSR positive controls pass both. **Stencil lower-bit errors0, material-class
errors0, depth-byte changes0**, including packed normals, clear/ties and reused
G-buffer. Exit0 is diagnostic completion only. NativeHDR differences are15changedchannels total, each onehalfunit; maxabs
0.0001220703125.32F finite differences max3ULP, no signed-zero/class changes.
Correctness summary SHA
`1ef2ee308ad11ae79628b49a3085ac2a3daf6f8955b924ae444a18614e06f0f8`;
raw validation log
`d97d348d171dd61494e884b3f9537959fc4b294268508d2b1d9484f2f9275454`.

Separate quiet exploratory timing session65843 actualexit0 used
`env -u MTL_DEBUG_LAYER -u MTL_DEBUG_LAYER_ERROR_MODE -u MTL_SHADER_VALIDATION -u MTL_SHADER_VALIDATION_REPORT_TO_STDERR -u MTL_CAPTURE_ENABLED -u MTL_HUD_ENABLED ./base_material_split --bench 1024 1024`.
8cases/224samples,7counterbalanced blocks,14samples/variant, actualRGBA16Float,
calibrated8–20msCB and thermal0. All additional draws are included.

| Fixture | Original→split median ms | Delta | Paired block range | Faster blocks |
| --- | --- | --- | --- | --- |
| SAME actual PSO |4.541410→4.511292|−0.66%|−5.35..+3.32%|4/7|
| SSRoff PBR |4.832667→4.227993|−12.51%|−18.43..−6.69%|7/7|
| SSRoff legacy |5.553556→5.064847|−8.80%|−12.22..−3.27%|7/7|
| SSRon PBR |4.644646→4.081632|−12.12%|−16.30..−8.60%|7/7|
| SSRon legacy |5.395924→5.076111|−5.93%|−8.83..−4.65%|7/7|
| Checkerboard mixed |5.006153→5.139257|+2.66%|−0.33..+4.47%|1/7|
| RGB10A2 mixed |4.955264→5.185528|+4.65%|+0.01..+5.81%|0/7|
| Independent-mask mixed |5.033521→4.724340|−6.14%|−7.63..−2.54%|7/7|

`timing-exploratory-summary-01.json` SHA
`f71a6f88967537b1f934b452a3367e9b5e4f557bccc601fdc11fbd414657f518`.
The candidate is not generally admitted: repeatable uniform-material benefits
do not override mixed-material regressions or the unapproved precision gate.
No production renderer change or actual FPS improvement follows. All processes
finished, no viewer launched and no user-state changes during these tests.

## Product advancement and remaining requirement

Validated function constants enable explicit native specialization; the private
tests resolve real depth/stencil ownership questions. Neither test volume nor
these experiments establish a user-visible FPS gain.20FPS still needs about53%
lower mean frame time than68572. Exact GPU causality inside repeated main/hero
rendering remains unresolved; prior inclusive labels cannot be summed or
assigned to individual shaders. Quality/pixel parity, multi-scene performance,
stability, strict no-GL and release gates remain open. No hardware limit has
been proved. Continue only with a specific measurable implementation question.
