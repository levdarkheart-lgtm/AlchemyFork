/** Shared conservative visibility; no compaction or state/query reordering. */
#include <metal_stdlib>
using namespace metal;
struct BoundsRecord
{
    float4 minimum, maximum;
    ulong identity, revision;
    uint4 config;
};
struct Member
{
    float4x4 matrix;
    ulong identity, revision;
    uint slot, index_count, enabled, depth_clamp;
};
struct IndexedArguments
{
    uint index_count, instance_count, index_start;
    int base_vertex;
    uint base_instance;
};
static bool visibility_finite(float4 value)
{ return all((as_type<uint4>(value) & 0x7fffffffu) < 0x7f800000u); }
kernel void almetal_world_visibility_arguments(
    device const BoundsRecord* bounds [[buffer(0)]],
    device const Member* members [[buffer(1)]],
    device IndexedArguments* arguments [[buffer(2)]],
    constant uint4& counts [[buffer(3)]],
    device const uint* destinations [[buffer(4)]],
    uint index [[thread_position_in_grid]])
{
    if (index >= counts.x) return;
    const Member value = members[index];
    bool visible = true;
    if (value.enabled && value.slot < counts.y)
    {
        const BoundsRecord record = bounds[value.slot];
        if (record.config.x && record.identity == value.identity && record.revision == value.revision &&
            visibility_finite(record.minimum) && visibility_finite(record.maximum) &&
            all(record.minimum.xyz <= record.maximum.xyz))
        {
            uint outside = value.depth_clamp ? 15u : 63u;
            for (uint corner = 0; corner < 8; ++corner)
            {
                const float3 point = select(record.minimum.xyz, record.maximum.xyz,
                    bool3(corner & 1u, corner & 2u, corner & 4u));
                const float4 clip = value.matrix * float4(point, 1.f);
                // Unknown projection/eye crossings fail visible. The source
                // convention is GL clip Z; native vertices convert it later.
                if (!visibility_finite(clip) || clip.w <= 0.f) { outside = 0; break; }
                const float edge = clip.w + max(1.f, abs(clip.w)) * 1e-5f;
                const uint bits = uint(clip.x < -edge) | (uint(clip.x > edge) << 1) |
                    (uint(clip.y < -edge) << 2) | (uint(clip.y > edge) << 3) |
                    (uint(clip.z < -edge) << 4) | (uint(clip.z > edge) << 5);
                outside &= bits;
            }
            visible = outside == 0;
        }
    }
    arguments[counts.z ? destinations[index] : index] = IndexedArguments{visible ? value.index_count : 0u, 1u, 0u, 0, 0u};
}

// Immutable source-generation membership, with one exact CPU-computed MVP
// for the entire region cohort. There is no per-view source expansion.
struct CohortRecord { float4 minimum, maximum; uint4 config; };
struct CohortView { float4x4 matrix; uint4 config; };
kernel void almetal_world_visibility_cohort_arguments(
    device const CohortRecord* rows [[buffer(0)]],
    constant CohortView& view [[buffer(1)]],
    device IndexedArguments* arguments [[buffer(2)]],
    uint index [[thread_position_in_grid]])
{
    if(index>=view.config.x)return;
    const CohortRecord row=rows[index];bool visible=true;
    if(row.config.y && visibility_finite(row.minimum) && visibility_finite(row.maximum) &&
       all(row.minimum.xyz<=row.maximum.xyz))
    {
        uint outside=63u;
        for(uint corner=0;corner<8;++corner)
        {
            const float3 point=select(row.minimum.xyz,row.maximum.xyz,bool3(corner&1u,corner&2u,corner&4u));
            const float4 clip=view.matrix*float4(point,1.f);
            if(!visibility_finite(clip)||clip.w<=0.f){outside=0;break;}
            const float edge=clip.w+max(1.f,abs(clip.w))*1e-5f;
            const uint bits=uint(clip.x < -edge)|(uint(clip.x > edge)<<1)|
                (uint(clip.y < -edge)<<2)|(uint(clip.y > edge)<<3)|
                (uint(clip.z < -edge)<<4)|(uint(clip.z > edge)<<5);
            outside &= bits;
        }
        visible=outside==0;
    }
    arguments[view.config.y+index]=IndexedArguments{visible?row.config.x:0u,1u,0u,0,0u};
}
