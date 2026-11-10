# Above-water probe-shadow specialization — candidate gate

Observed2026-11-10, physical M1 Max/64GiB/macOS26.5.2/Xcode26.6.
Branch codex/apple-silicon-metal. Base6b80fa8b3fbe999bcedc44a00a2eff4b9b9f437e;
root-owned, uncommitted candidate. No FPS gain or viewer activation established.

## Measured reason and original contract

The completed68608 run isolated hero face5 above-water surface fragment elapsed
13.168–15.057ms, followed by its own haze encoder. This is elapsed stage time,
not shader-exclusive GPU busy time. The actual view is mirror-clipped, proved
by producer and periodic flags, so a no-mirror discard removal is ineligible.

Exact original1eb44bbd1bdc76d0100c75699bad1fb337e8cb45 class3/environment/waterF.glsl
retains mirror discard in MIRROR_CLIP variants and calls directional-shadow
filtering. Original shadowUtil.glsl and established native cube contract select
single-tap probe shadows. Native shared filter already has optional bool0 plus
runtime flag0x4; when either is true it calls the SAME single-tap helper. This
candidate specializes the existing condition; it does not change the filter.

## Implementation and ownership

Private rebased coref84250150c4afcfca0f4840cac4a36b312c72a47f47e0a4f8e8f1f48ed541b31
and fixture0122e99ee962dad7e39ffafbf5f8a7480897d73a603c2bdd35e27f729546e53c,
packet /private/tmp/alchemy-metal-water-probe-fc0-rebased-20261110.aph95ghy.
Root applied both after removing all five rejected early-depth files. Baseline
world613a33d2/header07d0611d/diagnostics5acb312c/GPUtested23bcb; MSL080fba9e
remains byte-identical. Only four renderer/test files are modified.

Scene flag defaults false. Selection requires above-water, uses_shadows, valid
probe count>1 and the ACTUAL bound shadow pack's probe_single_tap bit. Lazy
HDR/non-HDR flat/wave PSOs clone the original descriptor and set bool0=true.
Mirror clipping, depth/stencil, blending, tessellation, resource bindings,
constants and all math remain unchanged. Main PCSS, underwater, no-shadow and
default-only paths keep original PSOs. No producer enables the scene flag yet.
Actual-selected flat/wave×undefined/false/true census is recorded only under
the existing diagnostic arm; metadata is not a fragment invocation count.

Important lifecycle correction: ALMetalRenderPipeline stores its descriptor
with library=nullptr (pipeline.mm476). Copying it alone is insufficient for a
new pipeline. Root explicitly restored descriptor.library=library.get() from
the renderer's strong owner. No shader or rendering-state change accompanied
this correction. The separate private address-space candidate had the same
source-review defect and was sent back for correction before execution.

## Actual commands and validation

Release builds via `cmake --build build-Darwin-ninja-os-arm64 --config Release --target almetal_tests -j4`
completed0 for core, fixture and library correction. Logs are
/private/tmp/alchemy-metal-water-fc0-{core,fixture,library-fix}-build-20261110.log.
The test executable was copied into a private .app with its exact newly built
offline Metal3.1 library, so runtime fallback compilation was not used.
Environment was scrubbed, then MTL_DEBUG_LAYER=1/MTL_SHADER_VALIDATION=1 and
MTL_SHADER_VALIDATION_REPORT_TO_STDERR=1 explicitly set. Both actual validation
preambles are present. Selector:
`almetal: world simple pass writes the native four-attachment G-buffer`.

Initial testgk63ig5m ended42,14993assertions/48failures: specialized PSO creation
correctly rejected the missing library. Later checks cascaded from nulltargets;
that run establishes no image parity. Frozen failed logs/receipts retained.
Corrected testu3co3n7_ ended0: **15425assertions/1case passed**, including29 strict
fullRGBA16/rawdepth/stencil pairs and24 cache reads. Covers actual flat and wave
water, mirror on/off with nonvacuous clipping, geometry/shadow forward/reverse Z,
main PCSS/probe filtering contrast, default-only/no-shadow/underwater exclusions,
finite outputs and actual specialized PSO selection.

Packet /private/tmp/alchemy-metal-water-fc0-library-validation-20261110.u3co3n7_
contains wrapper, actual command/source/per-file/executable/library hashes,
validation log and exit receipt. Session89735 is terminal0. No viewer, profiler,
GPU test or build remains running at this checkpoint.

## Next gate and limits

The public-renderer quiet cost fixture is being prepared privately. It must
include1024² inputs, true mirror clipping, flat/wave, main/default and same-path
controls, strict attachment/activation preflights, and full renderer preparation.
Keep uploads/readbacks/logging outside timed sections, actual GPU/CPU costs
separate and all counterbalanced raw rows. No concurrent GPU workload.

Correctness is necessary but not a performance result. Do not enable or build a
candidate viewer solely from these passing checks. If mechanism cost is not a
repeatable win, preserve and remove this candidate. Otherwise run required full
native suites, source-exact normal build/verify and controlled300-frame viewer
comparisons. All Ultra/mirror/shadow/quality contracts and the20FPS goal remain.

## Cost gate completed — mechanism admitted, not yet FPS proof

Actual benchmark source87ec4ad0 compiled0. Initial larger test2u3ewsgz passed
all eight1024² attachment pairs but PROCESS FAILED42: the benchmark left
command timing disabled before subsequent underwater census checks. Root added
an inner RestoreBenchmarkTiming guard, rebuilt0, and reran the COMPLETE process:
_qriydos preflight0/15719assertions/1case, eight pairs/zero timing samples.
The shader, production core and measurement boundaries were unchanged.

Separate quiet R1 and R4 processes EACH passed18642assertions/1case and all224
samples. API/GPU validation, capture and HUD were absent; thermal state0 on
all rows. Eight strict preflights were repeated in each quiet process. Fixed
commonR1 commands were short; consistent eligible savings justified ONE R4
resolution confirmation. Eligible R4 whole-command times were all8–20ms;
default-only fallback stayed short and all its28 rows were retained.

| R4 case | OriginalGPUms/invocation | SpecializedGPUms/invocation | Mean paired block delta | Seven-block range% |
| --- | ---: | ---: | ---: | ---: |
| Flat probe, no clip |3.7333|3.1157|−16.52%|−21.17..−13.20|
| Flat probe, mirror clip |3.7309|3.1546|−15.42%|−19.76..−11.65|
| Wave probe, no clip |3.8190|3.2165|−15.76%|−19.40..−12.42|
| Wave probe, mirror clip |3.8643|3.1987|−17.17%|−21.61..−13.27|
| Main fallback |4.2846|4.2713|−0.26%|−4.62..+6.12|
| Default-only fallback |0.5448|0.5448|+0.46%|−15.21..+17.55|
| SAME runtime |3.7295|3.7741|+1.27%|−2.05..+6.94|
| SAME specialized mirror |3.1395|3.1256|−0.35%|−7.23..+3.57|

All seven blocks in each of the four eligible cases were faster. Independent
raw auditf1e5a524 verified exact8×7×4 schedule,14 rows/variant/case, both actual
process exits, binary/library/source identity, strict preflights, normalization,
thermal/out-of-band flags and reproduced block statistics within1e−9. R4 raw
SHA3c02987db6299f3a786c33af123f2644e6711891e6d840b50ff462485303ac6b.
This supports admission to full suites and a viewer trial, not a20FPS or general
whole-viewer gain claim. Synthetic256 logical records share two physical cubes;
physical output pool is uncontrolled and generic owner retirement unproved.

CPU gain is NOT established. Mirrored-wave R4 precommit mean193.13→227.93µs/
invocation, with a retained block+204.70% outlier (+293.10µs/invocation). Median
batch difference corresponds to+11.53µs/invocation. Neither CPU exclusivity nor
wall-frame savings can be inferred from these isolated GPU command durations.

R1 packet /private/tmp/alchemy-metal-water-fc0-cost-restored-20261110._qriydos;
R4 /private/tmp/alchemy-metal-water-fc0-cost-r4-20261110.2jbh9jbw;
independent audit /private/tmp/alchemy-metal-water-independent-audit-20261110.33Vt40.
Full tested cost patch2b48b8e3 frozenin8ia9bwxf. Root removed private benchmark
code/imports and proved remaining strict fixture SHAe68cb9 exactly matches the
previous actual15425-assertion validation source.

Generic pipeline producer now enables the guarded specialization. Bounded,
opt-in logging reports actual selected specialized draws once per probe view;
source-frame/view tuple must match. This proves successful ENCODING, not GPU
completion; normal exact-presentation/probe gates must still pass in the trial.
Source review approved production activation, library lifetime, cache bounds,
exclusions and logging. Current full native Release/RelWithDebInfo gates and
source-exact normal viewer build remain the next required work.

## Final native gate and renderer commit

Both optimized native full suites ended0: Release and RelWithDebInfo EACH
482529assertions/104cases with actualAPI+GPUvalidation preambles. Packet
nkscvin4 contains actualcommands/exits and15file sourcehashes, all rechecked
against final files before commit. Tests ran precommit6b80+explicitdiff; renderer
commit f549a796d2c734222665dd10c836e56c0df352b9 contains those exact sourcefiles.
Private cost fixture/imports are absent; permanent strictfixture e68cb9 retained.
Normal viewer rebuild and FPS trial remain unexecuted at this checkpoint.

Raw logs/JSONL/patches/scripts/audits/native gate receipts were hash-preserved
under ignored artifacts/metal/private-water-shadow-specialization-20261110/.
Consult preservation.json for all original/preserved paths and actual filecount.
Compiled testbundles excluded, with executable/metallib hashes retained. No
private upload. No whole-viewer FPS improvement or20FPS attainment claimed.
