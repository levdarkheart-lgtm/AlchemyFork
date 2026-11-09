# Direct shadow packets and populated texture bindings — 2026-11-08

Renderer commit `6c694df23756ed9805b7d853fa83d9fc64212c04` implements the repair after the owner requested further low-FPS work. Source-exact `68553` at `b1d198674d`
passed correctness but regressed its 300-frame run, so its isolated ICB CPU
benchmark was not accepted as a viewer gain. See the
[experiment report](CURRENT-68553-shadow-indirect-runtime-2026-11-08.md).

## Revised implementation

Production static opaque shadow runs now use ordinary native Metal direct
draw commands through one validated packet span and one immutable 80-byte
constant slab. The normal static PSO has indirect support disabled. Per-run
pipeline/cull setup and native per-buffer/offset caching replace per-draw C++
wrapper/publication work. Prepared packets retain each unique native resource
through real command completion; their ownership path is covered for renderer,
helper, packet, source-wrapper and command-wrapper destruction.
The experimental ICB path remains explicitly opt-in for isolated tests; it is
not production's default.

Adjacent full-triangle index ranges are merged only with identical native
buffer wrappers, vertex offset/count, index type, 80-byte MVP/config and cull
run. Byte ranges must be exactly contiguous, both counts multiples of three,
and the combined count fit uint32. Constants and physical packets compact
together; scene traversal skips original record counts while execution uses
physical command ranges. No caster-family boundary is crossed. Triangle order,
reverse-Z, clamp, bias, winding and primitive membership are preserved. Native
draw census counts physical commands; coalesced source counts are logged
separately for main and reflected views. Actual live opportunity is pending.

All ordinary, material and PBR G-buffer binding loops now bind only populated
slots. Simple/mask/bump shaders guard their dynamic index before sampling;
material/PBR stages clamp to their populated counts. Exact original
`1eb44bbd1b` binds actual indexed textures, and current forward owners already
use count-only binding with 32-wide Metal signatures. Normal/ORM maps retain
linear views; diffuse/specular/emissive retain their previous views. No shading
math changed: the previous base-light shader is restored byte-for-byte to175.

## Verification

Both complete Release and RelWithDebInfo suites pass **57,344 assertions across
75 cases**, with `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`, exit0. Focused
validation passes3,514 assertions/eight cases. Tests cover scalar/direct/ICB
exact images,128 contiguous source draws to one physical draw, merge barriers,
forward/reverse/clamp/cull, range/type/offset/lifetime failures, populated counts
1/2/8/9/32, highest/invalid indices,32-to-one transitions, material/PBR clamps,
and raw normal/sRGB color boundaries. Full native output baselines still pass.
Source-exact product build and revised live comparison are pending.

Raw test/build/benchmark evidence remains in
`/private/tmp/alchemy-metal-shadow-indirect-tests-20261108.HGz01r`.
The fresh68550 baseline rerun is owner-authorized and uses the preserved exact
bundle, same full-screen-before-login/Midday/Preset123 flow and21-control gate.
Its run directory is
`artifacts/metal/qa-runs/candidate-68550-baseline-rerun-20261108-01`;
metrics folder `/private/tmp/alchemy-metal-68550-baseline-rerun-20261108.92x7iims`.

Source-exact Xcode Release68554 build exits0; deep strict signature verifies.
Executable SHA-256:
`223bdc1054b9cdf73c336a26530431ded5eb4ca001de0339c74390ec41af93db`.
Packaged shader SHA-256 is exactly the measured68550 library:
`5bd02961a77fdfcef9d28410e9f92fc86d1de08197aec113660e0cd0ccd779d1`.
The source-exact frame comparison is pending.
Final validated native-span mechanism benchmark is
`native-span-comparison.json` SHA-256
`8c399378d65a3db97eb427fa2bbcb07b637625999c0c2ce578c85c75f4167e24`.
It reports18.7–28.7% lower CPU prepare/encoding versus direct wrappers and
broadly similar completed GPU command spans. Those spans have live-viewer
competition and are not isolatedGPUcost or viewerFPS. The optionalICB variant
retains higher syntheticGPUspans; flag-only normal draws show no consistent
penalty. All four modes preserve exactreadback.
