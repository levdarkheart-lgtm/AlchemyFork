#include <metal_stdlib>
using namespace metal;

struct ALMetalTestVertex
{
    packed_float2 position;
    packed_float4 color;
};

struct ALMetalTestVaryings
{
    float4 position [[position]];
    float4 color;
};

vertex ALMetalTestVaryings almetal_test_vertex(
    const device ALMetalTestVertex* vertices [[buffer(0)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalTestVaryings output;
    output.position = float4(vertices[vertex_id].position, 0.0, 1.0);
    output.color = vertices[vertex_id].color;
    return output;
}

fragment float4 almetal_test_fragment(ALMetalTestVaryings input [[stage_in]])
{
    return input.color;
}

kernel void almetal_test_compute(
    device uint* values [[buffer(0)]],
    constant uint& addend [[buffer(1)]],
    uint index [[thread_position_in_grid]])
{
    values[index] = values[index] * 3u + addend;
}

struct ALMetalMatrices
{
    float4x4 modelview;
    float4x4 projection;
    float4x4 modelview_projection;
    float4x4 inverse_projection;
    float4x4 texture0;
    float4 normal0;
    float4 normal1;
    float4 normal2;
};

struct ALMetalUIVaryings
{
    float4 position [[position]];
    float4 color;
    float2 texcoord;
};

struct ALMetalUIFragmentConstants
{
    uint text_shadow_mode;
};

struct ALMetalSolidFragmentConstants
{
    float4 color;
};

static float4 almetal_clip_from_gl(float4 position)
{
    // OpenGL clip depth is [-w,+w], while Metal is [0,+w]. X/Y and winding
    // remain in the viewer's established coordinate convention.
    position.z = (position.z + position.w) * 0.5;
    return position;
}

vertex ALMetalUIVaryings almetal_ui_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    const device uchar4* colors [[buffer(2)]],
    constant ALMetalMatrices& matrices [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalUIVaryings output;
    output.position = almetal_clip_from_gl(
        matrices.modelview_projection * float4(positions[vertex_id].xyz, 1.0));
    output.texcoord = (matrices.texture0 * float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.color = float4(colors[vertex_id]) * (1.0 / 255.0);
    return output;
}

fragment float4 almetal_ui_fragment(
    ALMetalUIVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant ALMetalUIFragmentConstants& constants [[buffer(0)]])
{
    if (constants.text_shadow_mode == 0)
    {
        return input.color * diffuse_map.sample(diffuse_sampler, input.texcoord);
    }

    const float2 atlas_texel_size = 1.0 / float2(diffuse_map.get_width(), diffuse_map.get_height());
    const float vertex_alpha = input.color.a;
    auto atlas_alpha = [&](float2 offset) {
        return diffuse_map.sample(diffuse_sampler, input.texcoord + atlas_texel_size * offset).a;
    };

    float product;
    if (constants.text_shadow_mode == 1)
    {
        product = 1.0 - vertex_alpha * atlas_alpha(float2(-1.0, 1.0));
    }
    else
    {
        product  = 1.0 - vertex_alpha * atlas_alpha(float2( 1.0,  1.0));
        product *= 1.0 - vertex_alpha * atlas_alpha(float2(-1.0,  1.0));
        product *= 1.0 - vertex_alpha * atlas_alpha(float2(-1.0, -1.0));
        product *= 1.0 - vertex_alpha * atlas_alpha(float2( 1.0, -1.0));
        product *= 1.0 - vertex_alpha * atlas_alpha(float2( 0.0,  2.0));
    }
    return float4(input.color.rgb, 1.0 - product);
}

vertex ALMetalUIVaryings almetal_solid_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalMatrices& matrices [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalUIVaryings output;
    output.position = almetal_clip_from_gl(
        matrices.modelview_projection * float4(positions[vertex_id].xyz, 1.0));
    output.texcoord = texcoords[vertex_id];
    output.color = 1.0;
    return output;
}

fragment float4 almetal_solid_fragment(
    ALMetalUIVaryings input [[stage_in]],
    texture2d<float> alpha_map [[texture(0)]],
    sampler alpha_sampler [[sampler(0)]],
    constant ALMetalSolidFragmentConstants& constants [[buffer(0)]])
{
    const float alpha = alpha_map.sample(alpha_sampler, input.texcoord).a * constants.color.a;
    return float4(constants.color.rgb, alpha);
}
