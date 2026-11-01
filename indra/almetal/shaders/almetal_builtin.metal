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

kernel void almetal_test_sample_cube_array(
    texturecube_array<float> source [[texture(0)]],
    device float4* result [[buffer(0)]])
{
    constexpr sampler cube_sampler(
        coord::normalized, filter::nearest, address::clamp_to_edge);
    result[0] = source.sample(
        cube_sampler, float3(0.0, 0.0, 1.0), 1u, level(0.0));
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

fragment float4 almetal_resample_flip_y_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]])
{
    // This permutation is used for same-size top-down IOSurface ingress. Read
    // the exact opposite source row so it matches glBlitFramebuffer(GL_NEAREST)
    // without filter-coordinate rounding at either edge.
    const uint2 destination = uint2(input.position.xy);
    return source.read(uint2(
        destination.x, source.get_height() - 1u - destination.y));
}

fragment float4 almetal_present_gl_rows_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]])
{
    // World and ordered UI targets deliberately preserve OpenGL's
    // lower-left row-zero convention so existing framebuffer consumers see
    // unchanged storage. CAMetalLayer uses Metal's upper-left convention;
    // convert exactly once at that display boundary.
    const float4 color = source.sample(
        source_sampler, float2(input.texcoord.x, 1.0 - input.texcoord.y));
    // Exact world publication makes the CAMetalLayer opaque and suppresses
    // the matching GL swap. Cover that frozen GL underlay completely even
    // though intermediate scene targets preserve alpha for later consumers.
    return float4(color.rgb, 1.0);
}

struct ALMetalNormalMapGenerateConstants
{
    float2 texel_step;
    float normal_scale;
    uint darkness;
};

inline float almetal_normal_map_height(
    texture2d<float> source,
    sampler source_sampler,
    float2 texcoord,
    uint darkness)
{
    constexpr float3 luminance_weights = float3(0.2995, 0.5875, 0.1145);
    const float value = dot(
        source.sample(source_sampler, texcoord).rgb,
        luminance_weights);
    return darkness != 0u ? 1.0 - value : value;
}

fragment float4 almetal_normal_map_generate_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalNormalMapGenerateConstants& constants [[buffer(0)]])
{
    const float center = almetal_normal_map_height(
        source, source_sampler, input.texcoord, constants.darkness);
    constexpr float slope_scale = 512.0;
    const float3 right = float3(
        constants.normal_scale, 0.0,
        (almetal_normal_map_height(
             source, source_sampler,
             input.texcoord + float2(constants.texel_step.x, 0.0),
             constants.darkness) - center) * slope_scale);
    const float3 left = float3(
        -constants.normal_scale, 0.0,
        (almetal_normal_map_height(
             source, source_sampler,
             input.texcoord - float2(constants.texel_step.x, 0.0),
             constants.darkness) - center) * slope_scale);
    const float3 up = float3(
        0.0, -constants.normal_scale,
        (almetal_normal_map_height(
             source, source_sampler,
             input.texcoord - float2(0.0, constants.texel_step.y),
             constants.darkness) - center) * slope_scale);
    const float3 down = float3(
        0.0, constants.normal_scale,
        (almetal_normal_map_height(
             source, source_sampler,
             input.texcoord + float2(0.0, constants.texel_step.y),
             constants.darkness) - center) * slope_scale);
    const float3 normal = normalize(
        cross(right, down) + cross(down, left) +
        cross(left, up) + cross(up, right));
    return float4(normal * 0.5 + 0.5, center);
}

struct ALMetalUIDepthOutput
{
    float depth [[depth(any)]];
};

fragment ALMetalUIDepthOutput almetal_ui_depth_clear_fragment(
    constant float& clear_depth [[buffer(0)]])
{
    ALMetalUIDepthOutput output;
    output.depth = clear_depth;
    return output;
}

fragment ALMetalUIDepthOutput almetal_ui_depth_restore_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> retained_depth [[texture(0)]],
    sampler point_sampler [[sampler(0)]])
{
    ALMetalUIDepthOutput output;
    output.depth = retained_depth.sample(
        point_sampler, input.texcoord, level(0.0)).r;
    return output;
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
    uint reverse_z;
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

static float4 almetal_shadow_clip_from_gl(
    float4 position, uint depth_convention)
{
    position.y = -position.y;
    const bool reverse_z = (depth_convention & 1u) != 0u;
    return reverse_z ? position : almetal_clip_from_gl(position);
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
    float4x4 modelview;
    float4 normal_matrix[3];
    float4 rigged_normal_matrix[3];
    float4x4 texture_matrix;
    float4 material;
    uint4 depth_convention;
    float4 avatar_wind;
    float4 avatar_sin_wave;
    float4 avatar_gravity;
    float4 avatar_cloth_depth;
    float4 mirror_clip_plane;
};

struct ALMetalWorldSimpleVaryings
{
    float4 position [[position]];
    float mirror_clip_distance [[clip_distance]];
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

struct ALMetalWorldHighlightConstants
{
    float4x4 modelview_projection;
    float4x4 texture_matrix;
    float4 color;
    uint4 config;
};

struct ALMetalWorldHighlightVaryings
{
    float4 position [[position]];
    float2 texcoord;
};

struct ALMetalWorldSpotShadowConstants
{
    float4x4 modelview_projection;
    float4x4 texture_matrix;
    float4 pbr_base_color_transforms[8][2];
    float alpha_cutoffs[32];
    uint4 config;
    float4 avatar_wind;
    float4 avatar_sin_wave;
    float4 avatar_gravity;
    float4 avatar_cloth_depth;
    float4 avatar_draw;
};

struct ALMetalWorldSpotShadowVaryings
{
    float4 position [[position]];
    float2 texcoord;
    float vertex_alpha;
    uint texture_index [[flat]];
};

struct ALMetalWorldWaterExclusionConstants
{
    float4x4 modelview_projection;
    float4x4 modelview;
    float4x4 eye_to_agent;
    float4 cell_square;
    float4 camera_default_height;
    uint4 config;
};

struct ALMetalWorldWaterExclusionVaryings
{
    float4 position [[position]];
    float3 agent_position;
};

struct ALMetalWorldWaterHazeConstants
{
    float4x4 modelview_projection;
    float4x4 inverse_projection;
    float4 cell_square;
    float4 default_height;
    float4 rings;
    float4 water_plane;
    float4 fog_color;
    float4 fog;
    int4 patch_rect;
    uint4 config;
};

struct ALMetalWorldWaterHazeVaryings
{
    float4 position [[position]];
    float2 texcoord;
};

struct ALMetalWorldWaterSurfaceConstants
{
    float4x4 modelview;
    float4 normal_matrix[3];
    float4 environment_matrix[3];
    float4 eye_position;
    float4 big_phase;
    float4 little_phase;
    float4 sweep_phase;
    float4 water_far;
    float4 map_mean;
    float4 lattice_fraction;
    int4 lattice_base;
    float4 normal_scale_glint;
    float4 optics;
    float4 rim;
    float4 light_direction;
    float4 parameters;
    float4 wave_eye_density;
    float4 wave_reach_amplitude;
    int4 patch_rect;
    int4 patch_order;
    float4 screen;
    uint4 config;
    uint4 environment_config;
    float4 mirror_clip_plane;
};

struct ALMetalWorldWaterOcclusionConstants
{
    float4x4 modelview_projection;
    float4 cell_square;
    float4 screen_displacement;
    int4 patch_rect;
    int4 patch_order;
    uint4 config;
};

struct ALMetalWorldWaterSurfaceVaryings
{
    float4 position [[position]];
    float3 eye_position;
    float3 agent_view;
};

struct ALMetalWorldAlphaEnvironment
{
    float4 water_plane;
    float4 eye_to_sky[3];
    float4 haze_extinction;
    float4 glow_max_y;
    float4 haze_additive;
    float4 haze_additive_glow;
    float4 water_fog_color;
    float4 light_direction;
    float4 atmos_sunlit;
    float4 atmos_sunlit_linear;
    float4 atmos_amblit;
    float4 atmospheric;
    float4 water_fog;
    float4 environment_matrix[3];
    float4 environment;
    uint4 environment_config;
};

struct ALMetalWorldAtmosphericHazeConstants
{
    float4x4 inverse_projection;
    // x = water-ground fade.
    float4 parameters;
    // x = reverse Z, y = sky-behind convergence.
    uint4 config;
};

struct ALMetalWorldShinyConstants
{
    float4x4 modelview_projection;
    float4x4 modelview;
    float4 normal_matrix[3];
    float4 rigged_normal_matrix[3];
    float4x4 texture_matrix;
    uint4 config;
    float4 avatar_wind;
    float4 avatar_sin_wave;
    float4 avatar_gravity;
    float4 avatar_cloth_depth;
    float4 mirror_clip_plane;
};

struct ALMetalWorldShinyVaryings
{
    float4 position [[position]];
    float mirror_clip_distance [[clip_distance]];
    float3 eye_position;
    float3 normal;
    float4 color;
    float2 texcoord;
    uint texture_index [[flat]];
    uint texture_count [[flat]];
};

struct ALMetalWorldLegacyAlphaVaryings
{
    float4 position [[position]];
    float mirror_clip_distance [[clip_distance]];
    float3 eye_position;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float4 color;
    float2 texcoord;
    float2 texcoord1;
    float2 texcoord2;
    uint material_index [[flat]];
};

struct ALMetalWorldLegacyAlphaLight
{
    float4 position;
    float4 direction;
    float4 diffuse;
    float4 parameters;
};

struct ALMetalWorldLegacyAlphaLighting
{
    float4 sun_direction;
    float4 sun_color;
    ALMetalWorldLegacyAlphaLight local_lights[7];
    float4 parameters;
};

struct ALMetalWorldDirectionalShadowConstants
{
    float4x4 shadow_matrix[4];
    float4 shadow_clip;
    float4 resolution;
    float4 parameters;
    uint4 config;
};

struct ALMetalWorldReflectionProbeConstants
{
    float4x4 ref_box[256];
    float4 ref_sphere[256];
    float4 ref_params[256];
    int4 ref_index[256];
    int4 ref_neighbor[1024];
    int4 ref_bucket[256];
    float4 env_matrix[3];
    float4 config;
};

struct ALMetalWorldReflectionProbeVolumeConstants
{
    float4x4 inverse_projection;
    uint4 config;
};

struct ALMetalWorldHeroProbeConstants
{
    float4x4 hero_box;
    float4 hero_sphere;
    float4 clip_plane;
    uint4 config;
};

struct ALMetalWorldScreenSpaceReflectionConstants
{
    float4x4 projection;
    float4x4 inverse_projection;
    float4x4 inverse_modelview_delta;
    float4 screen;
    float4 trace;
    float4 sampling;
    uint4 config;
};

struct ALMetalWorldLocalLight
{
    float4 position_radius;
    float4 color_falloff;
};

struct ALMetalWorldLocalLightingConstants
{
    float4x4 inverse_projection;
    float4 surface;
    uint4 config;
};

struct ALMetalWorldProjectorLightingConstants
{
    float4x4 inverse_projection;
    float4x4 projector_matrix;
    float4 position_radius;
    float4 color_falloff;
    float4 projector_plane;
    float4 projector_normal;
    float4 projector_origin;
    float4 sampling;
    uint4 config;
};

struct ALMetalWorldLightMapConstants
{
    float4x4 inverse_projection;
    float4 light_direction;
    float4 ssao;
    float4 blur_taps[4];
    float4 blur;
    float4 screen;
    uint4 config;
};

struct ALMetalWorldBaseLightingConstants
{
    float4x4 inverse_projection;
    float4 light_direction;
    float4 atmos_sunlit;
    float4 atmos_sunlit_linear;
    float4 atmos_amblit;
    float4 ssao_effect[3];
    float4 lighting;
    float4 surface;
    float4 environment_matrix[3];
    float4 environment;
    float4 sky_transfer;
    uint4 sky;
    uint4 config;
    uint4 environment_config;
};

struct ALMetalWorldHDRISkyConstants
{
    float4x4 modelview_projection;
    // xyz = sky-local camera, w = 2^RenderHDRIExposure.
    float4 camera_exposure;
    float4 environment_rotation[3];
    // xy = destination dimensions.
    float4 screen;
    // x = reverse Z, y = emissive/HDR G-buffer.
    uint4 config;
};

struct ALMetalWorldWindLightSkyConstants
{
    float4x4 modelview_projection;
    float4 camera_local;
    float4 light_density;
    float4 blue_horizon;
    float4 glow;
    float4 haze_extinction;
    float4 blue_weight;
    float4 haze_weight;
    float4 sky_light;
    float4 sky_ambient;
    float4 sky_light_atten;
    float4 sky_ambient_below;
    float4 optics;
    // xy = destination dimensions, z = deferred sky_hdr_scale.
    float4 transfer;
    // x = reverse Z, y = emissive/HDR G-buffer.
    uint4 config;
};

struct ALMetalWorldStarConstants
{
    float4x4 modelview_projection;
    // xy = screen dimensions, z = custom_alpha, w = wrapped time.
    float4 screen_alpha_time;
    // xyz = moon direction in the star frame, w = moon brightness.
    float4 moon_direction_brightness;
    float4 moon_quad_center;
    float4 moon_quad_u;
    float4 moon_quad_v;
    // x = reverse Z, y = emissive/HDR G-buffer.
    uint4 config;
};

struct ALMetalWorldHeavenlyBodyConstants
{
    float4x4 modelview_projection;
    // x = sun texture blend or moon brightness, y = moon direction z.
    float4 parameters;
    // x = reverse Z, y = emissive/HDR G-buffer, z = moon.
    uint4 config;
};

struct ALMetalWorldMeteorConstants
{
    float4x4 modelview_projection;
    // xy = target dimensions, z = half-width in pixels.
    float4 screen_width;
    // x = reverse Z, y = emissive/HDR G-buffer.
    uint4 config;
};

struct ALMetalWorldAuroraConstants
{
    float4x4 modelview_projection;
    // xyz = camera in dome-local space, w = intensity.
    float4 camera_intensity;
    // x = wrapped time, yz = target dimensions.
    float4 time_screen;
    // x = reverse Z, y = emissive/HDR G-buffer.
    uint4 config;
};

struct ALMetalWorldCloudConstants
{
    float4x4 modelview_projection;
    float4 camera_local;
    float4 light_density;
    float4 blue_horizon;
    float4 glow;
    float4 haze_extinction;
    float4 blue_weight;
    float4 haze_weight;
    float4 sky_sunlight;
    float4 atmos_ambient;
    float4 sky_light_atten;
    float4 cloud_sunlight;
    float4 cloud_color_shadow;
    float4 cloud_pos_density1;
    float4 cloud_pos_density2;
    // x = cloud scale, y = variance, z = texture blend,
    // w = sun/moon glow factor.
    float4 parameters;
    // xy = target dimensions.
    float4 screen;
    // x = reverse Z, y = emissive/HDR G-buffer.
    uint4 config;
};

struct ALMetalWorldLocalLightVaryings
{
    float4 position [[position]];
    float2 texcoord;
};

struct ALMetalWorldSSRHistoryOutput
{
    float4 scene_color [[color(0)]];
    float scene_depth [[color(1)]];
};

struct ALMetalWorldGBufferOutput
{
    float4 albedo [[color(0)]];
    float4 specular_orm [[color(1)]];
    float4 normal [[color(2)]];
    float4 emissive [[color(3)]];
};

struct ALMetalWorldHDRISkyVaryings
{
    float4 position [[position]];
    float3 local_position;
};

vertex ALMetalWorldHDRISkyVaryings almetal_world_hdri_sky_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldHDRISkyConstants& constants [[buffer(1)]],
    uint vertex_id [[vertex_id]])
{
    const float3 local_position = positions[vertex_id].xyz;
    float4 clip_position = constants.modelview_projection *
                           float4(local_position, 1.0);
    clip_position.y = -clip_position.y;
    // Match skyV.glsl: every dome vertex is on the far plane and depth is
    // read-only, so only pixels left clear by opaque geometry are writable.
    clip_position.z = constants.config.x != 0u ? 0.0 : clip_position.w;

    ALMetalWorldHDRISkyVaryings output;
    output.position = clip_position;
    output.local_position = local_position;
    return output;
}

vertex ALMetalWorldHDRISkyVaryings almetal_world_windlight_sky_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldWindLightSkyConstants& constants [[buffer(1)]],
    uint vertex_id [[vertex_id]])
{
    const float3 local_position = positions[vertex_id].xyz;
    float4 clip_position = constants.modelview_projection *
                           float4(local_position, 1.0);
    clip_position.y = -clip_position.y;
    clip_position.z = constants.config.x != 0u ? 0.0 : clip_position.w;

    ALMetalWorldHDRISkyVaryings output;
    output.position = clip_position;
    output.local_position = local_position;
    return output;
}

fragment ALMetalWorldGBufferOutput almetal_world_hdri_sky_fragment(
    ALMetalWorldHDRISkyVaryings input [[stage_in]],
    constant ALMetalWorldHDRISkyConstants& constants [[buffer(0)]],
    texture2d<float> environment_map [[texture(0)]],
    sampler environment_sampler [[sampler(0)]])
{
    float3 direction = normalize(
        input.local_position - constants.camera_exposure.xyz +
        float3(0.0, 50.0, 0.0));
    const float3x3 rotation = float3x3(
        constants.environment_rotation[0].xyz,
        constants.environment_rotation[1].xyz,
        constants.environment_rotation[2].xyz);
    direction = rotation * direction;
    const float2 texcoord = float2(
        atan2(direction.z, direction.x) + M_PI_F,
        acos(direction.y)) / float2(2.0 * M_PI_F, M_PI_F);
    float3 color = environment_map.sample(
        environment_sampler, texcoord, level(0.0)).rgb *
        constants.camera_exposure.w;
    color = min(color, float3(1073741824.0));

    ALMetalWorldGBufferOutput output;
    output.specular_orm = 0.0;
    output.normal = float4(0.0, 0.0, 0.0, 1.0);
    if (constants.config.y != 0u)
    {
        const float2 gl_fragment_position = float2(
            input.position.x, constants.screen.y - input.position.y);
        const float noise = fract(
            52.9829189 * fract(dot(
                gl_fragment_position, float2(0.06711056, 0.00583715)))) -
            0.5;
        output.albedo = 0.0;
        output.emissive = float4(
            color * (1.0 + noise *
                float3(1.0 / 64.0, 1.0 / 64.0, 1.0 / 32.0)),
            1.0);
    }
    else
    {
        // The destination is an sRGB attachment. Fixed-function encoding
        // stores the same display-encoded bytes that skyF writes raw with
        // FRAMEBUFFER_SRGB disabled; the base pass decodes them once.
        output.albedo = float4(clamp(color, 0.0, 1.0), 1.0);
        output.emissive = 0.0;
    }
    return output;
}

static float3 almetal_world_windlight_sky_color(
    float3 local_position,
    constant ALMetalWorldWindLightSkyConstants& constants,
    texture2d<float> rainbow_map,
    texture2d<float> halo_map,
    sampler optics_sampler)
{
    // Match skyF.glsl's sky-frame ray and its asymmetric altitude clamp.
    float3 relative_position = local_position -
        constants.camera_local.xyz + float3(0.0, 50.0, 0.0);
    if (relative_position.y > 0.0)
    {
        relative_position *= max(constants.glow.w, 1.0) /
                             relative_position.y;
    }
    if (relative_position.y < 0.0)
    {
        relative_position *= -32000.0 / relative_position.y;
    }

    const float3 direction = normalize(relative_position);
    const float distance = length(relative_position);
    const float light_dot = dot(
        direction, constants.light_density.xyz);
    const float off_axis = 1.0 / max(
        0.000001,
        max(0.0, direction.y) + constants.light_density.y);
    float3 sunlight = constants.sky_light.rgb * exp(
        -constants.sky_light_atten.rgb * off_axis);
    const float3 combined_haze = exp(
        -constants.haze_extinction.rgb *
        (distance * constants.light_density.w));

    float haze_glow = max(1.0 - light_dot, 0.001);
    haze_glow = pow(haze_glow * constants.glow.x, constants.glow.z);
    haze_glow = constants.optics.w < 1.0
        ? 0.0 : constants.optics.w * (haze_glow + 0.25);

    float3 color =
        constants.blue_horizon.rgb * constants.blue_weight.rgb *
            (sunlight + constants.sky_ambient.rgb) +
        constants.blue_horizon.w * constants.haze_weight.rgb *
            (sunlight * haze_glow + constants.sky_ambient.rgb);
    color *= 1.0 - combined_haze;

    sunlight *= max(0.0, 1.0 - constants.sky_ambient_below.w);
    const float3 below_cloud =
        constants.blue_horizon.rgb * constants.blue_weight.rgb *
            (sunlight + constants.sky_ambient_below.rgb) +
        constants.blue_horizon.w * constants.haze_weight.rgb *
            (sunlight * haze_glow + constants.sky_ambient_below.rgb);
    const float3 lower_haze = sqrt(combined_haze);
    color += (below_cloud - color) * (1.0 - sqrt(lower_haze));

    float rainbow_coordinate = clamp(-0.575 - light_dot, 0.0, 1.0);
    const float interior_coordinate =
        max(0.0, rainbow_coordinate - 0.25) * 4.2857;
    rainbow_coordinate = clamp(
        rainbow_coordinate, 0.0, 0.25) + interior_coordinate;
    const float radius = (constants.optics.y - 5.0) / 1024.0;
    color += pow(rainbow_map.sample(
        optics_sampler,
        float2(radius + 0.5, rainbow_coordinate),
        level(0.0)).rgb, float3(1.8)) * constants.optics.x;

    const float halo_dot = clamp(light_dot, 0.1, 1.0);
    const float halo_coordinate = sqrt(clamp(
        1.0 - halo_dot * halo_dot, 0.0, 1.0));
    color += halo_map.sample(
        optics_sampler, float2(0.0, halo_coordinate),
        level(0.0)).rgb * constants.optics.z;
    return clamp(color * 2.0, 0.0, 5.0);
}

fragment ALMetalWorldGBufferOutput almetal_world_windlight_sky_fragment(
    ALMetalWorldHDRISkyVaryings input [[stage_in]],
    constant ALMetalWorldWindLightSkyConstants& constants [[buffer(0)]],
    texture2d<float> rainbow_map [[texture(0)]],
    texture2d<float> halo_map [[texture(1)]],
    sampler optics_sampler [[sampler(0)]])
{
    const float3 color = almetal_world_windlight_sky_color(
        input.local_position, constants, rainbow_map, halo_map,
        optics_sampler);

    ALMetalWorldGBufferOutput output;
    output.specular_orm = 0.0;
    // GBUFFER_FLAG_SKIP_ATMOS is zero. The all-zero normal is also the
    // transaction marker used by the native soften owner.
    output.normal = 0.0;
    if (constants.config.y != 0u)
    {
        const float2 gl_fragment_position = float2(
            input.position.x, constants.transfer.y - input.position.y);
        const float noise = fract(
            52.9829189 * fract(dot(
                gl_fragment_position,
                float2(0.06711056, 0.00583715)))) - 0.5;
        output.albedo = 0.0;
        // WindLight deliberately stores display-encoded sky color in the
        // float emissive attachment. The soften transfer decodes it.
        output.emissive = float4(
            color * (1.0 + noise *
                float3(1.0 / 64.0, 1.0 / 64.0, 1.0 / 32.0)),
            1.0);
    }
    else
    {
        // Metal fixed-function sRGB encoding restores the display-encoded
        // bytes that GL writes raw with FRAMEBUFFER_SRGB disabled.
        output.albedo = float4(
            almetal_srgb_to_linear(clamp(color, 0.0, 1.0)), 1.0);
        output.emissive = 0.0;
    }
    return output;
}

fragment float4 almetal_world_windlight_sky_behind_fragment(
    ALMetalWorldHDRISkyVaryings input [[stage_in]],
    constant ALMetalWorldWindLightSkyConstants& constants [[buffer(0)]],
    texture2d<float> rainbow_map [[texture(0)]],
    texture2d<float> halo_map [[texture(1)]],
    sampler optics_sampler [[sampler(0)]])
{
    float3 color = almetal_world_windlight_sky_color(
        input.local_position, constants, rainbow_map, halo_map,
        optics_sampler);
    if (constants.config.y != 0u)
    {
        const float2 gl_fragment_position = float2(
            input.position.x, constants.transfer.y - input.position.y);
        const float noise = fract(
            52.9829189 * fract(dot(
                gl_fragment_position,
                float2(0.06711056, 0.00583715)))) - 0.5;
        color *= 1.0 + noise *
            float3(1.0 / 64.0, 1.0 / 64.0, 1.0 / 32.0);
    }
    // This target deliberately holds the display-encoded sky value exactly
    // as the GL sky-behind target does. Haze converts it after sampling.
    return float4(color, 1.0);
}

struct ALMetalWorldStarVaryings
{
    float4 position [[position]];
    float3 world_direction;
    float2 corner;
    float4 color;
    float intensity;
    float subpixel_fade;
};

vertex ALMetalWorldStarVaryings almetal_world_star_vertex(
    const device float4* positions [[buffer(0)]],
    const device uchar4* colors [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    constant ALMetalWorldStarConstants& constants [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    float4 clip_center = constants.modelview_projection *
                         float4(positions[vertex_id].xyz, 1.0);
    const float4 packed_color = float4(colors[vertex_id]) / 255.0;
    const float intensity = packed_color.a;
    const float base_pixels = mix(1.8, 7.0, intensity);
    const float spike_pixels = smoothstep(0.90, 1.0, intensity) * 12.0;
    const float ideal_pixels = base_pixels + spike_pixels;
    const float quad_pixels = max(ideal_pixels, 2.6);
    const float2 pixel_offset = texcoords[vertex_id] *
                                (quad_pixels * 0.5);
    const float2 ndc_offset = pixel_offset *
        (2.0 / max(constants.screen_alpha_time.xy, float2(1.0)));
    clip_center.xy += ndc_offset * clip_center.w;
    // Convert the final GL-oriented clip position to Metal's upper-left
    // attachment convention after applying the pixel billboard offset.
    clip_center.y = -clip_center.y;
    clip_center.z = constants.config.x != 0u ? 0.0 : clip_center.w;

    ALMetalWorldStarVaryings output;
    output.position = clip_center;
    output.world_direction = normalize(positions[vertex_id].xyz);
    output.corner = texcoords[vertex_id];
    output.color = float4(
        pow(max(packed_color.rgb, float3(0.0)), float3(2.2)),
        intensity);
    output.intensity = intensity;
    output.subpixel_fade =
        (ideal_pixels * ideal_pixels) / (quad_pixels * quad_pixels);
    return output;
}

static float almetal_star_hash31(float3 position)
{
    position = fract(position * float3(0.1031, 0.1030, 0.0973));
    position += dot(position, position.yzx + 33.33);
    return fract((position.x + position.y) * position.z);
}

static float almetal_star_value_noise(float3 position)
{
    const float3 cell = floor(position);
    const float3 fraction = fract(position);
    const float3 interpolation = fraction * fraction *
                                 (3.0 - 2.0 * fraction);
    const float n000 = almetal_star_hash31(cell + float3(0.0, 0.0, 0.0));
    const float n100 = almetal_star_hash31(cell + float3(1.0, 0.0, 0.0));
    const float n010 = almetal_star_hash31(cell + float3(0.0, 1.0, 0.0));
    const float n110 = almetal_star_hash31(cell + float3(1.0, 1.0, 0.0));
    const float n001 = almetal_star_hash31(cell + float3(0.0, 0.0, 1.0));
    const float n101 = almetal_star_hash31(cell + float3(1.0, 0.0, 1.0));
    const float n011 = almetal_star_hash31(cell + float3(0.0, 1.0, 1.0));
    const float n111 = almetal_star_hash31(cell + float3(1.0, 1.0, 1.0));
    return mix(
        mix(mix(n000, n100, interpolation.x),
            mix(n010, n110, interpolation.x), interpolation.y),
        mix(mix(n001, n101, interpolation.x),
            mix(n011, n111, interpolation.x), interpolation.y),
        interpolation.z);
}

static float almetal_star_airmass(float cosine_zenith)
{
    const float bounded = clamp(cosine_zenith, 0.02, 1.0);
    const float elevation_degrees = asin(bounded) *
                                    (180.0 / M_PI_F);
    return 1.0 / (bounded + 0.50572 *
        pow(max(6.07995 + elevation_degrees, 1.0), -1.6364));
}

fragment ALMetalWorldGBufferOutput almetal_world_star_fragment(
    ALMetalWorldStarVaryings input [[stage_in]],
    constant ALMetalWorldStarConstants& constants [[buffer(0)]],
    texture2d<float> moon_map [[texture(0)]],
    sampler moon_sampler [[sampler(0)]])
{
    const float radius_squared = dot(input.corner, input.corner);
    const float core = exp(-radius_squared * 9.0);
    const float halo = exp(-radius_squared * 1.8) * 0.06;
    const float horizontal = exp(-abs(input.corner.y) * 32.0) *
                             (1.0 - abs(input.corner.x));
    const float vertical = exp(-abs(input.corner.x) * 32.0) *
                           (1.0 - abs(input.corner.y));
    const float spike_strength = smoothstep(0.90, 1.0, input.intensity);
    const float shape = core + halo +
        (horizontal + vertical) * spike_strength * 0.30;
    if (shape < 0.002)
    {
        discard_fragment();
    }

    const float moon_facing = dot(
        input.world_direction, constants.moon_quad_center.xyz);
    if (moon_facing > 0.0)
    {
        const float3 on_quad = input.world_direction / moon_facing -
                               constants.moon_quad_center.xyz;
        const float2 quad = float2(
            dot(on_quad, constants.moon_quad_u.xyz),
            dot(on_quad, constants.moon_quad_v.xyz));
        if (all(abs(quad) < float2(1.0)) &&
            moon_map.sample(moon_sampler, quad * 0.5 + 0.5,
                            level(0.0)).a > 2.0 / 255.0)
        {
            discard_fragment();
        }
    }

    const float airmass = almetal_star_airmass(input.world_direction.z);
    const float3 extinction = exp(
        -airmass * float3(0.05, 0.13, 0.30));
    const float3 seed = input.world_direction * 120.0;
    const float time = constants.screen_alpha_time.w;
    const float fast_noise = almetal_star_value_noise(
        seed + float3(0.0, 0.0, time * 2.3));
    const float slow_noise = almetal_star_value_noise(
        seed * 0.37 + float3(0.0, 0.0, time * 0.7));
    const float twinkle_base = mix(fast_noise, slow_noise, 0.35);
    const float twinkle_amount = clamp(
        0.18 + 0.14 * (airmass - 1.0), 0.0, 0.65) * input.intensity;
    const float twinkle = 1.0 +
        (twinkle_base - 0.5) * 2.0 * twinkle_amount;
    const float3 chroma_noise = float3(
        almetal_star_value_noise(
            seed * 0.55 + float3(0.0, 0.0, time * 1.7)),
        almetal_star_value_noise(
            seed * 0.55 + float3(0.0, 0.0, time * 1.3 + 5.7)),
        almetal_star_value_noise(
            seed * 0.55 + float3(0.0, 0.0, time * 2.1 + 13.1)));
    const float chroma_amount = 0.18 * twinkle_amount;
    const float3 color_jitter = 1.0 +
        (chroma_noise - 0.5) * 2.0 * chroma_amount;

    const float moon_up = max(0.0, constants.moon_direction_brightness.z);
    const float moon_power = constants.moon_direction_brightness.w * moon_up;
    const float sky_wash = moon_power *
        (1.0 - input.intensity) * 0.55;
    const float moon_cosine = max(0.0, dot(
        input.world_direction,
        constants.moon_direction_brightness.xyz));
    const float direct_glare = pow(moon_cosine, 10.0) *
                               moon_power * 0.85;
    const float moon_fade = clamp(sky_wash + direct_glare, 0.0, 0.95);

    float3 star_color = input.color.rgb * color_jitter *
                        extinction * twinkle;
    star_color *= input.intensity;
    star_color *= constants.screen_alpha_time.z;
    star_color *= 1.0 - moon_fade;
    star_color *= input.subpixel_fade;
    const float3 premultiplied = star_color * shape;
    const float bloom_boost = 1.0 +
        input.intensity * input.intensity * 1.2;

    ALMetalWorldGBufferOutput output;
    output.specular_orm = 0.0;
    output.normal = float4(0.0, 1.0, 0.0, 0.0);
    if (constants.config.y != 0u)
    {
        output.albedo = 0.0;
        output.emissive = float4(
            premultiplied * bloom_boost, shape);
    }
    else
    {
        output.albedo = float4(
            premultiplied * (1.0 + input.intensity), shape);
        output.emissive = 0.0;
    }
    return output;
}

struct ALMetalWorldHeavenlyBodyVaryings
{
    float4 position [[position]];
    float2 texcoord;
};

vertex ALMetalWorldHeavenlyBodyVaryings
almetal_world_heavenly_body_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalWorldHeavenlyBodyConstants& constants [[buffer(2)]],
    uint vertex_id [[vertex_id]])
{
    float3 local_position = positions[vertex_id].xyz;
    // sunDiscV alone removes the historic sky-centre offset. moonV consumes
    // the stored face position directly.
    if (constants.config.z == 0u)
    {
        local_position -= float3(0.0, 0.0, 50.0);
    }
    float4 clip_position = constants.modelview_projection *
                           float4(local_position, 1.0);
    clip_position.y = -clip_position.y;
    clip_position.z = constants.config.x != 0u
        ? 0.0 : clip_position.w;

    ALMetalWorldHeavenlyBodyVaryings output;
    output.position = clip_position;
    output.texcoord = texcoords[vertex_id];
    return output;
}

fragment ALMetalWorldGBufferOutput
almetal_world_heavenly_body_fragment(
    ALMetalWorldHeavenlyBodyVaryings input [[stage_in]],
    constant ALMetalWorldHeavenlyBodyConstants& constants [[buffer(0)]],
    texture2d<float> texture_a [[texture(0)]],
    texture2d<float> texture_b [[texture(1)]],
    sampler body_sampler [[sampler(0)]])
{
    float4 color;
    ALMetalWorldGBufferOutput output;
    output.specular_orm = 0.0;
    output.normal = 0.0;
    if (constants.config.z != 0u)
    {
        color = texture_a.sample(
            body_sampler, input.texcoord, level(0.0));
        if (color.a <= 2.0 / 255.0)
        {
            discard_fragment();
        }
        color.rgb *= constants.parameters.x;
        float fade = 1.0;
        if (constants.parameters.y > 0.0)
        {
            fade = clamp(constants.parameters.y *
                         constants.parameters.y * 4.0, 0.0, 1.0);
        }
        color.a *= fade;
    }
    else
    {
        const float4 sun_a = texture_a.sample(
            body_sampler, input.texcoord, level(0.0));
        const float4 sun_b = texture_b.sample(
            body_sampler, input.texcoord, level(0.0));
        color = mix(sun_a, sun_b, constants.parameters.x);
        output.normal = float4(0.0, 1.0, 0.0, 0.0);
    }

    if (constants.config.y != 0u)
    {
        output.albedo = 0.0;
        output.emissive = color;
    }
    else
    {
        output.albedo = color;
        output.emissive = 0.0;
    }
    return output;
}

struct ALMetalWorldMeteorVaryings
{
    float4 position [[position]];
    float4 color;
    float2 coordinate;
};

vertex ALMetalWorldMeteorVaryings almetal_world_meteor_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device uchar4* colors [[buffer(2)]],
    const device float2* texcoords [[buffer(3)]],
    constant ALMetalWorldMeteorConstants& constants [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const float4 clip_mid = constants.modelview_projection *
                            float4(positions[vertex_id].xyz, 1.0);
    const float4 clip_head = constants.modelview_projection *
        float4(positions[vertex_id].xyz + normals[vertex_id].xyz, 1.0);

    ALMetalWorldMeteorVaryings output;
    if (clip_mid.w < 0.01 || clip_head.w < 0.01)
    {
        output.position = float4(2.0, -2.0, 2.0, 1.0);
        output.color = 0.0;
        output.coordinate = 0.0;
        return output;
    }

    const float2 ndc_mid = clip_mid.xy / clip_mid.w;
    const float2 ndc_head = clip_head.xy / clip_head.w;
    const float2 streak_ndc = ndc_head - ndc_mid;
    float2 perpendicular = float2(-streak_ndc.y, streak_ndc.x);
    perpendicular /= max(length(perpendicular), 1e-5);
    perpendicular *= constants.screen_width.z *
        (2.0 / max(constants.screen_width.xy, float2(1.0)));

    float4 clip_position = clip_mid;
    clip_position.xy += streak_ndc * texcoords[vertex_id].x *
                        clip_position.w;
    clip_position.xy += perpendicular * texcoords[vertex_id].y *
                        clip_position.w;
    clip_position.y = -clip_position.y;
    clip_position.z = constants.config.x != 0u
        ? 0.0 : clip_position.w;

    const float4 packed_color = float4(colors[vertex_id]) / 255.0;
    output.position = clip_position;
    output.color = float4(
        pow(max(packed_color.rgb, float3(0.0)), float3(2.2)),
        packed_color.a * 4.0);
    output.coordinate = texcoords[vertex_id];
    return output;
}

fragment ALMetalWorldGBufferOutput almetal_world_meteor_fragment(
    ALMetalWorldMeteorVaryings input [[stage_in]],
    constant ALMetalWorldMeteorConstants& constants [[buffer(0)]])
{
    const float along_t = clamp(
        (input.coordinate.x + 1.0) * 0.5, 0.0, 1.0);
    const float head_flash = pow(along_t, 6.0);
    const float along = along_t + head_flash * 1.2;
    const float width_scale = max(input.color.a, 1e-3);
    const float normalized_y = input.coordinate.y / width_scale;
    const float across = exp(-normalized_y * normalized_y * 6.0);
    const float shape = along * across;
    if (shape < 0.003)
    {
        discard_fragment();
    }
    const float3 color = input.color.rgb * shape;

    ALMetalWorldGBufferOutput output;
    output.specular_orm = 0.0;
    output.normal = float4(0.0, 1.0, 0.0, 0.0);
    if (constants.config.y != 0u)
    {
        output.albedo = 0.0;
        output.emissive = float4(color * 4.0, shape);
    }
    else
    {
        output.albedo = float4(color * 2.0, shape);
        output.emissive = 0.0;
    }
    return output;
}

struct ALMetalWorldAuroraVaryings
{
    float4 position [[position]];
    float3 local_position;
};

vertex ALMetalWorldAuroraVaryings almetal_world_aurora_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldAuroraConstants& constants [[buffer(1)]],
    uint vertex_id [[vertex_id]])
{
    const float3 local_position = positions[vertex_id].xyz;
    float4 clip_position = constants.modelview_projection *
                           float4(local_position, 1.0);
    clip_position.y = -clip_position.y;
    clip_position.z = constants.config.x != 0u
        ? 0.0 : clip_position.w;
    ALMetalWorldAuroraVaryings output;
    output.position = clip_position;
    output.local_position = local_position;
    return output;
}

fragment ALMetalWorldGBufferOutput almetal_world_aurora_fragment(
    ALMetalWorldAuroraVaryings input [[stage_in]],
    constant ALMetalWorldAuroraConstants& constants [[buffer(0)]])
{
    const float3 direction = normalize(
        input.local_position - constants.camera_intensity.xyz);
    const float elevation = direction.y;
    const float2 horizontal = direction.xz;
    const float horizontal_length = length(horizontal);
    if (horizontal_length < 1e-3 || elevation < -0.05 || elevation > 0.90)
    {
        discard_fragment();
    }
    const float2 azimuth = horizontal / horizontal_length;
    const float time = constants.time_screen.x;

    const float coverage_big = almetal_star_value_noise(
        float3(azimuth * 1.0, time * 0.03));
    const float coverage_medium = almetal_star_value_noise(
        float3(azimuth * 2.8, time * 0.07));
    const float coverage = smoothstep(
        0.22, 0.90, coverage_big * 0.62 + coverage_medium * 0.38);

    const float fold =
        0.06 * sin(azimuth.x * 2.0 + time * 0.18) +
        0.04 * sin(azimuth.y * 3.1 + time * 0.27 + 1.4) +
        0.03 * sin(azimuth.x * 4.3 - time * 0.33) +
        0.03 * almetal_star_value_noise(
            float3(azimuth * 2.5, time * 0.11));
    const float base = 0.10 + fold;
    const float distance_above = elevation - base;
    if (distance_above > 0.55)
    {
        discard_fragment();
    }

    const float2 drift = float2(time * 0.045, time * 0.028);
    const float3 noise_position = float3(
        azimuth * 16.0 + drift, elevation * 2.0);
    float noise = 0.50 * almetal_star_value_noise(
        noise_position + float3(0.0, 0.0, time * 0.09));
    noise += 0.28 * almetal_star_value_noise(
        noise_position * 2.1 + float3(drift, time * 0.14));
    noise += 0.14 * almetal_star_value_noise(
        noise_position * 4.5 + float3(drift * 1.4, time * 0.22));
    noise += 0.08 * almetal_star_value_noise(
        noise_position * 9.5 + float3(drift * 1.9, time * 0.33));
    const float curtain_mask = pow(
        smoothstep(0.05, 0.92, noise), 1.6);

    const float horizon_fade = smoothstep(-0.03, 0.28, elevation);
    const float zenith_fade = 1.0 - smoothstep(0.38, 0.88, elevation);
    const float elevation_envelope = horizon_fade * zenith_fade;
    const float body_top = 0.30;
    float body_envelope = exp(min(distance_above, 0.0) * 3.5) *
        (1.0 - smoothstep(
            body_top * 0.15, body_top, distance_above));
    body_envelope *= elevation_envelope;

    const float halo_low = 0.18;
    const float halo_high = 0.45;
    float halo_envelope = smoothstep(
        halo_low * 0.40, halo_low, distance_above) *
        (1.0 - smoothstep(
            halo_high * 0.55, halo_high * 1.15, distance_above));
    halo_envelope *= elevation_envelope;
    const float halo_mask = 0.5 + 0.5 * sqrt(curtain_mask);

    const float base_coordinate = (distance_above - 0.02) * 20.0;
    float base_line = exp(-base_coordinate * base_coordinate);
    base_line *= smoothstep(-0.22, -0.02, distance_above);
    base_line *= 0.45 + 0.55 * almetal_star_value_noise(
        float3(azimuth * 6.0, time * 0.10));
    base_line *= elevation_envelope;

    const float breath_phase = almetal_star_value_noise(
        float3(azimuth * 1.2, 0.0)) * 6.28318;
    const float breath = 0.80 + 0.20 * sin(time * 0.18 + breath_phase);
    const float pulse = 0.88 + 0.12 * sin(time * 0.65);
    const float shimmer = 0.88 + 0.12 * almetal_star_value_noise(
        float3(azimuth * 22.0, time * 1.4));

    const float3 green = float3(0.30, 1.50, 0.60);
    const float3 magenta = float3(1.15, 0.50, 0.95);
    const float3 purple = float3(0.60, 0.35, 1.10);
    const float3 base_color = float3(0.50, 1.80, 0.80);
    const float halo_fraction = clamp(
        (distance_above - halo_low) / (halo_high - halo_low),
        0.0, 1.0);
    const float3 halo_color = mix(
        magenta, purple, smoothstep(0.35, 1.00, halo_fraction));
    const float3 body_rgb = green * body_envelope * curtain_mask;
    const float3 halo_rgb = halo_color * halo_envelope *
                            halo_mask * 0.60;
    const float3 base_rgb = base_color * base_line;
    float strength = body_envelope * curtain_mask +
        halo_envelope * halo_mask * 0.40 + base_line;
    strength *= coverage * breath * pulse * shimmer;
    if (strength <= 0.003)
    {
        discard_fragment();
    }

    float3 aurora = (body_rgb + halo_rgb + base_rgb) *
        coverage * breath * pulse * shimmer *
        constants.camera_intensity.w;
    const float chroma = almetal_star_value_noise(
        float3(azimuth * 11.0, 0.0));
    aurora *= mix(float3(0.95, 1.05, 0.98),
                  float3(1.05, 0.98, 1.02), chroma);
    const float alpha = clamp(strength, 0.0, 1.0);

    ALMetalWorldGBufferOutput output;
    output.specular_orm = 0.0;
    output.normal = float4(0.0, 1.0, 0.0, 0.0);
    if (constants.config.y != 0u)
    {
        const float2 gl_fragment_position = float2(
            input.position.x, constants.time_screen.z - input.position.y);
        const float noise_value = fract(
            52.9829189 * fract(dot(
                gl_fragment_position,
                float2(0.06711056, 0.00583715)))) - 0.5;
        output.albedo = 0.0;
        output.emissive = float4(
            aurora * 1.5 * (1.0 + noise_value *
                float3(1.0 / 64.0, 1.0 / 64.0, 1.0 / 32.0)),
            alpha);
    }
    else
    {
        output.albedo = float4(aurora, alpha);
        output.emissive = 0.0;
    }
    return output;
}

struct ALMetalWorldCloudVaryings
{
    float4 position [[position]];
    float3 local_position;
    float2 texcoord0;
    float2 texcoord1;
    float2 texcoord2;
    float2 texcoord3;
};

vertex ALMetalWorldCloudVaryings almetal_world_cloud_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalWorldCloudConstants& constants [[buffer(2)]],
    uint vertex_id [[vertex_id]])
{
    const float3 local_position = positions[vertex_id].xyz;
    float4 clip_position = constants.modelview_projection *
                           float4(local_position, 1.0);
    clip_position.y = -clip_position.y;
    clip_position.z = constants.config.x != 0u
        ? 0.0 : clip_position.w;

    float2 texcoord0 = float2(
        -texcoords[vertex_id].x, texcoords[vertex_id].y);
    texcoord0 = (texcoord0 - 0.5) / constants.parameters.x + 0.5;
    float2 texcoord1 = texcoord0;
    texcoord1.x += constants.light_density.z * 0.0125;
    texcoord1.y -= constants.light_density.x * 0.0125;

    ALMetalWorldCloudVaryings output;
    output.position = clip_position;
    output.local_position = local_position;
    output.texcoord0 = texcoord0;
    output.texcoord1 = texcoord1;
    output.texcoord2 = texcoord0 * 16.0;
    output.texcoord3 = texcoord1 * 16.0;
    return output;
}

static float almetal_world_cloud_noise(
    texture2d<float> noise_map,
    texture2d<float> next_noise_map,
    sampler noise_sampler,
    float2 texcoord,
    float blend_factor)
{
    float sample_value = noise_map.sample(noise_sampler, texcoord).r;
    if (blend_factor > 0.0)
    {
        sample_value = mix(
            sample_value,
            next_noise_map.sample(noise_sampler, texcoord).r,
            blend_factor);
    }
    return sample_value;
}

fragment ALMetalWorldGBufferOutput almetal_world_cloud_fragment(
    ALMetalWorldCloudVaryings input [[stage_in]],
    constant ALMetalWorldCloudConstants& constants [[buffer(0)]],
    texture2d<float> noise_map [[texture(0)]],
    texture2d<float> next_noise_map [[texture(1)]],
    sampler noise_sampler [[sampler(0)]])
{
    if (constants.parameters.x < 0.001)
    {
        discard_fragment();
    }

    float2 uv1 = input.texcoord0;
    float2 uv2 = input.texcoord1;
    float2 uv3 = input.texcoord2;
    float2 uv4 = input.texcoord3;
    float3 relative_position = input.local_position -
        constants.camera_local.xyz + float3(0.0, 50.0, 0.0);
    float altitude_blend = smoothstep(
        0.0, 0.035,
        relative_position.y / length(relative_position));
    if (relative_position.y > 0.0)
    {
        relative_position *= max(constants.glow.w, 1.0) /
                             relative_position.y;
    }
    if (relative_position.y < 0.0)
    {
        altitude_blend = 0.0;
        relative_position *= -32000.0 / relative_position.y;
    }

    const float3 direction = normalize(relative_position);
    const float distance = length(relative_position);
    const float off_axis = 1.0 / max(
        0.000001,
        max(0.0, direction.y) + constants.light_density.y);
    float3 sunlight = constants.sky_sunlight.rgb * exp(
        -constants.sky_light_atten.rgb * off_axis);
    float3 combined_haze = exp(
        -constants.haze_extinction.rgb *
        (distance * constants.light_density.w));

    float haze_glow = max(
        1.0 - dot(direction, constants.light_density.xyz), 0.001);
    haze_glow = pow(haze_glow * constants.glow.x, constants.glow.z);
    haze_glow *= constants.parameters.w;
    haze_glow = constants.parameters.w < 1.0
        ? 0.0 : haze_glow + 0.25;

    sunlight *= 1.0 - constants.cloud_color_shadow.w;
    const float3 below_cloud =
        constants.blue_horizon.rgb * constants.blue_weight.rgb *
            (sunlight + constants.atmos_ambient.rgb) +
        constants.blue_horizon.w * constants.haze_weight.rgb *
            (sunlight * haze_glow + constants.atmos_ambient.rgb);

    float3 cloud_color_sun =
        constants.cloud_sunlight.rgb * haze_glow *
        constants.cloud_color_shadow.rgb;
    float3 cloud_color_ambient =
        constants.atmos_ambient.rgb * constants.cloud_color_shadow.rgb;
    combined_haze = sqrt(combined_haze);
    cloud_color_sun *= combined_haze;
    cloud_color_ambient *= combined_haze;
    cloud_color_ambient += below_cloud * (1.0 - combined_haze);
    float cloud_density = 2.0 *
        (constants.cloud_color_shadow.w - 0.25);

    const float variance_scale = constants.parameters.y *
        (1.0 - constants.parameters.x * 0.25);
    const float blend_factor = constants.parameters.z;
    const float2 disturbance = float2(
        almetal_world_cloud_noise(
            noise_map, next_noise_map, noise_sampler,
            uv1 / 8.0, blend_factor),
        almetal_world_cloud_noise(
            noise_map, next_noise_map, noise_sampler,
            (uv3 + uv1) / 16.0, blend_factor)) * variance_scale;
    const float2 disturbance2 = float2(
        almetal_world_cloud_noise(
            noise_map, next_noise_map, noise_sampler,
            (uv1 + uv3) / 4.0, blend_factor),
        almetal_world_cloud_noise(
            noise_map, next_noise_map, noise_sampler,
            (uv4 + uv2) / 8.0, blend_factor)) * variance_scale;

    uv1 += constants.cloud_pos_density1.xy + disturbance * 0.2;
    uv2 += constants.cloud_pos_density1.xy;
    uv3 += constants.cloud_pos_density2.xy;
    uv4 += constants.cloud_pos_density2.xy;
    const float density_variance = min(
        1.0,
        (disturbance.x * 2.0 + disturbance.y * 2.0 +
         disturbance2.x + disturbance2.y) * 4.0);
    cloud_density *= 1.0 - density_variance * density_variance;

    float alpha1 =
        almetal_world_cloud_noise(
            noise_map, next_noise_map, noise_sampler,
            uv1, blend_factor) - 0.5 +
        (almetal_world_cloud_noise(
            noise_map, next_noise_map, noise_sampler,
            uv3, blend_factor) - 0.5) *
            constants.cloud_pos_density2.z;
    alpha1 = min(
        max(alpha1 + cloud_density, 0.0) * 10.0 *
            constants.cloud_pos_density1.z,
        1.0);
    alpha1 = 1.0 - alpha1 * alpha1;
    alpha1 = 1.0 - alpha1 * alpha1;
    alpha1 = clamp(alpha1 * altitude_blend, 0.0, 1.0);

    float alpha2 = almetal_world_cloud_noise(
        noise_map, next_noise_map, noise_sampler,
        uv2, blend_factor) - 0.5;
    alpha2 = min(
        max(alpha2 + cloud_density, 0.0) * 2.5 *
            constants.cloud_pos_density1.z,
        1.0);
    alpha2 = 1.0 - alpha2;
    alpha2 = 1.0 - alpha2 * alpha2;

    float3 color = clamp(
        cloud_color_sun * (1.0 - alpha2) + cloud_color_ambient,
        float3(0.0), float3(1.0)) * 2.0;

    ALMetalWorldGBufferOutput output;
    output.specular_orm = 0.0;
    output.normal = 0.0;
    if (constants.config.y != 0u)
    {
        const float2 gl_fragment_position = float2(
            input.position.x, constants.screen.y - input.position.y);
        const float noise_value = fract(
            52.9829189 * fract(dot(
                gl_fragment_position,
                float2(0.06711056, 0.00583715)))) - 0.5;
        output.albedo = 0.0;
        output.emissive = float4(
            color * (1.0 + noise_value *
                float3(1.0 / 64.0, 1.0 / 64.0, 1.0 / 32.0)),
            alpha1);
    }
    else
    {
        output.albedo = float4(color, alpha1);
        output.emissive = 0.0;
    }
    return output;
}

struct ALMetalWorldImpostorConstants
{
    float4x4 modelview_projection;
    float4x4 modelview;
    float4 center;
    float4 left;
    float4 up;
    float4 normal_rotation[3];
    uint4 depth_convention;
    float4 mirror_clip_plane;
};

struct ALMetalWorldImpostorVaryings
{
    float4 position [[position]];
    float mirror_clip_distance [[clip_distance]];
    float2 texcoord;
};

struct ALMetalWorldCoverageVaryings
{
    float4 position [[position]];
};

vertex ALMetalWorldCoverageVaryings almetal_world_coverage_vertex(
    constant uint& reverse_z [[buffer(0)]],
    uint vertex_id [[vertex_id]])
{
    const float2 texcoord = float2((vertex_id << 1) & 2, vertex_id & 2);
    ALMetalWorldCoverageVaryings output;
    output.position = float4(texcoord.x * 2.0 - 1.0,
                             1.0 - texcoord.y * 2.0,
                             reverse_z != 0 ? 0.0 : 1.0, 1.0);
    return output;
}

fragment float4 almetal_world_coverage_fragment(
    ALMetalWorldCoverageVaryings input [[stage_in]])
{
    return float4(0.0, 0.0, 0.0, 1.0);
}

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
    const float4 eye_position = constants.modelview *
                                float4(world_position, 1.0);
    float4 clip_position = constants.modelview_projection *
                           float4(world_position, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldImpostorVaryings output;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.depth_convention.y != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
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
    const float4 object_position =
        float4(positions[vertex_id].xyz, 1.0);
    const float4 eye_position = constants.modelview * object_position;
    float4 clip_position = constants.modelview_projection * object_position;
    // Match the GL render target's lower-left row-zero storage while the
    // migration path compares the two G-buffers byte-for-byte.
    clip_position.y = -clip_position.y;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.depth_convention.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;

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

vertex ALMetalWorldHighlightVaryings almetal_world_highlight_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(2)]],
    constant ALMetalWorldHighlightConstants& constants [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    float4 clip_position = constants.modelview_projection *
                           float4(positions[vertex_id].xyz, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldHighlightVaryings output;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    return output;
}

vertex ALMetalWorldHighlightVaryings almetal_world_highlight_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(2)]],
    constant ALMetalWorldHighlightConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], float4(0.0), encoded_weights[vertex_id],
        skin_palette, joint_count);
    const float4 rebased_position =
        skinned.position + float4(skin_palette[0].xyz, 1.0);
    float4 clip_position =
        constants.modelview_projection * rebased_position;
    clip_position.y = -clip_position.y;

    ALMetalWorldHighlightVaryings output;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    return output;
}

fragment float4 almetal_world_highlight_fragment(
    ALMetalWorldHighlightVaryings input [[stage_in]],
    texture2d<float> diffuse_texture [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant ALMetalWorldHighlightConstants& constants [[buffer(0)]])
{
    return max(constants.color *
               diffuse_texture.sample(diffuse_sampler, input.texcoord),
               float4(0.0));
}

struct ALMetalWorldDebugBlip
{
    float4 position;
    float4 color;
};

struct ALMetalWorldDebugBlipConstants
{
    float4x4 modelview_projection;
    uint4 config;
};

struct ALMetalWorldDebugBlipVaryings
{
    float4 position [[position]];
    float point_size [[point_size]];
    float4 color;
};

vertex ALMetalWorldDebugBlipVaryings almetal_world_debug_blip_vertex(
    const device ALMetalWorldDebugBlip* blips [[buffer(0)]],
    constant ALMetalWorldDebugBlipConstants& constants [[buffer(1)]],
    uint vertex_id [[vertex_id]])
{
    float4 clip_position = constants.modelview_projection *
                           float4(blips[vertex_id].position.xyz, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldDebugBlipVaryings output;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.point_size = 8.0;
    output.color = blips[vertex_id].color;
    return output;
}

fragment float4 almetal_world_debug_blip_fragment(
    ALMetalWorldDebugBlipVaryings input [[stage_in]])
{
    return input.color;
}

struct ALMetalWorldFarPlaneOverlayColumn
{
    float4 center;
    float4 half_extent;
    float4 color;
};

struct ALMetalWorldFarPlaneOverlayConstants
{
    float4x4 modelview_projection;
    uint4 config;
};

struct ALMetalWorldFarPlaneOverlayVaryings
{
    float4 position [[position]];
    float4 color;
};

vertex ALMetalWorldFarPlaneOverlayVaryings
almetal_world_far_plane_overlay_vertex(
    const device ALMetalWorldFarPlaneOverlayColumn* columns [[buffer(0)]],
    constant ALMetalWorldFarPlaneOverlayConstants& constants [[buffer(1)]],
    uint vertex_id [[vertex_id]], uint instance_id [[instance_id]])
{
    constexpr uchar indices[36] = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4,
        1, 2, 6, 1, 6, 5,
        2, 3, 7, 2, 7, 6,
        3, 0, 4, 3, 4, 7,
    };
    constexpr float3 signs[8] = {
        {-1.0, -1.0, -1.0}, { 1.0, -1.0, -1.0},
        { 1.0,  1.0, -1.0}, {-1.0,  1.0, -1.0},
        {-1.0, -1.0,  1.0}, { 1.0, -1.0,  1.0},
        { 1.0,  1.0,  1.0}, {-1.0,  1.0,  1.0},
    };
    const ALMetalWorldFarPlaneOverlayColumn column = columns[instance_id];
    const float3 position = column.center.xyz +
                            column.half_extent.xyz * signs[indices[vertex_id]];
    float4 clip_position = constants.modelview_projection *
                           float4(position, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldFarPlaneOverlayVaryings output;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.color = column.color;
    return output;
}

fragment float4 almetal_world_far_plane_overlay_fragment(
    ALMetalWorldFarPlaneOverlayVaryings input [[stage_in]])
{
    return input.color;
}

static ALMetalWorldSpotShadowVaryings almetal_world_spot_shadow_output(
    float4 clip_position, constant ALMetalWorldSpotShadowConstants& constants)
{
    ALMetalWorldSpotShadowVaryings output;
    output.position = almetal_shadow_clip_from_gl(
        clip_position, constants.config.x);
    output.texcoord = 0.0;
    output.vertex_alpha = 1.0;
    output.texture_index = 0u;
    return output;
}

static float2 almetal_world_spot_shadow_pbr_texcoord(
    float2 texcoord, constant float4* transform)
{
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

static ALMetalWorldSpotShadowVaryings
almetal_world_spot_shadow_coverage_output(
    float4 clip_position, float4 packed_position, float2 texcoord,
    uchar4 color, constant ALMetalWorldSpotShadowConstants& constants)
{
    ALMetalWorldSpotShadowVaryings output =
        almetal_world_spot_shadow_output(clip_position, constants);
    const uint texture_count = max(constants.config.z, 1u);
    const uint texture_index = constants.config.z > 1u
        ? uint(max(as_type<int>(packed_position.w), 0)) : 0u;
    output.texture_index = min(texture_index, texture_count - 1u);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoord, 0.0, 1.0)).xy;
    if (constants.config.y == 5u || constants.config.y == 6u)
    {
        output.texcoord = almetal_world_spot_shadow_pbr_texcoord(
            output.texcoord,
            constants.pbr_base_color_transforms[
                min(output.texture_index, 7u)]);
    }
    output.vertex_alpha = float(color.a) / 255.0;
    return output;
}

vertex ALMetalWorldSpotShadowVaryings almetal_world_spot_shadow_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldSpotShadowConstants& constants [[buffer(1)]],
    uint vertex_id [[vertex_id]])
{
    return almetal_world_spot_shadow_output(
        constants.modelview_projection *
            float4(positions[vertex_id].xyz, 1.0),
        constants);
}

vertex ALMetalWorldSpotShadowVaryings
almetal_world_spot_shadow_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldSpotShadowConstants& constants [[buffer(1)]],
    const device float4* skin_palette [[buffer(2)]],
    const device float4* encoded_weights [[buffer(3)]],
    constant uint& joint_count [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], float4(0.0), encoded_weights[vertex_id],
        skin_palette, joint_count);
    return almetal_world_spot_shadow_output(
        constants.modelview_projection * skinned.position +
            constants.modelview_projection *
                float4(skin_palette[0].xyz, 1.0),
        constants);
}

vertex ALMetalWorldSpotShadowVaryings
almetal_world_spot_shadow_coverage_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSpotShadowConstants& constants [[buffer(1)]],
    uint vertex_id [[vertex_id]])
{
    const float4 position = positions[vertex_id];
    return almetal_world_spot_shadow_coverage_output(
        constants.modelview_projection *
            float4(position.xyz, 1.0),
        position, texcoords[vertex_id], colors[vertex_id], constants);
}

vertex ALMetalWorldSpotShadowVaryings
almetal_world_spot_shadow_coverage_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldSpotShadowConstants& constants [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    const device float4* skin_palette [[buffer(4)]],
    const device float4* encoded_weights [[buffer(5)]],
    constant uint& joint_count [[buffer(6)]],
    uint vertex_id [[vertex_id]])
{
    const float4 position = positions[vertex_id];
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        position, float4(0.0), encoded_weights[vertex_id],
        skin_palette, joint_count);
    return almetal_world_spot_shadow_coverage_output(
        constants.modelview_projection * skinned.position +
            constants.modelview_projection *
                float4(skin_palette[0].xyz, 1.0),
        position, texcoords[vertex_id], colors[vertex_id], constants);
}

static ALMetalWorldSpotShadowVaryings
almetal_world_avatar_spot_shadow_output(
    float4 clip_position, float2 texcoord,
    constant ALMetalWorldSpotShadowConstants& constants)
{
    ALMetalWorldSpotShadowVaryings output =
        almetal_world_spot_shadow_output(clip_position, constants);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoord, 0.0, 1.0)).xy;
    output.vertex_alpha = constants.avatar_draw.x;
    return output;
}

static void almetal_world_avatar_spot_shadow_rows(
    float encoded_weight, const device float4* skin_palette,
    thread float4& row0, thread float4& row1, thread float4& row2)
{
    const uint i = uint(clamp(floor(encoded_weight), 0.0, 14.0));
    const uint j = min(i + 1u, 14u);
    const float blend = fract(encoded_weight);
    row0 = mix(skin_palette[i], skin_palette[j], blend);
    row1 = mix(skin_palette[15u + i], skin_palette[15u + j], blend);
    row2 = mix(skin_palette[30u + i], skin_palette[30u + j], blend);
}

vertex ALMetalWorldSpotShadowVaryings
almetal_world_avatar_spot_shadow_rigid_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldSpotShadowConstants& constants [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    uint vertex_id [[vertex_id]])
{
    return almetal_world_avatar_spot_shadow_output(
        constants.modelview_projection *
            float4(positions[vertex_id].xyz, 1.0),
        texcoords[vertex_id], constants);
}

vertex ALMetalWorldSpotShadowVaryings
almetal_world_avatar_spot_shadow_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldSpotShadowConstants& constants [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device float4* skin_palette [[buffer(4)]],
    const device float* encoded_weights [[buffer(5)]],
    uint vertex_id [[vertex_id]])
{
    float4 row0;
    float4 row1;
    float4 row2;
    almetal_world_avatar_spot_shadow_rows(
        encoded_weights[vertex_id], skin_palette, row0, row1, row2);
    const float4 position = float4(positions[vertex_id].xyz, 1.0);
    const float4 skinned = float4(
        dot(row0, position), dot(row1, position),
        dot(row2, position), 1.0);
    return almetal_world_avatar_spot_shadow_output(
        constants.modelview_projection * skinned,
        texcoords[vertex_id], constants);
}

vertex ALMetalWorldSpotShadowVaryings
almetal_world_avatar_spot_shadow_cloth_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldSpotShadowConstants& constants [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device float4* skin_palette [[buffer(4)]],
    const device float* encoded_weights [[buffer(5)]],
    const device float4* normals [[buffer(6)]],
    const device float4* clothing [[buffer(7)]],
    uint vertex_id [[vertex_id]])
{
    float4 row0;
    float4 row1;
    float4 row2;
    almetal_world_avatar_spot_shadow_rows(
        encoded_weights[vertex_id], skin_palette, row0, row1, row2);

    float3 normal = normalize(float3(
        dot(cross(row1.xyz, row2.xyz), normals[vertex_id].xyz),
        dot(cross(row2.xyz, row0.xyz), normals[vertex_id].xyz),
        dot(cross(row0.xyz, row1.xyz), normals[vertex_id].xyz)));
    const float4 position = float4(positions[vertex_id].xyz, 1.0);
    const float wind_dot = dot(normal, constants.avatar_wind.xyz);
    float4 wind_effect = float4(wind_dot);
    const float4 eye_position = float4(
        dot(row0, position), dot(row1, position),
        dot(row2, position), 1.0);
    const float main_depth = dot(
        constants.avatar_cloth_depth, eye_position);
    wind_effect.xyz = main_depth * float3(0.015) + wind_effect.xyz;
    wind_effect.w = (wind_effect.w * 2.0 + 1.0) *
                    constants.avatar_wind.w;
    wind_effect.xyz = wind_effect.xyz *
                      constants.avatar_sin_wave.xyz +
                      float3(constants.avatar_sin_wave.w);

    constexpr float inverse_two_pi = 0.159154943;
    constexpr float two_pi = 6.28318530;
    constexpr float half_pi = 1.5707963;
    float3 periods = wind_effect.xyz * inverse_two_pi;
    wind_effect.x = (periods.x - floor(periods.x)) * two_pi;
    periods.z -= half_pi;
    wind_effect.z = (periods.z - floor(periods.z)) * two_pi;
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
    sin_wave.xyz *= clothing[vertex_id].w;
    sin_wave.xyz = max(sin_wave.xyz, float3(-1.0));
    const float4 offset_position =
        float4(1.0, 1.0, 1.0, 0.0) *
            (clothing[vertex_id] * sin_wave.x) + position;
    const float4 skinned = float4(
        dot(row0, offset_position), dot(row1, offset_position),
        dot(row2, offset_position), 1.0);
    return almetal_world_avatar_spot_shadow_output(
        constants.modelview_projection * skinned,
        texcoords[vertex_id], constants);
}

static float almetal_world_shadow_depth(
    ALMetalWorldSpotShadowVaryings input)
{
    // Depth-clamped fragments retain their unclamped stage-in z even though
    // the depth attachment receives the viewport-clamped value. Mirror that
    // value into the raw R32F texture explicitly; ordinary clipped fragments
    // are already in range.
    return clamp(input.position.z, 0.0, 1.0);
}

fragment float almetal_world_spot_shadow_fragment(
    ALMetalWorldSpotShadowVaryings input [[stage_in]])
{
    return almetal_world_shadow_depth(input);
}

fragment float almetal_world_spot_shadow_coverage_fragment(
    ALMetalWorldSpotShadowVaryings input [[stage_in]],
    array<texture2d<float>, 32> textures [[texture(0)]],
    sampler texture_sampler [[sampler(0)]],
    constant ALMetalWorldSpotShadowConstants& constants [[buffer(0)]])
{
    const uint texture_index = min(input.texture_index, 31u);
    const float texture_alpha = textures[texture_index].sample(
        texture_sampler, input.texcoord).a;
    const float cutoff = constants.alpha_cutoffs[texture_index];
    const uint coverage = constants.config.y;
    constexpr float quantization_bias = 1.0 / 512.0;

    if (coverage == 7u || coverage == 8u)
    {
        const float alpha = coverage == 8u
            ? texture_alpha * input.vertex_alpha : texture_alpha;
        if (alpha < cutoff)
        {
            discard_fragment();
        }
        if (coverage == 8u && alpha < 0.88)
        {
            const int gl_y = int(float(constants.config.w) -
                                 input.position.y);
            if (((int(input.position.x) + gl_y) & 1) == 0)
            {
                discard_fragment();
            }
        }
    }
    else if (coverage == 4u)
    {
        if (texture_alpha < cutoff - quantization_bias)
        {
            discard_fragment();
        }
    }
    else if (coverage == 5u)
    {
        if (texture_alpha * input.vertex_alpha <
            cutoff - quantization_bias)
        {
            discard_fragment();
        }
    }
    else
    {
        const bool blended = coverage == 3u || coverage == 6u;
        if (texture_alpha < cutoff -
                (blended ? 0.0 : quantization_bias))
        {
            discard_fragment();
        }
        float alpha = texture_alpha;
        if (coverage != 2u)
        {
            alpha *= input.vertex_alpha;
        }
        if (alpha < 0.05)
        {
            discard_fragment();
        }
        if (blended && alpha < 0.88)
        {
            // Metal's fragment position has a top-left origin. Recover GL's
            // lower-left gl_FragCoord row before applying the live shader's
            // checkerboard, including its integer truncation at pixel centres.
            const int gl_y = int(float(constants.config.w) -
                                 input.position.y);
            if (((int(input.position.x) + gl_y) & 1) == 0)
            {
                discard_fragment();
            }
        }
    }
    return almetal_world_shadow_depth(input);
}

vertex ALMetalWorldWaterExclusionVaryings
almetal_world_water_exclusion_vertex(
    const device float4* positions [[buffer(0)]],
    constant ALMetalWorldWaterExclusionConstants& constants [[buffer(1)]],
    uint vertex_id [[vertex_id]])
{
    const float4 object_position =
        float4(positions[vertex_id].xyz, 1.0);
    float4 clip_position = constants.modelview_projection *
                           object_position;
    clip_position.y = -clip_position.y;

    ALMetalWorldWaterExclusionVaryings output;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.agent_position = (
        constants.eye_to_agent * constants.modelview * object_position).xyz;
    return output;
}

fragment float4 almetal_world_water_exclusion_fragment(
    ALMetalWorldWaterExclusionVaryings input [[stage_in]],
    constant ALMetalWorldWaterExclusionConstants& constants [[buffer(0)]],
    texture2d<float, access::read> cell_map [[texture(0)]])
{
    const float2 cell = floor(
        (input.agent_position.xy - constants.cell_square.xy) /
        constants.cell_square.z);
    float water_height = constants.camera_default_height.w;
    if (all(cell >= float2(0.0)) &&
        all(cell < float2(constants.cell_square.w)))
    {
        water_height = cell_map.read(uint2(cell)).r;
    }
    if ((input.agent_position.z - water_height) *
        (constants.camera_default_height.z - water_height) < 0.0)
    {
        discard_fragment();
    }
    // Color writes are disabled in the pipeline. Surviving fragments update
    // only EXCLUDE_BIT through the typed stencil state.
    return 0.0;
}

static float2 almetal_world_water_square_point(
    float2 at, constant ALMetalWorldWaterHazeConstants& constants)
{
    const float side = constants.cell_square.w;
    const float2 centre = constants.cell_square.xy +
        0.5 * side * constants.cell_square.z;
    return centre + (at - 0.5 * side) * constants.cell_square.z;
}

static float almetal_world_depth_pyramid_farthest(
    float a, float b, uint reverse_z)
{
    return reverse_z != 0u ? min(a, b) : max(a, b);
}

static float almetal_world_depth_pyramid_farthest4(
    float4 value, uint reverse_z)
{
    return almetal_world_depth_pyramid_farthest(
        almetal_world_depth_pyramid_farthest(
            value.x, value.y, reverse_z),
        almetal_world_depth_pyramid_farthest(
            value.z, value.w, reverse_z),
        reverse_z);
}

kernel void almetal_world_depth_pyramid_first(
    texture2d<float> source [[texture(0)]],
    texture2d<float, access::write> level0 [[texture(1)]],
    texture2d<float, access::write> level1 [[texture(2)]],
    texture2d<float, access::write> level2 [[texture(3)]],
    texture2d<float, access::write> level3 [[texture(4)]],
    texture2d<float, access::write> level4 [[texture(5)]],
    constant uint& reverse_z [[buffer(0)]],
    uint2 position [[thread_position_in_grid]],
    uint2 local [[thread_position_in_threadgroup]],
    uint2 group [[threadgroup_position_in_grid]])
{
    threadgroup float tile[16][16];
    const uint2 source_last = uint2(
        source.get_width() - 1u, source.get_height() - 1u);
    const uint2 below = 2u * position;
    float depth = almetal_world_depth_pyramid_farthest4(
        float4(
            source.read(min(below, source_last)).r,
            source.read(min(below + uint2(1u, 0u), source_last)).r,
            source.read(min(below + uint2(0u, 1u), source_last)).r,
            source.read(min(below + uint2(1u), source_last)).r),
        reverse_z);
    level0.write(float4(depth), position);
    tile[local.y][local.x] = depth;

    for (uint level = 1u; level < 5u; ++level)
    {
        const uint side = 16u >> level;
        const bool reduces = all(local < uint2(side));
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (reduces)
        {
            const uint2 at = 2u * local;
            depth = almetal_world_depth_pyramid_farthest4(
                float4(tile[at.y][at.x], tile[at.y][at.x + 1u],
                       tile[at.y + 1u][at.x],
                       tile[at.y + 1u][at.x + 1u]),
                reverse_z);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (reduces)
        {
            tile[local.y][local.x] = depth;
            const uint2 target = group * side + local;
            switch (level)
            {
                case 1u: level1.write(float4(depth), target); break;
                case 2u: level2.write(float4(depth), target); break;
                case 3u: level3.write(float4(depth), target); break;
                default: level4.write(float4(depth), target); break;
            }
        }
    }
}

kernel void almetal_world_depth_pyramid_last(
    texture2d<float, access::read> source [[texture(0)]],
    texture2d<float, access::write> level5 [[texture(1)]],
    texture2d<float, access::write> level6 [[texture(2)]],
    texture2d<float, access::write> level7 [[texture(3)]],
    constant uint& reverse_z [[buffer(0)]],
    uint2 position [[thread_position_in_grid]],
    uint2 local [[thread_position_in_threadgroup]],
    uint2 group [[threadgroup_position_in_grid]])
{
    threadgroup float tile[16][16];
    const uint2 source_last = uint2(
        source.get_width() - 1u, source.get_height() - 1u);
    const uint2 below = 2u * position;
    float depth = almetal_world_depth_pyramid_farthest4(
        float4(
            source.read(min(below, source_last)).r,
            source.read(min(below + uint2(1u, 0u), source_last)).r,
            source.read(min(below + uint2(0u, 1u), source_last)).r,
            source.read(min(below + uint2(1u), source_last)).r),
        reverse_z);
    const uint2 level5_size = uint2(
        level5.get_width(), level5.get_height());
    if (all(position < level5_size))
    {
        level5.write(float4(depth), position);
    }
    tile[local.y][local.x] = depth;

    for (uint level = 1u; level < 3u; ++level)
    {
        const uint side = 16u >> level;
        const bool reduces = all(local < uint2(side));
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (reduces)
        {
            const uint2 at = 2u * local;
            depth = almetal_world_depth_pyramid_farthest4(
                float4(tile[at.y][at.x], tile[at.y][at.x + 1u],
                       tile[at.y + 1u][at.x],
                       tile[at.y + 1u][at.x + 1u]),
                reverse_z);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (reduces)
        {
            tile[local.y][local.x] = depth;
            const uint2 target = group * side + local;
            if (level == 1u)
            {
                if (target.x < level6.get_width() &&
                    target.y < level6.get_height())
                {
                    level6.write(float4(depth), target);
                }
            }
            else if (target.x < level7.get_width() &&
                     target.y < level7.get_height())
            {
                level7.write(float4(depth), target);
            }
        }
    }
}

kernel void almetal_world_water_wave_occlusion(
    device ushort* factors [[buffer(0)]],
    constant ALMetalWorldWaterOcclusionConstants& constants [[buffer(1)]],
    texture2d<float, access::read> cell_map [[texture(0)]],
    array<texture2d<float, access::read>, 8> pyramid [[texture(1)]],
    uint patch_id [[thread_position_in_grid]])
{
    if (patch_id >= constants.config.x)
    {
        return;
    }
    const uint factor = patch_id * 6u;
    if (factors[factor] == 0u)
    {
        return;
    }

    constexpr int patches_per_cell = 8;
    const int2 patch_dimensions = max(
        constants.patch_rect.zw - constants.patch_rect.xy,
        int2(1)) * patches_per_cell;
    const bool slow_x = constants.patch_order.x != 0;
    const int run = slow_x ? patch_dimensions.y : patch_dimensions.x;
    int slow = int(patch_id) / run;
    int fast = int(patch_id) - slow * run;
    slow = constants.patch_order.y != 0
        ? (slow_x ? patch_dimensions.x : patch_dimensions.y) - 1 - slow
        : slow;
    fast = constants.patch_order.z != 0 ? run - 1 - fast : fast;
    const int2 at_patch = slow_x
        ? int2(slow, fast) : int2(fast, slow);
    const int2 cell = constants.patch_rect.xy +
        at_patch / patches_per_cell;
    const int2 square = at_patch -
        (at_patch / patches_per_cell) * patches_per_cell;
    const float side = constants.cell_square.w;
    const float2 centre = constants.cell_square.xy +
        0.5 * side * constants.cell_square.z;
    const float2 south_west = centre +
        (float2(cell) + float2(square) / float(patches_per_cell) -
         0.5 * side) * constants.cell_square.z;
    const float2 north_east = centre +
        (float2(cell) + float2(square + 1) /
             float(patches_per_cell) - 0.5 * side) *
            constants.cell_square.z;
    const float water_height = cell_map.read(uint2(cell)).r;
    const float displacement = constants.screen_displacement.z;
    const float3 lo = float3(
        min(south_west, north_east), water_height - displacement);
    const float3 hi = float3(
        max(south_west, north_east), water_height + displacement);

    float2 ndc_lo = float2(1.0);
    float2 ndc_hi = float2(-1.0);
    const bool reverse_z = constants.config.z != 0u;
    float nearest = reverse_z ? 0.0 : 1.0;
    bool visible = false;
    for (uint corner_index = 0u; corner_index < 8u; ++corner_index)
    {
        const float3 corner = float3(
            (corner_index & 1u) != 0u ? hi.x : lo.x,
            (corner_index & 2u) != 0u ? hi.y : lo.y,
            (corner_index & 4u) != 0u ? hi.z : lo.z);
        const float4 clip = constants.modelview_projection *
            float4(corner, 1.0);
        if (clip.w <= 0.0)
        {
            visible = true;
            break;
        }
        const float3 ndc = clip.xyz / clip.w;
        ndc_lo = min(ndc_lo, ndc.xy);
        ndc_hi = max(ndc_hi, ndc.xy);
        nearest = reverse_z
            ? max(nearest, ndc.z)
            : min(nearest, ndc.z * 0.5 + 0.5);
    }
    if (!visible)
    {
        const int2 size = int2(constants.screen_displacement.xy);
        const int2 p_lo = clamp(
            int2(floor((ndc_lo * 0.5 + 0.5) * float2(size))),
            int2(0), size - 1);
        const int2 p_hi = clamp(
            int2(floor((ndc_hi * 0.5 + 0.5) * float2(size))),
            int2(0), size - 1);
        uint level = 0u;
        while (level < constants.config.y - 1u &&
               any((p_hi >> int(level + 1u)) -
                   (p_lo >> int(level + 1u)) > int2(1)))
        {
            ++level;
        }
        const int2 t_lo = p_lo >> int(level + 1u);
        const int2 t_hi = p_hi >> int(level + 1u);
        if (any(t_hi - t_lo >= int2(4)))
        {
            visible = true;
        }
        else
        {
            float farthest = pyramid[level].read(uint2(t_lo)).r;
            for (int y = t_lo.y; y <= t_hi.y; ++y)
            {
                const int first_x = y == t_lo.y
                    ? t_lo.x + 1 : t_lo.x;
                for (int x = first_x; x <= t_hi.x; ++x)
                {
                    farthest = almetal_world_depth_pyramid_farthest(
                        farthest,
                        pyramid[level].read(uint2(x, y)).r,
                        constants.config.z);
                }
            }
            visible = reverse_z
                ? !(nearest < farthest)
                : !(nearest > farthest);
        }
    }
    if (!visible)
    {
        for (uint index = 0u; index < 6u; ++index)
        {
            factors[factor + index] = 0u;
        }
    }
}

static float2 almetal_world_water_turn(float2 value, uint edge)
{
    if (edge == 1u)
    {
        return float2(1.0 - value.y, value.x);
    }
    if (edge == 2u)
    {
        return float2(1.0 - value.x, 1.0 - value.y);
    }
    if (edge == 3u)
    {
        return float2(value.y, 1.0 - value.x);
    }
    return value;
}

static float2 almetal_world_water_turn_offset(float2 value, uint edge)
{
    if (edge == 1u)
    {
        return float2(-value.y, value.x);
    }
    if (edge == 2u)
    {
        return -value;
    }
    if (edge == 3u)
    {
        return float2(value.y, -value.x);
    }
    return value;
}

static float2 almetal_world_water_between(
    float2 low, float2 high, float2 amount)
{
    return float2(
        amount.x <= 0.0 ? low.x
            : (amount.x >= 1.0 ? high.x : 0.5 * (low.x + high.x)),
        amount.y <= 0.0 ? low.y
            : (amount.y >= 1.0 ? high.y : 0.5 * (low.y + high.y)));
}

static ALMetalWorldWaterHazeVaryings almetal_world_water_haze_output(
    float3 position, constant ALMetalWorldWaterHazeConstants& constants)
{
    float4 clip_position = constants.modelview_projection *
        float4(position, 1.0);
    ALMetalWorldWaterHazeVaryings output;
    output.texcoord = clip_position.xy / clip_position.w * 0.5 + 0.5;
    clip_position.y = -clip_position.y;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    return output;
}

static float3 almetal_world_water_field_position(
    constant ALMetalWorldWaterHazeConstants& constants,
    texture2d<float, access::read> cell_map,
    uint vertex_id, uint instance_id)
{
    const uint part = constants.config.z;
    const uint side = uint(constants.cell_square.w);
    float3 position = 0.0;
    if (part == 0u)
    {
        const uint2 cell = uint2(instance_id % side,
                                 instance_id / side);
        const float2 texel = cell_map.read(cell).rg;
        const int2 signed_cell = int2(cell);
        const bool patch_rectangle = all(
            constants.patch_rect.xy < constants.patch_rect.zw);
        const bool patched = patch_rectangle &&
            all(signed_cell >= constants.patch_rect.xy) &&
            all(signed_cell < constants.patch_rect.zw);
        const bool along_x = patch_rectangle &&
            signed_cell.x >= constants.patch_rect.x &&
            signed_cell.x < constants.patch_rect.z;
        const bool along_y = patch_rectangle &&
            signed_cell.y >= constants.patch_rect.y &&
            signed_cell.y < constants.patch_rect.w;
        const bool fringe =
            (along_x && (signed_cell.y == constants.patch_rect.y - 1 ||
                         signed_cell.y == constants.patch_rect.w)) ||
            (along_y && (signed_cell.x == constants.patch_rect.x - 1 ||
                         signed_cell.x == constants.patch_rect.z));
        const bool drawn = ((uint(texel.y) & 1u) != 0u ||
            constants.rings.w > 0.0) && !patched && !fringe;
        const float2 corner = drawn
            ? float2(float(vertex_id & 1u), float(vertex_id >> 1u))
            : float2(0.0);
        position = float3(almetal_world_water_square_point(
                              float2(cell) + corner, constants),
                          texel.x);
    }
    else if (part == 3u)
    {
        const int width = constants.patch_rect.z -
            constants.patch_rect.x;
        const int height = constants.patch_rect.w -
            constants.patch_rect.y;
        const int instance = int(instance_id);
        int2 cell;
        if (instance < width)
        {
            cell = int2(constants.patch_rect.x + instance,
                        constants.patch_rect.y - 1);
        }
        else if (instance < 2 * width)
        {
            cell = int2(constants.patch_rect.x + instance - width,
                        constants.patch_rect.w);
        }
        else if (instance < 2 * width + height)
        {
            cell = int2(constants.patch_rect.x - 1,
                        constants.patch_rect.y + instance - 2 * width);
        }
        else
        {
            cell = int2(constants.patch_rect.z,
                        constants.patch_rect.y + instance -
                            2 * width - height);
        }
        const float2 texel = cell_map.read(uint2(cell)).rg;
        int edge = 0;
        const bool along_x = cell.x >= constants.patch_rect.x &&
            cell.x < constants.patch_rect.z;
        const bool along_y = cell.y >= constants.patch_rect.y &&
            cell.y < constants.patch_rect.w;
        bool fringe = false;
        if (along_x && cell.y == constants.patch_rect.y - 1)
        {
            edge = 2;
            fringe = true;
        }
        else if (along_x && cell.y == constants.patch_rect.w)
        {
            edge = 0;
            fringe = true;
        }
        else if (along_y && cell.x == constants.patch_rect.x - 1)
        {
            edge = 1;
            fringe = true;
        }
        else if (along_y && cell.x == constants.patch_rect.z)
        {
            edge = 3;
            fringe = true;
        }
        const bool drawn = fringe &&
            ((uint(texel.y) & 1u) != 0u || constants.rings.w > 0.0);
        constexpr int patches = 8;
        constexpr int half_patches = patches / 2;
        const int triangle = int(vertex_id / 3u);
        const int corner = int(vertex_id - uint(triangle * 3));
        float2 at;
        if (triangle < half_patches)
        {
            at = corner == 0 ? float2(float(triangle), 0.0)
                : (corner == 1
                    ? float2(float(triangle + 1), 0.0)
                    : float2(0.0, float(patches)));
        }
        else if (triangle == half_patches)
        {
            at = corner == 0 ? float2(float(half_patches), 0.0)
                : (corner == 1 ? float2(float(patches))
                               : float2(0.0, float(patches)));
        }
        else
        {
            const int patch = triangle - 1;
            at = corner == 0 ? float2(float(patch), 0.0)
                : (corner == 1
                    ? float2(float(patch + 1), 0.0)
                    : float2(float(patches)));
        }
        const float2 local = drawn
            ? almetal_world_water_turn(at / float(patches), uint(edge))
            : float2(0.0);
        position = float3(almetal_world_water_square_point(
                              float2(cell) + local, constants),
                          texel.x);
    }
    else if (part == 2u)
    {
        const int cells = int(side);
        const int half_cells = (cells - 1) / 2;
        const float square_half = constants.rings.x;
        const float rim = constants.rings.z;
        const float inner = (0.0 - 0.5 * float(side)) *
            constants.cell_square.z;
        const float outer = clamp(-2.0 * square_half, -rim, rim);
        const int triangle = int(vertex_id / 3u);
        const int corner = int(vertex_id - uint(triangle * 3));
        float2 offset;
        if (triangle < half_cells)
        {
            offset = corner == 0
                ? float2(-square_half, outer)
                : float2((float(triangle + (corner == 1 ? 1 : 0)) -
                          0.5 * float(side)) * constants.cell_square.z,
                         inner);
        }
        else if (triangle == half_cells)
        {
            offset = corner == 0 ? float2(-square_half, outer)
                : (corner == 1 ? float2(0.0, outer)
                    : float2((float(half_cells) - 0.5 * float(side)) *
                             constants.cell_square.z, inner));
        }
        else if (triangle <= cells)
        {
            const int cell = triangle - 1;
            offset = corner == 0 ? float2(0.0, outer)
                : float2((float(cell + (corner == 1 ? 1 : 0)) -
                          0.5 * float(side)) * constants.cell_square.z,
                         inner);
        }
        else
        {
            offset = corner == 0 ? float2(0.0, outer)
                : (corner == 1 ? float2(square_half, outer)
                    : float2((float(cells) - 0.5 * float(side)) *
                             constants.cell_square.z, inner));
        }
        const float2 centre = constants.cell_square.xy +
            0.5 * float(side) * constants.cell_square.z;
        const float2 point = constants.rings.w > 0.0
            ? almetal_world_water_turn_offset(offset, instance_id)
            : float2(0.0);
        position = float3(centre + point, constants.default_height.x);
    }
    else
    {
        const uint ring = instance_id / 12u;
        const uint quad = instance_id - ring * 12u;
        int2 at;
        int edge = -1;
        if (quad < 4u)
        {
            at = int2(int(quad), 0);
            edge = quad == 1u || quad == 2u ? 2 : -1;
        }
        else if (quad < 8u)
        {
            at = int2(int((quad & 1u) * 3u),
                      1 + int((quad - 4u) / 2u));
            edge = (quad & 1u) == 0u ? 1 : 3;
        }
        else
        {
            at = int2(int(quad - 8u), 3);
            edge = quad == 9u || quad == 10u ? 0 : -1;
        }
        const float scale = constants.rings.x * float(1u << ring);
        const float rim = constants.rings.z;
        const float2 low = clamp((float2(at) - 2.0) * scale,
                                 float2(-rim), float2(rim));
        const float2 high = clamp((float2(at) - 1.0) * scale,
                                  float2(-rim), float2(rim));
        const bool drawn = constants.rings.w > 0.0 &&
            !(ring == 0u && edge >= 0);
        const uint triangle = vertex_id / 3u;
        const uint corner = vertex_id - triangle * 3u;
        float2 amount;
        if (edge >= 0)
        {
            if (triangle == 0u)
            {
                amount = corner == 0u ? float2(0.0, 0.0)
                    : (corner == 1u ? float2(0.5, 0.0)
                                     : float2(0.0, 1.0));
            }
            else if (triangle == 1u)
            {
                amount = corner == 0u ? float2(0.5, 0.0)
                    : (corner == 1u ? float2(1.0, 1.0)
                                     : float2(0.0, 1.0));
            }
            else
            {
                amount = corner == 0u ? float2(0.5, 0.0)
                    : (corner == 1u ? float2(1.0, 0.0)
                                     : float2(1.0, 1.0));
            }
            amount = almetal_world_water_turn(amount, uint(edge));
        }
        else if (triangle == 0u)
        {
            amount = 0.0;
        }
        else if (triangle == 1u)
        {
            amount = corner == 0u ? float2(0.0, 0.0)
                : (corner == 1u ? float2(1.0, 0.0)
                                 : float2(1.0, 1.0));
        }
        else
        {
            amount = corner == 0u ? float2(0.0, 0.0)
                : (corner == 1u ? float2(1.0, 1.0)
                                 : float2(0.0, 1.0));
        }
        const float2 centre = constants.cell_square.xy +
            0.5 * float(side) * constants.cell_square.z;
        position = float3(
            centre + (drawn
                ? almetal_world_water_between(low, high, amount)
                : float2(0.0)),
            constants.default_height.x);
    }
    return position;
}

vertex ALMetalWorldWaterHazeVaryings
almetal_world_water_haze_field_vertex(
    constant ALMetalWorldWaterHazeConstants& constants [[buffer(0)]],
    texture2d<float, access::read> cell_map [[texture(0)]],
    uint vertex_id [[vertex_id]], uint instance_id [[instance_id]])
{
    return almetal_world_water_haze_output(
        almetal_world_water_field_position(
            constants, cell_map, vertex_id, instance_id),
        constants);
}

vertex ALMetalWorldWaterHazeVaryings
almetal_world_water_haze_fullscreen_vertex(
    constant ALMetalWorldWaterHazeConstants& constants [[buffer(0)]],
    uint vertex_id [[vertex_id]])
{
    const float2 texcoord = float2((vertex_id << 1u) & 2u,
                                   vertex_id & 2u);
    ALMetalWorldWaterHazeVaryings output;
    output.position = float4(texcoord.x * 2.0 - 1.0,
                             1.0 - texcoord.y * 2.0,
                             0.0, 1.0);
    output.texcoord = texcoord;
    return output;
}

fragment float4 almetal_world_water_haze_fragment(
    ALMetalWorldWaterHazeVaryings input [[stage_in]],
    constant ALMetalWorldWaterHazeConstants& constants [[buffer(0)]],
    texture2d<float> depth_texture [[texture(33)]],
    sampler point_sampler [[sampler(1)]])
{
    const float depth = depth_texture.sample(point_sampler,
                                             input.texcoord).r;
    const bool reverse_z = constants.config.x != 0u;
    const float ndc_z = reverse_z ? depth : depth * 2.0 - 1.0;
    float4 position = constants.inverse_projection *
        float4(input.texcoord * 2.0 - 1.0, ndc_z, 1.0);
    position.xyz /= max(position.w, 0.000001);
    position.w = 1.0;

    const bool far_depth = reverse_z ? depth <= 0.0 : depth >= 1.0;
    if (far_depth)
    {
        const float3 direction = normalize(position.xyz);
        const float eye_surface = -dot(direction,
                                       constants.water_plane.xyz);
        const float entry = constants.water_plane.w > 0.0 &&
                                  eye_surface > 0.0
            ? constants.water_plane.w / eye_surface : 0.0;
        position.xyz = direction * (entry + 1024.0);
    }

    if (dot(position.xyz, constants.water_plane.xyz) +
            constants.water_plane.w > 0.0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    const float3 view = normalize(position.xyz);
    const float eye_surface = -dot(view, constants.water_plane.xyz);
    const float eye_depth = max(-constants.water_plane.w, 0.0);
    const float3 intersection = constants.water_plane.w > 0.0
        ? view * constants.water_plane.w / eye_surface : float3(0.0);
    const float length_in_water = max(
        length(position.xyz - intersection), 0.1);
    const float density = constants.fog.y;
    const float light_scale = constants.fog.x;
    constexpr float base = 0.98;
    const float term1 = -density * pow(base, light_scale * eye_depth);
    const float term2 = density + light_scale * eye_surface;
    const float term3 = pow(base, term2 * length_in_water) - 1.0;
    const float light = pow(min(term1 / term2 * term3, 1.0),
                            1.0 / 1.7);
    const float transmission = pow(base,
                                   length_in_water * density);
    float4 fogged = float4(
        almetal_srgb_to_linear(constants.fog_color.rgb) * light,
        transmission);
    fogged.a = max(pow(fogged.a, 1.7), 0.0);
    return max(fogged, float4(0.0));
}

static ALMetalWorldWaterSurfaceVaryings
almetal_world_water_surface_output(
    float3 agent_position,
    constant ALMetalWorldWaterHazeConstants& field,
    constant ALMetalWorldWaterSurfaceConstants& water)
{
    float4 clip_position = field.modelview_projection *
        float4(agent_position, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldWaterSurfaceVaryings output;
    output.position = water.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.eye_position = (
        water.modelview * float4(agent_position, 1.0)).xyz;
    output.agent_view = agent_position - water.eye_position.xyz;
    return output;
}

vertex ALMetalWorldWaterSurfaceVaryings
almetal_world_flat_water_vertex(
    constant ALMetalWorldWaterHazeConstants& field [[buffer(0)]],
    constant ALMetalWorldWaterSurfaceConstants& water [[buffer(1)]],
    texture2d<float, access::read> cell_map [[texture(0)]],
    uint vertex_id [[vertex_id]], uint instance_id [[instance_id]])
{
    return almetal_world_water_surface_output(
        almetal_world_water_field_position(
            field, cell_map, vertex_id, instance_id),
        field, water);
}

static float2 almetal_world_water_hex_skew(float2 point)
{
    return float2(
        point.x - 0.57735027 * point.y,
        1.15470054 * point.y);
}

static void almetal_world_water_hex_triangle(
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

static uint almetal_world_water_hex_hash(uint2 identifier)
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

static void almetal_world_water_hex_cells(
    float2 point, float2 fraction, int2 base,
    thread float2 offset[3], thread float3& weight)
{
    const float2 q = fraction +
        almetal_world_water_hex_skew(point);
    const float2 cell = floor(q);
    int2 corners[3];
    float3 barycentric;
    almetal_world_water_hex_triangle(
        q - cell, corners[0], corners[1], corners[2], barycentric);
    float3 sharpened = pow(barycentric, 7.0);
    sharpened /= max(dot(sharpened, float3(1.0)), 1.0e-12);
    sharpened -= 0.01;
    weight = max(float3(0.0), sharpened);
    weight /= max(dot(weight, float3(1.0)), 1.0e-12);
    const uint2 identifier = uint2(base) + uint2(int2(cell));
    for (uint index = 0; index < 3; ++index)
    {
        const uint hash = almetal_world_water_hex_hash(
            identifier + uint2(corners[index]));
        offset[index] = float2(hash & 0x7ffu,
                               (hash >> 11) & 0x7ffu) /
            2048.0;
    }
}

static float almetal_world_water_hex_gain(
    float3 weight, float footprint, float2 texels)
{
    const float cell_texels = min(texels.x, texels.y) /
        max(footprint, 1.0);
    return mix(1.0,
               rsqrt(max(dot(weight, weight), 1.0e-12)),
               smoothstep(4.0, 16.0, cell_texels));
}

static float3 almetal_world_water_hex_read(
    texture2d<float> map, sampler normal_sampler,
    float2 uv, float2 uv_dx, float2 uv_dy,
    thread float2 offset[3], float3 weight, float gain,
    float2 mean)
{
    float2 xy = 0.0;
    float z = 0.0;
    for (uint index = 0; index < 3; ++index)
    {
        if (weight[index] > 0.0)
        {
            const float3 normal = map.sample(
                normal_sampler, uv + offset[index],
                gradient2d(uv_dx, uv_dy)).xyz * 2.0 - 1.0;
            xy += weight[index] * (normal.xy - mean);
            z += weight[index] * normal.z;
        }
    }
    return float3(mean + gain * xy, z);
}

static float3 almetal_world_water_tiled_layer(
    texture2d<float> map_a, texture2d<float> map_b,
    sampler normal_sampler, float2 uv, float2 point,
    float2 fraction, int2 base, float blend, float4 means)
{
    const float2 uv_dx = dfdx(uv);
    const float2 uv_dy = dfdy(uv);
    float2 offset[3];
    float3 weight;
    almetal_world_water_hex_cells(
        point, fraction, base, offset, weight);
    const float2 texels = float2(map_a.get_width(), map_a.get_height());
    const float gain = almetal_world_water_hex_gain(
        weight, max(length(uv_dx * texels),
                    length(uv_dy * texels)), texels);
    float3 normal = almetal_world_water_hex_read(
        map_a, normal_sampler, uv, uv_dx, uv_dy,
        offset, weight, gain, means.xy);
    if (blend > 0.0)
    {
        normal = mix(normal, almetal_world_water_hex_read(
            map_b, normal_sampler, uv, uv_dx, uv_dy,
            offset, weight, gain, means.zw), blend);
    }
    return normal;
}

static float3 almetal_world_water_plain_layer(
    texture2d<float> map_a, texture2d<float> map_b,
    sampler normal_sampler, float2 uv, float blend)
{
    float3 normal = map_a.sample(normal_sampler, uv).xyz * 2.0 - 1.0;
    if (blend > 0.0)
    {
        normal = mix(normal,
                     map_b.sample(normal_sampler, uv).xyz * 2.0 - 1.0,
                     blend);
    }
    return normal;
}

static float2 almetal_world_water_wave_clamp(
    float2 value, float limit)
{
    const float distance = max(length(value), 0.001);
    return value * (min(distance, limit) / distance);
}

static void almetal_world_water_wave_normals(
    float2 agent_view,
    texture2d<float> map_a, texture2d<float> map_b,
    sampler normal_sampler,
    constant ALMetalWorldWaterSurfaceConstants& water,
    thread float3& big, thread float3& little_xy,
    thread float3& little_zw)
{
    const float2 clamped = almetal_world_water_wave_clamp(
        agent_view, water.water_far.x);
    float2 swept = clamped;
    swept.x += (cos(clamped.x * 0.08 + water.sweep_phase.x) +
                sin(clamped.y * 0.02 + water.sweep_phase.y)) * 6.0;
    const float2 big_uv = swept * 0.04 + water.big_phase.xy;
    const float4 little_uv = float4(
        swept * float2(0.45, 0.9) + water.little_phase.xy,
        swept * float2(0.1, 0.2) + water.little_phase.zw);
    const float blend = water.parameters.x;
    if (water.config.z >= 1u)
    {
        big = almetal_world_water_tiled_layer(
            map_a, map_b, normal_sampler, big_uv,
            clamped * 0.04, water.lattice_fraction.xy,
            water.lattice_base.xy, blend, water.map_mean);
    }
    else
    {
        big = almetal_world_water_plain_layer(
            map_a, map_b, normal_sampler, big_uv, blend);
    }
    little_xy = almetal_world_water_plain_layer(
        map_a, map_b, normal_sampler, little_uv.xy, blend);
    if (water.config.z >= 2u)
    {
        little_zw = almetal_world_water_tiled_layer(
            map_a, map_b, normal_sampler, little_uv.zw,
            clamped * float2(0.1, 0.2),
            water.lattice_fraction.zw, water.lattice_base.zw,
            blend, water.map_mean);
    }
    else
    {
        little_zw = almetal_world_water_plain_layer(
            map_a, map_b, normal_sampler, little_uv.zw, blend);
    }
}

static float almetal_world_water_hex_height(
    texture2d<float> map, sampler height_sampler, uint field,
    float2 uv, float lod, thread float2 offset[3],
    float3 weight, float gain)
{
    float height = 0.0;
    for (uint index = 0; index < 3; ++index)
    {
        if (weight[index] > 0.0)
        {
            height += weight[index] * map.sample(
                height_sampler, uv + offset[index], level(lod))[field];
        }
    }
    return gain * height;
}

static float almetal_world_water_map_height(
    texture2d<float> map, sampler height_sampler,
    float2 big_uv, float2 mid_uv, float2 point, float spacing,
    constant ALMetalWorldWaterSurfaceConstants& water)
{
    const float2 texels = float2(map.get_width(), map.get_height());
    const float big_step = spacing * 0.04 * max(texels.x, texels.y);
    const float mid_step = spacing * 0.2 * max(texels.x, texels.y);
    const float big_lod = log2(max(big_step, 1.0));
    const float mid_lod = log2(max(mid_step, 1.0));
    float big;
    float mid;
    float2 offset[3];
    float3 weight;
    if (water.config.z >= 1u)
    {
        almetal_world_water_hex_cells(
            point * 0.04, water.lattice_fraction.xy,
            water.lattice_base.xy, offset, weight);
        big = almetal_world_water_hex_height(
            map, height_sampler, 0u, big_uv, big_lod,
            offset, weight,
            almetal_world_water_hex_gain(weight, big_step, texels));
    }
    else
    {
        big = map.sample(
            height_sampler, big_uv, level(big_lod)).r;
    }
    if (water.config.z >= 2u)
    {
        almetal_world_water_hex_cells(
            point * float2(0.1, 0.2), water.lattice_fraction.zw,
            water.lattice_base.zw, offset, weight);
        mid = almetal_world_water_hex_height(
            map, height_sampler, 1u, mid_uv, mid_lod,
            offset, weight,
            almetal_world_water_hex_gain(weight, mid_step, texels));
    }
    else
    {
        mid = map.sample(
            height_sampler, mid_uv, level(mid_lod)).g;
    }
    return 12.5 * big + 3.0 * mid;
}

static float almetal_world_water_displacement(
    float3 position,
    texture2d<float> height_map_a,
    texture2d<float> height_map_b,
    sampler height_sampler,
    constant ALMetalWorldWaterSurfaceConstants& water)
{
    const float amplitude = water.wave_reach_amplitude.y;
    if (amplitude <= 0.0)
    {
        return 0.0;
    }
    const float2 fade_vector =
        position.xy - water.wave_eye_density.xy;
    const float distance_from_layout_eye = length(fade_vector);
    float fade = 1.0 - smoothstep(
        0.75 * water.wave_reach_amplitude.x,
        water.wave_reach_amplitude.x,
        distance_from_layout_eye);
    const float camera_level = 1.0 - smoothstep(
        amplitude, 2.0 * amplitude,
        abs(water.wave_eye_density.z - position.z));
    fade *= mix(
        1.0,
        smoothstep(0.0, max(8.0, 4.0 * amplitude),
                   distance_from_layout_eye),
        camera_level);
    if (fade <= 0.0)
    {
        return 0.0;
    }

    const float2 relative = position.xy - water.eye_position.xy;
    const float2 clamped = almetal_world_water_wave_clamp(
        relative, water.water_far.x);
    float2 swept = clamped;
    swept.x += (cos(clamped.x * 0.08 + water.sweep_phase.x) +
                sin(clamped.y * 0.02 + water.sweep_phase.y)) * 6.0;
    const float2 big_uv = swept * 0.04 + water.big_phase.xy;
    const float2 mid_uv =
        swept * float2(0.1, 0.2) + water.little_phase.zw;
    const float spacing = max(
        distance(position, water.eye_position.xyz) /
            water.wave_eye_density.w,
        0.5);
    float height = water.wave_reach_amplitude.z > 0.0
        ? water.wave_reach_amplitude.z *
            almetal_world_water_map_height(
                height_map_a, height_sampler, big_uv, mid_uv,
                clamped, spacing, water)
        : 0.0;
    if (water.parameters.x > 0.0)
    {
        const float next = water.wave_reach_amplitude.w > 0.0
            ? water.wave_reach_amplitude.w *
                almetal_world_water_map_height(
                    height_map_b, height_sampler, big_uv, mid_uv,
                    clamped, spacing, water)
            : 0.0;
        height = mix(height, next, water.parameters.x);
    }
    return fade * height;
}

static float almetal_world_water_edge_exact(
    float low, float high, float amount)
{
    return amount >= 1.0 ? high : low + (high - low) * amount;
}

[[patch(quad, 4)]]
vertex ALMetalWorldWaterSurfaceVaryings
almetal_world_wave_water_vertex(
    constant ALMetalWorldWaterHazeConstants& field [[buffer(0)]],
    constant ALMetalWorldWaterSurfaceConstants& water [[buffer(1)]],
    texture2d<float, access::read> cell_map [[texture(0)]],
    texture2d<float> height_map_a [[texture(1)]],
    texture2d<float> height_map_b [[texture(2)]],
    sampler height_sampler [[sampler(0)]],
    uint patch_id [[patch_id]],
    float2 patch_position [[position_in_patch]])
{
    constexpr int patches_per_cell = 8;
    const int2 patch_dimensions = max(
        water.patch_rect.zw - water.patch_rect.xy, int2(1)) *
        patches_per_cell;
    const bool slow_x = water.patch_order.x != 0;
    const int run = slow_x
        ? patch_dimensions.y : patch_dimensions.x;
    int slow = int(patch_id) / run;
    int fast = int(patch_id) - slow * run;
    slow = water.patch_order.y != 0
        ? (slow_x ? patch_dimensions.x : patch_dimensions.y) - 1 - slow
        : slow;
    fast = water.patch_order.z != 0 ? run - 1 - fast : fast;
    const int2 at_patch = slow_x
        ? int2(slow, fast) : int2(fast, slow);
    const int2 cell = water.patch_rect.xy +
        at_patch / patches_per_cell;
    const int2 square = at_patch -
        (at_patch / patches_per_cell) * patches_per_cell;
    const float water_height = cell_map.read(uint2(cell)).r;
    const float2 south_west = almetal_world_water_square_point(
        float2(cell) + float2(square) / float(patches_per_cell),
        field);
    const float2 north_east = almetal_world_water_square_point(
        float2(cell) + float2(square + 1) /
            float(patches_per_cell),
        field);
    float3 agent_position = float3(
        almetal_world_water_edge_exact(
            south_west.x, north_east.x, patch_position.x),
        almetal_world_water_edge_exact(
            south_west.y, north_east.y, patch_position.y),
        water_height);
    agent_position.z += almetal_world_water_displacement(
        agent_position, height_map_a, height_map_b,
        height_sampler, water);
    return almetal_world_water_surface_output(
        agent_position, field, water);
}

static float3 almetal_world_underwater_fog(
    float3 eye_position, float3 color,
    constant ALMetalWorldWaterHazeConstants& field)
{
    const float3 view = normalize(eye_position);
    const float eye_surface = -dot(view, field.water_plane.xyz);
    const float eye_depth = max(-field.water_plane.w, 0.0);
    const float3 intersection = field.water_plane.w > 0.0
        ? view * field.water_plane.w / eye_surface : float3(0.0);
    const float length_in_water = max(
        length(eye_position - intersection), 0.1);
    const float density = field.fog.y;
    const float light_scale = field.fog.x;
    constexpr float base = 0.98;
    const float term1 = -density * pow(base, light_scale * eye_depth);
    const float term2 = density + light_scale * eye_surface;
    const float term3 = pow(base, term2 * length_in_water) - 1.0;
    const float light = pow(min(term1 / term2 * term3, 1.0),
                            1.0 / 1.7);
    const float transmission = pow(base, length_in_water * density);
    return color * transmission +
        almetal_srgb_to_linear(field.fog_color.rgb) * light;
}

fragment float4 almetal_world_underwater_fragment(
    ALMetalWorldWaterSurfaceVaryings input [[stage_in]],
    constant ALMetalWorldWaterHazeConstants& field [[buffer(0)]],
    constant ALMetalWorldWaterSurfaceConstants& water [[buffer(1)]],
    texture2d<float> normal_map_a [[texture(1)]],
    texture2d<float> normal_map_b [[texture(2)]],
    texture2d<float> refraction_color [[texture(3)]],
    sampler normal_sampler [[sampler(0)]],
    sampler screen_sampler [[sampler(1)]])
{
    const bool mirror_clipped =
        water.environment_config.y != 0u &&
        dot(float4(input.eye_position, 1.0),
            water.mirror_clip_plane) < 0.0;
    float3 wave1;
    float3 wave2;
    float3 wave3;
    almetal_world_water_wave_normals(
        input.agent_view.xy, normal_map_a, normal_map_b,
        normal_sampler, water, wave1, wave2, wave3);
    const float3 wave = normalize(wave1 + wave2 + wave3);
    const float2 screen_uv = input.position.xy / water.screen.xy;
    const float2 distorted_uv = mix(
        screen_uv,
        screen_uv + wave.xy * water.parameters.y,
        water.parameters.z);
    const float3 source = water.config.w != 0u
        ? refraction_color.sample(screen_sampler, distorted_uv).rgb
        : almetal_srgb_to_linear(field.fog_color.rgb);
    if (mirror_clipped)
    {
        discard_fragment();
    }
    return max(float4(
        almetal_world_underwater_fog(
            input.eye_position, source, field), 0.0),
        float4(0.0));
}

static void almetal_world_alpha_atmospherics(
    float3 eye_position,
    constant ALMetalWorldAlphaEnvironment& environment,
    thread float3& additive,
    thread float3& attenuation)
{
    const float3x3 eye_to_sky = float3x3(
        environment.eye_to_sky[0].xyz,
        environment.eye_to_sky[1].xyz,
        environment.eye_to_sky[2].xyz);
    float3 relative_position = eye_to_sky * eye_position;
    const float3 light_direction =
        eye_to_sky * environment.light_direction.xyz;

    const float height = max(relative_position.y, 0.0) /
        max(environment.glow_max_y.w, 1.0);
    const float maximum = max(height, 1.0);
    float quotient = height / maximum;
    float reciprocal = 1.0 / maximum;
    quotient *= quotient;
    quotient *= quotient;
    quotient *= quotient;
    reciprocal *= reciprocal;
    reciprocal *= reciprocal;
    reciprocal *= reciprocal;
    relative_position *= pow(quotient + reciprocal, -0.125) /
        maximum;

    const float3 direction = normalize(relative_position);
    const float distance = length(relative_position);
    const float density_distance =
        distance * environment.atmospheric.x;
    const float3 combined_haze = exp(
        -environment.haze_extinction.xyz * density_distance *
        environment.atmospheric.y);
    attenuation = combined_haze;

    float haze_glow = max(
        1.0 - dot(direction, light_direction), 0.03);
    haze_glow *= environment.glow_max_y.x;
    haze_glow = clamp(
        pow(haze_glow, environment.glow_max_y.z),
        -100000.0, 100000.0);
    haze_glow = (haze_glow + 0.25) * environment.atmospheric.z;
    additive = environment.haze_additive.xyz +
        environment.haze_additive_glow.xyz * haze_glow;
    additive *= 1.0 - combined_haze;
    additive = min(additive, float3(10.0));
}

static float3 almetal_world_alpha_atmospheric_fog(
    float3 color, float3 additive, float3 attenuation,
    constant ALMetalWorldAlphaEnvironment& environment)
{
    color *= attenuation.r;
    color += almetal_srgb_to_linear(additive * 2.0) *
             environment.atmospheric.w;
    return color;
}

static float3 almetal_world_alpha_water_fog(
    float3 eye_position, float3 color,
    constant ALMetalWorldAlphaEnvironment& environment)
{
    const float3 view = normalize(eye_position);
    const float eye_surface = -dot(
        view, environment.water_plane.xyz);
    const float eye_depth = max(-environment.water_plane.w, 0.0);
    const float3 intersection = environment.water_plane.w > 0.0
        ? view * environment.water_plane.w / eye_surface
        : float3(0.0);
    const float length_in_water = max(
        length(eye_position - intersection), 0.1);
    const float density = environment.water_fog.y;
    const float light_scale = environment.water_fog.x;
    constexpr float base = 0.98;
    const float term1 = -density * pow(base, light_scale * eye_depth);
    const float term2 = density + light_scale * eye_surface;
    const float term3 = pow(base, term2 * length_in_water) - 1.0;
    const float light = pow(min(term1 / term2 * term3, 1.0),
                            1.0 / 1.7);
    const float transmission = pow(base, length_in_water * density);
    return color * transmission +
           environment.water_fog_color.rgb * light;
}

static float3 almetal_world_alpha_fog(
    float3 eye_position, float3 color,
    constant ALMetalWorldAlphaEnvironment& environment)
{
    float3 additive;
    float3 attenuation;
    almetal_world_alpha_atmospherics(
        eye_position, environment, additive, attenuation);
    const bool object_above =
        dot(eye_position, environment.water_plane.xyz) +
            environment.water_plane.w > 0.0;
    return object_above
        ? almetal_world_alpha_atmospheric_fog(
              color, additive, attenuation, environment)
        : almetal_world_alpha_water_fog(
              eye_position, color, environment);
}

vertex ALMetalResampleVaryings almetal_world_atmospheric_haze_vertex(
    constant ALMetalWorldAtmosphericHazeConstants& constants [[buffer(0)]],
    uint vertex_id [[vertex_id]])
{
    const float2 texcoord = float2((vertex_id << 1u) & 2u,
                                   vertex_id & 2u);
    ALMetalResampleVaryings output;
    output.position = float4(
        texcoord.x * 2.0 - 1.0,
        1.0 - texcoord.y * 2.0,
        constants.config.x != 0u ? 0.0 : 1.0,
        1.0);
    output.texcoord = texcoord;
    return output;
}

fragment float4 almetal_world_atmospheric_haze_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    constant ALMetalWorldAtmosphericHazeConstants& constants [[buffer(0)]],
    constant ALMetalWorldAlphaEnvironment& environment [[buffer(1)]],
    texture2d<float> depth_texture [[texture(33)]],
    texture2d<float> sky_behind_map [[texture(34)]],
    sampler point_sampler [[sampler(1)]],
    sampler sky_sampler [[sampler(2)]])
{
    const float depth = depth_texture.sample(
        point_sampler, input.texcoord).r;
    const bool reverse_z = constants.config.x != 0u;
    const bool far_depth = reverse_z ? depth <= 0.0 : depth >= 1.0;
    const float ndc_z = reverse_z ? depth : depth * 2.0 - 1.0;
    float4 position = constants.inverse_projection *
        float4(input.texcoord * 2.0 - 1.0, ndc_z, 1.0);
    position.xyz /= max(position.w, 0.000001);

    const bool object_above_water =
        dot(position.xyz, environment.water_plane.xyz) +
            environment.water_plane.w > 0.0;
    if (far_depth ||
        (environment.water_plane.w <= 0.0 && !object_above_water))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 additive;
    float3 attenuation;
    almetal_world_alpha_atmospherics(
        position.xyz, environment, additive, attenuation);
    if (constants.config.y != 0u)
    {
        attenuation *= 1.0 - constants.parameters.x;
        const float3 sky = sky_behind_map.sample(
            sky_sampler, input.texcoord).rgb * 0.5;
        const float3 convergence = 1.0 - attenuation;
        additive = mix(
            additive, sky * convergence, convergence);
    }

    const float3 color = almetal_srgb_to_linear(additive * 2.0) *
        environment.atmospheric.w;
    return max(float4(color, attenuation.r), float4(0.0));
}

fragment float4 almetal_world_pre_water_fullbright_fragment(
    ALMetalWorldShinyVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant ALMetalWorldAlphaEnvironment& environment [[buffer(0)]])
{
    const float water_side =
        dot(input.eye_position, environment.water_plane.xyz) +
        environment.water_plane.w;
    if ((environment.water_fog.z > 0.0 && water_side < 0.0) ||
        (environment.water_fog.z <= 0.0 && water_side > 0.0))
    {
        discard_fragment();
    }

    float4 color = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord) * input.color
        : float4(1.0, 0.0, 1.0, 1.0);
    const float minimum_alpha = environment.water_fog.w;
    if (color.a < minimum_alpha)
    {
        discard_fragment();
    }
    color.rgb = almetal_world_alpha_fog(
        input.eye_position, color.rgb, environment);
    return max(color, float4(0.0));
}

struct ALMetalWorldLegacyGlowVaryings
{
    float4 position [[position]];
    float2 texcoord;
    float emissive_alpha;
    uint texture_index [[flat]];
    uint texture_count [[flat]];
};

static ALMetalWorldLegacyGlowVaryings almetal_world_legacy_glow_varyings(
    float4 clip_position,
    float4 packed_position,
    float2 texcoord,
    uchar4 emissive,
    constant ALMetalWorldSimpleConstants& constants)
{
    clip_position.y = -clip_position.y;
    ALMetalWorldLegacyGlowVaryings output;
    output.position = constants.depth_convention.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoord, 0.0, 1.0)).xy;
    output.emissive_alpha = float(emissive.a) / 255.0;
    output.texture_index = constants.depth_convention.y > 1u
        ? uint(max(as_type<int>(packed_position.w), 0)) : 0u;
    output.texture_count = constants.depth_convention.y;
    return output;
}

vertex ALMetalWorldLegacyGlowVaryings almetal_world_legacy_glow_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* emissive [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const float4 position = positions[vertex_id];
    return almetal_world_legacy_glow_varyings(
        constants.modelview_projection * float4(position.xyz, 1.0),
        position, texcoords[vertex_id], emissive[vertex_id], constants);
}

vertex ALMetalWorldLegacyGlowVaryings
almetal_world_legacy_glow_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* emissive [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    uint vertex_id [[vertex_id]])
{
    const float4 position = positions[vertex_id];
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        position, float4(0.0), encoded_weights[vertex_id],
        skin_palette, joint_count);
    return almetal_world_legacy_glow_varyings(
        constants.modelview_projection * skinned.position +
            constants.modelview_projection *
                float4(skin_palette[0].xyz, 1.0),
        position, texcoords[vertex_id], emissive[vertex_id], constants);
}

fragment float4 almetal_world_legacy_glow_fragment(
    ALMetalWorldLegacyGlowVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    const float alpha = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord).a * input.emissive_alpha
        : 0.0;
    return float4(0.0, 0.0, 0.0, max(alpha, 0.0));
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
    const float4 rebased_position =
        skinned.position + float4(skin_palette[0].xyz, 1.0);
    const float4 eye_position = constants.modelview * rebased_position;
    float4 clip_position =
        constants.modelview_projection * rebased_position;
    clip_position.y = -clip_position.y;

    ALMetalWorldSimpleVaryings output;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.depth_convention.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
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

static ALMetalWorldSimpleVaryings almetal_world_fullbright_varyings(
    float4 clip_position,
    float4 eye_position,
    float4 packed_position,
    float2 texcoord,
    uchar4 color,
    constant ALMetalWorldSimpleConstants& constants)
{
    clip_position.y = -clip_position.y;
    const float4 encoded_color = float4(color) / 255.0;
    const float2 transformed_texcoord =
        (constants.texture_matrix * float4(texcoord, 0.0, 1.0)).xy;

    ALMetalWorldSimpleVaryings output;
    output.position = constants.depth_convention.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.depth_convention.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
    // Fullbright never consumes a surface normal. Keeping this family on a
    // dedicated vertex contract mirrors the legal GL volume layout instead
    // of requiring a dummy normal stream solely because lit G-buffer draws
    // share the wider varying structure.
    output.normal = 0.0;
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = transformed_texcoord;
    output.texcoord1 = transformed_texcoord;
    output.texcoord2 = transformed_texcoord;
    output.tangent = 0.0;
    output.bitangent = 0.0;
    output.texture_index = constants.depth_convention.y > 1u
        ? uint(max(as_type<int>(packed_position.w), 0)) : 0u;
    output.texture_count = constants.depth_convention.y;
    output.alpha_cutoff = constants.material.x;
    output.material_flags = uint3(
        constants.material.y != 0.0,
        constants.material.z != 0.0,
        constants.material.w != 0.0);
    return output;
}

vertex ALMetalWorldSimpleVaryings almetal_world_fullbright_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const float4 position = positions[vertex_id];
    const float4 object_position = float4(position.xyz, 1.0);
    return almetal_world_fullbright_varyings(
        constants.modelview_projection * object_position,
        constants.modelview * object_position,
        position, texcoords[vertex_id], colors[vertex_id], constants);
}

vertex ALMetalWorldSimpleVaryings almetal_world_fullbright_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    uint vertex_id [[vertex_id]])
{
    const float4 position = positions[vertex_id];
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        position, float4(0.0), encoded_weights[vertex_id],
        skin_palette, joint_count);
    const float4 rebased_position =
        skinned.position + float4(skin_palette[0].xyz, 1.0);
    return almetal_world_fullbright_varyings(
        constants.modelview_projection * rebased_position,
        constants.modelview * rebased_position,
        position, texcoords[vertex_id], colors[vertex_id], constants);
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
    const float4 object_position =
        float4(positions[vertex_id].xyz, 1.0);
    const float4 eye_position = constants.modelview * object_position;
    float4 clip_position = constants.modelview_projection * object_position;
    clip_position.y = -clip_position.y;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.depth_convention.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;

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
    const float4 rebased_position =
        skinned.position + float4(skin_palette[0].xyz, 1.0);
    const float4 eye_position = constants.modelview * rebased_position;
    float4 clip_position =
        constants.modelview_projection * rebased_position;
    clip_position.y = -clip_position.y;

    ALMetalWorldSimpleVaryings output;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.depth_convention.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
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
    const float4 object_position =
        float4(positions[vertex_id].xyz, 1.0);
    const float4 eye_position = constants.modelview * object_position;
    float4 clip_position = constants.modelview_projection * object_position;
    clip_position.y = -clip_position.y;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.depth_convention.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;

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
    const float4 rebased_position =
        skinned.position + float4(skin_palette[0].xyz, 1.0);
    const float4 eye_position = constants.modelview * rebased_position;
    float4 clip_position =
        constants.modelview_projection * rebased_position;
    clip_position.y = -clip_position.y;

    ALMetalWorldSimpleVaryings output;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.depth_convention.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
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
    // x = roughness, y = metallic, z = alpha cutoff,
    // w = streamed base-colour alpha factor.
    float4 properties;
};

struct ALMetalWorldPBRConstants
{
    ALMetalWorldPBRSlotConstants slots[8];
    // x = populated slot count, y = alpha mask, z = HDR normal target.
    uint4 config;
    float4x4 modelview;
};

struct ALMetalWorldPBRVaryings
{
    float4 position [[position]];
    float mirror_clip_distance [[clip_distance]];
    float3 eye_position;
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

struct ALMetalWorldPBRGlowVaryings
{
    float4 position [[position]];
    float2 base_color_texcoord;
    float2 emissive_texcoord;
    float emissive_alpha;
    uint material_index [[flat]];
};

static ALMetalWorldPBRGlowVaryings almetal_world_pbr_glow_varyings(
    float4 clip_position,
    float4 packed_position,
    float2 texcoord,
    uchar4 emissive,
    constant ALMetalWorldSimpleConstants& world,
    constant ALMetalWorldPBRConstants& pbr)
{
    clip_position.y = -clip_position.y;
    const uint material_index = world.depth_convention.y > 1u
        ? uint(max(as_type<int>(packed_position.w), 0)) : 0u;
    const uint slot = min(
        material_index, min(max(pbr.config.x, 1u) - 1u, 7u));
    constant ALMetalWorldPBRSlotConstants& material = pbr.slots[slot];

    ALMetalWorldPBRGlowVaryings output;
    output.position = world.depth_convention.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.base_color_texcoord = almetal_world_pbr_texcoord(
        texcoord, world.texture_matrix, material.texture_transform[0]);
    output.emissive_texcoord = almetal_world_pbr_texcoord(
        texcoord, world.texture_matrix, material.texture_transform[3]);
    output.emissive_alpha = float(emissive.a) / 255.0;
    output.material_index = slot;
    return output;
}

vertex ALMetalWorldPBRGlowVaryings almetal_world_pbr_glow_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* emissive [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& world [[buffer(4)]],
    constant ALMetalWorldPBRConstants& pbr [[buffer(11)]],
    uint vertex_id [[vertex_id]])
{
    const float4 position = positions[vertex_id];
    return almetal_world_pbr_glow_varyings(
        world.modelview_projection * float4(position.xyz, 1.0),
        position, texcoords[vertex_id], emissive[vertex_id], world, pbr);
}

vertex ALMetalWorldPBRGlowVaryings
almetal_world_pbr_glow_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* emissive [[buffer(3)]],
    constant ALMetalWorldSimpleConstants& world [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    constant ALMetalWorldPBRConstants& pbr [[buffer(11)]],
    uint vertex_id [[vertex_id]])
{
    const float4 position = positions[vertex_id];
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        position, float4(0.0), encoded_weights[vertex_id],
        skin_palette, joint_count);
    return almetal_world_pbr_glow_varyings(
        world.modelview_projection * skinned.position +
            world.modelview_projection * float4(skin_palette[0].xyz, 1.0),
        position, texcoords[vertex_id], emissive[vertex_id], world, pbr);
}

fragment float4 almetal_world_pbr_glow_fragment(
    ALMetalWorldPBRGlowVaryings input [[stage_in]],
    array<texture2d<float>, 8> base_color_textures [[texture(0)]],
    array<texture2d<float>, 8> emissive_textures [[texture(8)]],
    sampler material_sampler [[sampler(0)]],
    constant ALMetalWorldPBRConstants& constants [[buffer(0)]])
{
    const uint material_index = min(input.material_index, 7u);
    const ALMetalWorldPBRSlotConstants material =
        constants.slots[material_index];
    const float coverage =
        base_color_textures[material_index].sample(
            material_sampler, input.base_color_texcoord).a *
        material.properties.w;
    const bool coverage_weights_glow = constants.config.y != 0u;
    const float quantization_bias = coverage_weights_glow
        ? 0.0 : 1.0 / 512.0;
    if (coverage < material.properties.z - quantization_bias)
    {
        discard_fragment();
    }
    const float3 emissive = material.emissive_color.rgb *
        emissive_textures[material_index].sample(
            material_sampler, input.emissive_texcoord).rgb;
    const float luminosity = max(max(emissive.r, emissive.g), emissive.b) *
                             input.emissive_alpha *
                             (coverage_weights_glow ? coverage : 1.0);
    return float4(0.0, 0.0, 0.0, max(luminosity, 0.0));
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
    float4 eye_position,
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
    output.mirror_clip_distance = world.depth_convention.z != 0u
        ? dot(eye_position, world.mirror_clip_plane) : 1.0;
    output.eye_position = eye_position.xyz;
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
        pbr.modelview * float4(positions[vertex_id].xyz, 1.0),
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
    const float4 rebased_position =
        skinned.position + float4(skin_palette[0].xyz, 1.0);
    return almetal_world_pbr_varyings(
        world.modelview_projection * rebased_position,
        pbr.modelview * rebased_position,
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

fragment float4 almetal_world_fullbright_fragment(
    ALMetalWorldSimpleVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    const float4 diffuse = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord)
        : float4(1.0, 0.0, 1.0, 1.0);
    if (input.material_flags.z != 0u &&
        diffuse.a < input.alpha_cutoff - (1.0 / 512.0))
    {
        discard_fragment();
    }

    // The post-deferred fullbright owners shade in linear and rely on the
    // sRGB target store. Outside the alpha pool their mask tests texture
    // coverage alone; streamed alpha carries shininess and is not a cutoff.
    return max(diffuse * input.color, float4(0.0));
}

vertex ALMetalWorldShinyVaryings almetal_world_shiny_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldShinyConstants& constants [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const float4 object_position = float4(positions[vertex_id].xyz, 1.0);
    float4 clip_position = constants.modelview_projection * object_position;
    clip_position.y = -clip_position.y;

    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;
    ALMetalWorldShinyVaryings output;
    output.position = constants.config.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    const float4 eye_position = constants.modelview * object_position;
    output.mirror_clip_distance = constants.config.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
    output.eye_position = eye_position.xyz;
    output.normal = normalize(normal_matrix * normals[vertex_id].xyz);
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texture_index = constants.config.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    output.texture_count = constants.config.y;
    return output;
}

vertex ALMetalWorldShinyVaryings almetal_world_shiny_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldShinyConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], normals[vertex_id],
        encoded_weights[vertex_id], skin_palette, joint_count);
    const float4 origin = float4(skin_palette[0].xyz, 1.0);
    float4 clip_position =
        constants.modelview_projection * skinned.position +
        constants.modelview_projection * origin;
    clip_position.y = -clip_position.y;

    const float3x3 normal_matrix = float3x3(
        constants.rigged_normal_matrix[0].xyz,
        constants.rigged_normal_matrix[1].xyz,
        constants.rigged_normal_matrix[2].xyz);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;
    ALMetalWorldShinyVaryings output;
    output.position = constants.config.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    const float4 eye_position = constants.modelview * skinned.position +
                                constants.modelview * origin;
    output.mirror_clip_distance = constants.config.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
    output.eye_position = eye_position.xyz;
    output.normal = normalize(normal_matrix * skinned.normal.xyz);
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texture_index = constants.config.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    output.texture_count = constants.config.y;
    return output;
}

vertex ALMetalWorldLegacyAlphaVaryings almetal_world_legacy_alpha_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldShinyConstants& constants [[buffer(4)]],
    const device float4* tangents [[buffer(8)]],
    const device float2* texcoords1 [[buffer(9)]],
    const device float2* texcoords2 [[buffer(10)]],
    uint vertex_id [[vertex_id]])
{
    const float4 object_position = float4(positions[vertex_id].xyz, 1.0);
    float4 clip_position = constants.modelview_projection * object_position;
    clip_position.y = -clip_position.y;
    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    const float3 normal = normalize(
        normal_matrix * normals[vertex_id].xyz);
    const float3 tangent = normalize(
        normal_matrix * tangents[vertex_id].xyz);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;

    ALMetalWorldLegacyAlphaVaryings output;
    output.position = constants.config.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    const float4 eye_position = constants.modelview * object_position;
    output.mirror_clip_distance = constants.config.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
    output.eye_position = eye_position.xyz;
    output.normal = normal;
    output.tangent = tangent;
    output.bitangent = tangents[vertex_id].w * cross(normal, tangent);
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = (constants.texture_matrix *
                        float4(texcoords1[vertex_id], 0.0, 1.0)).xy;
    output.texcoord2 = (constants.texture_matrix *
                        float4(texcoords2[vertex_id], 0.0, 1.0)).xy;
    output.material_index = constants.config.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    return output;
}

vertex ALMetalWorldLegacyAlphaVaryings
almetal_world_legacy_alpha_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    const device uchar4* colors [[buffer(3)]],
    constant ALMetalWorldShinyConstants& constants [[buffer(4)]],
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
    const float4 origin = float4(skin_palette[0].xyz, 1.0);
    float4 clip_position =
        constants.modelview_projection * skinned.position +
        constants.modelview_projection * origin;
    clip_position.y = -clip_position.y;
    const float3x3 normal_matrix = float3x3(
        constants.rigged_normal_matrix[0].xyz,
        constants.rigged_normal_matrix[1].xyz,
        constants.rigged_normal_matrix[2].xyz);
    const float3 normal = normalize(normal_matrix * skinned.normal.xyz);
    const float3 skinned_tangent = almetal_object_skin_direction(
        tangents[vertex_id].xyz, encoded_weights[vertex_id],
        skin_palette, joint_count);
    const float3 tangent = normalize(normal_matrix * skinned_tangent);
    const float4 encoded_color = float4(colors[vertex_id]) / 255.0;

    ALMetalWorldLegacyAlphaVaryings output;
    output.position = constants.config.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    const float4 eye_position = constants.modelview * skinned.position +
                                constants.modelview * origin;
    output.mirror_clip_distance = constants.config.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
    output.eye_position = eye_position.xyz;
    output.normal = normal;
    output.tangent = tangent;
    output.bitangent = tangents[vertex_id].w * cross(normal, tangent);
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = (constants.texture_matrix *
                        float4(texcoords1[vertex_id], 0.0, 1.0)).xy;
    output.texcoord2 = (constants.texture_matrix *
                        float4(texcoords2[vertex_id], 0.0, 1.0)).xy;
    output.material_index = constants.config.y > 1
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0;
    return output;
}

static bool almetal_probe_holds(
    int index, float3 position, bool automatic,
    thread bool& boxed, thread float3& box_position,
    constant ALMetalWorldReflectionProbeConstants& probes)
{
    boxed = false;
    box_position = 0.0;
    if (probes.ref_index[index].w < 0)
    {
        const float4 local = probes.ref_box[index] * float4(position, 1.0);
        if (any(abs(local.xyz) > float3(1.0)))
        {
            return false;
        }
        boxed = true;
        box_position = local.xyz;
        return true;
    }
    if (probes.ref_index[index].w == 0 && !automatic)
    {
        return false;
    }
    const float3 delta = position - probes.ref_sphere[index].xyz;
    const float radius = probes.ref_sphere[index].w;
    return dot(delta, delta) <= radius * radius;
}

static int almetal_probe_start_index(
    float3 position, int probe_count,
    constant ALMetalWorldReflectionProbeConstants& probes)
{
    const int bucket = clamp(int(floor(-position.z)), 0, 255);
    return clamp(probes.ref_bucket[bucket].x, 1, probe_count + 1);
}

static float3 almetal_probe_sphere_intersect(
    float3 origin, float3 direction, float3 center, float radius_squared)
{
    const float3 to_center = center - origin;
    if (dot(to_center, to_center) > radius_squared)
    {
        return center + direction;
    }
    const float nearest = dot(to_center, direction);
    const float perpendicular_squared =
        dot(to_center, to_center) - nearest * nearest;
    const float half_chord =
        sqrt(max(radius_squared - perpendicular_squared, 0.0));
    return origin + direction * (nearest + half_chord);
}

static float3 almetal_probe_box_intersect_from(
    float3 origin, float3 box_position, float3 direction,
    float4x4 clip_to_local, float scale)
{
    const float3 local_ray = float3x3(
        clip_to_local[0].xyz,
        clip_to_local[1].xyz,
        clip_to_local[2].xyz) * direction;
    const float3 ray_sign = float3(
        local_ray.x < 0.0 ? -1.0 : 1.0,
        local_ray.y < 0.0 ? -1.0 : 1.0,
        local_ray.z < 0.0 ? -1.0 : 1.0);
    const float3 denominator =
        ray_sign * max(abs(local_ray), float3(1e-6));
    const float3 unitary = float3(scale);
    const float3 first = (unitary - box_position) / denominator;
    const float3 second = (-unitary - box_position) / denominator;
    const float3 furthest = max(first, second);
    const float distance = min(furthest.x, min(furthest.y, furthest.z));
    return origin + direction * distance;
}

static float almetal_probe_box_depth(float3 box_position)
{
    return 1.0 - max(max(abs(box_position.x), abs(box_position.y)),
                     abs(box_position.z));
}

static void almetal_probe_debug_add(
    thread float4& color, float4 contribution)
{
    color += contribution * (1.0 - min(color.a, 1.0));
}

static void almetal_probe_sphere_intersect_debug(
    float3 direction, float3 center, float radius_squared,
    float scene_depth, thread float4& color)
{
    const float projection = dot(center, direction);
    const float perpendicular_squared =
        dot(center, center) - projection * projection;
    if (perpendicular_squared > radius_squared)
    {
        return;
    }
    const float half_chord = sqrt(max(
        radius_squared - perpendicular_squared, 0.0));
    const float intersections[2] = {
        projection - half_chord,
        projection + half_chord,
    };
    for (uint intersection = 0u; intersection < 2u; ++intersection)
    {
        const float distance = intersections[intersection];
        if (distance <= 0.0)
        {
            continue;
        }
        if (distance > scene_depth)
        {
            const float weight =
                0.125 / ((distance - scene_depth) * 0.125 + 1.0);
            almetal_probe_debug_add(
                color, float4(0.0, 0.0, weight, weight));
        }
        else
        {
            constexpr float weight = 0.25;
            almetal_probe_debug_add(
                color, float4(weight, weight, 0.0, weight));
        }
    }
}

static void almetal_probe_box_intersection_color_debug(
    float3 origin, float3 direction, float distance,
    float3 scene_position, thread float4& color)
{
    const float3 intersection = direction * distance;
    const float3 position = scene_position - origin;
    float weight = 0.25;
    if (dot(intersection, intersection) > dot(position, position))
    {
        weight *= 0.5;
        weight /=
            (length(intersection) - length(position)) * 0.5 + 1.0;
        almetal_probe_debug_add(
            color, float4(0.0, 0.0, weight, weight));
    }
    else
    {
        almetal_probe_debug_add(
            color, float4(weight, weight, 0.0, weight));
    }
}

static void almetal_probe_box_intersect_debug(
    float3 scene_position, float4x4 clip_to_local,
    thread float4& color)
{
    const float3 origin = (clip_to_local * float4(0.0, 0.0, 0.0, 1.0)).xyz;
    const float3 position =
        (clip_to_local * float4(scene_position, 1.0)).xyz;
    const float3 delta = position - origin;
    if (dot(delta, delta) <= 0.0)
    {
        return;
    }
    const float3 direction = normalize(delta);
    const float3 reciprocal = 1.0 / direction;
    const float3 scaled_origin = reciprocal * origin;
    const float3 extent = abs(reciprocal);
    const float3 near_planes = -scaled_origin - extent;
    const float3 far_planes = -scaled_origin + extent;
    const float near_distance = max(
        max(near_planes.x, near_planes.y), near_planes.z);
    const float far_distance = min(
        min(far_planes.x, far_planes.y), far_planes.z);
    if (near_distance > far_distance || far_distance < 0.0)
    {
        return;
    }

    const float first_distance = near_distance < 0.0
        ? far_distance : near_distance;
    almetal_probe_box_intersection_color_debug(
        origin, direction, first_distance, position, color);
    if (near_distance > 0.0)
    {
        almetal_probe_box_intersection_color_debug(
            origin, direction, far_distance, position, color);
    }
}

fragment float4 almetal_world_reflection_probe_volume_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> depth_texture [[texture(0)]],
    sampler depth_sampler [[sampler(0)]],
    constant ALMetalWorldReflectionProbeVolumeConstants& constants
        [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]])
{
    const float depth = depth_texture.sample(
        depth_sampler, input.texcoord, level(0.0)).r;
    const float ndc_z = constants.config.x != 0u
        ? depth : depth * 2.0 - 1.0;
    float4 position = constants.inverse_projection *
        float4(input.texcoord * 2.0 - 1.0, ndc_z, 1.0);
    position.xyz /= max(position.w, 0.000001);

    const float scene_depth = length(position.xyz);
    if (scene_depth <= 0.0)
    {
        return 0.0;
    }
    const float3 direction = position.xyz / scene_depth;
    float4 color = 0.0;
    const int probe_count = int(probes.config.x);
    for (int index = 1; index < probe_count; ++index)
    {
        const int shape = probes.ref_index[index].w;
        if (shape < 0)
        {
            almetal_probe_box_intersect_debug(
                position.xyz, probes.ref_box[index], color);
        }
        else if (shape > 0)
        {
            const float radius = probes.ref_sphere[index].w;
            almetal_probe_sphere_intersect_debug(
                direction, probes.ref_sphere[index].xyz,
                radius * radius, scene_depth, color);
        }
    }
    return max(color, float4(0.0));
}

static float almetal_probe_weight(
    int index, float3 position, float3 box_position,
    thread float& distance_weight,
    constant ALMetalWorldReflectionProbeConstants& probes)
{
    if (probes.ref_index[index].w < 0)
    {
        const float depth = almetal_probe_box_depth(box_position);
        distance_weight = clamp(depth, 0.0, 1.0) *
                          probes.ref_params[index].z;
        return max(depth, 0.001);
    }

    const float radius = probes.ref_sphere[index].w;
    const float inner_radius = radius * 0.5;
    const float distance = max(
        length(position - probes.ref_sphere[index].xyz), 0.001);
    const float attenuation = 1.0 -
        max(distance - inner_radius, 0.0) /
        max(radius - inner_radius, 0.001);
    float weight = probes.ref_params[index].z / distance;
    distance_weight = weight * attenuation * max(radius, 1.0) * 4.0;
    weight *= attenuation;
    return index == 0 ? max(weight, 1e-6) : weight;
}

static float3 almetal_probe_radiance(
    int index, float3 position, float3 box_position, float3 direction,
    float lod, bool water,
    texturecube_array<float> radiance, sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes)
{
    const float3 center = probes.ref_sphere[index].xyz;
    float3 sample_position;
    if (probes.ref_index[index].w < 0)
    {
        sample_position = almetal_probe_box_intersect_from(
            position, box_position, direction, probes.ref_box[index], 1.0);
    }
    else
    {
        if (water && index == 0)
        {
            // reflectionProbeF.glsl keeps water's default sky probe on the
            // reflected ray. Its camera-relative centre must not introduce
            // sphere parallax into the horizon/environment direction.
            sample_position = center + direction;
        }
        else
        {
            const float radius = probes.ref_sphere[index].w;
            const float radius_squared = probes.ref_index[index].w < 1
                ? 4096.0 * 4096.0 : radius * radius;
            sample_position = almetal_probe_sphere_intersect(
                position, direction, center, radius_squared);
        }
    }
    const float3x3 environment = float3x3(
        probes.env_matrix[0].xyz,
        probes.env_matrix[1].xyz,
        probes.env_matrix[2].xyz);
    const float3 sample_direction =
        environment * (sample_position - center);
    return radiance.sample(
        probe_sampler, sample_direction,
        uint(probes.ref_index[index].x), level(lod)).rgb *
        probes.ref_params[index].y;
}

struct ALMetalLegacyProbeSums
{
    float3 automatic_color;
    float3 manual_color;
    float automatic_weight;
    float manual_weight;
    float automatic_distance_weight;
    float manual_distance_weight;
};

static void almetal_legacy_probe_tap(
    int index, float3 position, float3 box_position, float3 direction,
    texturecube_array<float> radiance, sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes,
    thread ALMetalLegacyProbeSums& sums)
{
    float distance_weight = 0.0;
    const float weight = almetal_probe_weight(
        index, position, box_position, distance_weight, probes);
    const float3 color = almetal_probe_radiance(
        index, position, box_position, direction,
        0.0, false, radiance, probe_sampler, probes);
    if (clamp(abs(probes.ref_index[index].w), 0, 1) == 0)
    {
        sums.automatic_color += color * weight;
        sums.automatic_weight += weight;
        sums.automatic_distance_weight += distance_weight;
    }
    else
    {
        sums.manual_color += color * weight;
        sums.manual_weight += weight;
        sums.manual_distance_weight += distance_weight;
    }
}

static float3 almetal_legacy_probe_mix(
    ALMetalLegacyProbeSums sums, bool automatic)
{
    float3 automatic_color = automatic
        ? sums.automatic_color : float3(0.0);
    float3 manual_color = sums.manual_color;
    if (automatic && sums.automatic_weight > 0.0)
    {
        automatic_color /= sums.automatic_weight;
        if (sums.manual_weight > 0.0)
        {
            manual_color /= sums.manual_weight;
            manual_color = mix(
                automatic_color, manual_color,
                min(sums.manual_distance_weight, 1.0));
            automatic_color = 0.0;
        }
    }
    else if (sums.manual_weight > 0.0)
    {
        manual_color /= sums.manual_weight;
        automatic_color = 0.0;
    }
    return automatic_color + manual_color;
}

static float3 almetal_sample_legacy_probes(
    float3 position, float3 normal,
    texturecube_array<float> radiance, sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes)
{
    ALMetalLegacyProbeSums sums{};
    const int probe_count = clamp(int(probes.config.x), 1, 256);
    const float3 reflection_direction = normalize(reflect(position, normal));
    bool automatic = true;
    int accepted_count = 0;
    int index = almetal_probe_start_index(position, probe_count, probes);
    int neighbor = -1;
    int neighbor_base = 0;
    int neighbor_count = 0;
    while (accepted_count < 32)
    {
        int candidate;
        if (neighbor < 0)
        {
            if (index >= probe_count ||
                probes.ref_params[index].w > -position.z)
            {
                break;
            }
            candidate = index++;
        }
        else
        {
            if (neighbor >= neighbor_count)
            {
                break;
            }
            const int vector_index = neighbor_base + (neighbor >> 2);
            if (vector_index < 0 || vector_index >= 1024)
            {
                break;
            }
            candidate = probes.ref_neighbor[vector_index][neighbor & 3];
            ++neighbor;
            if (candidate < 0 || candidate >= probe_count)
            {
                continue;
            }
        }

        bool boxed = false;
        float3 box_position = 0.0;
        if (!almetal_probe_holds(
                candidate, position, automatic,
                boxed, box_position, probes))
        {
            continue;
        }
        if (boxed)
        {
            automatic = false;
        }
        almetal_legacy_probe_tap(
            candidate, position, box_position, reflection_direction,
            radiance, probe_sampler, probes, sums);
        ++accepted_count;

        if (neighbor < 0 && probes.ref_index[candidate].y != -1)
        {
            neighbor_base = probes.ref_index[candidate].y;
            neighbor_count = clamp(probes.ref_index[candidate].z, 0, 31);
            neighbor = 0;
        }
    }

    if (automatic && accepted_count < 32)
    {
        const float3 default_box_position = probes.ref_index[0].w < 0
            ? (probes.ref_box[0] * float4(position, 1.0)).xyz
            : float3(0.0);
        almetal_legacy_probe_tap(
            0, position, default_box_position, reflection_direction,
            radiance, probe_sampler, probes, sums);
    }
    return almetal_legacy_probe_mix(sums, automatic);
}

static float3 almetal_probe_sh_irradiance(
    int index, float3 position, float3 box_position, float3 direction,
    float3 ambient, texture2d<uint> irradiance,
    constant ALMetalWorldReflectionProbeConstants& probes)
{
    const float3 center = probes.ref_sphere[index].xyz;
    float3 sample_position;
    if (probes.ref_index[index].w < 0)
    {
        sample_position = almetal_probe_box_intersect_from(
            position, box_position, direction,
            probes.ref_box[index], 3.0);
    }
    else
    {
        const float radius = probes.ref_sphere[index].w;
        const float radius_squared = probes.ref_index[index].w < 1
            ? 4096.0 * 4096.0 : radius * radius;
        sample_position = almetal_probe_sphere_intersect(
            position, direction, center, radius_squared);
    }
    const float3x3 environment = float3x3(
        probes.env_matrix[0].xyz,
        probes.env_matrix[1].xyz,
        probes.env_matrix[2].xyz);
    float3 sample_direction = environment * (sample_position - center);
    const float direction_length_squared =
        dot(sample_direction, sample_direction);
    sample_direction = direction_length_squared > 1.0e-12
        ? sample_direction * rsqrt(direction_length_squared)
        : normalize(environment * direction);

    const uint layer = uint(probes.ref_index[index].x);
    const uint4 t0 = irradiance.read(uint2(0, layer));
    const uint4 t1 = irradiance.read(uint2(1, layer));
    const uint4 t2 = irradiance.read(uint2(2, layer));
    const uint4 t3 = irradiance.read(uint2(3, layer));
    const float2 h0 = float2(as_type<half2>(t0.x));
    const float2 h1 = float2(as_type<half2>(t0.y));
    const float2 h2 = float2(as_type<half2>(t0.z));
    const float2 h3 = float2(as_type<half2>(t0.w));
    const float2 h4 = float2(as_type<half2>(t1.x));
    const float2 h5 = float2(as_type<half2>(t1.y));
    const float2 h6 = float2(as_type<half2>(t1.z));
    const float2 h7 = float2(as_type<half2>(t1.w));
    const float2 h8 = float2(as_type<half2>(t2.x));
    const float2 h9 = float2(as_type<half2>(t2.y));
    const float2 h10 = float2(as_type<half2>(t2.z));
    const float2 h11 = float2(as_type<half2>(t2.w));
    const float2 h12 = float2(as_type<half2>(t3.x));
    const float2 h13 = float2(as_type<half2>(t3.y));
    const float3 l0 = float3(h0, h1.x);
    const float3 l1 = float3(h1.y, h2);
    const float3 l2 = float3(h3, h4.x);
    const float3 l3 = float3(h4.y, h5);
    const float3 l4 = float3(h6, h7.x);
    const float3 l5 = float3(h7.y, h8);
    const float3 l6 = float3(h9, h10.x);
    const float3 l7 = float3(h10.y, h11);
    const float3 l8 = float3(h12, h13.x);
    float3 value =
        0.282095 * l0 +
        0.6666667 * 0.488603 *
            (sample_direction.y * l1 + sample_direction.z * l2 +
             sample_direction.x * l3) +
        0.25 *
            (1.092548 *
                 (sample_direction.x * sample_direction.y * l4 +
                  sample_direction.y * sample_direction.z * l5 +
                  sample_direction.x * sample_direction.z * l7) +
             0.315392 *
                 (3.0 * sample_direction.z * sample_direction.z - 1.0) * l6 +
             0.546274 *
                 (sample_direction.x * sample_direction.x -
                  sample_direction.y * sample_direction.y) * l8);
    value = max(value, float3(0.0));
    value *= max(probes.ref_params[index].x, 1.0);
    return mix(ambient, value, min(probes.ref_params[index].x, 1.0));
}

struct ALMetalBaseProbeSums
{
    float3 ambient[2];
    float3 gloss[2];
    float3 legacy[2];
    float weight[2];
    float distance_weight[2];
};

static void almetal_base_probe_tap(
    int index, float3 position, float3 box_position,
    float3 normal, float3 ambient, bool sample_ambient,
    float3 gloss_direction, float gloss_lod, bool sample_gloss,
    float3 legacy_direction, bool sample_legacy,
    texturecube_array<float> radiance, texture2d<uint> irradiance,
    sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes,
    thread ALMetalBaseProbeSums& sums)
{
    float distance_weight = 0.0;
    const float weight = almetal_probe_weight(
        index, position, box_position, distance_weight, probes);
    const uint family =
        clamp(abs(probes.ref_index[index].w), 0, 1) == 0 ? 0u : 1u;
    if (sample_ambient)
    {
        sums.ambient[family] += almetal_probe_sh_irradiance(
            index, position, box_position, normal, ambient,
            irradiance, probes) * weight;
    }
    if (sample_gloss)
    {
        sums.gloss[family] += almetal_probe_radiance(
            index, position, box_position, gloss_direction, gloss_lod,
            false, radiance, probe_sampler, probes) * weight;
    }
    if (sample_legacy)
    {
        sums.legacy[family] += almetal_probe_radiance(
            index, position, box_position, legacy_direction, 0.0,
            false, radiance, probe_sampler, probes) * weight;
    }
    sums.weight[family] += weight;
    sums.distance_weight[family] += distance_weight;
}

static float3 almetal_base_probe_mix(
    float3 automatic_color, float3 manual_color,
    ALMetalBaseProbeSums sums, bool automatic)
{
    if (!automatic)
    {
        automatic_color = 0.0;
    }
    if (automatic && sums.weight[0] > 0.0)
    {
        automatic_color /= sums.weight[0];
        if (sums.weight[1] > 0.0)
        {
            manual_color /= sums.weight[1];
            manual_color = mix(
                automatic_color, manual_color,
                min(sums.distance_weight[1], 1.0));
            automatic_color = 0.0;
        }
    }
    else if (sums.weight[1] > 0.0)
    {
        manual_color /= sums.weight[1];
        automatic_color = 0.0;
    }
    return automatic_color + manual_color;
}

static float3 almetal_base_dominant_direction(
    float3 normal, float3 reflection, float perceptual_roughness)
{
    const float smoothness = 1.0 - perceptual_roughness;
    const float factor = smoothness *
        (sqrt(smoothness) + perceptual_roughness);
    return normalize(mix(normal, reflection, factor));
}

static void almetal_sample_base_probes(
    bool pbr, float3 position, float3 normal, float glossiness,
    float environment_intensity, float3 ambient, bool classic_mode,
    texturecube_array<float> radiance, texture2d<uint> irradiance,
    sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes,
    thread float3& ambient_environment,
    thread float3& gloss_environment,
    thread float3& legacy_environment)
{
    const float3 reflection = normalize(reflect(position, normal));
    const float perceptual_roughness = 1.0 - glossiness;
    const float3 gloss_direction = almetal_base_dominant_direction(
        normal, reflection, perceptual_roughness);
    const bool sample_ambient = !classic_mode;
    const bool sample_gloss = pbr || glossiness > 0.0;
    const bool sample_legacy = !pbr && environment_intensity > 0.0;
    const float gloss_lod = perceptual_roughness * probes.config.y;
    ALMetalBaseProbeSums sums{};
    bool automatic = true;
    int accepted_count = 0;
    const int probe_count = clamp(int(probes.config.x), 1, 256);
    int index = almetal_probe_start_index(position, probe_count, probes);
    int neighbor = -1;
    int neighbor_base = 0;
    int neighbor_count = 0;
    while (accepted_count < 32)
    {
        int candidate;
        if (neighbor < 0)
        {
            if (index >= probe_count ||
                probes.ref_params[index].w > -position.z)
            {
                break;
            }
            candidate = index++;
        }
        else
        {
            if (neighbor >= neighbor_count)
            {
                break;
            }
            const int vector_index = neighbor_base + (neighbor >> 2);
            if (vector_index < 0 || vector_index >= 1024)
            {
                break;
            }
            candidate = probes.ref_neighbor[vector_index][neighbor & 3];
            ++neighbor;
            if (candidate < 0 || candidate >= probe_count)
            {
                continue;
            }
        }
        bool boxed = false;
        float3 box_position = 0.0;
        if (!almetal_probe_holds(
                candidate, position, automatic,
                boxed, box_position, probes))
        {
            continue;
        }
        if (boxed)
        {
            automatic = false;
        }
        almetal_base_probe_tap(
            candidate, position, box_position, normal, ambient,
            sample_ambient, gloss_direction, gloss_lod, sample_gloss,
            reflection, sample_legacy, radiance, irradiance,
            probe_sampler, probes, sums);
        ++accepted_count;
        if (neighbor < 0 && probes.ref_index[candidate].y != -1)
        {
            neighbor_base = probes.ref_index[candidate].y;
            neighbor_count = clamp(
                probes.ref_index[candidate].z, 0, 31);
            neighbor = 0;
        }
    }

    if (automatic && accepted_count < 32)
    {
        const float3 box_position = probes.ref_index[0].w < 0
            ? (probes.ref_box[0] * float4(position, 1.0)).xyz
            : float3(0.0);
        almetal_base_probe_tap(
            0, position, box_position, normal, ambient,
            sample_ambient, gloss_direction, gloss_lod, sample_gloss,
            reflection, sample_legacy, radiance, irradiance,
            probe_sampler, probes, sums);
    }
    ambient_environment = sample_ambient
        ? almetal_base_probe_mix(
              sums.ambient[0], sums.ambient[1], sums, automatic)
        : ambient;
    gloss_environment = sample_gloss
        ? almetal_base_probe_mix(
              sums.gloss[0], sums.gloss[1], sums, automatic)
        : float3(0.0);
    legacy_environment = sample_legacy
        ? almetal_base_probe_mix(
              sums.legacy[0], sums.legacy[1], sums, automatic)
        : float3(0.0);
}

static float3 almetal_finish_hero_probe(
    float3 environment_color, float3 position, float3 normal,
    float glossiness,
    texturecube<float> hero_radiance, sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes,
    constant ALMetalWorldHeroProbeConstants& hero)
{
    if (hero.config.y < 1u || glossiness <= 0.75)
    {
        return environment_color;
    }

    float weight;
    const float3 reflected = reflect(position, normal);
    if (hero.config.x < 1u)
    {
        const float3 box_position =
            (hero.hero_box * float4(position, 1.0)).xyz;
        weight = max(almetal_probe_box_depth(box_position), 0.0);
    }
    else
    {
        const float radius = hero.hero_sphere.w;
        const float inner_radius = radius * 0.5;
        const float distance = max(
            length(position - hero.hero_sphere.xyz), 0.001);
        const float attenuation = 1.0 -
            max(distance - inner_radius, 0.0) /
            max(radius - inner_radius, 0.001);
        weight = attenuation / distance;
    }

    float clip_distance =
        dot(position, hero.clip_plane.xyz) + hero.clip_plane.w;
    clip_distance = clamp((clip_distance * 0.95 + 0.05) * 10.0,
                          0.0, 1.0);
    weight = clamp(weight * 10.0 * clip_distance, 0.0, 1.0);
    weight *= clamp(glossiness - 0.75, 0.0, 1.0) * 4.0;
    if (weight <= 0.0)
    {
        return environment_color;
    }

    const float3x3 environment = float3x3(
        probes.env_matrix[0].xyz,
        probes.env_matrix[1].xyz,
        probes.env_matrix[2].xyz);
    const float3 hero_environment = hero_radiance.sample(
        probe_sampler, environment * reflected,
        level((1.0 - glossiness) * float(hero.config.z))).rgb;
    return mix(environment_color, hero_environment, weight);
}

constant float3 almetal_ssr_poisson[128] = {
    float3(0.5433144, 0.1122154, 0.2501391),
    float3(0.6575254, 0.7214090, 0.1628600),
    float3(0.02888453, 0.05170321, 0.7573566),
    float3(0.06635678, 0.8286457, 0.07157445),
    float3(0.8957489, 0.4005505, 0.7916042),
    float3(0.3423355, 0.5053263, 0.9193521),
    float3(0.9694794, 0.9461077, 0.5406441),
    float3(0.9975473, 0.02789414, 0.7320132),
    float3(0.07781899, 0.3862341, 0.9185940),
    float3(0.4439073, 0.9686955, 0.4055861),
    float3(0.9657035, 0.6624081, 0.7082613),
    float3(0.7712346, 0.07273269, 0.3292839),
    float3(0.2489169, 0.2550394, 0.1950516),
    float3(0.7249326, 0.9328285, 0.3352458),
    float3(0.6028461, 0.4424961, 0.5393377),
    float3(0.2879795, 0.7427881, 0.6619173),
    float3(0.3193627, 0.0486145, 0.08109283),
    float3(0.1233155, 0.6026410, 0.4378719),
    float3(0.9800708, 0.2117290, 0.6771586),
    float3(0.4894537, 0.3319927, 0.8087631),
    float3(0.4802743, 0.6358885, 0.8149350),
    float3(0.2692913, 0.9911493, 0.9934899),
    float3(0.5648789, 0.8553897, 0.7784553),
    float3(0.8497344, 0.7870212, 0.02065313),
    float3(0.7503014, 0.2826185, 0.05412734),
    float3(0.8045461, 0.6167251, 0.9532926),
    float3(0.04225039, 0.2141281, 0.8678675),
    float3(0.07116079, 0.9971236, 0.3396397),
    float3(0.4640990, 0.4809590, 0.2775862),
    float3(0.6346927, 0.3187100, 0.6588384),
    float3(0.4490120, 0.8189669, 0.2736875),
    float3(0.4529290, 0.2119148, 0.6720040),
    float3(0.01506042, 0.7102436, 0.9800494),
    float3(0.1970513, 0.4713539, 0.4644522),
    float3(0.1371500, 0.7253224, 0.5056525),
    float3(0.9006432, 0.5335414, 0.02206874),
    float3(0.9960898, 0.7961011, 0.01468861),
    float3(0.3386469, 0.6337739, 0.9310676),
    float3(0.1745718, 0.9114985, 0.1728188),
    float3(0.6342545, 0.5721557, 0.4553517),
    float3(0.1347412, 0.1137158, 0.7793725),
    float3(0.3574478, 0.3448052, 0.08741581),
    float3(0.7283059, 0.4753885, 0.2240275),
    float3(0.8293507, 0.9971212, 0.2747005),
    float3(0.6501846, 0.000688076, 0.7795712),
    float3(0.01149416, 0.4930083, 0.7926080),
    float3(0.6661890, 0.1875442, 0.7256873),
    float3(0.8538797, 0.2107637, 0.1547532),
    float3(0.5826825, 0.9750752, 0.9105834),
    float3(0.8914346, 0.08266425, 0.5484225),
    float3(0.4374518, 0.02987111, 0.7810078),
    float3(0.2287418, 0.1443802, 0.1176908),
    float3(0.2671157, 0.8929081, 0.8989366),
    float3(0.5425819, 0.5524959, 0.6963879),
    float3(0.3515188, 0.8304397, 0.0502702),
    float3(0.3354864, 0.2130747, 0.1411690),
    float3(0.9729427, 0.3509927, 0.6098799),
    float3(0.7585629, 0.7115368, 0.9099342),
    float3(0.0140543, 0.6072157, 0.9436461),
    float3(0.9190664, 0.8497264, 0.1643751),
    float3(0.1538157, 0.3219983, 0.2984214),
    float3(0.8854713, 0.2968667, 0.8511457),
    float3(0.1910622, 0.03047311, 0.3571215),
    float3(0.2456353, 0.5568692, 0.3530164),
    float3(0.6927255, 0.8073994, 0.5808484),
    float3(0.8089353, 0.8969175, 0.3427134),
    float3(0.1944770, 0.7985603, 0.8712182),
    float3(0.7256182, 0.5653068, 0.3985921),
    float3(0.9889427, 0.4584851, 0.8363391),
    float3(0.5718582, 0.2127113, 0.2950557),
    float3(0.5480209, 0.0193435, 0.2992659),
    float3(0.6598953, 0.09478426, 0.9218700),
    float3(0.1385615, 0.2193868, 0.2052450),
    float3(0.7623423, 0.1790726, 0.1508465),
    float3(0.7569032, 0.3773386, 0.4393887),
    float3(0.5842971, 0.6538072, 0.5224424),
    float3(0.9954313, 0.5763943, 0.9169143),
    float3(0.001311183, 0.3403630, 0.1488652),
    float3(0.8167927, 0.4947158, 0.4454727),
    float3(0.3978434, 0.7106082, 0.002727509),
    float3(0.5459411, 0.7473233, 0.7062873),
    float3(0.4151598, 0.5614617, 0.4748358),
    float3(0.4440694, 0.1195122, 0.9624678),
    float3(0.1081301, 0.4813806, 0.07047641),
    float3(0.2402785, 0.3633997, 0.3898734),
    float3(0.2317942, 0.6488295, 0.4221864),
    float3(0.01145542, 0.9304277, 0.4105759),
    float3(0.3563728, 0.9228861, 0.3282344),
    float3(0.8553140, 0.6949819, 0.3175117),
    float3(0.7308320, 0.01478493, 0.5728671),
    float3(0.9304829, 0.02653277, 0.7125520),
    float3(0.4132186, 0.4127623, 0.6084146),
    float3(0.7517329, 0.9978395, 0.1330464),
    float3(0.5210338, 0.4318751, 0.9721575),
    float3(0.02953994, 0.1375937, 0.9458942),
    float3(0.1835506, 0.9896691, 0.7919457),
    float3(0.3857062, 0.2682322, 0.1264563),
    float3(0.6319699, 0.8735335, 0.04390657),
    float3(0.5630485, 0.3339024, 0.9939950),
    float3(0.9070100, 0.1512893, 0.8970422),
    float3(0.3027443, 0.1144253, 0.1488708),
    float3(0.9149003, 0.7382028, 0.7914025),
    float3(0.07979286, 0.6892691, 0.2866171),
    float3(0.7743186, 0.8046008, 0.4399814),
    float3(0.3128662, 0.4362317, 0.6030678),
    float3(0.1133721, 0.01605821, 0.3918720),
    float3(0.5185481, 0.9210006, 0.7889017),
    float3(0.8217013, 0.3253050, 0.1668191),
    float3(0.8358996, 0.1449739, 0.3668382),
    float3(0.1778213, 0.5599256, 0.1327691),
    float3(0.06690693, 0.5508637, 0.07212365),
    float3(0.9750564, 0.2840660, 0.5727578),
    float3(0.4350255, 0.8949825, 0.03574753),
    float3(0.8931149, 0.9177974, 0.8123496),
    float3(0.9055127, 0.9899030, 0.8132350),
    float3(0.2897243, 0.3123978, 0.5083504),
    float3(0.1519223, 0.3958645, 0.2640327),
    float3(0.6840154, 0.6463035, 0.2346607),
    float3(0.9864730, 0.8714055, 0.3960275),
    float3(0.6819352, 0.4169535, 0.8379834),
    float3(0.9147297, 0.6144146, 0.7313942),
    float3(0.6554981, 0.5014008, 0.9748477),
    float3(0.9805915, 0.1318207, 0.2371372),
    float3(0.5980836, 0.06796348, 0.9941338),
    float3(0.6836596, 0.9917196, 0.2319056),
    float3(0.5276511, 0.2745509, 0.5422578),
    float3(0.8294820, 0.03758276, 0.1240466),
    float3(0.2698198, 0.0002266169, 0.3449324),
};

static float almetal_ssr_random(float2 uv)
{
    return fract(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453123);
}

static float2 almetal_ssr_project(
    float3 position,
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr)
{
    const float4 projected = ssr.projection * float4(position, 1.0);
    return projected.xy / projected.w * 0.5 + 0.5;
}

static float2 almetal_ssr_source_uv(float2 gl_uv)
{
    // glReadPixels writes GL's lower row first, and the upload preserves that
    // byte order as Metal row zero. Keep the GL coordinate unchanged: a
    // second origin conversion here would invert the mirrored history.
    return gl_uv;
}

static float almetal_ssr_linear_depth(
    float2 gl_uv, texture2d<float> scene_depth, sampler depth_sampler,
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr)
{
    const float depth = scene_depth.sample(
        depth_sampler, almetal_ssr_source_uv(gl_uv)).r;
    const float ndc_z = ssr.config.x != 0u ? depth : 2.0 * depth - 1.0;
    const float4 position = ssr.inverse_projection *
        float4(gl_uv * 2.0 - 1.0, ndc_z, 1.0);
    return -position.z / max(position.w, 0.000001);
}

static bool almetal_ssr_trace(
    float3 position, float3 reflection,
    thread float4& hit_color, thread float& hit_depth,
    texture2d<float> scene_color, texture2d<float> scene_depth,
    sampler color_sampler, sampler depth_sampler,
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr)
{
    reflection += position;
    position = (ssr.inverse_modelview_delta *
                float4(position, 1.0)).xyz;
    reflection = (ssr.inverse_modelview_delta *
                  float4(reflection, 1.0)).xyz;
    reflection -= position;

    const float source_depth = -position.z;
    float3 step = ssr.trace.y * reflection;
    float3 marching_position = position + step;
    float delta = 0.0;
    bool hit = false;
    bool crossed = false;
    hit_color = 0.0;

    int iteration = 0;
    const int iteration_count = max(int(ssr.trace.x), 0);
    if (source_depth > ssr.trace.w)
    {
        for (; iteration < iteration_count && !hit; ++iteration)
        {
            if (marching_position.z >= 0.0)
            {
                break;
            }
            const float2 screen_position =
                almetal_ssr_project(marching_position, ssr);
            if (any(screen_position > 1.0) || any(screen_position < 0.0))
            {
                break;
            }
            const float depth_from_screen = almetal_ssr_linear_depth(
                screen_position, scene_depth, depth_sampler, ssr);
            delta = abs(marching_position.z) - depth_from_screen;
            if (source_depth < depth_from_screen + 0.1 &&
                source_depth > depth_from_screen - 0.1)
            {
                break;
            }
            if (abs(delta) < ssr.trace.z)
            {
                hit_color = scene_color.sample(
                    color_sampler,
                    almetal_ssr_source_uv(screen_position));
                hit_depth = depth_from_screen;
                hit = true;
                break;
            }
            if (delta > 0.0)
            {
                crossed = true;
                break;
            }
            const float direction_sign = sign(
                abs(marching_position.z) - depth_from_screen);
            step *= 1.0 - ssr.trace.y * max(direction_sign, 0.0);
            marching_position += step * -direction_sign;
            step *= ssr.sampling.y;
        }
        if (crossed)
        {
            for (; iteration < iteration_count && !hit; ++iteration)
            {
                step *= 0.5;
                marching_position -= step * sign(delta);
                if (marching_position.z >= 0.0)
                {
                    break;
                }
                const float2 screen_position =
                    almetal_ssr_project(marching_position, ssr);
                if (any(screen_position > 1.0) ||
                    any(screen_position < 0.0))
                {
                    break;
                }
                const float depth_from_screen = almetal_ssr_linear_depth(
                    screen_position, scene_depth, depth_sampler, ssr);
                delta = abs(marching_position.z) - depth_from_screen;
                if (source_depth < depth_from_screen + 0.1 &&
                    source_depth > depth_from_screen - 0.1)
                {
                    break;
                }
                if (abs(delta) < ssr.trace.z &&
                    depth_from_screen != source_depth - ssr.trace.z)
                {
                    hit_color = scene_color.sample(
                        color_sampler,
                        almetal_ssr_source_uv(screen_position));
                    hit_depth = depth_from_screen;
                    hit = true;
                    break;
                }
            }
        }
    }
    return hit;
}

static float4 almetal_sample_ssr(
    float2 gl_screen_position, float3 position, float3 normal,
    float3 ray_direction, float glossiness, bool transparent,
    texture2d<float> scene_color, texture2d<float> scene_depth,
    sampler color_sampler, sampler depth_sampler,
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr)
{
    constexpr float minimum_opaque_glossiness = 1.7 / 3.0;
    if (ssr.config.y < 1u ||
        (transparent ? glossiness <= 0.0
                     : glossiness <= minimum_opaque_glossiness))
    {
        return 0.0;
    }

    const float2 screen_position =
        1.0 - abs(gl_screen_position * 2.0 - 1.0);
    float vignette = clamp(
        abs(screen_position.x) * abs(screen_position.y) * 16.0,
        0.0, 1.0);
    vignette *= clamp(
        (dot(normalize(position), normal) * 0.5 + 0.5) * 5.5 - 0.8,
        0.0, 1.0);
    vignette *= clamp(1.0 + position.z / 128.0, 0.0, 1.0);
    // Transparent materials trace one unfaded mirror ray and spend authored
    // glossiness only on the final blend. Opaque deferred surfaces use the
    // shared 0.567 fade threshold and spread samples by authored roughness.
    const float trace_glossiness = transparent ? 1.0 : glossiness;
    vignette *= clamp(trace_glossiness * 3.0 - 1.7, 0.0, 1.0);
    if (vignette <= 0.0)
    {
        return 0.0;
    }

    const float roughness = 1.0 - trace_glossiness;
    const int maximum_samples = max(1, int(ssr.sampling.x));
    const int total_samples = clamp(
        int(ceil(ssr.sampling.x * roughness)), 1, maximum_samples);
    float4 collected = 0.0;
    int hits = 0;
    for (int index = 0; index < total_samples; ++index)
    {
        const float3 poisson = almetal_ssr_poisson[index] * 2.0 - 1.0;
        const float3 first_basis = normalize(cross(poisson, ray_direction));
        const float3 second_basis = normalize(
            cross(ray_direction, first_basis));
        const float2 coefficients = float2(
            almetal_ssr_random(
                gl_screen_position + float2(0.0, float(index))),
            almetal_ssr_random(
                gl_screen_position + float2(float(index), 0.5))) *
            2.0 - 1.0;
        const float3 randomized = ray_direction +
            (first_basis * coefficients.x +
             second_basis * coefficients.y) * roughness;
        float4 hit_color = 0.0;
        float hit_depth = 0.0;
        if (almetal_ssr_trace(
                position, normalize(randomized), hit_color, hit_depth,
                scene_color, scene_depth, color_sampler, depth_sampler,
                ssr))
        {
            ++hits;
            hit_color.a = 0.0;
            collected += hit_color;
            collected.a += 1.0;
        }
    }
    if (hits > 0)
    {
        collected /= float(hits);
    }
    else
    {
        collected = 0.0;
    }
    const float alpha = float(hits) / float(total_samples) * vignette *
        (transparent ? glossiness : 1.0);
    return float4(collected.rgb, alpha);
}

static float3 almetal_finish_legacy_ssr(
    float3 legacy_environment, float2 gl_screen_position,
    float3 position, float3 normal, float glossiness,
    texture2d<float> scene_color, texture2d<float> scene_depth,
    sampler color_sampler, sampler depth_sampler,
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr)
{
    const float4 sample = almetal_sample_ssr(
        gl_screen_position, position, normal,
        normalize(reflect(position, normal)), glossiness, true,
        scene_color, scene_depth, color_sampler, depth_sampler, ssr);
    return mix(legacy_environment, sample.rgb, sample.a);
}

static float3 almetal_sample_legacy_sky(
    float3 position, float3 normal,
    texturecube<float> legacy_environment, sampler probe_sampler,
    constant ALMetalWorldAlphaEnvironment& environment)
{
    const float3x3 environment_matrix = float3x3(
        environment.environment_matrix[0].xyz,
        environment.environment_matrix[1].xyz,
        environment.environment_matrix[2].xyz);
    const float3 reflection = normalize(reflect(position, normal));
    return legacy_environment.sample(
        probe_sampler, environment_matrix * reflection).rgb;
}

static float3 almetal_legacy_sky_ambient(
    float3 ambient,
    constant ALMetalWorldAlphaEnvironment& environment)
{
    const float ambiance = environment.environment.x;
    return mix(ambient, float3(ambiance * 0.25), ambiance);
}

fragment float4 almetal_world_shiny_fragment(
    ALMetalWorldShinyVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    texturecube_array<float> reflection_probes [[texture(32)]],
    texturecube<float> hero_probe [[texture(34)]],
    texturecube<float> legacy_sky [[texture(39)]],
    sampler diffuse_sampler [[sampler(0)]],
    sampler probe_sampler [[sampler(1)]],
    constant ALMetalWorldAlphaEnvironment& environment [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]],
    constant ALMetalWorldHeroProbeConstants& hero [[buffer(4)]])
{
    const float4 diffuse = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord)
        : float4(1.0, 0.0, 1.0, 1.0);
    float3 color = diffuse.rgb * input.color.rgb;
    const float3 normal = normalize(input.normal);
    const float environment_intensity = input.color.a;
    const bool legacy_environment_path =
        environment.environment_config.x != 0u;
    float3 legacy_environment = legacy_environment_path
        ? almetal_sample_legacy_sky(
              input.eye_position, normal, legacy_sky,
              probe_sampler, environment)
        : almetal_sample_legacy_probes(
              input.eye_position, normal,
              reflection_probes, probe_sampler, probes);
    if (legacy_environment_path)
    {
        color = almetal_srgb_to_linear(mix(
            almetal_linear_to_srgb(color),
            legacy_environment * 2.0,
            environment_intensity));
    }
    else
    {
        legacy_environment = almetal_finish_hero_probe(
            legacy_environment, input.eye_position, normal, 1.0,
            hero_probe, probe_sampler, probes, hero);
        float fresnel = 1.0 + dot(
            normalize(input.eye_position), normal);
        fresnel *= fresnel;
        fresnel = min(fresnel + environment_intensity, 1.0);
        const float3 reflected_color = legacy_environment *
            (environment_intensity * fresnel);
        color = mix(
            color, reflected_color * 0.5,
            environment_intensity);
    }
    return max(float4(color, 1.0), float4(0.0));
}

struct ALMetalWorldEmbossVaryings
{
    float4 position [[position]];
    float2 texcoord0;
    float2 texcoord1;
};

vertex ALMetalWorldEmbossVaryings almetal_world_emboss_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords0 [[buffer(2)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float2* texcoords1 [[buffer(9)]],
    uint vertex_id [[vertex_id]])
{
    ALMetalWorldEmbossVaryings output;
    float4 clip_position = constants.modelview_projection *
                           float4(positions[vertex_id].xyz, 1.0);
    clip_position.y = -clip_position.y;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord0 = (constants.texture_matrix *
                        float4(texcoords0[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = (constants.texture_matrix *
                        float4(texcoords1[vertex_id], 0.0, 1.0)).xy;
    return output;
}

vertex ALMetalWorldEmbossVaryings almetal_world_emboss_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords0 [[buffer(2)]],
    constant ALMetalWorldSimpleConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float4* encoded_weights [[buffer(6)]],
    constant uint& joint_count [[buffer(7)]],
    const device float2* texcoords1 [[buffer(9)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], float4(0.0), encoded_weights[vertex_id],
        skin_palette, joint_count);
    float4 clip_position =
        constants.modelview_projection * skinned.position +
        constants.modelview_projection * float4(skin_palette[0].xyz, 1.0);
    clip_position.y = -clip_position.y;

    ALMetalWorldEmbossVaryings output;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord0 = (constants.texture_matrix *
                        float4(texcoords0[vertex_id], 0.0, 1.0)).xy;
    output.texcoord1 = (constants.texture_matrix *
                        float4(texcoords1[vertex_id], 0.0, 1.0)).xy;
    return output;
}

fragment float4 almetal_world_emboss_fragment(
    ALMetalWorldEmbossVaryings input [[stage_in]],
    texture2d<float> bump_texture [[texture(0)]],
    sampler bump_sampler [[sampler(0)]])
{
    const float first = bump_texture.sample(
        bump_sampler, input.texcoord0).a;
    const float second = bump_texture.sample(
        bump_sampler, input.texcoord1).a;
    return max(float4(first + (1.0 - second) - 0.5), float4(0.0));
}

fragment float4 almetal_world_flat_alpha_fragment(
    ALMetalWorldSimpleVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    const float4 color = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord) * input.color
        : float4(1.0, 0.0, 1.0, 1.0);
    constexpr float minimum_alpha = 0.004;
    if (color.a < minimum_alpha)
    {
        discard_fragment();
    }

    // Both deferred/alphaF's FOR_IMPOSTOR branch and the fullbright program
    // emit this same flat decoded diffuse times linearised streamed tint.
    return max(color, float4(0.0));
}

fragment float4 almetal_world_pbr_alpha_fragment(
    ALMetalWorldPBRVaryings input [[stage_in]],
    array<texture2d<float>, 8> base_color_textures [[texture(0)]],
    sampler material_sampler [[sampler(0)]],
    constant ALMetalWorldPBRConstants& constants [[buffer(0)]])
{
    const uint material_index = min(input.material_index, 7u);
    const float4 base_color =
        base_color_textures[material_index].sample(
            material_sampler, input.base_color_texcoord) * input.color;
    constexpr float minimum_alpha = 0.004;
    if (base_color.a < minimum_alpha)
    {
        discard_fragment();
    }

    // pbralphaF's FOR_IMPOSTOR path deliberately emits flat, unlit base
    // colour and true coverage. The sRGB attachment performs its transfer
    // around fixed-function linear blending; alpha uses src + (1-src)*dst.
    return max(base_color, float4(0.0));
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

static float almetal_legacy_blinn_phong_lobe(
    float nh, float glossiness, float specular_exponent)
{
    constexpr float pi = 3.14159265358979323846;
    const float exponent = glossiness * glossiness * specular_exponent;
    const float normalization =
        ((exponent + 2.0) * (exponent + 4.0)) /
        (8.0 * pi * (exp2(-exponent * 0.5) + exponent));
    return pow(nh, exponent) * normalization;
}

static void almetal_legacy_half_vectors(
    float3 light_vector, float3 normal, float3 view,
    thread float3& half_vector, thread float3& light_direction,
    thread float& nh, thread float& nl,
    thread float& nv, thread float& vh)
{
    constexpr float epsilon = 0.000001;
    light_direction = normalize(light_vector);
    half_vector = normalize(light_direction + view);
    nh = clamp(dot(normal, half_vector), epsilon, 1.0);
    nl = clamp(dot(normal, light_direction), epsilon, 1.0);
    nv = clamp(dot(normal, view), epsilon, 1.0);
    vh = clamp(dot(view, half_vector), epsilon, 1.0);
}

static float almetal_legacy_distance_attenuation(
    float distance, float falloff)
{
    float attenuation = 1.0 -
        clamp((distance + falloff) / (1.0 + falloff), 0.0, 1.0);
    return attenuation * attenuation * 2.0;
}

static void almetal_legacy_local_glare(
    ALMetalWorldLegacyAlphaLight light,
    float3 normal, float3 position, float3 view,
    float3 specular, float glossiness, float specular_exponent,
    thread float& glare)
{
    const float light_size = light.parameters.x;
    if (light_size <= 0.0)
    {
        return;
    }
    const float3 light_vector = light.position.xyz - position;
    const float light_distance = length(light_vector);
    const float distance = light_distance / light_size;
    if (distance > 1.0 || dot(normal, light_vector) <= 0.0 ||
        light_distance <= 0.0 || glossiness <= 0.0)
    {
        return;
    }

    float3 half_vector;
    float3 light_direction;
    float nh;
    float nl;
    float nv;
    float vh;
    almetal_legacy_half_vectors(
        light_vector, normal, view, half_vector, light_direction,
        nh, nl, nv, vh);
    const float distance_attenuation = almetal_legacy_distance_attenuation(
        distance, light.parameters.y);
    const float spot = max(
        dot(-light.direction.xyz, light_direction), light.parameters.z);
    const float spot_attenuation = spot * spot;
    const float lit = min(nl * 6.0, 1.0) * distance_attenuation;
    const float fresnel = pow(1.0 - vh, 5.0) * 0.4 + 0.5;
    const float geometry_denominator = 2.0 * nh;
    const float geometry = max(
        0.0, min(geometry_denominator * nv / vh,
                 geometry_denominator * nl / vh));
    const float lobe = fresnel * almetal_legacy_blinn_phong_lobe(
        nh, glossiness, specular_exponent) * geometry / (nh * nl);
    const float3 specular_contribution =
        lit * lobe * light.diffuse.rgb * specular;
    const float3 glare_source = clamp(
        specular_contribution * spot_attenuation, 0.0, 1.0);
    const float current_glare = max(
        max(glare_source.r, glare_source.g), glare_source.b);
    glare = max(glare, glare_source.r);
    glare += current_glare;
}

static float3 almetal_legacy_local_light(
    ALMetalWorldLegacyAlphaLight light,
    float3 diffuse, float3 normal, float3 position, float3 view,
    float3 specular, float glossiness, float specular_exponent,
    bool classic_mode, thread float& glare)
{
    const float light_size = light.parameters.x;
    if (light_size <= 0.0)
    {
        return 0.0;
    }
    const float3 light_vector = light.position.xyz - position;
    const float light_distance = length(light_vector);
    const float distance = light_distance / light_size;
    if (distance > 1.0 || dot(normal, light_vector) <= 0.0 ||
        light_distance <= 0.0)
    {
        return 0.0;
    }

    float3 half_vector;
    float3 light_direction;
    float nh;
    float nl;
    float nv;
    float vh;
    almetal_legacy_half_vectors(
        light_vector, normal, view, half_vector, light_direction,
        nh, nl, nv, vh);
    const float distance_attenuation = almetal_legacy_distance_attenuation(
        distance, light.parameters.y);
    const float spot = max(
        dot(-light.direction.xyz, light_direction), light.parameters.z);
    const float spot_attenuation = spot * spot;
    float3 color = light.diffuse.rgb *
        (nl * distance_attenuation) * diffuse;

    if (glossiness > 0.0)
    {
        const float lit = min(nl * 6.0, 1.0) * distance_attenuation;
        const float fresnel = pow(1.0 - vh, 5.0) * 0.4 + 0.5;
        const float geometry_denominator = 2.0 * nh;
        const float geometry = max(
            0.0, min(geometry_denominator * nv / vh,
                     geometry_denominator * nl / vh));
        const float lobe = fresnel * almetal_legacy_blinn_phong_lobe(
            nh, glossiness, specular_exponent) * geometry / (nh * nl);
        const float3 specular_contribution =
            lit * lobe * light.diffuse.rgb * specular;
        color += specular_contribution;

        const float3 glare_source = clamp(
            specular_contribution * spot_attenuation, 0.0, 1.0);
        const float current_glare = max(
            max(glare_source.r, glare_source.g), glare_source.b);
        glare = max(glare, glare_source.r);
        glare += current_glare;
    }

    color = max(color * spot_attenuation, float3(0.0));
    const float peak = max(color.r, max(color.g, color.b));
    if (peak > 65504.0)
    {
        color *= 65504.0 / peak;
    }
    return color * (classic_mode ? 0.9 : 1.0);
}

static float almetal_shadow_depth(
    texture2d_array<float> shadow_maps, uint cascade,
    int2 texel, uint2 resolution)
{
    const int2 bounded = clamp(
        texel, int2(0), int2(resolution) - int2(1));
    // Both the GL mirror upload and native caster pass retain GL's raw row
    // order: Metal storage row zero contains the GL lower row. The shadow
    // matrices also produce GL texel coordinates, so address that retained
    // row directly. Flipping here would invert every shadow map a second
    // time after the producer has already established row-order parity.
    return shadow_maps.read(uint2(bounded), cascade).r;
}

static float almetal_shadow_compare(
    float depth, float reference, bool reverse_z)
{
    const float bounded = clamp(reference, 0.0, 1.0);
    return reverse_z
        ? (bounded >= depth ? 1.0 : 0.0)
        : (bounded <= depth ? 1.0 : 0.0);
}

static float almetal_shadow_single_tap(
    texture2d_array<float> shadow_maps, uint cascade,
    float3 uvz, uint2 resolution, uint quality, bool reverse_z)
{
    // shadowUtil.glsl's cube_snapshot shortcut preserves the sampler baked
    // into its quality variant. Ultra uses PointClamp over raw depth; the
    // compare tiers use one linearly filtered comparison tap, whose four
    // depth comparisons Metal reproduces explicitly over retained GL rows.
    if (quality >= 3u)
    {
        const int2 texel = int2(floor(
            uvz.xy * float2(resolution)));
        return almetal_shadow_compare(
            almetal_shadow_depth(
                shadow_maps, cascade, texel, resolution),
            uvz.z, reverse_z);
    }

    const float2 location =
        uvz.xy * float2(resolution) - 0.5;
    const int2 lower_left = int2(floor(location));
    const float2 fraction = fract(location);
    const float lower = mix(
        almetal_shadow_compare(
            almetal_shadow_depth(
                shadow_maps, cascade, lower_left, resolution),
            uvz.z, reverse_z),
        almetal_shadow_compare(
            almetal_shadow_depth(
                shadow_maps, cascade, lower_left + int2(1, 0),
                resolution),
            uvz.z, reverse_z),
        fraction.x);
    const float upper = mix(
        almetal_shadow_compare(
            almetal_shadow_depth(
                shadow_maps, cascade, lower_left + int2(0, 1),
                resolution),
            uvz.z, reverse_z),
        almetal_shadow_compare(
            almetal_shadow_depth(
                shadow_maps, cascade, lower_left + int2(1, 1),
                resolution),
            uvz.z, reverse_z),
        fraction.x);
    return mix(lower, upper, fraction.y);
}

static float2 almetal_shadow_receiver_plane_bias(
    float2 duvdx, float2 duvdy, float dzdx, float dzdy,
    float2 texel)
{
    const float determinant =
        duvdx.x * duvdy.y - duvdx.y * duvdy.x;
    float2 dz_duv = 0.0;
    if (abs(determinant) >
        0.001 * length(duvdx) * length(duvdy))
    {
        const float inverse = 1.0 / determinant;
        dz_duv.x =
            (duvdy.y * dzdx - duvdx.y * dzdy) * inverse;
        dz_duv.y =
            (-duvdy.x * dzdx + duvdx.x * dzdy) * inverse;
    }
    return clamp(
        dz_duv * texel, float2(-0.01), float2(0.01));
}

static float almetal_shadow_gaussian_pcf(
    texture2d_array<float> shadow_maps, uint cascade,
    float3 uvz, uint2 resolution, float2 dz_texel,
    uint quality, bool reverse_z)
{
    const uint filter_width = quality == 0u ? 2u :
                              (quality == 1u ? 4u : 6u);
    const float2 c = uvz.xy * float2(resolution);
    const int2 base = int2(floor(c + 0.5)) -
                      int2(int(filter_width / 2u));
    const float sigma = float(filter_width) * 0.25;
    const float inv2s2 = 1.0 / (2.0 * sigma * sigma);
    const float edge = float(filter_width) * 0.5;
    const float pedestal = exp(-edge * edge * inv2s2);
    float wx[6];
    float wy[6];
    for (uint index = 0; index < filter_width; ++index)
    {
        const float dx =
            float(base.x + int(index)) + 0.5 - c.x;
        const float dy =
            float(base.y + int(index)) + 0.5 - c.y;
        wx[index] = max(0.0, exp(-dx * dx * inv2s2) - pedestal);
        wy[index] = max(0.0, exp(-dy * dy * inv2s2) - pedestal);
    }

    const float toward_lit = reverse_z ? 1.0 : -1.0;
    const float block_slope = toward_lit * 0.5 *
        (abs(dz_texel.x) + abs(dz_texel.y));
    float shadow = 0.0;
    float weight_sum = 0.0;
    for (uint by = 0; by < filter_width; by += 2u)
    {
        for (uint bx = 0; bx < filter_width; bx += 2u)
        {
            const int2 lower_left =
                base + int2(int(bx), int(by));
            const float2 block_offset =
                float2(lower_left) + 1.0 - c;
            const float reference = uvz.z +
                dot(block_offset, dz_texel) + block_slope;
            const float d00 = almetal_shadow_depth(
                shadow_maps, cascade, lower_left,
                resolution);
            const float d10 = almetal_shadow_depth(
                shadow_maps, cascade, lower_left + int2(1, 0),
                resolution);
            const float d01 = almetal_shadow_depth(
                shadow_maps, cascade, lower_left + int2(0, 1),
                resolution);
            const float d11 = almetal_shadow_depth(
                shadow_maps, cascade, lower_left + int2(1, 1),
                resolution);
            shadow += almetal_shadow_compare(
                          d00, reference, reverse_z) *
                      wx[bx] * wy[by];
            shadow += almetal_shadow_compare(
                          d10, reference, reverse_z) *
                      wx[bx + 1u] * wy[by];
            shadow += almetal_shadow_compare(
                          d01, reference, reverse_z) *
                      wx[bx] * wy[by + 1u];
            shadow += almetal_shadow_compare(
                          d11, reference, reverse_z) *
                      wx[bx + 1u] * wy[by + 1u];
            weight_sum += (wx[bx] + wx[bx + 1u]) *
                          (wy[by] + wy[by + 1u]);
        }
    }
    return shadow / weight_sum;
}

constant float2 almetal_pcss_blocker_disc[8] = {
    float2(+0.250000, +0.000000), float2(-0.319290, +0.292496),
    float2(+0.048872, -0.556877), float2(+0.402444, +0.524918),
    float2(-0.738535, -0.130636), float2(+0.699605, -0.445031),
    float2(-0.234004, +0.870484), float2(-0.446271, -0.859268),
};

constant float2 almetal_pcss_filter_disc[12] = {
    float2(+0.204124, +0.000000), float2(-0.260699, +0.238822),
    float2(+0.039904, -0.454688), float2(+0.328595, +0.428593),
    float2(-0.603011, -0.106664), float2(+0.571225, -0.363367),
    float2(-0.191064, +0.710747), float2(-0.364379, -0.701590),
    float2(+0.790557, +0.288710), float2(-0.822442, +0.339492),
    float2(+0.396472, -0.847237), float2(+0.292982, +0.934074),
};

static float almetal_pcss_rotation(float2 screen_position)
{
    const float2 p = screen_position * 2048.0;
    return fract(52.9829189 * fract(
        dot(p, float2(0.06711056, 0.00583715)))) * 6.2831853;
}

static float2 almetal_pcss_rotate(float2 value, float2 cs)
{
    return float2(value.x * cs.x - value.y * cs.y,
                  value.x * cs.y + value.y * cs.x);
}

static float4 almetal_shadow_gather_depth(
    texture2d_array<float> shadow_maps, uint cascade,
    float2 at, uint2 resolution)
{
    const int2 lower_left = int2(floor(
        at * float2(resolution) - 0.5));
    return float4(
        almetal_shadow_depth(
            shadow_maps, cascade, lower_left + int2(0, 1), resolution),
        almetal_shadow_depth(
            shadow_maps, cascade, lower_left + int2(1, 1), resolution),
        almetal_shadow_depth(
            shadow_maps, cascade, lower_left + int2(1, 0), resolution),
        almetal_shadow_depth(
            shadow_maps, cascade, lower_left, resolution));
}

static float4 almetal_shadow_gather_references(
    float3 uvz, float2 resolution, float2 at, float2 dz_texel)
{
    const float2 lower_left =
        floor(at * resolution - 0.5) + 0.5 - uvz.xy * resolution;
    const float reference = uvz.z + dot(lower_left, dz_texel);
    return float4(
        reference + dz_texel.y,
        reference + dz_texel.x + dz_texel.y,
        reference + dz_texel.x,
        reference);
}

static float4 almetal_shadow_compare4(
    float4 depth, float4 reference, bool reverse_z)
{
    return float4(
        almetal_shadow_compare(depth.x, reference.x, reverse_z),
        almetal_shadow_compare(depth.y, reference.y, reverse_z),
        almetal_shadow_compare(depth.z, reference.z, reverse_z),
        almetal_shadow_compare(depth.w, reference.w, reverse_z));
}

static float almetal_shadow_pcss(
    texture2d_array<float> shadow_maps, uint cascade,
    float3 uvz, uint2 resolution, float2 dz_texel,
    float2 screen_position, float pcss_scale, bool reverse_z)
{
    const float angle = almetal_pcss_rotation(screen_position);
    const float2 cs = float2(cos(angle), sin(angle));
    float blocker_sum = 0.0;
    float blocker_count = 0.0;
    for (uint index = 0; index < 8; ++index)
    {
        const float2 offset = almetal_pcss_rotate(
            almetal_pcss_blocker_disc[index], cs) * 6.0;
        const float2 at = uvz.xy + offset / float2(resolution);
        const float4 depth = almetal_shadow_gather_depth(
            shadow_maps, cascade, at, resolution);
        const float4 reference = almetal_shadow_gather_references(
            uvz, float2(resolution), at, dz_texel);
        const float4 blocker = reverse_z
            ? select(float4(0.0), float4(1.0), depth >= reference)
            : select(float4(0.0), float4(1.0), depth <= reference);
        blocker_sum += dot(depth, blocker);
        blocker_count += dot(blocker, float4(1.0));
    }
    if (blocker_count <= 0.0)
    {
        return 1.0;
    }

    const float blocker_depth = blocker_sum / blocker_count;
    const float radius = clamp(
        (reverse_z ? blocker_depth - uvz.z : uvz.z - blocker_depth) *
            pcss_scale,
        1.0, 12.0);
    float shadow = 0.0;
    float weight_sum = 0.0;
    for (uint index = 0; index < 12; ++index)
    {
        const float2 unit = almetal_pcss_filter_disc[index];
        const float2 offset = almetal_pcss_rotate(unit, cs) * radius;
        const float2 at =
            uvz.xy + offset / float2(resolution);
        const float4 depth = almetal_shadow_gather_depth(
            shadow_maps, cascade, at, resolution);
        const float4 reference = almetal_shadow_gather_references(
            uvz, float2(resolution), at, dz_texel);
        const float weight = 1.0 - dot(unit, unit);
        shadow += dot(
            almetal_shadow_compare4(depth, reference, reverse_z),
            float4(0.25)) * weight;
        weight_sum += weight;
    }
    return shadow / weight_sum;
}

static bool almetal_shadow_map_built(
    uint cascade, constant ALMetalWorldDirectionalShadowConstants& shadows)
{
    return (shadows.config.y & (1u << cascade)) == 0u;
}

static float almetal_filter_directional_shadow(
    texture2d_array<float> shadow_maps, uint cascade,
    float4 light_position, float4 light_dx, float4 light_dy,
    float2 screen_position, float constant_bias,
    constant ALMetalWorldDirectionalShadowConstants& shadows)
{
    const float inverse_w = 1.0 / light_position.w;
    float3 uvz = light_position.xyz * inverse_w;
    uvz.z += constant_bias;
    const uint quality = shadows.config.w & 0x3u;
    const uint2 resolution = uint2(shadows.resolution.xy);
    if ((shadows.config.w & 0x4u) != 0u)
    {
        return almetal_shadow_single_tap(
            shadow_maps, cascade, uvz, resolution,
            quality, shadows.config.z != 0u);
    }
    const float2 duvdx =
        (light_dx.xy - uvz.xy * light_dx.w) * inverse_w;
    const float2 duvdy =
        (light_dy.xy - uvz.xy * light_dy.w) * inverse_w;
    const float dzdx =
        (light_dx.z - uvz.z * light_dx.w) * inverse_w;
    const float dzdy =
        (light_dy.z - uvz.z * light_dy.w) * inverse_w;
    const float2 dz_texel = almetal_shadow_receiver_plane_bias(
        duvdx, duvdy, dzdx, dzdy, 1.0 / shadows.resolution.xy);
    const bool reverse_z = shadows.config.z != 0u;
    return quality >= 3u
        ? almetal_shadow_pcss(
              shadow_maps, cascade, uvz, resolution, dz_texel,
              screen_position, shadows.parameters.z, reverse_z)
        : almetal_shadow_gaussian_pcf(
              shadow_maps, cascade, uvz, resolution, dz_texel,
              quality, reverse_z);
}

static float almetal_directional_shadow(
    texture2d_array<float> shadow_maps,
    constant ALMetalWorldDirectionalShadowConstants& shadows,
    float3 position, float3 normal, float3 position_dx,
    float3 position_dy, float2 screen_position,
    float3 directional_light)
{
    const float directional = clamp(
        dot(normal, normalize(directional_light)), 0.0, 1.0);
    const float3 shadow_position = position +
        normalize(directional_light) * (1.0 - directional) *
            shadows.parameters.y * 2.0;
    const float4 source = float4(shadow_position, 1.0);
    const float4 source_dx = float4(position_dx, 0.0);
    const float4 source_dy = float4(position_dy, 0.0);
    const float constant_bias = shadows.parameters.x * 2.0;
    if (source.z <= -shadows.shadow_clip.w)
    {
        return 1.0;
    }

    const float4 near_split = shadows.shadow_clip * -0.75;
    const float4 far_split = shadows.shadow_clip * -1.25;
    const float4 transition = near_split - far_split;
    const float to_lit = clamp(
        (position.z + shadows.shadow_clip.z) /
            (shadows.shadow_clip.z - shadows.shadow_clip.w) * 2.0 - 1.0,
        0.0, 1.0);
    const bool two_cascades = shadows.config.x == 2u;
    float shadow = 0.0;
    float weight_sum = 0.0;

    if (!two_cascades && source.z < near_split.z)
    {
        const uint cascade = 3u;
        float weight = 1.0 -
            max(source.z - far_split.z, 0.0) / transition.z;
        float filtered = almetal_shadow_map_built(cascade, shadows)
            ? almetal_filter_directional_shadow(
                  shadow_maps, cascade,
                  shadows.shadow_matrix[cascade] * source,
                  shadows.shadow_matrix[cascade] * source_dx,
                  shadows.shadow_matrix[cascade] * source_dy,
                  screen_position, constant_bias, shadows)
            : 1.0;
        shadow += mix(filtered, 1.0, to_lit) * weight;
        weight_sum += weight;
    }
    if (!two_cascades && source.z < near_split.y &&
        source.z > far_split.z)
    {
        const uint cascade = 2u;
        float weight = 1.0;
        weight -= max(source.z - far_split.y, 0.0) / transition.y;
        weight -= max(near_split.z - source.z, 0.0) / transition.z;
        const float filtered = almetal_shadow_map_built(cascade, shadows)
            ? almetal_filter_directional_shadow(
                  shadow_maps, cascade,
                  shadows.shadow_matrix[cascade] * source,
                  shadows.shadow_matrix[cascade] * source_dx,
                  shadows.shadow_matrix[cascade] * source_dy,
                  screen_position, constant_bias, shadows)
            : 1.0;
        shadow += filtered * weight;
        weight_sum += weight;
    }
    if (source.z < near_split.x && source.z > far_split.y)
    {
        const uint cascade = 1u;
        float weight = 1.0;
        weight -= max(source.z - far_split.x, 0.0) / transition.x;
        weight -= max(near_split.y - source.z, 0.0) / transition.y;
        float filtered = almetal_shadow_map_built(cascade, shadows)
            ? almetal_filter_directional_shadow(
                  shadow_maps, cascade,
                  shadows.shadow_matrix[cascade] * source,
                  shadows.shadow_matrix[cascade] * source_dx,
                  shadows.shadow_matrix[cascade] * source_dy,
                  screen_position, constant_bias, shadows)
            : 1.0;
        filtered = two_cascades ? mix(filtered, 1.0, to_lit) : filtered;
        shadow += filtered * weight;
        weight_sum += weight;
    }
    if (source.z > far_split.x)
    {
        const uint cascade = 0u;
        const float weight = 1.0 -
            max(near_split.x - source.z, 0.0) / transition.x;
        const float filtered = almetal_shadow_map_built(cascade, shadows)
            ? almetal_filter_directional_shadow(
                  shadow_maps, cascade,
                  shadows.shadow_matrix[cascade] * source,
                  shadows.shadow_matrix[cascade] * source_dx,
                  shadows.shadow_matrix[cascade] * source_dy,
                  screen_position, constant_bias, shadows)
            : 1.0;
        shadow += filtered * weight;
        weight_sum += weight;
    }
    return shadow / weight_sum;
}

static float3 almetal_world_water_transform_normal(
    float3 value,
    constant ALMetalWorldWaterSurfaceConstants& water)
{
    const float3x3 normal_matrix = float3x3(
        water.normal_matrix[0].xyz,
        water.normal_matrix[1].xyz,
        water.normal_matrix[2].xyz);
    const float3 plane_normal = normal_matrix[2];
    const float3 plane_tangent = normal_matrix[0];
    const float3 plane_bitangent = cross(plane_normal, plane_tangent);
    return normalize(value.x * plane_tangent +
                     value.y * plane_bitangent +
                     value.z * plane_normal);
}

// Water rejects automatic probes while retaining manual probes and the
// default probe zero. This is reflectionProbeF.glsl's water=true probe walk;
// the automatic family is enabled only at the final mix so probe zero can
// smooth the manual result without allowing other automatic probes in.
static void almetal_water_probe_tap(
    int index, float3 position, float3 box_position,
    float3 gloss_direction, float gloss_lod,
    texturecube_array<float> radiance, sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes,
    thread ALMetalBaseProbeSums& sums)
{
    float distance_weight = 0.0;
    const float weight = almetal_probe_weight(
        index, position, box_position, distance_weight, probes);
    const uint family =
        clamp(abs(probes.ref_index[index].w), 0, 1) == 0 ? 0u : 1u;
    sums.gloss[family] += almetal_probe_radiance(
        index, position, box_position, gloss_direction, gloss_lod,
        true, radiance, probe_sampler, probes) * weight;
    sums.weight[family] += weight;
    sums.distance_weight[family] += distance_weight;
}

static float3 almetal_sample_water_probes(
    float3 position, float3 normal, float glossiness,
    texturecube_array<float> radiance, sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes)
{
    const float3 reflection = normalize(reflect(position, normal));
    const float perceptual_roughness = 1.0 - glossiness;
    const float3 gloss_direction = almetal_base_dominant_direction(
        normal, reflection, perceptual_roughness);
    const float gloss_lod = perceptual_roughness * probes.config.y;
    ALMetalBaseProbeSums sums{};
    const bool automatic = false;
    int accepted_count = 0;
    const int probe_count = clamp(int(probes.config.x), 1, 256);
    int index = almetal_probe_start_index(position, probe_count, probes);
    int neighbor = -1;
    int neighbor_base = 0;
    int neighbor_count = 0;
    while (accepted_count < 32)
    {
        int candidate;
        if (neighbor < 0)
        {
            if (index >= probe_count ||
                probes.ref_params[index].w > -position.z)
            {
                break;
            }
            candidate = index++;
        }
        else
        {
            if (neighbor >= neighbor_count)
            {
                break;
            }
            const int vector_index = neighbor_base + (neighbor >> 2);
            if (vector_index < 0 || vector_index >= 1024)
            {
                break;
            }
            candidate = probes.ref_neighbor[vector_index][neighbor & 3];
            ++neighbor;
            if (candidate < 0 || candidate >= probe_count)
            {
                continue;
            }
        }

        bool boxed = false;
        float3 box_position = 0.0;
        if (!almetal_probe_holds(
                candidate, position, automatic,
                boxed, box_position, probes))
        {
            continue;
        }
        almetal_water_probe_tap(
            candidate, position, box_position,
            gloss_direction, gloss_lod,
            radiance, probe_sampler, probes, sums);
        ++accepted_count;
        if (neighbor < 0 && probes.ref_index[candidate].y != -1)
        {
            neighbor_base = probes.ref_index[candidate].y;
            neighbor_count = clamp(
                probes.ref_index[candidate].z, 0, 31);
            neighbor = 0;
        }
    }

    if (accepted_count < 32)
    {
        const float3 box_position = probes.ref_index[0].w < 0
            ? (probes.ref_box[0] * float4(position, 1.0)).xyz
            : float3(0.0);
        almetal_water_probe_tap(
            0, position, box_position,
            gloss_direction, gloss_lod,
            radiance, probe_sampler, probes, sums);
    }
    return almetal_base_probe_mix(
        sums.gloss[0], sums.gloss[1], sums, true);
}

static float3 almetal_sample_water_environment(
    float3 position, float3 normal, float glossiness,
    texturecube_array<float> radiance,
    texturecube<float> legacy_environment,
    sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes,
    constant ALMetalWorldWaterSurfaceConstants& water)
{
    if (water.environment_config.x != 0u)
    {
        const float3x3 environment_matrix = float3x3(
            water.environment_matrix[0].xyz,
            water.environment_matrix[1].xyz,
            water.environment_matrix[2].xyz);
        const float3 reflection = normalize(reflect(position, normal));
        return almetal_srgb_to_linear(
            legacy_environment.sample(
                probe_sampler, environment_matrix * reflection).rgb);
    }
    return almetal_sample_water_probes(
        position, normal, glossiness,
        radiance, probe_sampler, probes);
}

static float3 almetal_world_water_depth_position(
    float2 uv, float depth, bool reverse_z,
    constant ALMetalWorldWaterHazeConstants& field)
{
    const float ndc_z = reverse_z ? depth : depth * 2.0 - 1.0;
    const float4 position = field.inverse_projection *
        float4(uv * 2.0 - 1.0, ndc_z, 1.0);
    return position.xyz / max(position.w, 0.000001);
}

fragment float4 almetal_world_above_water_fragment(
    ALMetalWorldWaterSurfaceVaryings input [[stage_in]],
    constant ALMetalWorldWaterHazeConstants& field [[buffer(0)]],
    constant ALMetalWorldWaterSurfaceConstants& water [[buffer(1)]],
    constant ALMetalWorldAlphaEnvironment& environment [[buffer(2)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(3)]],
    constant ALMetalWorldLegacyAlphaLighting& lighting [[buffer(4)]],
    constant ALMetalWorldDirectionalShadowConstants& shadows [[buffer(5)]],
    constant ALMetalWorldHeroProbeConstants& hero [[buffer(6)]],
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr [[buffer(7)]],
    texture2d<float> normal_map_a [[texture(1)]],
    texture2d<float> normal_map_b [[texture(2)]],
    texture2d<float> refraction_color [[texture(3)]],
    texture2d<float> refraction_depth [[texture(4)]],
    texturecube_array<float> reflection_radiance [[texture(32)]],
    texture2d_array<float> shadow_maps [[texture(33)]],
    texturecube<float> hero_probe [[texture(34)]],
    texture2d<float> ssr_scene_color [[texture(35)]],
    texture2d<float> ssr_scene_depth [[texture(36)]],
    texturecube<float> legacy_environment [[texture(37)]],
    sampler normal_sampler [[sampler(0)]],
    sampler screen_sampler [[sampler(1)]],
    sampler probe_sampler [[sampler(2)]],
    sampler depth_sampler [[sampler(3)]])
{
    const bool mirror_clipped =
        water.environment_config.y != 0u &&
        dot(float4(input.eye_position, 1.0),
            water.mirror_clip_plane) < 0.0;
    const float3 position_dx = dfdx(input.eye_position);
    const float3 position_dy = dfdy(input.eye_position);
    float3 wave1;
    float3 wave2;
    float3 wave3;
    almetal_world_water_wave_normals(
        input.agent_view.xy, normal_map_a, normal_map_b,
        normal_sampler, water, wave1, wave2, wave3);

    const float3 view_vector = normalize(input.eye_position);
    const float3 agent_view_vector = normalize(input.agent_view);
    const float3 wave_flat =
        (wave1 + wave2 * 0.4 + wave3 * 0.6) * 0.5;

    float3 fresnel3 = max(float3(0.0), float3(
        dot(agent_view_vector, wave1),
        dot(agent_view_vector, (wave2 + wave3) * 0.5),
        dot(agent_view_vector, wave3)) * water.optics.x + water.optics.y);
    fresnel3 *= fresnel3;
    const float2 fresnel2 = max(float2(0.0), float2(
        fresnel3.x + fresnel3.y + fresnel3.z,
        dot(agent_view_vector, wave_flat) *
            water.optics.x + water.optics.y));

    const float3 up = almetal_world_water_transform_normal(
        float3(0.0, 0.0, 1.0), water);
    float3 wave_ibl = float3(
        (wave1.xy * water.normal_scale_glint.x +
         wave2.xy * (0.4 * water.normal_scale_glint.y) +
         wave3.xy * (0.6 * water.normal_scale_glint.z)) * 0.25,
        wave_flat.z * 2.0);
    wave_ibl = almetal_world_water_transform_normal(
        normalize(wave_ibl), water);
    const float below_horizon = dot(
        reflect(view_vector, wave_ibl), up);
    if (below_horizon < 0.0)
    {
        const float3 horizon =
            reflect(view_vector, wave_ibl) - up * below_horizon;
        const float3 half_vector = horizon *
            rsqrt(max(dot(horizon, horizon), 1.0e-12)) - view_vector;
        wave_ibl = dot(half_vector, half_vector) > 1.0e-12
            ? normalize(half_vector) : up;
    }

    const float3 normal = almetal_world_water_transform_normal(
        normalize(wave_flat), water);
    float3 steep_wave = wave_flat;
    steep_wave.z *= max(-view_vector.z, 0.1);
    steep_wave = normalize(steep_wave);
    const float3 sun_normal = almetal_world_water_transform_normal(
        steep_wave, water);
    const float3 light = almetal_world_water_transform_normal(
        water.light_direction.xyz, water);

    const float2 screen_uv = input.position.xy / water.screen.xy;
    const float flat_distance = max(length(input.agent_view.xy), 5.0);
    const float flat_modulus = sqrt(flat_distance);
    float2 refracted_uv = screen_uv + steep_wave.xy *
        water.parameters.y / max(flat_modulus * fresnel2.x, 1.0);
    refracted_uv = clamp(refracted_uv, float2(0.0), float2(0.999));

    float3 source;
    if (water.config.w != 0u)
    {
        const float depth = refraction_depth.sample(
            depth_sampler, refracted_uv).r;
        const float3 refracted_position =
            almetal_world_water_depth_position(
                refracted_uv, depth, water.config.x != 0u, field);
        if (input.eye_position.z < refracted_position.z - 0.05)
        {
            refracted_uv = screen_uv;
        }
        source = refraction_color.sample(
            screen_sampler, refracted_uv).rgb;
    }
    else
    {
        source = almetal_world_alpha_water_fog(
            input.eye_position + view_vector * 2048.0,
            float3(1.0), environment);
    }

    const bool classic_mode = lighting.parameters.w != 0.0;
    float3 sunlit = classic_mode
        ? environment.atmos_sunlit.rgb
        : environment.atmos_sunlit_linear.rgb;
    sunlit *= environment.atmos_sunlit.w;

    float shadow = 1.0;
    if (water.config.y != 0u)
    {
        const float2 gl_fragment_position = float2(
            input.position.x, shadows.parameters.w - input.position.y);
        shadow = almetal_directional_shadow(
            shadow_maps, shadows, input.eye_position, normal,
            position_dx, position_dy,
            gl_fragment_position / shadows.resolution.zw,
            lighting.sun_direction.xyz);
    }

    const float glossiness = 1.0 - water.optics.z;
    float3 radiance = almetal_sample_water_environment(
        input.eye_position, wave_ibl, glossiness,
        reflection_radiance, legacy_environment, probe_sampler,
        probes, water);
    const float3 reflection = normalize(
        reflect(input.eye_position, wave_ibl));
    const float3 dominant = almetal_base_dominant_direction(
        wave_ibl, reflection, water.optics.z);
    if (water.environment_config.x == 0u)
    {
        const float4 ssr_sample = almetal_sample_ssr(
            refracted_uv,
            input.eye_position, wave_ibl, dominant, glossiness, false,
            ssr_scene_color, ssr_scene_depth,
            screen_sampler, depth_sampler, ssr);
        radiance = mix(radiance, ssr_sample.rgb, ssr_sample.a);
        radiance = almetal_finish_hero_probe(
            radiance, input.eye_position, wave_ibl, glossiness,
            hero_probe, probe_sampler, probes, hero);
    }

    const float3 view = -view_vector;
    const float glint_alpha =
        water.normal_scale_glint.w * water.normal_scale_glint.w;
    const float glint_exponent = max(
        2.0 / (glint_alpha * glint_alpha) - 2.0, 1.0);
    const float lobe = pow(clamp(
        dot(reflect(-view, sun_normal), light), 0.0, 1.0),
        glint_exponent);
    const float glint = lobe * (glint_exponent + 2.0) / 6.2831853;
    const float view_half = sqrt(max(
        0.5 + 0.5 * dot(view, light), 0.0));
    const float sun_fresnel = 0.02 + 0.98 * pow(
        max(1.0 - view_half, 0.0), 5.0);
    const float3 punctual = min(glint * sun_fresnel, 10.0) *
        sunlit * shadow;
    float glow = lobe *
        (0.1 + 0.2 * max(water.light_direction.z, 0.0)) * shadow;

    radiance *= fresnel2.y;
    float3 color = mix(source, radiance, min(1.0, fresnel2.x)) +
        punctual;

    if (water.water_far.y > 0.0)
    {
        const float2 to_rim = min(
            input.agent_view.xy - water.rim.xy,
            water.rim.zw - input.agent_view.xy);
        const float gone = 1.0 - smoothstep(
            0.0, 0.15 * water.water_far.y,
            min(to_rim.x, to_rim.y));
        if (gone > 0.0)
        {
            float3 sky = almetal_sample_water_environment(
                input.eye_position, up, 1.0,
                reflection_radiance, legacy_environment, probe_sampler,
                probes, water);
            if (water.environment_config.x == 0u)
            {
                const float3 sky_reflection = normalize(
                    reflect(input.eye_position, up));
                const float4 sky_ssr = almetal_sample_ssr(
                    screen_uv,
                    input.eye_position, up, sky_reflection, 1.0, false,
                    ssr_scene_color, ssr_scene_depth,
                    screen_sampler, depth_sampler, ssr);
                sky = mix(sky, sky_ssr.rgb, sky_ssr.a);
                sky = almetal_finish_hero_probe(
                    sky, input.eye_position, up, 1.0,
                    hero_probe, probe_sampler, probes, hero);
            }
            color = mix(color, sky, gone);
            glow *= 1.0 - gone;
        }
    }

    if (mirror_clipped)
    {
        discard_fragment();
    }
    return max(float4(color, glow), float4(0.0));
}

static float almetal_projector_shadow(
    texture2d_array<float> shadow_maps,
    constant ALMetalWorldDirectionalShadowConstants& shadows,
    float3 position, float3 normal, float3 position_dx,
    float3 position_dy, uint index, float2 screen_position)
{
    if (!almetal_shadow_map_built(index, shadows))
    {
        return 1.0;
    }

    const float4 receiver = float4(position, 1.0);
    const float4 receiver_light =
        shadows.shadow_matrix[index] * receiver;
    if (receiver_light.w <= 0.0 ||
        any(receiver_light.xy < float2(0.0)) ||
        any(receiver_light.xy > receiver_light.ww))
    {
        return 1.0;
    }

    const float3 offset_position =
        position + normal * shadows.parameters.y;
    const float4 source = float4(offset_position, 1.0);
    if (source.z <= -shadows.shadow_clip.w)
    {
        return 1.0;
    }

    const float4 source_dx = float4(position_dx, 0.0);
    const float4 source_dy = float4(position_dy, 0.0);
    const float filtered = almetal_filter_directional_shadow(
        shadow_maps, index,
        shadows.shadow_matrix[index] * source,
        shadows.shadow_matrix[index] * source_dx,
        shadows.shadow_matrix[index] * source_dy,
        screen_position, shadows.parameters.x * 0.8, shadows);
    const float to_lit = clamp(
        (position.z + shadows.shadow_clip.z) /
            (shadows.shadow_clip.z - shadows.shadow_clip.w) * 2.0 - 1.0,
        0.0, 1.0);
    return mix(filtered, 1.0, to_lit);
}

static float3 almetal_world_pre_water_simple_local_light(
    ALMetalWorldLegacyAlphaLight light, float3 diffuse,
    float3 normal, float3 position, bool classic_mode)
{
    const float light_size = light.parameters.x;
    if (light_size <= 0.0)
    {
        return 0.0;
    }
    const float3 light_vector = light.position.xyz - position;
    const float light_distance = length(light_vector);
    const float distance = light_distance / light_size;
    if (distance > 1.0 || dot(normal, light_vector) <= 0.0 ||
        light_distance <= 0.0)
    {
        return 0.0;
    }

    const float3 direction = light_vector / light_distance;
    const float nl = clamp(dot(normal, direction), 0.000001, 1.0);
    float distance_attenuation = 1.0 - clamp(
        (distance + light.parameters.y) /
            (1.0 + light.parameters.y),
        0.0, 1.0);
    distance_attenuation *= distance_attenuation * 2.0;
    const float spot = max(
        dot(-light.direction.xyz, direction), light.parameters.z);
    float3 color = light.diffuse.rgb *
        (nl * distance_attenuation) * diffuse * (spot * spot);
    color = max(color, float3(0.0));
    const float peak = max(color.r, max(color.g, color.b));
    if (peak > 65504.0)
    {
        color *= 65504.0 / peak;
    }
    return color * (classic_mode ? 0.9 : 1.0);
}

static void almetal_world_pre_water_simple_surface(
    ALMetalWorldShinyVaryings input,
    array<texture2d<float>, 32> diffuse_textures,
    sampler diffuse_sampler,
    constant ALMetalWorldAlphaEnvironment& environment,
    thread float4& diffuse,
    thread float3& normal)
{
    const float water_side =
        dot(input.eye_position, environment.water_plane.xyz) +
        environment.water_plane.w;
    if ((environment.water_fog.z > 0.0 && water_side < 0.0) ||
        (environment.water_fog.z <= 0.0 && water_side > 0.0))
    {
        discard_fragment();
    }

    diffuse = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord) * input.color
        : float4(1.0, 0.0, 1.0, 1.0);
    if (diffuse.a < environment.water_fog.w)
    {
        discard_fragment();
    }
    normal = normalize(input.normal);
}

static float4 almetal_world_pre_water_simple_lighting(
    ALMetalWorldShinyVaryings input, float4 diffuse, float3 normal,
    float directional_visibility,
    texturecube_array<float> reflection_radiance,
    texture2d<uint> reflection_irradiance,
    texturecube<float> legacy_sky,
    sampler probe_sampler,
    constant ALMetalWorldAlphaEnvironment& environment,
    constant ALMetalWorldReflectionProbeConstants& probes,
    constant ALMetalWorldLegacyAlphaLighting& lighting)
{
    const bool classic_mode = lighting.parameters.w != 0.0;
    const float ambient_facing = 1.0 -
        pow(0.5 * min(abs(dot(
            normal, environment.light_direction.xyz)), 1.0), 2.0);
    float3 sunlit = classic_mode
        ? environment.atmos_sunlit.rgb
        : environment.atmos_sunlit_linear.rgb;
    float3 ambient = environment.atmos_amblit.rgb * ambient_facing;
    if (!classic_mode)
    {
        ambient = almetal_srgb_to_linear(ambient);
        ambient = float3(dot(
            ambient, float3(0.2126, 0.7152, 0.0722)));
    }
    else
    {
        sunlit *= 1.35;
    }
    sunlit *= environment.atmos_sunlit.w;
    ambient *= environment.atmos_sunlit_linear.w;

    float3 irradiance = ambient;
    float3 gloss_environment = 0.0;
    float3 legacy_environment = 0.0;
    if (environment.environment_config.x != 0u)
    {
        irradiance = almetal_legacy_sky_ambient(
            ambient, environment);
    }
    else
    {
        almetal_sample_base_probes(
            false, input.eye_position, normal, 0.0, 0.0,
            ambient, classic_mode,
            reflection_radiance, reflection_irradiance,
            probe_sampler, probes,
            irradiance, gloss_environment, legacy_environment);
    }

    float directional = clamp(dot(
        normal, environment.light_direction.xyz), 0.0, 1.0);
    float3 color = irradiance;
    if (classic_mode)
    {
        directional = pow(directional, 1.2);
        const float3 sun_contribution = float3(
            min(directional, directional_visibility));
        color = almetal_srgb_to_linear(
            color * 0.9 +
            almetal_linear_to_srgb(sun_contribution) *
                sunlit * 0.7);
    }
    else
    {
        color += min(directional, directional_visibility) * sunlit;
    }
    color *= diffuse.rgb;
    if (classic_mode)
    {
        color *= 1.1;
    }

    for (uint light_index = 0; light_index < 7; ++light_index)
    {
        color += almetal_world_pre_water_simple_local_light(
            lighting.local_lights[light_index], diffuse.rgb,
            normal, input.eye_position, classic_mode);
    }
    color = almetal_world_alpha_fog(
        input.eye_position, color, environment);
    return max(float4(color, diffuse.a), float4(0.0));
}

fragment float4 almetal_world_pre_water_simple_fragment(
    ALMetalWorldShinyVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    texturecube_array<float> reflection_radiance [[texture(32)]],
    texture2d<uint> reflection_irradiance [[texture(37)]],
    texturecube<float> legacy_sky [[texture(39)]],
    sampler diffuse_sampler [[sampler(0)]],
    sampler probe_sampler [[sampler(1)]],
    constant ALMetalWorldAlphaEnvironment& environment [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]],
    constant ALMetalWorldLegacyAlphaLighting& lighting [[buffer(2)]])
{
    float4 diffuse;
    float3 normal;
    almetal_world_pre_water_simple_surface(
        input, diffuse_textures, diffuse_sampler,
        environment, diffuse, normal);
    return almetal_world_pre_water_simple_lighting(
        input, diffuse, normal, 1.0,
        reflection_radiance, reflection_irradiance,
        legacy_sky,
        probe_sampler, environment, probes, lighting);
}

fragment float4 almetal_world_pre_water_simple_shadow_fragment(
    ALMetalWorldShinyVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    texturecube_array<float> reflection_radiance [[texture(32)]],
    texture2d<uint> reflection_irradiance [[texture(37)]],
    texture2d_array<float> shadow_maps [[texture(33)]],
    texturecube<float> legacy_sky [[texture(39)]],
    sampler diffuse_sampler [[sampler(0)]],
    sampler probe_sampler [[sampler(1)]],
    constant ALMetalWorldAlphaEnvironment& environment [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]],
    constant ALMetalWorldLegacyAlphaLighting& lighting [[buffer(2)]],
    constant ALMetalWorldDirectionalShadowConstants& shadows [[buffer(3)]])
{
    const float3 position_dx = dfdx(input.eye_position);
    const float3 position_dy = dfdy(input.eye_position);
    float4 diffuse;
    float3 normal;
    almetal_world_pre_water_simple_surface(
        input, diffuse_textures, diffuse_sampler,
        environment, diffuse, normal);
    const float2 gl_fragment_position = float2(
        input.position.x, shadows.parameters.w - input.position.y);
    const float directional_visibility = almetal_directional_shadow(
        shadow_maps, shadows, input.eye_position, normal,
        position_dx, position_dy,
        gl_fragment_position / shadows.resolution.zw,
        environment.light_direction.xyz);
    return almetal_world_pre_water_simple_lighting(
        input, diffuse, normal, directional_visibility,
        reflection_radiance, reflection_irradiance,
        legacy_sky,
        probe_sampler, environment, probes, lighting);
}

fragment float4 almetal_world_pre_water_material_fragment(
    ALMetalWorldLegacyAlphaVaryings input [[stage_in]],
    array<texture2d<float>, 8> diffuse_textures [[texture(0)]],
    array<texture2d<float>, 8> normal_textures [[texture(8)]],
    array<texture2d<float>, 8> specular_textures [[texture(16)]],
    texturecube_array<float> reflection_radiance [[texture(32)]],
    texture2d_array<float> shadow_maps [[texture(33)]],
    texturecube<float> hero_probe [[texture(34)]],
    texture2d<float> ssr_scene_color [[texture(35)]],
    texture2d<float> ssr_scene_depth [[texture(36)]],
    texture2d<uint> reflection_irradiance [[texture(37)]],
    texturecube<float> legacy_sky [[texture(39)]],
    sampler material_sampler [[sampler(0)]],
    sampler probe_sampler [[sampler(1)]],
    sampler ssr_color_sampler [[sampler(2)]],
    sampler ssr_depth_sampler [[sampler(3)]],
    constant ALMetalWorldMaterialConstants& constants [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]],
    constant ALMetalWorldLegacyAlphaLighting& lighting [[buffer(2)]],
    constant ALMetalWorldDirectionalShadowConstants& shadows [[buffer(3)]],
    constant ALMetalWorldHeroProbeConstants& hero [[buffer(4)]],
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr [[buffer(5)]],
    constant ALMetalWorldAlphaEnvironment& environment [[buffer(6)]])
{
    // Receiver and normal derivatives must remain outside all non-uniform
    // alpha and water discards, matching materialF.glsl's shadow and
    // specular-AA contract.
    const float3 position_dx = dfdx(input.eye_position);
    const float3 position_dy = dfdy(input.eye_position);
    const float water_side =
        dot(input.eye_position, environment.water_plane.xyz) +
        environment.water_plane.w;
    if ((environment.water_fog.z > 0.0 && water_side < 0.0) ||
        (environment.water_fog.z <= 0.0 && water_side > 0.0))
    {
        discard_fragment();
    }

    const uint slot_count = max(constants.config.x, 1u);
    const uint material_index = min(input.material_index, slot_count - 1u);
    const uint slot = min(material_index, 7u);
    const ALMetalWorldMaterialSlotConstants material =
        constants.slots[slot];
    float4 diffuse = diffuse_textures[slot].sample(
        material_sampler, input.texcoord);
    diffuse.rgb *= input.color.rgb;

    float glossiness = material.specular_color.a;
    float3 normal = normalize(input.normal);
    if (constants.config.z != 0u)
    {
        const float4 tangent_sample = normal_textures[slot].sample(
            material_sampler, input.texcoord1);
        glossiness *= tangent_sample.a;
        const float3 tangent_normal = tangent_sample.rgb * 2.0 - 1.0;
        normal = normalize(
            tangent_normal.x * input.tangent +
            tangent_normal.y * input.bitangent +
            tangent_normal.z * input.normal);
    }

    float4 specular = float4(
        almetal_srgb_to_linear(material.specular_color.rgb), 1.0);
    if (constants.config.w != 0u)
    {
        const float4 specular_sample = specular_textures[slot].sample(
            material_sampler, input.texcoord2);
        specular = float4(
            specular_sample.rgb * specular.rgb, specular_sample.a);
    }
    const float environment_intensity =
        material.properties.x * specular.a;

    const float3 normal_dx = dfdx(normal);
    const float3 normal_dy = dfdy(normal);
    const float variance = 0.15 *
        (dot(normal_dx, normal_dx) + dot(normal_dy, normal_dy));
    const float specular_kernel = min(2.0 * variance, 0.25);
    const float perceptual_roughness = 1.0 - glossiness;
    const float roughness_alpha =
        perceptual_roughness * perceptual_roughness;
    glossiness = 1.0 - sqrt(sqrt(clamp(
        roughness_alpha * roughness_alpha + specular_kernel,
        0.0, 1.0)));
    specular.a = glossiness;

    const float minimum_alpha = environment.water_fog.w;
    if (input.color.a < minimum_alpha ||
        (diffuse.a * input.color.a < minimum_alpha &&
         glossiness <= 0.0 && environment_intensity <= 0.0))
    {
        discard_fragment();
    }

    float directional_visibility = lighting.parameters.z;
    if ((shadows.config.y & 0xfu) != 0xfu)
    {
        const float2 gl_fragment_position = float2(
            input.position.x, shadows.parameters.w - input.position.y);
        directional_visibility *= almetal_directional_shadow(
            shadow_maps, shadows, input.eye_position, normal,
            position_dx, position_dy,
            gl_fragment_position / shadows.resolution.zw,
            lighting.sun_direction.xyz);
    }

    const bool classic_mode = lighting.parameters.w != 0.0;
    const float ambient_facing = 1.0 -
        pow(0.5 * min(abs(dot(
            normal, environment.light_direction.xyz)), 1.0), 2.0);
    float3 sunlit = classic_mode
        ? environment.atmos_sunlit.rgb
        : environment.atmos_sunlit_linear.rgb;
    float3 ambient = environment.atmos_amblit.rgb * ambient_facing;
    if (!classic_mode)
    {
        ambient = almetal_srgb_to_linear(ambient);
        ambient = float3(dot(
            ambient, float3(0.2126, 0.7152, 0.0722)));
    }
    else
    {
        sunlit *= 1.35;
    }
    sunlit *= environment.atmos_sunlit.w;
    ambient *= environment.atmos_sunlit_linear.w;

    float3 ambient_environment = ambient;
    float3 gloss_environment = 0.0;
    float3 legacy_environment = 0.0;
    const bool legacy_environment_path =
        environment.environment_config.x != 0u;
    if (legacy_environment_path)
    {
        ambient_environment = almetal_legacy_sky_ambient(
            ambient, environment);
        legacy_environment = almetal_sample_legacy_sky(
            input.eye_position, normal, legacy_sky,
            probe_sampler, environment);
        gloss_environment = legacy_environment;
    }
    else
    {
        almetal_sample_base_probes(
            false, input.eye_position, normal, glossiness,
            environment_intensity, ambient, classic_mode,
            reflection_radiance, reflection_irradiance,
            probe_sampler, probes,
            ambient_environment, gloss_environment,
            legacy_environment);
    }
    if (!legacy_environment_path && glossiness > 0.0)
    {
        gloss_environment = almetal_finish_legacy_ssr(
            gloss_environment,
            float2(input.position.x, ssr.screen.z - input.position.y) /
                ssr.screen.xy,
            input.eye_position, normal, glossiness,
            ssr_scene_color, ssr_scene_depth,
            ssr_color_sampler, ssr_depth_sampler, ssr);
    }
    if (!legacy_environment_path && environment_intensity > 0.0)
    {
        legacy_environment = almetal_finish_legacy_ssr(
            legacy_environment,
            float2(input.position.x, ssr.screen.z - input.position.y) /
                ssr.screen.xy,
            input.eye_position, normal, glossiness,
            ssr_scene_color, ssr_scene_depth,
            ssr_color_sampler, ssr_depth_sampler, ssr);
    }
    if (!legacy_environment_path)
    {
        gloss_environment = almetal_finish_hero_probe(
            gloss_environment, input.eye_position, normal, glossiness,
            hero_probe, probe_sampler, probes, hero);
        legacy_environment = almetal_finish_hero_probe(
            legacy_environment, input.eye_position, normal, 1.0,
            hero_probe, probe_sampler, probes, hero);
    }

    float directional = clamp(dot(
        normal, environment.light_direction.xyz), 0.0, 1.0);
    float3 sunlit_linear = sunlit;
    float3 color = ambient_environment;
    if (classic_mode)
    {
        directional = pow(directional, 1.2);
        const float3 sun_contribution = float3(
            min(directional, directional_visibility));
        color = almetal_srgb_to_linear(
            color * 0.9 +
            almetal_linear_to_srgb(sun_contribution) * sunlit * 0.7);
        sunlit_linear = almetal_srgb_to_linear(sunlit);
    }
    else
    {
        color += min(directional, directional_visibility) * sunlit_linear;
    }
    color *= diffuse.rgb;

    float glare = 0.0;
    const float3 view = -normalize(input.eye_position);
    if (glossiness > 0.0)
    {
        float3 half_vector;
        float3 light_direction;
        float nh;
        float nl;
        float nv;
        float vh;
        almetal_legacy_half_vectors(
            lighting.sun_direction.xyz, normal, view,
            half_vector, light_direction, nh, nl, nv, vh);
        if (dot(normal, light_direction) > 0.0 &&
            dot(normal, half_vector) > 0.0)
        {
            const float lit = min(nl * 6.0, 1.0);
            const float fresnel = pow(1.0 - vh, 5.0) * 0.4 + 0.5;
            const float geometry_denominator = 2.0 * nh;
            const float geometry = max(
                0.0, min(geometry_denominator * nv / vh,
                         geometry_denominator * nl / vh));
            const float lobe = directional_visibility * fresnel *
                almetal_legacy_blinn_phong_lobe(
                    nh, glossiness, lighting.parameters.x) *
                geometry / (nh * nl);
            const float3 contribution = lit * lobe *
                sunlit_linear * specular.rgb;
            color += contribution;
            glare = max(max(contribution.r, contribution.g),
                        contribution.b);
        }

        if (!legacy_environment_path)
        {
            gloss_environment *= 0.5;
            float fresnel = clamp(
                1.0 + dot(normalize(input.eye_position), normal),
                0.3, 1.0);
            fresnel *= fresnel * specular.a;
            gloss_environment *= specular.rgb * fresnel;
            gloss_environment *= float3(1.0) - color;
            color += gloss_environment * 0.5;
        }
    }

    const float emissive = material.properties.z;
    color = mix(color, diffuse.rgb, emissive);
    if (environment_intensity > 0.0)
    {
        if (legacy_environment_path)
        {
            color = almetal_srgb_to_linear(mix(
                almetal_linear_to_srgb(color),
                legacy_environment * 2.0,
                environment_intensity));
        }
        else
        {
            float fresnel = 1.0 +
                dot(normalize(input.eye_position), normal);
            fresnel *= fresnel;
            fresnel = min(fresnel + environment_intensity, 1.0);
            const float3 reflected = legacy_environment *
                (environment_intensity * fresnel);
            color = mix(
                color, reflected * 0.5,
                environment_intensity);
        }
        const float environment_glare = max(
            max(legacy_environment.r, legacy_environment.g),
            legacy_environment.b);
        glare += max(environment_glare, 0.0) *
            environment_intensity * 4.0;
    }
    if (classic_mode)
    {
        color *= 1.1;
    }

    for (uint light_index = 0; light_index < 7; ++light_index)
    {
        color += almetal_legacy_local_light(
            lighting.local_lights[light_index], diffuse.rgb,
            normal, input.eye_position, view,
            specular.rgb, glossiness, lighting.parameters.x,
            classic_mode, glare);
    }
    color = almetal_world_alpha_fog(
        input.eye_position, color, environment);

    glare *= 1.0 - emissive;
    glare = min(glare, 1.0);
    const float coverage = max(diffuse.a, glare) * input.color.a;
    if (coverage < minimum_alpha)
    {
        discard_fragment();
    }
    return max(float4(color, coverage), float4(0.0));
}

fragment float4 almetal_world_legacy_alpha_fragment(
    ALMetalWorldLegacyAlphaVaryings input [[stage_in]],
    array<texture2d<float>, 8> diffuse_textures [[texture(0)]],
    array<texture2d<float>, 8> normal_textures [[texture(8)]],
    array<texture2d<float>, 8> specular_textures [[texture(16)]],
    texturecube_array<float> reflection_probes [[texture(32)]],
    texturecube<float> hero_probe [[texture(34)]],
    texture2d<float> ssr_scene_color [[texture(35)]],
    texture2d<float> ssr_scene_depth [[texture(36)]],
    sampler material_sampler [[sampler(0)]],
    sampler probe_sampler [[sampler(1)]],
    sampler ssr_color_sampler [[sampler(2)]],
    sampler ssr_depth_sampler [[sampler(3)]],
    constant ALMetalWorldMaterialConstants& constants [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]],
    constant ALMetalWorldLegacyAlphaLighting& lighting [[buffer(2)]],
    texture2d_array<float> shadow_maps [[texture(33)]],
    constant ALMetalWorldDirectionalShadowConstants& shadows [[buffer(3)]],
    constant ALMetalWorldHeroProbeConstants& hero [[buffer(4)]],
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr [[buffer(5)]])
{
    // Keep the receiver derivatives in uniform control flow, before the
    // material cutoffs below can discard only part of a quad.
    const float3 position_dx = dfdx(input.eye_position);
    const float3 position_dy = dfdy(input.eye_position);
    const uint slot_count = max(constants.config.x, 1u);
    const uint material_index = min(input.material_index, slot_count - 1u);
    const uint slot = min(material_index, 7u);
    const ALMetalWorldMaterialSlotConstants material =
        constants.slots[slot];
    const float4 diffuse = diffuse_textures[slot].sample(
        material_sampler, input.texcoord);

    float3 normal = normalize(input.normal);
    float glossiness = material.specular_color.a;
    if (constants.config.z != 0u)
    {
        const float4 tangent_sample = normal_textures[slot].sample(
            material_sampler, input.texcoord1);
        glossiness *= tangent_sample.a;
        const float3 tangent_normal = tangent_sample.rgb * 2.0 - 1.0;
        normal = normalize(
            tangent_normal.x * input.tangent +
            tangent_normal.y * input.bitangent +
            tangent_normal.z * input.normal);
    }

    float4 specular = float4(
        almetal_srgb_to_linear(material.specular_color.rgb), 1.0);
    if (constants.config.w != 0u)
    {
        const float4 specular_sample = specular_textures[slot].sample(
            material_sampler, input.texcoord2);
        specular = float4(
            specular_sample.rgb * specular.rgb, specular_sample.a);
    }
    const float environment_intensity =
        material.properties.x * specular.a;

    const float3 normal_dx = dfdx(normal);
    const float3 normal_dy = dfdy(normal);
    const float variance = 0.15 *
        (dot(normal_dx, normal_dx) + dot(normal_dy, normal_dy));
    const float specular_kernel = min(2.0 * variance, 0.25);
    const float perceptual_roughness = 1.0 - glossiness;
    const float alpha = perceptual_roughness * perceptual_roughness;
    glossiness = 1.0 - sqrt(sqrt(clamp(
        alpha * alpha + specular_kernel, 0.0, 1.0)));

    float directional_visibility = lighting.parameters.z;
    if ((shadows.config.y & 0xfu) != 0xfu)
    {
        const float2 gl_fragment_position = float2(
            input.position.x,
            shadows.parameters.w - input.position.y);
        const float2 screen_position =
            gl_fragment_position / shadows.resolution.zw;
        directional_visibility *= almetal_directional_shadow(
            shadow_maps, shadows, input.eye_position, normal,
            position_dx, position_dy, screen_position,
            lighting.sun_direction.xyz);
    }

    const float minimum_alpha = lighting.parameters.y;
    if (input.color.a < minimum_alpha ||
        (diffuse.a * input.color.a < minimum_alpha &&
         glossiness <= 0.0 && environment_intensity <= 0.0))
    {
        discard_fragment();
    }

    float glare = 0.0;
    const float3 view = -normalize(input.eye_position);
    if (glossiness > 0.0)
    {
        const float3 light_vector = normalize(lighting.sun_direction.xyz);
        float3 half_vector;
        float3 light_direction;
        float nh;
        float nl;
        float nv;
        float vh;
        almetal_legacy_half_vectors(
            light_vector, normal, view, half_vector, light_direction,
            nh, nl, nv, vh);
        if (dot(normal, light_direction) > 0.0 &&
            dot(normal, half_vector) > 0.0)
        {
            const float lit = min(nl * 6.0, 1.0);
            const float fresnel = pow(1.0 - vh, 5.0) * 0.4 + 0.5;
            const float geometry_denominator = 2.0 * nh;
            const float geometry = max(
                0.0, min(geometry_denominator * nv / vh,
                         geometry_denominator * nl / vh));
            const float lobe = directional_visibility * fresnel *
                almetal_legacy_blinn_phong_lobe(
                    nh, glossiness, lighting.parameters.x) *
                geometry / (nh * nl);
            const float3 contribution =
                lit * lobe * lighting.sun_color.rgb * specular.rgb;
            glare = max(max(contribution.r, contribution.g),
                        contribution.b);
        }
    }

    if (environment_intensity > 0.0)
    {
        float3 legacy_environment = almetal_sample_legacy_probes(
            input.eye_position, normal,
            reflection_probes, probe_sampler, probes);
        const float2 gl_fragment_position = float2(
            input.position.x,
            ssr.screen.z - input.position.y);
        legacy_environment = almetal_finish_legacy_ssr(
            legacy_environment,
            gl_fragment_position / ssr.screen.xy,
            input.eye_position, normal, glossiness,
            ssr_scene_color, ssr_scene_depth,
            ssr_color_sampler, ssr_depth_sampler, ssr);
        legacy_environment = almetal_finish_hero_probe(
            legacy_environment, input.eye_position, normal, 1.0,
            hero_probe, probe_sampler, probes, hero);
        const float environment_glare = max(
            max(legacy_environment.r, legacy_environment.g),
            legacy_environment.b);
        glare += max(environment_glare, 0.0) *
                 environment_intensity * 4.0;
    }

    for (uint light_index = 0; light_index < 7; ++light_index)
    {
        almetal_legacy_local_glare(
            lighting.local_lights[light_index], normal,
            input.eye_position, view, specular.rgb, glossiness,
            lighting.parameters.x, glare);
    }

    glare *= 1.0 - material.properties.z;
    glare = min(glare, 1.0);
    const float coverage = max(diffuse.a, glare) * input.color.a;
    if (coverage < minimum_alpha)
    {
        discard_fragment();
    }
    const float3 flat_color = diffuse.rgb * input.color.rgb;
    return max(float4(flat_color, coverage), float4(0.0));
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
    float4 mirror_clip_plane;
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

struct ALMetalWorldTerrainSpotShadowConstants
{
    float4x4 modelview_projection;
    float4 surface;
    uint4 config;
};

[[patch(quad, 4)]]
vertex ALMetalWorldSpotShadowVaryings
almetal_world_terrain_spot_shadow_vertex(
    const device float4* control_points [[buffer(0)]],
    constant ALMetalWorldTerrainSpotShadowConstants& constants [[buffer(1)]],
    texture2d<float> height_map [[texture(0)]],
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
    const float2 grid = xy / constants.surface.x;
    const float height = constants.config.y != 0u
        ? almetal_world_terrain_height_smooth(height_map, grid).height
        : almetal_world_terrain_height_linear(height_map, grid);
    float4 clip_position = constants.modelview_projection *
                           float4(xy, height, 1.0);
    ALMetalWorldSpotShadowVaryings output;
    output.position = almetal_shadow_clip_from_gl(
        clip_position, constants.config.x);
    output.texcoord = 0.0;
    output.vertex_alpha = 1.0;
    output.texture_index = 0u;
    return output;
}

struct ALMetalWorldTerrainVaryings
{
    float4 position [[position]];
    float mirror_clip_distance [[clip_distance]];
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
    output.mirror_clip_distance = dot(
        float4(output.view_position, 1.0), constants.mirror_clip_plane);
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
    float4 eye_position,
    float3 eye_normal,
    float2 texcoord,
    constant ALMetalWorldSimpleConstants& constants)
{
    ALMetalWorldSimpleVaryings output;
    clip_position.y = -clip_position.y;
    output.position = constants.depth_convention.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.depth_convention.z != 0u
        ? dot(eye_position, constants.mirror_clip_plane) : 1.0;
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
    const float4 object_position =
        float4(positions[vertex_id].xyz, 1.0);
    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    return almetal_world_avatar_varyings(
        constants.modelview_projection * object_position,
        constants.modelview * object_position,
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
        skinned.position,
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
        skinned.position,
        skinned.normal.xyz, texcoords[vertex_id], constants);
}

static ALMetalWorldShinyVaryings almetal_world_avatar_alpha_varyings(
    float4 clip_position,
    float3 eye_position,
    float3 eye_normal,
    float2 texcoord,
    constant ALMetalWorldShinyConstants& constants)
{
    clip_position.y = -clip_position.y;
    ALMetalWorldShinyVaryings output;
    output.position = constants.config.x != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.mirror_clip_distance = constants.config.z != 0u
        ? dot(float4(eye_position, 1.0),
              constants.mirror_clip_plane) : 1.0;
    output.eye_position = eye_position;
    output.normal = normalize(eye_normal);
    output.color = 1.0;
    output.texcoord = texcoord;
    output.texture_index = 0;
    output.texture_count = constants.config.y;
    return output;
}

vertex ALMetalWorldShinyVaryings almetal_world_avatar_alpha_rigid_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    constant ALMetalWorldShinyConstants& constants [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    const float4 position = float4(positions[vertex_id].xyz, 1.0);
    const float3x3 normal_matrix = float3x3(
        constants.normal_matrix[0].xyz,
        constants.normal_matrix[1].xyz,
        constants.normal_matrix[2].xyz);
    return almetal_world_avatar_alpha_varyings(
        constants.modelview_projection * position,
        (constants.modelview * position).xyz,
        normal_matrix * normals[vertex_id].xyz,
        texcoords[vertex_id], constants);
}

vertex ALMetalWorldShinyVaryings almetal_world_avatar_alpha_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    constant ALMetalWorldShinyConstants& constants [[buffer(4)]],
    const device float4* skin_palette [[buffer(5)]],
    const device float* encoded_weights [[buffer(6)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalLitPreviewSkinnedVertex skinned =
        almetal_lit_preview_avatar_skin_vertex(
            positions[vertex_id], normals[vertex_id],
            encoded_weights[vertex_id], skin_palette);
    return almetal_world_avatar_alpha_varyings(
        constants.modelview_projection * skinned.position,
        skinned.position.xyz, skinned.normal.xyz,
        texcoords[vertex_id], constants);
}

vertex ALMetalWorldShinyVaryings
almetal_world_avatar_alpha_cloth_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float4* normals [[buffer(1)]],
    const device float2* texcoords [[buffer(2)]],
    constant ALMetalWorldShinyConstants& constants [[buffer(4)]],
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
    return almetal_world_avatar_alpha_varyings(
        constants.modelview_projection * skinned.position,
        skinned.position.xyz, skinned.normal.xyz,
        texcoords[vertex_id], constants);
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

struct ALMetalWorldHeroProbeFilterConstants
{
    uint mip_level;
    uint max_probe_lod;
    uint resolution;
    uint sample_scale;
    float probe_strength;
    packed_float3 padding;
};

struct ALMetalWorldProbeGaussianConstants
{
    float2 direction;
    float2 padding;
};

kernel void almetal_world_probe_gaussian(
    texture2d<float> source [[texture(0)]],
    texture2d<float, access::write> destination [[texture(1)]],
    constant ALMetalWorldProbeGaussianConstants& constants [[buffer(0)]],
    uint2 position [[thread_position_in_grid]])
{
    if (position.x >= destination.get_width() ||
        position.y >= destination.get_height())
    {
        return;
    }
    constexpr sampler linear_sampler(
        coord::normalized, filter::linear, address::clamp_to_edge);
    constexpr float weights[9] = {
        0.0002, 0.0060, 0.0606, 0.2417, 0.3829,
        0.2417, 0.0606, 0.0060, 0.0002,
    };
    const float2 uv = (float2(position) + 0.5) /
        float2(destination.get_width(), destination.get_height());
    float3 color = 0.0;
    for (int tap = 0; tap < 9; ++tap)
    {
        color += source.sample(
            linear_sampler,
            uv + float(tap - 4) * constants.direction).rgb *
            weights[tap];
    }
    destination.write(max(float4(color, 0.0), float4(0.0)), position);
}

kernel void almetal_world_probe_downsample(
    texture2d<float> source [[texture(0)]],
    texture2d<float, access::write> destination [[texture(1)]],
    uint2 position [[thread_position_in_grid]])
{
    if (position.x >= destination.get_width() ||
        position.y >= destination.get_height())
    {
        return;
    }
    constexpr sampler linear_sampler(
        coord::normalized, filter::linear, address::clamp_to_edge);
    const float2 uv = (float2(position) + 0.5) /
        float2(destination.get_width(), destination.get_height());
    destination.write(source.sample(linear_sampler, uv), position);
}

static float almetal_world_hero_random(float2 value)
{
    const float dot_value = dot(value, float2(12.9898, 78.233));
    return fract(sin(fmod(dot_value, 3.14)) * 43758.5453);
}

static float3 almetal_world_cube_direction(uint face, float2 uv)
{
    const float2 st = uv * 2.0 - 1.0;
    switch (face)
    {
        case 0: return normalize(float3( 1.0, -st.y, -st.x));
        case 1: return normalize(float3(-1.0, -st.y,  st.x));
        case 2: return normalize(float3( st.x,  1.0,  st.y));
        case 3: return normalize(float3( st.x, -1.0, -st.y));
        case 4: return normalize(float3( st.x, -st.y,  1.0));
        default:return normalize(float3(-st.x, -st.y, -1.0));
    }
}

struct ALMetalWorldProbeSHConstants
{
    uint source_mip;
    uint source_resolution;
    uint destination_row;
    uint padding;
};

static float almetal_world_probe_area_element(float x, float y)
{
    return atan2(x * y, sqrt(x * x + y * y + 1.0));
}

static float almetal_world_probe_solid_angle(
    float u, float v, float inverse_resolution)
{
    const float x0 = u - inverse_resolution;
    const float y0 = v - inverse_resolution;
    const float x1 = u + inverse_resolution;
    const float y1 = v + inverse_resolution;
    return almetal_world_probe_area_element(x1, y1) -
           almetal_world_probe_area_element(x0, y1) -
           almetal_world_probe_area_element(x1, y0) +
           almetal_world_probe_area_element(x0, y0);
}

static float almetal_world_probe_sh_basis(uint coefficient, float3 direction)
{
    switch (coefficient)
    {
        case 0: return 0.282095;
        case 1: return 0.488603 * direction.y;
        case 2: return 0.488603 * direction.z;
        case 3: return 0.488603 * direction.x;
        case 4: return 1.092548 * direction.x * direction.y;
        case 5: return 1.092548 * direction.y * direction.z;
        case 6: return 0.315392 *
                       (3.0 * direction.z * direction.z - 1.0);
        case 7: return 1.092548 * direction.x * direction.z;
        default:return 0.546274 *
                       (direction.x * direction.x -
                        direction.y * direction.y);
    }
}

kernel void almetal_world_probe_sh_rows(
    texturecube<float> source [[texture(0)]],
    texture2d<float, access::write> partial [[texture(1)]],
    constant ALMetalWorldProbeSHConstants& constants [[buffer(0)]],
    uint2 position [[thread_position_in_grid]])
{
    const uint row_count = 6u * constants.source_resolution;
    if (position.x >= 9u || position.y >= row_count)
    {
        return;
    }
    constexpr sampler cube_sampler(
        coord::normalized, filter::linear, mip_filter::linear,
        address::clamp_to_edge);
    const uint face = position.y / constants.source_resolution;
    const uint y = position.y - face * constants.source_resolution;
    const float inverse_resolution =
        1.0 / float(constants.source_resolution);
    float3 sum = 0.0;
    for (uint x = 0u; x < constants.source_resolution; ++x)
    {
        const float u =
            2.0 * (float(x) + 0.5) * inverse_resolution - 1.0;
        const float v =
            2.0 * (float(y) + 0.5) * inverse_resolution - 1.0;
        const float2 uv = float2(u, v) * 0.5 + 0.5;
        const float3 direction = almetal_world_cube_direction(face, uv);
        const float solid_angle = almetal_world_probe_solid_angle(
            u, v, inverse_resolution);
        const float3 radiance = source.sample(
            cube_sampler, direction, level(float(constants.source_mip))).rgb;
        sum += radiance *
            almetal_world_probe_sh_basis(position.x, direction) *
            solid_angle;
    }
    partial.write(float4(sum, 1.0), position);
}

static uint almetal_world_probe_pack_halves(float2 values)
{
    const ushort2 bits = as_type<ushort2>(half2(values));
    return uint(bits.x) | (uint(bits.y) << 16u);
}

kernel void almetal_world_probe_sh_reduce(
    texture2d<float, access::read> partial [[texture(0)]],
    texture2d<uint, access::write> destination [[texture(1)]],
    constant ALMetalWorldProbeSHConstants& constants [[buffer(0)]],
    device uint4* packed_words [[buffer(1)]],
    uint position [[thread_position_in_grid]])
{
    if (position >= 4u)
    {
        return;
    }
    const uint first = (8u * position) / 3u;
    const uint last = min((8u * position + 7u) / 3u, 8u);
    float3 sums[4] = {
        float3(0.0), float3(0.0), float3(0.0), float3(0.0),
    };
    const uint rows = 6u * constants.source_resolution;
    for (uint coefficient = first; coefficient <= last; ++coefficient)
    {
        for (uint row = 0u; row < rows; ++row)
        {
            sums[coefficient - first] +=
                partial.read(uint2(coefficient, row)).rgb;
        }
    }

    float values[8];
    for (uint index = 0u; index < 8u; ++index)
    {
        const uint packed_index = 8u * position + index;
        const uint coefficient = packed_index / 3u;
        values[index] = coefficient < 9u
            ? sums[coefficient - first][packed_index - 3u * coefficient]
            : 0.0;
    }
    const uint4 packed = uint4(
        almetal_world_probe_pack_halves(float2(values[0], values[1])),
        almetal_world_probe_pack_halves(float2(values[2], values[3])),
        almetal_world_probe_pack_halves(float2(values[4], values[5])),
        almetal_world_probe_pack_halves(float2(values[6], values[7])));
    destination.write(packed, uint2(position, constants.destination_row));
    packed_words[position] = packed;
}

kernel void almetal_world_hero_probe_filter(
    texturecube<float> source [[texture(0)]],
    texturecube<float, access::write> destination [[texture(1)]],
    constant ALMetalWorldHeroProbeFilterConstants& constants [[buffer(0)]],
    uint3 position [[thread_position_in_grid]])
{
    if (position.x >= constants.resolution ||
        position.y >= constants.resolution || position.z >= 6u)
    {
        return;
    }

    constexpr sampler cube_sampler(
        coord::normalized, filter::linear, mip_filter::linear,
        address::clamp_to_edge);
    const float2 uv = (float2(position.xy) + 0.5) /
                      float(constants.resolution);
    const float3 normal = almetal_world_cube_direction(position.z, uv);
    const float3 view = normal;
    const float roughness = constants.max_probe_lod == 0u
        ? 0.0 : float(constants.mip_level) /
                    float(constants.max_probe_lod);
    const uint sample_count = uint(max(
        float(constants.sample_scale) * roughness, 1.0));
    const float environment_dimension = float(source.get_width(0));
    float4 color = 0.0;
    float total_weight = 0.0;
    for (uint index = 0u; index < sample_count; ++index)
    {
        const float2 xi = almetal_pbr_hammersley(index, sample_count);
        const float alpha = roughness * roughness;
        const float phi = 2.0 * M_PI_F * xi.x +
            almetal_world_hero_random(normal.xz) * 0.1;
        const float cos_theta = sqrt((1.0 - xi.y) /
            (1.0 + (alpha * alpha - 1.0) * xi.y));
        const float sin_theta = sqrt(max(
            1.0 - cos_theta * cos_theta, 0.0));
        const float3 tangent_half = float3(
            sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
        const float3 up = abs(normal.z) < 0.999
            ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
        const float3 tangent_x = normalize(cross(up, normal));
        const float3 tangent_y = normalize(cross(normal, tangent_x));
        const float3 half_vector = normalize(
            tangent_x * tangent_half.x + tangent_y * tangent_half.y +
            normal * tangent_half.z);
        const float3 light = 2.0 * dot(view, half_vector) * half_vector -
                             view;
        const float nl = clamp(dot(normal, light), 0.0, 1.0);
        if (nl > 0.0)
        {
            const float nh = clamp(dot(normal, half_vector), 0.0, 1.0);
            const float vh = clamp(dot(view, half_vector), 0.0, 1.0);
            const float alpha2 = alpha * alpha;
            const float denominator = nh * nh * (alpha2 - 1.0) + 1.0;
            const float distribution = alpha2 /
                (M_PI_F * denominator * denominator);
            const float pdf = distribution * nh /
                (4.0 * vh) + 0.0001;
            const float sample_solid_angle = 1.0 /
                (float(sample_count) * pdf);
            const float texel_solid_angle = 4.0 * M_PI_F /
                (6.0 * environment_dimension * environment_dimension);
            const float sample_lod = roughness == 0.0 ? 0.0 : clamp(
                0.5 * log2(sample_solid_angle / texel_solid_angle) + 1.0,
                0.0, float(constants.max_probe_lod));
            color += source.sample(
                cube_sampler, light, level(sample_lod)) * nl;
            total_weight += nl;
        }
    }
    color = max(color / max(total_weight, 0.000001), float4(0.0));
    color.a *= constants.probe_strength;
    destination.write(
        color, position.xy, position.z, constants.mip_level);
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

vertex ALMetalWorldLocalLightVaryings almetal_world_local_light_vertex(
    uint vertex_id [[vertex_id]])
{
    const float2 texcoord = float2((vertex_id << 1) & 2, vertex_id & 2);
    ALMetalWorldLocalLightVaryings output;
    output.position = float4(texcoord.x * 2.0 - 1.0,
                             1.0 - texcoord.y * 2.0,
                             0.0, 1.0);
    output.texcoord = texcoord;
    return output;
}

static float3 almetal_world_bounded_radiance(float3 color)
{
    color = select(color, float3(65504.0), isinf(color));
    color = select(color, float3(0.0), isnan(color));
    return almetal_pbr_clamp_radiance(color);
}

fragment float4 almetal_world_local_light_fragment(
    ALMetalWorldLocalLightVaryings input [[stage_in]],
    const device ALMetalWorldLocalLight* lights [[buffer(0)]],
    constant ALMetalWorldLocalLightingConstants& constants [[buffer(1)]],
    texture2d<float> albedo_texture [[texture(0)]],
    texture2d<float> specular_texture [[texture(1)]],
    texture2d<float> normal_texture [[texture(2)]],
    texture2d<float> depth_texture [[texture(3)]],
    texture2d<float> brdf_lut [[texture(4)]],
    sampler point_sampler [[sampler(0)]],
    sampler brdf_sampler [[sampler(1)]])
{
    const float depth = depth_texture.sample(
        point_sampler, input.texcoord).r;
    const bool reverse_z = constants.config.y != 0u;
    if ((reverse_z && depth <= 0.0) || (!reverse_z && depth >= 1.0))
    {
        discard_fragment();
    }

    const float ndc_z = reverse_z ? depth : depth * 2.0 - 1.0;
    float4 position = constants.inverse_projection *
        float4(input.texcoord * 2.0 - 1.0, ndc_z, 1.0);
    position.w = max(position.w, 0.000001);
    position /= position.w;

    const float4 normal_sample = normal_texture.sample(
        point_sampler, input.texcoord);
    const float3 normal = almetal_world_oct_decode(normal_sample.xy);
    const float3 base_color = albedo_texture.sample(
        point_sampler, input.texcoord).rgb;
    float4 specular = specular_texture.sample(
        point_sampler, input.texcoord);
    const bool has_pbr = abs(normal_sample.a - 0.67) < 0.1;
    const float3 view = -normalize(position.xyz);
    float3 final_color = 0.0;

    if (has_pbr)
    {
        const float perceptual_roughness =
            specular.g + specular.a / 255.0;
        const float metallic = specular.b;
        const float3 diffuse_color = base_color * (1.0 - metallic);
        const float3 specular_color = mix(
            float3(0.04), base_color, metallic);
        const float3 energy_compensation =
            almetal_pbr_energy_compensation(
                brdf_lut, brdf_sampler, specular_color,
                perceptual_roughness, dot(normal, view));

        for (uint index = 0u; index < constants.config.x; ++index)
        {
            const device ALMetalWorldLocalLight& light = lights[index];
            const float3 light_vector =
                light.position_radius.xyz - position.xyz;
            const float light_distance = length(light_vector);
            const float normalized_distance =
                light_distance / light.position_radius.w;
            if (normalized_distance <= 1.0)
            {
                const float3 light_direction = normalize(light_vector);
                const float distance_attenuation =
                    almetal_legacy_distance_attenuation(
                        normalized_distance, light.color_falloff.w);
                const ALMetalPBRPreviewPunctual punctual =
                    almetal_pbr_punctual(
                        diffuse_color, specular_color,
                        perceptual_roughness, normal, view,
                        light_direction);
                const float3 lobe = punctual.nl *
                    (punctual.diffuse +
                     punctual.specular * energy_compensation);
                final_color +=
                    distance_attenuation * light.color_falloff.rgb * 3.0 *
                    almetal_world_bounded_radiance(lobe);
            }
        }
    }
    else
    {
        specular.rgb = almetal_srgb_to_linear(specular.rgb);
        for (uint index = 0u; index < constants.config.x; ++index)
        {
            const device ALMetalWorldLocalLight& light = lights[index];
            const float3 light_vector =
                light.position_radius.xyz - position.xyz;
            const float light_distance = length(light_vector);
            const float normalized_distance =
                light_distance / light.position_radius.w;
            if (normalized_distance > 1.0 ||
                dot(normal, light_vector) <= 0.0)
            {
                continue;
            }

            float3 half_vector;
            float3 light_direction;
            float nh;
            float nl;
            float nv;
            float vh;
            almetal_legacy_half_vectors(
                light_vector, normal, view, half_vector,
                light_direction, nh, nl, nv, vh);
            const float distance_attenuation =
                almetal_legacy_distance_attenuation(
                    normalized_distance, light.color_falloff.w);
            float lit = nl * distance_attenuation;
            float3 contribution =
                light.color_falloff.rgb * lit * base_color;
            if (specular.a > 0.0)
            {
                lit = min(nl * 6.0, 1.0) * distance_attenuation;
                const float fresnel =
                    pow(1.0 - vh, 5.0) * 0.4 + 0.5;
                const float geometry_denominator = 2.0 * nh;
                const float geometry = max(
                    0.0, min(geometry_denominator * nv / vh,
                             geometry_denominator * nl / vh));
                if (dot(normal, half_vector) > 0.0)
                {
                    const float lobe = fresnel *
                        almetal_legacy_blinn_phong_lobe(
                            nh, specular.a, constants.surface.x) *
                        geometry / (nh * nl);
                    contribution += lit * lobe *
                        light.color_falloff.rgb * specular.rgb;
                }
            }
            final_color += almetal_world_bounded_radiance(contribution);
        }
    }

    return float4(max(final_color * constants.surface.y, float3(0.0)), 0.0);
}

static float3 almetal_world_pre_water_pbr_local_light(
    ALMetalWorldLegacyAlphaLight light,
    float3 diffuse_color, float3 specular_color,
    float perceptual_roughness, float3 normal,
    float3 position, float3 view,
    texture2d<float> brdf_lut, sampler brdf_sampler,
    bool classic_mode)
{
    const float light_size = light.parameters.x;
    if (light_size <= 0.0)
    {
        return 0.0;
    }
    const float3 light_vector = light.position.xyz - position;
    const float light_distance = length(light_vector);
    const float distance = light_distance / light_size;
    if (distance > 1.0 || light_distance <= 0.0)
    {
        return 0.0;
    }

    const float3 light_direction = light_vector / light_distance;
    const float distance_attenuation =
        almetal_legacy_distance_attenuation(
            distance, light.parameters.y);
    const float spot = max(
        dot(-light.direction.xyz, light_direction), light.parameters.z);
    const float intensity = spot * spot * distance_attenuation;
    const ALMetalPBRPreviewPunctual punctual = almetal_pbr_punctual(
        diffuse_color, specular_color, perceptual_roughness,
        normal, view, light_direction);
    const float nv = dot(normal, view);
    const float3 energy_compensation = almetal_pbr_energy_compensation(
        brdf_lut, brdf_sampler, specular_color,
        perceptual_roughness, nv);
    const float3 lobe = punctual.nl *
        (punctual.diffuse + punctual.specular * energy_compensation);
    const float3 color = intensity * light.diffuse.rgb * 3.0 *
        almetal_world_bounded_radiance(lobe);
    return color * (classic_mode ? 0.9 : 1.0);
}

fragment float4 almetal_world_pre_water_pbr_fragment(
    ALMetalWorldPBRVaryings input [[stage_in]],
    array<texture2d<float>, 8> base_color_textures [[texture(0)]],
    array<texture2d<float>, 8> normal_textures [[texture(8)]],
    array<texture2d<float>, 8> orm_textures [[texture(16)]],
    array<texture2d<float>, 8> emissive_textures [[texture(24)]],
    texturecube_array<float> reflection_radiance [[texture(32)]],
    texture2d_array<float> shadow_maps [[texture(33)]],
    texturecube<float> hero_probe [[texture(34)]],
    texture2d<float> ssr_scene_color [[texture(35)]],
    texture2d<float> ssr_scene_depth [[texture(36)]],
    texture2d<uint> reflection_irradiance [[texture(37)]],
    texture2d<float> brdf_lut [[texture(38)]],
    texturecube<float> legacy_sky [[texture(39)]],
    sampler material_sampler [[sampler(0)]],
    sampler probe_sampler [[sampler(1)]],
    sampler linear_sampler [[sampler(2)]],
    sampler depth_sampler [[sampler(3)]],
    constant ALMetalWorldPBRConstants& constants [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]],
    constant ALMetalWorldLegacyAlphaLighting& lighting [[buffer(2)]],
    constant ALMetalWorldDirectionalShadowConstants& shadows [[buffer(3)]],
    constant ALMetalWorldHeroProbeConstants& hero [[buffer(4)]],
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr [[buffer(5)]],
    constant ALMetalWorldAlphaEnvironment& environment [[buffer(6)]],
    bool front_facing [[front_facing]])
{
    // Keep receiver and normal derivatives in uniform control flow before
    // water or coverage can discard only part of a quad.
    const float3 position_dx = dfdx(input.eye_position);
    const float3 position_dy = dfdy(input.eye_position);
    const float water_side =
        dot(input.eye_position, environment.water_plane.xyz) +
        environment.water_plane.w;
    if ((environment.water_fog.z > 0.0 && water_side < 0.0) ||
        (environment.water_fog.z <= 0.0 && water_side > 0.0))
    {
        discard_fragment();
    }

    const uint slot_count = max(constants.config.x, 1u);
    const uint slot = min(input.material_index,
                          min(slot_count - 1u, 7u));
    const ALMetalWorldPBRSlotConstants material = constants.slots[slot];
    const float4 base_color = base_color_textures[slot].sample(
        material_sampler, input.base_color_texcoord) * input.color;
    const float3 tangent_normal = normal_textures[slot].sample(
        material_sampler, input.normal_texcoord).rgb * 2.0 - 1.0;
    const float facing = front_facing ? 1.0 : -1.0;
    float3 normal = normalize(
        tangent_normal.x * input.tangent +
        tangent_normal.y * input.bitangent +
        tangent_normal.z * input.normal) * facing;
    const float3 geometric_normal = normalize(input.normal) * facing;
    const float3 orm_sample = orm_textures[slot].sample(
        material_sampler, input.orm_texcoord).rgb;
    const float ao = orm_sample.r;
    float perceptual_roughness =
        orm_sample.g * material.properties.x;
    const float metallic = orm_sample.b * material.properties.y;
    perceptual_roughness = almetal_pbr_filtered_roughness(
        perceptual_roughness, normal);
    const float3 emissive = material.emissive_color.rgb *
        emissive_textures[slot].sample(
            material_sampler, input.emissive_texcoord).rgb;

    const float coverage = base_color.a;
    if (coverage < environment.water_fog.w)
    {
        discard_fragment();
    }

    float directional_visibility = lighting.parameters.z;
    if ((shadows.config.y & 0xfu) != 0xfu)
    {
        const float2 gl_fragment_position = float2(
            input.position.x, shadows.parameters.w - input.position.y);
        directional_visibility *= almetal_directional_shadow(
            shadow_maps, shadows, input.eye_position, normal,
            position_dx, position_dy,
            gl_fragment_position / shadows.resolution.zw,
            lighting.sun_direction.xyz);
    }

    const bool classic_mode = lighting.parameters.w != 0.0;
    const float ambient_facing = 1.0 -
        pow(0.5 * min(abs(dot(
            normal, environment.light_direction.xyz)), 1.0), 2.0);
    float3 sunlit = classic_mode
        ? environment.atmos_sunlit.rgb
        : environment.atmos_sunlit_linear.rgb;
    float3 ambient = environment.atmos_amblit.rgb * ambient_facing;
    if (!classic_mode)
    {
        ambient = almetal_srgb_to_linear(ambient);
        ambient = float3(dot(
            ambient, float3(0.2126, 0.7152, 0.0722)));
    }
    else
    {
        sunlit *= 1.35;
    }
    sunlit *= environment.atmos_sunlit.w;
    ambient *= environment.atmos_sunlit_linear.w;

    const float glossiness = 1.0 - perceptual_roughness;
    float3 irradiance = ambient;
    float3 radiance = 0.0;
    float3 unused_legacy = 0.0;
    const bool legacy_environment_path =
        environment.environment_config.x != 0u;
    if (legacy_environment_path)
    {
        irradiance = almetal_legacy_sky_ambient(
            ambient, environment);
        radiance = almetal_srgb_to_linear(
            almetal_sample_legacy_sky(
                input.eye_position, normal, legacy_sky,
                probe_sampler, environment));
    }
    else
    {
        almetal_sample_base_probes(
            true, input.eye_position, normal, glossiness, 0.0,
            ambient, classic_mode,
            reflection_radiance, reflection_irradiance,
            probe_sampler, probes,
            irradiance, radiance, unused_legacy);
    }
    const float3 mirror_direction = normalize(
        reflect(input.eye_position, normal));
    const float3 dominant_direction = almetal_base_dominant_direction(
        normal, mirror_direction, perceptual_roughness);
    const float2 gl_fragment_position = float2(
        input.position.x, ssr.screen.z - input.position.y);
    if (!legacy_environment_path)
    {
        const float4 ssr_sample = almetal_sample_ssr(
            gl_fragment_position / ssr.screen.xy,
            input.eye_position, normal, dominant_direction,
            glossiness, true,
            ssr_scene_color, ssr_scene_depth,
            linear_sampler, depth_sampler, ssr);
        radiance = mix(radiance, ssr_sample.rgb, ssr_sample.a);
        radiance = almetal_finish_hero_probe(
            radiance, input.eye_position, normal, glossiness,
            hero_probe, probe_sampler, probes, hero);
    }
    radiance *= almetal_pbr_horizon_occlusion(
        reflect(normalize(input.eye_position), normal),
        geometric_normal);

    const float3 diffuse_color = base_color.rgb * (1.0 - metallic);
    const float3 specular_color = mix(
        float3(0.04), base_color.rgb, metallic);
    const float3 view = -normalize(input.eye_position);
    const float nv = clamp(abs(dot(normal, view)), 0.001, 1.0);
    const float roughness = max(perceptual_roughness, 0.045);
    const ALMetalPBRPreviewIBL ibl = almetal_pbr_ibl(
        brdf_lut, linear_sampler, diffuse_color, specular_color,
        radiance, irradiance, ao, nv, roughness);
    const float alpha = perceptual_roughness * perceptual_roughness;
    const float sun_roughness = sqrt(clamp(
        alpha + 0.00465, 0.0, 1.0));
    const float3 light_direction = normalize(
        lighting.sun_direction.xyz);
    ALMetalPBRPreviewPunctual punctual = almetal_pbr_punctual(
        diffuse_color, specular_color, sun_roughness,
        normal, view, light_direction);
    punctual.specular *= almetal_pbr_energy_compensation(
        brdf_lut, linear_sampler, specular_color, roughness, nv);

    float3 color;
    if (classic_mode)
    {
        const float3 classic_irradiance = almetal_srgb_to_linear(
            irradiance * 0.9);
        const float directional = pow(punctual.nl, 1.2);
        float3 sun_contribution = float3(
            min(directional, directional_visibility));
        sun_contribution = almetal_srgb_to_linear(
            almetal_linear_to_srgb(sun_contribution) *
            sunlit * 0.7) * M_PI_F;
        const float3 final_ambient = classic_irradiance *
            diffuse_color *
            almetal_pbr_gtao_multibounce(ao, diffuse_color);
        const float3 final_sun = almetal_world_bounded_radiance(
            sun_contribution *
            ((punctual.diffuse + punctual.specular) *
             directional_visibility));
        color = almetal_srgb_to_linear(
            almetal_linear_to_srgb(final_ambient) +
            almetal_linear_to_srgb(final_sun) * 1.1);
    }
    else
    {
        color = ibl.diffuse +
            almetal_world_bounded_radiance(
                punctual.nl *
                (punctual.diffuse + punctual.specular)) *
            sunlit * 3.0 * directional_visibility;
    }
    color += ibl.specular + emissive;
    if (classic_mode)
    {
        color *= 1.1;
    }

    for (uint light_index = 0; light_index < 7; ++light_index)
    {
        color += almetal_world_pre_water_pbr_local_light(
            lighting.local_lights[light_index],
            diffuse_color, specular_color, perceptual_roughness,
            normal, input.eye_position, view,
            brdf_lut, linear_sampler, classic_mode);
    }
    color = almetal_world_alpha_fog(
        input.eye_position, color, environment);
    color = almetal_world_bounded_radiance(color);
    return max(float4(color, coverage), float4(0.0));
}

static float4 almetal_projector_diffuse_cookie(
    texture2d<float> cookie,
    sampler cookie_sampler,
    float2 texcoord,
    float lod,
    float lod_range)
{
    float4 color = cookie.sample(
        cookie_sampler, texcoord, level(lod));
    const float2 distance_to_edge =
        float2(0.5) - abs(texcoord - float2(0.5));
    const float detail = min(lod / (lod_range * 0.5), 1.0);
    const float edge = 0.25 * detail;
    color *= clamp(min(distance_to_edge.x, distance_to_edge.y) /
                       max(edge, 1.0e-6),
                   0.0, 1.0);
    return color;
}

static float4 almetal_projector_ambient_cookie(
    texture2d<float> cookie,
    sampler cookie_sampler,
    float2 texcoord,
    float lod_range)
{
    float4 color = cookie.sample(
        cookie_sampler, texcoord, level(lod_range));
    const float2 delta = texcoord - float2(0.5);
    const float radial = dot(delta, delta);
    color *= clamp((0.25 - radial) / 0.25, 0.0, 1.0);
    return color;
}

static float4 almetal_projector_specular_cookie(
    texture2d<float> cookie,
    sampler cookie_sampler,
    float2 texcoord,
    float lod,
    float lod_range)
{
    float4 color = cookie.sample(
        cookie_sampler, texcoord, level(lod));
    const float2 distance_to_edge =
        float2(0.5) - abs(texcoord - float2(0.5));
    const float detail = min(lod / (lod_range * 0.5), 1.0);
    float distance = min(distance_to_edge.x, distance_to_edge.y);
    distance *= min(1.0, distance * (lod_range - lod));
    const float edge = 0.25 * detail;
    color *= clamp(distance / max(edge, 1.0e-6), 0.0, 1.0);
    return color;
}

fragment float4 almetal_world_projector_light_fragment(
    ALMetalWorldLocalLightVaryings input [[stage_in]],
    constant ALMetalWorldProjectorLightingConstants& constants [[buffer(0)]],
    constant ALMetalWorldDirectionalShadowConstants& shadows [[buffer(1)]],
    texture2d<float> albedo_texture [[texture(0)]],
    texture2d<float> specular_texture [[texture(1)]],
    texture2d<float> normal_texture [[texture(2)]],
    texture2d<float> depth_texture [[texture(3)]],
    texture2d<float> brdf_lut [[texture(4)]],
    texture2d<float> cookie_texture [[texture(5)]],
    texture2d_array<float> shadow_maps [[texture(6)]],
    sampler point_sampler [[sampler(0)]],
    sampler brdf_sampler [[sampler(1)]],
    sampler cookie_sampler [[sampler(2)]])
{
    const float depth = depth_texture.sample(
        point_sampler, input.texcoord).r;
    const bool reverse_z = constants.config.x != 0u;
    const float ndc_z = reverse_z ? depth : depth * 2.0 - 1.0;
    float4 position = constants.inverse_projection *
        float4(input.texcoord * 2.0 - 1.0, ndc_z, 1.0);
    position.w = max(position.w, 0.000001);
    position /= position.w;

    const float4 normal_sample = normal_texture.sample(
        point_sampler, input.texcoord);
    const float3 normal = almetal_world_oct_decode(normal_sample.xy);
    const bool has_pbr = abs(normal_sample.a - 0.67) < 0.1;
    float3 geometric_normal = normal;
    if (has_pbr)
    {
        const float levels = constants.config.z != 0u ? 255.0 : 31.0;
        const float range = constants.config.z != 0u ? 65535.0 : 1023.0;
        const float value = floor(normal_sample.z * range + 0.5);
        const float x = floor(value / (levels + 1.0));
        const float y = value - x * (levels + 1.0);
        geometric_normal = almetal_world_oct_decode(
            float2(x, y) / levels);
    }
    float3 position_dx = dfdx(position.xyz);
    float3 position_dy = dfdy(position.xyz);
    const float3 derivative_plane = cross(position_dx, position_dy);
    const float derivative_along = dot(
        derivative_plane, geometric_normal);
    if (derivative_along * derivative_along <
        0.25 * dot(derivative_plane, derivative_plane) *
            dot(geometric_normal, geometric_normal))
    {
        position_dx = 0.0;
        position_dy = 0.0;
    }

    if ((reverse_z && depth <= 0.0) || (!reverse_z && depth >= 1.0))
    {
        discard_fragment();
    }

    const float3 center_vector =
        constants.position_radius.xyz - position.xyz;
    const float center_distance = length(center_vector);
    if (center_distance >= constants.position_radius.w)
    {
        discard_fragment();
    }
    const float normalized_distance =
        center_distance / constants.position_radius.w;
    const float light_distance =
        -dot(center_vector, constants.projector_normal.xyz);
    float4 projected = constants.projector_matrix * position;
    if (projected.z < 0.0)
    {
        discard_fragment();
    }
    projected.xyz /= projected.w;

    const float distance_attenuation =
        almetal_legacy_distance_attenuation(
            normalized_distance, constants.color_falloff.w);
    if (distance_attenuation <= 0.0)
    {
        discard_fragment();
    }

    const bool in_cookie = projected.x > 0.0 && projected.x < 1.0 &&
                           projected.y > 0.0 && projected.y < 1.0 &&
                           (has_pbr || projected.z > 0.0);
    if (!in_cookie && (has_pbr || !(normal_sample.b > 0.0)))
    {
        discard_fragment();
    }

    float shadow = 1.0;
    if (constants.config.y != 0u)
    {
        const float2 gl_fragment_position = float2(
            input.position.x, shadows.parameters.w - input.position.y);
        shadow = almetal_projector_shadow(
            shadow_maps, shadows, position.xyz, normal,
            position_dx, position_dy, constants.config.y - 1u,
            gl_fragment_position / shadows.resolution.zw);
        shadow = clamp(shadow + constants.sampling.y, 0.0, 1.0);
    }

    const float3 base_color = albedo_texture.sample(
        point_sampler, input.texcoord).rgb;
    float4 specular = specular_texture.sample(
        point_sampler, input.texcoord);
    const float3 view = -normalize(position.xyz);
    const float3 origin_vector =
        constants.projector_origin.xyz - position.xyz;
    float3 half_vector;
    float3 light_direction;
    float nh;
    float nl;
    float nv;
    float vh;
    almetal_legacy_half_vectors(
        origin_vector, normal, view, half_vector, light_direction,
        nh, nl, nv, vh);

    const float lod_range = constants.sampling.x;
    const float focus = constants.projector_origin.w;
    const float projector_range = constants.projector_normal.w;
    const float ambiance = constants.projector_plane.w;
    float3 final_color = 0.0;

    if (has_pbr)
    {
        const float perceptual_roughness =
            specular.g + specular.a / 255.0;
        const float metallic = specular.b;
        const float3 diffuse_color = base_color * (1.0 - metallic);
        const float3 specular_color = mix(
            float3(0.04), base_color, metallic);
        const float3 energy_compensation =
            almetal_pbr_energy_compensation(
                brdf_lut, brdf_sampler, specular_color,
                perceptual_roughness, dot(normal, view));

        if (projected.x > 0.0 && projected.x < 1.0 &&
            projected.y > 0.0 && projected.y < 1.0)
        {
            float lit = 0.0;
            float ambient_amount = 0.0;
            if (dot(normal, light_direction) > 0.0)
            {
                ambient_amount += (nl * 0.5 + 0.5) * ambiance;
                const float lod = clamp(
                    (light_distance - focus) / projector_range,
                    0.0, 1.0) * lod_range;
                const float4 cookie = almetal_projector_diffuse_cookie(
                    cookie_texture, cookie_sampler, projected.xy,
                    lod, lod_range);
                const float3 projected_color =
                    constants.color_falloff.rgb *
                    cookie.rgb * cookie.a;
                const ALMetalPBRPreviewPunctual punctual =
                    almetal_pbr_punctual(
                        diffuse_color, specular_color,
                        perceptual_roughness, normal, view,
                        light_direction);
                nl = punctual.nl;
                const float3 lobe = nl *
                    (punctual.diffuse +
                     punctual.specular * energy_compensation);
                final_color += distance_attenuation * projected_color *
                    3.0 * shadow *
                    almetal_world_bounded_radiance(lobe);
                lit = clamp(nl * distance_attenuation, 0.0, 1.0);
            }

            const float4 ambient_cookie =
                almetal_projector_ambient_cookie(
                    cookie_texture, cookie_sampler, projected.xy,
                    lod_range);
            ambient_amount += ambiance;
            ambient_amount +=
                (nl * nl * 0.5 + 0.5) * ambiance;
            ambient_amount *= distance_attenuation;
            ambient_amount = min(ambient_amount, 1.0 - lit);
            const float3 ambient_color = ambient_amount *
                constants.color_falloff.rgb * ambient_cookie.rgb *
                ambient_cookie.a * 3.0;
            final_color += diffuse_color * ambient_color / M_PI_F;
        }
    }
    else
    {
        const float environment_intensity = normal_sample.b;
        specular.rgb = almetal_srgb_to_linear(specular.rgb);
        float3 direct_cookie = 0.0;
        if (projected.z > 0.0 && projected.x > 0.0 &&
            projected.x < 1.0 && projected.y > 0.0 &&
            projected.y < 1.0)
        {
            float ambient_amount = 0.0;
            float lit = 0.0;
            if (nl > 0.0)
            {
                lit = nl * distance_attenuation;
                const float lod = clamp(
                    (light_distance - focus) / projector_range,
                    0.0, 1.0) * lod_range;
                const float4 cookie = almetal_projector_diffuse_cookie(
                    cookie_texture, cookie_sampler, projected.xy,
                    lod, lod_range);
                direct_cookie = constants.color_falloff.rgb *
                    cookie.rgb * cookie.a;
                final_color = direct_cookie * lit * base_color * shadow;
                ambient_amount +=
                    (nl * 0.5 + 0.5) * ambiance;
            }
            const float4 ambient_cookie =
                almetal_projector_ambient_cookie(
                    cookie_texture, cookie_sampler, projected.xy,
                    lod_range);
            ambient_amount += ambiance;
            ambient_amount +=
                (nl * nl * 0.5 + 0.5) * ambiance;
            ambient_amount *= distance_attenuation;
            ambient_amount = min(ambient_amount, 1.0 - lit);
            const float3 ambient_color = ambient_amount *
                constants.color_falloff.rgb * ambient_cookie.rgb *
                ambient_cookie.a;
            final_color += base_color * ambient_color *
                max(dot(-normalize(origin_vector), normal), 0.0);
        }

        if (specular.a > 0.0)
        {
            direct_cookie *=
                min(nl * 6.0, 1.0) * distance_attenuation;
            const float fresnel =
                pow(1.0 - vh, 5.0) * 0.4 + 0.5;
            const float geometry_denominator = 2.0 * nh;
            const float geometry = max(
                0.0, min(geometry_denominator * nv / vh,
                         geometry_denominator * nl / vh));
            if (nh > 0.0)
            {
                const float lobe = fresnel *
                    almetal_legacy_blinn_phong_lobe(
                        nh, specular.a, constants.sampling.z) *
                    geometry / (nh * nl);
                final_color += clamp(
                    direct_cookie * lobe * specular.rgb * shadow,
                    float3(0.0), float3(1.0));
            }
        }

        if (environment_intensity > 0.0)
        {
            const float3 reflection = reflect(
                normalize(position.xyz), normal);
            const float3 plane_delta =
                constants.projector_plane.xyz - position.xyz;
            const float plane_distance = dot(
                reflection, constants.projector_normal.xyz);
            if (plane_distance < 0.0)
            {
                const float3 reflected_position = position.xyz +
                    reflection * dot(
                        plane_delta, constants.projector_normal.xyz) /
                    plane_distance;
                float4 reflected_texcoord =
                    constants.projector_matrix *
                    float4(reflected_position, 1.0);
                if (reflected_texcoord.z > 0.0)
                {
                    reflected_texcoord /= reflected_texcoord.w;
                    if (reflected_texcoord.x > 0.0 &&
                        reflected_texcoord.x < 1.0 &&
                        reflected_texcoord.y > 0.0 &&
                        reflected_texcoord.y < 1.0)
                    {
                        const float lod = (1.0 - specular.a) *
                            (lod_range * 0.6);
                        const float4 cookie =
                            almetal_projector_specular_cookie(
                                cookie_texture, cookie_sampler,
                                reflected_texcoord.xy, lod, lod_range);
                        final_color += constants.color_falloff.rgb *
                            cookie.rgb * shadow *
                            environment_intensity;
                    }
                }
            }
        }
    }

    return float4(
        max(final_color * constants.sampling.w, float3(0.0)), 0.0);
}

fragment float4 almetal_world_lighting_composition_fragment(
    ALMetalWorldLocalLightVaryings input [[stage_in]],
    texture2d<float> base_lighting [[texture(0)]],
    texture2d<float> local_lighting [[texture(1)]],
    texture2d<float> projector_lighting [[texture(2)]],
    sampler point_sampler [[sampler(0)]])
{
    // GL writes the soften/base result first, then uses ONE/ONE blending for
    // both local-light families. Every contributing owner writes zero alpha.
    return base_lighting.sample(point_sampler, input.texcoord) +
        local_lighting.sample(point_sampler, input.texcoord) +
        projector_lighting.sample(point_sampler, input.texcoord);
}

fragment ALMetalWorldSSRHistoryOutput almetal_world_ssr_history_fragment(
    ALMetalWorldLocalLightVaryings input [[stage_in]],
    texture2d<float> scene_color [[texture(0)]],
    texture2d<float> scene_depth [[texture(1)]])
{
    const uint2 target = uint2(input.position.xy);
    const uint2 source = uint2(
        target.x, scene_color.get_height() - 1u - target.y);
    ALMetalWorldSSRHistoryOutput output;
    output.scene_color = scene_color.read(source);
    output.scene_depth = scene_depth.read(source).r;
    return output;
}

static float3 almetal_world_unpack_geometric_normal(
    float packed, bool high_precision)
{
    const float levels = high_precision ? 255.0 : 31.0;
    const float range = high_precision ? 65535.0 : 1023.0;
    const float value = floor(packed * range + 0.5);
    const float x = floor(value / (levels + 1.0));
    const float y = value - x * (levels + 1.0);
    return almetal_world_oct_decode(float2(x, y) / levels);
}

static float4 almetal_world_light_map_position(
    float2 texcoord, texture2d<float> depth_texture,
    sampler point_sampler,
    constant ALMetalWorldLightMapConstants& constants)
{
    const float depth = depth_texture.sample(point_sampler, texcoord).r;
    const float ndc_z = constants.config.x != 0u
        ? depth : depth * 2.0 - 1.0;
    float4 position = constants.inverse_projection *
        float4(texcoord * 2.0 - 1.0, ndc_z, 1.0);
    position.w = max(position.w, 0.000001);
    position /= position.w;
    position.w = 1.0;
    return position;
}

static float2 almetal_world_ssao_kernel(uint index)
{
    constexpr float2 taps[8] = {
        float2(-1.0, 0.0) * (0.125 * 0.125),
        float2(1.0, 0.0) * (0.250 * 0.250),
        float2(0.0, 1.0) * (0.375 * 0.375),
        float2(0.0, -1.0) * (0.500 * 0.500),
        float2(0.7071, 0.7071) * (0.625 * 0.625),
        float2(-0.7071, -0.7071) * (0.750 * 0.750),
        float2(-0.7071, 0.7071) * (0.875 * 0.875),
        float2(0.7071, -0.7071),
    };
    return taps[index];
}

static float almetal_world_ambient_occlusion(
    float4 position, float3 normal, float2 texcoord,
    texture2d<float> depth_texture,
    texture2d<float> noise_texture,
    sampler point_sampler, sampler repeat_sampler,
    constant ALMetalWorldLightMapConstants& constants)
{
    const float2 noise_reflect = noise_texture.sample(
        repeat_sampler, texcoord * (constants.screen.xy / 128.0)).xy;
    const float scale = min(
        constants.ssao.x / -position.z, constants.ssao.y);
    float angle_hidden = 0.0;
    float points = 0.0;
    for (uint index = 0; index < 8; ++index)
    {
        const float2 tap =
            almetal_world_ssao_kernel(index) / constants.screen.xy;
        const float2 sample_texcoord = texcoord +
            scale * reflect(tap, noise_reflect);
        const float3 sample_position = almetal_world_light_map_position(
            sample_texcoord, depth_texture, point_sampler, constants).xyz;
        const float3 difference = position.xyz - sample_position;
        const float distance_squared = dot(difference, difference);
        const float hidden = dot(
            sample_position - 0.05 * normal - position.xyz,
            normal) > 0.0 ? 1.0 : 0.0;
        angle_hidden += hidden * min(
            1.0 / distance_squared, constants.ssao.w);
        points += difference.z > -1.0 ? 1.0 : 0.0;
    }
    angle_hidden = points > 0.0
        ? min(constants.ssao.z * angle_hidden / points, 1.0)
        : 0.0;
    return clamp(1.0 - angle_hidden, 0.0, 1.0);
}

fragment float2 almetal_world_light_map_fragment(
    ALMetalWorldLocalLightVaryings input [[stage_in]],
    constant ALMetalWorldLightMapConstants& constants [[buffer(0)]],
    constant ALMetalWorldDirectionalShadowConstants& shadows [[buffer(1)]],
    texture2d<float> normal_texture [[texture(0)]],
    texture2d<float> depth_texture [[texture(1)]],
    texture2d_array<float> shadow_maps [[texture(2)]],
    texture2d<float> noise_texture [[texture(3)]],
    sampler point_sampler [[sampler(0)]],
    sampler repeat_sampler [[sampler(1)]])
{
    const float depth = depth_texture.sample(
        point_sampler, input.texcoord).r;
    const bool reverse_z = constants.config.x != 0u;
    if ((reverse_z && depth <= 0.0) || (!reverse_z && depth >= 1.0))
    {
        discard_fragment();
    }

    const float4 position = almetal_world_light_map_position(
        input.texcoord, depth_texture, point_sampler, constants);
    const float4 normal_sample = normal_texture.sample(
        point_sampler, input.texcoord);
    const float3 normal = almetal_world_oct_decode(normal_sample.xy);
    const float3 geometric_normal =
        almetal_world_unpack_geometric_normal(
            normal_sample.z, constants.config.w != 0u);

    float directional_visibility = 1.0;
    if (constants.config.y != 0u)
    {
        const float3 light_direction = normalize(
            constants.light_direction.xyz);
        // Keep the receiver derivatives in uniform control flow before the
        // per-pixel facing test, matching surfaceDerivatives in shadowUtil.
        float3 position_dx = dfdx(position.xyz);
        float3 position_dy = dfdy(position.xyz);
        const float3 plane = cross(position_dx, position_dy);
        const float along = dot(plane, geometric_normal);
        if (along * along < 0.25 * dot(plane, plane) *
                                 dot(geometric_normal,
                                     geometric_normal))
        {
            position_dx = 0.0;
            position_dy = 0.0;
        }
        if (dot(normal, light_direction) <= 0.0)
        {
            directional_visibility = 0.0;
        }
        else
        {
            const float2 gl_fragment_position = float2(
                input.position.x,
                constants.screen.y - input.position.y);
            directional_visibility =
                (shadows.config.y & 0xfu) == 0xfu
                ? 1.0
                : almetal_directional_shadow(
                      shadow_maps, shadows, position.xyz, normal,
                      position_dx, position_dy,
                      gl_fragment_position / shadows.resolution.zw,
                      light_direction);
        }
    }

    const float ambient_occlusion = constants.config.z != 0u
        ? almetal_world_ambient_occlusion(
              position, geometric_normal, input.texcoord,
              depth_texture, noise_texture,
              point_sampler, repeat_sampler, constants)
        : 1.0;
    return clamp(
        float2(directional_visibility, ambient_occlusion),
        float2(0.0), float2(1.0));
}

static int almetal_world_light_map_mirror(int value, int size)
{
    if (value >= -size && value < 2 * size)
    {
        return value < 0 ? -1 - value
                         : (value < size ? value : 2 * size - 1 - value);
    }
    const int period = 2 * size;
    const int mirrored = (value < 0 ? -1 - value : value) % period;
    return mirrored < size ? mirrored : period - 1 - mirrored;
}

static float almetal_world_light_map_view_depth(
    int2 texel, texture2d<float> depth_texture,
    constant ALMetalWorldLightMapConstants& constants)
{
    const int2 size = int2(constants.screen.xy);
    const int2 clamped = clamp(texel, int2(0), size - 1);
    const float depth = depth_texture.read(uint2(clamped)).r;
    const bool reverse_z = constants.config.x != 0u;
    if ((reverse_z && depth <= 0.0) || (!reverse_z && depth >= 1.0))
    {
        return -1.0;
    }
    const float2 texcoord =
        (float2(clamped) + 0.5) / constants.screen.xy;
    const float2 screen_position = texcoord * 2.0 - 1.0;
    const float ndc_z = reverse_z ? depth : depth * 2.0 - 1.0;
    const float w =
        constants.inverse_projection[0].w * screen_position.x +
        constants.inverse_projection[1].w * screen_position.y +
        constants.inverse_projection[2].w * ndc_z +
        constants.inverse_projection[3].w;
    return 1.0 / max(w, 0.000001);
}

static float3 almetal_world_light_map_blur_kernel(
    int step, constant ALMetalWorldLightMapConstants& constants)
{
    const int index = step >> 1;
    return (step & 1) == 0
        ? constants.blur_taps[index].xyz
        : (constants.blur_taps[index].xyz +
           constants.blur_taps[index + 1].xyz) * 0.5;
}

struct ALMetalWorldLightMapBlurPixel
{
    float2 step;
    float2 origin;
    float3 plane;
    float offset;
    float tolerance;
};

static ALMetalWorldLightMapBlurPixel almetal_world_light_map_blur_pixel(
    int2 pixel, float2 delta, float view_depth,
    texture2d<float> normal_texture,
    constant ALMetalWorldLightMapConstants& constants)
{
    ALMetalWorldLightMapBlurPixel result;
    const float2 texcoord =
        (float2(pixel) + 0.5) / constants.screen.xy;
    const float4 normal_sample = normal_texture.read(uint2(pixel));
    const float3 normal = almetal_world_oct_decode(normal_sample.xy);
    const float2 texel_size = 2.0 / constants.screen.xy;
    const float across = dot(
        normal, constants.inverse_projection[0].xyz) * texel_size.x;
    const float down = dot(
        normal, constants.inverse_projection[1].xyz) * texel_size.y;
    result.plane = float3(
        dot(normal, constants.inverse_projection[3].xyz) +
            (across + down) * 0.5 -
            dot(normal, constants.inverse_projection[0].xyz +
                        constants.inverse_projection[1].xyz),
        across, down);
    const float3 position =
        (constants.inverse_projection[0].xyz *
             (texcoord.x * 2.0 - 1.0) +
         constants.inverse_projection[1].xyz *
             (texcoord.y * 2.0 - 1.0) +
         constants.inverse_projection[3].xyz) * view_depth;
    result.offset =
        (result.plane.x + float(pixel.x) * result.plane.y +
         float(pixel.y) * result.plane.z) * view_depth;
    result.step = constants.blur.x * delta /
        (1.0 + normal.xy * normal.xy);
    result.step /= max(-position.z * constants.blur.y, 1.0);
    result.tolerance = position.z * position.z * 0.00005;
    const float checker = float((pixel.x + pixel.y + 1) & 1);
    result.origin = float2(pixel) + 0.5 +
        (checker - 0.5) * constants.blur_taps[1].z *
            result.step * 0.5;
    return result;
}

fragment float2 almetal_world_light_map_blur_fragment(
    ALMetalWorldLocalLightVaryings input [[stage_in]],
    constant ALMetalWorldLightMapConstants& constants [[buffer(0)]],
    constant float4& blur_delta [[buffer(1)]],
    texture2d<float> light_map [[texture(0)]],
    texture2d<float> normal_texture [[texture(1)]],
    texture2d<float> depth_texture [[texture(2)]],
    sampler point_sampler [[sampler(0)]])
{
    const int2 pixel = int2(input.position.xy);
    const float2 center = light_map.read(uint2(pixel)).rg;
    const float center_depth = almetal_world_light_map_view_depth(
        pixel, depth_texture, constants);
    if (center_depth <= 0.0)
    {
        return center;
    }

    const ALMetalWorldLightMapBlurPixel blur =
        almetal_world_light_map_blur_pixel(
            pixel, blur_delta.xy, center_depth,
            normal_texture, constants);
    float defined_weight = constants.blur_taps[0].y;
    float ambient_occlusion = defined_weight * center.y;
    const int2 size = int2(constants.screen.xy);
    for (int index = 0; index < 12; ++index)
    {
        const int step = index < 6 ? index + 1 : index - 5;
        const float side = index < 6 ? 1.0 : -1.0;
        const float3 tap = almetal_world_light_map_blur_kernel(
            step, constants);
        const int2 unmirrored = int2(floor(
            blur.origin + side * tap.z * blur.step * 2.0));
        const float view_depth = almetal_world_light_map_view_depth(
            unmirrored, depth_texture, constants);
        const float distance =
            (blur.plane.x + float(unmirrored.x) * blur.plane.y +
             float(unmirrored.y) * blur.plane.z) * view_depth -
            blur.offset;
        const float weight = view_depth > 0.0 &&
            distance * distance <= blur.tolerance ? tap.y : 0.0;
        if (weight > 0.0)
        {
            const int2 texel(
                almetal_world_light_map_mirror(unmirrored.x, size.x),
                almetal_world_light_map_mirror(unmirrored.y, size.y));
            ambient_occlusion +=
                light_map.read(uint2(texel)).g * weight;
            defined_weight += weight;
        }
    }
    return float2(
        center.r, max(ambient_occlusion / defined_weight, 0.0));
}

static float almetal_world_ssao_visibility(
    float ambient_occlusion,
    constant ALMetalWorldBaseLightingConstants& constants)
{
    if (constants.config.w == 0u)
    {
        return 1.0;
    }
    const float3x3 effect = float3x3(
        constants.ssao_effect[0].xyz,
        constants.ssao_effect[1].xyz,
        constants.ssao_effect[2].xyz);
    const float value_factor = dot(
        effect * float3(1.0), float3(1.0 / 3.0));
    const float occluded = clamp(
        constants.lighting.z * value_factor, 0.0, 1.0);
    return clamp(mix(occluded, 1.0, ambient_occlusion), 0.0, 1.0);
}

static float3 almetal_world_base_ambient(
    float3 normal,
    constant ALMetalWorldBaseLightingConstants& constants)
{
    float factor = min(abs(dot(normal, constants.light_direction.xyz)),
                       1.0) * 0.5;
    factor = 1.0 - factor * factor;
    float3 ambient = constants.atmos_amblit.rgb * factor;
    if (constants.config.y == 0u)
    {
        ambient = almetal_srgb_to_linear(ambient);
        ambient = float3(dot(
            ambient, float3(0.2126, 0.7152, 0.0722)));
    }
    return ambient * constants.lighting.y;
}

fragment float4 almetal_world_base_light_fragment(
    ALMetalWorldLocalLightVaryings input [[stage_in]],
    constant ALMetalWorldBaseLightingConstants& constants [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]],
    constant ALMetalWorldHeroProbeConstants& hero [[buffer(2)]],
    constant ALMetalWorldScreenSpaceReflectionConstants& ssr [[buffer(3)]],
    texture2d<float> albedo_texture [[texture(0)]],
    texture2d<float> specular_texture [[texture(1)]],
    texture2d<float> normal_texture [[texture(2)]],
    texture2d<float> emissive_texture [[texture(3)]],
    texture2d<float> depth_texture [[texture(4)]],
    texture2d<float> light_map [[texture(5)]],
    texturecube_array<float> probe_radiance [[texture(6)]],
    texture2d<uint> probe_irradiance [[texture(7)]],
    texture2d<float> brdf_lut [[texture(8)]],
    texturecube<float> hero_radiance [[texture(9)]],
    texture2d<float> ssr_scene_color [[texture(10)]],
    texture2d<float> ssr_scene_depth [[texture(11)]],
    texturecube<float> legacy_environment_map [[texture(12)]],
    sampler point_sampler [[sampler(0)]],
    sampler probe_sampler [[sampler(1)]],
    sampler brdf_sampler [[sampler(2)]])
{
    const float depth = depth_texture.sample(
        point_sampler, input.texcoord).r;
    const bool reverse_z = constants.config.x != 0u;
    const bool clear_depth =
        (reverse_z && depth <= 0.0) || (!reverse_z && depth >= 1.0);
    const float4 normal_sample = normal_texture.sample(
        point_sampler, input.texcoord);
    if (clear_depth)
    {
        // A clear target also has alpha one. Require the explicit
        // transaction bit and the family-specific zero-normal marker rather
        // than accepting diagnostic clear pixels as sky.
        // Family 0 is full HDRI, 1 is full WindLight and 2 is the
        // split-screen transaction. The split decodes each clear-depth pixel
        // from the exact family marker written by its branch.
        const bool windlight_marker =
            normal_sample.r < 0.001 &&
            normal_sample.b < 0.001 &&
            normal_sample.a < 0.1;
        const bool hdri_marker =
            max(normal_sample.r,
                max(normal_sample.g, normal_sample.b)) < 0.001 &&
            normal_sample.a > 0.9;
        const bool windlight_sky = constants.sky.z == 1u ||
            (constants.sky.z == 2u && windlight_marker);
        const bool sky_marker = constants.sky.x != 0u &&
            (windlight_sky ? windlight_marker : hdri_marker);
        if (!sky_marker)
        {
            discard_fragment();
        }
        float3 sky_color = constants.sky.y != 0u
            ? emissive_texture.sample(
                  point_sampler, input.texcoord).rgb
            : albedo_texture.sample(
                  point_sampler, input.texcoord).rgb;
        if (windlight_sky && constants.sky.y != 0u)
        {
            sky_color = almetal_srgb_to_linear(sky_color);
        }
        sky_color *= windlight_sky ? constants.sky_transfer.x : 1.0;
        sky_color *= constants.config.y != 0u ? 1.1 : 1.0;
        sky_color = select(
            sky_color, float3(65504.0), isinf(sky_color));
        sky_color = select(sky_color, float3(0.0), isnan(sky_color));
        return float4(sky_color, 0.0);
    }

    const float ndc_z = reverse_z ? depth : depth * 2.0 - 1.0;
    float4 position = constants.inverse_projection *
        float4(input.texcoord * 2.0 - 1.0, ndc_z, 1.0);
    position.w = max(position.w, 0.000001);
    position /= position.w;

    const float3 normal = almetal_world_oct_decode(normal_sample.xy);
    const float4 base_color = albedo_texture.sample(
        point_sampler, input.texcoord);
    float4 specular = specular_texture.sample(
        point_sampler, input.texcoord);
    const float3 emissive = emissive_texture.sample(
        point_sampler, input.texcoord).rgb;
    const bool pbr = abs(normal_sample.a - 0.67) < 0.1;
    const bool classic_mode = constants.config.y != 0u;
    const float2 shadow_occlusion = light_map.sample(
        point_sampler, input.texcoord).rg;
    const float sun_visibility = constants.config.z != 0u
        ? max(shadow_occlusion.r, base_color.a) : 1.0;
    const float ambient_occlusion = constants.config.w != 0u
        ? shadow_occlusion.g : 1.0;

    float3 sunlit = classic_mode
        ? constants.atmos_sunlit.rgb
        : constants.atmos_sunlit_linear.rgb;
    if (classic_mode)
    {
        sunlit *= 1.35;
    }
    sunlit *= constants.lighting.x;
    const float3 ambient = almetal_world_base_ambient(normal, constants);
    const float environment_intensity = normal_sample.z;
    const float glossiness = pbr
        ? 1.0 - (specular.g + specular.a / 255.0)
        : specular.a;
    const bool legacy_environment_path =
        constants.environment_config.x != 0u;
    float3 irradiance = ambient;
    float3 radiance = 0.0;
    float3 legacy_environment = 0.0;
    if (legacy_environment_path)
    {
        // class2/deferred/reflectionProbeF.glsl: without reflection probes,
        // diffuse ambiance is a sky-authored grey blend and every glossy
        // lobe samples the sharp legacy sky cube through env_mat.
        const float ambiance = constants.environment.x;
        irradiance = mix(
            ambient, float3(ambiance * 0.25), ambiance);
        const float3x3 environment_matrix = float3x3(
            constants.environment_matrix[0].xyz,
            constants.environment_matrix[1].xyz,
            constants.environment_matrix[2].xyz);
        const float3 reflection = normalize(
            reflect(position.xyz, normal));
        const float3 environment_sample = legacy_environment_map.sample(
            probe_sampler, environment_matrix * reflection).rgb;
        radiance = pbr
            ? almetal_srgb_to_linear(environment_sample)
            : environment_sample;
        legacy_environment = !pbr && environment_intensity > 0.0
            ? environment_sample : float3(0.0);
    }
    else
    {
        almetal_sample_base_probes(
            pbr, position.xyz, normal, glossiness,
            environment_intensity, ambient, classic_mode,
            probe_radiance, probe_irradiance, probe_sampler, probes,
            irradiance, radiance, legacy_environment);
    }

    const float3 mirror_direction = normalize(
        reflect(position.xyz, normal));
    const float3 gloss_direction = pbr
        ? almetal_base_dominant_direction(
              normal, mirror_direction, 1.0 - glossiness)
        : mirror_direction;
    const float2 gl_fragment_position = float2(
        input.position.x, ssr.screen.z - input.position.y);
    float4 ssr_sample = float4(0.0);
    if (!legacy_environment_path)
    {
        ssr_sample = almetal_sample_ssr(
            gl_fragment_position / ssr.screen.xy,
            position.xyz, normal, gloss_direction, glossiness, false,
            ssr_scene_color, ssr_scene_depth,
            brdf_sampler, point_sampler, ssr);
        radiance = mix(radiance, ssr_sample.rgb, ssr_sample.a);
        radiance = almetal_finish_hero_probe(
            radiance, position.xyz, normal, glossiness,
            hero_radiance, probe_sampler, probes, hero);
        if (!pbr)
        {
            legacy_environment = mix(
                legacy_environment, ssr_sample.rgb, ssr_sample.a);
            legacy_environment = almetal_finish_hero_probe(
                legacy_environment, position.xyz, normal, 1.0,
                hero_radiance, probe_sampler, probes, hero);
        }
    }

    float3 color = 0.0;
    const float3 view = -normalize(position.xyz);
    const float3 light_direction = normalize(
        constants.light_direction.xyz);
    if (pbr)
    {
        const float perceptual_roughness =
            specular.g + specular.a / 255.0;
        const float metallic = specular.b;
        const float material_ao = specular.r;
        const float3 diffuse_color = base_color.rgb * (1.0 - metallic);
        const float3 specular_color = mix(
            float3(0.04), base_color.rgb, metallic);
        const float3 geometric_normal =
            almetal_world_unpack_geometric_normal(
                normal_sample.z, constants.surface.y != 0.0);
        radiance *= almetal_pbr_horizon_occlusion(
            reflect(normalize(position.xyz), normal), geometric_normal);
        const float visibility = min(
            material_ao,
            almetal_world_ssao_visibility(
                ambient_occlusion, constants));
        const float nv = clamp(abs(dot(normal, view)), 0.001, 1.0);
        const float roughness = max(perceptual_roughness, 0.045);
        const ALMetalPBRPreviewIBL ibl = almetal_pbr_ibl(
            brdf_lut, brdf_sampler, diffuse_color, specular_color,
            radiance, irradiance, visibility, nv, roughness);
        const float alpha = perceptual_roughness * perceptual_roughness;
        const float sun_roughness = sqrt(clamp(alpha + 0.00465,
                                                0.0, 1.0));
        ALMetalPBRPreviewPunctual punctual = almetal_pbr_punctual(
            diffuse_color, specular_color, sun_roughness,
            normal, view, light_direction);
        punctual.specular *= almetal_pbr_energy_compensation(
            brdf_lut, brdf_sampler, specular_color, roughness, nv);

        if (classic_mode)
        {
            const float3 classic_irradiance = almetal_srgb_to_linear(
                irradiance * 0.9);
            const float directional = pow(punctual.nl, 1.2);
            float3 sun_contribution = float3(
                min(directional, sun_visibility));
            sun_contribution = almetal_srgb_to_linear(
                almetal_linear_to_srgb(sun_contribution) *
                sunlit * 0.7) * M_PI_F;
            const float3 final_ambient = classic_irradiance *
                diffuse_color *
                almetal_pbr_gtao_multibounce(
                    visibility, diffuse_color);
            const float3 final_sun = almetal_world_bounded_radiance(
                sun_contribution *
                ((punctual.diffuse + punctual.specular) *
                 sun_visibility));
            color = almetal_srgb_to_linear(
                almetal_linear_to_srgb(final_ambient) +
                almetal_linear_to_srgb(final_sun) * 1.1);
        }
        else
        {
            color = ibl.diffuse +
                almetal_world_bounded_radiance(
                    punctual.nl *
                    (punctual.diffuse + punctual.specular)) *
                sunlit * 3.0 * sun_visibility;
        }
        color += ibl.specular + emissive;
    }
    else
    {
        specular.rgb = almetal_srgb_to_linear(specular.rgb);
        float3 legacy_irradiance = irradiance;
        if (constants.config.w != 0u)
        {
            const float3x3 effect = float3x3(
                constants.ssao_effect[0].xyz,
                constants.ssao_effect[1].xyz,
                constants.ssao_effect[2].xyz);
            legacy_irradiance = mix(
                effect * min(
                    legacy_irradiance * constants.lighting.z,
                    float3(constants.lighting.w)),
                legacy_irradiance, ambient_occlusion);
        }
        const float environment_visibility =
            almetal_world_ssao_visibility(
                ambient_occlusion, constants);
        radiance *= environment_visibility;
        legacy_environment *= environment_visibility;
        float directional = clamp(dot(normal, light_direction), 0.0, 1.0);
        color = legacy_irradiance;
        float3 legacy_sunlit = sunlit;
        if (classic_mode)
        {
            directional = pow(directional, 1.2);
            const float3 sun_contribution = float3(
                min(directional, sun_visibility));
            color = almetal_srgb_to_linear(
                color * 0.9 +
                almetal_linear_to_srgb(sun_contribution) *
                legacy_sunlit * 0.7);
            legacy_sunlit = almetal_srgb_to_linear(legacy_sunlit);
        }
        else
        {
            color += min(directional, sun_visibility) * legacy_sunlit;
        }
        color *= base_color.rgb;

        if (specular.a > 0.0)
        {
            float3 half_vector;
            float3 normalized_light;
            float nh;
            float nl;
            float nv;
            float vh;
            almetal_legacy_half_vectors(
                light_direction, normal, view, half_vector,
                normalized_light, nh, nl, nv, vh);
            if (dot(normal, normalized_light) > 0.0 &&
                dot(normal, half_vector) > 0.0)
            {
                const float lit = min(nl * 6.0, 1.0);
                const float fresnel =
                    pow(1.0 - vh, 5.0) * 0.4 + 0.5;
                const float geometry_denominator = 2.0 * nh;
                const float geometry = max(
                    0.0, min(geometry_denominator * nv / vh,
                             geometry_denominator * nl / vh));
                const float lobe = sun_visibility * fresnel *
                    almetal_legacy_blinn_phong_lobe(
                        nh, specular.a, constants.surface.x) *
                    geometry / (nh * nl);
                color += lit * lobe * legacy_sunlit * specular.rgb;
            }

            if (!legacy_environment_path)
            {
                float3 gloss_environment = radiance * 0.5;
                float fresnel = clamp(
                    1.0 + dot(normalize(position.xyz), normal),
                    0.3, 1.0);
                fresnel *= fresnel * specular.a;
                gloss_environment *= specular.rgb * fresnel;
                gloss_environment *= float3(1.0) - color;
                color += gloss_environment * 0.5;
            }
        }
        color = mix(color, base_color.rgb, base_color.a);
        if (environment_intensity > 0.0)
        {
            if (legacy_environment_path)
            {
                color = almetal_srgb_to_linear(mix(
                    almetal_linear_to_srgb(color),
                    legacy_environment * 2.0,
                    environment_intensity));
            }
            else
            {
                const float3 look_at = normalize(position.xyz);
                float fresnel = 1.0 + dot(look_at, normal);
                fresnel *= fresnel;
                fresnel = min(fresnel + environment_intensity, 1.0);
                const float3 reflected = legacy_environment *
                    (environment_intensity * fresnel);
                color = mix(
                    color, reflected * 0.5, environment_intensity);
            }
        }
    }

    const float output_scale = classic_mode ? 1.1 : 1.0;
    return float4(max(
        almetal_world_bounded_radiance(color * output_scale),
        float3(0.0)), 0.0);
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

struct ALMetalWorldExposureMeterConstants
{
    // xy = source texels per meter patch, zw = taps per patch axis.
    float4 patch;
};

struct ALMetalWorldExposureConstants
{
    // log2(key) + compensation, min EV, max EV, delta seconds.
    float4 exposure;
    // low/high kept fractions, speed toward brighter/darker scenes.
    float4 adaptation;
    // sky weight, metering mode, reserved, reserved.
    float4 meter;
};

static float almetal_world_meter_weight(float2 uv, float mode)
{
    const float2 q = uv * 2.0 - 1.0;
    const float distance_squared = dot(q, q);
    if (mode < 0.5)
    {
        return 1.0;
    }
    if (mode < 1.5)
    {
        return exp(-2.0 * distance_squared);
    }
    return 1.0 - smoothstep(0.15, 0.3, sqrt(distance_squared));
}

kernel void almetal_world_exposure_meter(
    texture2d<float> scene [[texture(0)]],
    texture2d<float> normal_map [[texture(1)]],
    texture2d<float, access::write> meter [[texture(2)]],
    sampler scene_sampler [[sampler(0)]],
    constant ALMetalWorldExposureMeterConstants& constants [[buffer(0)]],
    uint2 position [[thread_position_in_grid]])
{
    if (position.x >= meter.get_width() || position.y >= meter.get_height())
    {
        return;
    }

    const float2 scene_size = float2(scene.get_width(), scene.get_height());
    const float2 patch_origin = float2(position) * constants.patch.xy;
    const float2 tap_step = constants.patch.xy / constants.patch.zw;
    const uint2 taps = uint2(constants.patch.zw);
    float ground = 0.0;
    float sky = 0.0;
    float sky_taps = 0.0;
    for (uint y = 0; y < taps.y; ++y)
    {
        for (uint x = 0; x < taps.x; ++x)
        {
            const float2 pixel = patch_origin +
                (float2(x, y) + 0.5) * tap_step;
            const float3 color = scene.sample(
                scene_sampler, pixel / scene_size, level(0.0)).rgb;
            const float luminance = min(
                dot(float3(0.2126, 0.7152, 0.0722), color), 60000.0);
            const uint2 normal_position = min(
                uint2(pixel),
                uint2(normal_map.get_width() - 1,
                      normal_map.get_height() - 1));
            const float flag = normal_map.read(normal_position).a;
            const bool is_sky = abs(flag) < 0.1 || abs(flag - 1.0) < 0.1;
            if (is_sky)
            {
                sky += luminance;
                sky_taps += 1.0;
            }
            else
            {
                ground += luminance;
            }
        }
    }

    const float count = float(taps.x * taps.y);
    const float ground_taps = count - sky_taps;
    meter.write(float4(
        ground_taps > 0.0 ? ground / ground_taps : 0.0,
        sky_taps > 0.0 ? sky / sky_taps : 0.0,
        sky_taps / count, 1.0), position);
}

static float4 almetal_world_resolve_exposure(
    texture2d<float> meter,
    constant ALMetalWorldExposureConstants& constants,
    float previous_ev)
{
    constexpr uint bins = 64;
    constexpr float minimum_histogram_ev = -16.0;
    constexpr float maximum_histogram_ev = 12.0;
    constexpr float bins_per_ev =
        float(bins) / (maximum_histogram_ev - minimum_histogram_ev);
    thread float histogram_weight[bins];
    thread float histogram_log[bins];
    for (uint bin = 0; bin < bins; ++bin)
    {
        histogram_weight[bin] = 0.0;
        histogram_log[bin] = 0.0;
    }

    const float2 meter_size = float2(meter.get_width(), meter.get_height());
    for (uint y = 0; y < meter.get_height(); ++y)
    {
        for (uint x = 0; x < meter.get_width(); ++x)
        {
            const float4 cell = meter.read(uint2(x, y));
            for (uint sky_part = 0; sky_part < 2; ++sky_part)
            {
                const bool is_sky = sky_part != 0;
                const float luminance = is_sky ? cell.g : cell.r;
                const float share = is_sky ? cell.b : 1.0 - cell.b;
                const float2 uv = (float2(x, y) + 0.5) / meter_size;
                const float weight = share *
                    almetal_world_meter_weight(uv, constants.meter.y) *
                    (is_sky ? constants.meter.x : 1.0);
                if (weight > 0.0 && luminance > 0.0)
                {
                    const float ev = clamp(
                        log2(max(luminance, 1.0e-10)),
                        minimum_histogram_ev, maximum_histogram_ev);
                    const uint bin = min(
                        uint(floor((ev - minimum_histogram_ev) *
                                   bins_per_ev)),
                        bins - 1);
                    histogram_weight[bin] += weight;
                    histogram_log[bin] += weight * ev;
                }
            }
        }
    }

    float total = 0.0;
    for (uint bin = 0; bin < bins; ++bin)
    {
        total += histogram_weight[bin];
    }
    const float low = total * constants.adaptation.x;
    const float high = total * constants.adaptation.y;
    float below = 0.0;
    float kept = 0.0;
    float kept_log = 0.0;
    for (uint bin = 0; bin < bins; ++bin)
    {
        const float weight = histogram_weight[bin];
        const float from = max(low, below);
        const float to = min(high, below + weight);
        if (to > from)
        {
            const float fraction = (to - from) / weight;
            kept += weight * fraction;
            kept_log += histogram_log[bin] * fraction;
        }
        below += weight;
    }

    float target = kept > 0.0
        ? kept_log / kept - constants.exposure.x
        : constants.exposure.y;
    target = clamp(target, constants.exposure.y, constants.exposure.z);
    const float rate = target > previous_ev
        ? constants.adaptation.z : constants.adaptation.w;
    const float ev = mix(
        previous_ev, target,
        1.0 - exp(-rate * constants.exposure.w));
    return float4(exp2(-ev), ev, target, 1.0);
}

kernel void almetal_world_exposure_initial(
    texture2d<float> meter [[texture(0)]],
    texture2d<float, access::write> exposure [[texture(1)]],
    constant ALMetalWorldExposureConstants& constants [[buffer(0)]],
    uint thread_index [[thread_position_in_grid]])
{
    if (thread_index == 0)
    {
        exposure.write(
            almetal_world_resolve_exposure(meter, constants, 0.0),
            uint2(0));
    }
}

kernel void almetal_world_exposure_history(
    texture2d<float> meter [[texture(0)]],
    texture2d<float> previous [[texture(1)]],
    texture2d<float, access::write> exposure [[texture(2)]],
    constant ALMetalWorldExposureConstants& constants [[buffer(0)]],
    uint thread_index [[thread_position_in_grid]])
{
    if (thread_index == 0)
    {
        exposure.write(
            almetal_world_resolve_exposure(
                meter, constants, previous.read(uint2(0)).g),
            uint2(0));
    }
}

struct ALMetalWorldDepthOfFieldConstants
{
    float4x4 inverse_projection;
    float4 focus;
    float4 limits;
    float4 shape0;
    float4 shape1;
    float4 shape2;
    float4 anamorphic;
    float4 fringe_near;
    float4 fringe_far;
    uint4 config;
};

static float almetal_world_dof_cof(
    texture2d<float> depth_texture,
    sampler point_mirror_sampler,
    float2 texcoord,
    constant ALMetalWorldDepthOfFieldConstants& constants)
{
    const float depth = depth_texture.sample(
        point_mirror_sampler, texcoord, level(0.0)).r;
    const float ndc_z = constants.config.x != 0u
        ? depth : depth * 2.0 - 1.0;
    const float4 position = constants.inverse_projection *
        float4(0.0, 0.0, ndc_z, 1.0);
    const float defocus = constants.focus.x *
        max(position.w, 0.000001) / position.z - 1.0;
    const float pixel_length = constants.focus.z * -constants.focus.x;
    const float circle = defocus *
        (constants.focus.y * 1.414 /
         (constants.focus.w * pixel_length));
    return clamp(circle, -constants.limits.x, constants.limits.x);
}

static float3 almetal_world_dof_sample_clamp(
    float3 color,
    constant ALMetalWorldDepthOfFieldConstants& constants)
{
    if (constants.shape0.x <= 0.0)
    {
        return color;
    }
    const float peak = max(color.r, max(color.g, color.b));
    return peak > constants.shape0.x
        ? color * (constants.shape0.x / peak) : color;
}

static float almetal_world_dof_sample_weight(
    float3 color,
    constant ALMetalWorldDepthOfFieldConstants& constants)
{
    if (constants.limits.w <= 0.0)
    {
        return 1.0;
    }
    const float peak = max(color.r, max(color.g, color.b));
    return 1.0 + constants.limits.w *
        max(peak - constants.limits.z, 0.0);
}

static float almetal_world_dof_shape_weight(
    float radius, float2 direction, float apodization, float2 coma)
{
    constexpr float floor_weight = 0.15;
    const float weight = 1.0 +
        apodization * (2.0 * radius * radius - 1.0) +
        dot(direction, coma) * radius;
    return max(weight, floor_weight);
}

static float almetal_world_dof_aperture_mask(
    float angle, float sine, float cosine, float radius,
    float2 cat_offset,
    constant ALMetalWorldDepthOfFieldConstants& constants)
{
    float edge = 1.0;
    if (constants.config.w >= 3u)
    {
        const float sector_position =
            fmod(angle + constants.shape0.y, constants.shape2.z) -
            constants.shape2.y;
        const float polygon = constants.shape2.w /
            max(cos(sector_position), 0.001);
        edge = mix(polygon, 1.0,
                   clamp(constants.shape0.z, 0.0, 1.0));
    }
    if (radius > edge)
    {
        return 0.0;
    }
    if (constants.shape0.w > 0.0)
    {
        const float2 point = float2(sine, cosine) * radius;
        const float2 difference = point - cat_offset;
        if (dot(difference, difference) > 1.0)
        {
            return 0.0;
        }
    }
    return 1.0;
}

static float3 almetal_world_dof_fringe(
    float3 color, float radius, float circle_sign,
    constant ALMetalWorldDepthOfFieldConstants& constants)
{
    if (constants.shape1.x <= 0.0)
    {
        return color;
    }
    const float3 tint = circle_sign < 0.0
        ? constants.fringe_far.xyz : constants.fringe_near.xyz;
    return color * mix(float3(1.0), tint,
        clamp(radius, 0.0, 1.0) * constants.shape1.x);
}

static void almetal_world_dof_accumulate(
    thread float4& sum, thread float& sum_weight,
    float4 sample_value, float radius, float circle_sign,
    float shape_weight, bool shaped,
    constant ALMetalWorldDepthOfFieldConstants& constants)
{
    float3 color = almetal_world_dof_sample_clamp(
        sample_value.rgb, constants);
    if (shaped)
    {
        color = almetal_world_dof_fringe(
            color, radius, circle_sign, constants);
    }
    const float4 weighted_color = float4(color, sample_value.a);
    const float weight = almetal_world_dof_sample_weight(
        weighted_color.rgb, constants) * shape_weight;
    sum += weight * weighted_color;
    sum_weight += weight;
}

static void almetal_world_dof_sample_far(
    thread float4& sum, thread float& sum_weight,
    texture2d<float> source, texture2d<float> depth_texture,
    sampler point_mirror_sampler,
    float minimum_circle, float2 texcoord, float radius,
    float circle_sign, float shape_weight, bool front_blur, bool shaped,
    constant ALMetalWorldDepthOfFieldConstants& constants)
{
    const float circle = almetal_world_dof_cof(
        depth_texture, point_mirror_sampler, texcoord, constants);
    const float sampled_circle = front_blur ? abs(circle) : -circle;
    if (sampled_circle > minimum_circle + 0.001)
    {
        almetal_world_dof_accumulate(
            sum, sum_weight,
            source.sample(point_mirror_sampler, texcoord, level(0.0)),
            radius, circle_sign, shape_weight, shaped, constants);
    }
}

static float4 almetal_world_dof_gather(
    ALMetalResampleVaryings input,
    texture2d<float> source,
    texture2d<float> depth_texture,
    sampler point_mirror_sampler,
    constant ALMetalWorldDepthOfFieldConstants& constants,
    bool front_blur, bool shaped)
{
    const float2 texcoord = input.texcoord;
    float4 sum = source.sample(
        point_mirror_sampler, texcoord, level(0.0));
    float circle = almetal_world_dof_cof(
        depth_texture, point_mirror_sampler, texcoord, constants);
    const float maximum_radius = max(abs(circle), 0.0001);
    const float circle_sign = circle < 0.0 ? -1.0 : 1.0;

    float2 cat_offset = 0.0;
    float2 axis_x = float2(1.0, 0.0);
    float2 axis_y = float2(0.0, 1.0);
    float2 coma = 0.0;
    float apodization = 0.0;
    float ring_density = 1.0;
    if (shaped && abs(circle) > 0.5)
    {
        const float2 screen_size = float2(
            source.get_width(), source.get_height());
        const float aspect = screen_size.x / max(screen_size.y, 1.0);
        const float2 aspect_scale = max(
            float2(aspect, 1.0 / max(aspect, 0.0001)), float2(1.0));
        const float2 field = (texcoord - 0.5) * aspect_scale;
        const float field_length = length(field);
        const float2 field_direction = field_length > 0.00001
            ? field / field_length : float2(0.0);
        const float field_radius = clamp(
            field_length / (0.5 * length(aspect_scale)), 0.0, 1.0);
        const float shape_fade = smoothstep(1.5, 4.0, maximum_radius);
        cat_offset = field_direction *
            (field_radius * constants.shape0.w);

        if (constants.shape1.z != 0.0 && field_length > 0.00001)
        {
            const float2 direction = constants.shape1.z > 0.0
                ? float2(-field_direction.y, field_direction.x)
                : field_direction;
            const float stretch = sqrt(1.0 + abs(constants.shape1.z) *
                pow(field_radius, max(constants.shape1.w, 1.0)));
            const float factor = stretch - 1.0 / stretch;
            axis_x = float2(
                1.0 / stretch + factor * direction.x * direction.x,
                factor * direction.x * direction.y);
            axis_y = float2(
                factor * direction.x * direction.y,
                1.0 / stretch + factor * direction.y * direction.y);
        }
        axis_x *= constants.anamorphic.xy;
        axis_y *= constants.anamorphic.xy;

        const float basis_f = dot(axis_x, axis_x) + dot(axis_y, axis_y);
        const float determinant =
            axis_x.x * axis_y.y - axis_x.y * axis_y.x;
        ring_density = min(sqrt(max(0.5 *
            (basis_f + sqrt(max(basis_f * basis_f -
             4.0 * determinant * determinant, 0.0))), 0.0001)), 3.0);
        apodization = constants.shape1.y * shape_fade * -circle_sign;

        if (constants.shape2.x != 0.0 && field_length > 0.00001)
        {
            const float2 gradient = float2(
                axis_y.y * field_direction.x -
                    axis_y.x * field_direction.y,
                axis_x.x * field_direction.y -
                    axis_x.y * field_direction.x);
            const float gradient_length = length(gradient);
            if (gradient_length > 0.00001)
            {
                coma = -(gradient / gradient_length) *
                    (constants.shape2.x * field_radius * shape_fade);
            }
        }
    }

    float sum_weight = shaped
        ? almetal_world_dof_shape_weight(
            0.0, float2(0.0), apodization, coma)
        : 1.0;
    sum *= sum_weight;
    const float2 screen_size = float2(
        source.get_width(), source.get_height());

    if (front_blur && circle > 0.5)
    {
        while (circle > 0.5)
        {
            const int iterations = int(max(
                1.0, circle * 3.7 * ring_density));
            const float step_angle = 2.0 * M_PI_F / float(iterations);
            const float2 turn = float2(
                sin(step_angle), cos(step_angle));
            float2 direction = float2(sin(circle), cos(circle));
            for (int tap = 0; tap < iterations; ++tap)
            {
                const float sine = direction.x;
                const float cosine = direction.y;
                direction = float2(
                    sine * turn.y + cosine * turn.x,
                    cosine * turn.y - sine * turn.x);
                const float radius = circle / maximum_radius;
                if (shaped && almetal_world_dof_aperture_mask(
                        circle + float(tap) * step_angle,
                        sine, cosine, radius, cat_offset, constants) < 0.5)
                {
                    continue;
                }
                const float2 sample_offset = shaped
                    ? circle * (sine * axis_x + cosine * axis_y)
                    : circle * float2(sine, cosine);
                const float shape_weight = shaped
                    ? almetal_world_dof_shape_weight(
                        radius, float2(sine, cosine), apodization, coma)
                    : 1.0;
                almetal_world_dof_accumulate(
                    sum, sum_weight,
                    source.sample(point_mirror_sampler,
                        texcoord + sample_offset / screen_size,
                        level(0.0)),
                    radius, circle_sign, shape_weight, shaped, constants);
            }
            circle -= 1.0;
        }
    }
    else if (circle < -0.5)
    {
        circle = abs(circle);
        while (circle > 0.5)
        {
            const int iterations = int(max(
                1.0, circle * 3.7 * ring_density));
            const float step_angle = 2.0 * M_PI_F / float(iterations);
            const float2 turn = float2(
                sin(step_angle), cos(step_angle));
            float2 direction = float2(sin(circle), cos(circle));
            for (int tap = 0; tap < iterations; ++tap)
            {
                const float sine = direction.x;
                const float cosine = direction.y;
                direction = float2(
                    sine * turn.y + cosine * turn.x,
                    cosine * turn.y - sine * turn.x);
                const float radius = circle / maximum_radius;
                if (shaped && almetal_world_dof_aperture_mask(
                        circle + float(tap) * step_angle,
                        sine, cosine, radius, cat_offset, constants) < 0.5)
                {
                    continue;
                }
                const float2 sample_offset = shaped
                    ? circle * (sine * axis_x + cosine * axis_y)
                    : circle * float2(sine, cosine);
                const float shape_weight = shaped
                    ? almetal_world_dof_shape_weight(
                        radius, float2(sine, cosine), apodization, coma)
                    : 1.0;
                almetal_world_dof_sample_far(
                    sum, sum_weight, source, depth_texture,
                    point_mirror_sampler, circle,
                    texcoord + sample_offset / screen_size,
                    radius, circle_sign, shape_weight,
                    front_blur, shaped, constants);
            }
            circle -= 1.0;
        }
    }

    float4 result = sum / max(sum_weight, 0.0001);
    result.rgb = select(
        result.rgb, float3(65504.0), isinf(result.rgb));
    result.rgb = select(result.rgb, float3(0.0), isnan(result.rgb));
    return result;
}

fragment float4 almetal_world_dof_gather_far_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> depth_texture [[texture(1)]],
    sampler point_mirror_sampler [[sampler(0)]],
    constant ALMetalWorldDepthOfFieldConstants& constants [[buffer(0)]])
{
    return almetal_world_dof_gather(
        input, source, depth_texture, point_mirror_sampler,
        constants, false, false);
}

fragment float4 almetal_world_dof_gather_near_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> depth_texture [[texture(1)]],
    sampler point_mirror_sampler [[sampler(0)]],
    constant ALMetalWorldDepthOfFieldConstants& constants [[buffer(0)]])
{
    return almetal_world_dof_gather(
        input, source, depth_texture, point_mirror_sampler,
        constants, true, false);
}

fragment float4 almetal_world_dof_gather_far_shaped_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> depth_texture [[texture(1)]],
    sampler point_mirror_sampler [[sampler(0)]],
    constant ALMetalWorldDepthOfFieldConstants& constants [[buffer(0)]])
{
    return almetal_world_dof_gather(
        input, source, depth_texture, point_mirror_sampler,
        constants, false, true);
}

fragment float4 almetal_world_dof_gather_near_shaped_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> depth_texture [[texture(1)]],
    sampler point_mirror_sampler [[sampler(0)]],
    constant ALMetalWorldDepthOfFieldConstants& constants [[buffer(0)]])
{
    return almetal_world_dof_gather(
        input, source, depth_texture, point_mirror_sampler,
        constants, true, true);
}

static float3 almetal_world_dof_blur(
    texture2d<float> blur,
    texture2d<float> depth_texture,
    sampler point_mirror_sampler,
    float2 texcoord,
    constant ALMetalWorldDepthOfFieldConstants& constants,
    bool front_blur)
{
    const float2 size = float2(blur.get_width(), blur.get_height());
    const float2 position = texcoord * size - 0.5;
    const float2 fraction = fract(position);
    const int2 low = int2(floor(position));
    const int2 high = int2(size) - 1;
    const int2 taps[4] = {
        low, low + int2(1, 0), low + int2(0, 1), low + int2(1, 1),
    };
    const float weights[4] = {
        (1.0 - fraction.x) * (1.0 - fraction.y),
        fraction.x * (1.0 - fraction.y),
        (1.0 - fraction.x) * fraction.y,
        fraction.x * fraction.y,
    };
    float3 sum = 0.0;
    float sum_weight = 0.0;
    float3 filtered = 0.0;
    for (uint tap = 0; tap < 4; ++tap)
    {
        const int2 coordinate = clamp(taps[tap], int2(0), high);
        const float3 value = blur.read(uint2(coordinate)).rgb;
        filtered += value * weights[tap];
        float weight = weights[tap];
        if (!front_blur)
        {
            const float circle = almetal_world_dof_cof(
                depth_texture, point_mirror_sampler,
                (float2(coordinate) + 0.5) / size, constants);
            weight = circle > 0.0 ? 0.0 : weight;
        }
        sum += value * weight;
        sum_weight += weight;
    }
    return sum_weight > 0.0 ? sum / sum_weight : filtered;
}

static float4 almetal_world_dof_combine(
    ALMetalResampleVaryings input,
    texture2d<float> blur,
    texture2d<float> sharp_source,
    texture2d<float> depth_texture,
    sampler point_mirror_sampler,
    constant ALMetalWorldDepthOfFieldConstants& constants,
    bool front_blur)
{
    const float2 texcoord = input.texcoord;
    const float4 sharp = sharp_source.sample(
        point_mirror_sampler, texcoord, level(0.0));
    float3 result = sharp.rgb;
    const float circle = almetal_world_dof_cof(
        depth_texture, point_mirror_sampler, texcoord, constants);
    const float amount = front_blur
        ? min(abs(circle) * constants.limits.y * constants.limits.y, 1.0)
        : (circle > 0.0 ? 0.0 :
           min(abs(circle) * constants.limits.y * constants.limits.y, 1.0));
    if (amount > 0.25 && amount < 0.75)
    {
        const float transition_circle = amount / constants.limits.y;
        const float2 screen_size = float2(
            sharp_source.get_width(), sharp_source.get_height());
        float3 transition = sharp_source.sample(
            point_mirror_sampler,
            texcoord + float2(transition_circle) / screen_size,
            level(0.0)).rgb;
        transition += sharp_source.sample(
            point_mirror_sampler,
            texcoord + float2(-transition_circle, transition_circle) /
                screen_size, level(0.0)).rgb;
        transition += sharp_source.sample(
            point_mirror_sampler,
            texcoord + float2(transition_circle, -transition_circle) /
                screen_size, level(0.0)).rgb;
        transition += sharp_source.sample(
            point_mirror_sampler,
            texcoord - float2(transition_circle) / screen_size,
            level(0.0)).rgb;
        result = mix(result, transition * 0.25, amount);
    }
    if (amount > 0.0)
    {
        result = mix(result, almetal_world_dof_blur(
            blur, depth_texture, point_mirror_sampler,
            texcoord, constants, front_blur), amount);
    }
    return float4(result, sharp.a);
}

fragment float4 almetal_world_dof_combine_far_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> blur [[texture(0)]],
    texture2d<float> sharp_source [[texture(1)]],
    texture2d<float> depth_texture [[texture(2)]],
    sampler point_mirror_sampler [[sampler(0)]],
    constant ALMetalWorldDepthOfFieldConstants& constants [[buffer(0)]])
{
    return almetal_world_dof_combine(
        input, blur, sharp_source, depth_texture,
        point_mirror_sampler, constants, false);
}

fragment float4 almetal_world_dof_combine_near_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> blur [[texture(0)]],
    texture2d<float> sharp_source [[texture(1)]],
    texture2d<float> depth_texture [[texture(2)]],
    sampler point_mirror_sampler [[sampler(0)]],
    constant ALMetalWorldDepthOfFieldConstants& constants [[buffer(0)]])
{
    return almetal_world_dof_combine(
        input, blur, sharp_source, depth_texture,
        point_mirror_sampler, constants, true);
}

struct ALMetalWorldLensFlareStateConstants
{
    // source width/height, clamped dt, fade time.
    float4 screen_time;
    // sun UV, edge visibility, probe radius in screen heights.
    float4 sun_visibility_radius;
    // x = reverse-Z.
    uint4 config;
};

fragment float4 almetal_world_lens_flare_state_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> depth_texture [[texture(1)]],
    texture2d<float> previous [[texture(2)]],
    sampler point_clamp_sampler [[sampler(0)]],
    constant ALMetalWorldLensFlareStateConstants& constants [[buffer(0)]])
{
    constexpr float3 luma_weights = float3(0.2126, 0.7152, 0.0722);
    constexpr float golden_angle = 2.39996322972865332;
    constexpr uint tap_count = 256u;
    constexpr float tap_falloff = 8.0;
    constexpr float luminance_gate = 2.0;

    const float2 source_size = max(constants.screen_time.xy, float2(1.0));
    const float2 sun_uv = constants.sun_visibility_radius.xy;
    const float2 radius_uv = constants.sun_visibility_radius.w *
        float2(source_size.y / source_size.x, 1.0);
    float weight_sum = 0.0;
    float3 energy = float3(0.0);
    float peak = 0.0;
    for (uint index = 0u; index < tap_count; ++index)
    {
        const float radius = sqrt((float(index) + 0.5) /
                                  float(tap_count));
        const float angle = float(index) * golden_angle;
        const float2 tap = float2(cos(angle), sin(angle)) * radius;
        const float weight = exp(-tap_falloff * dot(tap, tap));
        weight_sum += weight;
        const float2 uv = clamp(sun_uv + tap * radius_uv,
                                float2(0.0), float2(1.0));
        const float depth = depth_texture.sample(
            point_clamp_sampler, uv, level(0.0)).r;
        const bool sky = constants.config.x != 0u
            ? depth <= 0.0 : depth >= 1.0;
        if (sky)
        {
            const float3 color = source.sample(
                point_clamp_sampler, uv, level(0.0)).rgb;
            const float luminance = dot(color, luma_weights);
            const float3 overbright = color *
                (max(luminance - luminance_gate, 0.0) /
                 max(luminance, 1.0e-4));
            energy += weight * overbright;
            peak = max(peak, dot(overbright, luma_weights));
        }
    }
    const float3 target = energy / weight_sum *
        constants.sun_visibility_radius.z;

    const float4 previous0 = previous.read(uint2(0, 0));
    const float4 previous1 = previous.read(uint2(1, 0));
    const float dt = clamp(constants.screen_time.z, 0.0, 0.25);
    const float fade = max(constants.screen_time.w, 0.05);
    const float previous_luminance = dot(previous0.rgb, luma_weights);
    const float target_luminance = dot(target, luma_weights);
    const float reference = max(
        max(previous1.g * exp(-dt / 2.0), target_luminance),
        previous_luminance);

    float packed_instability = previous0.a;
    float instability = abs(packed_instability);
    const float previous_sign = sign(packed_instability);
    float anchor = previous1.b;
    const float delta = (target_luminance - anchor) /
        max(max(peak, reference), 1.0e-6);
    const float current_sign = abs(delta) > 0.1 ? sign(delta) : 0.0;
    if (current_sign != 0.0)
    {
        anchor = target_luminance;
    }
    const bool reversal = current_sign != 0.0 && previous_sign != 0.0 &&
        current_sign != previous_sign;
    instability = min(instability * exp(-dt) +
                      (reversal ? 0.35 : 0.0), 1.0);
    const float stored_sign = current_sign != 0.0
        ? current_sign : previous_sign;
    packed_instability = stored_sign != 0.0
        ? stored_sign * max(instability, 1.0e-3) : 0.0;

    const bool rising = target_luminance > previous_luminance;
    float tau = fade * (rising ? (1.0 / 3.0) : 0.2);
    tau *= 1.0 + 20.0 * max(instability - 0.30, 0.0);
    float alpha = 1.0 - exp(-dt / tau);
    const float slew = (rising ? 1.0 : (1.0 / 0.6)) / fade;
    const float cap = slew * dt * reference;
    const float difference = abs(target_luminance - previous_luminance);
    if (difference > 1.0e-9)
    {
        alpha = min(alpha, cap / difference);
    }
    float3 drive = max(mix(previous0.rgb, target, alpha), float3(0.0));
    if (target_luminance <= 0.0 && dot(drive, luma_weights) < 1.0e-4)
    {
        drive = float3(0.0);
    }

    return input.position.x < 1.0
        ? float4(drive, packed_instability)
        : float4(target_luminance, reference, anchor, dt);
}

struct ALMetalWorldLensDirtConstants
{
    float4 screen_seed_grime;
    float4 detail_toe;
    float4 gain;
};

static float almetal_lens_dirt_hash21(float2 p, float seed)
{
    return fract(sin(p.x * 127.1 + p.y * 311.7 + seed * 74.7) *
                 43758.5453);
}

static float2 almetal_lens_dirt_hash22(float2 p, float seed)
{
    return fract(float2(
        sin(p.x * 127.1 + p.y * 311.7 + seed * 74.7),
        sin(p.x * 269.5 + p.y * 183.3 + seed * 51.3)) *
        43758.5453);
}

static float almetal_lens_dirt_screen(float a, float b)
{
    return 1.0 - (1.0 - a) * (1.0 - b);
}

static float almetal_lens_dirt_motes(
    float2 uv, float cells, float minimum_radius, float maximum_radius,
    float softness, float seed, float aspect, float density, float bias)
{
    float accumulated = 0.0;
    const float2 cell = floor(uv * cells);
    for (int offset_x = -1; offset_x <= 1; ++offset_x)
    {
        for (int offset_y = -1; offset_y <= 1; ++offset_y)
        {
            const float2 grid = cell + float2(
                float(offset_x), float(offset_y));
            const float keep = step(
                almetal_lens_dirt_hash21(grid, seed + 57.0), density);
            const float2 feature =
                (grid + almetal_lens_dirt_hash22(grid, seed)) / cells;
            const float radius = minimum_radius +
                (maximum_radius - minimum_radius) * pow(
                    almetal_lens_dirt_hash21(grid, seed + 19.0), bias);
            const float2 delta = float2(
                (uv.x - feature.x) * aspect, uv.y - feature.y);
            const float distance = length(delta);
            const float value = clamp(
                (radius - distance) /
                    (radius * softness + 1.0e-9),
                0.0, 1.0);
            accumulated = almetal_lens_dirt_screen(
                accumulated,
                value * value * (3.0 - 2.0 * value) * keep);
        }
    }
    return accumulated;
}

static float almetal_lens_dirt_noise(
    float2 uv, float frequency, float seed)
{
    const float2 p = uv * frequency;
    const float2 index = floor(p);
    float2 fraction = p - index;
    fraction = fraction * fraction * (3.0 - 2.0 * fraction);
    const float a = almetal_lens_dirt_hash21(index, seed);
    const float b = almetal_lens_dirt_hash21(
        index + float2(1.0, 0.0), seed);
    const float c = almetal_lens_dirt_hash21(
        index + float2(0.0, 1.0), seed);
    const float d = almetal_lens_dirt_hash21(
        index + float2(1.0, 1.0), seed);
    return mix(mix(a, b, fraction.x), mix(c, d, fraction.x), fraction.y);
}

static float almetal_lens_dirt_fbm3(
    float2 uv, float frequency, float seed)
{
    float total = 0.0;
    float amplitude = 1.0;
    float normalization = 0.0;
    for (int octave = 0; octave < 3; ++octave)
    {
        total += amplitude * almetal_lens_dirt_noise(
            uv, frequency * exp2(float(octave)),
            seed + float(octave) * 13.0);
        normalization += amplitude;
        amplitude *= 0.5;
    }
    return total / normalization;
}

static float almetal_lens_dirt_segment_distance(
    float2 uv, float2 start, float2 end, float aspect)
{
    const float2 position = float2(
        (uv.x - start.x) * aspect, uv.y - start.y);
    const float2 segment = float2(
        (end.x - start.x) * aspect, end.y - start.y);
    const float amount = clamp(
        dot(position, segment) /
            max(dot(segment, segment), 1.0e-9),
        0.0, 1.0);
    return length(position - segment * amount);
}

static float almetal_lens_dirt_lines(
    float2 uv, int count, float line_length, float width, float seed,
    float aspect, float wander)
{
    float2 warped = uv;
    if (wander > 0.0)
    {
        warped += (float2(
            almetal_lens_dirt_fbm3(uv, 3.0, seed + 7.0),
            almetal_lens_dirt_fbm3(uv, 3.0, seed + 31.0)) - 0.5) *
            wander;
    }
    float accumulated = 0.0;
    for (int index = 0; index < 32; ++index)
    {
        if (index >= count)
        {
            break;
        }
        const float value = float(index);
        const float2 start = float2(
            almetal_lens_dirt_hash21(float2(value, 1.0), seed),
            almetal_lens_dirt_hash21(float2(value, 2.0), seed));
        const float angle = almetal_lens_dirt_hash21(
            float2(value, 3.0), seed) * 6.2831853;
        const float length_scale = line_length * (0.4 + 0.6 *
            almetal_lens_dirt_hash21(float2(value, 4.0), seed));
        const float2 end = start +
            float2(cos(angle), sin(angle)) * length_scale;
        const float line_width = width * (0.5 + 0.5 *
            almetal_lens_dirt_hash21(float2(value, 5.0), seed));
        const float distance = almetal_lens_dirt_segment_distance(
            warped, start, end, aspect);
        const float x = clamp(
            (line_width - distance) / max(line_width, 1.0e-9),
            0.0, 1.0);
        accumulated = max(
            accumulated,
            x * x * (3.0 - 2.0 * x) * (0.55 + 0.45 *
                almetal_lens_dirt_hash21(
                    float2(value, 6.0), seed)));
    }
    return accumulated;
}

fragment float4 almetal_world_lens_dirt_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    constant ALMetalWorldLensDirtConstants& constants [[buffer(0)]])
{
    // GL's procedural UV has its origin at the lower-left, while the Metal
    // render target's row zero is the screen top. Flip only the generation
    // coordinate so both plates put the same seeded feature on screen.
    const float2 uv = float2(input.texcoord.x, 1.0 - input.texcoord.y);
    const float aspect = constants.screen_seed_grime.x /
        max(constants.screen_seed_grime.y, 1.0);
    const float seed = constants.screen_seed_grime.z;
    const float grime = constants.screen_seed_grime.w;
    const float mote_scale = max(constants.detail_toe.x, 0.05);
    float clump = almetal_lens_dirt_fbm3(uv, 5.0, seed + 101.0);
    clump = 0.62 + 0.76 * clamp((clump - 0.34) / 0.36, 0.0, 1.0);

    float image = 0.0;
    image = almetal_lens_dirt_screen(image, almetal_lens_dirt_motes(
        uv, 4.5 / mote_scale, 0.050 * mote_scale, 0.140 * mote_scale,
        0.45, seed, aspect, 0.80 * grime, 1.7) * 0.46 * clump);
    image = almetal_lens_dirt_screen(image, almetal_lens_dirt_motes(
        uv, 9.0 / mote_scale, 0.022 * mote_scale, 0.064 * mote_scale,
        0.42, seed + 3.0, aspect, 0.75 * grime, 1.9) * 0.50 * clump);
    image = almetal_lens_dirt_screen(image, almetal_lens_dirt_motes(
        uv, 20.0 / mote_scale, 0.009 * mote_scale,
        0.028 * mote_scale, 0.40, seed + 5.0, aspect,
        0.60 * grime, 2.1) * 0.44 * clump);
    image = almetal_lens_dirt_screen(image, almetal_lens_dirt_motes(
        uv, 90.0, 0.0011, 0.0030, 0.55, seed + 9.0, aspect,
        0.34 * grime, 1.5) * 0.75);

    const float smudge = almetal_lens_dirt_fbm3(
        uv, 1.6, seed + 21.0);
    image = almetal_lens_dirt_screen(
        image, pow(clamp((smudge - 0.50) / 0.34, 0.0, 1.0), 1.4) *
            0.34 * constants.detail_toe.y);
    image = almetal_lens_dirt_screen(
        image, almetal_lens_dirt_lines(
            uv, 12, 0.20, 0.0014, seed + 41.0,
            aspect, 0.06) * 0.62);
    const int scratches = int(round(constants.detail_toe.z));
    if (scratches > 0)
    {
        image = almetal_lens_dirt_screen(
            image, almetal_lens_dirt_lines(
                uv, scratches, 0.55, 0.0022, seed + 57.0,
                aspect, 0.0));
    }

    const float2 center = float2(
        (uv.x - 0.5) * aspect, uv.y - 0.5);
    const float radius = clamp(
        length(center) / (0.5 * length(float2(aspect, 1.0))),
        0.0, 1.0);
    image *= 0.22 + 0.78 * pow(radius, 0.75);
    const float value = clamp(
        constants.gain.x * pow(clamp(image, 0.0, 1.0),
                               constants.detail_toe.w),
        0.0, 1.0);
    return float4(value, value, value, 1.0);
}

struct ALMetalWorldDisplayPostConstants
{
    // static exposure, adapted exposure, tonemap mix, tonemap type
    float4 post;
    float4 tonemap_params;
    // non-HDR dither, bloom strength, halation strength, cross strength
    float4 display;
    float4 halation_tint;
    // cutout-premultiplied, HDR/legacy flags, animate dither, frame id
    uint4 config;
    // squared/scaled amount, reciprocal falloff, anisotropy, legacy gamma
    float4 chromatic;
    // angle sine/cosine, red radial/tangential offset
    float4 chromatic_angle_offset_r;
    // blue radial/tangential offset
    float4 chromatic_offset_b;
    float4 flare_sun_strength;
    float4 flare_streak;
    float4 flare_streak_tint;
    float4 flare_glow_ghost;
    float4 flare_ghost_halo;
    float4 flare_halo_starburst;
    float4 flare_star_light;
    float4 lens_dirt;
    float4 grade_white_balance;
    float4 grade_lift;
    float4 grade_inverse_gamma;
    float4 grade_gain;
    float4 grade_shadow_ratio;
    float4 grade_midtone_ratio;
    float4 grade_highlight_ratio;
    float4 grade_split0;
    float4 grade_split1;
    float4 grade_display0;
    float4 grade_display1;
    float4 grade_lut;
    float4 grade_curve;
};

static float4 almetal_chromatic_aberration(
    texture2d<float> scene,
    sampler scene_sampler,
    float2 uv,
    float amount,
    float reciprocal_falloff,
    float anisotropy,
    float2 angle_sin_cos,
    float2 red_offset_control,
    float2 blue_offset_control)
{
    if (amount <= 0.0)
    {
        return scene.sample(scene_sampler, uv);
    }

    float2 direction = uv - 0.5;
    direction *= float2(1.0 - max(anisotropy, 0.0),
                        1.0 - max(-anisotropy, 0.0));

    const float aspect = float(scene.get_width()) /
                         max(float(scene.get_height()), 1.0);
    const float2 scale = max(
        float2(aspect, 1.0 / max(aspect, 1.0e-4)), float2(1.0));
    const float normalized_distance = clamp(
        length(direction * scale) / (0.5 * length(scale)), 0.0, 1.0);
    const float strength = amount * pow(
        normalized_distance, reciprocal_falloff);

    const float sine = angle_sin_cos.x;
    const float cosine = angle_sin_cos.y;
    const float2 radial = float2(
        cosine * direction.x - sine * direction.y,
        sine * direction.x + cosine * direction.y);
    const float2 tangential = float2(-radial.y, radial.x);
    const float2 red_offset = radial * red_offset_control.x +
        tangential * red_offset_control.y;
    const float2 blue_offset = radial * blue_offset_control.x +
        tangential * blue_offset_control.y;

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

struct ALMetalWorldCASConstants
{
    float4 params;
};

struct ALMetalWorldFXAAConstants
{
    uint4 config;
};

struct ALMetalWorldSMAAEdgeConstants
{
    float4 predication;
    uint4 config;
};

struct ALMetalWorldSMAAWeightConstants
{
    uint4 config;
};

// The four viewer permutations are FXAA 3.11 presets 12, 23, 28 and 39.
// Keeping their authored search sequences in one uniform table produces the
// same taps without maintaining four otherwise-identical Metal functions.
constant uint almetal_world_fxaa_step_counts[4] = {5u, 6u, 11u, 12u};
constant float almetal_world_fxaa_steps[4][12] = {
    {1.0, 1.5, 2.0, 4.0, 12.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 1.5, 2.0, 2.0, 2.0, 8.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0, 0.0},
    {1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0},
};

static float almetal_world_display_hash12(float2 p)
{
    float3 p3 = fract(float3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

static float2 almetal_world_display_noise_offset(
    uint frame_id, bool animate)
{
    const float frame = float(animate ? frame_id : 0u);
    return float2(
        almetal_world_display_hash12(float2(frame, 13.7)),
        almetal_world_display_hash12(float2(frame, 91.3))) * 1024.0;
}

static float3 almetal_world_display_dither(
    float3 color, float2 position, float amount,
    uint frame_id, bool animate)
{
    if (amount <= 0.0)
    {
        return color;
    }
    const float2 p = position + almetal_world_display_noise_offset(
        frame_id, animate);
    const float noise = fract(
        52.9829189 * fract(dot(p, float2(0.06711056, 0.00583715)))) *
        2.0 - 1.0;
    const float triangular = sign(noise) *
        (1.0 - sqrt(1.0 - abs(noise)));
    const float3 room = clamp(
        min(color, 1.0 - color) * 255.0,
        float3(0.0), float3(1.0));
    return color + triangular * (amount / 255.0) * room;
}

static float almetal_world_grade_ramp(float value, float2 ramp)
{
    const float t = clamp(value * ramp.x + ramp.y, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

static float3 almetal_world_grade_rgb_to_hsv(float3 color)
{
    const float4 k = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    const float4 p = mix(
        float4(color.bg, k.wz), float4(color.gb, k.xy),
        step(color.b, color.g));
    const float4 q = mix(
        float4(p.xyw, color.r), float4(color.r, p.yzx),
        step(p.x, color.r));
    const float d = q.x - min(q.w, q.y);
    return float3(
        abs(q.z + (q.w - q.y) / (6.0 * d + 1.0e-10)),
        d / (q.x + 1.0e-10), q.x);
}

static float3 almetal_world_grade_hsv_to_rgb(float3 color)
{
    const float4 k = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    const float3 p = abs(fract(color.xxx + k.xyz) * 6.0 - k.www);
    return color.z * mix(k.xxx, clamp(p - k.xxx, 0.0, 1.0), color.y);
}

static float3 almetal_world_grade_linear_pre(
    float3 color, constant ALMetalWorldDisplayPostConstants& constants)
{
    if (any(constants.grade_white_balance.rgb != float3(1.0)))
    {
        color *= constants.grade_white_balance.rgb;
    }
    if (any(constants.grade_lift.rgb != float3(0.0)) ||
        any(constants.grade_gain.rgb != float3(1.0)) ||
        any(constants.grade_inverse_gamma.rgb != float3(1.0)))
    {
        color = color * constants.grade_gain.rgb + constants.grade_lift.rgb;
        color = pow(max(color, float3(0.0)),
                    constants.grade_inverse_gamma.rgb);
    }
    return color;
}

static float3 almetal_world_grade_split(
    float3 color, constant ALMetalWorldDisplayPostConstants& constants)
{
    const float midtone_amount = constants.grade_split0.x;
    const float split_amount = constants.grade_split0.y;
    if (split_amount <= 0.0 && midtone_amount <= 0.0)
    {
        return color;
    }
    const float3 luma_weights = float3(0.2126, 0.7152, 0.0722);
    const float luma = dot(color, luma_weights);
    const float high = almetal_world_grade_ramp(
        luma, constants.grade_split1.xy);
    const float low = 1.0 - almetal_world_grade_ramp(
        luma, constants.grade_split0.zw);
    const float middle = max(1.0 - high - low, 0.0);
    color = mix(color, constants.grade_shadow_ratio.rgb * luma,
                low * split_amount);
    color = mix(color, constants.grade_midtone_ratio.rgb * luma,
                middle * midtone_amount);
    return mix(color, constants.grade_highlight_ratio.rgb * luma,
               high * split_amount);
}

static float3 almetal_world_grade_display(
    float3 color, constant ALMetalWorldDisplayPostConstants& constants,
    texture2d_array<float> color_lut,
    texture2d<float> tone_curve,
    sampler grade_sampler)
{
    const float3 luma_weights = float3(0.2126, 0.7152, 0.0722);
    if (constants.grade_split1.z != 1.0 ||
        constants.grade_split1.w != 0.0)
    {
        color = color * constants.grade_split1.z +
            constants.grade_split1.w;
    }
    if (constants.grade_display0.x != 1.0 ||
        constants.grade_display0.y != 0.0)
    {
        color = color * constants.grade_display0.x +
            constants.grade_display0.y;
    }

    if (constants.grade_display0.z != 0.0 ||
        constants.grade_display0.w != 0.0)
    {
        const float luma = dot(color, luma_weights);
        const float highlight_mask = smoothstep(0.5, 1.0, luma);
        const float shadow_mask = 1.0 - smoothstep(0.0, 0.5, luma);
        color += (float3(1.0) - color) *
            (constants.grade_display0.z * highlight_mask);
        color += color * (constants.grade_display0.w * shadow_mask);
    }

    if (constants.grade_display1.x != 1.0)
    {
        const float saturation_luma = dot(color, luma_weights);
        color = mix(float3(saturation_luma), color,
                    constants.grade_display1.x);
    }
    if (constants.grade_display1.y != 0.0)
    {
        const float vibrance_luma = dot(color, luma_weights);
        const float maximum = max(color.r, max(color.g, color.b));
        const float minimum = min(color.r, min(color.g, color.b));
        color = mix(
            float3(vibrance_luma), color,
            1.0 + constants.grade_display1.y *
                (1.0 - (maximum - minimum)));
        color = clamp(color, float3(0.0), float3(1.0));
    }
    if (constants.grade_display1.z != 0.0)
    {
        float3 hsv = almetal_world_grade_rgb_to_hsv(color);
        hsv.x = fract(hsv.x + constants.grade_display1.z);
        color = almetal_world_grade_hsv_to_rgb(hsv);
    }

    if (constants.grade_lut.w > 0.0)
    {
        const float3 original = color;
        if (constants.grade_lut.y > 0.5)
        {
            color.g = 1.0 - color.g;
        }
        if (constants.grade_lut.z > 0.5)
        {
            color = color.rbg;
        }
        const float edge = constants.grade_lut.x;
        const float inverse_edge = 1.0 / edge;
        const float2 uv = color.rg * (1.0 - inverse_edge) +
            0.5 * inverse_edge;
        // GL's clamp-to-edge sampler also clamps its third coordinate. Metal
        // array slices are selected manually, so make that implicit 3D-texture
        // behavior explicit before converting to an unsigned layer index.
        const float blue = clamp(color.b, 0.0, 1.0) * (edge - 1.0);
        const uint low_slice = uint(floor(blue));
        const uint high_slice = min(low_slice + 1u, uint(edge) - 1u);
        const float3 low = color_lut.sample(
            grade_sampler, uv, low_slice, level(0.0)).rgb;
        const float3 high = color_lut.sample(
            grade_sampler, uv, high_slice, level(0.0)).rgb;
        color = mix(
            original, mix(low, high, fract(blue)),
            constants.grade_lut.w);
    }

    if (constants.grade_curve.z > 0.0)
    {
        const float3 u = color * constants.grade_curve.x +
            constants.grade_curve.y;
        const float3 curved = float3(
            tone_curve.sample(
                grade_sampler, float2(u.r, 0.5), level(0.0)).r,
            tone_curve.sample(
                grade_sampler, float2(u.g, 0.5), level(0.0)).g,
            tone_curve.sample(
                grade_sampler, float2(u.b, 0.5), level(0.0)).b);
        color = mix(color, curved, constants.grade_curve.z);
    }
    return color;
}

static float3 almetal_world_lens_flare(
    float2 uv, texture2d<float> state,
    constant ALMetalWorldDisplayPostConstants& constants,
    float2 resolution)
{
    const float strength = constants.flare_sun_strength.z;
    if (strength <= 0.0)
    {
        return float3(0.0);
    }
    const float3 sun_color = state.read(uint2(0, 0)).rgb;
    if (max(sun_color.r, max(sun_color.g, sun_color.b)) <= 0.0)
    {
        return float3(0.0);
    }

    const float2 sun_uv = constants.flare_sun_strength.xy;
    const float aspect = resolution.x / max(resolution.y, 1.0);
    const float2 delta = uv - sun_uv;
    float3 flare = float3(0.0);

    const float glow_intensity = constants.flare_glow_ghost.x;
    if (glow_intensity > 0.0)
    {
        const float2 glow_delta = float2(delta.x * aspect, delta.y);
        const float radius = constants.flare_glow_ghost.y;
        const float radius_squared = max(radius * radius, 1.0e-8);
        const float glow = exp(-dot(glow_delta, glow_delta) *
            constants.flare_glow_ghost.z / radius_squared);
        flare += sun_color * glow * glow_intensity *
            constants.flare_sun_strength.w;
    }

    const float streak_width = max(constants.flare_streak.z, 1.0e-5);
    const float streak_length = max(constants.flare_streak.x, 1.0e-5);
    const float spread = max(constants.flare_streak_tint.w, 0.0);
    const float fringe_width = streak_width + spread * 0.5;
    if (constants.flare_streak.w > 0.0 &&
        abs(delta.y) < fringe_width * 8.0)
    {
        const float horizontal = abs(delta.x) * aspect;
        const float vertical = abs(delta.y);
        const float vertical_falloff = exp(
            -vertical * vertical / (streak_width * streak_width));
        const float horizontal_falloff = exp(
            -horizontal * constants.flare_streak.y / streak_length);
        const float streak = vertical_falloff * horizontal_falloff;
        if (spread > 0.0)
        {
            const float vertical_red = abs(delta.y + spread);
            const float vertical_blue = abs(delta.y - spread);
            const float red = exp(-vertical_red * vertical_red /
                (fringe_width * fringe_width)) * horizontal_falloff;
            const float blue = exp(-vertical_blue * vertical_blue /
                (fringe_width * fringe_width)) * horizontal_falloff;
            flare.r += sun_color.r * red * constants.flare_streak_tint.r *
                constants.flare_streak.w;
            flare.g += sun_color.g * streak * constants.flare_streak_tint.g *
                constants.flare_streak.w;
            flare.b += sun_color.b * blue * constants.flare_streak_tint.b *
                constants.flare_streak.w;
        }
        else
        {
            flare += sun_color * streak * constants.flare_streak_tint.rgb *
                constants.flare_streak.w;
        }
    }

    const float ghost_intensity = constants.flare_glow_ghost.w;
    const uint ghost_count = uint(round(constants.flare_ghost_halo.x));
    if (ghost_intensity > 0.0 && ghost_count > 0u)
    {
        const float2 ghost_vector = float2(0.5) - sun_uv;
        for (uint index = 0u; index < ghost_count; ++index)
        {
            const float scale = 1.0 / float(index + 1u);
            const float2 ghost_uv = sun_uv + ghost_vector *
                constants.flare_ghost_halo.y * float(index + 1u);
            const float radius = 0.04 * scale + 0.02;
            float2 ghost_delta = uv - ghost_uv;
            ghost_delta.x *= aspect;
            const float disk = 1.0 - smoothstep(
                radius * 0.5, radius, length(ghost_delta));
            flare += sun_color * disk * scale * ghost_intensity;
        }
    }

    const float halo_intensity = constants.flare_ghost_halo.z;
    const float halo_radius = constants.flare_ghost_halo.w;
    if (halo_intensity > 0.0 && halo_radius > 0.0)
    {
        const float2 halo_center = float2(0.5) +
            (float2(0.5) - sun_uv);
        float2 halo_delta = uv - halo_center;
        halo_delta.x *= aspect;
        const float halo_width = max(
            constants.flare_halo_starburst.x, 0.01);
        float halo = 1.0 - abs(length(halo_delta) - halo_radius) /
            halo_width;
        halo = clamp(halo, 0.0, 1.0);
        halo *= halo;
        flare += sun_color * halo * 0.3 * halo_intensity;
    }

    const float starburst_intensity = constants.flare_halo_starburst.y;
    if (starburst_intensity > 0.0)
    {
        float2 star_delta = delta;
        star_delta.x *= aspect;
        const float distance = length(star_delta);
        const float radial = exp(-distance * max(
            constants.flare_star_light.x, 0.01));
        const float core_fade = smoothstep(
            0.0, max(constants.flare_glow_ghost.y, 1.0e-4), distance);
        const float envelope = radial * core_fade;
        if (envelope > 1.0e-5)
        {
            const float angle = atan2(star_delta.y, star_delta.x);
            const float primary = max(
                constants.flare_halo_starburst.z, 1.0);
            const float sharpness = max(
                constants.flare_halo_starburst.w, 1.0);
            const float pattern =
                pow(abs(cos(angle * primary)), sharpness) * 0.5 +
                pow(abs(cos(angle * primary * 2.0 + 0.5)),
                    sharpness * 1.33) * 0.3 +
                pow(abs(cos(angle * primary * 4.0 + 1.0)),
                    sharpness * 1.66) * 0.2;
            flare += sun_color * pattern * envelope *
                starburst_intensity * constants.flare_sun_strength.w;
        }
    }

    return max(flare * strength * constants.flare_star_light.yzw * 0.15,
               float3(0.0));
}

static float4 almetal_world_display_post(
    ALMetalResampleVaryings input,
    texture2d<float> source,
    sampler source_sampler,
    float4 bloom_sample,
    float3 cross_filter_sample,
    texture2d_array<float> color_lut,
    texture2d<float> tone_curve,
    texture2d<float> lens_flare_state,
    texture2d<float> lens_dirt,
    sampler grade_sampler,
    constant ALMetalWorldDisplayPostConstants& constants,
    float adapted_exposure)
{
    const bool hdr = (constants.config.y & 1u) != 0u;
    const bool legacy_gamma = (constants.config.y & 2u) != 0u;
    const bool bypass_tonemap = (constants.config.y & 4u) != 0u;
    float4 color = almetal_chromatic_aberration(
        source, source_sampler, input.texcoord,
        constants.chromatic.x, constants.chromatic.y,
        constants.chromatic.z, constants.chromatic_angle_offset_r.xy,
        constants.chromatic_angle_offset_r.zw,
        constants.chromatic_offset_b.xy);
    const float3 flare = almetal_world_lens_flare(
        input.texcoord, lens_flare_state, constants,
        float2(source.get_width(), source.get_height()));
    color.rgb += flare;
    float3 lens_light = flare * constants.lens_dirt.z;
    const float coverage = clamp(color.a, 0.0, 1.0);
    if (constants.config.x != 0)
    {
        color.rgb = coverage > 0.0
            ? color.rgb / coverage : float3(0.0);
    }

    if (hdr)
    {
        // Match colorCorrectF's BLOOM_COMPOSITE permutation: the completed
        // pyramid is added in linear light after cutout unpremultiplication
        // and before exposure/tonemapping.
        const float3 bloom_term =
            bloom_sample.rgb * constants.display.y +
            bloom_sample.a * constants.display.z *
                constants.halation_tint.rgb +
            cross_filter_sample * constants.display.w *
                constants.display.y;
        color.rgb += bloom_term;
        lens_light += bloom_term * constants.lens_dirt.y;
    }

    if (constants.lens_dirt.x > 0.0)
    {
        color.rgb += lens_light * lens_dirt.sample(
            grade_sampler, input.texcoord, level(0.0)).r *
            constants.lens_dirt.x;
    }

    if (hdr && !legacy_gamma && !bypass_tonemap)
    {
        color.rgb *= constants.post.x * adapted_exposure;
    }

    const bool color_grade = constants.grade_display1.w != 0.0;
    if (color_grade)
    {
        color.rgb = almetal_world_grade_linear_pre(color.rgb, constants);
    }

    if (hdr && !legacy_gamma && !bypass_tonemap)
    {
        const float3 clamped = clamp(
            color.rgb, float3(0.0), float3(1.0));
        const uint tonemap_type = uint(round(constants.post.w));
        color.rgb = mix(
            clamped,
            clamp(almetal_pbr_tonemap(
                      color.rgb, tonemap_type,
                      constants.tonemap_params),
                  float3(0.0), float3(1.0)),
            constants.post.z);
    }
    else
    {
        color.rgb = clamp(color.rgb, float3(0.0), float3(1.0));
    }

    if (color_grade)
    {
        color.rgb = almetal_world_grade_split(color.rgb, constants);
    }

    color.rgb = almetal_linear_to_srgb(color.rgb);
    if (legacy_gamma)
    {
        const float3 inverse = float3(1.0) - clamp(
            color.rgb, float3(0.0), float3(1.0));
        color.rgb = float3(1.0) - pow(
            inverse, float3(constants.chromatic.w));
    }
    if (color_grade)
    {
        color.rgb = almetal_world_grade_display(
            color.rgb, constants, color_lut, tone_curve, grade_sampler);
    }
    color.rgb = clamp(color.rgb, float3(0.0), float3(1.0));
    if (constants.config.x != 0)
    {
        color.rgb *= coverage;
    }
    if (!hdr)
    {
        color.rgb = almetal_world_display_dither(
            color.rgb, input.position.xy, constants.display.x,
            constants.config.w, constants.config.z != 0);
    }
    return float4(color.rgb, color.a);
}

fragment float4 almetal_world_display_post_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    texture2d_array<float> color_lut [[texture(4)]],
    texture2d<float> tone_curve [[texture(5)]],
    texture2d<float> lens_flare_state [[texture(6)]],
    texture2d<float> lens_dirt [[texture(7)]],
    sampler grade_sampler [[sampler(3)]],
    constant ALMetalWorldDisplayPostConstants& constants [[buffer(0)]])
{
    return almetal_world_display_post(
        input, source, source_sampler, float4(0.0), float3(0.0),
        color_lut, tone_curve, lens_flare_state, lens_dirt, grade_sampler,
        constants, constants.post.y);
}

fragment float4 almetal_world_display_post_exposure_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> exposure [[texture(1)]],
    sampler source_sampler [[sampler(0)]],
    texture2d_array<float> color_lut [[texture(4)]],
    texture2d<float> tone_curve [[texture(5)]],
    texture2d<float> lens_flare_state [[texture(6)]],
    texture2d<float> lens_dirt [[texture(7)]],
    sampler grade_sampler [[sampler(3)]],
    constant ALMetalWorldDisplayPostConstants& constants [[buffer(0)]])
{
    return almetal_world_display_post(
        input, source, source_sampler, float4(0.0), float3(0.0),
        color_lut, tone_curve, lens_flare_state, lens_dirt, grade_sampler,
        constants,
        exposure.read(uint2(0)).r);
}

fragment float4 almetal_world_display_post_bloom_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> bloom [[texture(2)]],
    texture2d<float> cross_filter [[texture(3)]],
    sampler source_sampler [[sampler(0)]],
    sampler bloom_sampler [[sampler(1)]],
    sampler cross_filter_sampler [[sampler(2)]],
    texture2d_array<float> color_lut [[texture(4)]],
    texture2d<float> tone_curve [[texture(5)]],
    texture2d<float> lens_flare_state [[texture(6)]],
    texture2d<float> lens_dirt [[texture(7)]],
    sampler grade_sampler [[sampler(3)]],
    constant ALMetalWorldDisplayPostConstants& constants [[buffer(0)]])
{
    return almetal_world_display_post(
        input, source, source_sampler,
        bloom.sample(bloom_sampler, input.texcoord),
        cross_filter.sample(cross_filter_sampler, input.texcoord).rgb,
        color_lut, tone_curve, lens_flare_state, lens_dirt, grade_sampler,
        constants, constants.post.y);
}

fragment float4 almetal_world_display_post_bloom_exposure_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> exposure [[texture(1)]],
    texture2d<float> bloom [[texture(2)]],
    texture2d<float> cross_filter [[texture(3)]],
    sampler source_sampler [[sampler(0)]],
    sampler bloom_sampler [[sampler(1)]],
    sampler cross_filter_sampler [[sampler(2)]],
    texture2d_array<float> color_lut [[texture(4)]],
    texture2d<float> tone_curve [[texture(5)]],
    texture2d<float> lens_flare_state [[texture(6)]],
    texture2d<float> lens_dirt [[texture(7)]],
    sampler grade_sampler [[sampler(3)]],
    constant ALMetalWorldDisplayPostConstants& constants [[buffer(0)]])
{
    return almetal_world_display_post(
        input, source, source_sampler,
        bloom.sample(bloom_sampler, input.texcoord),
        cross_filter.sample(cross_filter_sampler, input.texcoord).rgb,
        color_lut, tone_curve, lens_flare_state, lens_dirt, grade_sampler,
        constants, exposure.read(uint2(0)).r);
}

struct ALMetalWorldRlvSphereConstants
{
    float4x4 inverse_projection;
    float4 origin;
    float4 distance_value;
    float4 effect;
    float4 blur_direction;
    uint4 config;
};

static float3 almetal_world_rlv_blur_fixed(
    texture2d<float> source, sampler source_sampler, float2 texcoord,
    float2 direction, float2 dimensions)
{
    const float2 off1 = float2(1.411764705882353) * direction;
    const float2 off2 = float2(3.2941176470588234) * direction;
    const float2 off3 = float2(5.176470588235294) * direction;
    float3 color = source.sample(source_sampler, texcoord).rgb *
        0.1964825501511404;
    color += source.sample(
        source_sampler, texcoord + off1 / dimensions).rgb *
        0.2969069646728344;
    color += source.sample(
        source_sampler, texcoord - off1 / dimensions).rgb *
        0.2969069646728344;
    color += source.sample(
        source_sampler, texcoord + off2 / dimensions).rgb *
        0.09447039785044732;
    color += source.sample(
        source_sampler, texcoord - off2 / dimensions).rgb *
        0.09447039785044732;
    color += source.sample(
        source_sampler, texcoord + off3 / dimensions).rgb *
        0.010381362401148057;
    color += source.sample(
        source_sampler, texcoord - off3 / dimensions).rgb *
        0.010381362401148057;
    return color;
}

static float3 almetal_world_rlv_blur_variable(
    texture2d<float> source, sampler source_sampler, float2 texcoord,
    float kernel_size, float2 direction, float strength,
    float2 dimensions)
{
    // RLVa scripts control the kernel parameter. Keep the established GLSL
    // safety bound so malformed input cannot create an unbounded GPU loop.
    const uint taps = uint(clamp(
        floor(kernel_size * 0.5), 0.0, 32.0));
    constexpr float pi = 3.14159265;
    float3 gaussian;
    gaussian.x = 1.0 / (sqrt(2.0 * pi) * strength);
    gaussian.y = exp(-0.5 / (strength * strength));
    gaussian.z = gaussian.y * gaussian.y;
    float3 average = source.sample(source_sampler, texcoord).rgb *
        gaussian.x;
    float coefficient_sum = gaussian.x;
    gaussian.xy *= gaussian.yz;
    for (uint tap = 1u; tap <= taps; ++tap)
    {
        const float2 offset = float(tap) * direction / dimensions;
        average += source.sample(
            source_sampler, texcoord - offset).rgb * gaussian.x;
        average += source.sample(
            source_sampler, texcoord + offset).rgb * gaussian.x;
        coefficient_sum += 2.0 * gaussian.x;
        gaussian.xy *= gaussian.yz;
    }
    return average / coefficient_sum;
}

fragment float4 almetal_world_rlv_sphere_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> depth_texture [[texture(1)]],
    sampler source_sampler [[sampler(0)]],
    sampler depth_sampler [[sampler(1)]],
    constant ALMetalWorldRlvSphereConstants& constants [[buffer(0)]])
{
    const float depth = depth_texture.sample(
        depth_sampler, input.texcoord, level(0.0)).r;
    const float ndc_z = constants.config.w != 0u
        ? depth : depth * 2.0 - 1.0;
    float4 position = constants.inverse_projection *
        float4(input.texcoord * 2.0 - 1.0, ndc_z, 1.0);
    position.xyz /= max(position.w, 0.000001);
    const float distance_to_origin = length(
        position.xyz - constants.origin.xyz);

    const float value_min = constants.distance_value.x;
    const float distance_min = constants.distance_value.y;
    const float value_max = constants.distance_value.z;
    const float distance_max = constants.distance_value.w;
    float strength = value_min + (value_max - value_min) *
        ((distance_to_origin - distance_min) /
         (distance_max - distance_min));
    if (distance_to_origin < distance_min)
    {
        strength = constants.config.y != 0u ? value_min : 0.0;
    }
    if (distance_to_origin > distance_max)
    {
        strength = constants.config.z != 0u ? value_max : 0.0;
    }

    const float2 dimensions = float2(
        source.get_width(), source.get_height());
    float2 texcoord = input.texcoord;
    float3 color;
    switch (constants.config.x)
    {
        case 0u:
            color = mix(
                source.sample(source_sampler, texcoord).rgb,
                constants.effect.rgb, strength);
            break;
        case 1u:
            color = almetal_world_rlv_blur_fixed(
                source, source_sampler, texcoord,
                constants.blur_direction.xy * strength, dimensions);
            break;
        case 2u:
            color = strength > 0.0
                ? almetal_world_rlv_blur_variable(
                      source, source_sampler, texcoord,
                      constants.effect.x, constants.blur_direction.xy,
                      strength, dimensions)
                : source.sample(source_sampler, texcoord).rgb;
            break;
        case 3u:
        {
            const float3 original =
                source.sample(source_sampler, texcoord).rgb;
            const float red = source.sample(
                source_sampler,
                texcoord + constants.effect.xy / dimensions).r;
            const float blue = source.sample(
                source_sampler,
                texcoord + constants.effect.zw / dimensions).b;
            color = mix(original, float3(red, original.g, blue), strength);
            break;
        }
        case 4u:
        {
            const float signed_strength = sign(strength);
            const float2 pixel_size = max(
                float2(1.0),
                round(constants.effect.xy * signed_strength)) /
                dimensions;
            texcoord = pixel_size * floor(texcoord / pixel_size);
            color = source.sample(source_sampler, texcoord).rgb;
            break;
        }
        default:
            color = source.sample(source_sampler, texcoord).rgb;
            break;
    }
    return float4(color, 0.0);
}

struct ALMetalWorldPresentationConstants
{
    float4 lens_primary;
    float4 lens_squeeze_center;
    float4 lens_tangent_reference;
    float4 vignette_primary;
    float4 vignette_outer_middle;
    float4 vignette_middle_feather;
    float4 vignette_center_aspect;
    float4 effects;
    float4 grain_tint_dither;
    uint4 config;
    uint4 noise;
};

static float2 almetal_world_presentation_lens_uv(
    float2 uv, float2 dimensions,
    constant ALMetalWorldPresentationConstants& constants)
{
    if (constants.lens_primary.x <= 0.0)
    {
        return uv;
    }
    const float2 center = constants.lens_squeeze_center.zw;
    const float2 offset = uv - 0.5 - center;
    const float aspect = dimensions.x / max(dimensions.y, 1.0);
    const float2 axis = max(
        float2(aspect, 1.0 / max(aspect, 1.0e-4)), 1.0);
    const float2 q = offset * axis;
    const float radius_squared = dot(q, q);
    const float radial = 1.0 + radius_squared *
        (constants.lens_primary.y +
         radius_squared * constants.lens_primary.z);
    const float p1 = constants.lens_tangent_reference.x;
    const float p2 = constants.lens_tangent_reference.y;
    const float2 tangential = float2(
        2.0 * p1 * q.x * q.y + p2 *
            (radius_squared + 2.0 * q.x * q.x),
        p1 * (radius_squared + 2.0 * q.y * q.y) +
            2.0 * p2 * q.x * q.y);
    float2 warped = (q * radial + tangential) *
        constants.lens_primary.w;
    warped /= axis;
    warped *= constants.lens_squeeze_center.xy;
    return warped + 0.5 + center;
}

static float almetal_world_presentation_lens_mask(
    float2 uv, float amount)
{
    if (amount <= 0.0)
    {
        return 1.0;
    }
    const float2 inside = step(float2(0.0), uv) *
        step(uv, float2(1.0));
    return inside.x * inside.y;
}

static float3 almetal_world_presentation_vignette(
    float3 color, float2 uv,
    constant ALMetalWorldPresentationConstants& constants)
{
    const float amount = constants.vignette_primary.x;
    if (amount <= 0.0)
    {
        return color;
    }
    float2 delta = uv - 0.5 - constants.vignette_center_aspect.xy;
    const float aspect = max(constants.vignette_center_aspect.z, 1.0e-4) /
        max(constants.vignette_center_aspect.w, 1.0e-4);
    delta *= max(float2(aspect, 1.0 / aspect), 1.0);
    const float circular = length(delta) * 2.0;
    const float square_base = max(abs(delta.x), abs(delta.y)) * 2.0;
    const float square = square_base * sqrt(square_base);
    const float distance = mix(
        circular, square, constants.vignette_primary.w);
    const float start = constants.vignette_primary.y -
        constants.vignette_primary.z;
    const float end = constants.vignette_primary.y;
    const float midpoint = constants.vignette_outer_middle.w;
    const float feather = constants.vignette_middle_feather.w;
    if (midpoint > 1.0e-4 && midpoint < 0.9999)
    {
        const float middle = mix(start, end, midpoint);
        const float middle_amount = pow(
            smoothstep(start, middle, distance), feather) * amount;
        const float edge_amount = pow(
            smoothstep(middle, end, distance), feather) * amount;
        color = mix(
            color, constants.vignette_middle_feather.rgb,
            middle_amount);
        color = mix(
            color, constants.vignette_outer_middle.rgb,
            edge_amount);
        return color;
    }
    const float edge_amount = pow(
        smoothstep(start, end, distance), feather) * amount;
    return mix(color, constants.vignette_outer_middle.rgb, edge_amount);
}

static float3 almetal_world_presentation_simulate_cvd(
    float3 color, uint mode)
{
    constexpr float3 luma = float3(0.2126, 0.7152, 0.0722);
    if (mode == 4u)
    {
        return float3(dot(color, luma));
    }
    if (mode == 5u)
    {
        const float luminance = dot(color, luma);
        const float blue_yellow = color.b - 0.5 * (color.r + color.g);
        return float3(
            luminance - blue_yellow * 0.15,
            luminance - blue_yellow * 0.05,
            luminance + blue_yellow * 0.25);
    }
    if (mode == 6u)
    {
        return mix(color, float3(dot(color, luma)), 0.8);
    }
    if (mode == 1u)
    {
        return float3(
            dot(color, float3(0.152286, 1.052583, -0.204868)),
            dot(color, float3(0.114503, 0.786281, 0.099216)),
            dot(color, float3(-0.003882, -0.048116, 1.051998)));
    }
    if (mode == 2u)
    {
        return float3(
            dot(color, float3(0.367322, 0.860646, -0.227968)),
            dot(color, float3(0.280085, 0.672501, 0.047413)),
            dot(color, float3(-0.011820, 0.042940, 0.968881)));
    }
    return float3(
        dot(color, float3(1.255528, -0.076749, -0.178779)),
        dot(color, float3(-0.078411, 0.930809, 0.147602)),
        dot(color, float3(0.004733, 0.691367, 0.303900)));
}

static float3 almetal_world_presentation_cvd(
    float3 color, uint mode, float amount)
{
    if (mode == 0u || amount <= 0.0)
    {
        return color;
    }
    const float3 linear = almetal_srgb_to_linear(
        clamp(color, float3(0.0), float3(1.0)));
    const float3 simulated =
        almetal_world_presentation_simulate_cvd(linear, mode);
    const float3 error = linear - simulated;
    float3 shift;
    if (mode == 3u)
    {
        const float blue_error = error.b;
        shift = float3(
            blue_error * 0.7 + error.g * 0.3,
            -blue_error * 0.7 - error.g * 0.3, 0.0);
    }
    else
    {
        shift = float3(0.0, error.r * 0.7 + error.g, error.r * 0.7);
    }
    return almetal_linear_to_srgb(clamp(
        linear + shift * amount, float3(0.0), float3(1.0)));
}

static float3 almetal_world_presentation_grain(
    float3 color, float2 position,
    constant ALMetalWorldPresentationConstants& constants,
    float height)
{
    const float amount = constants.effects.y;
    if (amount <= 0.0)
    {
        return color;
    }
    const uint style = constants.config.z;
    float cell = constants.effects.z * height / 1080.0;
    if (style == 2u)
    {
        cell *= 2.5;
    }
    cell = max(floor(cell + 0.5), 1.0);
    const float2 grain_position = floor(
        (position + almetal_world_display_noise_offset(
            constants.noise.x, constants.noise.y != 0u)) / cell) * cell;
    float3 sample;
    if (style == 1u)
    {
        sample = float3(
            almetal_world_display_hash12(grain_position),
            almetal_world_display_hash12(grain_position + 17.0),
            almetal_world_display_hash12(grain_position + 31.0)) - 0.5;
    }
    else
    {
        sample = float3(
            almetal_world_display_hash12(grain_position) - 0.5);
    }
    constexpr float3 luma = float3(0.2126, 0.7152, 0.0722);
    const float luminance = dot(color, luma);
    const float amplitude = amount * amount * 0.1;
    if (style == 3u)
    {
        return color + sample * constants.grain_tint_dither.rgb *
            sqrt(max(luminance, 0.0)) * amplitude;
    }
    const float delta = luminance - constants.effects.w;
    const float weight = max(1.0 - delta * delta * 4.0, 0.0);
    return color + sample * constants.grain_tint_dither.rgb *
        weight * amplitude;
}

static float3 almetal_world_presentation_dither(
    float3 color, float2 position,
    constant ALMetalWorldPresentationConstants& constants)
{
    const float amount = constants.grain_tint_dither.w;
    if (amount <= 0.0)
    {
        return color;
    }
    const float2 p = position + almetal_world_display_noise_offset(
        constants.noise.x, constants.noise.w != 0u);
    const float noise = fract(
        52.9829189 * fract(dot(p, float2(0.06711056, 0.00583715)))) *
        2.0 - 1.0;
    const float triangular = sign(noise) *
        (1.0 - sqrt(1.0 - abs(noise)));
    const float levels = constants.noise.z >= 10u ? 1023.0 : 255.0;
    const float3 room = clamp(
        min(color, 1.0 - color) * levels,
        float3(0.0), float3(1.0));
    return color + triangular * (amount / levels) * room;
}

static float3 almetal_world_presentation_preview(float3 color, uint mode)
{
    if (mode == 0u)
    {
        return color;
    }
    if (mode == 7u)
    {
        const float luminance = dot(
            color, float3(0.2126, 0.7152, 0.0722));
        if (luminance < 0.02) return float3(0.0, 0.0, 0.4);
        if (luminance < 0.10) return float3(0.0, 0.3, 0.8);
        if (luminance < 0.30) return float3(0.0, 0.7, 0.5);
        if (luminance < 0.50) return float3(0.3, 0.7, 0.0);
        if (luminance < 0.70) return float3(0.8, 0.8, 0.0);
        if (luminance < 0.90) return float3(1.0, 0.5, 0.0);
        if (luminance < 0.98) return float3(1.0, 0.1, 0.0);
        return float3(1.0);
    }
    const float3 linear = almetal_world_presentation_simulate_cvd(
        almetal_srgb_to_linear(clamp(
            color, float3(0.0), float3(1.0))), mode);
    return almetal_linear_to_srgb(clamp(
        linear, float3(0.0), float3(1.0)));
}

fragment float4 almetal_world_presentation_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> reference [[texture(1)]],
    sampler mirror_sampler [[sampler(0)]],
    constant ALMetalWorldPresentationConstants& constants [[buffer(0)]])
{
    const float2 dimensions = float2(
        source.get_width(), source.get_height());
    float2 pane_uv = input.texcoord;
    bool from_reference = false;
    if (constants.config.x == 1u)
    {
        from_reference = input.texcoord.x <
            constants.lens_tangent_reference.z;
    }
    else if (constants.config.x == 2u)
    {
        from_reference = input.texcoord.x < 0.5;
        pane_uv.x = from_reference
            ? input.texcoord.x * 2.0
            : (input.texcoord.x - 0.5) * 2.0;
    }
    const float2 distorted = almetal_world_presentation_lens_uv(
        pane_uv, dimensions, constants);
    float4 color = from_reference
        ? reference.sample(mirror_sampler, distorted, level(0.0))
        : source.sample(mirror_sampler, distorted, level(0.0));
    color.rgb *= almetal_world_presentation_lens_mask(
        distorted, constants.lens_primary.x);
    color.rgb = almetal_world_presentation_vignette(
        color.rgb, input.texcoord, constants);
    color.rgb = almetal_world_presentation_cvd(
        color.rgb, constants.config.y, constants.effects.x);

    // GL's gl_FragCoord counts from the lower-left; Metal positions count
    // from the top. Preserve the authored screen-space noise location.
    const float2 gl_position = float2(
        input.position.x, dimensions.y - input.position.y);
    color.rgb = almetal_world_presentation_grain(
        color.rgb, gl_position, constants, dimensions.y);
    color.rgb = almetal_world_presentation_dither(
        color.rgb, gl_position, constants);
    color.rgb = almetal_world_presentation_preview(
        color.rgb, constants.config.w);

    if (constants.config.x != 0u)
    {
        const float seam = constants.config.x == 1u
            ? constants.lens_tangent_reference.z : 0.5;
        const float width = fwidth(input.texcoord.x);
        const float line = 1.0 - smoothstep(
            0.0, width, abs(input.texcoord.x - seam));
        color.rgb = mix(color.rgb, float3(1.0), line * 0.75);
    }
    return float4(clamp(color.rgb, float3(0.0), float3(1.0)), color.a);
}

static float4 almetal_world_fxaa_sample(
    texture2d<float> source, sampler source_sampler, float2 position)
{
    return source.sample(source_sampler, position, level(0.0));
}

static float almetal_world_fxaa_luma(float4 color)
{
    return dot(color.rgb, float3(0.299, 0.587, 0.114));
}

fragment float4 almetal_world_fxaa_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalWorldFXAAConstants& constants [[buffer(0)]])
{
    const uint quality = min(constants.config.x, 3u);
    const float2 reciprocal_frame = 1.0 / float2(
        source.get_width(), source.get_height());
    float2 position = input.texcoord;
    const float4 center = almetal_world_fxaa_sample(
        source, source_sampler, position);
    const float luma_center = almetal_world_fxaa_luma(center);
    float luma_south = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler,
            position + float2(0.0, reciprocal_frame.y)));
    float luma_east = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler,
            position + float2(reciprocal_frame.x, 0.0)));
    float luma_north = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler,
            position - float2(0.0, reciprocal_frame.y)));
    float luma_west = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler,
            position - float2(reciprocal_frame.x, 0.0)));

    const float max_sm = max(luma_south, luma_center);
    const float min_sm = min(luma_south, luma_center);
    const float max_esm = max(luma_east, max_sm);
    const float min_esm = min(luma_east, min_sm);
    const float max_wn = max(luma_north, luma_west);
    const float min_wn = min(luma_north, luma_west);
    const float range_max = max(max_wn, max_esm);
    const float range_min = min(min_wn, min_esm);
    const float range = range_max - range_min;
    if (range < max(0.03, range_max * 0.07))
    {
        return float4(center.rgb, luma_center);
    }

    const float luma_north_west = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler,
            position + reciprocal_frame * float2(-1.0, -1.0)));
    const float luma_south_east = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler,
            position + reciprocal_frame * float2(1.0, 1.0)));
    const float luma_north_east = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler,
            position + reciprocal_frame * float2(1.0, -1.0)));
    const float luma_south_west = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler,
            position + reciprocal_frame * float2(-1.0, 1.0)));

    const float luma_ns = luma_north + luma_south;
    const float luma_we = luma_west + luma_east;
    const float subpixel_reciprocal_range = 1.0 / range;
    const float subpixel_nswe = luma_ns + luma_we;
    const float edge_horizontal_1 = -2.0 * luma_center + luma_ns;
    const float edge_vertical_1 = -2.0 * luma_center + luma_we;
    const float luma_nese = luma_north_east + luma_south_east;
    const float luma_nwne = luma_north_west + luma_north_east;
    const float edge_horizontal_2 = -2.0 * luma_east + luma_nese;
    const float edge_vertical_2 = -2.0 * luma_north + luma_nwne;
    const float luma_nwsw = luma_north_west + luma_south_west;
    const float luma_swse = luma_south_west + luma_south_east;
    const float edge_horizontal_4 =
        abs(edge_horizontal_1) * 2.0 + abs(edge_horizontal_2);
    const float edge_vertical_4 =
        abs(edge_vertical_1) * 2.0 + abs(edge_vertical_2);
    const float edge_horizontal_3 = -2.0 * luma_west + luma_nwsw;
    const float edge_vertical_3 = -2.0 * luma_south + luma_swse;
    const float edge_horizontal = abs(edge_horizontal_3) +
        edge_horizontal_4;
    const float edge_vertical = abs(edge_vertical_3) + edge_vertical_4;
    const float subpixel_nwswnese = luma_nwsw + luma_nese;

    float length_sign = reciprocal_frame.x;
    const bool horizontal_span = edge_horizontal >= edge_vertical;
    const float subpixel_a =
        subpixel_nswe * 2.0 + subpixel_nwswnese;
    if (!horizontal_span)
    {
        luma_north = luma_west;
        luma_south = luma_east;
    }
    if (horizontal_span)
    {
        length_sign = reciprocal_frame.y;
    }
    const float subpixel_b = subpixel_a * (1.0 / 12.0) - luma_center;
    const float gradient_north = luma_north - luma_center;
    const float gradient_south = luma_south - luma_center;
    float luma_nn = luma_north + luma_center;
    const float luma_ss = luma_south + luma_center;
    const bool pair_north =
        abs(gradient_north) >= abs(gradient_south);
    const float gradient =
        max(abs(gradient_north), abs(gradient_south));
    if (pair_north)
    {
        length_sign = -length_sign;
    }
    const float subpixel_c = saturate(
        abs(subpixel_b) * subpixel_reciprocal_range);

    float2 position_base = position;
    const float2 offset_np = float2(
        horizontal_span ? reciprocal_frame.x : 0.0,
        horizontal_span ? 0.0 : reciprocal_frame.y);
    if (!horizontal_span)
    {
        position_base.x += length_sign * 0.5;
    }
    if (horizontal_span)
    {
        position_base.y += length_sign * 0.5;
    }

    float2 position_north = position_base - offset_np *
        almetal_world_fxaa_steps[quality][0];
    float2 position_south = position_base + offset_np *
        almetal_world_fxaa_steps[quality][0];
    const float subpixel_d = -2.0 * subpixel_c + 3.0;
    float luma_end_north = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler, position_north));
    const float subpixel_e = subpixel_c * subpixel_c;
    float luma_end_south = almetal_world_fxaa_luma(
        almetal_world_fxaa_sample(
            source, source_sampler, position_south));
    if (!pair_north)
    {
        luma_nn = luma_ss;
    }
    const float gradient_scaled = gradient * 0.25;
    const float luma_mm = luma_center - luma_nn * 0.5;
    const float subpixel_f = subpixel_d * subpixel_e;
    const bool luma_m_less_than_zero = luma_mm < 0.0;
    luma_end_north -= luma_nn * 0.5;
    luma_end_south -= luma_nn * 0.5;
    bool done_north = abs(luma_end_north) >= gradient_scaled;
    bool done_south = abs(luma_end_south) >= gradient_scaled;

    const uint step_count = almetal_world_fxaa_step_counts[quality];
    for (uint step = 1u; step < step_count; ++step)
    {
        const float distance =
            almetal_world_fxaa_steps[quality][step];
        if (!done_north)
        {
            position_north -= offset_np * distance;
        }
        if (!done_south)
        {
            position_south += offset_np * distance;
        }
        // FXAA advances the endpoints by the final preset distance but does
        // not issue another texture tap there.
        if (step + 1u == step_count || (done_north && done_south))
        {
            break;
        }
        if (!done_north)
        {
            luma_end_north = almetal_world_fxaa_luma(
                almetal_world_fxaa_sample(
                    source, source_sampler, position_north)) -
                luma_nn * 0.5;
        }
        if (!done_south)
        {
            luma_end_south = almetal_world_fxaa_luma(
                almetal_world_fxaa_sample(
                    source, source_sampler, position_south)) -
                luma_nn * 0.5;
        }
        done_north = abs(luma_end_north) >= gradient_scaled;
        done_south = abs(luma_end_south) >= gradient_scaled;
    }

    float distance_north = position.x - position_north.x;
    float distance_south = position_south.x - position.x;
    if (!horizontal_span)
    {
        distance_north = position.y - position_north.y;
        distance_south = position_south.y - position.y;
    }
    const bool good_span_north =
        (luma_end_north < 0.0) != luma_m_less_than_zero;
    const bool good_span_south =
        (luma_end_south < 0.0) != luma_m_less_than_zero;
    const bool direction_north = distance_north < distance_south;
    const float distance = min(distance_north, distance_south);
    const bool good_span = direction_north
        ? good_span_north : good_span_south;
    const float subpixel_g = subpixel_f * subpixel_f;
    const float pixel_offset =
        distance * (-1.0 / (distance_south + distance_north)) + 0.5;
    const float subpixel_h = subpixel_g * 0.75;
    const float final_offset = max(
        good_span ? pixel_offset : 0.0, subpixel_h);
    if (!horizontal_span)
    {
        position.x += final_offset * length_sign;
    }
    if (horizontal_span)
    {
        position.y += final_offset * length_sign;
    }
    return float4(
        almetal_world_fxaa_sample(
            source, source_sampler, position).rgb,
        luma_center);
}

constant float almetal_world_smaa_thresholds[4] = {
    0.15, 0.10, 0.10, 0.05,
};

static float3 almetal_world_smaa_color(
    texture2d<float> source, sampler source_sampler, float2 position)
{
    return source.sample(source_sampler, position, level(0.0)).rgb;
}

static float4 almetal_world_smaa_edges(
    texture2d<float> source,
    sampler source_sampler,
    float2 position,
    float2 threshold)
{
    const float2 metric = 1.0 / float2(
        source.get_width(), source.get_height());
    const float3 center = almetal_world_smaa_color(
        source, source_sampler, position);
    const float3 left = almetal_world_smaa_color(
        source, source_sampler, position + float2(-metric.x, 0.0));
    const float3 top = almetal_world_smaa_color(
        source, source_sampler, position + float2(0.0, metric.y));

    float4 delta;
    const float3 left_delta = abs(center - left);
    const float3 top_delta = abs(center - top);
    delta.x = max(max(left_delta.r, left_delta.g), left_delta.b);
    delta.y = max(max(top_delta.r, top_delta.g), top_delta.b);
    float2 edges = step(threshold, delta.xy);
    if (dot(edges, float2(1.0)) == 0.0)
    {
        return float4(0.0);
    }

    const float3 right = almetal_world_smaa_color(
        source, source_sampler, position + float2(metric.x, 0.0));
    const float3 bottom = almetal_world_smaa_color(
        source, source_sampler, position - float2(0.0, metric.y));
    const float3 right_delta = abs(center - right);
    const float3 bottom_delta = abs(center - bottom);
    delta.z = max(max(right_delta.r, right_delta.g), right_delta.b);
    delta.w = max(max(bottom_delta.r, bottom_delta.g), bottom_delta.b);
    float2 maximum_delta = max(delta.xy, delta.zw);

    const float3 left_left = almetal_world_smaa_color(
        source, source_sampler, position + float2(-2.0 * metric.x, 0.0));
    const float3 top_top = almetal_world_smaa_color(
        source, source_sampler, position + float2(0.0, 2.0 * metric.y));
    const float3 left_left_delta = abs(center - left_left);
    const float3 top_top_delta = abs(center - top_top);
    delta.z = max(
        max(left_left_delta.r, left_left_delta.g), left_left_delta.b);
    delta.w = max(max(top_top_delta.r, top_top_delta.g), top_top_delta.b);
    maximum_delta = max(maximum_delta, delta.zw);
    const float final_delta = max(maximum_delta.x, maximum_delta.y);
    edges *= step(float2(final_delta), 2.0 * delta.xy);
    return float4(edges, 0.0, 0.0);
}

fragment float4 almetal_world_smaa_edge_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalWorldSMAAEdgeConstants& constants [[buffer(0)]])
{
    const uint quality = min(constants.config.x, 3u);
    return almetal_world_smaa_edges(
        source, source_sampler, input.texcoord,
        float2(almetal_world_smaa_thresholds[quality]));
}

fragment float4 almetal_world_smaa_edge_predicated_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    depth2d<float> predication [[texture(1)]],
    sampler source_sampler [[sampler(0)]],
    sampler predication_sampler [[sampler(1)]],
    constant ALMetalWorldSMAAEdgeConstants& constants [[buffer(0)]])
{
    const uint quality = min(constants.config.x, 3u);
    const float2 metric = 1.0 / float2(
        source.get_width(), source.get_height());
    const float center = predication.sample(
        predication_sampler, input.texcoord, level(0.0));
    const float left = predication.sample(
        predication_sampler,
        input.texcoord + float2(-metric.x, 0.0), level(0.0));
    const float top = predication.sample(
        predication_sampler,
        input.texcoord + float2(0.0, metric.y), level(0.0));
    const float2 predicated_edges = step(
        float2(constants.predication.x),
        abs(float2(center - left, center - top)));
    const float2 threshold = constants.predication.y *
        almetal_world_smaa_thresholds[quality] *
        (1.0 - constants.predication.z * predicated_edges);
    return almetal_world_smaa_edges(
        source, source_sampler, input.texcoord, threshold);
}

constant uint almetal_world_smaa_search_steps[4] = {4u, 8u, 16u, 32u};

static float almetal_world_smaa_v_dir(float value)
{
    return -value;
}

static float almetal_world_smaa_v_coord(float value)
{
    return 1.0 - value;
}

static float4 almetal_world_smaa_sample(
    texture2d<float> source, sampler source_sampler, float2 position)
{
    return source.sample(source_sampler, position, level(0.0));
}

static float4 almetal_world_smaa_sample_offset(
    texture2d<float> source,
    sampler source_sampler,
    float2 position,
    int2 offset,
    float2 metric)
{
    return almetal_world_smaa_sample(
        source, source_sampler, position + float2(offset) * metric);
}

static float2 almetal_world_smaa_decode_diag(float2 edges)
{
    edges.x *= abs(5.0 * edges.x - 5.0 * 0.75);
    return round(edges);
}

static float4 almetal_world_smaa_decode_diag(float4 edges)
{
    edges.x *= abs(5.0 * edges.x - 5.0 * 0.75);
    edges.z *= abs(5.0 * edges.z - 5.0 * 0.75);
    return round(edges);
}

static float2 almetal_world_smaa_search_diag_1(
    texture2d<float> edges,
    sampler source_sampler,
    float2 texcoord,
    float2 direction,
    uint max_steps,
    thread float2& end_edges)
{
    direction.y = almetal_world_smaa_v_dir(direction.y);
    float4 coordinate = float4(texcoord, -1.0, 1.0);
    const float3 stride = float3(
        1.0 / float(edges.get_width()),
        1.0 / float(edges.get_height()), 1.0);
    while (coordinate.z < float(max_steps - 1u) && coordinate.w > 0.9)
    {
        coordinate.xyz = fma(stride, float3(direction, 1.0),
                             coordinate.xyz);
        end_edges = almetal_world_smaa_sample(
            edges, source_sampler, coordinate.xy).rg;
        coordinate.w = dot(end_edges, float2(0.5));
    }
    return coordinate.zw;
}

static float2 almetal_world_smaa_search_diag_2(
    texture2d<float> edges,
    sampler source_sampler,
    float2 texcoord,
    float2 direction,
    uint max_steps,
    thread float2& end_edges)
{
    direction.y = almetal_world_smaa_v_dir(direction.y);
    float4 coordinate = float4(texcoord, -1.0, 1.0);
    const float2 metric = 1.0 / float2(
        edges.get_width(), edges.get_height());
    coordinate.x += 0.25 * metric.x;
    const float3 stride = float3(metric, 1.0);
    while (coordinate.z < float(max_steps - 1u) && coordinate.w > 0.9)
    {
        coordinate.xyz = fma(stride, float3(direction, 1.0),
                             coordinate.xyz);
        end_edges = almetal_world_smaa_sample(
            edges, source_sampler, coordinate.xy).rg;
        end_edges = almetal_world_smaa_decode_diag(end_edges);
        coordinate.w = dot(end_edges, float2(0.5));
    }
    return coordinate.zw;
}

static float2 almetal_world_smaa_area_diag(
    texture2d<float> area,
    sampler source_sampler,
    float2 distance,
    float2 edges,
    float offset)
{
    float2 texcoord = fma(float2(20.0), edges, distance);
    constexpr float2 pixel_size = 1.0 / float2(160.0, 560.0);
    texcoord = fma(pixel_size, texcoord, 0.5 * pixel_size);
    texcoord.x += 0.5;
    texcoord.y += (1.0 / 7.0) * offset;
    texcoord.y = almetal_world_smaa_v_coord(texcoord.y);
    return almetal_world_smaa_sample(
        area, source_sampler, texcoord).rg;
}

static float2 almetal_world_smaa_diag_weights(
    texture2d<float> edges,
    texture2d<float> area,
    sampler source_sampler,
    float2 texcoord,
    float2 edge,
    uint max_steps)
{
    float2 weights = float2(0.0);
    const float2 metric = 1.0 / float2(
        edges.get_width(), edges.get_height());
    float4 distance;
    float2 end_edges;
    if (edge.x > 0.0)
    {
        distance.xz = almetal_world_smaa_search_diag_1(
            edges, source_sampler, texcoord, float2(-1.0, 1.0),
            max_steps, end_edges);
        distance.x += end_edges.y > 0.9 ? 1.0 : 0.0;
    }
    else
    {
        distance.xz = float2(0.0);
    }
    distance.yw = almetal_world_smaa_search_diag_1(
        edges, source_sampler, texcoord, float2(1.0, -1.0),
        max_steps, end_edges);

    if (distance.x + distance.y > 2.0)
    {
        const float4 coordinates = fma(
            float4(-distance.x + 0.25,
                   almetal_world_smaa_v_dir(distance.x),
                   distance.y,
                   almetal_world_smaa_v_dir(-distance.y - 0.25)),
            float4(metric, metric), float4(texcoord, texcoord));
        float4 crossing;
        crossing.xy = almetal_world_smaa_sample_offset(
            edges, source_sampler, coordinates.xy,
            int2(-1, 0), metric).rg;
        crossing.zw = almetal_world_smaa_sample_offset(
            edges, source_sampler, coordinates.zw,
            int2(1, 0), metric).rg;
        const float4 decoded =
            almetal_world_smaa_decode_diag(crossing);
        crossing = float4(decoded.y, decoded.x, decoded.w, decoded.z);
        float2 merged = fma(float2(2.0), crossing.xz, crossing.yw);
        if (distance.z >= 0.9)
        {
            merged.x = 0.0;
        }
        if (distance.w >= 0.9)
        {
            merged.y = 0.0;
        }
        weights += almetal_world_smaa_area_diag(
            area, source_sampler, distance.xy, merged, 0.0);
    }

    distance.xz = almetal_world_smaa_search_diag_2(
        edges, source_sampler, texcoord, float2(-1.0, -1.0),
        max_steps, end_edges);
    if (almetal_world_smaa_sample_offset(
            edges, source_sampler, texcoord, int2(1, 0), metric).r > 0.0)
    {
        distance.yw = almetal_world_smaa_search_diag_2(
            edges, source_sampler, texcoord, float2(1.0, 1.0),
            max_steps, end_edges);
        distance.y += end_edges.y > 0.9 ? 1.0 : 0.0;
    }
    else
    {
        distance.yw = float2(0.0);
    }

    if (distance.x + distance.y > 2.0)
    {
        const float4 coordinates = fma(
            float4(-distance.x,
                   almetal_world_smaa_v_dir(-distance.x),
                   distance.y,
                   almetal_world_smaa_v_dir(distance.y)),
            float4(metric, metric), float4(texcoord, texcoord));
        float4 crossing;
        crossing.x = almetal_world_smaa_sample_offset(
            edges, source_sampler, coordinates.xy,
            int2(-1, 0), metric).g;
        crossing.y = almetal_world_smaa_sample_offset(
            edges, source_sampler, coordinates.xy,
            int2(0, 1), metric).r;
        const float2 right = almetal_world_smaa_sample_offset(
            edges, source_sampler, coordinates.zw,
            int2(1, 0), metric).gr;
        crossing.zw = right;
        float2 merged = fma(float2(2.0), crossing.xz, crossing.yw);
        if (distance.z >= 0.9)
        {
            merged.x = 0.0;
        }
        if (distance.w >= 0.9)
        {
            merged.y = 0.0;
        }
        weights += almetal_world_smaa_area_diag(
            area, source_sampler, distance.xy, merged, 0.0).gr;
    }
    return weights;
}

static float almetal_world_smaa_search_length(
    texture2d<float> search,
    sampler source_sampler,
    float2 edge,
    float offset)
{
    float2 scale = float2(66.0, 33.0) * float2(0.5, -1.0);
    float2 bias = float2(66.0, 33.0) * float2(offset, 1.0);
    scale += float2(-1.0, 1.0);
    bias += float2(0.5, -0.5);
    scale /= float2(64.0, 16.0);
    bias /= float2(64.0, 16.0);
    float2 coordinate = fma(scale, edge, bias);
    coordinate.y = almetal_world_smaa_v_coord(coordinate.y);
    return almetal_world_smaa_sample(
        search, source_sampler, coordinate).r;
}

static float almetal_world_smaa_search_x_left(
    texture2d<float> edges,
    texture2d<float> search,
    sampler source_sampler,
    float2 texcoord,
    float end)
{
    const float2 metric = 1.0 / float2(
        edges.get_width(), edges.get_height());
    float2 edge = float2(0.0, 1.0);
    while (texcoord.x > end && edge.y > 0.8281 && edge.x == 0.0)
    {
        edge = almetal_world_smaa_sample(
            edges, source_sampler, texcoord).rg;
        texcoord = fma(-float2(2.0, 0.0), metric, texcoord);
    }
    const float offset = fma(
        -(255.0 / 127.0),
        almetal_world_smaa_search_length(
            search, source_sampler, edge, 0.0), 3.25);
    return fma(metric.x, offset, texcoord.x);
}

static float almetal_world_smaa_search_x_right(
    texture2d<float> edges,
    texture2d<float> search,
    sampler source_sampler,
    float2 texcoord,
    float end)
{
    const float2 metric = 1.0 / float2(
        edges.get_width(), edges.get_height());
    float2 edge = float2(0.0, 1.0);
    while (texcoord.x < end && edge.y > 0.8281 && edge.x == 0.0)
    {
        edge = almetal_world_smaa_sample(
            edges, source_sampler, texcoord).rg;
        texcoord = fma(float2(2.0, 0.0), metric, texcoord);
    }
    const float offset = fma(
        -(255.0 / 127.0),
        almetal_world_smaa_search_length(
            search, source_sampler, edge, 0.5), 3.25);
    return fma(-metric.x, offset, texcoord.x);
}

static float almetal_world_smaa_search_y_up(
    texture2d<float> edges,
    texture2d<float> search,
    sampler source_sampler,
    float2 texcoord,
    float end)
{
    const float2 metric = 1.0 / float2(
        edges.get_width(), edges.get_height());
    float2 edge = float2(1.0, 0.0);
    while (texcoord.y < end && edge.x > 0.8281 && edge.y == 0.0)
    {
        edge = almetal_world_smaa_sample(
            edges, source_sampler, texcoord).rg;
        texcoord = fma(float2(0.0, 2.0), metric, texcoord);
    }
    const float offset = fma(
        -(255.0 / 127.0),
        almetal_world_smaa_search_length(
            search, source_sampler, edge.gr, 0.0), 3.25);
    return fma(metric.y, almetal_world_smaa_v_dir(offset), texcoord.y);
}

static float almetal_world_smaa_search_y_down(
    texture2d<float> edges,
    texture2d<float> search,
    sampler source_sampler,
    float2 texcoord,
    float end)
{
    const float2 metric = 1.0 / float2(
        edges.get_width(), edges.get_height());
    float2 edge = float2(1.0, 0.0);
    while (texcoord.y > end && edge.x > 0.8281 && edge.y == 0.0)
    {
        edge = almetal_world_smaa_sample(
            edges, source_sampler, texcoord).rg;
        texcoord = fma(float2(0.0, -2.0), metric, texcoord);
    }
    const float offset = fma(
        -(255.0 / 127.0),
        almetal_world_smaa_search_length(
            search, source_sampler, edge.gr, 0.5), 3.25);
    return fma(-metric.y, almetal_world_smaa_v_dir(offset), texcoord.y);
}

static float2 almetal_world_smaa_area(
    texture2d<float> area,
    sampler source_sampler,
    float2 distance,
    float edge1,
    float edge2)
{
    float2 texcoord = fma(
        float2(16.0), round(4.0 * float2(edge1, edge2)), distance);
    constexpr float2 pixel_size = 1.0 / float2(160.0, 560.0);
    texcoord = fma(pixel_size, texcoord, 0.5 * pixel_size);
    texcoord.y = almetal_world_smaa_v_coord(texcoord.y);
    return almetal_world_smaa_sample(
        area, source_sampler, texcoord).rg;
}

static float2 almetal_world_smaa_horizontal_corners(
    texture2d<float> edges,
    sampler source_sampler,
    float2 weights,
    float4 texcoord,
    float2 distance)
{
    const float2 metric = 1.0 / float2(
        edges.get_width(), edges.get_height());
    const float2 left_right = step(distance.xy, distance.yx);
    float2 rounding = 0.75 * left_right;
    rounding /= left_right.x + left_right.y;
    float2 factor = float2(1.0);
    factor.x -= rounding.x * almetal_world_smaa_sample_offset(
        edges, source_sampler, texcoord.xy, int2(0, -1), metric).r;
    factor.x -= rounding.y * almetal_world_smaa_sample_offset(
        edges, source_sampler, texcoord.zw, int2(1, -1), metric).r;
    factor.y -= rounding.x * almetal_world_smaa_sample_offset(
        edges, source_sampler, texcoord.xy, int2(0, 2), metric).r;
    factor.y -= rounding.y * almetal_world_smaa_sample_offset(
        edges, source_sampler, texcoord.zw, int2(1, 2), metric).r;
    return weights * saturate(factor);
}

static float2 almetal_world_smaa_vertical_corners(
    texture2d<float> edges,
    sampler source_sampler,
    float2 weights,
    float4 texcoord,
    float2 distance)
{
    const float2 metric = 1.0 / float2(
        edges.get_width(), edges.get_height());
    const float2 left_right = step(distance.xy, distance.yx);
    float2 rounding = 0.75 * left_right;
    rounding /= left_right.x + left_right.y;
    float2 factor = float2(1.0);
    factor.x -= rounding.x * almetal_world_smaa_sample_offset(
        edges, source_sampler, texcoord.xy, int2(1, 0), metric).g;
    factor.x -= rounding.y * almetal_world_smaa_sample_offset(
        edges, source_sampler, texcoord.zw, int2(1, -1), metric).g;
    factor.y -= rounding.x * almetal_world_smaa_sample_offset(
        edges, source_sampler, texcoord.xy, int2(-2, 0), metric).g;
    factor.y -= rounding.y * almetal_world_smaa_sample_offset(
        edges, source_sampler, texcoord.zw, int2(-2, -1), metric).g;
    return weights * saturate(factor);
}

fragment float4 almetal_world_smaa_weight_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> edges [[texture(0)]],
    texture2d<float> area [[texture(1)]],
    texture2d<float> search [[texture(2)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalWorldSMAAWeightConstants& constants [[buffer(0)]])
{
    const uint quality = min(constants.config.x, 3u);
    const uint search_steps = almetal_world_smaa_search_steps[quality];
    const float2 metric = 1.0 / float2(
        edges.get_width(), edges.get_height());
    const float4 metrics = float4(
        metric, edges.get_width(), edges.get_height());
    const float2 pixcoord = input.texcoord * metrics.zw;
    float4 offset[3];
    offset[0] = fma(
        float4(metric, metric),
        float4(-0.25, almetal_world_smaa_v_dir(-0.125),
               1.25, almetal_world_smaa_v_dir(-0.125)),
        float4(input.texcoord, input.texcoord));
    offset[1] = fma(
        float4(metric, metric),
        float4(-0.125, almetal_world_smaa_v_dir(-0.25),
               -0.125, almetal_world_smaa_v_dir(1.25)),
        float4(input.texcoord, input.texcoord));
    offset[2] = fma(
        float4(metric.x, metric.x, metric.y, metric.y),
        float4(-2.0, 2.0,
               almetal_world_smaa_v_dir(-2.0),
               almetal_world_smaa_v_dir(2.0)) * float(search_steps),
        float4(offset[0].x, offset[0].z,
               offset[1].y, offset[1].w));

    float4 weights = float4(0.0);
    float2 edge = almetal_world_smaa_sample(
        edges, source_sampler, input.texcoord).rg;
    if (edge.y > 0.0)
    {
        bool horizontal_vertical = true;
        if (quality >= 2u)
        {
            const uint diagonal_steps = quality == 2u ? 8u : 16u;
            weights.rg = almetal_world_smaa_diag_weights(
                edges, area, source_sampler, input.texcoord,
                edge, diagonal_steps);
            horizontal_vertical = weights.r == -weights.g;
            if (!horizontal_vertical)
            {
                edge.r = 0.0;
            }
        }
        if (horizontal_vertical)
        {
            float3 coordinates;
            coordinates.x = almetal_world_smaa_search_x_left(
                edges, search, source_sampler,
                offset[0].xy, offset[2].x);
            coordinates.y = offset[1].y;
            float2 distance;
            distance.x = coordinates.x;
            const float edge1 = almetal_world_smaa_sample(
                edges, source_sampler, coordinates.xy).r;
            coordinates.z = almetal_world_smaa_search_x_right(
                edges, search, source_sampler,
                offset[0].zw, offset[2].y);
            distance.y = coordinates.z;
            distance = abs(round(
                metrics.zz * distance - pixcoord.xx));
            const float edge2 = almetal_world_smaa_sample_offset(
                edges, source_sampler, coordinates.zy,
                int2(1, 0), metric).r;
            weights.rg = almetal_world_smaa_area(
                area, source_sampler, sqrt(distance), edge1, edge2);
            if (quality >= 2u)
            {
                coordinates.y = input.texcoord.y;
                weights.rg = almetal_world_smaa_horizontal_corners(
                    edges, source_sampler, weights.rg,
                    float4(coordinates.x, coordinates.y,
                           coordinates.z, coordinates.y),
                    distance);
            }
        }
    }

    if (edge.x > 0.0)
    {
        float3 coordinates;
        coordinates.y = almetal_world_smaa_search_y_up(
            edges, search, source_sampler,
            offset[1].xy, offset[2].z);
        coordinates.x = offset[0].x;
        float2 distance;
        distance.x = coordinates.y;
        const float edge1 = almetal_world_smaa_sample(
            edges, source_sampler, coordinates.xy).g;
        coordinates.z = almetal_world_smaa_search_y_down(
            edges, search, source_sampler,
            offset[1].zw, offset[2].w);
        distance.y = coordinates.z;
        distance = abs(round(
            metrics.ww * distance - pixcoord.yy));
        const float edge2 = almetal_world_smaa_sample_offset(
            edges, source_sampler, coordinates.xz,
            int2(0, -1), metric).g;
        weights.ba = almetal_world_smaa_area(
            area, source_sampler, sqrt(distance), edge1, edge2);
        if (quality >= 2u)
        {
            coordinates.x = input.texcoord.x;
            weights.ba = almetal_world_smaa_vertical_corners(
                edges, source_sampler, weights.ba,
                float4(coordinates.x, coordinates.y,
                       coordinates.x, coordinates.z),
                distance);
        }
    }
    return weights;
}

static float3 almetal_world_smaa_srgb_to_linear(float3 encoded)
{
    const float3 low = encoded / 12.92;
    const float3 high = pow(
        (encoded + 0.055) / 1.055, float3(2.4));
    return select(high, low, encoded <= 0.04045);
}

static float3 almetal_world_smaa_linear_to_srgb(float3 linear)
{
    const float3 low = linear * 12.92;
    const float3 high =
        1.055 * pow(linear, float3(0.41666)) - 0.055;
    return select(high, low, linear < 0.0031308);
}

fragment float4 almetal_world_smaa_neighborhood_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    texture2d<float> weights [[texture(1)]],
    sampler source_sampler [[sampler(0)]],
    sampler weight_sampler [[sampler(1)]])
{
    const float2 metric = 1.0 / float2(
        source.get_width(), source.get_height());
    const float4 offset = fma(
        float4(metric, metric),
        float4(1.0, 0.0, 0.0, almetal_world_smaa_v_dir(1.0)),
        float4(input.texcoord, input.texcoord));
    float4 blend;
    blend.x = almetal_world_smaa_sample(
        weights, weight_sampler, offset.xy).a;
    blend.y = almetal_world_smaa_sample(
        weights, weight_sampler, offset.zw).g;
    const float4 center = almetal_world_smaa_sample(
        weights, weight_sampler, input.texcoord);
    blend.w = center.x;
    blend.z = center.z;

    if (dot(blend, float4(1.0)) < 1e-5)
    {
        float4 color = almetal_world_smaa_sample(
            source, source_sampler, input.texcoord);
        color.rgb = almetal_world_smaa_linear_to_srgb(
            almetal_world_smaa_srgb_to_linear(color.rgb));
        return color;
    }

    const bool horizontal =
        max(blend.x, blend.z) > max(blend.y, blend.w);
    float4 blending_offset = float4(
        0.0, almetal_world_smaa_v_dir(blend.y),
        0.0, almetal_world_smaa_v_dir(blend.w));
    float2 blending_weight = blend.yw;
    if (horizontal)
    {
        blending_offset = float4(blend.x, 0.0, blend.z, 0.0);
        blending_weight = blend.xz;
    }
    blending_weight /= dot(blending_weight, float2(1.0));
    const float4 blending_coordinate = fma(
        blending_offset, float4(metric, -metric),
        float4(input.texcoord, input.texcoord));

    float4 color = almetal_world_smaa_sample(
        source, source_sampler, blending_coordinate.xy);
    color.rgb = almetal_world_smaa_srgb_to_linear(color.rgb);
    color *= blending_weight.x;
    float4 color2 = almetal_world_smaa_sample(
        source, source_sampler, blending_coordinate.zw);
    color2.rgb = almetal_world_smaa_srgb_to_linear(color2.rgb);
    color += blending_weight.y * color2;
    color.rgb = almetal_world_smaa_linear_to_srgb(color.rgb);
    return color;
}

static float3 almetal_world_cas_load(
    texture2d<float> source, int2 point)
{
    const int2 last = int2(
        int(source.get_width()) - 1, int(source.get_height()) - 1);
    return source.read(uint2(clamp(point, int2(0), last))).rgb;
}

static float3 almetal_world_cas_low_reciprocal(float3 value)
{
    return as_type<float3>(
        uint3(0x7ef07ebbu) - as_type<uint3>(value));
}

static float3 almetal_world_cas_low_sqrt(float3 value)
{
    return as_type<float3>(
        (as_type<uint3>(value) >> uint3(1u)) + uint3(0x1fbc4639u));
}

static float3 almetal_world_cas_medium_reciprocal(float3 value)
{
    const float3 estimate = as_type<float3>(
        uint3(0x7ef19fffu) - as_type<uint3>(value));
    return estimate * (-estimate * value + 2.0);
}

fragment float4 almetal_world_cas_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    constant ALMetalWorldCASConstants& constants [[buffer(0)]])
{
    const int2 p = int2(input.position.xy);
    const float3 a = almetal_world_cas_load(source, p + int2(-1, -1));
    const float3 b = almetal_world_cas_load(source, p + int2( 0, -1));
    const float3 c = almetal_world_cas_load(source, p + int2( 1, -1));
    const float3 d = almetal_world_cas_load(source, p + int2(-1,  0));
    const float3 e = almetal_world_cas_load(source, p);
    const float3 f = almetal_world_cas_load(source, p + int2( 1,  0));
    const float3 g = almetal_world_cas_load(source, p + int2(-1,  1));
    const float3 h = almetal_world_cas_load(source, p + int2( 0,  1));
    const float3 i = almetal_world_cas_load(source, p + int2( 1,  1));

    // CAS_BETTER_DIAGONALS: preserve the reference's factored 2x soft
    // extrema rather than dividing both neighborhoods by two.
    float3 minimum = min(min(min(d, e), f), min(b, h));
    minimum += min(min(min(minimum, a), c), min(g, i));
    float3 maximum = max(max(max(d, e), f), max(b, h));
    maximum += max(max(max(maximum, a), c), max(g, i));

    float3 amplitude = clamp(
        min(minimum, 2.0 - maximum) *
            almetal_world_cas_low_reciprocal(maximum),
        float3(0.0), float3(1.0));
    amplitude = almetal_world_cas_low_sqrt(amplitude);
    const float3 weight = amplitude * constants.params.x;
    const float3 reciprocal_weight =
        almetal_world_cas_medium_reciprocal(1.0 + 4.0 * weight);
    const float3 result = clamp(
        ((b + d + f + h) * weight + e) * reciprocal_weight,
        float3(0.0), float3(1.0));

    // The standalone GLSL pass initializes a vec4 to zero and only writes
    // RGB through CasFilter, so alpha intentionally remains zero.
    return float4(result, 0.0);
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
    return almetal_chromatic_aberration(
        scene, scene_sampler, uv,
        constants.chromatic.x, constants.chromatic.y,
        constants.chromatic.z, constants.chromatic_angle_offset_r.xy,
        constants.chromatic_angle_offset_r.zw,
        constants.chromatic_offset_b.xy);
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

static float4 almetal_bloom_extract(
    ALMetalResampleVaryings input,
    texture2d<float> scene,
    sampler scene_sampler,
    constant ALMetalPBRPreviewBloomExtractConstants& constants)
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

fragment float4 almetal_pbr_preview_bloom_extract_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> scene [[texture(0)]],
    sampler scene_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewBloomExtractConstants& constants [[buffer(0)]])
{
    return almetal_bloom_extract(
        input, scene, scene_sampler, constants);
}

fragment float4 almetal_world_bloom_extract_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> scene [[texture(0)]],
    sampler scene_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewBloomExtractConstants& constants [[buffer(0)]])
{
    return almetal_bloom_extract(
        input, scene, scene_sampler, constants);
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

static float4 almetal_bloom_downsample(
    ALMetalResampleVaryings input,
    texture2d<float> source,
    sampler source_sampler,
    constant ALMetalPBRPreviewBloomFilterConstants& constants)
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

fragment float4 almetal_pbr_preview_bloom_downsample_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewBloomFilterConstants& constants [[buffer(0)]])
{
    return almetal_bloom_downsample(
        input, source, source_sampler, constants);
}

fragment float4 almetal_world_bloom_downsample_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewBloomFilterConstants& constants [[buffer(0)]])
{
    return almetal_bloom_downsample(
        input, source, source_sampler, constants);
}

static float4 almetal_bloom_upsample(
    ALMetalResampleVaryings input,
    texture2d<float> source,
    sampler source_sampler,
    constant ALMetalPBRPreviewBloomFilterConstants& constants)
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

fragment float4 almetal_pbr_preview_bloom_upsample_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewBloomFilterConstants& constants [[buffer(0)]])
{
    return almetal_bloom_upsample(
        input, source, source_sampler, constants);
}

fragment float4 almetal_world_bloom_upsample_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewBloomFilterConstants& constants [[buffer(0)]])
{
    return almetal_bloom_upsample(
        input, source, source_sampler, constants);
}

static float4 almetal_cross_filter(
    ALMetalResampleVaryings input,
    texture2d<float> source,
    sampler source_sampler,
    constant ALMetalPBRPreviewCrossFilterConstants& constants)
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

fragment float4 almetal_pbr_preview_cross_filter_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewCrossFilterConstants& constants [[buffer(0)]])
{
    return almetal_cross_filter(input, source, source_sampler, constants);
}

fragment float4 almetal_world_cross_filter_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]],
    constant ALMetalPBRPreviewCrossFilterConstants& constants [[buffer(0)]])
{
    return almetal_cross_filter(input, source, source_sampler, constants);
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

struct ALMetalSelectionIDConstants
{
    float4x4 modelview_projection;
    float4x4 texture_matrix;
    float4x4 inverse_projection;
    float4 base_color_transform[2];
    // xy = target dimensions, z = alpha cutoff, w = visible slack.
    float4 screen_material;
    // x = id, y = priority. z bits: reverse-Z, scene depth,
    // show-hidden, outlined and keep-cut. w = skin joint count.
    uint4 config;
};

struct ALMetalSelectionIDVaryings
{
    float4 position [[position]];
    float2 texcoord;
};

static float2 almetal_selection_texcoord(
    float2 source,
    constant ALMetalSelectionIDConstants& constants)
{
    float2 texcoord = (constants.texture_matrix *
                       float4(source, 0.0, 1.0)).xy;
    texcoord.y = 1.0 - texcoord.y;
    const float2 scaled = texcoord * constants.base_color_transform[0].xy;
    const float cosine = cos(constants.base_color_transform[0].z);
    const float sine = sin(constants.base_color_transform[0].z);
    texcoord = float2(cosine * scaled.x + sine * scaled.y,
                      -sine * scaled.x + cosine * scaled.y) +
               constants.base_color_transform[1].xy;
    texcoord.y = 1.0 - texcoord.y;
    return texcoord;
}

static ALMetalSelectionIDVaryings almetal_selection_id_varyings(
    float4 object_position,
    float2 source_texcoord,
    constant ALMetalSelectionIDConstants& constants)
{
    float4 clip_position =
        constants.modelview_projection * object_position;
    // The ordered world/UI capture is a GL-compatible offscreen texture:
    // row zero remains the lower row for its transitional consumer.
    clip_position.y = -clip_position.y;
    ALMetalSelectionIDVaryings output;
    output.position = (constants.config.z & 1u) != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = almetal_selection_texcoord(
        source_texcoord, constants);
    return output;
}

vertex ALMetalSelectionIDVaryings almetal_selection_id_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalSelectionIDConstants& constants [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    return almetal_selection_id_varyings(
        float4(positions[vertex_id].xyz, 1.0), texcoords[vertex_id],
        constants);
}

vertex ALMetalSelectionIDVaryings almetal_selection_id_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    const device float4* encoded_weights [[buffer(2)]],
    constant ALMetalSelectionIDConstants& constants [[buffer(3)]],
    const device float4* skin_palette [[buffer(4)]],
    uint vertex_id [[vertex_id]])
{
    // The selection pass needs position only; the helper's normal result is
    // immaterial, so a fixed direction avoids another vertex stream.
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], float4(0.0, 0.0, 1.0, 0.0),
        encoded_weights[vertex_id], skin_palette, constants.config.w);
    const float4 rebased_position =
        skinned.position + float4(skin_palette[0].xyz, 1.0);
    return almetal_selection_id_varyings(
        rebased_position, texcoords[vertex_id], constants);
}

struct ALMetalSelectionIDOutput
{
    float4 color [[color(0)]];
    float depth [[depth(any)]];
};

static float almetal_selection_eye_distance(
    float depth,
    constant ALMetalSelectionIDConstants& constants)
{
    const float ndc_depth = (constants.config.z & 1u) != 0u
        ? depth : depth * 2.0 - 1.0;
    const float4 eye = constants.inverse_projection *
        float4(0.0, 0.0, ndc_depth, 1.0);
    return -eye.z / max(eye.w, 0.000001);
}

fragment ALMetalSelectionIDOutput almetal_selection_id_fragment(
    ALMetalSelectionIDVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    texture2d<float> scene_depth_map [[texture(1)]],
    sampler diffuse_sampler [[sampler(0)]],
    sampler depth_sampler [[sampler(1)]],
    constant ALMetalSelectionIDConstants& constants [[buffer(0)]])
{
    const float cutoff = constants.screen_material.z;
    float alpha = 1.0;
    if (cutoff > 1.0)
    {
        alpha = 0.0;
    }
    else if (cutoff > 0.0)
    {
        alpha = diffuse_map.sample(
            diffuse_sampler, input.texcoord).a;
    }

    const float own_distance = almetal_selection_eye_distance(
        input.position.z, constants);
    const float slope = abs(dfdx(own_distance)) +
                        abs(dfdy(own_distance));
    float visible = 1.0;
    if ((constants.config.z & 2u) != 0u)
    {
        const float2 screen = constants.screen_material.xy;
        const float scene_depth = scene_depth_map.sample(
            depth_sampler, input.position.xy / screen, level(0.0)).r;
        const float scene_distance = almetal_selection_eye_distance(
            scene_depth, constants);
        const float2 texel_ratio = screen /
            float2(scene_depth_map.get_width(),
                   scene_depth_map.get_height());
        const float reach = 0.5 *
            max(max(texel_ratio.x, texel_ratio.y), 1.0) * slope;
        visible = own_distance <= scene_distance + reach +
            own_distance * constants.screen_material.w ? 1.0 : 0.0;
    }

    const bool cut = alpha < cutoff - (1.0 / 512.0);
    if (cut && (constants.config.z & 16u) == 0u)
    {
        discard_fragment();
    }

    const bool outlined = (constants.config.z & 8u) != 0u;
    const bool show_hidden = (constants.config.z & 4u) != 0u;
    const float drawn = !outlined ? 0.0 :
        (visible > 0.5 ? 1.0 : (show_hidden ? 0.5 : 0.0));
    ALMetalSelectionIDOutput output;
    output.color = float4(
        float(constants.config.x & 255u) / 255.0,
        float((constants.config.x >> 8u) & 255u) / 255.0,
        drawn,
        float(constants.config.y) / 255.0);
    output.depth = cut
        ? (((constants.config.z & 1u) != 0u) ? 0.0 : 1.0)
        : input.position.z;
    return output;
}

// The wireframe expands each indexed source triangle to two native triangles.
// That gives the vertex stage enough lanes to clip the source against the near
// plane itself and retain only the three original edge lines, matching the GL
// geometry shader without drawing a line along the clipped plane.
struct ALMetalSelectionWireConstants
{
    float4x4 modelview_projection;
    float4x4 texture_matrix;
    float4x4 inverse_projection;
    float4 base_color_transform[2];
    // xy = target dimensions, z = alpha cutoff, w = HUD surface slack.
    float4 screen_material;
    // x = id, y = width, z bits: reverse-Z, keep-cut. w = joint count.
    uint4 config;
    // x = wire pass (visible, hidden, HUD id-depth), y = contrast shift.
    uint4 draw;
};

struct ALMetalSelectionWireVaryings
{
    float4 position [[position]];
    float2 texcoord;
    float3 edge_distance [[center_no_perspective]];
    float2 window_position [[center_no_perspective]];
};

static float almetal_selection_wire_inside_near(
    float4 clip, bool reverse_z)
{
    return reverse_z ? clip.w - clip.z : clip.w + clip.z;
}

static float2 almetal_selection_wire_window(
    float4 clip, float2 screen)
{
    // The native target's row orientation is the GL-compatible offscreen
    // orientation established by the y reflection at every production draw.
    return (float2(clip.x, -clip.y) / max(clip.w, 0.000001) * 0.5 + 0.5) *
        screen;
}

static float4 almetal_selection_wire_metal_clip(
    float4 clip, bool reverse_z)
{
    clip.y = -clip.y;
    return reverse_z ? clip : almetal_clip_from_gl(clip);
}

static float2 almetal_selection_wire_texcoord(
    float2 source,
    constant ALMetalSelectionWireConstants& constants)
{
    float2 texcoord = (constants.texture_matrix *
                       float4(source, 0.0, 1.0)).xy;
    texcoord.y = 1.0 - texcoord.y;
    const float2 scaled = texcoord * constants.base_color_transform[0].xy;
    const float cosine = cos(constants.base_color_transform[0].z);
    const float sine = sin(constants.base_color_transform[0].z);
    texcoord = float2(cosine * scaled.x + sine * scaled.y,
                      -sine * scaled.x + cosine * scaled.y) +
               constants.base_color_transform[1].xy;
    texcoord.y = 1.0 - texcoord.y;
    return texcoord;
}

static ALMetalSelectionWireVaryings almetal_selection_wire_vertex_common(
    thread const float4* object_positions,
    thread const float2* source_texcoords,
    constant ALMetalSelectionWireConstants& constants,
    uint lane)
{
    const bool reverse_z = (constants.config.z & 1u) != 0u;
    float4 source_clip[3];
    float2 source_texcoord[3];
    float inside[3];
    bool cut = false;
    bool kept = false;
    for (uint index = 0u; index < 3u; ++index)
    {
        source_clip[index] = constants.modelview_projection *
            object_positions[index];
        source_texcoord[index] = almetal_selection_wire_texcoord(
            source_texcoords[index], constants);
        inside[index] = almetal_selection_wire_inside_near(
            source_clip[index], reverse_z);
        cut = cut || inside[index] < 0.0;
        kept = kept || inside[index] >= 0.0;
    }

    float4 clip[4];
    float2 texcoord[4];
    uint count = 0u;
    if (kept && !cut)
    {
        for (uint index = 0u; index < 3u; ++index)
        {
            clip[index] = source_clip[index];
            texcoord[index] = source_texcoord[index];
        }
        count = 3u;
    }
    else if (kept)
    {
        for (uint index = 0u; index < 3u; ++index)
        {
            const uint next = (index + 1u) % 3u;
            if (inside[index] >= 0.0)
            {
                clip[count] = source_clip[index];
                texcoord[count] = source_texcoord[index];
                ++count;
            }
            if ((inside[index] >= 0.0) != (inside[next] >= 0.0))
            {
                const float amount = inside[index] /
                    (inside[index] - inside[next]);
                clip[count] = mix(
                    source_clip[index], source_clip[next], amount);
                texcoord[count] = mix(
                    source_texcoord[index], source_texcoord[next], amount);
                ++count;
            }
        }
    }

    ALMetalSelectionWireVaryings output;
    if (count < 3u)
    {
        output.position = float4(0.0, 0.0, 0.0, 1.0);
        output.texcoord = float2(0.0);
        output.edge_distance = float3(1000000.0);
        output.window_position = float2(0.0);
        return output;
    }

    const float2 screen = constants.screen_material.xy;
    float2 line_start[3];
    float2 line_direction[3];
    bool has_line[3];
    for (uint edge = 0u; edge < 3u; ++edge)
    {
        const uint from_index = (edge + 1u) % 3u;
        const uint to_index = (edge + 2u) % 3u;
        has_line[edge] = inside[from_index] >= 0.0 ||
                         inside[to_index] >= 0.0;
        float4 from = source_clip[from_index];
        float4 to = source_clip[to_index];
        if (inside[from_index] < 0.0)
        {
            from = mix(from, to, inside[from_index] /
                (inside[from_index] - inside[to_index]));
        }
        else if (inside[to_index] < 0.0)
        {
            to = mix(to, from, inside[to_index] /
                (inside[to_index] - inside[from_index]));
        }
        line_start[edge] = almetal_selection_wire_window(from, screen);
        line_direction[edge] =
            almetal_selection_wire_window(to, screen) - line_start[edge];
        has_line[edge] = has_line[edge] &&
            length(line_direction[edge]) > 0.000001;
    }

    constexpr uint triangle_vertices[6] = {0u, 1u, 3u, 3u, 1u, 2u};
    uint corner = count == 4u ? triangle_vertices[lane] :
        (lane < 3u ? lane : 0u);
    const float2 window = almetal_selection_wire_window(
        clip[corner], screen);
    float3 distance;
    for (uint edge = 0u; edge < 3u; ++edge)
    {
        const float2 from_start = window - line_start[edge];
        distance[edge] = has_line[edge]
            ? abs(line_direction[edge].x * from_start.y -
                  line_direction[edge].y * from_start.x) /
                  length(line_direction[edge])
            : 1000000.0;
    }
    output.position = almetal_selection_wire_metal_clip(
        clip[corner], reverse_z);
    // The unused second triangle is exactly degenerate for an unclipped
    // source triangle, so it produces no fragments under either cull mode.
    if (count == 3u && lane >= 3u)
    {
        output.position = almetal_selection_wire_metal_clip(
            clip[0], reverse_z);
    }
    output.texcoord = texcoord[corner];
    output.edge_distance = distance;
    output.window_position = window;
    return output;
}

static ALMetalSelectionWireVaryings almetal_selection_wire_static_vertex(
    const device float4* positions,
    const device float2* texcoords,
    constant ALMetalSelectionWireConstants& constants,
    uint3 source_ids, uint lane)
{
    float4 triangle_positions[3];
    float2 triangle_texcoords[3];
    for (uint index = 0u; index < 3u; ++index)
    {
        triangle_positions[index] =
            float4(positions[source_ids[index]].xyz, 1.0);
        triangle_texcoords[index] = texcoords[source_ids[index]];
    }
    return almetal_selection_wire_vertex_common(
        triangle_positions, triangle_texcoords, constants, lane);
}

static ALMetalSelectionWireVaryings almetal_selection_wire_skinned_vertex(
    const device float4* positions,
    const device float2* texcoords,
    const device float4* encoded_weights,
    const device float4* skin_palette,
    constant ALMetalSelectionWireConstants& constants,
    uint3 source_ids, uint lane)
{
    float4 triangle_positions[3];
    float2 triangle_texcoords[3];
    for (uint index = 0u; index < 3u; ++index)
    {
        const uint source_id = source_ids[index];
        const ALMetalObjectSkinnedVertex skinned =
            almetal_object_skin_vertex(
                positions[source_id], float4(0.0, 0.0, 1.0, 0.0),
                encoded_weights[source_id], skin_palette,
                constants.config.w);
        triangle_positions[index] =
            skinned.position + float4(skin_palette[0].xyz, 1.0);
        triangle_texcoords[index] = texcoords[source_id];
    }
    return almetal_selection_wire_vertex_common(
        triangle_positions, triangle_texcoords, constants, lane);
}

vertex ALMetalSelectionWireVaryings almetal_selection_wire_u16_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalSelectionWireConstants& constants [[buffer(3)]],
    const device ushort* indices [[buffer(5)]],
    uint vertex_id [[vertex_id]])
{
    const uint start = (vertex_id / 6u) * 3u;
    return almetal_selection_wire_static_vertex(
        positions, texcoords, constants,
        uint3(indices[start], indices[start + 1u], indices[start + 2u]),
        vertex_id % 6u);
}

vertex ALMetalSelectionWireVaryings almetal_selection_wire_u32_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalSelectionWireConstants& constants [[buffer(3)]],
    const device uint* indices [[buffer(5)]],
    uint vertex_id [[vertex_id]])
{
    const uint start = (vertex_id / 6u) * 3u;
    return almetal_selection_wire_static_vertex(
        positions, texcoords, constants,
        uint3(indices[start], indices[start + 1u], indices[start + 2u]),
        vertex_id % 6u);
}

vertex ALMetalSelectionWireVaryings
almetal_selection_wire_skinned_u16_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    const device float4* encoded_weights [[buffer(2)]],
    constant ALMetalSelectionWireConstants& constants [[buffer(3)]],
    const device float4* skin_palette [[buffer(4)]],
    const device ushort* indices [[buffer(5)]],
    uint vertex_id [[vertex_id]])
{
    const uint start = (vertex_id / 6u) * 3u;
    return almetal_selection_wire_skinned_vertex(
        positions, texcoords, encoded_weights, skin_palette, constants,
        uint3(indices[start], indices[start + 1u], indices[start + 2u]),
        vertex_id % 6u);
}

vertex ALMetalSelectionWireVaryings
almetal_selection_wire_skinned_u32_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    const device float4* encoded_weights [[buffer(2)]],
    constant ALMetalSelectionWireConstants& constants [[buffer(3)]],
    const device float4* skin_palette [[buffer(4)]],
    const device uint* indices [[buffer(5)]],
    uint vertex_id [[vertex_id]])
{
    const uint start = (vertex_id / 6u) * 3u;
    return almetal_selection_wire_skinned_vertex(
        positions, texcoords, encoded_weights, skin_palette, constants,
        uint3(indices[start], indices[start + 1u], indices[start + 2u]),
        vertex_id % 6u);
}

// The optimized selection edge transaction mirrors selectionJumpF.glsl,
// selectionTileF.glsl, selectionGlowReachF.glsl, selectionGlowRowF.glsl,
// selectionBackdropMaskF.glsl and selectionOutlineF.glsl. Its private
// id/depth target and intermediates never leave the GPU.
struct ALMetalSelectionContourConstants
{
    float4x4 inverse_projection;
    // xy = id target, zw = tile target dimensions.
    uint4 target;
    // x = contour width, y = inner width, z = rim width, w = reverse-Z.
    uint4 widths;
    // x = glow radius, y = contour enabled, z = contrast shift enabled.
    uint4 glow;
    // x = glow brightness.
    float4 glow_values;
};

static uint almetal_selection_decode_id(float4 texel)
{
    return uint(texel.r * 255.0 + 0.5) |
           (uint(texel.g * 255.0 + 0.5) << 8u);
}

static uint almetal_selection_decode_priority(float4 texel)
{
    return uint(texel.a * 255.0 + 0.5);
}

static bool almetal_selection_is_glow(float4 texel)
{
    return almetal_selection_decode_priority(texel) == 6u;
}

static bool almetal_selection_is_drawn(float4 texel)
{
    return texel.b > 0.25;
}

static bool almetal_selection_is_visible(float4 texel)
{
    return texel.b > 0.75;
}

static float almetal_selection_contour_eye_distance(
    float depth,
    constant ALMetalSelectionContourConstants& constants)
{
    const float ndc_depth = constants.widths.w != 0u
        ? depth : depth * 2.0 - 1.0;
    const float4 eye = constants.inverse_projection *
        float4(0.0, 0.0, ndc_depth, 1.0);
    return -eye.z / max(eye.w, 0.000001);
}

static bool almetal_selection_has_id(
    int2 position, int2 size, uint id,
    texture2d<float, access::read> id_map)
{
    return all(position >= int2(0)) && all(position < size) &&
           almetal_selection_decode_id(id_map.read(uint2(position))) == id;
}

static bool almetal_selection_jump_test(
    float depth, float behind, float other, float beyond,
    constant ALMetalSelectionContourConstants& constants)
{
    const float own_slope = depth - behind;
    const float other_slope = beyond - other;
    const float across = other - depth;
    if ((across - own_slope) * (across - other_slope) <= 0.0)
    {
        return false;
    }
    const float own = almetal_selection_contour_eye_distance(depth, constants);
    const float there = almetal_selection_contour_eye_distance(other, constants);
    return own < there &&
        abs(almetal_selection_contour_eye_distance(
                depth + own_slope, constants) - there) > there * 0.01 &&
        abs(almetal_selection_contour_eye_distance(
                other - other_slope, constants) - own) > own * 0.01;
}

static bool almetal_selection_jumps_along(
    int2 position, int2 size, int2 step, uint id, float depth,
    texture2d<float, access::read> id_map,
    texture2d<float, access::read> depth_map,
    constant ALMetalSelectionContourConstants& constants)
{
    const int2 behind = position - step;
    const int2 other = position + step;
    const int2 beyond = position + step * 2;
    if (all(behind >= int2(0)) && all(other >= int2(0)) &&
        all(beyond >= int2(0)) && all(behind < size) &&
        all(other < size) && all(beyond < size) &&
        almetal_selection_jump_test(
            depth, depth_map.read(uint2(behind)).r,
            depth_map.read(uint2(other)).r,
            depth_map.read(uint2(beyond)).r, constants) &&
        almetal_selection_has_id(behind, size, id, id_map) &&
        almetal_selection_has_id(other, size, id, id_map) &&
        almetal_selection_has_id(beyond, size, id, id_map))
    {
        return true;
    }

    const int2 reverse_behind = position + step;
    const int2 reverse_other = position - step;
    const int2 reverse_beyond = position - step * 2;
    return all(reverse_behind >= int2(0)) &&
        all(reverse_other >= int2(0)) &&
        all(reverse_beyond >= int2(0)) &&
        all(reverse_behind < size) && all(reverse_other < size) &&
        all(reverse_beyond < size) &&
        almetal_selection_jump_test(
            depth, depth_map.read(uint2(reverse_behind)).r,
            depth_map.read(uint2(reverse_other)).r,
            depth_map.read(uint2(reverse_beyond)).r, constants) &&
        almetal_selection_has_id(reverse_behind, size, id, id_map) &&
        almetal_selection_has_id(reverse_other, size, id, id_map) &&
        almetal_selection_has_id(reverse_beyond, size, id, id_map);
}

fragment float almetal_selection_jump_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float, access::read> id_map [[texture(0)]],
    texture2d<float, access::read> depth_map [[texture(1)]],
    constant ALMetalSelectionContourConstants& constants [[buffer(0)]])
{
    const int2 size = int2(constants.target.xy);
    const int2 position = clamp(
        int2(input.position.xy), int2(0), size - int2(1));
    const float4 texel = id_map.read(uint2(position));
    const uint id = almetal_selection_decode_id(texel);
    bool front = false;
    if (id != 0u && !almetal_selection_is_glow(texel))
    {
        const float depth = depth_map.read(uint2(position)).r;
        front = almetal_selection_jumps_along(
                    position, size, int2(1, 0), id, depth,
                    id_map, depth_map, constants) ||
                almetal_selection_jumps_along(
                    position, size, int2(0, 1), id, depth,
                    id_map, depth_map, constants);
    }
    return front ? 1.0 : 0.0;
}

struct ALMetalSelectionTileOutput
{
    float4 ids [[color(0)]];
    float nearest [[color(1)]];
};

fragment ALMetalSelectionTileOutput almetal_selection_tile_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float, access::read> id_map [[texture(0)]],
    texture2d<float, access::read> depth_map [[texture(1)]],
    texture2d<float, access::read> jump_map [[texture(2)]],
    constant ALMetalSelectionContourConstants& constants [[buffer(0)]])
{
    const int2 size = int2(constants.target.xy);
    const int2 origin = int2(input.position.xy) * 8;
    const int2 end = min(origin + int2(8), size);
    uint lowest = 65536u;
    uint highest = 0u;
    bool jump = false;
    bool glow = false;
    float nearest = 1.0e30;
    for (int y = origin.y; y < end.y; ++y)
    {
        for (int x = origin.x; x < end.x; ++x)
        {
            const uint2 position = uint2(x, y);
            const float4 texel = id_map.read(position);
            const uint id = almetal_selection_decode_id(texel);
            if (id == 0u)
            {
                continue;
            }
            lowest = min(lowest, id);
            highest = max(highest, id);
            const bool texel_glows = almetal_selection_is_glow(texel);
            jump = jump || (!texel_glows && jump_map.read(position).r > 0.5);
            glow = glow || (texel_glows && almetal_selection_is_drawn(texel));
            if (!texel_glows)
            {
                nearest = min(nearest,
                    almetal_selection_contour_eye_distance(
                        depth_map.read(position).r, constants));
            }
        }
    }
    ALMetalSelectionTileOutput output;
    output.ids = float4(
        highest != 0u ? float(lowest) / 65535.0 : 0.0,
        float(highest) / 65535.0, jump ? 1.0 : 0.0,
        glow ? 1.0 : 0.0);
    output.nearest = nearest;
    return output;
}

fragment float4 almetal_selection_backdrop_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float, access::read> current_map [[texture(0)]],
    texture2d<float, access::read> id_map [[texture(1)]],
    texture2d<float, access::read> depth_map [[texture(2)]],
    constant ALMetalSelectionContourConstants& constants [[buffer(0)]])
{
    const int2 size = int2(constants.target.xy);
    const int2 origin = int2(input.position.xy) * 2;
    const float cleared_depth = constants.widths.w != 0u ? 0.0 : 1.0;
    float3 color = float3(0.0);
    bool selected = false;
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 2; ++x)
        {
            const int2 position = min(origin + int2(x, y), size - int2(1));
            const float4 texel = id_map.read(uint2(position));
            color += current_map.read(uint2(position)).rgb;
            selected = selected ||
                (almetal_selection_decode_id(texel) != 0u &&
                 !almetal_selection_is_glow(texel) &&
                 almetal_selection_is_visible(texel) &&
                 depth_map.read(uint2(position)).r != cleared_depth);
        }
    }
    return float4(color * 0.25, selected ? 0.0 : 1.0);
}

fragment float4 almetal_selection_glow_reach_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float, access::read> tile_map [[texture(0)]],
    constant ALMetalSelectionContourConstants& constants [[buffer(0)]])
{
    const int2 tile = int2(input.position.xy);
    const int2 last = int2(constants.target.zw) - int2(1);
    const int tile_reach =
        (int(constants.glow.x) + 1 + 7) / 8;
    const int2 low = max(tile - int2(tile_reach), int2(0));
    const int2 high = min(tile + int2(tile_reach), last);
    uint lowest = 65536u;
    uint highest = 0u;
    for (int y = low.y; y <= high.y; ++y)
    {
        for (int x = low.x; x <= high.x; ++x)
        {
            const float4 record = tile_map.read(uint2(x, y));
            if (record.a > 0.5)
            {
                lowest = min(lowest,
                    uint(record.r * 65535.0 + 0.5));
                highest = max(highest,
                    uint(record.g * 65535.0 + 0.5));
            }
        }
    }
    return float4(
        highest != 0u ? float(lowest) / 65535.0 : 0.0,
        float(highest) / 65535.0, 0.0, 0.0);
}

static bool almetal_selection_glows_at(
    int2 position, int2 size,
    texture2d<float, access::read> id_map)
{
    if (any(position < int2(0)) || any(position >= size))
    {
        return false;
    }
    const float4 texel = id_map.read(uint2(position));
    return almetal_selection_decode_id(texel) != 0u &&
        almetal_selection_is_glow(texel) &&
        almetal_selection_is_drawn(texel);
}

fragment float almetal_selection_glow_row_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float, access::read> id_map [[texture(0)]],
    texture2d<float, access::read> glow_reach_map [[texture(1)]],
    constant ALMetalSelectionContourConstants& constants [[buffer(0)]])
{
    const int2 size = int2(constants.target.xy);
    const int2 position = clamp(
        int2(input.position.xy), int2(0), size - int2(1));
    uint code = 0u;
    if (glow_reach_map.read(uint2(position / 8)).g > 0.0)
    {
        const int reach = int(constants.glow.x) + 1;
        for (int distance = 0; distance <= reach; ++distance)
        {
            if (almetal_selection_glows_at(
                    position - int2(distance, 0), size, id_map) ||
                almetal_selection_glows_at(
                    position + int2(distance, 0), size, id_map))
            {
                code = uint(distance + 1);
                break;
            }
        }
    }
    return float(code) / 255.0;
}

static float almetal_selection_depth_slope(
    int2 position, int2 size, int2 step, uint id, float depth,
    texture2d<float, access::read> id_map,
    texture2d<float, access::read> depth_map)
{
    const bool back = almetal_selection_has_id(
        position - step, size, id, id_map);
    const bool ahead = almetal_selection_has_id(
        position + step, size, id, id_map);
    const float to_back = back
        ? depth - depth_map.read(uint2(position - step)).r : 0.0;
    const float to_ahead = ahead
        ? depth_map.read(uint2(position + step)).r - depth : 0.0;
    if (back && ahead)
    {
        return abs(to_back) <= abs(to_ahead) ? to_back : to_ahead;
    }
    return back ? to_back : to_ahead;
}

static bool almetal_selection_candidate_within(
    int2 position, int reach, uint own_id,
    thread bool& jump, thread float& nearest,
    texture2d<float, access::read> tile_map,
    texture2d<float, access::read> tile_depth_map,
    constant ALMetalSelectionContourConstants& constants)
{
    const int2 last = int2(constants.target.zw) - int2(1);
    const int2 low = max(position - int2(reach), int2(0)) / 8;
    const int2 high = min((position + int2(reach)) / 8, last);
    bool found = false;
    jump = false;
    nearest = 1.0e30;
    for (int y = low.y; y <= high.y; ++y)
    {
        for (int x = low.x; x <= high.x; ++x)
        {
            const uint2 tile_position = uint2(x, y);
            nearest = min(nearest, tile_depth_map.read(tile_position).r);
            const float4 tile = tile_map.read(tile_position);
            const uint lowest = uint(tile.r * 65535.0 + 0.5);
            const uint highest = uint(tile.g * 65535.0 + 0.5);
            const bool tile_jump = tile.b > 0.5;
            found = found || (own_id == 0u
                ? highest != 0u
                : ((lowest != 0u && lowest != own_id) ||
                   (highest != 0u && highest != own_id) || tile_jump));
            jump = jump || tile_jump;
        }
    }
    return found;
}

static bool almetal_selection_glow_within(
    int2 position, uint own_id,
    texture2d<float, access::read> glow_reach_map)
{
    const float4 record = glow_reach_map.read(uint2(position / 8));
    const uint lowest = uint(record.r * 65535.0 + 0.5);
    const uint highest = uint(record.g * 65535.0 + 0.5);
    return highest != 0u &&
        (lowest != own_id || highest != own_id);
}

static uint almetal_selection_glow_id_at(
    int2 position, int2 size,
    texture2d<float, access::read> id_map)
{
    if (any(position < int2(0)) || any(position >= size))
    {
        return 65536u;
    }
    const float4 texel = id_map.read(uint2(position));
    const uint id = almetal_selection_decode_id(texel);
    return id != 0u && almetal_selection_is_glow(texel) &&
        almetal_selection_is_drawn(texel) ? id : 65536u;
}

static void almetal_selection_glow_from_rows(
    int2 position, int2 size, int reach,
    thread uint& glow_id, thread int& glow_distance_squared,
    texture2d<float, access::read> id_map,
    texture2d<float, access::read> glow_row_map)
{
    for (int dy = 0;
         dy <= reach && dy * dy <= glow_distance_squared; ++dy)
    {
        const int sides = dy == 0 ? 1 : 2;
        for (int side = 0; side < sides; ++side)
        {
            const int y = position.y + (side == 0 ? dy : -dy);
            if (y < 0 || y >= size.y)
            {
                continue;
            }
            const int code = int(
                glow_row_map.read(uint2(position.x, y)).r * 255.0 + 0.5);
            if (code == 0)
            {
                continue;
            }
            const int dx = code - 1;
            const int distance_squared = dx * dx + dy * dy;
            if (distance_squared > glow_distance_squared)
            {
                continue;
            }
            const uint id = min(
                almetal_selection_glow_id_at(
                    int2(position.x - dx, y), size, id_map),
                almetal_selection_glow_id_at(
                    int2(position.x + dx, y), size, id_map));
            if (distance_squared < glow_distance_squared || id < glow_id)
            {
                glow_id = id;
                glow_distance_squared = distance_squared;
            }
        }
    }
}

static float almetal_selection_coverage(float distance, float width)
{
    return 1.0 - smoothstep(width, width + 1.0, distance);
}

static float almetal_selection_glow_opacity(
    float distance,
    constant ALMetalSelectionContourConstants& constants)
{
    const float x = max(distance - 0.5, 0.0) /
        float(constants.glow.x);
    const float profile = 0.25 * (1.0 - smoothstep(0.0, 1.0, x));
    return clamp(constants.glow_values.x * profile, 0.0, 1.0);
}

static float3 almetal_selection_rim_color(float3 line)
{
    const float luminance = dot(
        almetal_srgb_to_linear(line), float3(0.2126, 0.7152, 0.0722));
    return luminance > 0.18 ? float3(0.0) : float3(1.0);
}

static float3 almetal_selection_linear_to_oklab(float3 color)
{
    float3 lms = float3(
        0.4122214708 * color.r + 0.5363325363 * color.g +
            0.0514459929 * color.b,
        0.2119034982 * color.r + 0.6806995451 * color.g +
            0.1073969566 * color.b,
        0.0883024619 * color.r + 0.2817188376 * color.g +
            0.6299787005 * color.b);
    lms = sign(lms) * pow(abs(lms), float3(1.0 / 3.0));
    return float3(
        0.2104542553 * lms.x + 0.7936177850 * lms.y -
            0.0040720468 * lms.z,
        1.9779984951 * lms.x - 2.4285922050 * lms.y +
            0.4505937099 * lms.z,
        0.0259040371 * lms.x + 0.7827717662 * lms.y -
            0.8086757660 * lms.z);
}

static float3 almetal_selection_oklab_to_linear(float3 lab)
{
    float3 lms = float3(
        lab.x + 0.3963377774 * lab.y + 0.2158037573 * lab.z,
        lab.x - 0.1055613458 * lab.y - 0.0638541728 * lab.z,
        lab.x - 0.0894841775 * lab.y - 1.2914855480 * lab.z);
    lms = lms * lms * lms;
    return float3(
        4.0767416621 * lms.x - 1.2684380046 * lms.y -
            0.0041960863 * lms.z,
        -3.3077115913 * lms.x + 2.6097574011 * lms.y -
            0.7034186147 * lms.z,
        0.2309699292 * lms.x - 0.3413193965 * lms.y +
            1.7076147010 * lms.z);
}

static float almetal_selection_contrast_lightness(float line, float back)
{
    const float turn = clamp(line, 0.3, 0.7);
    if (back < turn)
    {
        return max(line, back + 0.3);
    }
    return line <= back - 0.3
        ? line : max(back - 0.3 - (1.0 - line), 0.0);
}

static float3 almetal_selection_with_lightness(
    float3 color, float lightness)
{
    const float2 ab = almetal_selection_linear_to_oklab(
        almetal_srgb_to_linear(color)).yz;
    float kept = 1.0;
    float3 rgb = almetal_selection_oklab_to_linear(
        float3(lightness, ab));
    if (any(rgb < float3(0.0)) || any(rgb > float3(1.0)))
    {
        float low = 0.0;
        float high = 1.0;
        for (int iteration = 0; iteration < 10; ++iteration)
        {
            const float amount = 0.5 * (low + high);
            rgb = almetal_selection_oklab_to_linear(
                float3(lightness, ab * amount));
            if (any(rgb < float3(0.0)) || any(rgb > float3(1.0)))
            {
                high = amount;
            }
            else
            {
                low = amount;
            }
        }
        kept = low;
    }
    return almetal_linear_to_srgb(clamp(
        almetal_selection_oklab_to_linear(
            float3(lightness, ab * kept)), 0.0, 1.0));
}

static float4 almetal_selection_backdrop_color(
    int2 position,
    texture2d<float, access::read> backdrop,
    bool enabled,
    bool masked)
{
    if (!enabled)
    {
        return float4(0.0);
    }
    const int2 last = int2(
        backdrop.get_width(), backdrop.get_height()) - int2(1);
    const int2 center = position / 2;
    float3 sum = float3(0.0);
    float weight = 0.0;
    for (int dy = -2; dy < 2; ++dy)
    {
        for (int dx = -2; dx < 2; ++dx)
        {
            const float4 texel = backdrop.read(uint2(clamp(
                center + int2(dx, dy), int2(0), last)));
            const float texel_weight = masked ? texel.a : 1.0;
            sum += almetal_srgb_to_linear(texel.rgb) * texel_weight;
            weight += texel_weight;
        }
    }
    return weight > 0.0
        ? float4(almetal_selection_linear_to_oklab(sum / weight), 1.0)
        : float4(0.0);
}

static float3 almetal_selection_contrast_color(
    float3 color, float4 back)
{
    if (back.w <= 0.0)
    {
        return color;
    }
    const float3 lab = almetal_selection_linear_to_oklab(
        almetal_srgb_to_linear(color));
    if (length(lab - back.xyz) >= 0.3)
    {
        return color;
    }
    const float lightness = almetal_selection_contrast_lightness(
        lab.x, back.x);
    return lightness == lab.x
        ? color : almetal_selection_with_lightness(color, lightness);
}

static float4 almetal_selection_palette_color(
    uint id, bool visible,
    texture2d<float, access::read> palette)
{
    const uint row = (id / 256u) * 2u + (visible ? 0u : 1u);
    return palette.read(uint2(id % 256u, row));
}

static float almetal_selection_wire_eye_distance(
    float depth,
    constant ALMetalSelectionWireConstants& constants)
{
    const float ndc_depth = (constants.config.z & 1u) != 0u
        ? depth : depth * 2.0 - 1.0;
    const float4 eye = constants.inverse_projection *
        float4(0.0, 0.0, ndc_depth, 1.0);
    return -eye.z / max(eye.w, 0.000001);
}

fragment float4 almetal_selection_wire_fragment(
    ALMetalSelectionWireVaryings input [[stage_in]],
    texture2d<float> diffuse_map [[texture(0)]],
    texture2d<float, access::read> selection_depth [[texture(1)]],
    texture2d<float, access::read> palette [[texture(2)]],
    texture2d<float, access::read> backdrop [[texture(3)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant ALMetalSelectionWireConstants& constants [[buffer(0)]])
{
    const uint pass = constants.draw.x;
    float own_distance = 0.0;
    float slope = 0.0;
    if (pass == 2u)
    {
        own_distance = almetal_selection_wire_eye_distance(
            input.position.z, constants);
        slope = fwidth(own_distance);
    }

    const float cutoff = constants.screen_material.z;
    float alpha = 1.0;
    if (cutoff > 1.0)
    {
        alpha = 0.0;
    }
    else if (cutoff > 0.0)
    {
        alpha = diffuse_map.sample(
            diffuse_sampler, input.texcoord).a;
    }
    const bool cut = alpha < cutoff - (1.0 / 512.0);
    if (cut && (constants.config.z & 2u) == 0u)
    {
        discard_fragment();
    }

    const float distance = min(
        min(input.edge_distance.x, input.edge_distance.y),
        input.edge_distance.z);
    const float half_width = 0.5 * float(constants.config.y);
    const float coverage = 1.0 - smoothstep(
        half_width - 0.5, half_width + 0.5, distance);
    if (coverage <= 0.0)
    {
        discard_fragment();
    }

    bool visible = pass == 0u;
    if (pass == 2u)
    {
        const int2 size = int2(constants.screen_material.xy);
        const int2 position = clamp(
            int2(input.window_position), int2(0), size - int2(1));
        const float surface_distance =
            almetal_selection_wire_eye_distance(
                selection_depth.read(uint2(position)).r, constants);
        visible = own_distance <= surface_distance + slope +
            own_distance * constants.screen_material.w;
    }

    float4 color = almetal_selection_palette_color(
        constants.config.x, visible, palette);
    if (color.a <= 0.0)
    {
        discard_fragment();
    }
    const int2 position = clamp(
        int2(input.window_position), int2(0),
        int2(constants.screen_material.xy) - int2(1));
    color.rgb = almetal_selection_contrast_color(
        color.rgb,
        almetal_selection_backdrop_color(
            position, backdrop, constants.draw.y != 0u, false));
    return float4(color.rgb, color.a * coverage);
}

static float4 almetal_selection_contour_layer(
    float4 color, float distance,
    constant ALMetalSelectionContourConstants& constants)
{
    const float line = almetal_selection_coverage(
        distance, float(constants.widths.x));
    float4 layer = float4(color.rgb, 1.0) * (color.a * line);
    if (constants.widths.z != 0u)
    {
        const float rim = color.a *
            (almetal_selection_coverage(
                 distance, float(constants.widths.x + constants.widths.z)) -
             line);
        layer += float4(almetal_selection_rim_color(color.rgb), 1.0) * rim;
    }
    return layer;
}

fragment float4 almetal_selection_contour_fragment(
    ALMetalResampleVaryings input [[stage_in]],
    texture2d<float, access::read> id_map [[texture(0)]],
    texture2d<float, access::read> depth_map [[texture(1)]],
    texture2d<float, access::read> jump_map [[texture(2)]],
    texture2d<float, access::read> tile_map [[texture(3)]],
    texture2d<float, access::read> tile_depth_map [[texture(4)]],
    texture2d<float, access::read> palette [[texture(5)]],
    texture2d<float, access::read> glow_reach_map [[texture(6)]],
    texture2d<float, access::read> glow_row_map [[texture(7)]],
    texture2d<float, access::read> backdrop_map [[texture(8)]],
    constant ALMetalSelectionContourConstants& constants [[buffer(0)]])
{
    const int2 size = int2(constants.target.xy);
    const int2 position = clamp(
        int2(input.position.xy), int2(0), size - int2(1));
    const float4 own = id_map.read(uint2(position));
    const uint own_id = almetal_selection_decode_id(own);
    const bool own_drawn = own_id != 0u &&
        almetal_selection_is_drawn(own);
    const bool own_glows = own_id != 0u &&
        almetal_selection_is_glow(own);
    const bool contour_enabled = constants.glow.y != 0u;

    const int line_reach = int(max(
        constants.widths.x + constants.widths.z,
        constants.widths.y)) + 1;
    const int inner_reach = int(constants.widths.y) + 1;
    const int glow_reach = constants.glow.x != 0u
        ? int(constants.glow.x) + 1 : 0;
    const bool glow_near = glow_reach > 0 &&
        almetal_selection_glow_within(
            position, own_id, glow_reach_map);
    bool jump_near = false;
    float tile_nearest = 0.0;
    const bool line_near = contour_enabled &&
        almetal_selection_candidate_within(
            position, line_reach, own_id, jump_near, tile_nearest,
            tile_map, tile_depth_map, constants);
    if (!glow_near && !line_near)
    {
        discard_fragment();
    }

    bool planed = false;
    float own_depth = 0.0;
    float2 slope = float2(0.0);
    int line_search = line_reach;
    if (line_near && own_id != 0u && !own_glows && !jump_near)
    {
        own_depth = depth_map.read(uint2(position)).r;
        slope = float2(
            almetal_selection_depth_slope(
                position, size, int2(1, 0), own_id, own_depth,
                id_map, depth_map),
            almetal_selection_depth_slope(
                position, size, int2(0, 1), own_id, own_depth,
                id_map, depth_map));
        planed = true;
        float plane_far = 0.0;
        for (int corner = 0; corner < 4; ++corner)
        {
            const float2 to = float2(
                (corner & 1) == 0 ? -line_reach : line_reach,
                (corner & 2) == 0 ? -line_reach : line_reach);
            plane_far = max(plane_far,
                almetal_selection_contour_eye_distance(
                    clamp(own_depth + dot(slope, to), 0.0, 1.0),
                    constants));
        }
        if (tile_nearest >= plane_far)
        {
            line_search = inner_reach;
        }
    }

    const bool glow_rows = glow_near && own_id == 0u;
    const bool glow_taps = glow_near && !glow_rows;
    uint glow_id = 0u;
    int glow_distance_squared = glow_reach * glow_reach + 1;
    if (glow_rows)
    {
        almetal_selection_glow_from_rows(
            position, size, glow_reach, glow_id,
            glow_distance_squared, id_map, glow_row_map);
    }
    const int reach = glow_taps
        ? max(line_search, glow_reach) : line_search;

    uint top_id = 0u;
    uint top_priority = 256u;
    float top_distance = 0.0;
    bool top_visible = true;
    uint near_id = 0u;
    uint near_priority = 256u;
    float near_distance = float(line_reach) + 1.0;
    bool near_visible = true;
    float inner_distance = float(inner_reach) + 1.0;

    const int disk = (line_near || glow_taps) ? reach : -1;
    for (int dy = -disk; dy <= disk; ++dy)
    {
        const int span = int(sqrt(
            float(reach * reach - dy * dy) + 0.5));
        for (int dx = -span; dx <= span; ++dx)
        {
            const int2 tap_position = position + int2(dx, dy);
            if (any(tap_position < int2(0)) || any(tap_position >= size))
            {
                continue;
            }
            const float4 tap = id_map.read(uint2(tap_position));
            const uint tap_id = almetal_selection_decode_id(tap);
            const bool own_object = tap_id == own_id;
            if (tap_id == 0u ||
                (own_object && !(jump_near &&
                    jump_map.read(uint2(tap_position)).r > 0.5)))
            {
                continue;
            }
            const int distance_squared = dx * dx + dy * dy;
            const bool tap_glows = almetal_selection_is_glow(tap);
            const int tap_reach = tap_glows ? glow_reach : line_reach;
            if ((tap_glows && (own_object || !glow_taps)) ||
                distance_squared > tap_reach * tap_reach)
            {
                continue;
            }
            const float distance = length(float2(dx, dy));

            uint relation = 0u;
            if (own_id != 0u)
            {
                if (!planed)
                {
                    own_depth = depth_map.read(uint2(position)).r;
                    slope = float2(
                        almetal_selection_depth_slope(
                            position, size, int2(1, 0), own_id,
                            own_depth, id_map, depth_map),
                        almetal_selection_depth_slope(
                            position, size, int2(0, 1), own_id,
                            own_depth, id_map, depth_map));
                    planed = true;
                }
                const float plane = almetal_selection_contour_eye_distance(
                    clamp(own_depth + dot(
                        slope, float2(dx, dy)), 0.0, 1.0), constants);
                const float there = almetal_selection_contour_eye_distance(
                    depth_map.read(uint2(tap_position)).r, constants);
                const float margin = plane * 0.002 + 0.0001;
                relation = there < plane - margin
                    ? 0u : (there <= plane + margin ? 1u : 2u);
            }

            const bool tap_drawn = almetal_selection_is_drawn(tap);
            const bool in_front = relation == 0u ||
                (own_glows && relation == 1u);
            if (tap_glows)
            {
                if (relation == 0u && tap_drawn &&
                    (distance_squared < glow_distance_squared ||
                     (distance_squared == glow_distance_squared &&
                      tap_id < glow_id)))
                {
                    glow_id = tap_id;
                    glow_distance_squared = distance_squared;
                }
            }
            else if (contour_enabled && in_front && tap_drawn)
            {
                const uint tap_priority =
                    almetal_selection_decode_priority(tap);
                if (tap_priority < top_priority ||
                    (tap_priority == top_priority &&
                     distance < top_distance))
                {
                    top_id = tap_id;
                    top_priority = tap_priority;
                    top_distance = distance;
                    top_visible = almetal_selection_is_visible(tap);
                }
                if (distance < near_distance ||
                    (distance == near_distance &&
                     tap_priority < near_priority))
                {
                    near_id = tap_id;
                    near_priority = tap_priority;
                    near_distance = distance;
                    near_visible = almetal_selection_is_visible(tap);
                }
            }
            else if (contour_enabled && relation == 1u && !own_object &&
                     own_drawn && !own_glows &&
                     distance <= float(inner_reach) &&
                     (!tap_drawn || own_id < tap_id))
            {
                inner_distance = min(inner_distance, distance);
            }
        }
    }

    const float4 back = contour_enabled &&
        (inner_distance <= float(inner_reach) || top_id != 0u)
        ? almetal_selection_backdrop_color(
              position, backdrop_map, constants.glow.z != 0u, true)
        : float4(0.0);

    float3 rgb = float3(0.0);
    float alpha = 0.0;
    if (glow_id != 0u)
    {
        const float4 glow_color = almetal_selection_palette_color(
            glow_id, true, palette);
        alpha = glow_color.a * almetal_selection_glow_opacity(
            sqrt(float(glow_distance_squared)), constants);
        rgb = glow_color.rgb * alpha;
    }
    if (contour_enabled && own_drawn && !own_glows)
    {
        float4 color = almetal_selection_palette_color(
            own_id, almetal_selection_is_visible(own), palette);
        const float inner_alpha = color.a * 0.8 *
            almetal_selection_coverage(
                inner_distance, float(constants.widths.y));
        if (inner_alpha > 0.0)
        {
            color.rgb = almetal_selection_contrast_color(color.rgb, back);
            rgb = color.rgb * inner_alpha + rgb * (1.0 - inner_alpha);
            alpha = inner_alpha + alpha * (1.0 - inner_alpha);
        }
    }
    if (contour_enabled && top_id != 0u)
    {
        const float outer = float(constants.widths.x + constants.widths.z);
        float4 top_color = almetal_selection_palette_color(
            top_id, top_visible, palette);
        if (almetal_selection_coverage(top_distance, outer) > 0.0)
        {
            top_color.rgb = almetal_selection_contrast_color(
                top_color.rgb, back);
        }
        const float4 top_layer = almetal_selection_contour_layer(
            top_color, top_distance, constants);
        float4 near_layer = float4(0.0);
        if (near_id != top_id &&
            almetal_selection_coverage(near_distance, outer) > 0.0)
        {
            float4 near_color = almetal_selection_palette_color(
                near_id, near_visible, palette);
            near_color.rgb = almetal_selection_contrast_color(
                near_color.rgb, back);
            near_layer = almetal_selection_contour_layer(
                near_color, near_distance, constants);
        }
        const float3 contour_rgb = top_layer.rgb +
            near_layer.rgb * (1.0 - top_layer.a);
        const float contour_alpha = top_layer.a +
            near_layer.a * (1.0 - top_layer.a);
        rgb = contour_rgb + rgb * (1.0 - contour_alpha);
        alpha = contour_alpha + alpha * (1.0 - contour_alpha);
    }

    if (alpha <= 0.0)
    {
        discard_fragment();
    }
    return float4(rgb / max(alpha, 0.000001), alpha);
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
    // Forward GL clip depth is [-w,+w]. The viewer's reverse-Z projection
    // already emits Metal's [0,+w] range, matching every native world owner.
    output.position = remap.reverse_z != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = (matrices.texture0 * float4(texcoords[source_id], 0.0, 1.0)).xy;
    output.color = float4(colors[source_id]) * (1.0 / 255.0);
    return output;
}

struct ALMetalHUDFullbrightConstants
{
    float4x4 modelview_projection;
    float4x4 texture_matrix;
    // x = alpha cutoff.
    float4 material;
    // x = reverse Z, y = texture count, z = offscreen Y flip,
    // w = coverage permutation: 0 opaque, 1 texture-only mask,
    // 2 blended alpha-pool final-alpha mask.
    uint4 config;
};

struct ALMetalHUDFullbrightVaryings
{
    float4 position [[position]];
    float2 texcoord;
    float4 color;
    uint texture_index [[flat]];
    uint texture_count [[flat]];
};

vertex ALMetalHUDFullbrightVaryings almetal_hud_fullbright_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    const device uchar4* colors [[buffer(2)]],
    constant ALMetalHUDFullbrightConstants& constants [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    float4 clip_position = constants.modelview_projection *
        float4(positions[vertex_id].xyz, 1.0);
    if (constants.config.z != 0u)
    {
        clip_position.y = -clip_position.y;
    }

    ALMetalHUDFullbrightVaryings output;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    // IS_HUD deliberately keeps both the sampled texel and streamed prim
    // tint display-encoded, matching deferred/fullbrightV/F.glsl.
    output.color = float4(colors[vertex_id]) * (1.0 / 255.0);
    output.texture_index = constants.config.y > 1u
        ? uint(max(as_type<int>(positions[vertex_id].w), 0)) : 0u;
    output.texture_count = constants.config.y;
    return output;
}

fragment float4 almetal_hud_fullbright_fragment(
    ALMetalHUDFullbrightVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]],
    constant ALMetalHUDFullbrightConstants& constants [[buffer(0)]])
{
    float4 texture_color = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord)
        : float4(1.0, 0.0, 1.0, 1.0);
    const float final_alpha = texture_color.a * input.color.a;
    // Outside the alpha pool, streamed alpha carries shininess. The opaque
    // fullbright-mask pool therefore tests texture coverage alone with the
    // same half-quantum bias as the authoritative GLSL program. In the alpha
    // pool it is coverage, so match fullbrightF's unbiased product test.
    if (constants.config.w == 1u &&
        texture_color.a < constants.material.x - (1.0 / 512.0))
    {
        discard_fragment();
    }
    if (constants.config.w == 2u &&
        final_alpha < constants.material.x)
    {
        discard_fragment();
    }
    return max(float4(texture_color.rgb * input.color.rgb, final_alpha),
               float4(0.0));
}

struct ALMetalHUDHighlightConstants
{
    float4x4 modelview_projection;
    float4x4 texture_matrix;
    float4 material;
    uint4 config;
    float4 base_color_transform[2];
    float4 emissive_transform[2];
    float4 emissive_color;
    float4 highlight_color;
};

vertex ALMetalHUDFullbrightVaryings almetal_hud_highlight_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalHUDHighlightConstants& constants [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    float4 clip_position = constants.modelview_projection *
        float4(positions[vertex_id].xyz, 1.0);
    if (constants.config.z != 0u)
    {
        clip_position.y = -clip_position.y;
    }

    ALMetalHUDFullbrightVaryings output;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.color = constants.highlight_color;
    output.texture_index = 0u;
    output.texture_count = 1u;
    return output;
}

vertex ALMetalHUDFullbrightVaryings almetal_hud_highlight_skinned_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    constant ALMetalHUDHighlightConstants& constants [[buffer(3)]],
    const device float4* skin_palette [[buffer(4)]],
    const device float4* encoded_weights [[buffer(5)]],
    constant uint& joint_count [[buffer(6)]],
    uint vertex_id [[vertex_id]])
{
    const ALMetalObjectSkinnedVertex skinned = almetal_object_skin_vertex(
        positions[vertex_id], float4(0.0), encoded_weights[vertex_id],
        skin_palette, joint_count);
    const float4 rebased_position =
        skinned.position + float4(skin_palette[0].xyz, 1.0);
    float4 clip_position =
        constants.modelview_projection * rebased_position;
    if (constants.config.z != 0u)
    {
        clip_position.y = -clip_position.y;
    }

    ALMetalHUDFullbrightVaryings output;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = (constants.texture_matrix *
                       float4(texcoords[vertex_id], 0.0, 1.0)).xy;
    output.color = constants.highlight_color;
    output.texture_index = 0u;
    output.texture_count = 1u;
    return output;
}

fragment float4 almetal_hud_highlight_fragment(
    ALMetalHUDFullbrightVaryings input [[stage_in]],
    texture2d<float> diffuse_texture [[texture(0)]],
    sampler diffuse_sampler [[sampler(0)]])
{
    return max(input.color *
               diffuse_texture.sample(diffuse_sampler, input.texcoord),
               float4(0.0));
}

struct ALMetalHUDPBRConstants
{
    float4x4 modelview_projection;
    float4x4 texture_matrix;
    // x = alpha cutoff.
    float4 material;
    // x = reverse Z, y = texture count, z = offscreen Y flip.
    uint4 config;
    float4 base_color_transform[2];
    float4 emissive_transform[2];
    float4 emissive_color;
};

struct ALMetalHUDPBRVaryings
{
    float4 position [[position]];
    float2 base_color_texcoord;
    float2 emissive_texcoord;
    float4 color;
};

static float2 almetal_hud_pbr_texcoord(
    float2 vertex_texcoord,
    constant float4* material_transform,
    constant ALMetalHUDPBRConstants& constants)
{
    // Match deferred/textureUtilV.glsl::texture_transform: apply the live SL
    // texture animation in right-handed UV space, then evaluate the KHR
    // transform in its left-handed convention and convert back.
    float2 texcoord = (constants.texture_matrix *
                       float4(vertex_texcoord, 0.0, 1.0)).xy;
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

vertex ALMetalHUDPBRVaryings almetal_hud_pbr_alpha_vertex(
    const device float4* positions [[buffer(0)]],
    const device float2* texcoords [[buffer(1)]],
    const device uchar4* colors [[buffer(2)]],
    constant ALMetalHUDPBRConstants& constants [[buffer(3)]],
    uint vertex_id [[vertex_id]])
{
    float4 clip_position = constants.modelview_projection *
        float4(positions[vertex_id].xyz, 1.0);
    if (constants.config.z != 0u)
    {
        clip_position.y = -clip_position.y;
    }

    ALMetalHUDPBRVaryings output;
    output.position = constants.config.x != 0u
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.base_color_texcoord = almetal_hud_pbr_texcoord(
        texcoords[vertex_id], constants.base_color_transform, constants);
    output.emissive_texcoord = almetal_hud_pbr_texcoord(
        texcoords[vertex_id], constants.emissive_transform, constants);
    const float4 encoded_color = float4(colors[vertex_id]) * (1.0 / 255.0);
    output.color = float4(
        almetal_srgb_to_linear(encoded_color.rgb), encoded_color.a);
    return output;
}

fragment float4 almetal_hud_pbr_alpha_fragment(
    ALMetalHUDPBRVaryings input [[stage_in]],
    texture2d<float> base_color_texture [[texture(0)]],
    texture2d<float> emissive_texture [[texture(1)]],
    sampler color_sampler [[sampler(0)]],
    constant ALMetalHUDPBRConstants& constants [[buffer(0)]])
{
    const float4 base_color = base_color_texture.sample(
        color_sampler, input.base_color_texcoord);
    // Read emissive before the coverage discard, matching pbralphaF's
    // derivative-safe ordering even though this HUD permutation has no LOD
    // derivatives after the discard.
    const float3 emissive = emissive_texture.sample(
        color_sampler, input.emissive_texcoord).rgb *
        constants.emissive_color.rgb;
    const float coverage = base_color.a * input.color.a;
    if (coverage < constants.material.x)
    {
        discard_fragment();
    }
    const float3 linear = input.color.rgb * base_color.rgb + emissive;
    return max(float4(almetal_linear_to_srgb(linear), coverage),
               float4(0.0));
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

fragment float4 almetal_ui_vertex_color_fragment(
    ALMetalUIVaryings input [[stage_in]])
{
    // ALTextureSlot::unbind leaves GL's opaque-white placeholder bound. The
    // exact uiF.glsl result is consequently the streamed vertex colour.
    return input.color;
}

vertex ALMetalUIVaryings almetal_uniform_color_vertex(
    const device float4* positions [[buffer(0)]],
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
    output.position = remap.reverse_z != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
    output.texcoord = 0.0;
    output.color = 1.0;
    return output;
}

fragment float4 almetal_uniform_color_fragment(
    constant ALMetalSolidFragmentConstants& constants [[buffer(0)]])
{
    // interface/debugF.glsl ignores the streamed vertex colour and emits the
    // current diffuse-color uniform. The position-only debug vertex contract
    // is otherwise identical to the established immediate-mode transform.
    return max(constants.color, float4(0.0));
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
    output.position = remap.reverse_z != 0
        ? clip_position : almetal_clip_from_gl(clip_position);
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
