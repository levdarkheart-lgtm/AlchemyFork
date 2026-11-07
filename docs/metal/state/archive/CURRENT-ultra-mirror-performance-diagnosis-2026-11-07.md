# Ultra mirror performance diagnosis — 2026-11-07

This checkpoint records bounded diagnosis, not a qualifying performance claim.
The candidate was source-exact Metal `26.4.0.68508` from `921aed5ca9` on the
physical M1 Max acceptance host. Its executable SHA-256 is
`01dbd31503c99c2d3fb315c2584fe75d38c5f3478a35facc889c5b0fd82c6c5d`.
The owner retained the same attended Ultra scene and camera and reports
`RenderHeroProbeDistance=16` for the run.

## Reversible live observation

A clean uninstrumented in-session reversal measured stable samples of
`8.1, 8.1, 8.0, 8.1, 8.0, 8.1, 8.2, 8.0` FPS with Mirrors disabled and
`4.8, 4.8, 4.8, 4.9, 4.9, 4.9, 4.9, 4.9` FPS after re-enabling them. Median
rates are 8.1 and 4.9 FPS, or about 123.5 and 204.1 ms/frame. Mirrors add about
80.6 ms/frame in this scene. LEAP confirmed the same Ultra controls and near-
identical position across the toggle; Mirrors were re-enabled at handoff.

## Metal System Trace evidence

Matched 30-second captures are local at
`/private/tmp/alchemy-metal-68508-ultra-metal-system.trace` and
`/private/tmp/alchemy-metal-68508-ultra-no-mirror-metal-system.trace`.
Their exported application-command-buffer XML SHA-256 values are respectively
`ffac2a900e139cad0fa3da967e19a24e125d0f192654be2268aef3bebead534f` and
`b60363e00ad1ec6f1a9e0819e4bd4b55db8989e62e7f28ffda0ba59519a70d70`.
GPU-interval export SHA-256 values are
`70307831de5277a2e549440c3e10ee363ad01a1c59174b640ef8e8cb3e729047` and
`3d827bed2a25b943ba96717a8b0ab03a3d4137c2134c3fa8b4bf5398eb3e114a`.

Mirrors enabled submitted 10,922 application command buffers for 164 presents,
or 66.60 per present. Mirrors disabled submitted 4,398 for 156 presents, or
28.19 per present. Enabled labels normalize to 21.5 direct spot-shadow slices,
17.84 directional-shadow slice copies, 6 hero-probe face transactions and 3.66
direct spot-shadow slice copies per present. Disabled labels normalize to 5.98,
5.81, zero and 0.167 respectively. The 57.7% command-buffer reduction per
present is stronger evidence than raw traced FPS because tracing perturbs both
runs.

GPU active-union time was 40.4% with Mirrors enabled and 51.1% disabled;
thermals remained Nominal. The increased occupancy with Mirrors disabled is
consistent with the CPU feeding more frames, not a saturated GPU. Allocated
Metal memory remained near 4.87 GiB because disabling Mirrors retains resources
for reuse; this toggle does not establish a memory-saving claim.

## Time Profiler and original behavior

Time Profiler exports are local at
`/private/tmp/alchemy-metal-68508-ultra-time-profile.xml` (SHA-256
`f85e1e7547262a047f843a5f969eb2d1d5a0bcf579e20a4e72d9a07731766715`) and
`/private/tmp/alchemy-metal-68508-ultra-no-mirror-time-profile.xml` (SHA-256
`7f2f6a46cca5c3a79478f5994d444466e4c25ba4155725bedffb7dc862780780`).
The enabled capture attributes dominant main-thread samples to sun/spot shadow
generation, hero/reflection capture and Metal binding/resource encoding.
Absolute enabled/disabled sample times are not directly comparable because
waiting threads are not sampled and the disabled run feeds more frames.

Exact original-viewer source tracing at `1eb44bbd1b` confirms
`RenderHeroProbeUpdateRate=1` intentionally schedules all six cube faces every
frame. Six faces are therefore not themselves a Metal semantic bug. The Metal
path instead creates a render target and command buffer for each spot-shadow
slice, followed by another command buffer that copies the slice into a retained
array. Trace original resource, synchronization and lifetime ownership before
replacing this with persistent array-slice rendering or coalesced command
buffers.

The current Release binary defines `LL_RELEASE_FOR_DOWNLOAD`, compiling out the
built-in CSV collector even though its environment variables were present.
Formal quality-equivalent 300-frame Metal/OpenGL evidence therefore requires an
instrumentation-only Release build or matched external captures. These local
traces identify the optimization boundary but do not satisfy
`docs/metal/PERFORMANCE.md`.
