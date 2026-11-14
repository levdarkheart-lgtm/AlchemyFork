/**
 * @file almetal_shadow_visibility.metal
 * @brief Conservative native shadow visibility and stable command generation.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include <metal_stdlib>
#include <metal_command_buffer>
using namespace metal;

struct ShadowVisibilityInput
{
    float4x4 matrix;
    float4 minimum;
    float4 maximum;
    uint4 config; // x: conservative bounds, y: depth clamp
};
struct ShadowCommandTables
{
    command_buffer destination [[id(0)]];
};
struct alignas(16) ShadowVisibilityDraw
{
    device const uchar* vertex_buffers[7];
    device const uchar* fragment_buffers[2];
    device const uchar* indices;
    uint index_count;
    uint index32;
};
static_assert(sizeof(ShadowVisibilityInput) == 112, "Shadow bounds ABI");
static_assert(sizeof(ShadowVisibilityDraw) == 96, "Shadow command table ABI");
static bool shadow_finite(float4 value)
{ return all((as_type<uint4>(value) & 0x7fffffffu) < 0x7f800000u); }

kernel void almetal_shadow_visibility_classify(
    device const ShadowVisibilityInput* inputs [[buffer(1)]],
    device uint* visible [[buffer(2)]], constant uint2& counts [[buffer(6)]],
    uint index [[thread_position_in_grid]])
{
    if (index >= counts.x) return;
    const auto value = inputs[index];
    if (!value.config.x || !shadow_finite(value.minimum) || !shadow_finite(value.maximum) ||
        any(value.minimum.xyz > value.maximum.xyz)) { visible[index] = 1; return; }
    uint outside = value.config.y ? 15u : 63u;
    for (uint corner = 0; corner < 8; ++corner)
    {
        const float3 point = select(value.minimum.xyz, value.maximum.xyz,
            bool3(corner & 1u, corner & 2u, corner & 4u));
        const float4 clip = value.matrix * float4(point, 1.f);
        if (!shadow_finite(clip) || clip.w <= 0.f) { visible[index] = 1; return; }
        const float edge = clip.w + max(1.f, abs(clip.w)) * 1e-5f;
        uint bits = uint(clip.x < -edge) | (uint(clip.x > edge) << 1) |
            (uint(clip.y < -edge) << 2) | (uint(clip.y > edge) << 3) |
            (uint(clip.z < -edge) << 4) | (uint(clip.z > edge) << 5);
        outside &= bits;
    }
    visible[index] = outside == 0;
}

// Each owner supplies disjoint, ordered state/query runs. The GPU emits every
// binding and draw from the immutable native-address table; no CPU ICB command
// encoding or command-content copy is required by this kernel.
kernel void almetal_shadow_visibility_compact(
    constant ShadowCommandTables& commands [[buffer(0)]],
    device const uint* visible [[buffer(2)]], device const uint2* runs [[buffer(3)]],
    device uint2* execution_ranges [[buffer(4)]], constant uint2& counts [[buffer(6)]],
    device const ShadowVisibilityDraw* draws [[buffer(5)]],
    uint3 group [[threadgroup_position_in_grid]],
    uint thread_index [[thread_index_in_threadgroup]],
    uint simd_lane [[thread_index_in_simdgroup]],
    uint simd_group [[simdgroup_index_in_threadgroup]],
    uint simd_width [[threads_per_simdgroup]],
    uint3 threads [[threads_per_threadgroup]])
{
    if (group.x >= counts.y) return;
    const uint2 run = runs[group.x];
    // One threadgroup owns one state/query run. SIMD prefixes and a small
    // threadgroup prefix preserve exact source order while all lanes emit
    // independent native commands. Carry propagates only between 256-record
    // tiles, instead of serializing every buffer setter in one GPU thread.
    threadgroup uint count;
    threadgroup uint group_counts[256];
    threadgroup uint group_prefixes[256];
    if (thread_index == 0) count = 0;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    const uint simd_count = (threads.x + simd_width - 1) / simd_width;
    for (uint base = 0; base < run.y; base += threads.x)
    {
        const uint source_index = base + thread_index;
        const uint active = source_index < run.y ? visible[run.x + source_index] : 0;
        const uint local_prefix = simd_prefix_exclusive_sum(active);
        const uint local_count = simd_sum(active);
        if (simd_lane == 0) group_counts[simd_group] = local_count;
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (thread_index == 0)
        {
            uint next = count;
            for (uint i = 0; i < simd_count; ++i)
            {
                group_prefixes[i] = next;
                next += group_counts[i];
            }
            count = next;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (active)
        {
            render_command output(commands.destination, run.x + group_prefixes[simd_group] + local_prefix);
            output.reset();
            const auto draw = draws[run.x + source_index];
            if (draw.index_count)
            {
                for (uint slot = 0; slot < 7; ++slot)
                    if (draw.vertex_buffers[slot]) output.set_vertex_buffer(draw.vertex_buffers[slot], slot);
                for (uint slot = 0; slot < 2; ++slot)
                    if (draw.fragment_buffers[slot]) output.set_fragment_buffer(draw.fragment_buffers[slot], slot);
                if (draw.index32)
                    output.draw_indexed_primitives(primitive_type::triangle, draw.index_count,
                        reinterpret_cast<device const uint*>(draw.indices), 1);
                else
                    output.draw_indexed_primitives(primitive_type::triangle, draw.index_count,
                        reinterpret_cast<device const ushort*>(draw.indices), 1);
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    for (uint i = count + thread_index; i < run.y; i += threads.x)
        render_command(commands.destination, run.x + i).reset();
    if (thread_index == 0) execution_ranges[group.x] = uint2(run.x, count);
}
