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

struct ALMetalVertexRemapConstants
{
    uint mode;
    uint source_count;
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

// Exact position half of avatarSkinV.glsl's getClothedPosition(). The hint
// path is unlit, so the routine need not carry the final perturbed normal past
// the point where the legacy formula has finished deriving displacement.
static float4 almetal_lit_preview_avatar_cloth_position(
    float4 position,
    float4 normal_value,
    float4 clothing,
    float encoded_weight,
    const device float4* skin_palette,
    constant ALMetalLitPreviewConstants& constants)
{
    const ALMetalLitPreviewAvatarRows rows =
        almetal_lit_preview_avatar_rows(encoded_weight, skin_palette);
    const float3 source_normal = normal_value.xyz;
    float3 normal = normalize(float3(
        dot(cross(rows.row1.xyz, rows.row2.xyz), source_normal),
        dot(cross(rows.row2.xyz, rows.row0.xyz), source_normal),
        dot(cross(rows.row0.xyz, rows.row1.xyz), source_normal)));

    const float wind_dot = dot(normal, constants.avatar_wind.xyz);
    float4 wind_effect = float4(wind_dot);
    const float4 eye_position = float4(
        dot(rows.row0, position), dot(rows.row1, position),
        dot(rows.row2, position), 1.0);
    const float main_depth = dot(constants.avatar_cloth_depth, eye_position);
    wind_effect.xyz = main_depth * float3(0.015) + wind_effect.xyz;
    wind_effect.w = (wind_effect.w * 2.0 + 1.0) *
                    constants.avatar_wind.w;
    wind_effect.xyz = wind_effect.xyz * constants.avatar_sin_wave.xyz +
                      float3(constants.avatar_sin_wave.w);

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
    sin_wave.xyz = sin_wave.xyz * constants.avatar_wind.w +
                   float3(wind_effect.w);

    float4 gravity = float4(dot(normal, constants.avatar_gravity.xyz));
    gravity = min(gravity, float4(0.2, 0.0, 0.0, 0.0));
    gravity *= float4(1.5, 0.0, 0.0, 0.0);
    sin_wave.x += gravity.x;
    sin_wave.xyz *= clothing.w;
    sin_wave.xyz = max(sin_wave.xyz, float3(-1.0));

    const float4 offset_position =
        float4(1.0, 1.0, 1.0, 0.0) * (clothing * sin_wave.x) + position;
    return float4(dot(rows.row0, offset_position),
                  dot(rows.row1, offset_position),
                  dot(rows.row2, offset_position), 1.0);
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
    float4 weights = fract(encoded_weights);
    const float weight_sum = dot(weights, float4(1.0));
    // Imported skin weights are guaranteed non-zero by LLModel::loadModel.
    // Keep malformed data finite here so a preview cannot poison a frame.
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
    ALMetalLitPreviewSkinnedVertex output;
    output.position = float4(
        skin_matrix * position.xyz +
            float3(column0.w, column1.w, column2.w),
        0.0);
    // Match objectSkinV.glsl's cofactor transform. It is the inverse
    // transpose times the determinant and is normalized by the shared lit
    // preview path afterwards.
    const float3x3 skin_normal_matrix = float3x3(
        cross(skin_matrix[1], skin_matrix[2]),
        cross(skin_matrix[2], skin_matrix[0]),
        cross(skin_matrix[0], skin_matrix[1]));
    output.normal = float4(skin_normal_matrix * normal_value.xyz, 0.0);
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
    const float4 skinned_position =
        almetal_lit_preview_avatar_cloth_position(
            positions[vertex_id], normals[vertex_id], clothing[vertex_id],
            encoded_weights[vertex_id], skin_palette, constants);
    ALMetalLitPreviewTexturedVaryings output;
    output.position = almetal_lit_preview_clip_position(
        skinned_position, constants);
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
    output.position = almetal_clip_from_gl(
        matrices.modelview_projection * float4(positions[source_id].xyz, 1.0));
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

vertex ALMetalUIVaryings almetal_solid_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalMatrices& matrices [[buffer(3)]],
    constant ALMetalVertexRemapConstants& remap [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const uint source_id = almetal_source_vertex(vertex_id, remap);
    ALMetalUIVaryings output;
    output.position = almetal_clip_from_gl(
        matrices.modelview_projection * float4(positions[source_id].xyz, 1.0));
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
