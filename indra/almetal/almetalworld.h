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

// World/UI publication is exact only when every viewer draw after the retained
// native world replay is either represented in the ordered Metal transaction
// or absent. Keep this small policy explicit so new GL-only overlays cannot be
// omitted silently while native presentation remains eligible.
struct ALMetalWorldOverlayEligibility
{
    bool render_debug = false;
    bool pathfinding = false;

    bool exact() const
    {
        return !render_debug && !pathfinding;
    }
};

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
// legacyProbeFinish's transparent SSR branch. GL readback bootstraps and
// fail-closes the migration path; a complete native final-color frame may
// publish the same lower-row-first color/depth contract directly on-GPU.
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
// shadowUtil.glsl. Direct Metal cascades are retained as R32F array slices;
// an incomplete direct transaction may retain the transitional GL mirror.
// The shader performs the same forward/reverse comparison, RPDB and
// quality-tier filter so both sources share one contract.
struct alignas(16) ALMetalWorldDirectionalShadowConstants
{
    static constexpr std::uint32_t filter_quality_mask = 0x3u;
    static constexpr std::uint32_t probe_single_tap = 0x4u;
    static constexpr std::uint32_t filter_config_mask =
        filter_quality_mask | probe_single_tap;

    float shadow_matrix[4][16]{};
    float shadow_clip[4]{};
    // xy = shadow-map resolution, zw = GL screen_res used by PCSS rotation.
    float resolution[4]{};
    // x = signed constant bias, y = normal offset, z = PCSS scale,
    // w = destination height for Metal-to-GL fragment-coordinate inversion.
    float parameters[4]{};
    // x = cascade count, y = unbuilt-map mask, z = reverse-Z,
    // w low two bits = filter quality (0/1/2 Gaussian 2/4/6, 3 PCSS);
    // bit 2 = reflection-probe single-tap shortcut matching cube_snapshot.
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
    bool depth_clamp = false;
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
    bool avatar_alpha = false;
    // Hair and skirt meshes deliberately replay the same geometry with
    // different raster/depth/color state. Preserve that draw-boundary state
    // so the native sidecar does not collapse the two-/three-pass algorithm.
    ALMetalCullMode avatar_cull_mode = ALMetalCullMode::back;
    bool avatar_depth_write = true;
    bool avatar_color_write = true;
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

// Legacy edit-face and touch highlights are rendered after the ordered world
// geometry and before display post-processing. They sample one authored UV
// channel through the live texture matrix, multiply by the uniform highlight
// colour and alpha-blend against retained world colour with read-only depth.
struct ALMetalWorldHighlightBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t weight_offset = 0;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    const ALMetalTexture* texture = nullptr;
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
    float color[4]{};
    bool write_alpha = true;
};

// Developer "Show Updates to Objects" markers are emitted as immediate-mode
// eight-pixel points after all retained world geometry. Their streamed world
// position and color are small enough to retain directly; the renderer keeps
// the live camera transform separate so the batch matches the gUIProgram draw
// without manufacturing an LLVertexBuffer.
struct ALMetalWorldDebugBlip
{
    float position[4] = {0.f, 0.f, 0.f, 1.f};
    float color[4] = {1.f, 1.f, 1.f, 1.f};
};

// Develop > Wind Vectors is a fixed simulator lattice of one-pixel green
// points and red line segments. Its immediate-mode GL owner has the same
// position/color payload as an object-update blip, but uses the ambient
// read/write world depth state and a one-pixel raster footprint.
using ALMetalWorldWindVectorVertex = ALMetalWorldDebugBlip;

// Develop > Agent Target draws four translucent axis crosshairs for the self
// avatar. Each crosshair is six immediate-mode line vertices. The streamed
// position/color layout is shared with the other compact developer overlays;
// unlike Wind Vectors, this owner alpha-blends over the scene while retaining
// the same ambient read/write world depth convention. Scene alpha remains
// untouched.
using ALMetalWorldAgentTargetVertex = ALMetalWorldDebugBlip;

// One axis-aligned column in the developer far-plane projection overlay. The
// native vertex stage expands these retained center/half-extent records into
// the same 12 triangles as the immediate GL owner. Columns are ordered with
// all nearer (orange) records first, followed by farther (cyan) records, so a
// private depth attachment proves projection reach and ordering before the
// overlay is composited over the scene.
struct alignas(16) ALMetalWorldFarPlaneOverlayColumn
{
    float center[4] = {0.f, 0.f, 0.f, 1.f};
    float half_extent[4] = {0.f, 0.f, 0.f, 0.f};
    float color[4] = {1.f, 1.f, 1.f, 1.f};
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
    bool wireframe = false;
    // Eye-space plane used by the authoritative GLSL MIRROR_CLIP variants.
    // Native G-buffer vertices emit the matching Metal clip distance only
    // for hero-mirror cube faces; ordinary world and impostor scenes leave
    // the positive default plane disabled.
    float mirror_clip_plane[4] = {0.f, 0.f, 0.f, 1.f};
    bool mirror_clip = false;
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
    // The integrated main-world route can carry exact G-buffer depth into
    // the composed target so the following opaque-forward owner continues
    // in place instead of copying a full-frame color attachment. Standalone
    // lighting composition remains color-only when this is null.
    const ALMetalRenderTarget* depth_source = nullptr;
    bool depth_stencil_shader_read = false;
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
    // The probe-disabled GL water path rotates reflected eye-space vectors
    // into the current legacy sky cube with env_mat. These are the same
    // three padded columns captured from the live modelview.
    float environment_matrix[3][4] = {
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
    };
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
    // The probe-disabled GL forward paths rotate reflected eye-space
    // vectors into the current legacy sky cube with env_mat. These are the
    // same vec4-strided columns used by the base-light and water owners.
    float environment_matrix[3][4] = {
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
    };
    // x = reflection_probe_ambiance.
    float environment[4]{};
    // x = legacy sky-cube environment instead of reflection probes.
    std::uint32_t environment_config[4]{};
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
    const ALMetalWorldHighlightBatch* highlight_batches = nullptr;
    std::size_t highlight_batch_count = 0;
    const ALMetalWorldDebugBlip* debug_blips = nullptr;
    std::size_t debug_blip_count = 0;
    float debug_blip_modelview_projection[16]{};
    const ALMetalWorldWindVectorVertex* wind_vector_points = nullptr;
    std::size_t wind_vector_point_count = 0;
    const ALMetalWorldWindVectorVertex* wind_vector_lines = nullptr;
    std::size_t wind_vector_line_vertex_count = 0;
    float wind_vector_modelview_projection[16]{};
    const ALMetalWorldAgentTargetVertex* agent_target_lines = nullptr;
    std::size_t agent_target_line_vertex_count = 0;
    float agent_target_modelview_projection[16]{};
    const ALMetalWorldFarPlaneOverlayColumn* far_plane_overlay_columns =
        nullptr;
    std::size_t far_plane_overlay_column_count = 0;
    std::size_t far_plane_overlay_near_count = 0;
    float far_plane_overlay_modelview_projection[16]{};
    // Optional full-screen developer visualization of the live manual probe
    // influence volumes. It consumes only the selection constants and scene
    // depth; radiance/irradiance generations are irrelevant to this owner.
    const ALMetalWorldReflectionProbeConstants*
        reflection_probe_volume_constants = nullptr;
    float reflection_probe_volume_inverse_projection[16]{};
    const ALMetalWorldReflectionProbes* reflection_probes = nullptr;
    // Mutually exclusive with reflection_probes for forward rendering.
    // This is the current LLCubeMap publication used by the class-2 GLSL
    // shiny, alpha, PBR-alpha and water paths when reflection probes are
    // disabled.
    const ALMetalTexture* legacy_environment = nullptr;
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
    // The same eye-space half-space used by the G-buffer MIRROR_CLIP
    // variants. Forward alpha is replayed after deferred lighting, so it
    // must carry the plane independently instead of relying on clipped
    // opaque depth to reject transparent geometry.
    float mirror_clip_plane[4] = {0.f, 0.f, 0.f, 1.f};
    bool mirror_clip = false;
    // Immediately before the first alpha family, clear final color and
    // coverage where retained depth is still at the far plane, then stamp
    // opaque depth into alpha. Subsequent alpha pipelines accumulate their
    // source coverage over that seed, matching the cutout snapshot contract.
    bool seed_cutout_coverage = false;
    // Keep the combined depth plane shader-readable so the completed target
    // can become the next frame's coherent SSR color/depth history pair.
    bool publish_screen_space_reflection_history = false;
    bool wireframe = false;
};

// The display-transfer base of colorCorrectF.glsl. This owner
// deliberately stops at the real post-chain storage boundary: HDR writes
// RGB10_A2 after exposure and tonemapping, while the vintage path writes
// RGBA8 after the display transfer and output dither. Optional ordinary
// bloom/halation/cross-filter are folded in before exposure. Analytical color
// grading, the artist 3D LUT, baked channel curves and chromatic aberration can
// run in their exact pre/post-tonemap positions; flare/dirt and the legacy-sky
// gamma/no-tonemap permutation are native here, while CAS/AA and legacy glow
// remain distinct owners. Callers must not substitute this target when any
// omitted family or bypass is live.
struct ALMetalWorldColorGrade
{
    bool enabled = false;
    float white_balance_gain[3] = {1.f, 1.f, 1.f};
    float lift[3] = {0.f, 0.f, 0.f};
    float inverse_gamma[3] = {1.f, 1.f, 1.f};
    float gain[3] = {1.f, 1.f, 1.f};
    float shadow_ratio[3] = {1.f, 1.f, 1.f};
    float midtone_ratio[3] = {1.f, 1.f, 1.f};
    float highlight_ratio[3] = {1.f, 1.f, 1.f};
    float midtone_amount = 0.f;
    float split_amount = 0.f;
    float shadow_ramp[2] = {1.f, 0.f};
    float highlight_ramp[2] = {1.f, 0.f};
    float black_white_scale = 1.f;
    float black_white_bias = 0.f;
    float brightness_contrast_scale = 1.f;
    float brightness_contrast_bias = 0.f;
    float highlights_scaled = 0.f;
    float shadows_scaled = 0.f;
    float saturation = 1.f;
    float vibrance = 0.f;
    float hue_shift = 0.f;
    // A 3D LUT is represented as a square 2D-array texture with one slice per
    // blue coordinate; the shader performs the exact trilinear z blend.
    const ALMetalTexture* color_lut_texture = nullptr;
    std::uint32_t color_lut_size = 0;
    bool color_lut_flip_green = false;
    bool color_lut_swap_blue_green = false;
    float color_lut_strength = 0.f;
    // RGBA16_UNORM row containing the three baked composite channel curves.
    const ALMetalTexture* tone_curve_texture = nullptr;
    float tone_curve_scale = 1.f;
    float tone_curve_bias = 0.f;
    float tone_curve_amount = 0.f;
};

struct ALMetalWorldDisplayPostScene
{
    const ALMetalRenderTarget* source = nullptr;
    // Optional dynamic-exposure result. When present it must be the 1x1
    // RGBA16F output of renderExposure; its red scale replaces the scalar
    // adapted_exposure without crossing CPU memory.
    const ALMetalTexture* adapted_exposure_texture = nullptr;
    // Optional top level of the native HDR bloom pyramid. Ordinary bloom is
    // R11G11B10F; the halation permutation is RGBA16F with its intensity in
    // alpha.
    const ALMetalTexture* bloom_texture = nullptr;
    // Optional half-resolution RGBA16F sum of the cross-filter arms.
    const ALMetalTexture* cross_filter_texture = nullptr;
    bool hdr = false;
    // Snapshot clean plates keep the HDR source/storage contract but select
    // colorCorrectF's modern gamma-only permutation: no exposure or tonemap,
    // just linear clamping followed by the display transfer. This is distinct
    // from legacy_gamma, which additionally applies the authored sky gamma.
    bool bypass_tonemap = false;
    // Legacy skies keep the HDR/bloom storage path but deliberately skip
    // exposure and tonemapping, then apply colorCorrectF's historical gamma
    // curve after the linear-to-sRGB transfer.
    bool legacy_gamma = false;
    float legacy_gamma_exponent = 1.f;
    float static_exposure = 1.f;
    float adapted_exposure = 1.f;
    float tonemap_mix = 1.f;
    std::uint32_t tonemap_type = 0;
    float tonemap_params[4]{};
    float bloom_strength = 0.f;
    float halation_strength = 0.f;
    float halation_tint[3] = {1.f, 0.35f, 0.15f};
    float cross_filter_strength = 0.f;
    // Shader-ready values shared with the GLSL owner: the authored strength
    // has already been squared/scaled, falloff inverted and angle reduced.
    float chromatic_aberration_amount = 0.f;
    float chromatic_aberration_reciprocal_falloff = 1.f;
    float chromatic_aberration_anisotropy = 0.f;
    float chromatic_aberration_angle_sin_cos[2] = {0.f, 1.f};
    float chromatic_aberration_offset_r[2] = {-1.f, 0.f};
    float chromatic_aberration_offset_b[2] = {1.f, 0.f};
    // Optional 2x1 RGBA16F output of renderLensFlareState. A positive
    // strength requires this exact current-frame state; inactive flare uses
    // the renderer's immutable black state texture.
    const ALMetalTexture* lens_flare_state_texture = nullptr;
    float lens_flare_strength = 0.f;
    float lens_flare_sun_uv[2] = {0.5f, 0.5f};
    float lens_flare_light_color[3] = {1.f, 1.f, 1.f};
    float lens_flare_sky_hdr_scale = 1.f;
    float lens_flare_streak_length = 0.5f;
    float lens_flare_streak_falloff = 1.5f;
    float lens_flare_streak_width = 0.004f;
    float lens_flare_streak_intensity = 1.f;
    float lens_flare_streak_tint[3] = {0.6f, 0.7f, 1.f};
    float lens_flare_chromatic_spread = 0.008f;
    float lens_flare_glow = 1.f;
    float lens_flare_glow_radius = 0.12f;
    float lens_flare_glow_falloff = 8.f;
    float lens_flare_ghost = 0.f;
    std::uint32_t lens_flare_ghost_count = 4;
    float lens_flare_ghost_spacing = 0.3f;
    float lens_flare_halo = 0.f;
    float lens_flare_halo_radius = 0.5f;
    float lens_flare_halo_width = 0.15f;
    float lens_flare_starburst = 0.f;
    std::uint32_t lens_flare_starburst_spikes = 4;
    float lens_flare_starburst_sharpness = 24.f;
    float lens_flare_starburst_falloff = 13.333333f;
    // Optional same-size R8 plate from renderLensDirt. Dirt is applied to the
    // already-computed flare and bloom light before exposure/tonemapping.
    const ALMetalTexture* lens_dirt_texture = nullptr;
    float lens_dirt_strength = 0.f;
    float lens_dirt_bloom_response = 1.f;
    float lens_dirt_flare_response = 1.f;
    ALMetalWorldColorGrade color_grade;
    bool cutout_premultiplied = false;
    // HDR cutouts retain eight bits of coverage instead of crossing the
    // ordinary RGB10_A2 display boundary, whose two alpha bits are not an
    // adequate snapshot mask. HDR math and tonemapping remain unchanged.
    bool preserve_alpha = false;
    // Non-HDR only: colorCorrectF applies output dither at this first 8-bit
    // boundary. HDR defers its 10-bit dither until final presentation.
    float dither_amount = 0.f;
    bool dither_animated = false;
    std::uint32_t frame_id = 0;
};

// One enabled RLVa @setsphere owner, already sampled from the policy/tween
// object that feeds the established GLSL program.  The native post pass runs
// after AA/CAS/bloom and before scopes/reference capture, matching
// renderFinalize's observable ordering.
struct ALMetalWorldRlvSphereScene
{
    const ALMetalRenderTarget* source = nullptr;
    const ALMetalTexture* depth_texture = nullptr;
    float inverse_projection[16]{};
    float origin[4]{};
    // x/y = value-min/distance-min; z/w = value-max/distance-max.
    float distance_value[4]{};
    float effect[4]{};
    std::uint32_t mode = 0;
    bool extend_min = false;
    bool extend_max = false;
    bool reverse_z = false;
};

// Final display-space print treatment. The source is the completed RGB10_A2
// or RGBA8 display target after AA/CAS. The result uses the window surface's
// linear BGRA8 storage and is deliberately offscreen until native world UI
// can be composed over it in the same drawable transaction.
struct ALMetalWorldPresentationScene
{
    const ALMetalRenderTarget* source = nullptr;
    const ALMetalTexture* reference_texture = nullptr;
    std::uint32_t reference_mode = 0;
    float reference_position = 0.5f;

    // CPU-baked Brown-Conrady inverse gather, matching renderFinalize.
    float lens_distortion_amount = 0.f;
    float lens_distortion_k[2] = {0.f, 0.f};
    float lens_distortion_scale = 1.f;
    float lens_distortion_squeeze[2] = {1.f, 1.f};
    float lens_distortion_center[2] = {0.f, 0.f};
    float lens_distortion_tangential[2] = {0.f, 0.f};

    float vignette_amount = 0.f;
    float vignette_radius = 1.f;
    float vignette_softness = 0.5f;
    float vignette_shape = 0.f;
    float vignette_outer_color[3] = {0.f, 0.f, 0.f};
    float vignette_middle_color[3] = {0.f, 0.f, 0.f};
    float vignette_middle_point = 0.f;
    float vignette_center[2] = {0.f, 0.f};
    // (1,1) disables correction; production supplies source dimensions when
    // RenderVignetteCorrectAspect is active.
    float vignette_aspect[2] = {1.f, 1.f};
    float vignette_feather = 1.f;

    std::uint32_t cvd_mode = 0;
    float cvd_amount = 0.f;
    float grain_amount = 0.f;
    std::uint32_t grain_style = 0;
    float grain_size = 1.f;
    float grain_range = 0.5f;
    float grain_tint[3] = {1.f, 1.f, 1.f};
    bool grain_animated = true;
    float dither_amount = 0.f;
    std::uint32_t dither_bits = 8;
    bool dither_animated = true;
    std::uint32_t preview_mode = 0;
    std::uint32_t frame_id = 0;
};

// Per-frame flare visibility and temporal state. The source is the sharp
// linear scene and depth is its completed world depth. Previous is null only
// for the exact zero-history bootstrap; otherwise it must be the prior 2x1
// output from this method.
struct ALMetalWorldLensFlareStateScene
{
    const ALMetalRenderTarget* source = nullptr;
    const ALMetalTexture* depth_texture = nullptr;
    const ALMetalTexture* previous = nullptr;
    float delta_seconds = 0.f;
    float sun_uv[2] = {0.5f, 0.5f};
    float edge_visibility = 0.f;
    float occlusion_radius = 0.f;
    float fade_time = 0.35f;
    bool reverse_z = false;
};

// Cached procedural front-element dirt plate. Production rebuilds this only
// when its dimensions or authored parameters change.
struct ALMetalWorldLensDirtScene
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float seed = 7.f;
    float grime = 1.f;
    float mote_scale = 1.f;
    float smudge = 1.f;
    std::uint32_t scratches = 0;
    float toe = 1.6f;
    float gain = 1.f;
};

// Pre-tonemap HDR depth of field. The gather runs at resolution_scale and
// the combine restores the same-size RGBA16F scene while preserving alpha.
// inverse_projection uses the viewer's existing column-major convention.
struct ALMetalWorldDepthOfFieldScene
{
    const ALMetalRenderTarget* source = nullptr;
    const ALMetalTexture* depth_texture = nullptr;
    float inverse_projection[16]{};
    float focal_distance = -16.f;
    float blur_constant = 0.f;
    float tan_pixel_angle = 0.f;
    float magnification = 0.f;
    float max_cof = 0.f;
    float resolution_scale = 1.f;
    float highlight_threshold = 0.f;
    float highlight_gain = 0.f;
    float highlight_clamp = 64.f;
    std::int32_t aperture_blades = 0;
    float aperture_rotation = 0.f;
    float aperture_curvature = 0.f;
    float anamorphic_squeeze = 1.f;
    float cat_eye = 0.f;
    float fringe_amount = 0.f;
    float fringe_near_tint[3] = {1.f, 0.85f, 1.f};
    float fringe_far_tint[3] = {0.85f, 1.f, 0.9f};
    float spherical_aberration = 0.f;
    float field_stretch = 0.f;
    float field_falloff = 2.f;
    float coma_asymmetry = 0.f;
    bool reverse_z = false;
    bool near_blur = false;
    // GL uses RGBA16F for the gather when halation needs its alpha-capable
    // bloom family; otherwise it uses R11G11B10F.
    bool rgba16_blur = false;
};

// First pass of the production SMAA 1x chain. It writes the west/north color
// edge mask into RGBA8; an optional same-size depth texture applies the live
// predicated threshold. The result is not displayable until the weight and
// neighborhood stages have completed.
struct ALMetalWorldSMAAEdgeScene
{
    const ALMetalRenderTarget* source = nullptr;
    const ALMetalTexture* predication_texture = nullptr;
    std::uint32_t quality = 0;
    float predication_threshold = 0.002f;
    float predication_scale = 2.f;
    float predication_strength = 0.4f;
};

// Second pass of the production SMAA 1x chain. It consumes the exact RGBA8
// edge mask above and combines it with the shipped area/search lookup tables.
// The resulting weights are not displayable until neighborhood blending.
struct ALMetalWorldSMAAWeightScene
{
    const ALMetalRenderTarget* edges = nullptr;
    std::uint32_t quality = 0;
};

// Third pass of the production SMAA 1x chain. It combines the display-encoded
// source with the exact directional weight target in linear light.
struct ALMetalWorldSMAANeighborhoodScene
{
    const ALMetalRenderTarget* source = nullptr;
    const ALMetalRenderTarget* weights = nullptr;
};

// Complete production SMAA 1x transaction. No intermediate is published:
// failure of any stage returns null so the caller can retain its existing
// display owner.
struct ALMetalWorldSMAAScene
{
    const ALMetalRenderTarget* source = nullptr;
    const ALMetalTexture* predication_texture = nullptr;
    std::uint32_t quality = 0;
    float predication_threshold = 0.002f;
    float predication_scale = 2.f;
    float predication_strength = 0.4f;
};

// The production HDR bloom owner. It reproduces the world
// resource plan's five resolution scales and 3..7-level transient pyramid:
// soft-knee/legacy-alpha extract, outlier-aware first downsample, ordinary
// 13-tap lower levels and additive tent upsample. Ordinary bloom uses
// R11G11B10F; the optional halation permutation carries intensity through
// RGBA16F alpha. The returned target is mip zero after accumulation and is
// ready for renderDisplayPost to composite in linear light before exposure
// and tonemapping.
struct ALMetalWorldBloomScene
{
    const ALMetalRenderTarget* source = nullptr;
    std::uint32_t mip_count = 5;
    std::uint32_t resolution_scale = 0;
    float threshold = 1.f;
    float knee = 0.5f;
    float firefly_clamp = 24.f;
    float scatter = 0.7f;
    float alpha_glow_boost = 2.f;
    bool halation = false;
    float halation_weights[3] = {1.f, 1.f, 1.f};
    bool cross_filter = false;
    std::uint32_t cross_filter_points = 4;
    float cross_filter_angle = 0.f;
    float cross_filter_length = 1.f;
    float cross_filter_falloff = 1.5f;
    float cross_filter_chromatic = 0.f;
};

// The production HDR exposure owner. The first compute pass reproduces the
// fixed 128x72 luminance grid, including the G-buffer sky split; the second
// constructs and trims the 64-bin log-luminance histogram and adapts from the
// optional previous 1x1 result. The returned RGBA16F texel has the same
// (scale, adapted EV, target EV, 1) layout as exposureF.glsl.
struct ALMetalWorldExposureScene
{
    const ALMetalRenderTarget* source = nullptr;
    const ALMetalTexture* normal = nullptr;
    const ALMetalTexture* previous = nullptr;
    float delta_seconds = 0.f;
    float compensation = 0.f;
    float min_ev = -3.f;
    float max_ev = 4.f;
    float low_fraction = 0.1f;
    float high_fraction = 0.95f;
    float speed_up = 3.f;
    float speed_down = 1.f;
    float sky_weight = 1.f;
    std::uint32_t metering_mode = 1;
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
    bool wireframe = false;
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
    bool wireframe = false;
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
    bool wireframe = false;
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
    bool wireframe = false;
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
    bool wireframe = false;
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
    bool wireframe = false;
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
    bool wireframe = false;
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
    // The probe-disabled GL path rotates reflection vectors into the legacy
    // sky cube with env_mat. Store the same three padded columns here; x of
    // environment is reflection_probe_ambiance.
    float environment_matrix[3][4] = {
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
    };
    float environment[4]{};
    // x = WindLight sky_hdr_scale. The HDRI path leaves this at one.
    float sky_transfer[4] = {1.f, 0.f, 0.f, 0.f};
    // x = a native sky transaction populated the clear-depth pixels,
    // y = the G-buffer has an emissive attachment, z = sky family
    // (0 HDRI, 1 WindLight, 2 split). w is reserved.
    std::uint32_t sky[4]{};
    // x = reverse Z, y = classic mode, z = sun shadow, w = SSAO.
    std::uint32_t config[4]{};
    // x = legacy sky-cube environment instead of reflection probes.
    std::uint32_t environment_config[4]{};
};

struct ALMetalWorldBaseLightingScene
{
    const ALMetalRenderTarget* gbuffer = nullptr;
    // Transitional producer: the exact GL sun-shadow/SSAO result after its
    // configured blur. The native soften owner consumes it without changing
    // its response curve while the shadow/SSAO producers are migrated.
    const ALMetalTexture* light_map = nullptr;
    const ALMetalWorldReflectionProbes* reflection_probes = nullptr;
    // Mutually exclusive with reflection_probes. This is the current
    // LLCubeMap publication used by class2/deferred/reflectionProbeF.glsl
    // when RenderReflectionProbes is disabled.
    const ALMetalTexture* legacy_environment = nullptr;
    const ALMetalWorldHeroProbe* hero_probe = nullptr;
    const ALMetalWorldScreenSpaceReflections* screen_space_reflections =
        nullptr;
    ALMetalWorldBaseLightingConstants constants;
    // The integrated main-world route continues this owner through local and
    // projector accumulation, then opaque-forward rendering. Carrying the
    // exact G-buffer depth here removes the lighting-composition target and
    // draw while preserving zero-copy opaque-forward continuation. Standalone
    // base-light callers keep the historical color-only target by leaving
    // this false.
    bool carry_depth = false;
    bool depth_stencil_shader_read = false;
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

    // Group a caller-owned sequence of dependent world encoders into one
    // command buffer. Standalone calls retain their existing immediate
    // commit behavior. A batch is deliberately explicit so reflection,
    // impostor and test callers cannot accidentally share frame lifetime.
    bool beginCommandBatch(
        const std::string& label,
        std::string* error = nullptr);
    bool commitCommandBatch(std::string* error = nullptr);
    void cancelCommandBatch();
    bool commandBatchActive() const;

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

    // Add ordinary point lights directly to an existing base-light target.
    // This is the production continuation path; the standalone method above
    // remains available for isolated validation and produces the same light
    // contribution in a cleared target.
    bool accumulateLocalLights(
        ALMetalRenderTarget& target,
        const ALMetalWorldLocalLightingScene& scene,
        std::string* error = nullptr);

    // Accumulate deferred projector cookies, ambiance, legacy reflection
    // and the two live filtered spot-shadow channels. A draw per projector
    // keeps each cookie's independent mip chain and sampler semantics.
    std::unique_ptr<ALMetalRenderTarget> renderProjectorLights(
        const ALMetalWorldProjectorLightingScene& scene,
        std::string* error = nullptr);

    // Add projector draws directly after base and ordinary lighting. The
    // projector pipeline already implements the viewer's ONE/ONE blend.
    bool accumulateProjectorLights(
        ALMetalRenderTarget& target,
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

    // Continue an integrated lighting-composition target in place. The
    // owner must be the same object borrowed by scene.composed_lighting and
    // must already contain the exact G-buffer depth attachment.
    std::unique_ptr<ALMetalRenderTarget> renderOpaquePostDeferred(
        const ALMetalWorldOpaquePostDeferredScene& scene,
        std::unique_ptr<ALMetalRenderTarget> composed_lighting_owner,
        std::string* error = nullptr);

    // Convert one completed linear native world target into the GL post
    // chain's first display-space target without crossing CPU memory. Bloom,
    // halation, cross-filter, chromatic aberration, synchronized lens flare
    // and complete grading may be folded in; this is not final presentation.
    std::unique_ptr<ALMetalRenderTarget> renderDisplayPost(
        const ALMetalWorldDisplayPostScene& scene,
        std::string* error = nullptr);

    // Apply one enabled RLVa vision-sphere effect to the completed
    // post-AA/CAS display target. Blur modes execute their dependent
    // horizontal/vertical passes inside this transaction; callers chain up
    // to six enabled effects in LLVfxManager priority order.
    std::unique_ptr<ALMetalRenderTarget> renderRlvSphere(
        const ALMetalWorldRlvSphereScene& scene,
        std::string* error = nullptr);

    // Freeze the exact post-AA/CAS display source consumed by the final
    // presentation pass. The copy stays on the GPU and joins an active
    // command batch, so a reference is never published from a transaction
    // whose later presentation or UI owner failed.
    std::unique_ptr<ALMetalRenderTarget> capturePresentationReference(
        const ALMetalRenderTarget& source,
        std::string* error = nullptr);

    // Apply the final lens/print/reference family as one display-space pass.
    // Standalone CAS remains the preceding owner, so this method never hides
    // an extra sharpen transaction or changes the scope measurement boundary.
    std::unique_ptr<ALMetalRenderTarget> renderPresentationPost(
        const ALMetalWorldPresentationScene& scene,
        std::string* error = nullptr);

    // Measure the HDR sun disc through final world depth and advance the
    // flare's bounded 2x1 temporal state entirely on the Metal queue.
    std::unique_ptr<ALMetalRenderTarget> renderLensFlareState(
        const ALMetalWorldLensFlareStateScene& scene,
        std::string* error = nullptr);

    // Generate the same full-resolution scalar dirt plate as
    // lensDirtGenF.glsl without GL readback.
    std::unique_ptr<ALMetalRenderTarget> renderLensDirt(
        const ALMetalWorldLensDirtScene& scene,
        std::string* error = nullptr);

    // Gather and combine pre-tonemap depth of field as one fail-closed
    // transaction. The sharp source is unchanged when this returns null.
    std::unique_ptr<ALMetalRenderTarget> renderDepthOfField(
        const ALMetalWorldDepthOfFieldScene& scene,
        std::string* error = nullptr);

    // Generate the exact HDR bloom/halation pyramid used by the main world
    // and optionally return its half-resolution cross-filter arm sum.
    std::unique_ptr<ALMetalRenderTarget> renderBloom(
        const ALMetalWorldBloomScene& scene,
        std::string* error = nullptr,
        std::unique_ptr<ALMetalRenderTarget>* cross_filter = nullptr);

    // Apply the standalone, one-to-one FidelityFX CAS pass used whenever a
    // scope or another post owner prevents sharpening from being folded into
    // the final GL blit. The RGB10_A2 output intentionally carries alpha zero,
    // matching CASF.glsl's vec4-zero initialization.
    std::unique_ptr<ALMetalRenderTarget> renderCAS(
        const ALMetalRenderTarget& source,
        float sharpness,
        std::string* error = nullptr);

    // Apply the post-tonemap one-pass FXAA 3.11 quality path used by the
    // viewer. Quality 0..3 maps to the legacy presets 12/23/28/39; the
    // RGB10_A2 result carries the center-pixel luminance in alpha exactly as
    // fxaaF.glsl does when FXAA_LUMA_FROM_RGB is enabled.
    std::unique_ptr<ALMetalRenderTarget> renderFXAA(
        const ALMetalRenderTarget& source,
        std::uint32_t quality,
        std::string* error = nullptr);

    // Generate only SMAA's exact color-edge target. This deliberately does
    // not imply complete SMAA ownership; callers must retain their existing
    // output until the later weight and neighborhood passes also succeed.
    std::unique_ptr<ALMetalRenderTarget> renderSMAAEdges(
        const ALMetalWorldSMAAEdgeScene& scene,
        std::string* error = nullptr);

    // Generate only SMAA's exact blending-weight target. This remains an
    // offscreen intermediate and does not imply complete SMAA ownership.
    std::unique_ptr<ALMetalRenderTarget> renderSMAABlendWeights(
        const ALMetalWorldSMAAWeightScene& scene,
        std::string* error = nullptr);

    // Complete SMAA's third pass over already validated edge/weight results.
    // Production callers still own the transaction-level fallback decision.
    std::unique_ptr<ALMetalRenderTarget> renderSMAANeighborhood(
        const ALMetalWorldSMAANeighborhoodScene& scene,
        std::string* error = nullptr);

    // Execute edge detection, blending weights and neighborhood blending as
    // one fail-closed publication transaction.
    std::unique_ptr<ALMetalRenderTarget> renderSMAA(
        const ALMetalWorldSMAAScene& scene,
        std::string* error = nullptr);

    // Meter and adapt one HDR world frame entirely on the Metal queue. The
    // previous result is optional and EV 0 is used when no history exists.
    std::unique_ptr<ALMetalRenderTarget> renderExposure(
        const ALMetalWorldExposureScene& scene,
        std::string* error = nullptr);

    // Point-decimate a completed display-post target into the exact RGBA8
    // CPU scope format. RGB10_A2 therefore takes its real 10-to-8-bit
    // quantization on the GPU before the nonblocking readback ring sees it.
    std::unique_ptr<ALMetalRenderTarget> renderDisplaySample(
        const ALMetalRenderTarget& source,
        std::uint32_t width,
        std::uint32_t height,
        std::string* error = nullptr);

    // Read one raw-storage row/column from a completed linear final-color
    // target as float RGBA. This is intentionally synchronous for sparse CPU
    // tools such as the click-driven scene probe, never a frame path.
    bool readSceneColorPixel(
        const ALMetalRenderTarget& source,
        std::uint32_t x,
        std::uint32_t y,
        float color[4],
        std::string* error = nullptr) const;

    // Convert the completed native final-color target into the exact paired
    // SSR history formats without a CPU readback. HDR RGBA16F is quantized at
    // this boundary to R11G11B10F, combined depth is extracted to R32F, and
    // both outputs are vertically reversed to retain glReadPixels' historical
    // lower-row-first storage contract. A matching target is reused in place.
    bool updateScreenSpaceReflectionHistory(
        const ALMetalRenderTarget& source,
        std::unique_ptr<ALMetalRenderTarget>& history,
        std::string* error = nullptr);

    // Retain one completed native hero-probe face in a private cube without
    // crossing through CPU memory. Recreating the cube clears valid_faces;
    // a successful copy publishes the selected bit only after submission.
    bool updateHeroProbeFace(
        const ALMetalRenderTarget& source,
        std::uint32_t face,
        std::uint32_t max_probe_lod,
        std::unique_ptr<ALMetalTexture>& source_cube,
        std::uint32_t& valid_faces,
        std::string* error = nullptr);

    // Build the source cube's mip chain and reproduce radianceGenF.glsl's
    // gloss-quarter GGX filter directly on the Metal queue. All six native
    // faces must belong to the same caller-managed generation.
    bool filterHeroProbeRadiance(
        ALMetalTexture& source_cube,
        std::uint32_t valid_faces,
        std::uint32_t max_probe_lod,
        float probe_strength,
        std::unique_ptr<ALMetalTexture>& radiance_cube,
        std::string* error = nullptr);

    // Retain one ordinary-probe face after reproducing the live separable
    // Gaussian supersample resolve. The resulting cube carries exactly the
    // authored probe mip range and never crosses CPU memory.
    bool updateReflectionProbeFace(
        const ALMetalRenderTarget& source,
        std::uint32_t face,
        std::uint32_t probe_resolution,
        std::uint32_t max_probe_lod,
        std::unique_ptr<ALMetalTexture>& source_cube,
        std::uint32_t& valid_faces,
        std::string* error = nullptr);

    // Reproduce radianceGenF.glsl's full 32-sample ordinary-probe filter.
    // All six source faces must be coherent; the retained output is a cube
    // ready for a GPU-only copy into the live cube-array slot.
    bool filterReflectionProbeRadiance(
        ALMetalTexture& source_cube,
        std::uint32_t valid_faces,
        std::uint32_t max_probe_lod,
        std::unique_ptr<ALMetalTexture>& radiance_cube,
        std::string* error = nullptr);

    // Project one coherent ordinary source cube into the exact four-texel
    // packed SH row consumed by reflectionProbeF.glsl. The command remains
    // owned by the caller for non-blocking completion polling; packed_words
    // is shared storage for the small default-preview CPU consumer.
    bool projectReflectionProbeSH(
        ALMetalTexture& source_cube,
        std::uint32_t valid_faces,
        std::uint32_t source_mip,
        std::uint32_t source_resolution,
        ALMetalTexture& destination,
        std::uint32_t destination_row,
        std::unique_ptr<ALMetalBuffer>& packed_words,
        std::unique_ptr<ALMetalCommandBuffer>& pending_command,
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
    // deferred owner has completed successfully.
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
    ALMetalRenderTarget* renderLocalLightsInternal(
        const ALMetalWorldLocalLightingScene& scene,
        ALMetalRenderTarget* accumulation_target,
        std::unique_ptr<ALMetalRenderTarget>& standalone_target,
        std::string* error);
    ALMetalRenderTarget* renderProjectorLightsInternal(
        const ALMetalWorldProjectorLightingScene& scene,
        ALMetalRenderTarget* accumulation_target,
        std::unique_ptr<ALMetalRenderTarget>& standalone_target,
        std::string* error);

    struct Impl;
    explicit ALMetalWorldGBufferRenderer(std::unique_ptr<Impl> impl);

    bool filterProbeRadiance(
        ALMetalTexture& source_cube,
        std::uint32_t valid_faces,
        std::uint32_t max_probe_lod,
        std::uint32_t output_mip_count,
        std::uint32_t sample_scale,
        ALMetalPixelFormat output_format,
        bool hero_probe,
        float probe_strength,
        std::unique_ptr<ALMetalTexture>& radiance_cube,
        std::string* error);
    std::unique_ptr<Impl> mImpl;
};
