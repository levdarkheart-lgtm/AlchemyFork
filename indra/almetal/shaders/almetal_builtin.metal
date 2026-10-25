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

kernel void almetal_test_sample_texture(
    texture2d<float, access::read> source [[texture(0)]],
    device float4* result [[buffer(0)]])
{
    result[0] = source.read(uint2(0, 0));
}

struct ALMetalResampleVaryings
{
    float4 position [[position]];
    float2 texcoord;
};

vertex ALMetalResampleVaryings almetal_resample_vertex(uint vertex_id [[vertex_id]])
{
    // One oversized triangle covers the viewport. Mapping (0,0) to the
    // upper-left matches Metal texture coordinates and preserves raw row
    // orientation through a resize.
    const float2 texcoord = float2((vertex_id << 1) & 2, vertex_id & 2);
    ALMetalResampleVaryings output;
    output.position = float4(texcoord.x * 2.0 - 1.0,
                             1.0 - texcoord.y * 2.0,
                             0.0, 1.0);
    output.texcoord = texcoord;
    return output;
}

fragment float4 almetal_resample_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]])
{
    return source.sample(source_sampler, input.texcoord);
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

struct ALMetalAlphaMaskFragmentConstants
{
    float minimum_alpha;
};

struct ALMetalVertexRemapConstants
{
    uint mode;
    uint source_count;
    uint flip_y;
};

static uint almetal_source_vertex(uint vertex_id,
                                  constant ALMetalVertexRemapConstants& remap)
{
    // 0: direct. 1: triangle fan expanded to a triangle list.
    // 2: line loop expanded to a line strip with vertex zero repeated.
    if (remap.mode == 1)
    {
        const uint lane = vertex_id % 3;
        const uint triangle = vertex_id / 3;
        return lane == 0 ? 0 : triangle + lane;
    }
    if (remap.mode == 2 && vertex_id == remap.source_count)
    {
        return 0;
    }
    return vertex_id;
}

static float4 almetal_clip_from_gl(float4 position)
{
    // OpenGL clip depth is [-w,+w], while Metal is [0,+w]. X/Y and winding
    // remain in the viewer's established coordinate convention.
    position.z = (position.z + position.w) * 0.5;
    return position;
}

static float3 almetal_srgb_to_linear(float3 encoded);
static float3 almetal_linear_to_srgb(float3 linear);

struct ALMetalObjectSkinnedVertex
{
    float4 position;
    float4 normal;
};

// Decode objectSkinV.glsl's packed four-influence stream. Element zero is the
// agent-space rebase origin; every joint contributes three float4 columns.
static ALMetalObjectSkinnedVertex almetal_object_skin_vertex(
    float4 position,
    float4 normal_value,
    float4 encoded_weights,
    const device float4* skin_palette,
    uint joint_count)
{
    float4 weights = fract(encoded_weights);
    const float weight_sum = dot(weights, float4(1.0));
    weights = weight_sum > 0.0
        ? weights / weight_sum : float4(1.0, 0.0, 0.0, 0.0);
    const float4 clamped_indices = clamp(
        floor(encoded_weights), float4(0.0),
        float4(float(max(joint_count, 1u) - 1u)));
    const uint4 indices = uint4(clamped_indices);

    float4 column0 = 0.0;
    float4 column1 = 0.0;
    float4 column2 = 0.0;
    for (uint influence = 0; influence < 4; ++influence)
    {
        const uint palette_base = 1u + indices[influence] * 3u;
        column0 += skin_palette[palette_base] * weights[influence];
        column1 += skin_palette[palette_base + 1u] * weights[influence];
        column2 += skin_palette[palette_base + 2u] * weights[influence];
    }

    const float3x3 skin_matrix = float3x3(
        column0.xyz, column1.xyz, column2.xyz);
    ALMetalObjectSkinnedVertex output;
    output.position = float4(
        skin_matrix * position.xyz +
            float3(column0.w, column1.w, column2.w),
        0.0);
    const float3x3 skin_normal_matrix = float3x3(
        cross(skin_matrix[1], skin_matrix[2]),
        cross(skin_matrix[2], skin_matrix[0]),
        cross(skin_matrix[0], skin_matrix[1]));
    output.normal = float4(skin_normal_matrix * normal_value.xyz, 0.0);
    return output;
}

static float3 almetal_object_skin_direction(
    float3 direction,
    float4 encoded_weights,
    const device float4* skin_palette,
    uint joint_count)
{
    float4 weights = fract(encoded_weights);
    const float weight_sum = dot(weights, float4(1.0));
    weights = weight_sum > 0.0
        ? weights / weight_sum : float4(1.0, 0.0, 0.0, 0.0);
    const uint4 indices = uint4(clamp(
        floor(encoded_weights), float4(0.0),
        float4(float(max(joint_count, 1u) - 1u))));

    float4 column0 = 0.0;
    float4 column1 = 0.0;
    float4 column2 = 0.0;
    for (uint influence = 0; influence < 4; ++influence)
    {
        const uint palette_base = 1u + indices[influence] * 3u;
        column0 += skin_palette[palette_base] * weights[influence];
        column1 += skin_palette[palette_base + 1u] * weights[influence];
        column2 += skin_palette[palette_base + 2u] * weights[influence];
    }
    return float3x3(column0.xyz, column1.xyz, column2.xyz) * direction;
}

struct ALMetalWorldSimpleConstants
{
    float4x4 modelview_projection;
    float4 normal_matrix[3];
    float4 rigged_normal_matrix[3];
    float4x4 texture_matrix;
    float4 material;
    uint4 depth_convention;
    float4 avatar_wind;
    float4 avatar_sin_wave;
    float4 avatar_gravity;
    float4 avatar_cloth_depth;
};

struct ALMetalWorldSimpleVaryings
{
    float4 position [[position]];
    float3 normal;
    float4 color;
    float2 texcoord;
    float2 texcoord1;
    float2 texcoord2;
    float3 tangent;
    float3 bitangent;
    uint texture_index [[flat]];
    uint texture_count [[flat]];
    float alpha_cutoff [[flat]];
    uint3 material_flags [[flat]];
};

struct ALMetalWorldGBufferOutput
{
    float4 albedo [[color(0)]];
    float4 specular_orm [[color(1)]];
    float4 normal [[color(2)]];
    float4 emissive [[color(3)]];
};

struct ALMetalWorldImpostorConstants
{
    float4x4 modelview_projection;
    float4 center;
    float4 left;
    float4 up;
    float4 normal_rotation[3];
    uint4 depth_convention;
};

struct ALMetalWorldImpostorVaryings
{
    float4 position [[position]];
    float2 texcoord;
};

vertex ALMetalWorldImpostorVaryings almetal_world_impostor_vertex(
    constant ALMetalWorldImpostorConstants& constants [[buffer(0)]],
    uint vertex_id [[vertex_id]])
{
    constexpr float2 corner[6] = {
        float2( 1.0, -1.0), float2(-1.0, -1.0),
        float2(-1.0,  1.0), float2( 1.0, -1.0),
        float2(-1.0,  1.0), float2( 1.0,  1.0),
    };
    constexpr float2 texcoord[6] = {
        float2(0.0, 0.0), float2(1.0, 0.0),
        float2(1.0, 1.0), float2(0.0, 0.0),
        float2(1.0, 1.0), float2(0.0, 1.0),
    };
    const float3 world_position = constants.center.xyz +
        corner[vertex_id].x * constants.left.xyz +
        corner[vertex_id].y * constants.up.xyz;
    float4 clip_position = constants.modelview_projection *
                           float4(world_position, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldImpostorVaryings output;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = texcoord[vertex_id];
    return output;
}

vertex ALMetalWorldSimpleVaryings almetal_world_simple_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalWorldSimpleVaryings output;
    float4 clip_position = constants.modelview_projection *
                           float4(positions[vertex_id].xyz, 1.0);
    // Match the GL render target's lower-left row-zero storage while the
    // migration path compares the two G-buffers byte-for-byte.
    clip_position.y = -clip_position.y;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);

    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    output.normal = normalize(normal_matrix * normals[vertex_id].xyz);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = output.texcoord;
    output.texcoord2 = output.texcoord;
    output.tangent = 0.0;
    output.bitangent = 0.0;
    output.texture_index = constants.depth_convention.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    output.texture_count = constants.depth_convention.y;
    output.alpha_cutoff = constants.material.x;
    output.material_flags = uint3(
        constants.material.y != 0.0,
        constants.material.z != 0.0,
        constants.material.w != 0.0);
    return output;
}

vertex ALMetalWorldSimpleVaryings almetal_world_simple_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], normals[vertex_id],
        encoded_weights[vertex_id], skin_palette, joint_count);
    float4 clip_position =
        constants.modelview_projection * skinned.position +
        constants.modelview_projection * float4(skin_palette[0].xyz, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldSimpleVaryings output;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    const float3x3 rigged_normal_matrix = float3x3(
        constants.rigged_normal_matrix[0].xyz,
        constants.rigged_normal_matrix[1].xyz,
        constants.rigged_normal_matrix[2].xyz);
    output.normal = normalize(
        rigged_normal_matrix * skinned.normal.xyz);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = output.texcoord;
    output.texcoord2 = output.texcoord;
    output.tangent = 0.0;
    output.bitangent = 0.0;
    output.texture_index = constants.depth_convention.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    output.texture_count = constants.depth_convention.y;
    output.alpha_cutoff = constants.material.x;
    output.material_flags = uint3(
        constants.material.y != 0.0,
        constants.material.z != 0.0,
        constants.material.w != 0.0);
    return output;
}

vertex ALMetalWorldSimpleVaryings almetal_world_bump_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* tangents [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalWorldSimpleVaryings output;
    float4 clip_position = constants.modelview_projection *
                           float4(positions[vertex_id].xyz, 1.0);
    clip_position.y = -clip_position.y;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);

    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    output.normal = normalize(normal_matrix * normals[vertex_id].xyz);
    output.tangent = normalize(normal_matrix * tangents[vertex_id].xyz);
    output.bitangent = tangents[vertex_id].w *
                       cross(output.normal, output.tangent);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = output.texcoord;
    output.texcoord2 = output.texcoord;
    output.texture_index = constants.depth_convention.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    output.texture_count = constants.depth_convention.y;
    output.alpha_cutoff = constants.material.x;
    output.material_flags = uint3(
        constants.material.y != 0.0,
        constants.material.z != 0.0,
        constants.material.w != 0.0);
    return output;
}

vertex ALMetalWorldSimpleVaryings almetal_world_bump_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    const device float4* tangents [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], normals[vertex_id],
        encoded_weights[vertex_id], skin_palette, joint_count);
    float4 clip_position =
        constants.modelview_projection * skinned.position +
        constants.modelview_projection * float4(skin_palette[0].xyz, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldSimpleVaryings output;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    const float3x3 rigged_normal_matrix = float3x3(
        constants.rigged_normal_matrix[0].xyz,
        constants.rigged_normal_matrix[1].xyz,
        constants.rigged_normal_matrix[2].xyz);
    output.normal = normalize(
        rigged_normal_matrix * skinned.normal.xyz);
    const float3 skinned_tangent = almetal_object_skin_direction(
        tangents[vertex_id].xyz, encoded_weights[vertex_id],
        skin_palette, joint_count);
    output.tangent = normalize(
        rigged_normal_matrix * skinned_tangent);
    output.bitangent = tangents[vertex_id].w *
                       cross(output.normal, output.tangent);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = output.texcoord;
    output.texcoord2 = output.texcoord;
    output.texture_index = constants.depth_convention.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    output.texture_count = constants.depth_convention.y;
    output.alpha_cutoff = constants.material.x;
    output.material_flags = uint3(
        constants.material.y != 0.0,
        constants.material.z != 0.0,
        constants.material.w != 0.0);
    return output;
}

vertex ALMetalWorldSimpleVaryings almetal_world_material_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* tangents [[buffer(8)]],
    const device float2* texcoords1 [[buffer(9)]],
    const device float2* texcoords2 [[buffer(10)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalWorldSimpleVaryings output;
    float4 clip_position = constants.modelview_projection *
                           float4(positions[vertex_id].xyz, 1.0);
    clip_position.y = -clip_position.y;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);

    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    output.normal = normalize(normal_matrix * normals[vertex_id].xyz);
    output.tangent = normalize(normal_matrix * tangents[vertex_id].xyz);
    output.bitangent = tangents[vertex_id].w *
                       cross(output.normal, output.tangent);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = (constants.texture_matrix *
                        float4(texcoords1[vertex_id], 0.0, 1.0)).xy;
    output.texcoord2 = (constants.texture_matrix *
                        float4(texcoords2[vertex_id], 0.0, 1.0)).xy;
    output.texture_index = constants.depth_convention.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    output.texture_count = constants.depth_convention.y;
    output.alpha_cutoff = constants.material.x;
    output.material_flags = uint3(
        constants.material.y != 0.0,
        constants.material.z != 0.0,
        constants.material.w != 0.0);
    return output;
}

vertex ALMetalWorldSimpleVaryings almetal_world_material_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    const device float4* tangents [[buffer(8)]],
    const device float2* texcoords1 [[buffer(9)]],
    const device float2* texcoords2 [[buffer(10)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], normals[vertex_id],
        encoded_weights[vertex_id], skin_palette, joint_count);
    float4 clip_position =
        constants.modelview_projection * skinned.position +
        constants.modelview_projection * float4(skin_palette[0].xyz, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldSimpleVaryings output;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    const float3x3 rigged_normal_matrix = float3x3(
        constants.rigged_normal_matrix[0].xyz,
        constants.rigged_normal_matrix[1].xyz,
        constants.rigged_normal_matrix[2].xyz);
    output.normal = normalize(
        rigged_normal_matrix * skinned.normal.xyz);
    const float3 skinned_tangent = almetal_object_skin_direction(
        tangents[vertex_id].xyz, encoded_weights[vertex_id],
        skin_palette, joint_count);
    output.tangent = normalize(
        rigged_normal_matrix * skinned_tangent);
    output.bitangent = tangents[vertex_id].w *
                       cross(output.normal, output.tangent);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = (constants.texture_matrix *
                        float4(texcoords1[vertex_id], 0.0, 1.0)).xy;
    output.texcoord2 = (constants.texture_matrix *
                        float4(texcoords2[vertex_id], 0.0, 1.0)).xy;
    output.texture_index = constants.depth_convention.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    output.texture_count = constants.depth_convention.y;
    output.alpha_cutoff = constants.material.x;
    output.material_flags = uint3(
        constants.material.y != 0.0,
        constants.material.z != 0.0,
        constants.material.w != 0.0);
    return output;
}

struct ALMetalWorldPBRSlotConstants
{
    float4 texture_transform[4][2];
    float4 emissive_color;
    // x = roughness factor, y = metallic factor, z = alpha cutoff.
    float4 properties;
};

struct ALMetalWorldPBRConstants
{
    ALMetalWorldPBRSlotConstants slots[8];
    // x = populated slot count, y = alpha mask, z = HDR normal target.
    uint4 config;
};

struct ALMetalWorldPBRVaryings
{
    float4 position [[position]];
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float4 color;
    float2 base_color_texcoord;
    float2 normal_texcoord;
    float2 orm_texcoord;
    float2 emissive_texcoord;
    uint material_index [[flat]];
};

static float2 almetal_world_pbr_texcoord(
    float2 vertex_texcoord,
    float4x4 sl_animation_transform,
    constant float4* material_transform)
{
    float2 texcoord =
        (sl_animation_transform * float4(vertex_texcoord, 0.0, 1.0)).xy;
    texcoord.y = 1.0 - texcoord.y;
    const float2 scaled = texcoord * material_transform[0].xy;
    const float cosine = cos(material_transform[0].z);
    const float sine = sin(material_transform[0].z);
    texcoord = float2(cosine * scaled.x + sine * scaled.y,
                      -sine * scaled.x + cosine * scaled.y) +
               material_transform[1].xy;
    texcoord.y = 1.0 - texcoord.y;
    return texcoord;
}

static float4 almetal_world_pbr_tangent(
    float4 tangent,
    float3 normal,
    float4x4 sl_animation_transform,
    constant float4* normal_transform)
{
    const float inverse_rotation = -normal_transform[0].z;
    float2 weights = float2(cos(inverse_rotation),
                            -sin(inverse_rotation));
    const float2 scale_sign = sign(normal_transform[0].xy);
    weights *= scale_sign;
    const float2x2 sl_rotation_scale = float2x2(
        float2(sl_animation_transform[0][0],
               sl_animation_transform[0][1]),
        float2(sl_animation_transform[1][0],
               sl_animation_transform[1][1]));
    weights = sl_rotation_scale * weights;
    if (abs(weights.x) + abs(weights.y) == 0.0)
    {
        weights = float2(1.0, 0.0);
    }
    weights = normalize(weights);
    weights.y = -weights.y;
    const float3 binormal = tangent.w * cross(normal, tangent.xyz);
    return float4(weights.x * tangent.xyz + weights.y * binormal,
                  tangent.w * scale_sign.x * scale_sign.y);
}

static ALMetalWorldPBRVaryings almetal_world_pbr_varyings(
    float4 clip_position,
    float3 normal,
    float4 tangent,
    float2 texcoord,
    uchar4 encoded_color,
    uint material_index,
    constant ALMetalWorldSimpleConstants& world,
    constant ALMetalWorldPBRConstants& pbr)
{
    ALMetalWorldPBRVaryings output;
    clip_position.y = -clip_position.y;
    output.position = world.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.normal = normalize(normal);
    const uint slot = min(material_index, max(pbr.config.x, 1u) - 1u);
    constant ALMetalWorldPBRSlotConstants& material =
        pbr.slots[min(slot, 7u)];
    const float4 adjusted_tangent = almetal_world_pbr_tangent(
        float4(normalize(tangent.xyz), tangent.w), output.normal,
        world.texture_matrix, material.texture_transform[1]);
    output.tangent = normalize(adjusted_tangent.xyz);
    output.bitangent = adjusted_tangent.w *
                       cross(output.normal, output.tangent);
    const float4 color = float4(encoded_color) / 255.0;
    output.color = float4(almetal_srgb_to_linear(color.rgb), color.a);
    output.base_color_texcoord = almetal_world_pbr_texcoord(
        texcoord, world.texture_matrix, material.texture_transform[0]);
    output.normal_texcoord = almetal_world_pbr_texcoord(
        texcoord, world.texture_matrix, material.texture_transform[1]);
    output.orm_texcoord = almetal_world_pbr_texcoord(
        texcoord, world.texture_matrix, material.texture_transform[2]);
    output.emissive_texcoord = almetal_world_pbr_texcoord(
        texcoord, world.texture_matrix, material.texture_transform[3]);
    output.material_index = slot;
    return output;
}

vertex ALMetalWorldPBRVaryings almetal_world_pbr_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& world [[buffer(4)]],
    const device float4* tangents [[buffer(8)]],
    constant ALMetalWorldPBRConstants& pbr [[buffer(11)]],
    uint vertex_id [[vertex_id]])
{
    const float3x3 normal_matrix = float3x3(
        world.normal_matrix[0].xyz,
        world.normal_matrix[1].xyz,
        world.normal_matrix[2].xyz);
    const uint material_index = world.depth_convention.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    return almetal_world_pbr_varyings(
        world.modelview_projection *
            float4(positions[vertex_id].xyz, 1.0),
        normal_matrix * normals[vertex_id].xyz,
        float4(normal_matrix * tangents[vertex_id].xyz,
               tangents[vertex_id].w),
        texcoords[vertex_id], colors[vertex_id], material_index, world, pbr);
}

vertex ALMetalWorldPBRVaryings almetal_world_pbr_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& world [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    const device float4* tangents [[buffer(8)]],
    constant ALMetalWorldPBRConstants& pbr [[buffer(11)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], normals[vertex_id],
        encoded_weights[vertex_id], skin_palette, joint_count);
    const float3x3 normal_matrix = float3x3(
        world.rigged_normal_matrix[0].xyz,
        world.rigged_normal_matrix[1].xyz,
        world.rigged_normal_matrix[2].xyz);
    const float3 tangent = almetal_object_skin_direction(
        tangents[vertex_id].xyz, encoded_weights[vertex_id],
        skin_palette, joint_count);
    const uint material_index = world.depth_convention.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    return almetal_world_pbr_varyings(
        world.modelview_projection * skinned.position +
            world.modelview_projection * float4(skin_palette[0].xyz, 1.0),
        normal_matrix * skinned.normal.xyz,
        float4(normal_matrix * tangent, tangents[vertex_id].w),
        texcoords[vertex_id], colors[vertex_id], material_index, world, pbr);
}

static float2 almetal_world_oct_encode(float3 normal)
{
    const float denominator =
        abs(normal.x) + abs(normal.y) + abs(normal.z);
    float2 oct = normal.xy / max(denominator, 1.0e-20);
    if (normal.z < 0.0)
    {
        oct = (1.0 - abs(oct.yx)) *
              select(float2(-1.0), float2(1.0), oct >= 0.0);
    }
    return oct * 0.5 + 0.5;
}

static float3 almetal_world_oct_decode(float2 encoded)
{
    const float2 oct = encoded * 2.0 - 1.0;
    float3 normal = float3(
        oct, 1.0 - abs(oct.x) - abs(oct.y));
    const float fold = clamp(-normal.z, 0.0, 1.0);
    normal.xy += select(float2(fold), float2(-fold), normal.xy >= 0.0);
    return normalize(normal);
}

fragment ALMetalWorldGBufferOutput almetal_world_impostor_fragment(
    ALMetalWorldImpostorVaryings input [[stage_in]],
    texture2d<float> albedo_texture [[texture(0)]],
    texture2d<float> specular_texture [[texture(1)]],
    texture2d<float> normal_texture [[texture(2)]],
    texture2d<float> emissive_texture [[texture(3)]],
    sampler point_sampler [[sampler(0)]],
    constant ALMetalWorldImpostorConstants& constants [[buffer(0)]])
{
    constexpr float minimum_alpha = 0.01;
    const float4 encoded_albedo = albedo_texture.sample(
        point_sampler, input.texcoord);
    if (encoded_albedo.a < minimum_alpha)
    {
        discard_fragment();
    }

    const float4 normal_sample = normal_texture.sample(
        point_sampler, input.texcoord);
    const bool has_gbuffer = normal_sample.a > 0.17;
    const float material_flag =
        has_gbuffer && abs(normal_sample.a - 0.67) < 0.1
            ? 0.67 : 0.34;
    const float3x3 normal_rotation = float3x3(
        constants.normal_rotation[0].xyz,
        constants.normal_rotation[1].xyz,
        constants.normal_rotation[2].xyz);
    const float3 view_normal = has_gbuffer
        ? normalize(normal_rotation *
                    almetal_world_oct_decode(normal_sample.xy))
        : float3(0.0, 0.0, 1.0);

    ALMetalWorldGBufferOutput output;
    // Attachment zero is sRGB. Returning the recovered linear colour lets
    // the Metal render target perform the final encode, matching the GL
    // impostor path's explicit decode/unpremultiply/encode plus raw store.
    output.albedo = float4(
        almetal_srgb_to_linear(encoded_albedo.rgb) /
            max(encoded_albedo.a, minimum_alpha),
        0.0);
    output.specular_orm = specular_texture.sample(
        point_sampler, input.texcoord);
    output.normal = float4(
        almetal_world_oct_encode(view_normal), normal_sample.z,
        material_flag);
    output.emissive = float4(
        emissive_texture.sample(point_sampler, input.texcoord).rgb, 0.0);
    return output;
}

fragment ALMetalWorldGBufferOutput almetal_world_pbr_fragment(
    ALMetalWorldPBRVaryings input [[stage_in]],
    array<texture2d<float>, 8> base_color_textures [[texture(0)]],
    array<texture2d<float>, 8> normal_textures [[texture(8)]],
    array<texture2d<float>, 8> orm_textures [[texture(16)]],
    array<texture2d<float>, 8> emissive_textures [[texture(24)]],
    sampler material_sampler [[sampler(0)]],
    constant ALMetalWorldPBRConstants& constants [[buffer(0)]],
    bool front_facing [[front_facing]])
{
    const uint material_index = min(input.material_index, 7u);
    const ALMetalWorldPBRSlotConstants material =
        constants.slots[material_index];
    const float4 base_color =
        base_color_textures[material_index].sample(
            material_sampler, input.base_color_texcoord) * input.color;
    constexpr float quantization_bias = 1.0 / 512.0;
    if (constants.config.y != 0 &&
        base_color.a < material.properties.z - quantization_bias)
    {
        discard_fragment();
    }

    const float3 tangent_normal =
        normal_textures[material_index].sample(
            material_sampler, input.normal_texcoord).rgb * 2.0 - 1.0;
    float3 normal = normalize(
        tangent_normal.x * input.tangent +
        tangent_normal.y * input.bitangent +
        tangent_normal.z * input.normal);
    float3 geometric_normal = normalize(input.normal);
    if (!front_facing)
    {
        normal = -normal;
        geometric_normal = -geometric_normal;
    }

    float3 orm = orm_textures[material_index].sample(
        material_sampler, input.orm_texcoord).rgb;
    orm.g *= material.properties.x;
    orm.b *= material.properties.y;
    const float3 du = dfdx(normal);
    const float3 dv = dfdy(normal);
    const float variance = 0.15 * (dot(du, du) + dot(dv, dv));
    const float roughness_kernel = min(2.0 * variance, 0.25);
    const float alpha = orm.g * orm.g;
    orm.g = sqrt(sqrt(clamp(
        alpha * alpha + roughness_kernel, 0.0, 1.0)));

    const float3 emissive = material.emissive_color.rgb *
        emissive_textures[material_index].sample(
            material_sampler, input.emissive_texcoord).rgb;
    const float roughness_high = floor(clamp(orm.g, 0.0, 1.0) * 255.0) /
                                 255.0;
    const float roughness_low = (orm.g - roughness_high) * 255.0;
    const float2 geometric_oct = clamp(
        almetal_world_oct_encode(geometric_normal), 0.0, 1.0);
    const float geometric_levels = constants.config.z != 0 ? 255.0 : 31.0;
    const float geometric_range = constants.config.z != 0 ? 65535.0 : 1023.0;
    const float2 geometric_quantized =
        floor(geometric_oct * geometric_levels + 0.5);
    const float packed_geometric_normal =
        (geometric_quantized.x * (geometric_levels + 1.0) +
         geometric_quantized.y) / geometric_range;

    ALMetalWorldGBufferOutput output;
    output.albedo = max(float4(base_color.rgb, 0.0), float4(0.0));
    output.specular_orm = max(
        float4(orm.r, roughness_high, orm.b, roughness_low), float4(0.0));
    output.normal = float4(
        almetal_world_oct_encode(normal), packed_geometric_normal, 0.67);
    output.emissive = max(float4(emissive, 0.0), float4(0.0));
    return output;
}

fragment ALMetalWorldGBufferOutput almetal_world_simple_fragment(
    ALMetalWorldSimpleVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    const float4 diffuse = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord)
        : float4(1.0, 0.0, 1.0, 1.0);
    const float3 color = input.color.rgb * diffuse.rgb;
    const float3 normal = normalize(input.normal);
    const float denominator = abs(normal.x) + abs(normal.y) + abs(normal.z);
    float2 oct = normal.xy / max(denominator, 1.0e-20);
    if (normal.z < 0.0)
    {
        oct = (1.0 - abs(oct.yx)) *
              select(float2(-1.0), float2(1.0), oct >= 0.0);
    }

    ALMetalWorldGBufferOutput output;
    output.albedo = float4(color, 0.0);
    output.specular_orm = float4(input.color.aaa, input.color.a);
    output.normal = float4(oct * 0.5 + 0.5,
                           input.color.a, 0.34);
    output.emissive = 0.0;
    return output;
}

fragment ALMetalWorldGBufferOutput almetal_world_alpha_mask_fragment(
    ALMetalWorldSimpleVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    const float4 diffuse = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord)
        : float4(1.0, 0.0, 1.0, 1.0);
    const bool use_vertex_color = input.material_flags.x != 0;
    const float4 color = use_vertex_color ? input.color * diffuse : diffuse;
    const float quantization_bias = input.material_flags.y != 0
        ? 1.0 / 512.0 : 0.0;
    if (color.a < input.alpha_cutoff - quantization_bias)
    {
        discard_fragment();
    }
    const float3 normal = normalize(input.normal);
    const float denominator = abs(normal.x) + abs(normal.y) + abs(normal.z);
    float2 oct = normal.xy / max(denominator, 1.0e-20);
    if (normal.z < 0.0)
    {
        oct = (1.0 - abs(oct.yx)) *
              select(float2(-1.0), float2(1.0), oct >= 0.0);
    }

    ALMetalWorldGBufferOutput output;
    output.albedo = float4(color.rgb, 0.0);
    output.specular_orm = 0.0;
    output.normal = float4(oct * 0.5 + 0.5, 0.0, 0.34);
    output.emissive = 0.0;
    return output;
}

struct ALMetalWorldMaterialSlotConstants
{
    float4 specular_color;
    // x = environment intensity, y = alpha cutoff,
    // z = fullbright/emissive brightness.
    float4 properties;
};

struct ALMetalWorldMaterialConstants
{
    ALMetalWorldMaterialSlotConstants slots[8];
    // x = populated slot count, y = legacy diffuse alpha mode
    // (0 opaque, 2 mask, 3 emissive), z = normal map, w = specular map.
    uint4 config;
};

fragment ALMetalWorldGBufferOutput almetal_world_material_fragment(
    ALMetalWorldSimpleVaryings input [[stage_in]],
    array<texture2d<float>, 8> diffuse_textures [[texture(0)]],
    array<texture2d<float>, 8> normal_textures [[texture(8)]],
    array<texture2d<float>, 8> specular_textures [[texture(16)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant ALMetalWorldMaterialConstants& constants [[buffer(0)]])
{
    const uint slot_count = max(constants.config.x, 1u);
    const uint material_index = min(input.texture_index, slot_count - 1u);
    const ALMetalWorldMaterialSlotConstants material =
        constants.slots[min(material_index, 7u)];
    const float4 diffuse = diffuse_textures[min(material_index, 7u)].sample(
        diffuse_sampler, input.texcoord);

    constexpr uint alpha_mode_mask = 2u;
    constexpr uint alpha_mode_emissive = 3u;
    constexpr float quantization_bias = 1.0 / 512.0;
    if (constants.config.y == alpha_mode_mask &&
        diffuse.a < material.properties.y - quantization_bias)
    {
        discard_fragment();
    }

    const float3 color = diffuse.rgb * input.color.rgb;
    float glossiness = material.specular_color.a;
    float3 normal = normalize(input.normal);
    if (constants.config.z != 0)
    {
        const float4 tangent_sample =
            normal_textures[min(material_index, 7u)].sample(
                diffuse_sampler, input.texcoord1);
        glossiness *= tangent_sample.a;
        const float3 tangent_normal = tangent_sample.rgb * 2.0 - 1.0;
        normal = normalize(
            tangent_normal.x * input.tangent +
            tangent_normal.y * input.bitangent +
            tangent_normal.z * input.normal);
    }

    float4 specular = float4(
        almetal_srgb_to_linear(material.specular_color.rgb), 1.0);
    if (constants.config.w != 0)
    {
        const float4 specular_sample =
            specular_textures[min(material_index, 7u)].sample(
                diffuse_sampler, input.texcoord2);
        specular = float4(
            specular_sample.rgb * specular.rgb, specular_sample.a);
    }

    const float3 du = dfdx(normal);
    const float3 dv = dfdy(normal);
    const float variance = 0.15 * (dot(du, du) + dot(dv, dv));
    const float spec_kernel = min(2.0 * variance, 0.25);
    const float perceptual_roughness = 1.0 - glossiness;
    const float alpha = perceptual_roughness * perceptual_roughness;
    glossiness = 1.0 - sqrt(sqrt(clamp(
        alpha * alpha + spec_kernel, 0.0, 1.0)));
    const float emissive = constants.config.y == alpha_mode_emissive
        ? max(diffuse.a, material.properties.z)
        : material.properties.z;

    const float denominator = abs(normal.x) + abs(normal.y) + abs(normal.z);
    float2 oct = normal.xy / max(denominator, 1.0e-20);
    if (normal.z < 0.0)
    {
        oct = (1.0 - abs(oct.yx)) *
              select(float2(-1.0), float2(1.0), oct >= 0.0);
    }

    ALMetalWorldGBufferOutput output;
    output.albedo = max(float4(color, emissive), float4(0.0));
    // The legacy material G-buffer stores specular RGB encoded in its plain
    // RGBA8 attachment. Specular maps and tint multiply after hardware sRGB
    // decode, then return to encoded storage exactly like materialF.glsl.
    const float3 encoded_specular = constants.config.w != 0
        ? almetal_linear_to_srgb(specular.rgb)
        : material.specular_color.rgb;
    output.specular_orm = max(
        float4(encoded_specular, glossiness), float4(0.0));
    output.normal = float4(oct * 0.5 + 0.5,
                           material.properties.x * specular.a, 0.34);
    output.emissive = 0.0;
    return output;
}

fragment ALMetalWorldGBufferOutput almetal_world_bump_fragment(
    ALMetalWorldSimpleVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    texture2d<float> normal_texture [[texture(32)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    const float4 diffuse = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord)
        : float4(1.0, 0.0, 1.0, 1.0);
    const float quantization_bias = input.material_flags.y != 0
        ? 1.0 / 512.0 : 0.0;
    if (input.material_flags.z != 0 &&
        diffuse.a < input.alpha_cutoff - quantization_bias)
    {
        discard_fragment();
    }
    const float4 color = input.color * diffuse;
    const float3 tangent_normal =
        normal_texture.sample(diffuse_sampler, input.texcoord).rgb * 2.0 - 1.0;
    const float3 normal = normalize(
        tangent_normal.x * input.tangent +
        tangent_normal.y * input.bitangent +
        tangent_normal.z * input.normal);

    constexpr float spec_aa_variance = 0.15;
    constexpr float spec_aa_threshold = 0.25;
    const float3 du = dfdx(normal);
    const float3 dv = dfdy(normal);
    const float variance = spec_aa_variance *
        (dot(du, du) + dot(dv, dv));
    const float spec_kernel = min(2.0 * variance, spec_aa_threshold);
    const float perceptual_roughness = 1.0 - input.color.a;
    const float alpha = perceptual_roughness * perceptual_roughness;
    const float filtered_roughness = sqrt(sqrt(clamp(
        alpha * alpha + spec_kernel, 0.0, 1.0)));
    const float glossiness = 1.0 - filtered_roughness;

    const float denominator = abs(normal.x) + abs(normal.y) + abs(normal.z);
    float2 oct = normal.xy / max(denominator, 1.0e-20);
    if (normal.z < 0.0)
    {
        oct = (1.0 - abs(oct.yx)) *
              select(float2(-1.0), float2(1.0), oct >= 0.0);
    }

    ALMetalWorldGBufferOutput output;
    output.albedo = float4(color.rgb, 0.0);
    output.specular_orm = float4(input.color.aaa, glossiness);
    output.normal = float4(oct * 0.5 + 0.5,
                           input.color.a, 0.34);
    output.emissive = 0.0;
    return output;
}

struct ALMetalWorldTerrainPBRSlot
{
    float4 uv_transform;
    float4 uv_offset;
    float4 normal_axes;
    float4 base_color_factor;
    float4 emissive_color;
    float4 properties;
};

struct ALMetalWorldTerrainConstants
{
    float4x4 modelview_projection;
    float4x4 modelview;
    float4 normal_matrix[3];
    // x = metres/grid, y = region width, z = detail scale,
    // w = triplanar sharpness.
    float4 surface;
    // xy = detail phase, z = smooth surface, w = triplanar mapping.
    float4 detail;
    // x = reverse-Z, y = parcel overlay present, z = hex tiling,
    // w = high-precision normal target.
    uint4 config;
    // x = PBR detail + 4, y = paint type, z = alpha mask,
    // w = faceted geometric normals.
    uint4 pbr_config;
    float4 hex_fraction[8];
    int4 hex_base[8];
    ALMetalWorldTerrainPBRSlot pbr_slots[4];
};

struct ALMetalWorldTerrainSurface
{
    float height;
    float2 gradient;
};

static float almetal_world_terrain_height_at(
    texture2d<float> height_map, int2 grid)
{
    constexpr int apron = 2;
    const int2 extent = int2(height_map.get_width(), height_map.get_height());
    const int2 texel = clamp(grid + apron, int2(0), extent - 1);
    return height_map.read(uint2(texel)).r;
}

static float almetal_world_terrain_height_linear(
    texture2d<float> height_map, float2 grid)
{
    const int2 cell = int2(floor(grid));
    const float2 fraction = grid - float2(cell);
    const float lower_left = almetal_world_terrain_height_at(height_map, cell);
    const float lower_right = almetal_world_terrain_height_at(
        height_map, cell + int2(1, 0));
    const float upper_left = almetal_world_terrain_height_at(
        height_map, cell + int2(0, 1));
    const float upper_right = almetal_world_terrain_height_at(
        height_map, cell + int2(1, 1));
    return fraction.y > fraction.x
        ? lower_left + fraction.y * (upper_left - lower_left) +
              fraction.x * (upper_right - upper_left)
        : lower_left + fraction.x * (lower_right - lower_left) +
              fraction.y * (upper_right - lower_right);
}

static float3 almetal_world_terrain_normal_linear(
    texture2d<float> height_map, float2 grid, float grid_scale)
{
    const float extent = 2.0 * grid_scale;
    const float3 p00 = float3(
        -extent, -extent,
        almetal_world_terrain_height_linear(height_map, grid - 2.0));
    const float3 p01 = float3(
        -extent, extent,
        almetal_world_terrain_height_linear(
            height_map, grid + float2(-2.0, 2.0)));
    const float3 p10 = float3(
        extent, -extent,
        almetal_world_terrain_height_linear(
            height_map, grid + float2(2.0, -2.0)));
    const float3 p11 = float3(
        extent, extent,
        almetal_world_terrain_height_linear(height_map, grid + 2.0));
    return normalize(cross(p11 - p00, p01 - p10));
}

static float almetal_world_terrain_limited_slope(
    float left, float middle, float right)
{
    const float dl = middle - left;
    const float dr = right - middle;
    if (dl * dr <= 0.0)
    {
        return 0.0;
    }
    const float slope = 0.5 * (dl + dr);
    const float bound = 3.0 * min(abs(dl), abs(dr));
    return clamp(slope, -bound, bound);
}

static float4 almetal_world_terrain_hermite(float value)
{
    const float squared = value * value;
    const float cubed = squared * value;
    return float4(
         2.0 * cubed - 3.0 * squared + 1.0,
        -2.0 * cubed + 3.0 * squared,
               cubed - 2.0 * squared + value,
               cubed - squared);
}

static float4 almetal_world_terrain_hermite_derivative(float value)
{
    const float squared = value * value;
    return float4(
         6.0 * squared - 6.0 * value,
        -6.0 * squared + 6.0 * value,
         3.0 * squared - 4.0 * value + 1.0,
         3.0 * squared - 2.0 * value);
}

static ALMetalWorldTerrainSurface almetal_world_terrain_height_smooth(
    texture2d<float> height_map, float2 grid)
{
    const int2 cell = int2(floor(grid));
    const float2 fraction = grid - float2(cell);

    float row0[4];
    float row1[4];
    for (int column = 0; column < 4; ++column)
    {
        row0[column] = almetal_world_terrain_height_at(
            height_map, cell + int2(column - 1, 0));
        row1[column] = almetal_world_terrain_height_at(
            height_map, cell + int2(column - 1, 1));
    }
    const float c0m = almetal_world_terrain_height_at(
        height_map, cell + int2(0, -1));
    const float c0p = almetal_world_terrain_height_at(
        height_map, cell + int2(0, 2));
    const float c1m = almetal_world_terrain_height_at(
        height_map, cell + int2(1, -1));
    const float c1p = almetal_world_terrain_height_at(
        height_map, cell + int2(1, 2));

    const float f00 = row0[1];
    const float f10 = row0[2];
    const float f01 = row1[1];
    const float f11 = row1[2];
    const float fx00 = almetal_world_terrain_limited_slope(
        row0[0], f00, f10);
    const float fx10 = almetal_world_terrain_limited_slope(
        f00, f10, row0[3]);
    const float fx01 = almetal_world_terrain_limited_slope(
        row1[0], f01, f11);
    const float fx11 = almetal_world_terrain_limited_slope(
        f01, f11, row1[3]);
    const float fy00 = almetal_world_terrain_limited_slope(c0m, f00, f01);
    const float fy01 = almetal_world_terrain_limited_slope(f00, f01, c0p);
    const float fy10 = almetal_world_terrain_limited_slope(c1m, f10, f11);
    const float fy11 = almetal_world_terrain_limited_slope(f10, f11, c1p);

    const float4 hu = almetal_world_terrain_hermite(fraction.x);
    const float4 hv = almetal_world_terrain_hermite(fraction.y);
    const float4 du = almetal_world_terrain_hermite_derivative(fraction.x);
    const float4 dv = almetal_world_terrain_hermite_derivative(fraction.y);

    ALMetalWorldTerrainSurface result;
    result.height =
        hu.x * (hv.x * f00 + hv.y * f01) +
        hu.y * (hv.x * f10 + hv.y * f11) +
        hu.z * (hv.x * fx00 + hv.y * fx01) +
        hu.w * (hv.x * fx10 + hv.y * fx11) +
        hu.x * (hv.z * fy00 + hv.w * fy01) +
        hu.y * (hv.z * fy10 + hv.w * fy11);
    result.gradient.x =
        du.x * (hv.x * f00 + hv.y * f01) +
        du.y * (hv.x * f10 + hv.y * f11) +
        du.z * (hv.x * fx00 + hv.y * fx01) +
        du.w * (hv.x * fx10 + hv.y * fx11) +
        du.x * (hv.z * fy00 + hv.w * fy01) +
        du.y * (hv.z * fy10 + hv.w * fy11);
    result.gradient.y =
        hu.x * (dv.x * f00 + dv.y * f01) +
        hu.y * (dv.x * f10 + dv.y * f11) +
        hu.z * (dv.x * fx00 + dv.y * fx01) +
        hu.w * (dv.x * fx10 + dv.y * fx11) +
        hu.x * (dv.z * fy00 + dv.w * fy01) +
        hu.y * (dv.z * fy10 + dv.w * fy11);
    result.height = clamp(
        result.height,
        min(min(f00, f10), min(f01, f11)),
        max(max(f00, f10), max(f01, f11)));
    return result;
}

static float3 almetal_world_terrain_surface(
    texture2d<float> height_map, float2 region_position,
    float grid_scale, bool smoothing, thread float3& normal)
{
    const float2 grid = region_position / grid_scale;
    if (smoothing)
    {
        const ALMetalWorldTerrainSurface surface =
            almetal_world_terrain_height_smooth(height_map, grid);
        normal = normalize(float3(-surface.gradient / grid_scale, 1.0));
        return float3(region_position, surface.height);
    }
    normal = almetal_world_terrain_normal_linear(
        height_map, grid, grid_scale);
    return float3(
        region_position,
        almetal_world_terrain_height_linear(height_map, grid));
}

struct ALMetalWorldTerrainVaryings
{
    float4 position [[position]];
    float3 view_position;
    float3 normal;
    float3 region_position;
    float2 detail_texcoord;
    float4 side_texcoord;
    float2 composition;
    float2 region_texcoord;
};

[[patch(quad, 4)]]
vertex ALMetalWorldTerrainVaryings almetal_world_terrain_vertex(
    const device float4* control_points [[buffer(0)]],
    constant ALMetalWorldTerrainConstants& constants [[buffer(4)]],
    texture2d<float> height_map [[texture(0)]],
    texture2d<float> composition_map [[texture(1)]],
    sampler composition_sampler [[sampler(2)]],
    uint patch_id [[patch_id]],
    float2 patch_position [[position_in_patch]])
{
    const uint base = patch_id * 4;
    const float2 bottom = mix(
        control_points[base].xy, control_points[base + 1].xy,
        patch_position.x);
    const float2 top = mix(
        control_points[base + 3].xy, control_points[base + 2].xy,
        patch_position.x);
    const float2 xy = mix(bottom, top, patch_position.y);

    float3 region_normal;
    const float3 region_position = almetal_world_terrain_surface(
        height_map, xy, constants.surface.x, constants.detail.z != 0.0,
        region_normal);
    float4 clip_position = constants.modelview_projection *
                           float4(region_position, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldTerrainVaryings output;
    output.position = constants.config.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    output.normal = normalize(normal_matrix * region_normal);
    output.view_position =
        (constants.modelview * float4(region_position, 1.0)).xyz;
    output.region_position = region_position;
    output.detail_texcoord =
        region_position.xy * constants.surface.z + constants.detail.xy;
    output.side_texcoord = float4(
        region_position.y * constants.surface.z + constants.detail.y,
        region_position.z * constants.surface.z,
        region_position.x * constants.surface.z + constants.detail.x,
        region_position.z * constants.surface.z);
    constexpr float apron = 2.0;
    const float2 composition_texcoord =
        (xy / constants.surface.x + apron + 0.5) /
        float2(composition_map.get_width(), composition_map.get_height());
    output.composition = composition_map.sample(
        composition_sampler, composition_texcoord).rg;
    output.region_texcoord = xy / constants.surface.y;
    return output;
}

struct ALMetalWorldTerrainProjection
{
    float3 weight;
    float3 usage;
    float sx;
    float sy;
    float2 top_dx;
    float2 top_dy;
    float4 side_dx;
    float4 side_dy;
};

static ALMetalWorldTerrainProjection almetal_world_terrain_projection(
    ALMetalWorldTerrainVaryings input,
    texture2d<float> height_map,
    constant ALMetalWorldTerrainConstants& constants)
{
    ALMetalWorldTerrainProjection projection;
    projection.top_dx = dfdx(input.detail_texcoord);
    projection.top_dy = dfdy(input.detail_texcoord);
    projection.side_dx = dfdx(input.side_texcoord);
    projection.side_dy = dfdy(input.side_texcoord);
    projection.weight = float3(0.0, 0.0, 1.0);
    projection.usage = float3(0.0, 0.0, 1.0);
    projection.sx = 1.0;
    projection.sy = 1.0;
    if (constants.detail.w != 0.0)
    {
        const ALMetalWorldTerrainSurface facet =
            almetal_world_terrain_height_smooth(
                height_map,
                input.region_position.xy / constants.surface.x);
        const float3 facet_normal = normalize(float3(
            -facet.gradient / constants.surface.x, 1.0));
        float3 signed_weight = pow(
            abs(facet_normal), constants.surface.w);
        signed_weight /= dot(signed_weight, float3(1.0));
        signed_weight -= 0.01;
        projection.weight = max(signed_weight, 0.0);
        projection.weight /= dot(projection.weight, float3(1.0));
        projection.usage = select(
            float3(0.0), float3(1.0), signed_weight > 0.0);
        projection.sx = facet_normal.x > 0.0 ? 1.0 : -1.0;
        projection.sy = facet_normal.y > 0.0 ? 1.0 : -1.0;
    }
    return projection;
}

struct ALMetalWorldTerrainHexPlace
{
    float2 fraction;
    int2 base;
};

struct ALMetalWorldTerrainHexTile
{
    int2 corner[3];
    int2 base;
    float2 q;
    float3 weight;
    uint type;
};

static float2 almetal_world_terrain_hex_skew(float2 point)
{
    return float2(
        point.x - 0.57735027 * point.y,
        1.15470054 * point.y);
}

static void almetal_world_terrain_hex_triangle(
    float2 fraction, thread int2& corner0, thread int2& corner1,
    thread int2& corner2, thread float3& barycentric)
{
    const int upper = fraction.x + fraction.y >= 1.0 ? 1 : 0;
    const float flip = float(upper) * 2.0 - 1.0;
    corner0 = int2(upper);
    corner1 = int2(upper, 1 - upper);
    corner2 = int2(1 - upper, upper);
    barycentric = max(float3(0.0), float3(
        (fraction.x + fraction.y - 1.0) * flip,
        float(upper) - fraction.y * flip,
        float(upper) - fraction.x * flip));
}

static uint almetal_world_terrain_hex_sharpen(
    float3 barycentric, thread float3& weight)
{
    float3 sharpened = pow(barycentric, 7.0);
    sharpened /= dot(sharpened, float3(1.0));
    sharpened -= 0.01;
    const uint3 usage = select(
        uint3(0), uint3(1), sharpened > 0.0);
    weight = max(float3(0.0), sharpened);
    weight /= dot(weight, float3(1.0));
    return usage.x | (usage.y << 1) | (usage.z << 2);
}

static uint almetal_world_terrain_hex_hash(uint2 identifier)
{
    uint hash = identifier.x * 0x8da6b343u +
                identifier.y * 0xd8163841u;
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    return hash;
}

static ALMetalWorldTerrainHexPlace almetal_world_terrain_hex_place(
    constant ALMetalWorldTerrainConstants& constants,
    uint material, uint projection, float side)
{
    const uint element = 2 * material + (projection >> 1);
    const bool upper = (projection & 1) != 0;
    ALMetalWorldTerrainHexPlace place;
    place.fraction = upper
        ? constants.hex_fraction[element].zw
        : constants.hex_fraction[element].xy;
    place.base = upper
        ? constants.hex_base[element].zw
        : constants.hex_base[element].xy;
    if (side < 0.0)
    {
        // -(base + fraction) floors one cell lower. Express -base-1 as
        // an unsigned complement so INT_MIN retains its defined low bits.
        place.base = as_type<int2>(~as_type<uint2>(place.base));
        place.fraction = 1.0 - place.fraction;
    }
    return place;
}

static ALMetalWorldTerrainHexTile almetal_world_terrain_hex_tile(
    float2 uv, ALMetalWorldTerrainHexPlace place)
{
    constexpr float grid_scale = 3.4641016;
    const float2 q = place.fraction +
        almetal_world_terrain_hex_skew(uv * grid_scale);
    const float2 cell_float = floor(q);
    const int2 cell = int2(cell_float);
    int2 corner0;
    int2 corner1;
    int2 corner2;
    float3 barycentric;
    almetal_world_terrain_hex_triangle(
        q - cell_float, corner0, corner1, corner2, barycentric);

    ALMetalWorldTerrainHexTile tile;
    tile.corner[0] = cell + corner0;
    tile.corner[1] = cell + corner1;
    tile.corner[2] = cell + corner2;
    tile.base = place.base;
    tile.q = q;
    tile.type = almetal_world_terrain_hex_sharpen(
        barycentric, tile.weight);
    return tile;
}

static float2x2 almetal_world_terrain_hex_cell(
    ALMetalWorldTerrainHexTile tile, uint cell,
    thread float2& texcoord)
{
    const uint hash = almetal_world_terrain_hex_hash(
        uint2(tile.base) + uint2(tile.corner[cell]));
    const float2 offset = float2(
        hash & 0x7ffu, (hash >> 11) & 0x7ffu) * (1.0 / 2048.0);
    const float angle =
        (float(hash >> 22) * (1.0 / 512.0) - 1.0) * M_PI_F;
    const float cosine = cos(angle);
    const float sine = sin(angle);
    const float2x2 rotation = float2x2(
        float2(cosine, sine), float2(-sine, cosine));
    const float2 lattice = tile.q - float2(tile.corner[cell]);
    const float2 from_center = float2(
        lattice.x + 0.5 * lattice.y,
        0.8660254 * lattice.y) * (1.0 / 3.4641016);
    texcoord = rotation * from_center + offset;
    return rotation;
}

static float4 almetal_world_terrain_detail_fetch(
    texture2d<float> detail_texture, sampler detail_sampler,
    float2 texcoord, float2 texcoord_dx, float2 texcoord_dy,
    uint projection, float side,
    constant ALMetalWorldTerrainConstants& constants)
{
    if (constants.config.z == 0)
    {
        return detail_texture.sample(
            detail_sampler, texcoord,
            gradient2d(texcoord_dx, texcoord_dy));
    }

    const ALMetalWorldTerrainHexTile tile =
        almetal_world_terrain_hex_tile(
            texcoord,
            almetal_world_terrain_hex_place(
                constants, 0, projection, side));
    float4 cells[3] = {};
    float3 luma = 0.0;
    for (uint cell = 0; cell < 3; ++cell)
    {
        if ((tile.type & (1u << cell)) != 0)
        {
            float2 cell_texcoord;
            const float2x2 rotation = almetal_world_terrain_hex_cell(
                tile, cell, cell_texcoord);
            cells[cell] = detail_texture.sample(
                detail_sampler, cell_texcoord,
                gradient2d(rotation * texcoord_dx,
                           rotation * texcoord_dy));
            luma[cell] = dot(cells[cell].rgb,
                             float3(0.299, 0.587, 0.114));
        }
    }
    float3 weight = tile.weight * mix(float3(1.0), luma, 0.6);
    weight /= dot(weight, float3(1.0));
    return cells[0] * weight.x + cells[1] * weight.y +
           cells[2] * weight.z;
}

static float4 almetal_world_terrain_detail_sample(
    texture2d<float> detail_texture,
    sampler detail_sampler,
    ALMetalWorldTerrainVaryings input,
    ALMetalWorldTerrainProjection projection,
    constant ALMetalWorldTerrainConstants& constants)
{
    float4 color = 0.0;
    if (projection.usage.x != 0.0)
    {
        const float2 flip = float2(projection.sx, 1.0);
        color += almetal_world_terrain_detail_fetch(
            detail_texture, detail_sampler,
            flip * input.side_texcoord.xy,
            flip * projection.side_dx.xy,
            flip * projection.side_dy.xy,
            1, projection.sx, constants) * projection.weight.x;
    }
    if (projection.usage.y != 0.0)
    {
        const float2 flip = float2(-projection.sy, 1.0);
        color += almetal_world_terrain_detail_fetch(
            detail_texture, detail_sampler,
            flip * input.side_texcoord.zw,
            flip * projection.side_dx.zw,
            flip * projection.side_dy.zw,
            2, -projection.sy, constants) * projection.weight.y;
    }
    if (projection.usage.z != 0.0)
    {
        color += almetal_world_terrain_detail_fetch(
            detail_texture, detail_sampler, input.detail_texcoord,
            projection.top_dx, projection.top_dy,
            0, 1.0, constants) * projection.weight.z;
    }
    return color;
}

fragment ALMetalWorldGBufferOutput almetal_world_terrain_fragment(
    ALMetalWorldTerrainVaryings input [[stage_in]],
    array<texture2d<float>, 4> detail_textures [[texture(0)]],
    texture2d<float> height_map [[texture(4)]],
    texture2d<float> alpha_ramp [[texture(5)]],
    texture2d<float> parcel_overlay [[texture(6)]],
    sampler detail_sampler [[sampler(0)]],
    sampler parcel_sampler [[sampler(1)]],
    sampler ramp_sampler [[sampler(3)]],
    constant ALMetalWorldTerrainConstants& constants [[buffer(0)]])
{
    const float alpha1 = alpha_ramp.sample(
        ramp_sampler, input.composition).a;
    const float alpha2 = alpha_ramp.sample(
        ramp_sampler, input.composition - float2(2.0, 0.0)).a;
    const float alpha_final = alpha_ramp.sample(
        ramp_sampler, input.composition - float2(1.0, 0.0)).a;
    float4 weight = mix(
        mix(float4(0.0, 0.0, 0.0, 1.0),
            float4(0.0, 0.0, 1.0, 0.0), alpha2),
        mix(float4(0.0, 1.0, 0.0, 0.0),
            float4(1.0, 0.0, 0.0, 0.0), alpha1),
        alpha_final) - 0.1;
    const float4 usage = select(float4(0.0), float4(1.0), weight > 0.0);
    weight *= usage;
    weight /= dot(weight, float4(1.0));

    const ALMetalWorldTerrainProjection projection =
        almetal_world_terrain_projection(input, height_map, constants);
    float4 color = 0.0;
    for (uint detail = 0; detail < 4; ++detail)
    {
        if (usage[detail] != 0.0)
        {
            color += almetal_world_terrain_detail_sample(
                detail_textures[detail], detail_sampler, input,
                projection, constants) * weight[detail];
        }
    }
    if (constants.config.y != 0)
    {
        const float4 overlay = parcel_overlay.sample(
            parcel_sampler, input.region_texcoord);
        color.rgb = mix(color.rgb, overlay.rgb, overlay.a);
    }
    color.a = 0.0;

    ALMetalWorldGBufferOutput output;
    output.albedo = max(color, 0.0);
    output.specular_orm = 0.0;
    output.normal = float4(
        almetal_world_oct_encode(normalize(input.normal)), 0.0, 0.34);
    output.emissive = 0.0;
    return output;
}

struct ALMetalWorldTerrainPBRMix
{
    float4 color;
    float3 orm;
    float3 normal;
    float3 emissive;
};

static ALMetalWorldTerrainPBRMix almetal_world_terrain_pbr_zero()
{
    ALMetalWorldTerrainPBRMix result;
    result.color = 0.0;
    result.orm = 0.0;
    result.normal = 0.0;
    result.emissive = 0.0;
    return result;
}

static ALMetalWorldTerrainPBRMix almetal_world_terrain_pbr_add(
    ALMetalWorldTerrainPBRMix destination,
    ALMetalWorldTerrainPBRMix source, float weight)
{
    destination.color += source.color * weight;
    destination.orm += source.orm * weight;
    destination.normal += source.normal * weight;
    destination.emissive += source.emissive * weight;
    return destination;
}

static ALMetalWorldTerrainPBRMix almetal_world_terrain_pbr_fetch(
    texture2d<float> base_color_texture,
    texture2d<float> normal_texture,
    texture2d<float> orm_texture,
    texture2d<float> emissive_texture,
    sampler material_sampler, float2 texcoord,
    float2 texcoord_dx, float2 texcoord_dy, uint detail)
{
    ALMetalWorldTerrainPBRMix result;
    result.color = base_color_texture.sample(
        material_sampler, texcoord,
        gradient2d(texcoord_dx, texcoord_dy));
    if (detail >= 1)
    {
        result.orm = orm_texture.sample(
            material_sampler, texcoord,
            gradient2d(texcoord_dx, texcoord_dy)).rgb;
        if (detail < 3)
        {
            result.orm.r = 1.0;
        }
    }
    else
    {
        result.orm = float3(1.0, 1.0, 0.0);
    }
    result.normal = detail >= 2
        ? normal_texture.sample(
              material_sampler, texcoord,
              gradient2d(texcoord_dx, texcoord_dy)).rgb * 2.0 - 1.0
        : float3(0.0, 0.0, 1.0);
    result.emissive = detail >= 4
        ? emissive_texture.sample(
              material_sampler, texcoord,
              gradient2d(texcoord_dx, texcoord_dy)).rgb
        : float3(0.0);
    return result;
}

static ALMetalWorldTerrainPBRMix almetal_world_terrain_pbr_sample(
    texture2d<float> base_color_texture,
    texture2d<float> normal_texture,
    texture2d<float> orm_texture,
    texture2d<float> emissive_texture,
    sampler material_sampler, float2 texcoord,
    float2 texcoord_dx, float2 texcoord_dy,
    uint material, uint projection, float side,
    constant ALMetalWorldTerrainConstants& constants)
{
    const uint detail = constants.pbr_config.x;
    if (constants.config.z == 0)
    {
        return almetal_world_terrain_pbr_fetch(
            base_color_texture, normal_texture, orm_texture,
            emissive_texture, material_sampler, texcoord,
            texcoord_dx, texcoord_dy, detail);
    }

    const ALMetalWorldTerrainHexTile tile =
        almetal_world_terrain_hex_tile(
            texcoord,
            almetal_world_terrain_hex_place(
                constants, material, projection, side));
    ALMetalWorldTerrainPBRMix cells[3];
    float2 slopes[3];
    float3 luma = 0.0;
    for (uint cell = 0; cell < 3; ++cell)
    {
        cells[cell] = almetal_world_terrain_pbr_zero();
        slopes[cell] = 0.0;
        if ((tile.type & (1u << cell)) != 0)
        {
            float2 cell_texcoord;
            const float2x2 rotation = almetal_world_terrain_hex_cell(
                tile, cell, cell_texcoord);
            cells[cell] = almetal_world_terrain_pbr_fetch(
                base_color_texture, normal_texture, orm_texture,
                emissive_texture, material_sampler, cell_texcoord,
                rotation * texcoord_dx, rotation * texcoord_dy, detail);
            luma[cell] = dot(cells[cell].color.rgb,
                             float3(0.299, 0.587, 0.114));
            if (detail >= 2)
            {
                slopes[cell] =
                    (cells[cell].normal.xy * rotation) /
                    max(cells[cell].normal.z, 0.015625);
            }
        }
    }
    float3 weight = tile.weight * mix(float3(1.0), luma, 0.6);
    weight /= dot(weight, float3(1.0));
    ALMetalWorldTerrainPBRMix result =
        almetal_world_terrain_pbr_zero();
    result = almetal_world_terrain_pbr_add(result, cells[0], weight.x);
    result = almetal_world_terrain_pbr_add(result, cells[1], weight.y);
    result = almetal_world_terrain_pbr_add(result, cells[2], weight.z);
    if (detail >= 2)
    {
        result.normal = float3(
            slopes[0] * weight.x + slopes[1] * weight.y +
            slopes[2] * weight.z, 1.0);
    }
    return result;
}

static float3 almetal_world_terrain_pbr_compose_normal(
    float3 sampled, float2x2 axes, float3 geometric)
{
    sampled.xy = axes * sampled.xy;
    return normalize(float3(
        sampled.xy * geometric.z + geometric.xy * sampled.z,
        sampled.z * geometric.z));
}

static ALMetalWorldTerrainPBRMix
almetal_world_terrain_pbr_sample_material(
    ALMetalWorldTerrainVaryings input,
    ALMetalWorldTerrainProjection projection,
    float3 point_dx, float3 point_dy,
    float3 geometric_normal_region,
    texture2d<float> base_color_texture,
    texture2d<float> normal_texture,
    texture2d<float> orm_texture,
    texture2d<float> emissive_texture,
    sampler material_sampler, uint material,
    constant ALMetalWorldTerrainConstants& constants)
{
    const ALMetalWorldTerrainPBRSlot slot = constants.pbr_slots[material];
    const float2x2 transform = float2x2(
        slot.uv_transform.xy, slot.uv_transform.zw);
    const float2x2 normal_axes = float2x2(
        slot.normal_axes.xy, slot.normal_axes.zw);
    const float2 offset = slot.uv_offset.xy;
    const uint detail = constants.pbr_config.x;
    ALMetalWorldTerrainPBRMix result =
        almetal_world_terrain_pbr_zero();

    if (constants.detail.w == 0.0)
    {
        result = almetal_world_terrain_pbr_sample(
            base_color_texture, normal_texture, orm_texture,
            emissive_texture, material_sampler,
            transform * input.region_position.xy + offset,
            transform * point_dx.xy, transform * point_dy.xy,
            material, 0, 1.0, constants);
        if (detail >= 2)
        {
            result.normal = almetal_world_terrain_pbr_compose_normal(
                result.normal, normal_axes, geometric_normal_region);
        }
    }
    else
    {
        if (projection.usage.x != 0.0)
        {
            const float sx = projection.sx;
            ALMetalWorldTerrainPBRMix sample =
                almetal_world_terrain_pbr_sample(
                    base_color_texture, normal_texture, orm_texture,
                    emissive_texture, material_sampler,
                    transform * float2(
                        sx * input.region_position.y,
                        input.region_position.z) + offset,
                    transform * float2(sx * point_dx.y, point_dx.z),
                    transform * float2(sx * point_dy.y, point_dy.z),
                    material, 1, sx, constants);
            if (detail >= 2)
            {
                const float3 frame_normal = float3(
                    sx * geometric_normal_region.y,
                    geometric_normal_region.z,
                    sx * geometric_normal_region.x);
                const float3 composed =
                    almetal_world_terrain_pbr_compose_normal(
                        sample.normal, normal_axes, frame_normal);
                sample.normal = float3(
                    sx * composed.z, sx * composed.x, composed.y);
            }
            result = almetal_world_terrain_pbr_add(
                result, sample, projection.weight.x);
        }
        if (projection.usage.y != 0.0)
        {
            const float sy = projection.sy;
            ALMetalWorldTerrainPBRMix sample =
                almetal_world_terrain_pbr_sample(
                    base_color_texture, normal_texture, orm_texture,
                    emissive_texture, material_sampler,
                    transform * float2(
                        -sy * input.region_position.x,
                        input.region_position.z) + offset,
                    transform * float2(-sy * point_dx.x, point_dx.z),
                    transform * float2(-sy * point_dy.x, point_dy.z),
                    material, 2, -sy, constants);
            if (detail >= 2)
            {
                const float3 frame_normal = float3(
                    -sy * geometric_normal_region.x,
                    geometric_normal_region.z,
                    sy * geometric_normal_region.y);
                const float3 composed =
                    almetal_world_terrain_pbr_compose_normal(
                        sample.normal, normal_axes, frame_normal);
                sample.normal = float3(
                    -sy * composed.x, sy * composed.z, composed.y);
            }
            result = almetal_world_terrain_pbr_add(
                result, sample, projection.weight.y);
        }
        if (projection.usage.z != 0.0)
        {
            ALMetalWorldTerrainPBRMix sample =
                almetal_world_terrain_pbr_sample(
                    base_color_texture, normal_texture, orm_texture,
                    emissive_texture, material_sampler,
                    transform * input.region_position.xy + offset,
                    transform * point_dx.xy, transform * point_dy.xy,
                    material, 0, 1.0, constants);
            if (detail >= 2)
            {
                sample.normal =
                    almetal_world_terrain_pbr_compose_normal(
                        sample.normal, normal_axes,
                        geometric_normal_region);
            }
            result = almetal_world_terrain_pbr_add(
                result, sample, projection.weight.z);
        }
        if (detail >= 2)
        {
            result.normal = normalize(result.normal);
        }
    }

    result.color *= slot.base_color_factor;
    if (detail >= 1)
    {
        result.orm.g *= slot.properties.x;
        result.orm.b *= slot.properties.y;
    }
    if (detail >= 4)
    {
        result.emissive *= slot.emissive_color.rgb;
    }
    return result;
}

fragment ALMetalWorldGBufferOutput almetal_world_pbr_terrain_fragment(
    ALMetalWorldTerrainVaryings input [[stage_in]],
    array<texture2d<float>, 4> base_color_textures [[texture(0)]],
    array<texture2d<float>, 4> normal_textures [[texture(4)]],
    array<texture2d<float>, 4> orm_textures [[texture(8)]],
    array<texture2d<float>, 4> emissive_textures [[texture(12)]],
    texture2d<float> height_map [[texture(16)]],
    texture2d<float> blend_map [[texture(17)]],
    texture2d<float> parcel_overlay [[texture(18)]],
    sampler material_sampler [[sampler(0)]],
    sampler parcel_sampler [[sampler(1)]],
    sampler blend_sampler [[sampler(3)]],
    constant ALMetalWorldTerrainConstants& constants [[buffer(0)]],
    bool front_facing [[front_facing]])
{
    const float3 point_dx = dfdx(input.region_position);
    const float3 point_dy = dfdy(input.region_position);
    float3 geometric_normal = normalize(input.normal);
    if (constants.pbr_config.w != 0)
    {
        float3 triangle_normal = normalize(cross(
            dfdx(input.view_position), dfdy(input.view_position)));
        if (dot(triangle_normal, geometric_normal) < 0.0)
        {
            triangle_normal = -triangle_normal;
        }
        geometric_normal = triangle_normal;
    }
    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    const float3 geometric_normal_region =
        transpose(normal_matrix) * geometric_normal;

    float4 material_weight;
    if (constants.pbr_config.y == 0)
    {
        const float alpha1 = blend_map.sample(
            blend_sampler, input.composition).a;
        const float alpha2 = blend_map.sample(
            blend_sampler, input.composition - float2(2.0, 0.0)).a;
        const float alpha_final = blend_map.sample(
            blend_sampler, input.composition - float2(1.0, 0.0)).a;
        material_weight = mix(
            mix(float4(0.0, 0.0, 0.0, 1.0),
                float4(0.0, 0.0, 1.0, 0.0), alpha2),
            mix(float4(0.0, 1.0, 0.0, 0.0),
                float4(1.0, 0.0, 0.0, 0.0), alpha1),
            alpha_final) - 0.1;
        material_weight = max(material_weight, 0.0);
        material_weight /= dot(material_weight, float4(1.0));
    }
    else
    {
        float3 stored_weight = max(
            blend_map.sample(
                blend_sampler, input.region_texcoord).rgb,
            float3(0.0));
        stored_weight /= max(dot(stored_weight, float3(1.0)), 1.0);
        material_weight = float4(
            1.0 - dot(stored_weight, float3(1.0)), stored_weight);
    }

    const ALMetalWorldTerrainProjection projection =
        almetal_world_terrain_projection(input, height_map, constants);
    ALMetalWorldTerrainPBRMix mixed =
        almetal_world_terrain_pbr_zero();
    float coverage = 0.0;
    float minimum_alpha = 0.0;
    for (uint material = 0; material < 4; ++material)
    {
        if (material_weight[material] > 0.0)
        {
            const ALMetalWorldTerrainPBRMix sample =
                almetal_world_terrain_pbr_sample_material(
                    input, projection, point_dx, point_dy,
                    geometric_normal_region,
                    base_color_textures[material],
                    normal_textures[material], orm_textures[material],
                    emissive_textures[material], material_sampler,
                    material, constants);
            mixed = almetal_world_terrain_pbr_add(
                mixed, sample, material_weight[material]);
            const float cutoff =
                constants.pbr_slots[material].properties.z;
            coverage += material_weight[material] *
                (cutoff < 0.0 ? 1.0 : sample.color.a);
            minimum_alpha += material_weight[material] * max(cutoff, 0.0);
        }
    }
    constexpr float quantization_bias = 1.0 / 512.0;
    if (constants.pbr_config.z != 0 &&
        coverage < minimum_alpha - quantization_bias)
    {
        discard_fragment();
    }

    float3 normal = constants.pbr_config.x >= 2
        ? normalize(normal_matrix * mixed.normal)
        : geometric_normal;
    float3 output_geometric_normal = geometric_normal;
    if (!front_facing)
    {
        normal = -normal;
        output_geometric_normal = -output_geometric_normal;
    }
    const float3 normal_dx = dfdx(normal);
    const float3 normal_dy = dfdy(normal);
    const float variance = 0.15 *
        (dot(normal_dx, normal_dx) + dot(normal_dy, normal_dy));
    const float roughness_kernel = min(2.0 * variance, 0.25);
    const float alpha = mixed.orm.g * mixed.orm.g;
    mixed.orm.g = sqrt(sqrt(clamp(
        alpha * alpha + roughness_kernel, 0.0, 1.0)));

    if (constants.config.y != 0)
    {
        const float4 overlay = parcel_overlay.sample(
            parcel_sampler, input.region_texcoord);
        mixed.color.rgb = mix(
            mixed.color.rgb, overlay.rgb, overlay.a);
    }

    const float roughness_high =
        floor(clamp(mixed.orm.g, 0.0, 1.0) * 255.0) / 255.0;
    const float roughness_low =
        (mixed.orm.g - roughness_high) * 255.0;
    const float2 geometric_oct = clamp(
        almetal_world_oct_encode(output_geometric_normal), 0.0, 1.0);
    const float geometric_quantization_levels =
        constants.config.w != 0 ? 255.0 : 31.0;
    const float geometric_range =
        constants.config.w != 0 ? 65535.0 : 1023.0;
    const float2 geometric_quantized = floor(
        geometric_oct * geometric_quantization_levels + 0.5);
    const float packed_geometric_normal =
        (geometric_quantized.x *
             (geometric_quantization_levels + 1.0) +
         geometric_quantized.y) / geometric_range;

    ALMetalWorldGBufferOutput output;
    output.albedo = max(float4(mixed.color.rgb, 0.0), float4(0.0));
    output.specular_orm = max(float4(
        mixed.orm.r, roughness_high, mixed.orm.b, roughness_low),
        float4(0.0));
    output.normal = float4(
        almetal_world_oct_encode(normal), packed_geometric_normal, 0.67);
    output.emissive = max(float4(mixed.emissive, 0.0), float4(0.0));
    return output;
}

struct ALMetalTerrainPaintConstants
{
    float4x4 modelview_projection;
    float4 paint;
};

struct ALMetalTerrainPaintVaryings
{
    float4 position [[position]];
    float2 texcoord;
};

vertex ALMetalTerrainPaintVaryings almetal_terrain_paint_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalTerrainPaintConstants& constants [[buffer(2)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalTerrainPaintVaryings output;
    float4 clip_position =
        constants.modelview_projection * float4(positions[vertex_id].xyz, 1.0);
    // GL framebuffer row zero is the lower row. Metal render-target row zero
    // is the upper row, so invert clip Y for byte-for-byte mirror storage.
    clip_position.y = -clip_position.y;
    output.position = almetal_clip_from_gl(clip_position);
    output.texcoord = texcoords[vertex_id];
    return output;
}

fragment float4 almetal_terrain_paint_fragment(
    ALMetalTerrainPaintVaryings input [[stage_in]],
    texture2d<float> alpha_ramp [[texture(0)]],
    sampler alpha_sampler [[sampler(0)]],
    constant ALMetalTerrainPaintConstants& constants [[buffer(0)]])
{
    const float alpha1 = alpha_ramp.sample(alpha_sampler, input.texcoord).a;
    const float alpha2 = alpha_ramp.sample(
        alpha_sampler, input.texcoord - float2(2.0, 0.0)).a;
    const float alpha_final = alpha_ramp.sample(
        alpha_sampler, input.texcoord - float2(1.0, 0.0)).a;

    const float4 sample_x = float4(1.0, 0.0, 0.0, 0.0);
    const float4 sample_y = float4(0.0, 1.0, 0.0, 0.0);
    const float4 sample_z = float4(0.0, 0.0, 1.0, 0.0);
    const float4 sample_w = float4(0.0, 0.0, 0.0, 1.0);
    float4 weight = mix(mix(sample_w, sample_z, alpha2),
                        mix(sample_y, sample_x, alpha1), alpha_final) - 0.1;
    const float4 usage = select(float4(0.0), float4(1.0), weight > 0.0);
    weight *= usage;
    weight /= dot(weight, float4(1.0));

    const float3 stored = round(weight.yzw * constants.paint.x) /
                          constants.paint.x;
    return max(float4(stored, 1.0), float4(0.0));
}

struct ALMetalLitPreviewConstants
{
    float4x4 modelview_projection;
    float4 normal_matrix[3];
    float4 light_direction[3];
    float4 light_diffuse[3];
    float4 diffuse_color;
    float4 avatar_wind;
    float4 avatar_sin_wave;
    float4 avatar_gravity;
    float4 avatar_cloth_depth;
    float4 pbr_base_color_transform[2];
    float4 pbr_emissive_transform[2];
    float4 pbr_emissive_color;
    float4x4 texture_matrix;
};

struct ALMetalLitPreviewVaryings
{
    float4 position [[position]];
    float4 color;
};

struct ALMetalLitPreviewTexturedVaryings
{
    float4 position [[position]];
    float4 color;
    float2 texcoord;
};

struct ALMetalLitPreviewPBRGlowVaryings
{
    float4 position [[position]];
    float4 color;
    float4 emissive;
    float3 emissive_color;
    float2 base_color_texcoord;
    float2 emissive_texcoord;
};

static float4 almetal_lit_preview_clip_position(
    float4 position,
    constant ALMetalLitPreviewConstants& constants)
{
    float4 clip_position = constants.modelview_projection *
                           float4(position.xyz, 1.0);
    // Match glCopyTexSubImage's lower-left framebuffer row convention in the
    // resulting texture bytes.
    clip_position.y = -clip_position.y;
    return almetal_clip_from_gl(clip_position);
}

static float4 almetal_lit_preview_rebased_clip_position(
    float4 local_position,
    float3 origin,
    constant ALMetalLitPreviewConstants& constants)
{
    // Preserve objectSkinV.glsl's precision contract: the varying local
    // position is transformed with w=0 and the region-scale origin with w=1.
    // Adding them before the matrix multiply would quantize local mesh detail
    // at large agent coordinates.
    float4 clip_position =
        constants.modelview_projection * float4(local_position.xyz, 0.0) +
        constants.modelview_projection * float4(origin, 1.0);
    clip_position.y = -clip_position.y;
    return almetal_clip_from_gl(clip_position);
}

static ALMetalLitPreviewVaryings almetal_lit_preview_vertex_common(
    float4 position,
    float4 normal_value,
    constant ALMetalLitPreviewConstants& constants)
{
    ALMetalLitPreviewVaryings output;
    output.position = almetal_lit_preview_clip_position(position, constants);

    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    const float3 normal = normalize(normal_matrix * normal_value.xyz);
    float3 lighting = 0.0;
    for (uint light = 0; light < 3; ++light)
    {
        lighting += constants.light_diffuse[light].rgb *
                    max(dot(normal, constants.light_direction[light].xyz), 0.0);
    }
    output.color = float4(lighting * constants.diffuse_color.rgb,
                          constants.diffuse_color.a);
    return output;
}

struct ALMetalLitPreviewSkinnedVertex
{
    float4 position;
    float4 normal;
};

struct ALMetalLitPreviewAvatarRows
{
    float4 row0;
    float4 row1;
    float4 row2;
};

static ALMetalLitPreviewAvatarRows almetal_lit_preview_avatar_rows(
    float encoded_weight,
    const device float4* skin_palette)
{
    const uint i = uint(clamp(floor(encoded_weight), 0.0, 14.0));
    const uint j = min(i + 1u, 14u);
    const float blend = fract(encoded_weight);
    ALMetalLitPreviewAvatarRows rows;
    rows.row0 = mix(skin_palette[i], skin_palette[j], blend);
    rows.row1 = mix(skin_palette[15u + i], skin_palette[15u + j], blend);
    rows.row2 = mix(skin_palette[30u + i], skin_palette[30u + j], blend);
    return rows;
}

// Legacy system-avatar meshes predate the four-influence object stream below.
// Their scalar weight stores joint i in the integer part and the blend toward
// min(i + 1, 14) in the fraction. The palette is exactly the 45 float4 layout
// built by LLViewerJointMesh: three contiguous rows of fifteen transforms.
static ALMetalLitPreviewSkinnedVertex almetal_lit_preview_avatar_skin_vertex(
    float4 position,
    float4 normal_value,
    float encoded_weight,
    const device float4* skin_palette)
{
    const ALMetalLitPreviewAvatarRows rows =
        almetal_lit_preview_avatar_rows(encoded_weight, skin_palette);

    ALMetalLitPreviewSkinnedVertex output;
    output.position = float4(dot(rows.row0, position),
                             dot(rows.row1, position),
                             dot(rows.row2, position), 1.0);
    const float3 normal = normal_value.xyz;
    output.normal = float4(
        dot(cross(rows.row1.xyz, rows.row2.xyz), normal),
        dot(cross(rows.row2.xyz, rows.row0.xyz), normal),
        dot(cross(rows.row0.xyz, rows.row1.xyz), normal), 0.0);
    return output;
}

// Exact avatarSkinV.glsl getClothedPosition() result. Preview callers consume
// only its position; the world G-buffer also stores the perturbed normal.
static ALMetalLitPreviewSkinnedVertex almetal_avatar_cloth_vertex(
    float4 position,
    float4 normal_value,
    float4 clothing,
    float encoded_weight,
    const device float4* skin_palette,
    float4 avatar_wind,
    float4 avatar_sin_wave,
    float4 avatar_gravity,
    float4 avatar_cloth_depth)
{
    const ALMetalLitPreviewAvatarRows rows =
        almetal_lit_preview_avatar_rows(encoded_weight, skin_palette);
    const float3 source_normal = normal_value.xyz;
    float3 normal = normalize(float3(
        dot(cross(rows.row1.xyz, rows.row2.xyz), source_normal),
        dot(cross(rows.row2.xyz, rows.row0.xyz), source_normal),
        dot(cross(rows.row0.xyz, rows.row1.xyz), source_normal)));

    const float wind_dot = dot(normal, avatar_wind.xyz);
    float4 wind_effect = float4(wind_dot);
    const float4 eye_position = float4(
        dot(rows.row0, position), dot(rows.row1, position),
        dot(rows.row2, position), 1.0);
    const float main_depth = dot(avatar_cloth_depth, eye_position);
    wind_effect.xyz = main_depth * float3(0.015) + wind_effect.xyz;
    wind_effect.w = (wind_effect.w * 2.0 + 1.0) *
                    avatar_wind.w;
    wind_effect.xyz = wind_effect.xyz * avatar_sin_wave.xyz +
                      float3(avatar_sin_wave.w);

    const float inverse_two_pi = 0.159154943;
    const float two_pi = 6.28318530;
    const float half_pi = 1.5707963;
    float3 periods = wind_effect.xyz * inverse_two_pi;
    wind_effect.x = (periods.x - floor(periods.x)) * two_pi;
    periods.z -= half_pi;
    wind_effect.z = (periods.z - floor(periods.z)) * two_pi;
    // The source shader intentionally uses this literal rather than
    // gPiConstants.z; retain its exact rounding.
    wind_effect.xyz -= float3(3.141592);

    const float4 squared = wind_effect * wind_effect;
    float4 sin_wave = -squared * 0.00018542 + float4(0.0083143);
    sin_wave = sin_wave * -squared + float4(0.166666);
    sin_wave = sin_wave * -squared + float4(1.0);
    sin_wave *= wind_effect;
    sin_wave.xyz = sin_wave.xyz * avatar_wind.w +
                   float3(wind_effect.w);

    float4 gravity = float4(dot(normal, avatar_gravity.xyz));
    gravity = min(gravity, float4(0.2, 0.0, 0.0, 0.0));
    gravity *= float4(1.5, 0.0, 0.0, 0.0);
    sin_wave.x += gravity.x;
    sin_wave.xyz *= clothing.w;
    sin_wave.xyz = max(sin_wave.xyz, float3(-1.0));

    const float4 offset_position =
        float4(1.0, 1.0, 1.0, 0.0) * (clothing * sin_wave.x) + position;
    const float3 normal_offset = avatar_wind.xyz * sin_wave.z + normal;
    normal = normalize(normal + normal_offset * 2.0);

    ALMetalLitPreviewSkinnedVertex output;
    output.position = float4(dot(rows.row0, offset_position),
                             dot(rows.row1, offset_position),
                             dot(rows.row2, offset_position), 1.0);
    output.normal = float4(normal, 0.0);
    return output;
}

static ALMetalWorldSimpleVaryings almetal_world_avatar_varyings(
    float4 clip_position,
    float3 eye_normal,
    float2 texcoord,
    constant ALMetalWorldSimpleConstants& constants)
{
    ALMetalWorldSimpleVaryings output;
    clip_position.y = -clip_position.y;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.normal = normalize(eye_normal);
    output.color = 1.0;
    output.texcoord = texcoord;
    output.texcoord1 = texcoord;
    output.texcoord2 = texcoord;
    output.tangent = 0.0;
    output.bitangent = 0.0;
    output.texture_index = 0;
    output.texture_count = constants.depth_convention.y;
    output.alpha_cutoff = constants.material.x;
    output.material_flags = uint3(
        constants.material.y != 0.0,
        constants.material.z != 0.0,
        constants.material.w != 0.0);
    return output;
}

vertex ALMetalWorldSimpleVaryings almetal_world_avatar_rigid_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    return almetal_world_avatar_varyings(
        constants.modelview_projection *
            float4(positions[vertex_id].xyz, 1.0),
        normal_matrix * normals[vertex_id].xyz,
        texcoords[vertex_id], constants);
}

vertex ALMetalWorldSimpleVaryings almetal_world_avatar_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float* encoded_weights [[buffer(6)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_avatar_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette);
    return almetal_world_avatar_varyings(
        constants.modelview_projection * skinned.position,
        skinned.normal.xyz, texcoords[vertex_id], constants);
}

vertex ALMetalWorldSimpleVaryings
almetal_world_avatar_cloth_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float* encoded_weights [[buffer(6)]],
    const device float4* clothing [[buffer(9)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_avatar_cloth_vertex(
            positions[vertex_id], normals[vertex_id], clothing[vertex_id],
            encoded_weights[vertex_id], skin_palette,
            constants.avatar_wind, constants.avatar_sin_wave,
            constants.avatar_gravity, constants.avatar_cloth_depth);
    return almetal_world_avatar_varyings(
        constants.modelview_projection * skinned.position,
        skinned.normal.xyz, texcoords[vertex_id], constants);
}

static ALMetalLitPreviewVaryings almetal_lit_preview_avatar_vertex_common(
    float4 eye_position,
    float4 eye_normal,
    constant ALMetalLitPreviewConstants& constants)
{
    ALMetalLitPreviewVaryings output;
    output.position = almetal_lit_preview_clip_position(
        eye_position, constants);
    const float3 normal = normalize(eye_normal.xyz);
    const float3 direction = constants.light_direction[0].xyz;
    const float front = max(dot(normal, direction), 0.0);
    const float wrapped_back = sqrt(
        max(dot(normal, -direction), 0.0) * 0.5 + 0.25);
    const float3 lighting = constants.light_diffuse[0].rgb *
                            (front + wrapped_back);
    output.color = float4(
        min(lighting * constants.diffuse_color.rgb, float3(1.0)),
        constants.diffuse_color.a);
    return output;
}

static ALMetalLitPreviewVaryings almetal_lit_preview_rigid_avatar_vertex_common(
    float4 position,
    float4 normal_value,
    constant ALMetalLitPreviewConstants& constants)
{
    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    return almetal_lit_preview_avatar_vertex_common(
        position, float4(normal_matrix * normal_value.xyz, 0.0), constants);
}

// Decode the viewer's object-skinning stream without changing its precision
// contract. Palette element zero is the agent-space rebase origin; each joint
// then contributes the three float4 columns packed by
// LLVOAvatar::updateSkinInfoMatrixPalette().
static ALMetalLitPreviewSkinnedVertex almetal_lit_preview_skin_vertex(
    float4 position,
    float4 normal_value,
    float4 encoded_weights,
    const device float4* skin_palette,
    uint joint_count)
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        position, normal_value, encoded_weights, skin_palette, joint_count);
    ALMetalLitPreviewSkinnedVertex output;
    output.position = skinned.position;
    output.normal = skinned.normal;
    return output;
}

vertex ALMetalLitPreviewVaryings almetal_lit_preview_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    uint vertex_id [[vertex_id]])
{
    return almetal_lit_preview_vertex_common(
        positions[vertex_id], normals[vertex_id], constants);
}

vertex ALMetalLitPreviewVaryings almetal_lit_preview_flat_vertex(
    const device packed_float3* positions [[buffer(0)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalLitPreviewVaryings output;
    output.position = almetal_lit_preview_clip_position(
        float4(float3(positions[vertex_id]), 1.0), constants);
    output.color = constants.diffuse_color;
    return output;
}

vertex ALMetalLitPreviewVaryings almetal_lit_preview_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float4* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    constant uint& joint_count [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette, joint_count);
    ALMetalLitPreviewVaryings output = almetal_lit_preview_vertex_common(
        skinned.position, skinned.normal, constants);
    output.position = almetal_lit_preview_rebased_clip_position(
        skinned.position, skin_palette[0].xyz, constants);
    return output;
}

vertex ALMetalLitPreviewVaryings almetal_lit_preview_avatar_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    uint vertex_id [[vertex_id]])
{
    return almetal_lit_preview_rigid_avatar_vertex_common(
        positions[vertex_id], normals[vertex_id], constants);
}

vertex ALMetalLitPreviewVaryings almetal_lit_preview_avatar_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_avatar_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette);
    return almetal_lit_preview_avatar_vertex_common(
        skinned.position, skinned.normal, constants);
}

fragment float4 almetal_lit_preview_fragment(
    ALMetalLitPreviewVaryings input [[stage_in]])
{
    return max(input.color, float4(0.0));
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_textured_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewVaryings lit = almetal_lit_preview_vertex_common(
        positions[vertex_id], normals[vertex_id], constants);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = lit.position;
    output.color = lit.color;
    output.texcoord = texcoords[vertex_id];
    return output;
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_skinned_textured_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    const device float4* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    constant uint& joint_count [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette, joint_count);
    const ALMetalLitPreviewVaryings lit = almetal_lit_preview_vertex_common(
        skinned.position, skinned.normal, constants);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = lit.position;
    output.color = lit.color;
    output.texcoord = texcoords[vertex_id];
    output.position = almetal_lit_preview_rebased_clip_position(
        skinned.position, skin_palette[0].xyz, constants);
    return output;
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_avatar_textured_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewVaryings lit =
        almetal_lit_preview_rigid_avatar_vertex_common(
            positions[vertex_id], normals[vertex_id], constants);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = lit.position;
    output.color = lit.color;
    output.texcoord = texcoords[vertex_id];
    return output;
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_avatar_unlit_textured_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalLitPreviewTexturedVaryings output;
    output.position = almetal_lit_preview_clip_position(
        positions[vertex_id], constants);
    output.color = constants.diffuse_color;
    output.texcoord = texcoords[vertex_id];
    return output;
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_avatar_skinned_textured_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    const device float* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_avatar_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette);
    const ALMetalLitPreviewVaryings lit =
        almetal_lit_preview_avatar_vertex_common(
            skinned.position, skinned.normal, constants);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = lit.position;
    output.color = lit.color;
    output.texcoord = texcoords[vertex_id];
    return output;
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_avatar_skinned_unlit_textured_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    const device float* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_avatar_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = almetal_lit_preview_clip_position(
        skinned.position, constants);
    output.color = constants.diffuse_color;
    output.texcoord = texcoords[vertex_id];
    return output;
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_avatar_skinned_unlit_cloth_textured_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    const device float* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    const device float4* clothing [[buffer(9)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_avatar_cloth_vertex(
            positions[vertex_id], normals[vertex_id], clothing[vertex_id],
            encoded_weights[vertex_id], skin_palette,
            constants.avatar_wind, constants.avatar_sin_wave,
            constants.avatar_gravity, constants.avatar_cloth_depth);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = almetal_lit_preview_clip_position(
        skinned.position, constants);
    output.color = constants.diffuse_color;
    output.texcoord = texcoords[vertex_id];
    return output;
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_deferred_rigged_textured_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    const device uchar4* colors [[buffer(4)]],
    const device float4* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    constant uint& joint_count [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette, joint_count);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = almetal_lit_preview_rebased_clip_position(
        skinned.position, skin_palette[0].xyz, constants);
    output.color = float4(colors[vertex_id]) / 255.0;
    output.texcoord = texcoords[vertex_id];
    return output;
}

static float2 almetal_lit_preview_pbr_texcoord(
    float2 vertex_texcoord,
    constant float4* material_transform,
    constant ALMetalLitPreviewConstants& constants)
{
    // Keep this operation order and handedness aligned with
    // deferred/textureUtilV.glsl::texture_transform. SL texture animation is
    // right-handed; KHR_texture_transform is defined in glTF's left-handed
    // UV space and is evaluated in-shader to retain reference precision.
    float2 texcoord =
        (constants.texture_matrix * float4(vertex_texcoord, 0.0, 1.0)).xy;
    texcoord.y = 1.0 - texcoord.y;

    const float2 scale = material_transform[0].xy;
    const float rotation = material_transform[0].z;
    const float2 offset = material_transform[1].xy;
    const float2 scaled = texcoord * scale;
    const float cosine = cos(rotation);
    const float sine = sin(rotation);
    texcoord = float2(cosine * scaled.x + sine * scaled.y,
                      -sine * scaled.x + cosine * scaled.y) + offset;
    texcoord.y = 1.0 - texcoord.y;
    return texcoord;
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_pbr_rigged_textured_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    const device uchar4* colors [[buffer(4)]],
    const device float4* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    constant uint& joint_count [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette, joint_count);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = almetal_lit_preview_rebased_clip_position(
        skinned.position, skin_palette[0].xyz, constants);
    output.color = float4(colors[vertex_id]) / 255.0;
    output.texcoord = almetal_lit_preview_pbr_texcoord(
        texcoords[vertex_id], constants.pbr_base_color_transform, constants);
    return output;
}

vertex ALMetalLitPreviewPBRGlowVaryings almetal_lit_preview_pbr_glow_rigged_textured_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    const device uchar4* colors [[buffer(4)]],
    const device uchar4* emissive [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    constant uint& joint_count [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette, joint_count);
    ALMetalLitPreviewPBRGlowVaryings output;
    output.position = almetal_lit_preview_rebased_clip_position(
        skinned.position, skin_palette[0].xyz, constants);
    output.color = float4(colors[vertex_id]) / 255.0;
    output.emissive = float4(emissive[vertex_id]) / 255.0;
    output.emissive_color = constants.pbr_emissive_color.rgb;
    output.base_color_texcoord = almetal_lit_preview_pbr_texcoord(
        texcoords[vertex_id], constants.pbr_base_color_transform, constants);
    output.emissive_texcoord = almetal_lit_preview_pbr_texcoord(
        texcoords[vertex_id], constants.pbr_emissive_transform, constants);
    return output;
}

static float3 almetal_srgb_to_linear(float3 encoded)
{
    return select(encoded / 12.92,
                  pow((encoded + 0.055) / 1.055, float3(2.4)),
                  encoded > 0.04045);
}

static float3 almetal_linear_to_srgb(float3 linear)
{
    const float3 value = max(linear, float3(0.0));
    return select(value * 12.92,
                  1.055 * pow(value, float3(1.0 / 2.4)) - 0.055,
                  value > 0.0031308);
}

struct ALMetalPBRPreviewConstants
{
    float4x4 modelview_projection;
    float4x4 modelview;
    float4 normal_matrix[3];
    float4 texture_transform[4][2];
    float4 emissive_color;
    float4 sun_direction;
    float4 sun_color;
    // metallic, roughness, alpha cutoff, reserved
    float4 material;
};

struct ALMetalPBRPreviewEnvironmentConstants
{
    float4 probe_origin;
    float4 env_matrix[3];
    // atmos_amblit.rgb, sky ambient scale
    float4 ambient;
    // irradiance scale, radiance scale, max LOD, reserved
    float4 probe;
    uint4 sh_coefficients[4];
};

struct ALMetalPBRPreviewVaryings
{
    float4 position [[position]];
    float3 eye_position;
    float3 normal;
    float3 tangent;
    float tangent_sign [[flat]];
    float4 color;
    float2 base_color_texcoord;
    float2 normal_texcoord;
    float2 orm_texcoord;
    float2 emissive_texcoord;
};

static float2 almetal_pbr_preview_texcoord(
    float2 vertex_texcoord,
    constant float4* transform)
{
    // Match deferred/textureUtilV.glsl::texture_transform with the preview's
    // identity SL animation matrix. KHR_texture_transform is evaluated in its
    // left-handed UV convention, then returned to SL's handedness.
    float2 texcoord = vertex_texcoord;
    texcoord.y = 1.0 - texcoord.y;
    const float2 scaled = texcoord * transform[0].xy;
    const float cosine = cos(transform[0].z);
    const float sine = sin(transform[0].z);
    texcoord = float2(cosine * scaled.x + sine * scaled.y,
                      -sine * scaled.x + cosine * scaled.y) +
               transform[1].xy;
    texcoord.y = 1.0 - texcoord.y;
    return texcoord;
}

static float4 almetal_pbr_preview_tangent(
    float4 tangent,
    float3 normal,
    constant float4* normal_transform)
{
    const float inverse_rotation = -normal_transform[0].z;
    float2 weights = float2(cos(inverse_rotation),
                            -sin(inverse_rotation));
    const float2 scale_sign = sign(normal_transform[0].xy);
    weights *= scale_sign;
    if (abs(weights.x) + abs(weights.y) == 0.0)
    {
        weights = float2(1.0, 0.0);
    }
    weights = normalize(weights);
    weights.y = -weights.y;
    const float3 binormal = tangent.w * cross(normal, tangent.xyz);
    return float4(weights.x * tangent.xyz + weights.y * binormal,
                  tangent.w * scale_sign.x * scale_sign.y);
}

vertex ALMetalPBRPreviewVaryings almetal_pbr_preview_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    const device float4* tangents [[buffer(4)]],
    constant ALMetalPBRPreviewConstants& constants [[buffer(5)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalPBRPreviewVaryings output;
    float4 clip_position = constants.modelview_projection *
                           float4(positions[vertex_id].xyz, 1.0);
    // The adopted LLImageGL mirror retains GL's lower-left row-zero bytes.
    clip_position.y = -clip_position.y;
    output.position = almetal_clip_from_gl(clip_position);
    output.eye_position = (constants.modelview *
                           float4(positions[vertex_id].xyz, 1.0)).xyz;
    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    output.normal = normalize(normal_matrix * normals[vertex_id].xyz);
    const float3 transformed_tangent =
        normalize(normal_matrix * tangents[vertex_id].xyz);
    const float4 adjusted_tangent = almetal_pbr_preview_tangent(
        float4(transformed_tangent, tangents[vertex_id].w),
        output.normal, constants.texture_transform[1]);
    output.tangent = normalize(adjusted_tangent.xyz);
    output.tangent_sign = adjusted_tangent.w;
    output.color = float4(colors[vertex_id]) / 255.0;
    output.base_color_texcoord = almetal_pbr_preview_texcoord(
        texcoords[vertex_id], constants.texture_transform[0]);
    output.normal_texcoord = almetal_pbr_preview_texcoord(
        texcoords[vertex_id], constants.texture_transform[1]);
    output.orm_texcoord = almetal_pbr_preview_texcoord(
        texcoords[vertex_id], constants.texture_transform[2]);
    output.emissive_texcoord = almetal_pbr_preview_texcoord(
        texcoords[vertex_id], constants.texture_transform[3]);
    return output;
}

static float3 almetal_pbr_fresnel(float cosine, float3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosine, 0.0, 1.0), 5.0);
}

static float2 almetal_pbr_hammersley(uint index, uint count)
{
    uint bits = (index << 16u) | (index >> 16u);
    bits = ((bits & 0x55555555u) << 1u) |
           ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) |
           ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) |
           ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) |
           ((bits & 0xFF00FF00u) >> 8u);
    return float2(float(index) / float(count),
                  float(bits) * 2.3283064365386963e-10);
}

static float3 almetal_pbr_importance_sample_ggx(float2 xi,
                                                float roughness,
                                                float3 normal)
{
    const float alpha = roughness * roughness;
    const float phi = 2.0 * M_PI_F * xi.x;
    const float cos_theta = sqrt((1.0 - xi.y) /
        (1.0 + (alpha * alpha - 1.0) * xi.y));
    const float sin_theta = sqrt(max(1.0 - cos_theta * cos_theta, 0.0));
    const float3 half_tangent = float3(
        sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
    const float3 up = abs(normal.z) < 0.999
        ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    const float3 tangent_x = normalize(cross(up, normal));
    const float3 tangent_y = normalize(cross(normal, tangent_x));
    return normalize(tangent_x * half_tangent.x +
                     tangent_y * half_tangent.y +
                     normal * half_tangent.z);
}

// Height-correlated Smith visibility with the 1/(4 NdotL NdotV)
// denominator folded in. This is the same term used by deferredUtil.glsl
// and by the GL BRDF-table generator.
static float almetal_pbr_visibility(float nv, float nl,
                                    float perceptual_roughness)
{
    const float alpha = perceptual_roughness * perceptual_roughness;
    const float alpha2 = alpha * alpha;
    const float lambda_v = nl * sqrt(nv * nv * (1.0 - alpha2) + alpha2);
    const float lambda_l = nv * sqrt(nl * nl * (1.0 - alpha2) + alpha2);
    const float denominator = lambda_v + lambda_l;
    return denominator > 0.0 ? 0.5 / denominator : 0.0;
}

static float2 almetal_pbr_integrate_brdf(float nv,
                                         float perceptual_roughness)
{
    constexpr uint sample_count = 1024u;
    constexpr float3 normal = float3(0.0, 0.0, 1.0);
    const float3 view = float3(sqrt(max(1.0 - nv * nv, 0.0)), 0.0, nv);
    float2 result = float2(0.0);
    for (uint index = 0u; index < sample_count; ++index)
    {
        const float2 xi = almetal_pbr_hammersley(index, sample_count);
        const float3 half_vector = almetal_pbr_importance_sample_ggx(
            xi, perceptual_roughness, normal);
        const float3 light = 2.0 * dot(view, half_vector) * half_vector - view;
        const float nl = max(light.z, 0.0);
        const float vh = max(dot(view, half_vector), 0.0);
        const float nh = max(half_vector.z, 0.0);
        if (nl > 0.0 && nh > 0.0)
        {
            const float visibility_pdf = almetal_pbr_visibility(
                nv, nl, perceptual_roughness) * vh * nl / nh;
            const float fresnel = pow(1.0 - vh, 5.0);
            result += float2(1.0 - fresnel, fresnel) * visibility_pdf;
        }
    }
    return 4.0 * result / float(sample_count);
}

kernel void almetal_pbr_preview_brdf_lut(
    texture2d<float, access::write> output [[texture(0)]],
    uint2 position [[thread_position_in_grid]])
{
    if (position.x >= output.get_width() || position.y >= output.get_height())
    {
        return;
    }
    const float2 uv = (float2(position) + 0.5) /
                      float2(output.get_width(), output.get_height());
    // Preserve the GL table's glossiness Y axis. Its consumers sample at
    // 1-roughness, so a texel written at y integrates 1-y.
    const float2 integrated = almetal_pbr_integrate_brdf(uv.x, 1.0 - uv.y);
    output.write(float4(integrated, 0.0, 1.0), position);
}

static float almetal_pbr_distribution(float nh,
                                      float perceptual_roughness)
{
    const float alpha = perceptual_roughness * perceptual_roughness;
    const float alpha2 = alpha * alpha;
    const float denominator = (nh * alpha2 - nh) * nh + 1.0;
    return alpha2 / (M_PI_F * denominator * denominator);
}

static float almetal_pbr_filtered_roughness(float perceptual_roughness,
                                            float3 normal)
{
    constexpr float variance_scale = 0.15;
    constexpr float variance_threshold = 0.25;
    const float3 du = dfdx(normal);
    const float3 dv = dfdy(normal);
    const float variance = variance_scale *
        (dot(du, du) + dot(dv, dv));
    const float roughness_kernel = min(2.0 * variance, variance_threshold);
    const float alpha = perceptual_roughness * perceptual_roughness;
    return sqrt(sqrt(clamp(alpha * alpha + roughness_kernel, 0.0, 1.0)));
}

static float3 almetal_pbr_clamp_radiance(float3 color)
{
    color = max(color, float3(0.0));
    const float peak = max(color.r, max(color.g, color.b));
    return peak > 65504.0 ? color * (65504.0 / peak) : color;
}

static float2 almetal_pbr_brdf(texture2d<float> lut,
                               sampler lut_sampler,
                               float nv,
                               float perceptual_roughness)
{
    return lut.sample(lut_sampler,
                      float2(nv, 1.0 - perceptual_roughness)).rg;
}

static float3 almetal_pbr_energy_compensation(texture2d<float> lut,
                                               sampler lut_sampler,
                                               float3 specular_color,
                                               float perceptual_roughness,
                                               float nv)
{
    const float2 brdf = almetal_pbr_brdf(
        lut, lut_sampler, clamp(abs(nv), 0.001, 1.0),
        max(perceptual_roughness, 0.045));
    const float single_scatter = max(brdf.x + brdf.y, 1.0e-4);
    return 1.0 + specular_color * (1.0 / single_scatter - 1.0);
}

static float3 almetal_pbr_environment_transform(
    float3 direction,
    constant ALMetalPBRPreviewEnvironmentConstants& environment)
{
    return direction.x * environment.env_matrix[0].xyz +
           direction.y * environment.env_matrix[1].xyz +
           direction.z * environment.env_matrix[2].xyz;
}

static float3 almetal_pbr_probe_direction(
    float3 position,
    float3 direction,
    constant ALMetalPBRPreviewEnvironmentConstants& environment)
{
    constexpr float radius_squared = 4096.0 * 4096.0;
    const float3 center = environment.probe_origin.xyz;
    const float3 to_center = center - position;
    float3 intersection;
    if (dot(to_center, to_center) > radius_squared)
    {
        intersection = center + direction;
    }
    else
    {
        const float tca = dot(to_center, direction);
        const float distance_squared = dot(to_center, to_center) - tca * tca;
        const float half_chord = sqrt(max(radius_squared - distance_squared, 0.0));
        intersection = position + direction * (tca + half_chord);
    }
    return almetal_pbr_environment_transform(
        intersection - center, environment);
}

static float2 almetal_pbr_unpack_halves(uint packed)
{
    return float2(as_type<half2>(packed));
}

static float3 almetal_pbr_sh_irradiance(
    float3 direction,
    constant ALMetalPBRPreviewEnvironmentConstants& environment)
{
    const float2 h0 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[0].x);
    const float2 h1 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[0].y);
    const float2 h2 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[0].z);
    const float2 h3 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[0].w);
    const float2 h4 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[1].x);
    const float2 h5 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[1].y);
    const float2 h6 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[1].z);
    const float2 h7 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[1].w);
    const float2 h8 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[2].x);
    const float2 h9 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[2].y);
    const float2 h10 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[2].z);
    const float2 h11 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[2].w);
    const float2 h12 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[3].x);
    const float2 h13 = almetal_pbr_unpack_halves(
        environment.sh_coefficients[3].y);

    const float3 l0 = float3(h0, h1.x);
    const float3 l1 = float3(h1.y, h2);
    const float3 l2 = float3(h3, h4.x);
    const float3 l3 = float3(h4.y, h5);
    const float3 l4 = float3(h6, h7.x);
    const float3 l5 = float3(h7.y, h8);
    const float3 l6 = float3(h9, h10.x);
    const float3 l7 = float3(h10.y, h11);
    const float3 l8 = float3(h12, h13.x);

    constexpr float a0 = 1.0;
    constexpr float a1 = 0.6666667;
    constexpr float a2 = 0.25;
    const float3 result =
        a0 * 0.282095 * l0 +
        a1 * 0.488603 *
            (direction.y * l1 + direction.z * l2 + direction.x * l3) +
        a2 * (1.092548 *
                  (direction.x * direction.y * l4 +
                   direction.y * direction.z * l5 +
                   direction.x * direction.z * l7) +
              0.315392 * (3.0 * direction.z * direction.z - 1.0) * l6 +
              0.546274 *
                  (direction.x * direction.x -
                   direction.y * direction.y) * l8);
    return max(result, float3(0.0));
}

static float3 almetal_pbr_ambient(
    float3 normal,
    float3 light,
    constant ALMetalPBRPreviewEnvironmentConstants& environment)
{
    float ambient_lighting = min(abs(dot(normal, light)), 1.0) * 0.5;
    ambient_lighting = 1.0 - ambient_lighting * ambient_lighting;
    const float3 linear = almetal_srgb_to_linear(
        environment.ambient.rgb * ambient_lighting);
    return float3(dot(linear, float3(0.2126, 0.7152, 0.0722))) *
           environment.ambient.w;
}

static float3 almetal_pbr_probe_irradiance(
    float3 position,
    float3 normal,
    float3 ambient,
    constant ALMetalPBRPreviewEnvironmentConstants& environment)
{
    const float3 probe_direction = almetal_pbr_probe_direction(
        position, normal, environment);
    const float length_squared = dot(probe_direction, probe_direction);
    const float3 fallback = almetal_pbr_environment_transform(
        normal, environment);
    const float3 sample_direction = length_squared > 1.0e-12
        ? probe_direction * rsqrt(length_squared) : normalize(fallback);
    float3 irradiance = almetal_pbr_sh_irradiance(
        sample_direction, environment);
    irradiance *= max(environment.probe.x, 1.0);
    return mix(ambient, irradiance, min(environment.probe.x, 1.0));
}

static float3 almetal_pbr_dominant_direction(
    float3 normal, float3 reflection, float perceptual_roughness)
{
    const float smoothness = 1.0 - perceptual_roughness;
    const float factor = smoothness *
        (sqrt(smoothness) + perceptual_roughness);
    return normalize(mix(normal, reflection, factor));
}

static float almetal_pbr_horizon_occlusion(
    float3 reflection, float3 geometric_normal)
{
    const float horizon = clamp(
        1.0 + dot(normalize(reflection), normalize(geometric_normal)),
        0.0, 1.0);
    return horizon * horizon;
}

static float almetal_pbr_specular_ao(
    float nv, float ao, float perceptual_roughness)
{
    return clamp(pow(nv + ao,
                     exp2(-16.0 * perceptual_roughness - 1.0)) -
                 1.0 + ao,
                 0.0, 1.0);
}

static float3 almetal_pbr_gtao_multibounce(float visibility, float3 albedo)
{
    const float3 a = 2.0404 * albedo - 0.3324;
    const float3 b = -4.7951 * albedo + 0.6417;
    const float3 c = 2.7552 * albedo + 0.6903;
    return max(float3(visibility),
               ((visibility * a + b) * visibility + c) * visibility);
}

struct ALMetalPBRPreviewIBL
{
    float3 diffuse;
    float3 specular;
};

static ALMetalPBRPreviewIBL almetal_pbr_ibl(
    texture2d<float> brdf_lut,
    sampler brdf_sampler,
    float3 diffuse_color,
    float3 specular_color,
    float3 radiance,
    float3 irradiance,
    float ao,
    float nv,
    float perceptual_roughness)
{
    ALMetalPBRPreviewIBL result;
    nv = clamp(nv, 0.0, 1.0);
    const float2 brdf = almetal_pbr_brdf(
        brdf_lut, brdf_sampler, nv, perceptual_roughness);
    const float3 fr = max(float3(1.0 - perceptual_roughness),
                          specular_color) - specular_color;
    const float3 ks = specular_color +
        fr * pow(1.0 - nv, 5.0);
    const float3 fss_ess = ks * brdf.x + brdf.y;
    const float ems = 1.0 - (brdf.x + brdf.y);
    const float3 f_average = specular_color +
        (1.0 - specular_color) / 21.0;
    const float3 fms_ems = ems * fss_ess * f_average /
        max(float3(1.0) - f_average * ems, float3(1.0e-4));
    const float3 kd = diffuse_color *
        max(float3(1.0) - fss_ess - fms_ems, float3(0.0));
    result.diffuse = (fms_ems + kd) * irradiance *
        almetal_pbr_gtao_multibounce(ao, diffuse_color);
    result.specular = radiance * fss_ess *
        almetal_pbr_specular_ao(
            nv, ao, perceptual_roughness * perceptual_roughness);
    return result;
}

struct ALMetalPBRPreviewPunctual
{
    float nl;
    float3 diffuse;
    float3 specular;
};

static ALMetalPBRPreviewPunctual almetal_pbr_punctual(
    float3 diffuse_color,
    float3 specular_color,
    float perceptual_roughness,
    float3 normal,
    float3 view,
    float3 light)
{
    ALMetalPBRPreviewPunctual result;
    perceptual_roughness = max(perceptual_roughness, 0.045);
    const float3 half_vector = normalize(light + view);
    const float nl = clamp(dot(normal, light), 0.0, 1.0);
    const float nv = clamp(abs(dot(normal, view)), 0.001, 1.0);
    const float nh = clamp(dot(normal, half_vector), 0.0, 1.0);
    const float vh = clamp(dot(view, half_vector), 0.0, 1.0);
    const float3 fresnel = almetal_pbr_fresnel(vh, specular_color);
    result.nl = nl;
    result.diffuse = (1.0 - fresnel) * diffuse_color / M_PI_F;
    result.specular = fresnel *
        almetal_pbr_visibility(nv, nl, perceptual_roughness) *
        almetal_pbr_distribution(nh, perceptual_roughness);
    return result;
}

static float3 almetal_pbr_neutral_tonemap(float3 color)
{
    constexpr float start_compression = 0.76;
    constexpr float desaturation = 0.15;
    const float minimum_channel = min(color.r, min(color.g, color.b));
    const float offset = minimum_channel < 0.08
        ? minimum_channel - 6.25 * minimum_channel * minimum_channel : 0.04;
    color -= offset;
    const float peak = max(color.r, max(color.g, color.b));
    if (peak < start_compression)
    {
        return color;
    }
    constexpr float distance = 1.0 - start_compression;
    const float new_peak = 1.0 - distance * distance /
        (peak + distance - start_compression);
    color *= new_peak / peak;
    const float grey_mix = 1.0 - 1.0 /
        (desaturation * (peak - new_peak) + 1.0);
    return mix(color, float3(new_peak), grey_mix);
}

static float3 almetal_pbr_aces_hill(float3 color)
{
    const float3x3 input_matrix = float3x3(
        float3(0.59719, 0.07600, 0.02840),
        float3(0.35458, 0.90834, 0.13383),
        float3(0.04823, 0.01566, 0.83777));
    const float3x3 output_matrix = float3x3(
        float3(1.60475, -0.10208, -0.00327),
        float3(-0.53108, 1.10813, -0.07276),
        float3(-0.07367, -0.00605, 1.07602));
    color = input_matrix * color;
    const float3 fitted =
        (color * (color + 0.0245786) - 0.000090537) /
        (color * (0.983729 * color + 0.4329510) + 0.238081);
    return output_matrix * fitted;
}

static float3 almetal_pbr_aces_boosted(float3 color, float white_tonemapped)
{
    return almetal_pbr_aces_hill(color * 1.2) / white_tonemapped;
}

static float3 almetal_pbr_reinhard(float3 color, float white_squared)
{
    return color * (1.0 + color / white_squared) / (1.0 + color);
}

static float3 almetal_pbr_filmic(float3 color, float white_tonemapped)
{
    constexpr float exposure_bias = 2.0;
    constexpr float a = 0.22 * exposure_bias * exposure_bias;
    constexpr float b = 0.30 * exposure_bias;
    constexpr float c = 0.10;
    constexpr float d = 0.20;
    constexpr float e = 0.01;
    constexpr float f = 0.30;
    const float3 mapped =
        ((color * (a * color + c * b) + d * e) /
         (color * (a * color + b) + d * f)) - e / f;
    return mapped / white_tonemapped;
}

static float3 almetal_pbr_uchimura(float3 color)
{
    constexpr float peak = 1.0;
    constexpr float contrast = 1.0;
    constexpr float linear_start = 0.22;
    constexpr float linear_length = 0.4;
    constexpr float black = 1.33;
    constexpr float pedestal = 0.0;
    const float l0 = ((peak - linear_start) * linear_length) / contrast;
    const float s0 = linear_start + l0;
    const float s1 = linear_start + contrast * l0;
    const float c2 = (contrast * peak) / (peak - s1);
    const float cp = -c2 / peak;
    const float3 w0 = 1.0 - smoothstep(
        float3(0.0), float3(linear_start), color);
    const float3 w2 = step(float3(linear_start + l0), color);
    const float3 w1 = 1.0 - w0 - w2;
    const float3 toe = linear_start * pow(
        color / linear_start, float3(black)) + pedestal;
    const float3 shoulder = peak - (peak - s1) * exp(
        cp * (color - s0));
    const float3 linear = linear_start + contrast *
        (color - linear_start);
    return toe * w0 + linear * w1 + shoulder * w2;
}

static float3 almetal_pbr_allenwp(float3 color, float4 params)
{
    constexpr float crossover = 0.18;
    constexpr float shoulder_max = 1.0 - crossover;
    float3 shoulder = color - crossover;
    const float3 slope_shoulder = params.z * shoulder;
    shoulder = slope_shoulder * (1.0 + shoulder / params.w) /
               (1.0 + slope_shoulder / shoulder_max) + crossover;
    float3 toe = pow(color, float3(params.x));
    toe = toe / (toe + params.y);
    return select(shoulder, toe, color < float3(crossover));
}

static float3 almetal_pbr_agx(float3 color, float4 params)
{
    const float3x3 inset = float3x3(
        float3(0.544814746488245, 0.140416948464053, 0.0888104196149096),
        float3(0.373787398372697, 0.754137554567394, 0.178871756420858),
        float3(0.0813978551390581, 0.105445496968552, 0.732317823964232));
    const float3x3 outset = float3x3(
        float3(1.96488741169489, -0.299313364904742, -0.164352742528393),
        float3(-0.855988495690215, 1.32639796461980, -0.238183969428088),
        float3(-0.108898916004672, -0.0270845997150571, 1.40253671195648));
    color = inset * color;
    color = almetal_pbr_allenwp(color, params);
    color = min(float3(1.0), color);
    return outset * color;
}

static float3 almetal_pbr_tonemap(
    float3 color, uint type, float4 params)
{
    color = max(color, float3(0.0));
    switch (type)
    {
        case 0: return almetal_pbr_neutral_tonemap(color);
        case 1: return almetal_pbr_aces_hill(color);
        case 2: return almetal_pbr_aces_boosted(color, params.x);
        case 3: return almetal_pbr_reinhard(color, params.x);
        case 4: return almetal_pbr_filmic(color, params.x);
        case 5: return almetal_pbr_uchimura(color);
        case 6: return almetal_pbr_agx(color, params);
    }
    return color;
}

fragment float4 almetal_pbr_preview_fragment(
    ALMetalPBRPreviewVaryings input [[stage_in]],
    bool front_facing [[front_facing]],
    texture2d<float> base_color_map [[texture(0)]],
    texture2d<float> normal_map [[texture(1)]],
    texture2d<float> orm_map [[texture(2)]],
    texture2d<float> emissive_map [[texture(3)]],
    texture2d<float> brdf_lut [[texture(4)]],
    texturecube<float> radiance_map [[texture(5)]],
    sampler color_sampler [[sampler(0)]],
    sampler normal_sampler [[sampler(1)]],
    sampler orm_sampler [[sampler(2)]],
    sampler emissive_sampler [[sampler(3)]],
    sampler brdf_sampler [[sampler(4)]],
    sampler environment_sampler [[sampler(5)]],
    constant ALMetalPBRPreviewConstants& constants [[buffer(0)]],
    constant ALMetalPBRPreviewEnvironmentConstants& environment
        [[buffer(1)]])
{
    const float4 base_sample = base_color_map.sample(
        color_sampler, input.base_color_texcoord);
    const float coverage = base_sample.a * input.color.a;
    if (coverage < constants.material.z)
    {
        discard_fragment();
    }

    const float3 base_color = base_sample.rgb *
                              almetal_srgb_to_linear(input.color.rgb);
    const float3 tangent_normal = normal_map.sample(
        normal_sampler, input.normal_texcoord).xyz * 2.0 - 1.0;
    const float3 vertex_normal = normalize(input.normal);
    const float3 vertex_tangent = normalize(input.tangent);
    const float3 vertex_binormal = input.tangent_sign *
                                   cross(vertex_normal, vertex_tangent);
    float3 normal = normalize(tangent_normal.x * vertex_tangent +
                              tangent_normal.y * vertex_binormal +
                              tangent_normal.z * vertex_normal);
    normal *= front_facing ? 1.0 : -1.0;
    const float3 geometric_normal = vertex_normal *
        (front_facing ? 1.0 : -1.0);

    const float3 orm = orm_map.sample(orm_sampler, input.orm_texcoord).rgb;
    const float ao = clamp(orm.r, 0.0, 1.0);
    const float authored_roughness = clamp(
        orm.g * constants.material.y, 0.0, 1.0);
    const float roughness = almetal_pbr_filtered_roughness(
        authored_roughness, normal);
    const float metallic = clamp(
        orm.b * constants.material.x, 0.0, 1.0);
    const float3 view = normalize(-input.eye_position);
    const float3 light = normalize(constants.sun_direction.xyz);
    const float nv = clamp(abs(dot(normal, view)), 0.001, 1.0);
    const float3 diffuse_color = base_color * (1.0 - metallic);
    const float3 specular_color = mix(float3(0.04), base_color, metallic);
    const float alpha = roughness * roughness;
    const float sun_roughness = sqrt(clamp(alpha + 0.00465, 0.0, 1.0));
    ALMetalPBRPreviewPunctual punctual = almetal_pbr_punctual(
        diffuse_color, specular_color, sun_roughness,
        normal, view, light);
    punctual.specular *= almetal_pbr_energy_compensation(
        brdf_lut, brdf_sampler, specular_color, max(roughness, 0.045), nv);
    const float3 direct = almetal_pbr_clamp_radiance(
        punctual.nl * (punctual.diffuse + punctual.specular)) *
        constants.sun_color.rgb * 3.0;

    const float3 ambient = almetal_pbr_ambient(normal, light, environment);
    const float3 irradiance = almetal_pbr_probe_irradiance(
        input.eye_position, normal, ambient, environment);
    const float3 mirror_direction = normalize(reflect(-view, normal));
    const float3 dominant_direction = almetal_pbr_dominant_direction(
        normal, mirror_direction, roughness);
    const float3 probe_direction = almetal_pbr_probe_direction(
        input.eye_position, dominant_direction, environment);
    float3 radiance = radiance_map.sample(
        environment_sampler, probe_direction,
        level(roughness * environment.probe.z)).rgb * environment.probe.y;
    radiance *= almetal_pbr_horizon_occlusion(
        mirror_direction, geometric_normal);
    const ALMetalPBRPreviewIBL ibl = almetal_pbr_ibl(
        brdf_lut, brdf_sampler, diffuse_color, specular_color,
        radiance, irradiance, ao, nv, max(roughness, 0.045));
    const float3 emissive = constants.emissive_color.rgb *
        emissive_map.sample(emissive_sampler, input.emissive_texcoord).rgb;
    const float3 color = direct + ibl.diffuse + ibl.specular + emissive;

    // Match the GL preview's ordering: alpha-composite the complete linear HDR
    // sphere first. Exposure and tone mapping are a later full-target pass.
    return float4(max(color, float3(0.0)), coverage);
}

struct ALMetalPBRPreviewExposureConstants
{
    float compensation;
    float min_ev;
    float max_ev;
    float low_fraction;
    float high_fraction;
    uint metering_mode;
    uint enabled;
    uint reserved;
};

static float almetal_pbr_preview_meter_weight(float2 uv, uint mode)
{
    const float2 q = uv * 2.0 - 1.0;
    const float distance_squared = dot(q, q);
    if (mode == 0)
    {
        return 1.0;
    }
    if (mode == 1)
    {
        return exp(-2.0 * distance_squared);
    }
    return 1.0 - smoothstep(0.15, 0.3, sqrt(distance_squared));
}

kernel void almetal_pbr_preview_exposure(
    texture2d<float> scene [[texture(0)]],
    texture2d<float, access::write> exposure [[texture(1)]],
    sampler scene_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewExposureConstants& constants [[buffer(0)]],
    uint thread_index [[thread_position_in_grid]])
{
    if (thread_index != 0)
    {
        return;
    }
    if (constants.enabled == 0)
    {
        exposure.write(float4(1.0, 0.0, 0.0, 1.0), uint2(0));
        return;
    }

    constexpr uint meter_width = 128;
    constexpr uint meter_height = 72;
    constexpr uint bins = 64;
    constexpr float min_histogram_ev = -16.0;
    constexpr float max_histogram_ev = 12.0;
    constexpr float bins_per_ev =
        float(bins) / (max_histogram_ev - min_histogram_ev);
    thread float histogram_weight[bins];
    thread float histogram_log[bins];
    for (uint bin = 0; bin < bins; ++bin)
    {
        histogram_weight[bin] = 0.0;
        histogram_log[bin] = 0.0;
    }

    const float2 scene_size = float2(scene.get_width(), scene.get_height());
    const float2 patch_size = scene_size /
                              float2(meter_width, meter_height);
    const uint2 taps = uint2(
        max(1.0, ceil(patch_size.x * 0.5)),
        max(1.0, ceil(patch_size.y * 0.5)));
    const float2 tap_step = patch_size / float2(taps);
    const float tap_count = float(taps.x * taps.y);

    for (uint y = 0; y < meter_height; ++y)
    {
        for (uint x = 0; x < meter_width; ++x)
        {
            const float2 patch_origin = float2(x, y) * patch_size;
            float luminance = 0.0;
            for (uint tap_y = 0; tap_y < taps.y; ++tap_y)
            {
                for (uint tap_x = 0; tap_x < taps.x; ++tap_x)
                {
                    const float2 pixel = patch_origin +
                        (float2(tap_x, tap_y) + 0.5) * tap_step;
                    const float3 color = scene.sample(
                        scene_sampler, pixel / scene_size, level(0.0)).rgb;
                    luminance += min(dot(float3(0.2126, 0.7152, 0.0722),
                                         color),
                                     60000.0);
                }
            }
            luminance /= tap_count;
            const float2 uv = (float2(x, y) + 0.5) /
                              float2(meter_width, meter_height);
            const float weight = almetal_pbr_preview_meter_weight(
                uv, constants.metering_mode);
            if (weight > 0.0 && luminance > 0.0)
            {
                const float ev = clamp(log2(max(luminance, 1.0e-10)),
                                       min_histogram_ev, max_histogram_ev);
                const uint bin = min(
                    uint(floor((ev - min_histogram_ev) * bins_per_ev)),
                    bins - 1);
                histogram_weight[bin] += weight;
                histogram_log[bin] += weight * ev;
            }
        }
    }

    float total = 0.0;
    for (uint bin = 0; bin < bins; ++bin)
    {
        total += histogram_weight[bin];
    }
    const float low = total * constants.low_fraction;
    const float high = total * constants.high_fraction;
    float below = 0.0;
    float kept = 0.0;
    float kept_log = 0.0;
    for (uint bin = 0; bin < bins; ++bin)
    {
        const float bin_weight = histogram_weight[bin];
        const float from = max(low, below);
        const float to = min(high, below + bin_weight);
        if (to > from)
        {
            const float share = (to - from) / bin_weight;
            kept += bin_weight * share;
            kept_log += histogram_log[bin] * share;
        }
        below += bin_weight;
    }

    constexpr float exposure_key = 0.18;
    float ev = kept > 0.0
        ? kept_log / kept - (log2(exposure_key) + constants.compensation)
        : constants.min_ev;
    ev = clamp(ev, constants.min_ev, constants.max_ev);
    exposure.write(float4(exp2(-ev), ev, ev, 1.0), uint2(0));
}

struct ALMetalPBRPreviewPostConstants
{
    // static exposure, tonemap mix, tonemap type, target is sRGB
    float4 post;
    float4 tonemap_params;
    // bloom strength, halation strength, enabled, cross-filter strength
    float4 bloom;
    float4 halation_tint;
    // squared-and-scaled amount, reciprocal falloff, anisotropy
    float4 chromatic;
    // angle sin/cos, red radial/tangential offset
    float4 chromatic_angle_offset_r;
    // blue radial/tangential offset
    float4 chromatic_offset_b;
};

static float4 almetal_pbr_preview_chromatic_aberration(
    texture2d<float> scene,
    sampler scene_sampler,
    float2 uv,
    constant ALMetalPBRPreviewPostConstants& constants)
{
    const float amount = constants.chromatic.x;
    if (amount <= 0.0)
    {
        return scene.sample(scene_sampler, uv);
    }

    float2 direction = uv - 0.5;
    const float anisotropy = constants.chromatic.z;
    direction *= float2(1.0 - max(anisotropy, 0.0),
                        1.0 - max(-anisotropy, 0.0));

    const float aspect = float(scene.get_width()) /
                         max(float(scene.get_height()), 1.0);
    const float2 scale = max(
        float2(aspect, 1.0 / max(aspect, 1.0e-4)), float2(1.0));
    const float normalized_distance = clamp(
        length(direction * scale) / (0.5 * length(scale)), 0.0, 1.0);
    const float strength = amount * pow(
        normalized_distance, constants.chromatic.y);

    const float sine = constants.chromatic_angle_offset_r.x;
    const float cosine = constants.chromatic_angle_offset_r.y;
    const float2 radial = float2(
        cosine * direction.x - sine * direction.y,
        sine * direction.x + cosine * direction.y);
    const float2 tangential = float2(-radial.y, radial.x);
    const float2 red_offset =
        radial * constants.chromatic_angle_offset_r.z +
        tangential * constants.chromatic_angle_offset_r.w;
    const float2 blue_offset =
        radial * constants.chromatic_offset_b.x +
        tangential * constants.chromatic_offset_b.y;

    float4 color;
    color.r = scene.sample(
        scene_sampler, uv + red_offset * strength).r;
    const float4 center = scene.sample(scene_sampler, uv);
    color.g = center.g;
    color.a = center.a;
    color.b = scene.sample(
        scene_sampler, uv + blue_offset * strength).b;
    return color;
}

struct ALMetalPBRPreviewBloomExtractConstants
{
    // threshold, knee, firefly clamp, alpha glow boost
    float4 params;
    float4 halation_weights;
};

struct ALMetalPBRPreviewBloomFilterConstants
{
    // inverse source width/height, scatter or first-downsample flag
    float4 params;
};

struct ALMetalPBRPreviewCrossFilterConstants
{
    // target texel x/y, authored length, chromatic dispersion
    float4 sampling;
    // arm direction x/y, pass scale, exponential falloff base
    float4 filtering;
};

static float almetal_pbr_preview_bloom_soft_threshold(
    float luminance, float threshold, float knee)
{
    const float width = max(knee, 1.0e-4);
    float soft = clamp(luminance - threshold + width,
                       0.0, 2.0 * width);
    soft = soft * soft / (4.0 * width + 1.0e-4);
    return max(soft, luminance - threshold);
}

fragment float4 almetal_pbr_preview_bloom_extract_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> scene [[texture(0)]],
    sampler scene_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewBloomExtractConstants& constants [[buffer(0)]])
{
    const float4 source = scene.sample(scene_sampler, input.texcoord);
    float3 color = max(source.rgb, float3(0.0));
    const float peak = max(color.r, max(color.g, color.b));
    color *= peak > constants.params.z ? constants.params.z / peak : 1.0;
    const float luminance = max(color.r, max(color.g, color.b));
    const float factor = almetal_pbr_preview_bloom_soft_threshold(
        luminance, constants.params.x, constants.params.y) /
        max(luminance, 1.0e-4);
    constexpr float legacy_tonemap_compensation = 2.0;
    const float3 bloom = color * factor +
        color * source.a * constants.params.w *
        legacy_tonemap_compensation;
    const float warmth = max(
        color.r * constants.halation_weights.r,
        max(color.g * constants.halation_weights.g,
            color.b * constants.halation_weights.b));
    const float halation = almetal_pbr_preview_bloom_soft_threshold(
        warmth, constants.params.x, constants.params.y);
    return float4(bloom, halation);
}

static float almetal_pbr_preview_bloom_max_luminance(float4 color)
{
    return max(color.r, max(color.g, color.b));
}

static float4 almetal_pbr_preview_bloom_karis_box(
    float4 a, float4 b, float4 c, float4 d)
{
    const float la = almetal_pbr_preview_bloom_max_luminance(a);
    const float lb = almetal_pbr_preview_bloom_max_luminance(b);
    const float lc = almetal_pbr_preview_bloom_max_luminance(c);
    const float ld = almetal_pbr_preview_bloom_max_luminance(d);
    const float wa = 1.0 / (1.0 + max(la - max(max(lb, lc), ld), 0.0));
    const float wb = 1.0 / (1.0 + max(lb - max(max(la, lc), ld), 0.0));
    const float wc = 1.0 / (1.0 + max(lc - max(max(la, lb), ld), 0.0));
    const float wd = 1.0 / (1.0 + max(ld - max(max(la, lb), lc), 0.0));
    return (a * wa + b * wb + c * wc + d * wd) /
           (wa + wb + wc + wd);
}

fragment float4 almetal_pbr_preview_bloom_downsample_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewBloomFilterConstants& constants [[buffer(0)]])
{
    const float2 uv = input.texcoord;
    const float2 texel = constants.params.xy;
    const float4 a = source.sample(
        source_sampler, uv + texel * float2(-2.0, -2.0));
    const float4 b = source.sample(
        source_sampler, uv + texel * float2( 0.0, -2.0));
    const float4 c = source.sample(
        source_sampler, uv + texel * float2( 2.0, -2.0));
    const float4 d = source.sample(
        source_sampler, uv + texel * float2(-1.0, -1.0));
    const float4 e = source.sample(
        source_sampler, uv + texel * float2( 1.0, -1.0));
    const float4 f = source.sample(
        source_sampler, uv + texel * float2(-2.0,  0.0));
    const float4 g = source.sample(source_sampler, uv);
    const float4 h = source.sample(
        source_sampler, uv + texel * float2( 2.0,  0.0));
    const float4 i = source.sample(
        source_sampler, uv + texel * float2(-1.0,  1.0));
    const float4 j = source.sample(
        source_sampler, uv + texel * float2( 1.0,  1.0));
    const float4 k = source.sample(
        source_sampler, uv + texel * float2(-2.0,  2.0));
    const float4 l = source.sample(
        source_sampler, uv + texel * float2( 0.0,  2.0));
    const float4 m = source.sample(
        source_sampler, uv + texel * float2( 2.0,  2.0));

    float4 group0;
    float4 group1;
    float4 group2;
    float4 group3;
    float4 group4;
    if (constants.params.z > 0.5)
    {
        group0 = almetal_pbr_preview_bloom_karis_box(d, e, i, j);
        group1 = almetal_pbr_preview_bloom_karis_box(a, b, f, g);
        group2 = almetal_pbr_preview_bloom_karis_box(b, c, g, h);
        group3 = almetal_pbr_preview_bloom_karis_box(f, g, k, l);
        group4 = almetal_pbr_preview_bloom_karis_box(g, h, l, m);
    }
    else
    {
        group0 = (d + e + i + j) * 0.25;
        group1 = (a + b + f + g) * 0.25;
        group2 = (b + c + g + h) * 0.25;
        group3 = (f + g + k + l) * 0.25;
        group4 = (g + h + l + m) * 0.25;
    }
    return group0 * 0.5 +
           (group1 + group2 + group3 + group4) * 0.125;
}

fragment float4 almetal_pbr_preview_bloom_upsample_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewBloomFilterConstants& constants [[buffer(0)]])
{
    const float2 uv = input.texcoord;
    const float2 texel = constants.params.xy * constants.params.z;
    float4 result = source.sample(
        source_sampler, uv + texel * float2(-1.0, -1.0)) * (1.0 / 16.0);
    result += source.sample(
        source_sampler, uv + texel * float2( 0.0, -1.0)) * (2.0 / 16.0);
    result += source.sample(
        source_sampler, uv + texel * float2( 1.0, -1.0)) * (1.0 / 16.0);
    result += source.sample(
        source_sampler, uv + texel * float2(-1.0,  0.0)) * (2.0 / 16.0);
    result += source.sample(source_sampler, uv) * (4.0 / 16.0);
    result += source.sample(
        source_sampler, uv + texel * float2( 1.0,  0.0)) * (2.0 / 16.0);
    result += source.sample(
        source_sampler, uv + texel * float2(-1.0,  1.0)) * (1.0 / 16.0);
    result += source.sample(
        source_sampler, uv + texel * float2( 0.0,  1.0)) * (2.0 / 16.0);
    result += source.sample(
        source_sampler, uv + texel * float2( 1.0,  1.0)) * (1.0 / 16.0);
    return result;
}

fragment float4 almetal_pbr_preview_cross_filter_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewCrossFilterConstants& constants [[buffer(0)]])
{
    constexpr int taps = 4;
    constexpr float3 dispersion = float3(0.3, 0.0, -0.3);
    const float length = constants.sampling.z;
    float3 stride = float3(1.0) +
        clamp(constants.sampling.w, 0.0, 1.0) * dispersion;
    stride = min(stride, float3(2.0 / max(length, 1.0e-4)));

    float3 accumulated = float3(0.0);
    float total_weight = 0.0;
    for (int tap = 0; tap < taps; ++tap)
    {
        const float step_index =
            float(tap) * constants.filtering.z;
        const float2 offset = constants.filtering.xy *
            constants.sampling.xy * length * step_index;
        const float weight = pow(constants.filtering.w, -step_index);

        float3 color;
        if (constants.sampling.w > 0.0)
        {
            color.r = source.sample(
                source_sampler, input.texcoord + offset * stride.r).r;
            color.g = source.sample(
                source_sampler, input.texcoord + offset * stride.g).g;
            color.b = source.sample(
                source_sampler, input.texcoord + offset * stride.b).b;
        }
        else
        {
            color = source.sample(
                source_sampler, input.texcoord + offset).rgb;
        }
        accumulated += color * weight;
        total_weight += weight;
    }
    return float4(accumulated / max(total_weight, 1.0e-4), 0.0);
}

fragment float4 almetal_pbr_preview_post_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> scene [[texture(0)]],
    texture2d<float> exposure [[texture(1)]],
    texture2d<float> bloom_map [[texture(2)]],
    texture2d<float> cross_filter_map [[texture(3)]],
    sampler scene_sampler [[sampler(0)]],
    sampler bloom_sampler [[sampler(1)]],
    sampler cross_filter_sampler [[sampler(2)]],
    constant ALMetalPBRPreviewPostConstants& constants [[buffer(0)]])
{
    const float4 scene_color = almetal_pbr_preview_chromatic_aberration(
        scene, scene_sampler, input.texcoord, constants);
    float3 color = scene_color.rgb;
    if (constants.bloom.z > 0.5)
    {
        const float4 bloom = bloom_map.sample(
            bloom_sampler, input.texcoord);
        color += bloom.rgb * constants.bloom.x +
                 bloom.a * constants.bloom.y *
                     constants.halation_tint.rgb;
        if (constants.bloom.w > 0.0)
        {
            color += cross_filter_map.sample(
                cross_filter_sampler, input.texcoord).rgb *
                constants.bloom.w * constants.bloom.x;
        }
    }
    color *= constants.post.x * exposure.read(uint2(0)).r;
    const float3 clamped = clamp(color, float3(0.0), float3(1.0));
    const uint tonemap_type = uint(round(constants.post.z));
    color = mix(clamped,
                clamp(almetal_pbr_tonemap(
                          color, tonemap_type, constants.tonemap_params),
                      float3(0.0), float3(1.0)),
                constants.post.y);
    // An sRGB attachment performs the final transfer itself. The ordinary
    // RGBA8 dynamic texture stores the same encoded bytes explicitly.
    if (constants.post.w < 0.5)
    {
        color = almetal_linear_to_srgb(color);
    }
    return float4(max(color, float3(0.0)), scene_color.a);
}

fragment float4 almetal_lit_preview_deferred_rigged_fragment(
    ALMetalLitPreviewTexturedVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant float& alpha_cutoff [[buffer(1)]])
{
    // The GL deferred pass decodes both the sRGB texture and its sRGB vertex
    // tint, multiplies in linear space, then re-encodes into an sRGB target.
    // Metal's ordinary texture view performs the texture decode when its
    // storage is sRGB. The dynamic texture mirror is linear RGBA8, so decode
    // the tint and explicitly perform the target encode here.
    const float4 diffuse = diffuse_map.sample(diffuse_sampler, input.texcoord);
    const float coverage = diffuse.a * input.color.a;
    if (coverage < alpha_cutoff - 0.001953125)
    {
        discard_fragment();
    }
    const float3 linear = diffuse.rgb *
                          almetal_srgb_to_linear(input.color.rgb);
    return float4(almetal_linear_to_srgb(linear), 1.0);
}

fragment float4 almetal_lit_preview_pbr_blend_rigged_fragment(
    ALMetalLitPreviewTexturedVaryings input [[stage_in]],
    float4 destination [[color(0), raster_order_group(0)]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant float& minimum_alpha [[buffer(1)]])
{
    const float4 diffuse = diffuse_map.sample(diffuse_sampler, input.texcoord);
    const float coverage = diffuse.a * input.color.a;
    if (coverage < minimum_alpha)
    {
        discard_fragment();
    }

    // pbralphaF's FOR_IMPOSTOR branch emits unlit base colour and coverage.
    // The GL target is sRGB, so its fixed-function blend decodes the existing
    // destination, blends in linear space, and encodes the store. Visual hints
    // own ordinary RGBA8 bytes containing that encoded result; programmable
    // blending reproduces both RGB and the alpha pool's coverage recurrence.
    const float3 source_linear = diffuse.rgb *
                                 almetal_srgb_to_linear(input.color.rgb);
    const float3 destination_linear =
        almetal_srgb_to_linear(destination.rgb);
    const float3 blended_linear = source_linear * coverage +
                                  destination_linear * (1.0 - coverage);
    const float blended_coverage = coverage +
                                   destination.a * (1.0 - coverage);
    return float4(almetal_linear_to_srgb(blended_linear),
                  blended_coverage);
}

fragment float4 almetal_lit_preview_pbr_glow_rigged_fragment(
    ALMetalLitPreviewPBRGlowVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    texture2d<float> emissive_map [[texture(1)]],
    sampler diffuse_sampler [[sampler(0)]],
    sampler emissive_sampler [[sampler(1)]],
    constant float& alpha_cutoff [[buffer(1)]])
{
    const float4 diffuse =
        diffuse_map.sample(diffuse_sampler, input.base_color_texcoord);
    const float coverage = diffuse.a * input.color.a;
    if (coverage < alpha_cutoff - 0.001953125)
    {
        discard_fragment();
    }

    const float3 linear = diffuse.rgb *
                          almetal_srgb_to_linear(input.color.rgb);
    const float3 emissive = input.emissive_color *
        emissive_map.sample(emissive_sampler, input.emissive_texcoord).rgb;
    const float glow = max(max(emissive.r, emissive.g), emissive.b) *
                       input.emissive.a;

    // The GL owner first stamps opaque coverage to one and then overwrites
    // alpha with pbrglowF's luminance. Combining those two owner passes is
    // exact for opaque PBR and avoids a second depth-equal native draw.
    return float4(almetal_linear_to_srgb(linear), glow);
}

fragment float4 almetal_lit_preview_legacy_material_rigged_fragment(
    ALMetalLitPreviewTexturedVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant float& alpha_cutoff [[buffer(1)]])
{
    const float4 diffuse = diffuse_map.sample(diffuse_sampler, input.texcoord);
    // materialF.glsl's deferred mask tests diffuse alpha alone; vertex alpha
    // is not folded into its G-buffer coverage. A zero cutoff also represents
    // the opaque/emissive permutations without an extra pipeline family.
    if (diffuse.a < alpha_cutoff - 0.001953125)
    {
        discard_fragment();
    }
    const float3 linear = diffuse.rgb *
                          almetal_srgb_to_linear(input.color.rgb);
    // Attachment 0 is sRGB in GL. Opaque coverage is stamped from depth after
    // all deferred pools, so accepted material fragments finish with alpha 1.
    return float4(almetal_linear_to_srgb(linear), 1.0);
}

vertex ALMetalLitPreviewTexturedVaryings almetal_lit_preview_fullbright_mask_rigged_textured_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    const device uchar4* colors [[buffer(4)]],
    const device float4* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    constant uint& joint_count [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    // The fullbright GL vertex path transforms no normal. Reuse the exact
    // object-skin position math with a throwaway direction so fullbright-only
    // vertex buffers do not need to manufacture a normal stream.
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_skin_vertex(
            positions[vertex_id], float4(0.0, 0.0, 1.0, 0.0),
            encoded_weights[vertex_id], skin_palette, joint_count);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = almetal_lit_preview_rebased_clip_position(
        skinned.position, skin_palette[0].xyz, constants);
    output.color = float4(colors[vertex_id]) / 255.0;
    output.texcoord = texcoords[vertex_id];
    return output;
}

fragment float4 almetal_lit_preview_fullbright_rigged_fragment(
    ALMetalLitPreviewTexturedVaryings input [[stage_in]],
    float4 destination [[color(0), raster_order_group(0)]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    const float4 diffuse = diffuse_map.sample(diffuse_sampler, input.texcoord);
    const float3 source_linear = diffuse.rgb *
                                 almetal_srgb_to_linear(input.color.rgb);
    const float coverage = diffuse.a * input.color.a;

    // OpenGL decodes the existing sRGB attachment value, blends the shader's
    // linear result, and re-encodes the store. Visual hints deliberately own
    // linear RGBA8 storage containing those encoded bytes, so programmable
    // blending reproduces the operation without an intermediate target.
    const float3 destination_linear = almetal_srgb_to_linear(destination.rgb);
    const float3 blended_linear = source_linear * coverage +
                                  destination_linear * (1.0 - coverage);
    return float4(almetal_linear_to_srgb(blended_linear), destination.a);
}

fragment float4 almetal_lit_preview_fullbright_mask_rigged_fragment(
    ALMetalLitPreviewTexturedVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant float& alpha_cutoff [[buffer(1)]])
{
    const float4 diffuse = diffuse_map.sample(diffuse_sampler, input.texcoord);
    // Outside the alpha pool, LLFace vertex alpha carries legacy shininess;
    // fullbrightF.glsl therefore applies this mask to texture alpha alone.
    if (diffuse.a < alpha_cutoff - 0.001953125)
    {
        discard_fragment();
    }
    const float3 linear = diffuse.rgb *
                          almetal_srgb_to_linear(input.color.rgb);
    // Visual hints own linear RGBA8 storage, while the GL preview writes this
    // linear result through an sRGB framebuffer. Preserve the stored bytes.
    return float4(almetal_linear_to_srgb(linear), 1.0);
}

fragment float4 almetal_lit_preview_textured_fragment(
    ALMetalLitPreviewTexturedVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    return max(diffuse_map.sample(diffuse_sampler, input.texcoord) *
               input.color, float4(0.0));
}

fragment float4 almetal_lit_preview_avatar_fragment(
    ALMetalLitPreviewTexturedVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    const float4 diffuse = diffuse_map.sample(diffuse_sampler, input.texcoord);
    if (diffuse.a < 0.2)
    {
        discard_fragment();
    }
    return max(diffuse * input.color, float4(0.0));
}

fragment float4 almetal_lit_preview_avatar_unlit_fragment(
    ALMetalLitPreviewTexturedVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    const float4 diffuse = diffuse_map.sample(diffuse_sampler, input.texcoord);
    if (diffuse.a < 0.2)
    {
        discard_fragment();
    }
    return float4(max(diffuse.rgb * input.color.rgb, float3(0.0)), 1.0);
}

static uint almetal_lit_preview_edge_index(uint vertex_id)
{
    const uint triangle_start = (vertex_id / 6u) * 3u;
    switch (vertex_id % 6u)
    {
        case 0u:
        case 5u:
            return triangle_start;
        case 1u:
        case 2u:
            return triangle_start + 1u;
        default:
            return triangle_start + 2u;
    }
}

static bool almetal_lit_preview_edge_triangle_is_back_facing(
    const device float4* positions,
    uint i0,
    uint i1,
    uint i2,
    constant ALMetalLitPreviewConstants& constants)
{
    const float4 p0 = almetal_lit_preview_clip_position(positions[i0], constants);
    const float4 p1 = almetal_lit_preview_clip_position(positions[i1], constants);
    const float4 p2 = almetal_lit_preview_clip_position(positions[i2], constants);
    if (p0.w <= 0.0 || p1.w <= 0.0 || p2.w <= 0.0)
    {
        // Let Metal clip near-plane crossings. The model preview normally
        // keeps all geometry in front of the camera, and discarding before
        // clipping would be less faithful than retaining the edge.
        return false;
    }
    const float2 a = p0.xy / p0.w;
    const float2 b = p1.xy / p1.w;
    const float2 c = p2.xy / p2.w;
    const float2 ab = b - a;
    const float2 ac = c - a;
    // The preview flips clip-space Y to preserve GL framebuffer row order,
    // so its original counter-clockwise front faces are clockwise here.
    return ab.x * ac.y - ab.y * ac.x >= 0.0;
}

static bool almetal_lit_preview_skinned_edge_triangle_is_back_facing(
    const device float4* positions,
    const device float4* normals,
    const device float4* encoded_weights,
    const device float4* skin_palette,
    uint joint_count,
    uint i0,
    uint i1,
    uint i2,
    constant ALMetalLitPreviewConstants& constants)
{
    const ALMetalLitPreviewSkinnedVertex v0 = almetal_lit_preview_skin_vertex(
        positions[i0], normals[i0], encoded_weights[i0],
        skin_palette, joint_count);
    const ALMetalLitPreviewSkinnedVertex v1 = almetal_lit_preview_skin_vertex(
        positions[i1], normals[i1], encoded_weights[i1],
        skin_palette, joint_count);
    const ALMetalLitPreviewSkinnedVertex v2 = almetal_lit_preview_skin_vertex(
        positions[i2], normals[i2], encoded_weights[i2],
        skin_palette, joint_count);
    const float3 origin = skin_palette[0].xyz;
    const float4 p0 = almetal_lit_preview_rebased_clip_position(
        v0.position, origin, constants);
    const float4 p1 = almetal_lit_preview_rebased_clip_position(
        v1.position, origin, constants);
    const float4 p2 = almetal_lit_preview_rebased_clip_position(
        v2.position, origin, constants);
    if (p0.w <= 0.0 || p1.w <= 0.0 || p2.w <= 0.0)
    {
        return false;
    }
    const float2 a = p0.xy / p0.w;
    const float2 b = p1.xy / p1.w;
    const float2 c = p2.xy / p2.w;
    const float2 ab = b - a;
    const float2 ac = c - a;
    return ab.x * ac.y - ab.y * ac.x >= 0.0;
}

static ALMetalLitPreviewVaryings almetal_lit_preview_edge_vertex_common(
    const device float4* positions,
    const device float4* normals,
    uint source_id,
    uint i0,
    uint i1,
    uint i2,
    constant ALMetalLitPreviewConstants& constants)
{
    ALMetalLitPreviewVaryings output = almetal_lit_preview_vertex_common(
        positions[source_id], normals[source_id], constants);
    if (almetal_lit_preview_edge_triangle_is_back_facing(
            positions, i0, i1, i2, constants))
    {
        // A degenerate line entirely outside the clip volume reproduces the
        // triangle culling that GL applies before polygon-line rasterization.
        output.position = float4(2.0, 2.0, 2.0, 1.0);
    }
    return output;
}

static ALMetalLitPreviewVaryings almetal_lit_preview_skinned_edge_vertex_common(
    const device float4* positions,
    const device float4* normals,
    const device float4* encoded_weights,
    const device float4* skin_palette,
    uint joint_count,
    uint source_id,
    uint i0,
    uint i1,
    uint i2,
    constant ALMetalLitPreviewConstants& constants)
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_skin_vertex(
            positions[source_id], normals[source_id],
            encoded_weights[source_id], skin_palette, joint_count);
    ALMetalLitPreviewVaryings output = almetal_lit_preview_vertex_common(
        skinned.position, skinned.normal, constants);
    output.position = almetal_lit_preview_rebased_clip_position(
        skinned.position, skin_palette[0].xyz, constants);
    if (almetal_lit_preview_skinned_edge_triangle_is_back_facing(
            positions, normals, encoded_weights, skin_palette, joint_count,
            i0, i1, i2, constants))
    {
        output.position = float4(2.0, 2.0, 2.0, 1.0);
    }
    return output;
}

vertex ALMetalLitPreviewVaryings almetal_lit_preview_edge_u16_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device ushort* indices [[buffer(4)]],
    constant uint& index_element_offset [[buffer(5)]],
    uint vertex_id [[vertex_id]])
{
    const uint triangle_start = index_element_offset + (vertex_id / 6u) * 3u;
    const uint source_id = indices[index_element_offset +
                                   almetal_lit_preview_edge_index(vertex_id)];
    return almetal_lit_preview_edge_vertex_common(
        positions, normals, source_id,
        indices[triangle_start], indices[triangle_start + 1u],
        indices[triangle_start + 2u], constants);
}

vertex ALMetalLitPreviewVaryings almetal_lit_preview_edge_u32_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device uint* indices [[buffer(4)]],
    constant uint& index_element_offset [[buffer(5)]],
    uint vertex_id [[vertex_id]])
{
    const uint triangle_start = index_element_offset + (vertex_id / 6u) * 3u;
    const uint source_id = indices[index_element_offset +
                                   almetal_lit_preview_edge_index(vertex_id)];
    return almetal_lit_preview_edge_vertex_common(
        positions, normals, source_id,
        indices[triangle_start], indices[triangle_start + 1u],
        indices[triangle_start + 2u], constants);
}

vertex ALMetalLitPreviewVaryings almetal_lit_preview_skinned_edge_u16_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device ushort* indices [[buffer(4)]],
    constant uint& index_element_offset [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    constant uint& joint_count [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    const uint triangle_start = index_element_offset + (vertex_id / 6u) * 3u;
    const uint source_id = indices[index_element_offset +
                                   almetal_lit_preview_edge_index(vertex_id)];
    return almetal_lit_preview_skinned_edge_vertex_common(
        positions, normals, encoded_weights, skin_palette, joint_count,
        source_id, indices[triangle_start], indices[triangle_start + 1u],
        indices[triangle_start + 2u], constants);
}

vertex ALMetalLitPreviewVaryings almetal_lit_preview_skinned_edge_u32_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    constant ALMetalLitPreviewConstants& constants [[buffer(2)]],
    const device uint* indices [[buffer(4)]],
    constant uint& index_element_offset [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    const device float4* skin_palette [[buffer(7)]],
    constant uint& joint_count [[buffer(8)]],
    uint vertex_id [[vertex_id]])
{
    const uint triangle_start = index_element_offset + (vertex_id / 6u) * 3u;
    const uint source_id = indices[index_element_offset +
                                   almetal_lit_preview_edge_index(vertex_id)];
    return almetal_lit_preview_skinned_edge_vertex_common(
        positions, normals, encoded_weights, skin_palette, joint_count,
        source_id, indices[triangle_start], indices[triangle_start + 1u],
        indices[triangle_start + 2u], constants);
}

vertex ALMetalUIVaryings almetal_ui_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    const device uchar4* colors [[buffer(2)]],
    constant ALMetalMatrices& matrices [[buffer(3)]],
    constant ALMetalVertexRemapConstants& remap [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const uint source_id = almetal_source_vertex(vertex_id, remap);
    ALMetalUIVaryings output;
    float4 clip_position =
        matrices.modelview_projection * float4(positions[source_id].xyz, 1.0);
    if (remap.flip_y != 0)
    {
        // Offscreen UI targets become GL-owned texture mirrors. Preserve the
        // lower-left row-zero byte convention used by glCopyTexSubImage.
        clip_position.y = -clip_position.y;
    }
    output.position = almetal_clip_from_gl(clip_position);
    output.texcoord = (matrices.texture0 * float4(texcoords[source_id], 0.0, 1.0)).xy;
    output.color = float4(colors[source_id]) * (1.0 / 255.0);
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

    float product;
    if (constants.text_shadow_mode == 1)
    {
        product = 1.0 - vertex_alpha *
            diffuse_map.sample(diffuse_sampler,
                               input.texcoord + atlas_texel_size * float2(-1.0, 1.0)).a;
    }
    else
    {
        product  = 1.0 - vertex_alpha *
            diffuse_map.sample(diffuse_sampler,
                               input.texcoord + atlas_texel_size * float2( 1.0,  1.0)).a;
        product *= 1.0 - vertex_alpha *
            diffuse_map.sample(diffuse_sampler,
                               input.texcoord + atlas_texel_size * float2(-1.0,  1.0)).a;
        product *= 1.0 - vertex_alpha *
            diffuse_map.sample(diffuse_sampler,
                               input.texcoord + atlas_texel_size * float2(-1.0, -1.0)).a;
        product *= 1.0 - vertex_alpha *
            diffuse_map.sample(diffuse_sampler,
                               input.texcoord + atlas_texel_size * float2( 1.0, -1.0)).a;
        product *= 1.0 - vertex_alpha *
            diffuse_map.sample(diffuse_sampler,
                               input.texcoord + atlas_texel_size * float2( 0.0,  2.0)).a;
    }
    return float4(input.color.rgb, 1.0 - product);
}

fragment float4 almetal_alpha_mask_fragment(
    ALMetalUIVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant ALMetalAlphaMaskFragmentConstants& constants [[buffer(0)]])
{
    const float4 color = input.color *
                         diffuse_map.sample(diffuse_sampler, input.texcoord);
    if (color.a < constants.minimum_alpha)
    {
        discard_fragment();
    }
    return max(color, float4(0.0));
}

fragment float4 almetal_alpha_mask_solid_fragment(
    ALMetalUIVaryings input [[stage_in]],
    constant ALMetalAlphaMaskFragmentConstants& constants [[buffer(0)]])
{
    if (input.color.a < constants.minimum_alpha)
    {
        discard_fragment();
    }
    return max(input.color, float4(0.0));
}

vertex ALMetalUIVaryings almetal_solid_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalMatrices& matrices [[buffer(3)]],
    constant ALMetalVertexRemapConstants& remap [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const uint source_id = almetal_source_vertex(vertex_id, remap);
    ALMetalUIVaryings output;
    float4 clip_position =
        matrices.modelview_projection * float4(positions[source_id].xyz, 1.0);
    if (remap.flip_y != 0)
    {
        clip_position.y = -clip_position.y;
    }
    output.position = almetal_clip_from_gl(clip_position);
    output.texcoord = texcoords[source_id];
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
