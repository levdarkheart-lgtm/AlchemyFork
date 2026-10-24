# Apple Silicon engineering contract

**Primary device:** M1 Max, 64 GiB unified memory. Confirm actual GPU core count, Metal family, display resolution and macOS version on host rather than assuming M1 Max SKU. Required executable ISA: ARM64 only.

## GPU and memory
- Select an `MTLDevice` on the physical Mac; record `supportsFamily`, `recommendedMaxWorkingSetSize` and relevant feature queries from actual runtime. Base implementation only on supported M1 Max features. Modern API availability in an installed SDK does not imply hardware capability.
- Preserve existing geometry/material algorithms first, then restructure command streams where captures prove a bottleneck. Batch compatible draws, minimize CPU serial submission, avoid shader variant compilation per frame, and use reproducible pipeline-state caching.
- Treat tile-based deferred rendering as a performance consideration: test `private`, `shared`, and eligible `memoryless` storage modes based on attachment lifetime and readback needs. Memoryless is not valid for data sampled later from system memory. Avoid unnecessary attachment stores and resolves.
- Unified memory removes PCIe transfers but **does not** erase synchronization, cache, resource hazards or access-cost differences. `shared` is not always faster than `private`; choose based on CPU/GPU access patterns.
- Use appropriate resource hazard tracking, buffers-in-flight, texture staging, fences/events and readback completion. Prefer captures/counters over intuition.
- Consider compute for depth pyramid, lightmap, postprocess, culling/occlusion and image manipulation where the source already describes a suitable algorithm. Revalidate correctness before optimizing.
- Handle Retina/backing-scale changes, display modes, HDR/EDR color handling and idle/pacing correctly. Do not inflate FPS by reducing image quality or resolution unnoticed.

## CPU and Neural Engine
- Optimize scene traversal, asset decode, texture streaming, avatar/animation CPU hot paths only when profiles establish them as limiters. Vectorized ARM64/Accelerate implementations may help measured hotspots.
- The Neural Engine does not execute arbitrary Metal shaders. Only consider it for an explicit Core ML inference workload (e.g., a proven ML denoiser/upscaler if fidelity permits), including transfer/scheduling overhead and battery/memory effect. Baseline implementation need not use ANE. Do **not** generate a fake neural-acceleration feature merely to claim 100% hardware utilization.
- 64 GiB memory permits ample cache but memory traffic, GPU working-set and thermal behavior still matter; benchmark cold/warm cache separately.

## Measurements
On host run `tools/metal/doctor.sh`; capture its output in a versioned text summary with sensitive information removed. Use Instruments/Metal GPU capture, frame time distributions (CPU and GPU), allocated texture bytes, memory traffic, draw/dispatch count, PSO creation events, pacing, thermals and battery/power mode where measurable. See `PERFORMANCE.md`.
