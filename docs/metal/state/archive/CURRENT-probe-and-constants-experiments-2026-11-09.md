# Probe selection and common constants — isolated experiments,2026-11-09

Physical M1 Max64GiB/macOS26.5.2/Xcode26.6. Viewer closed during timings.
No production shader edit; synthetic gains are not viewer FPS predictions.
Exact original reflection selection source is1eb44bbd1bdc76d0100c75699bad1fb337e8cb45,
class2/deferred/reflectionProbeF.glsl. Current Metal mirrors its ordered initial
scan/depth bucket/break, accepted cap32 including zero-fade entries, permanent
first-neighbor owner, duplicate/order handling, box precedence and default0.
Never filter neighbors with the initial depth bucket or resume the initial list
after a neighbor list exhausts. No quality, tap-count or cadence reduction.

## Always-on spatial masks — rejected as general default

Private prototype `/private/tmp/alchemy-metal-probe-spatial-mask-20261109.0OlIC9`.
25API/GPU fixtures and50resolution pairs have bitwise identical pixels and
accept/tap counters; missedHolds0 and9CPU guards pass. Actual infinite reverse-Z
projection is conservatively pruned, not blanket-fallback.3456x2168 sparse
76.316→66.745ms, neighbor43.802→40.620ms. But default+10%, overlap32+7.5%,
mixed boxes+12%, all-mask+25.8%; no product integration.
JSON SHA2560f6210bd8429c21fa784744e3656400ac56d1eba185c8a1625bf942d717cec7d.

## Ordered compact initial lists — promising, admission unproven

Private prototype `/private/tmp/alchemy-metal-probe-ordered-list-20261109.2gDTzx`.
29API/GPU fixtures and58resolution pairs pass BITWISE, including forced cap33
with zero fade, box precedence, neighbor below initial bucket and unsorted depth.
Preserves neighbor traversal; compact ascending IDs replace only initial scan.
uint2 offset/count per tile, uchar IDs. Default/whole-view no-pruning/unsorted
packs bypass using exact original PSO. No candidate truncation to32.

At3456x2168, sparse actual-infinite projection76.152→6.947ms and sparse-neighbor
43.622→8.775ms; CPU plan/bin/upload approximately0.523/0.515ms. Sparse tile lists
average approximately5IDs; initial linear visits255→4.985. Neighbor initial
127.655→2.717, but7.920neighbor iterations unchanged.

Dense cap33 36.734→42.071ms, boxes29.759→34.947ms and4probe-bucket-neighbor
5.322→6.069ms regress. Dense CPU plan overhead1.27–1.31ms; removing one initial
iteration is insufficient. Neighbor-reject31 gains about0.37ms against0.34msCPU,
essentially break-even. Therefore a broad default is unsafe. Live packs64–67
probes do not establish per-pixel traversal or conservative list selectivity.
Collect sparse actual geometry/depth and exact constants before admission work.
JSON SHA25685f835ba88b7884001221a3dd60540bb9bc1ece61ebcd576a8afa14e1e590d44.

Conservative projection guards preserve all uncertain geometry: positive corner
w and roundoff margins required; near/camera/overflow/ill-conditioned cases
fail open. Generic inverse projection scaling is unsafe because reconstruction
clamps w to1e-6. Standard perspective x/y/w translation structure permits common
scale cancellation, including actual infinite reverse-Z. Never extrapolate to
arbitrary matrices such as1e-9I. Boxes/default retained by prototypes.

## Local400B common vertex constants — implemented, viewer effect pending

Original lldrawpool.applyModelMatrix724–735 and llrender.packMatricesUBO743–797
skip unchanged transforms/uploads. Native common G-buffer draws previously
rehashed/re-published identical400B constants for each compatible draw.
The implementation keeps one owned value and arena slice within one render/
command only. Exact memcmp determines reuse; slot4 is ALWAYS restored even on
hits because terrain/visibility or inline bytes can overwrite it. No pointer
identity or cross-command raw slice reuse. No-arena inline path unchanged.
Cumulative successful-render binds/reuses counters provide real hit-rate proof.

New GPU fixture compares all4color attachments and raw depth against inline
baseline, changes MVP/config/normals/textures/ranges, exercises reverse-Z/HDR,
terrain slot4 clobber, two renders in one command and invalid later preflight.
Final Release/RWDI full API/GPU suites each67291assertions/81cases pass:
`/private/tmp/alchemy-metal-400b-cache-final-{release,rwdi}-gpu-20261109.log`.

Seven counterbalanced10k-draw CPU-encoding repetitions, validationoff, no GPU
submission: repeated alternating textures1408.708→1061.417us; unique matrices
1440.625→1525.166us;4draw mixed groups1410.500→1208.375us; repeated with slot4
inline-byte clobber1452.417→1138.125us. Hidden benchmark801assertions/1casepass.
Log `/private/tmp/alchemy-metal-400b-cache-cpu-benchmark-retry-20261109.log`.
Repeated-state savings0.347ms/10k draws; unique-state overhead0.0845ms/10k.
Actual hit rate and full-viewer benefit await rebuilt controlled runtime.

The initial fixture failures were a missing benchmark PSOlabel and a reserved
MSL parameter name; corrected test fixture only. Production metallib unchanged.
No view/caster/quality changes, and20FPS is not established.

## Integration checkpoint

Committed source5a99ceb9ece582728339920658f1f0e75e6041e4 includes constant reuse,
sky/far provenance and post-timing workload capture. Focused sampler53910/4;
full Release/RWDI Metal API/GPU suites121201/85 each; benchmark644/16 each pass.
Independent helper/integration review approved, default-off and normal-render
failure isolation preserved. Logs /private/tmp/alchemy-metal-probe-workload-*.
No rebuilt product or live speedup from this checkpoint is claimed yet.
