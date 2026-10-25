/**
 * @file almetalworld.h
 * @brief Native Metal world G-buffer bootstrap pass.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#pragma once

#include "almetalpipeline.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct ALMetalWorldMaterialSlot
{
    const ALMetalTexture* normal_texture = nullptr;
    const ALMetalTexture* specular_texture = nullptr;
    float specular_color[4] = {1.f, 1.f, 1.f, 0.5f};
    float env_intensity = 0.f;
    float alpha_cutoff = 0.5f;
    float fullbright = 0.f;
};

struct ALMetalWorldPBRSlot
{
    const ALMetalTexture* normal_texture = nullptr;
    const ALMetalTexture* orm_texture = nullptr;
    const ALMetalTexture* emissive_texture = nullptr;
    float base_color_factor[4] = {1.f, 1.f, 1.f, 1.f};
    float texture_transform[4][8] = {
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    };
    float emissive_color[4]{};
    float roughness_factor = 1.f;
    float metallic_factor = 1.f;
    float alpha_cutoff = -1.f;
};

// The live GL reflection-probe pack mirrored into Metal. Radiance is updated
// one completed probe at a time; the manager exposes this only when every
// cube slot referenced by these constants belongs to the same probe
// generation in the Metal array. The fixed layout intentionally follows the
// selection data used by reflectionProbeF.glsl so native forward owners can
// preserve its exact traversal and weighting.
struct alignas(16) ALMetalWorldReflectionProbeConstants
{
    static constexpr std::size_t max_probe_count = 256;

    float ref_box[max_probe_count][16]{};
    float ref_sphere[max_probe_count][4]{};
    float ref_params[max_probe_count][4]{};
    std::int32_t ref_index[max_probe_count][4]{};
    std::int32_t ref_neighbor[4096]{};
    std::int32_t ref_bucket[256][4]{};
    float env_matrix[3][4] = {
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
    };
    // x = active probe count, y = maximum radiance LOD.
    float config[4]{};
};

struct ALMetalWorldReflectionProbes
{
    const ALMetalTexture* radiance_texture = nullptr;
    // Four RGBA32UI texels per cube slot pack the nine RGB half-float SH
    // coefficients used by the PBR diffuse-irradiance path. This remains
    // optional for legacy-only consumers, which sample radiance alone.
    const ALMetalTexture* irradiance_texture = nullptr;
    ALMetalWorldReflectionProbeConstants constants;
};

// The active mirror/hero probe overrides ordinary reflection-probe radiance
// after selection. The base-light owner consumes the glossiest quarter of its
// authored mip chain; legacy impostor owners finish at glossiness one and
// therefore consume the sharpest level. Keeping this separate from the fixed
// probe-selection block avoids changing its 49,216-byte ABI while the GL
// owner remains the transitional producer.
struct alignas(16) ALMetalWorldHeroProbeConstants
{
    float hero_box[16]{};
    float hero_sphere[4]{};
    float clip_plane[4]{};
    // x = shape (0 box, 1 sphere), y = active probe count (0 or 1),
    // z = the LOD that radiance generation maps to perceptual roughness one.
    std::uint32_t config[4]{};
};

struct ALMetalWorldHeroProbe
{
    const ALMetalTexture* radiance_texture = nullptr;
    ALMetalWorldHeroProbeConstants constants;
};

// Previous-frame scene history plus the matrices and tuning values used by
// legacyProbeFinish's transparent SSR branch. The source remains the GL
// scene/depth copy during migration; both textures are mirrored as one
// generation before an impostor replay may consume them.
struct alignas(16) ALMetalWorldScreenSpaceReflectionConstants
{
    float projection[16]{};
    float inverse_projection[16]{};
    float inverse_modelview_delta[16]{};
    // xy = GL screen_res, z = Metal destination height for gl_FragCoord.y.
    float screen[4]{};
    // x = iteration count, y = ray step, z = distance bias,
    // w = depth-reject bias.
    float trace[4]{};
    // x = glossy sample count, y = adaptive-step multiplier.
    float sampling[4]{};
    // x = reverse-Z, y = active scene-history generation.
    std::uint32_t config[4]{};
};

struct ALMetalWorldScreenSpaceReflections
{
    const ALMetalTexture* scene_color = nullptr;
    const ALMetalTexture* scene_depth = nullptr;
    ALMetalWorldScreenSpaceReflectionConstants constants;
};

struct alignas(16) ALMetalWorldLegacyAlphaLight
{
    float position[4]{};
    float direction[4]{};
    float diffuse[4]{};
    // x = radius/size, y = falloff, z = point-light flag,
    // w = projector ambiance (retained for the shared PBR light ABI).
    float parameters[4]{};
};

// The forward-light subset used by a legacy BLEND material. The main-world
// owner consumes its RGB and glare terms; the impostor bake deliberately uses
// only glare because the billboard is lit again when it is consumed.
struct alignas(16) ALMetalWorldLegacyAlphaLighting
{
    float sun_direction[4]{};
    float sun_color[4]{};
    std::array<ALMetalWorldLegacyAlphaLight, 7> local_lights{};
    // x = Blinn-Phong exponent, y = coverage cutoff,
    // z = directional-light visibility, w = classic-mode flag.
    float parameters[4] = {128.f, 0.004f, 1.f, 0.f};
};

// Raw directional-shadow depth plus the exact Deferred-block subset used by
// shadowUtil.glsl. The four GL cascades are uploaded as R32F array slices;
// the Metal shader performs the same forward/reverse comparison, RPDB and
// quality-tier filter in shader so D24 and D32F sources share one contract.
struct alignas(16) ALMetalWorldDirectionalShadowConstants
{
    float shadow_matrix[4][16]{};
    float shadow_clip[4]{};
    // xy = shadow-map resolution, zw = GL screen_res used by PCSS rotation.
    float resolution[4]{};
    // x = signed constant bias, y = normal offset, z = PCSS scale,
    // w = destination height for Metal-to-GL fragment-coordinate inversion.
    float parameters[4]{};
    // x = cascade count, y = unbuilt-map mask, z = reverse-Z,
    // w = filter quality (0/1/2 Gaussian 2/4/6, 3 PCSS).
    std::uint32_t config[4]{};
};

struct ALMetalWorldDirectionalShadows
{
    const ALMetalTexture* depth_texture = nullptr;
    ALMetalWorldDirectionalShadowConstants constants;
};

enum class ALMetalWorldSpotShadowCoverage : std::uint32_t
{
    opaque = 0,
    legacy_mask = 1,
    legacy_fullbright_mask = 2,
    legacy_blend = 3,
    texture_mask = 4,
    pbr_mask = 5,
    pbr_blend = 6,
    avatar_mask = 7,
    avatar_blend = 8,
};

// One exact draw seen by a spotlight's own shadow camera. Object casters are
// indexed triangles. Opaque ones need only position; cutout/stippled ones
// retain diffuse UV, streamed alpha, texture ladders and exact live-shader
// cutoffs; rigged ones also retain the packed object-skin palette and weight
// plane. Terrain instead retains one four-control-point tessellated patch,
// its height surface and the six factors computed by the live shadow pass.
struct ALMetalWorldSpotShadowBatch
{
    static constexpr std::size_t max_textures = 32;
    static constexpr std::size_t max_pbr_slots = 8;

    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t normal_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t color_offset = 0;
    std::size_t weight_offset = 0;
    std::size_t cloth_offset = 0;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    const float* skin_palette = nullptr;
    std::size_t skin_palette_float_count = 0;
    std::uint32_t skin_joint_count = 0;
    float modelview_projection[16]{};
    float texture_matrix[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };
    float avatar_wind[4]{};
    float avatar_sin_wave[4]{};
    float avatar_gravity[4]{};
    float avatar_cloth_depth[4]{};
    float avatar_draw_alpha = 1.f;
    std::array<const ALMetalTexture*, max_textures> textures{};
    std::array<float, max_textures> alpha_cutoffs{};
    std::array<std::array<std::array<float, 4>, 2>, max_pbr_slots>
        pbr_base_color_transforms{};
    const ALMetalTexture* terrain_height_texture = nullptr;
    std::uint16_t terrain_tessellation_factors[6]{};
    float terrain_grid_scale = 0.f;
    std::uint32_t texture_count = 0;
    ALMetalWorldSpotShadowCoverage coverage =
        ALMetalWorldSpotShadowCoverage::opaque;
    bool double_sided = false;
    bool terrain = false;
    bool avatar = false;
    bool avatar_cloth = false;
    bool terrain_smoothing = false;
    bool terrain_tessellation_pinned = false;
};

struct ALMetalWorldSpotShadowScene
{
    const ALMetalWorldSpotShadowBatch* batches = nullptr;
    std::size_t batch_count = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool reverse_z = false;
};

struct ALMetalWorldSimpleBatch
{
    static constexpr std::size_t max_diffuse_textures = 32;
    static constexpr std::size_t max_material_slots = 8;

    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t normal_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t texcoord1_offset = 0;
    std::size_t texcoord2_offset = 0;
    std::size_t color_offset = 0;
    std::size_t emissive_offset = 0;
    std::size_t tangent_offset = 0;
    std::size_t weight_offset = 0;
    std::size_t cloth_offset = 0;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    std::array<const ALMetalTexture*, max_diffuse_textures> diffuse_textures{};
    std::uint32_t texture_count = 0;
    const ALMetalTexture* normal_texture = nullptr;
    std::array<ALMetalWorldMaterialSlot, max_material_slots> material_slots{};
    std::uint32_t material_slot_count = 0;
    std::uint32_t material_alpha_mode = 0;
    std::array<ALMetalWorldPBRSlot, max_material_slots> pbr_slots{};
    std::uint32_t pbr_slot_count = 0;
    const float* skin_palette = nullptr;
    std::size_t skin_palette_float_count = 0;
    std::uint32_t skin_joint_count = 0;
    bool alpha_mask = false;
    bool double_sided = false;
    bool avatar_rigid = false;
    bool avatar_skinning = false;
    bool avatar_cloth = false;
    bool impostor = false;
    bool impostor_fullbright = false;
    bool impostor_shiny = false;
    bool impostor_emboss = false;
    bool water_exclusion = false;
    bool water_haze = false;
    bool water_surface = false;
    bool atmospheric_haze = false;
    bool pre_water_alpha = false;
    bool post_water_alpha = false;
    bool post_water_depth_only = false;
    bool post_water_glow = false;
    bool pre_water_fullbright = false;
    bool pre_water_simple = false;
    bool pre_water_material = false;
    bool pre_water_pbr = false;
    bool pre_water_shadows = false;
    bool pre_water_particle = false;
    bool pre_water_alpha_writes_coverage = false;
    float pre_water_minimum_alpha = 0.004f;
    ALMetalBlendFactor pre_water_source_rgb =
        ALMetalBlendFactor::source_alpha;
    ALMetalBlendFactor pre_water_destination_rgb =
        ALMetalBlendFactor::one_minus_source_alpha;
    bool legacy_glow = false;
    bool pbr_glow = false;
    bool terrain = false;
    bool use_vertex_color = true;
    bool alpha_cutoff_bias = true;
    float alpha_cutoff = 0.f;
    float modelview_projection[16]{};
    float modelview[16]{};
    float normal_matrix[3][4]{};
    float rigged_normal_matrix[3][4]{};
    float texture_matrix[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };
    float avatar_wind[4]{};
    float avatar_sin_wave[4]{};
    float avatar_gravity[4]{};
    float avatar_cloth_depth[4]{};
    std::array<const ALMetalTexture*, 4> impostor_textures{};
    float impostor_center[4]{};
    float impostor_left[4]{};
    float impostor_up[4]{};
    float impostor_normal_rotation[3][4]{};
    const ALMetalTexture* terrain_height_texture = nullptr;
    const ALMetalTexture* terrain_composition_texture = nullptr;
    const ALMetalTexture* terrain_alpha_ramp_texture = nullptr;
    const ALMetalTexture* terrain_paint_texture = nullptr;
    const ALMetalTexture* terrain_parcel_overlay_texture = nullptr;
    std::uint16_t terrain_tessellation_factors[6]{};
    float terrain_modelview[16]{};
    float terrain_grid_scale = 1.f;
    float terrain_region_scale = 256.f;
    float terrain_detail_scale = 1.f / 12.f;
    float terrain_detail_phase[2]{};
    float terrain_triplanar_blend = 8.f;
    float terrain_uv_transform[4][4]{};
    float terrain_uv_offset[4][2]{};
    float terrain_normal_axes[4][4]{};
    float terrain_hex_fraction[8][4]{};
    std::int32_t terrain_hex_base[8][4]{};
    std::int32_t terrain_pbr_detail = -4;
    std::uint32_t terrain_paint_type = 0;
    bool terrain_smoothing = false;
    bool terrain_triplanar = true;
    bool terrain_hex = false;
    bool terrain_pbr = false;
    bool terrain_flat_normals = false;
    bool terrain_tessellation_pinned = false;
};

struct ALMetalWorldGBufferScene
{
    const ALMetalWorldSimpleBatch* batches = nullptr;
    std::size_t batch_count = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool normal_hdr = false;
    bool hdr = false;
    bool reverse_z = false;
    bool stamp_opaque_coverage = false;
    const ALMetalWorldReflectionProbes* reflection_probes = nullptr;
    const ALMetalWorldHeroProbe* hero_probe = nullptr;
    const ALMetalWorldScreenSpaceReflections* screen_space_reflections = nullptr;
    const ALMetalWorldLegacyAlphaLighting* legacy_alpha_lighting = nullptr;
    const ALMetalWorldDirectionalShadows* directional_shadows = nullptr;
    float clear_color[4] = {1.f, 0.f, 1.f, 1.f};
};

struct alignas(16) ALMetalWorldLocalLight
{
    // Eye-space centre in xyz, GL deferred radius in w.
    float position_radius[4]{};
    // Linear RGB after global/fade scaling, deferred falloff in w.
    float color_falloff[4]{};
};

struct ALMetalWorldLocalLightingScene
{
    const ALMetalRenderTarget* gbuffer = nullptr;
    const ALMetalWorldLocalLight* lights = nullptr;
    std::size_t light_count = 0;
    float inverse_projection[16]{};
    float specular_exponent = 128.f;
    bool reverse_z = false;
    bool hdr = false;
    bool classic_mode = false;
};

struct alignas(16) ALMetalWorldProjectorLight
{
    const ALMetalTexture* cookie_texture = nullptr;
    // Eye-space centre in xyz, GL deferred radius in w.
    float position_radius[4]{};
    // Linear RGB after global/fade scaling, deferred falloff in w.
    float color_falloff[4]{};
    // Eye space to the projector's forward texture space.
    float projector_matrix[16]{};
    // xyz = emitting plane point, w = projector ambiance.
    float projector_plane[4]{};
    // xyz = emitting plane normal, w = far-minus-near range.
    float projector_normal[4]{};
    // xyz = projector origin, w = focus distance.
    float projector_origin[4]{};
    // x = cookie LOD range, y = shadow fade.
    float sampling[4]{};
    // -1 = unshadowed, otherwise the raw spot-shadow array slice.
    std::int32_t shadow_channel = -1;
};

struct ALMetalWorldProjectorLightingScene
{
    const ALMetalRenderTarget* gbuffer = nullptr;
    const ALMetalWorldProjectorLight* lights = nullptr;
    std::size_t light_count = 0;
    // Two raw spot-shadow depth slices plus the exact matrix/filter block.
    // The raw maps remain a migration input while Metal owns filtering.
    // Null is valid only when every light is unshadowed.
    const ALMetalWorldDirectionalShadows* projector_shadows = nullptr;
    float inverse_projection[16]{};
    float specular_exponent = 128.f;
    bool reverse_z = false;
    bool hdr = false;
    bool normal_hdr = false;
    bool classic_mode = false;
};

struct ALMetalWorldLightingCompositionScene
{
    // These are the exact three owners accumulated into GL's screen target:
    // soften/base first, then ordinary and projector lights additively.
    const ALMetalRenderTarget* base_lighting = nullptr;
    const ALMetalRenderTarget* local_lighting = nullptr;
    const ALMetalRenderTarget* projector_lighting = nullptr;
    bool hdr = false;
};

// The water-exclusion fragment needs the same live height field and
// eye-to-agent transform as waterExclusionF.glsl. The cell map is the
// persistent Metal mirror of ALWaterSurface's RG32F height/flag texture.
struct ALMetalWorldWaterExclusionSurface
{
    const ALMetalTexture* cell_map = nullptr;
    float eye_to_agent[16]{};
    // xy = cell square origin, z = cell width, w = cells per side.
    float cell_square[4]{};
    // xyz = camera position in agent space, w = default water height.
    float camera_default_height[4]{};
};

struct ALMetalWorldWaterFieldDraw
{
    std::uint32_t part = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t instance_count = 0;
    bool triangle_strip = false;
};

// The exact procedural water field and fog inputs consumed by
// waterHazeV/F.glsl. Above water, the field is depth-tested and masked by the
// retained exclusion stencil. Underwater, only the matrices, plane and fog
// terms are used by a full-screen triangle.
struct ALMetalWorldWaterHaze
{
    const ALMetalTexture* cell_map = nullptr;
    float modelview_projection[16]{};
    float inverse_projection[16]{};
    // xy = cell square origin, z = cell width, w = cells per side.
    float cell_square[4]{};
    // x = default height; the remaining elements are reserved.
    float default_height[4]{};
    // x = square half-size, y = ring count, z = rim, w = draw void.
    float rings[4]{};
    float water_plane[4]{};
    // RGB is the same display-encoded waterFogColor converted by the shader.
    float fog_color[4]{};
    // x = waterFogKS, y = waterFogDensity.
    float fog[4]{};
    std::array<ALMetalWorldWaterFieldDraw, 4> draws{};
    std::uint32_t draw_count = 0;
    bool above_water = false;
};

// The water-surface subset consumes the same exact procedural field as the
// haze pass plus the live normal-map phase, optical and lighting inputs.
// The renderer snapshots its retained color (and, above water, depth)
// immediately before this marker so transparent water never samples an
// attachment it is writing. With waves, the field omits the patch rectangle
// and adds its fringe; one packed half-factor record drives each native quad
// patch before the remaining field is drawn.
struct ALMetalWorldWaterSurface
{
    ALMetalWorldWaterHaze field;
    std::array<const ALMetalTexture*, 2> normal_maps{};
    std::array<const ALMetalTexture*, 2> height_maps{};
    float modelview[16]{};
    // Three vec4-strided columns, matching the live GLSL normal matrix.
    float normal_matrix[3][4]{};
    // xyz = camera in agent space.
    float eye_position[4]{};
    float big_phase[4]{};
    float little_phase[4]{};
    float sweep_phase[4]{};
    // x = wave-coordinate clamp, y = infinite-projection edge fade.
    float water_far[4]{};
    float map_mean[4]{};
    float lattice_fraction[4]{};
    std::int32_t lattice_base[4]{};
    // xyz = authored layer normal scales, w = glint roughness.
    float normal_scale_glint[4]{};
    // x = Fresnel scale, y = Fresnel offset,
    // z = reflection/refraction perceptual roughness.
    float optics[4]{};
    // xy = field rim low, zw = field rim high.
    float rim[4]{};
    // xyz = exact agent-space light direction used by waterF.glsl.
    float light_direction[4]{};
    // x = normal-map blend, y = refraction scale, z = exclusion distortion,
    // w = water-hextile mode (0..2).
    float parameters[4]{};
    // xyz = the main camera used to lay out the waves, w = tessellation
    // density. x = reach, y = maximum displacement, zw = map amplitudes.
    float wave_eye_density[4]{};
    float wave_reach_amplitude[4]{};
    std::int32_t patch_rect[4]{};
    std::int32_t patch_order[4]{};
    std::uint32_t patch_count = 0;
    std::vector<std::uint16_t> tessellation_factors;
    bool waves = false;
    // Cull wave patches against a Metal pyramid built from the exact retained
    // pre-water depth. This is independent of the transitional GL pyramid.
    bool wave_occlusion = false;
    bool underwater = true;
    bool refracts = false;
    bool has_excluded_surface = false;
    bool uses_shadows = false;
};

// The shared environment subset used by post-deferred transparent geometry.
// These values are copied from the same live Environment block and loose sky
// uniforms as the GLSL alpha programs; no settings are reinterpreted by the
// renderer. The first native family is fogged fullbright pre-water alpha, and
// the contract is deliberately broad enough for the lit alpha families that
// follow it.
struct alignas(16) ALMetalWorldAlphaEnvironment
{
    float water_plane[4]{};
    // Three vec4-strided columns, matching EnvironmentUBOData::eyeToSky.
    float eye_to_sky[3][4]{};
    float haze_extinction[4]{};
    // xyz = glow, w = maximum haze-layer height.
    float glow_max_y[4]{};
    float haze_additive[4]{};
    float haze_additive_glow[4]{};
    // RGB is already linear, copied from waterFogColorLinear.
    float water_fog_color[4]{};
    // xyz = active sun/moon direction in eye space.
    float light_direction[4]{};
    // Packed Environment-block lighting terms. The .w components of the
    // first two carry sky_sunlight_scale and sky_ambient_scale.
    float atmos_sunlit[4]{};
    float atmos_sunlit_linear[4]{};
    float atmos_amblit[4]{};
    // x = density multiplier, y = distance multiplier,
    // z = sun/moon glow factor, w = deferred sky HDR scale.
    float atmospheric[4]{};
    // x = waterFogKS, y = waterFogDensity,
    // z = waterSign for this alpha pool. The renderer injects the current
    // draw's minimum-alpha threshold into w before binding this block.
    float water_fog[4]{};
};

// The full-screen deferred atmospheric pass that follows water above the
// surface and precedes it below the surface. The Environment payload remains
// shared with transparent geometry so the same live atmospheric equations are
// used by both owners.
struct alignas(16) ALMetalWorldAtmosphericHaze
{
    float inverse_projection[16]{};
    float water_ground_fade = 0.f;
    bool above_water = true;
    bool sky_behind = false;
    bool alpha_writes_coverage = false;
};

// The first ordered post-deferred world slice. The composed lighting color
// and retained G-buffer depth/stencil attachment are copied into one
// final-color target. Water-exclusion batches first rebuild EXCLUDE_BIT,
// followed by opaque-forward fullbright, shiny and emboss replay, both
// ordered glow families, water haze, and the exact supported pre-water alpha
// families, class-3 water including its tessellated wave patches, and the
// exact deferred atmospheric haze on the appropriate side of that surface.
// Post-water alpha and presentation remain later owners.
struct ALMetalWorldOpaquePostDeferredScene
{
    const ALMetalRenderTarget* gbuffer = nullptr;
    const ALMetalRenderTarget* composed_lighting = nullptr;
    const ALMetalWorldSimpleBatch* batches = nullptr;
    std::size_t batch_count = 0;
    const ALMetalWorldReflectionProbes* reflection_probes = nullptr;
    const ALMetalWorldHeroProbe* hero_probe = nullptr;
    const ALMetalWorldScreenSpaceReflections* screen_space_reflections =
        nullptr;
    const ALMetalWorldWaterExclusionSurface* water_exclusion_surface =
        nullptr;
    const ALMetalWorldWaterHaze* water_haze = nullptr;
    const ALMetalWorldWaterSurface* water_surface = nullptr;
    const ALMetalWorldAtmosphericHaze* atmospheric_haze = nullptr;
    const ALMetalTexture* sky_behind = nullptr;
    const ALMetalWorldAlphaEnvironment* alpha_environment = nullptr;
    const ALMetalWorldLegacyAlphaLighting* alpha_lighting = nullptr;
    const ALMetalWorldDirectionalShadows* alpha_directional_shadows =
        nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool reverse_z = false;
    bool hdr = false;
};

struct alignas(16) ALMetalWorldLightMapConstants
{
    float inverse_projection[16]{};
    // xyz = active sun/moon direction in eye space.
    float light_direction[4]{};
    // x = radius, y = maximum radius, z = factor, w = inverse factor.
    float ssao[4]{};
    // xyz = the four (shadow weight, AO weight, offset) kernel taps.
    float blur_taps[4][4]{};
    // x = kernel scale, y = distance factor.
    float blur[4]{};
    // xy = destination dimensions.
    float screen[4]{};
    // x = reverse Z, y = directional shadows, z = SSAO,
    // w = high-precision geometric-normal packing.
    std::uint32_t config[4]{};
};

struct ALMetalWorldLightMapScene
{
    const ALMetalRenderTarget* gbuffer = nullptr;
    const ALMetalWorldDirectionalShadows* directional_shadows = nullptr;
    const ALMetalTexture* noise_texture = nullptr;
    ALMetalWorldLightMapConstants constants;
};

// One retained strip of the WindLight sky dome. The vertex/index buffers are
// the same immutable mirrors used by the authoritative GL draw; only the
// model-view-projection transform is snapshotted per strip. Both the HDRI and
// atmospheric WindLight owners consume this geometry contract.
struct ALMetalWorldHDRISkyBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    float modelview_projection[16]{};
};

// The exact full-screen HDRI branch of skyF.glsl. Split-screen HDRI is not a
// valid instance of this scene because its other half is the WindLight haze
// family; callers must retain GL until that producer is native too.
struct ALMetalWorldHDRISkyScene
{
    const ALMetalWorldHDRISkyBatch* batches = nullptr;
    std::size_t batch_count = 0;
    const ALMetalTexture* environment_texture = nullptr;
    float camera_local[4]{};
    // Three vec4-strided columns matching GLSL's env_mat multiply.
    float environment_rotation[3][4] = {
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
    };
    float exposure_scale = 1.f;
    // Fraction of the target's left edge written by the HDRI branch. One is
    // the ordinary full panorama; intermediate values reproduce the GLSL
    // split-screen pixel-center test after WindLight fills the target.
    float horizontal_fraction = 1.f;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool normal_hdr = false;
    bool hdr = false;
    bool reverse_z = false;
};

// The CPU-packed subset of EnvironmentUBOData and loose sky uniforms needed
// by skyF.glsl's non-HDRI atmospheric branch. Every vec3 is padded to a
// float4 so the structure can be copied directly into the Metal constant
// buffer without inventing a second packing convention.
struct alignas(16) ALMetalWorldWindLightSkyConstants
{
    float camera_local[4]{};
    // xyz = clamped sky-frame light direction, w = density multiplier.
    float light_density[4]{};
    // xyz = blue horizon, w = scalar haze horizon.
    float blue_horizon[4]{};
    // xyz = glow, w = maximum atmospheric layer height.
    float glow[4]{};
    float haze_extinction[4]{};
    float blue_weight[4]{};
    float haze_weight[4]{};
    float sky_light[4]{};
    float sky_ambient[4]{};
    float sky_light_atten[4]{};
    // xyz = ambient below cloud, w = cloud shadow.
    float sky_ambient_below[4]{};
    // x = moisture, y = droplet radius, z = ice,
    // w = sun/moon glow factor.
    float optics[4]{};
};

struct ALMetalWorldWindLightSkyScene
{
    const ALMetalWorldHDRISkyBatch* batches = nullptr;
    std::size_t batch_count = 0;
    const ALMetalTexture* rainbow_texture = nullptr;
    const ALMetalTexture* halo_texture = nullptr;
    ALMetalWorldWindLightSkyConstants constants;
    float sky_hdr_scale = 1.f;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool normal_hdr = false;
    bool hdr = false;
    bool reverse_z = false;
};

// The retained non-indexed star field drawn between WindLight atmospheric
// haze and the heavenly bodies. The source buffer uses LLVertexBuffer's
// independent attribute planes: float4 positions, packed RGBA8 colors and
// float2 billboard corners.
struct ALMetalWorldStarBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t color_offset = 0;
    std::size_t texcoord_offset = 0;
    std::uint32_t vertex_count = 0;
    float modelview_projection[16]{};
};

struct ALMetalWorldStarScene
{
    const ALMetalWorldStarBatch* batches = nullptr;
    std::size_t batch_count = 0;
    // Required only when moon_mask is true. An internal transparent fallback
    // is bound otherwise so the shader keeps one exact resource ABI.
    const ALMetalTexture* moon_texture = nullptr;
    float star_alpha = 0.f;
    float time = 0.f;
    // xyz terms are already rotated into the slowly turning star frame.
    float moon_direction[4]{};
    float moon_quad_center[4]{};
    float moon_quad_u[4]{};
    float moon_quad_v[4]{};
    float moon_brightness = 0.f;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool moon_mask = false;
    bool normal_hdr = false;
    bool hdr = false;
    bool reverse_z = false;
};

// One of the indexed far-plane quads drawn after the WindLight stars. Sun
// quads interpolate two environment textures; moon quads use one texture plus
// the legacy horizon fade and brightness response.
struct ALMetalWorldHeavenlyBodyBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t texcoord_offset = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    const ALMetalTexture* texture_a = nullptr;
    const ALMetalTexture* texture_b = nullptr;
    float modelview_projection[16]{};
    // x = sun texture blend or moon brightness, y = moon direction z.
    float parameters[4]{};
    bool moon = false;
};

struct ALMetalWorldHeavenlyBodyScene
{
    const ALMetalWorldHeavenlyBodyBatch* batches = nullptr;
    std::size_t batch_count = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool normal_hdr = false;
    bool hdr = false;
    bool reverse_z = false;
};

// The retained non-indexed meteor field drawn after the heavenly bodies.
// The source buffer uses independent float4 position/normal, packed RGBA8
// color and float2 streak-coordinate planes.
struct ALMetalWorldMeteorBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t normal_offset = 0;
    std::size_t color_offset = 0;
    std::size_t texcoord_offset = 0;
    std::uint32_t vertex_count = 0;
    float modelview_projection[16]{};
};

struct ALMetalWorldMeteorScene
{
    const ALMetalWorldMeteorBatch* batches = nullptr;
    std::size_t batch_count = 0;
    float width_pixels = 1.5f;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool normal_hdr = false;
    bool hdr = false;
    bool reverse_z = false;
};

struct ALMetalWorldAuroraScene
{
    const ALMetalWorldHDRISkyBatch* batches = nullptr;
    std::size_t batch_count = 0;
    float camera_local[4]{};
    float intensity = 0.f;
    float time = 0.f;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool normal_hdr = false;
    bool hdr = false;
    bool reverse_z = false;
};

// The indexed WindLight dome replayed for clouds. The source vertex buffer
// keeps float4 positions and float2 cloud coordinates in independent planes.
struct ALMetalWorldCloudBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t texcoord_offset = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    float modelview_projection[16]{};
};

// CPU-packed cloudsF.glsl inputs. Every vec3 is padded into a float4 so the
// structure is also the Metal constant-buffer ABI.
struct alignas(16) ALMetalWorldCloudConstants
{
    float camera_local[4]{};
    // xyz = clamped sky-frame light direction, w = density multiplier.
    float light_density[4]{};
    // xyz = blue horizon, w = scalar haze horizon.
    float blue_horizon[4]{};
    // xyz = glow, w = maximum atmospheric layer height.
    float glow[4]{};
    float haze_extinction[4]{};
    float blue_weight[4]{};
    float haze_weight[4]{};
    float sky_sunlight[4]{};
    float atmos_ambient[4]{};
    float sky_light_atten[4]{};
    float cloud_sunlight[4]{};
    // xyz = cloud color, w = cloud shadow.
    float cloud_color_shadow[4]{};
    float cloud_pos_density1[4]{};
    float cloud_pos_density2[4]{};
    // x = cloud scale, y = variance, z = texture blend,
    // w = sun/moon glow factor.
    float parameters[4]{};
};

struct ALMetalWorldCloudScene
{
    const ALMetalWorldCloudBatch* batches = nullptr;
    std::size_t batch_count = 0;
    const ALMetalTexture* noise_texture = nullptr;
    const ALMetalTexture* next_noise_texture = nullptr;
    ALMetalWorldCloudConstants constants;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool normal_hdr = false;
    bool hdr = false;
    bool reverse_z = false;
};

struct alignas(16) ALMetalWorldBaseLightingConstants
{
    float inverse_projection[16]{};
    // xyz = active sun/moon direction in eye space.
    float light_direction[4]{};
    // The already packed Environment block terms used by softenLightF.
    float atmos_sunlit[4]{};
    float atmos_sunlit_linear[4]{};
    float atmos_amblit[4]{};
    // xyz columns, vec4-strided, matching the deferred SSAO matrix.
    float ssao_effect[3][4]{};
    // x = sky sunlight scale, y = sky ambient scale,
    // z = SSAO irradiance scale, w = SSAO irradiance maximum.
    float lighting[4]{};
    // x = legacy specular exponent.
    float surface[4] = {128.f, 0.f, 0.f, 0.f};
    // x = WindLight sky_hdr_scale. The HDRI path leaves this at one.
    float sky_transfer[4] = {1.f, 0.f, 0.f, 0.f};
    // x = a native sky transaction populated the clear-depth pixels,
    // y = the G-buffer has an emissive attachment, z = sky family
    // (0 HDRI, 1 WindLight, 2 split). w is reserved.
    std::uint32_t sky[4]{};
    // x = reverse Z, y = classic mode, z = sun shadow, w = SSAO.
    std::uint32_t config[4]{};
};

struct ALMetalWorldBaseLightingScene
{
    const ALMetalRenderTarget* gbuffer = nullptr;
    // Transitional producer: the exact GL sun-shadow/SSAO result after its
    // configured blur. The native soften owner consumes it without changing
    // its response curve while the shadow/SSAO producers are migrated.
    const ALMetalTexture* light_map = nullptr;
    const ALMetalWorldReflectionProbes* reflection_probes = nullptr;
    const ALMetalWorldHeroProbe* hero_probe = nullptr;
    const ALMetalWorldScreenSpaceReflections* screen_space_reflections =
        nullptr;
    ALMetalWorldBaseLightingConstants constants;
    bool hdr = false;
};

// The ordered native world G-buffer owner for supported object, material and
// system-avatar families.
// It writes four colour attachments plus depth and is not presented yet. The
// viewer runs one bounded fail-closed probe so world integration can advance
// without doubling these passes every migration frame.
class ALMetalWorldGBufferRenderer final
{
public:
    ALMetalWorldGBufferRenderer(const ALMetalWorldGBufferRenderer&) = delete;
    ALMetalWorldGBufferRenderer& operator=(const ALMetalWorldGBufferRenderer&) = delete;
    ~ALMetalWorldGBufferRenderer();

    static std::unique_ptr<ALMetalWorldGBufferRenderer> create(
        const ALMetalDevice& device,
        std::string* error = nullptr);

    std::unique_ptr<ALMetalRenderTarget> render(
        const ALMetalWorldGBufferScene& scene,
        std::string* error = nullptr);

    // Render one spotlight-camera slice directly from retained viewer buffers.
    // R32F color stores the exact raster depth sampled by the projector owner;
    // a private D32 attachment supplies the authoritative depth test. The
    // caller may blit color attachment zero into its persistent two-slice pack.
    std::unique_ptr<ALMetalRenderTarget> renderSpotShadow(
        const ALMetalWorldSpotShadowScene& scene,
        std::string* error = nullptr);

    // Decode the retained native G-buffer and accumulate every ordinary
    // (non-projector) deferred point light into a linear screen-format
    // target. This is the first downstream native world owner; the result is
    // deliberately retained offscreen until the remaining lighting and
    // composition passes are native as well.
    std::unique_ptr<ALMetalRenderTarget> renderLocalLights(
        const ALMetalWorldLocalLightingScene& scene,
        std::string* error = nullptr);

    // Accumulate deferred projector cookies, ambiance, legacy reflection
    // and the two live filtered spot-shadow channels. A draw per projector
    // keeps each cookie's independent mip chain and sampler semantics.
    std::unique_ptr<ALMetalRenderTarget> renderProjectorLights(
        const ALMetalWorldProjectorLightingScene& scene,
        std::string* error = nullptr);

    // Reproduce the deferred screen-target accumulation boundary without
    // folding in post-deferred geometry, tone mapping or presentation.
    std::unique_ptr<ALMetalRenderTarget> composeLighting(
        const ALMetalWorldLightingCompositionScene& scene,
        std::string* error = nullptr);

    // Retain composed lighting and scene depth, then render the supported
    // ordered post-deferred families into a one-color output target.
    std::unique_ptr<ALMetalRenderTarget> renderOpaquePostDeferred(
        const ALMetalWorldOpaquePostDeferredScene& scene,
        std::string* error = nullptr);

    // Produce the deferred RG light map from native G-buffer depth/normals:
    // directional visibility in R and bilateral two-axis SSAO in G.
    std::unique_ptr<ALMetalRenderTarget> renderLightMap(
        const ALMetalWorldLightMapScene& scene,
        std::string* error = nullptr);

    // Load the retained world G-buffer and fill only its clear-depth pixels
    // with the exact full-HDRI branch of the deferred sky dome. Geometry is
    // preserved by read-only far-plane depth testing. This does not accept a
    // split HDRI because the complementary WindLight branch is a separate
    // native owner.
    bool compositeHDRISky(
        ALMetalRenderTarget& target,
        const ALMetalWorldHDRISkyScene& scene,
        std::string* error = nullptr);

    // Fill clear-depth pixels with skyF.glsl's exact non-HDRI atmospheric
    // branch. HDR output retains its display-encoded emissive convention;
    // vintage output round-trips the same bytes through an sRGB attachment.
    bool compositeWindLightSky(
        ALMetalRenderTarget& target,
        const ALMetalWorldWindLightSkyScene& scene,
        std::string* error = nullptr);

    // Re-render only the base WindLight dome into the half-resolution color
    // target sampled by hazeF's on-water convergence path. No later stars,
    // bodies, aurora or clouds belong in this texture.
    std::unique_ptr<ALMetalRenderTarget> renderWindLightSkyBehind(
        const ALMetalWorldWindLightSkyScene& scene,
        std::string* error = nullptr);

    // Add the exact starsV/starsF layer over a completed native WindLight
    // haze transaction. Vintage albedo blending uses the target's raw UNORM
    // storage view to preserve GL's framebuffer-sRGB-disabled byte contract.
    bool compositeWindLightStars(
        ALMetalRenderTarget& target,
        const ALMetalWorldStarScene& scene,
        std::string* error = nullptr);

    // Source-over the exact sunDisc and moon layers after the native stars.
    // Vintage albedo again uses the raw UNORM storage view because the GL sky
    // pool disables framebuffer sRGB for its complete ordered transaction.
    bool compositeWindLightHeavenlyBodies(
        ALMetalRenderTarget& target,
        const ALMetalWorldHeavenlyBodyScene& scene,
        std::string* error = nullptr);

    // Add the exact meteorsV/meteorsF streak layer after the heavenly bodies.
    bool compositeWindLightMeteors(
        ALMetalRenderTarget& target,
        const ALMetalWorldMeteorScene& scene,
        std::string* error = nullptr);

    // Add the exact procedural aurora dome after the meteor layer.
    bool compositeWindLightAurora(
        ALMetalRenderTarget& target,
        const ALMetalWorldAuroraScene& scene,
        std::string* error = nullptr);

    bool compositeWindLightClouds(
        ALMetalRenderTarget& target,
        const ALMetalWorldCloudScene& scene,
        std::string* error = nullptr);

    // Shade the retained world G-buffer with the deferred base-light owner:
    // ordinary-probe irradiance/radiance, sun/moon, SSAO and legacy/PBR
    // ambient semantics. The result remains offscreen until every downstream
    // deferred owner and final composition are native.
    std::unique_ptr<ALMetalRenderTarget> renderBaseLighting(
        const ALMetalWorldBaseLightingScene& scene,
        std::string* error = nullptr);

    // Composite supported post-deferred impostor owners into an existing
    // native G-buffer target. Attachment zero and depth are loaded in place;
    // the remaining attachments are preserved byte-for-byte. Callers may
    // request a depth-derived coverage stamp after an opaque forward phase.
    bool compositeImpostorPostDeferred(
        ALMetalRenderTarget& target,
        const ALMetalWorldGBufferScene& scene,
        std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalWorldGBufferRenderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
