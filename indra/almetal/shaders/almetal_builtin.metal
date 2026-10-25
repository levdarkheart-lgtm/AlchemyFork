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

static float almetal_pbr_distribution(float3 normal, float3 half_vector,
                                      float roughness)
{
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;
    const float nh = max(dot(normal, half_vector), 0.0);
    const float denominator = nh * nh * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(M_PI_F * denominator * denominator, 1.0e-6);
}

static float almetal_pbr_geometry_term(float cosine, float roughness)
{
    const float k = (roughness + 1.0) * (roughness + 1.0) * 0.125;
    return cosine / max(cosine * (1.0 - k) + k, 1.0e-6);
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
    sampler color_sampler [[sampler(0)]],
    sampler normal_sampler [[sampler(1)]],
    sampler orm_sampler [[sampler(2)]],
    sampler emissive_sampler [[sampler(3)]],
    constant ALMetalPBRPreviewConstants& constants [[buffer(0)]])
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

    const float3 orm = orm_map.sample(orm_sampler, input.orm_texcoord).rgb;
    const float ao = clamp(orm.r, 0.0, 1.0);
    const float roughness = clamp(
        orm.g * constants.material.y, 0.045, 1.0);
    const float metallic = clamp(
        orm.b * constants.material.x, 0.0, 1.0);
    const float3 view = normalize(-input.eye_position);
    const float3 light = normalize(constants.sun_direction.xyz);
    const float3 half_vector = normalize(view + light);
    const float nv = max(dot(normal, view), 0.001);
    const float nl = max(dot(normal, light), 0.0);
    const float vh = max(dot(view, half_vector), 0.0);
    const float3 f0 = mix(float3(0.04), base_color, metallic);
    const float3 fresnel = almetal_pbr_fresnel(vh, f0);
    const float distribution = almetal_pbr_distribution(
        normal, half_vector, roughness);
    const float geometry = almetal_pbr_geometry_term(nv, roughness) *
                           almetal_pbr_geometry_term(nl, roughness);
    const float3 specular = distribution * geometry * fresnel /
                            max(4.0 * nv * max(nl, 0.001), 0.001);
    const float3 diffuse_weight = (1.0 - fresnel) * (1.0 - metallic);
    const float3 direct = (diffuse_weight * base_color / M_PI_F + specular) *
                          constants.sun_color.rgb * nl;

    // The GL owner forces the default reflection probe and a white sun. This
    // compact split-sum approximation keeps roughness/metal/occlusion response
    // in the native swatch without borrowing world-owned probe textures.
    const float3 ambient_diffuse = base_color * (1.0 - metallic) * 0.18 * ao;
    const float3 ambient_specular = f0 * mix(0.12, 0.035, roughness) * ao;
    const float3 emissive = constants.emissive_color.rgb *
        emissive_map.sample(emissive_sampler, input.emissive_texcoord).rgb;
    const float3 color = direct + ambient_diffuse + ambient_specular + emissive;

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
};

fragment float4 almetal_pbr_preview_post_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> scene [[texture(0)]],
    texture2d<float> exposure [[texture(1)]],
    sampler scene_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewPostConstants& constants [[buffer(0)]])
{
    const float4 scene_color = scene.sample(scene_sampler, input.texcoord);
    float3 color = scene_color.rgb * constants.post.x *
                   exposure.read(uint2(0)).r;
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
