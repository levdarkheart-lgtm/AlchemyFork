/**
 * @file almetalworld.cpp
 * @brief Native Metal world G-buffer bootstrap pass.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetalworld.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    void setError(std::string* destination, const std::string& message)
    {
        if (destination)
        {
            *destination = message;
        }
    }

    struct alignas(16) WorldSimpleConstants
    {
        float modelview_projection[16];
        float normal_matrix[3][4];
        float rigged_normal_matrix[3][4];
        float texture_matrix[16];
        float material[4]{};
        std::uint32_t depth_convention[4]{};
        float avatar_wind[4]{};
        float avatar_sin_wave[4]{};
        float avatar_gravity[4]{};
        float avatar_cloth_depth[4]{};
    };

    struct alignas(16) WorldWaterExclusionConstants
    {
        float modelview_projection[16]{};
        float modelview[16]{};
        float eye_to_agent[16]{};
        float cell_square[4]{};
        float camera_default_height[4]{};
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldWaterExclusionConstants) == 240);

    struct alignas(16) WorldWaterHazeConstants
    {
        float modelview_projection[16]{};
        float inverse_projection[16]{};
        float cell_square[4]{};
        float default_height[4]{};
        float rings[4]{};
        float water_plane[4]{};
        float fog_color[4]{};
        float fog[4]{};
        // x = reverse-Z, y = above-water field, z = field part.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldWaterHazeConstants) == 240);

    struct alignas(16) WorldShinyConstants
    {
        float modelview_projection[16];
        float modelview[16];
        float normal_matrix[3][4];
        float rigged_normal_matrix[3][4];
        float texture_matrix[16];
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldShinyConstants) == 304);
    static_assert(sizeof(ALMetalWorldAlphaEnvironment) == 240);
    static_assert(sizeof(ALMetalWorldLegacyAlphaLight) == 64);
    static_assert(sizeof(ALMetalWorldLegacyAlphaLighting) == 496);
    static_assert(sizeof(ALMetalWorldHeroProbeConstants) == 112);
    static_assert(sizeof(ALMetalWorldScreenSpaceReflectionConstants) == 256);
    static_assert(sizeof(ALMetalWorldDirectionalShadowConstants) == 320);

    struct alignas(16) WorldHDRISkyConstants
    {
        float modelview_projection[16]{};
        float camera_exposure[4]{};
        float environment_rotation[3][4]{};
        float screen[4]{};
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldHDRISkyConstants) == 160);

    struct alignas(16) WorldWindLightSkyConstants
    {
        float modelview_projection[16]{};
        ALMetalWorldWindLightSkyConstants sky;
        // xy = destination dimensions, z = deferred sky_hdr_scale.
        float transfer[4]{};
        // x = reverse Z, y = emissive/HDR G-buffer.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(ALMetalWorldWindLightSkyConstants) == 192);
    static_assert(sizeof(WorldWindLightSkyConstants) == 288);

    struct alignas(16) WorldStarConstants
    {
        float modelview_projection[16]{};
        float screen_alpha_time[4]{};
        float moon_direction_brightness[4]{};
        float moon_quad_center[4]{};
        float moon_quad_u[4]{};
        float moon_quad_v[4]{};
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldStarConstants) == 160);

    struct alignas(16) WorldHeavenlyBodyConstants
    {
        float modelview_projection[16]{};
        // x = sun texture blend or moon brightness, y = moon direction z.
        float parameters[4]{};
        // x = reverse Z, y = emissive/HDR G-buffer, z = moon.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldHeavenlyBodyConstants) == 96);

    struct alignas(16) WorldMeteorConstants
    {
        float modelview_projection[16]{};
        // xy = target dimensions, z = half-width in pixels.
        float screen_width[4]{};
        // x = reverse Z, y = emissive/HDR G-buffer.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldMeteorConstants) == 96);

    struct alignas(16) WorldAuroraConstants
    {
        float modelview_projection[16]{};
        // xyz = camera in dome-local space, w = intensity.
        float camera_intensity[4]{};
        // x = wrapped time, yz = target dimensions.
        float time_screen[4]{};
        // x = reverse Z, y = emissive/HDR G-buffer.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldAuroraConstants) == 112);

    struct alignas(16) WorldCloudConstants
    {
        float modelview_projection[16]{};
        ALMetalWorldCloudConstants cloud;
        float screen[4]{};
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(ALMetalWorldCloudConstants) == 240);
    static_assert(sizeof(WorldCloudConstants) == 336);

    struct alignas(16) WorldMaterialSlotConstants
    {
        float specular_color[4];
        float properties[4];
    };

    struct alignas(16) WorldImpostorConstants
    {
        float modelview_projection[16];
        float center[4];
        float left[4];
        float up[4];
        float normal_rotation[3][4];
        std::uint32_t depth_convention[4]{};
    };

    struct alignas(16) WorldMaterialConstants
    {
        WorldMaterialSlotConstants
            slots[ALMetalWorldSimpleBatch::max_material_slots]{};
        std::uint32_t config[4]{};
    };

    struct alignas(16) WorldPBRSlotConstants
    {
        float texture_transform[4][8];
        float emissive_color[4];
        // x = roughness, y = metallic, z = alpha cutoff,
        // w = streamed base-colour alpha factor.
        float properties[4];
    };

    struct alignas(16) WorldPBRConstants
    {
        WorldPBRSlotConstants
            slots[ALMetalWorldSimpleBatch::max_material_slots]{};
        // x = populated slot count, y = alpha mask, z = HDR normal target.
        std::uint32_t config[4]{};
    };

    struct alignas(16) WorldLocalLightingConstants
    {
        float inverse_projection[16]{};
        // x = legacy specular exponent, y = classic-mode output scale.
        float surface[4] = {128.f, 1.f, 0.f, 0.f};
        // x = light count, y = reverse-Z convention.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldLocalLightingConstants) == 96);
    static_assert(sizeof(ALMetalWorldLocalLight) == 32);

    struct alignas(16) WorldProjectorLightingConstants
    {
        float inverse_projection[16]{};
        float projector_matrix[16]{};
        float position_radius[4]{};
        float color_falloff[4]{};
        float projector_plane[4]{};
        float projector_normal[4]{};
        float projector_origin[4]{};
        // x = cookie LOD range, y = shadow fade,
        // z = legacy specular exponent, w = classic output scale.
        float sampling[4]{};
        // x = reverse Z, y = shadow channel + 1,
        // z = high-precision geometric-normal packing.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldProjectorLightingConstants) == 240);
    static_assert(sizeof(ALMetalWorldLightMapConstants) == 208);
    static_assert(sizeof(ALMetalWorldBaseLightingConstants) == 256);

    struct alignas(16) WorldTerrainConstants
    {
        float modelview_projection[16];
        float modelview[16];
        float normal_matrix[3][4];
        float surface[4]{};
        float detail[4]{};
        std::uint32_t config[4]{};
        std::uint32_t pbr_config[4]{};
        float hex_fraction[8][4]{};
        std::int32_t hex_base[8][4]{};
        struct alignas(16) PBRSlot
        {
            float uv_transform[4]{};
            float uv_offset[4]{};
            float normal_axes[4]{};
            float base_color_factor[4]{};
            float emissive_color[4]{};
            // x = roughness, y = metallic, z = alpha cutoff.
            float properties[4]{};
        } pbr_slots[4];
    };

    bool finiteValues(const float* values, std::size_t count)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            if (!std::isfinite(values[index]))
            {
                return false;
            }
        }
        return true;
    }

    bool rangeFits(std::size_t offset, std::size_t count,
                   std::size_t stride, std::size_t length)
    {
        return count <= (std::numeric_limits<std::size_t>::max() - offset) /
                            stride &&
               offset + count * stride <= length;
    }

    bool validMaterialAlphaMode(std::uint32_t mode)
    {
        return mode == 0 || mode == 2 || mode == 3;
    }

    bool materialHasNormalMap(const ALMetalWorldSimpleBatch& batch)
    {
        return batch.material_slot_count != 0 &&
               batch.material_slots[0].normal_texture != nullptr;
    }

    bool materialHasSpecularMap(const ALMetalWorldSimpleBatch& batch)
    {
        return batch.material_slot_count != 0 &&
               batch.material_slots[0].specular_texture != nullptr;
    }

    bool isPBRBatch(const ALMetalWorldSimpleBatch& batch)
    {
        return batch.pbr_slot_count != 0;
    }

    bool sampleableTexture(const ALMetalTexture* texture)
    {
        return texture &&
               texture->descriptor().type == ALMetalTextureType::texture_2d &&
               alMetalHasUsage(texture->descriptor().usage,
                               ALMetalTextureUsage::shader_read);
    }

    bool validReflectionProbes(
        const ALMetalWorldReflectionProbes* probes)
    {
        static_assert(sizeof(ALMetalWorldReflectionProbeConstants) == 49216);
        if (!probes || !probes->radiance_texture ||
            !finiteValues(probes->constants.config, 4) ||
            !finiteValues(&probes->constants.env_matrix[0][0], 12))
        {
            return false;
        }
        const float count_value = probes->constants.config[0];
        if (count_value < 1.f ||
            count_value > static_cast<float>(
                ALMetalWorldReflectionProbeConstants::max_probe_count) ||
            std::floor(count_value) != count_value)
        {
            return false;
        }
        const std::size_t probe_count =
            static_cast<std::size_t>(count_value);

        const ALMetalTextureDescriptor& descriptor =
            probes->radiance_texture->descriptor();
        if (descriptor.type != ALMetalTextureType::texture_cube_array ||
            (descriptor.format != ALMetalPixelFormat::rgba16_float &&
             descriptor.format != ALMetalPixelFormat::rgba8_unorm) ||
            descriptor.width == 0 || descriptor.width != descriptor.height ||
            descriptor.array_length == 0 ||
            descriptor.mipmap_level_count == 0 ||
            descriptor.sample_count != 1 ||
            !alMetalHasUsage(descriptor.usage,
                             ALMetalTextureUsage::shader_read) ||
            probes->constants.config[1] < 0.f ||
            probes->constants.config[1] >
                static_cast<float>(descriptor.mipmap_level_count - 1))
        {
            return false;
        }

        for (std::size_t index = 0; index < probe_count; ++index)
        {
            const std::int32_t cube =
                probes->constants.ref_index[index][0];
            if (cube < 0 ||
                static_cast<std::uint32_t>(cube) >=
                    descriptor.array_length ||
                !finiteValues(probes->constants.ref_box[index], 16) ||
                !finiteValues(probes->constants.ref_sphere[index], 4) ||
                !finiteValues(probes->constants.ref_params[index], 4))
            {
                return false;
            }
        }
        return true;
    }

    bool validPBRReflectionProbes(
        const ALMetalWorldReflectionProbes* probes)
    {
        if (!validReflectionProbes(probes) ||
            !probes->irradiance_texture)
        {
            return false;
        }
        const ALMetalTextureDescriptor& descriptor =
            probes->irradiance_texture->descriptor();
        return descriptor.type == ALMetalTextureType::texture_2d &&
               descriptor.format == ALMetalPixelFormat::rgba32_uint &&
               descriptor.width >= 4 &&
               descriptor.height >=
                   probes->radiance_texture->descriptor().array_length &&
               descriptor.sample_count == 1 &&
               alMetalHasUsage(descriptor.usage,
                               ALMetalTextureUsage::shader_read);
    }

    bool validLegacyAlphaLighting(
        const ALMetalWorldLegacyAlphaLighting* lighting)
    {
        if (!lighting ||
            !finiteValues(lighting->sun_direction, 4) ||
            !finiteValues(lighting->sun_color, 4) ||
            !finiteValues(lighting->parameters, 4) ||
            lighting->parameters[0] <= 0.f ||
            lighting->parameters[1] < 0.f ||
            lighting->parameters[1] > 1.f ||
            lighting->parameters[2] < 0.f ||
            lighting->parameters[2] > 1.f ||
            (lighting->parameters[3] != 0.f &&
             lighting->parameters[3] != 1.f))
        {
            return false;
        }
        const float sun_length_squared =
            lighting->sun_direction[0] * lighting->sun_direction[0] +
            lighting->sun_direction[1] * lighting->sun_direction[1] +
            lighting->sun_direction[2] * lighting->sun_direction[2];
        if (sun_length_squared <= 0.f)
        {
            return false;
        }
        for (const ALMetalWorldLegacyAlphaLight& light :
             lighting->local_lights)
        {
            if (!finiteValues(light.position, 4) ||
                !finiteValues(light.direction, 4) ||
                !finiteValues(light.diffuse, 4) ||
                !finiteValues(light.parameters, 4) ||
                light.parameters[0] < 0.f ||
                (light.parameters[0] > 0.f &&
                 light.parameters[1] <= -1.f) ||
                light.parameters[2] < 0.f ||
                light.parameters[2] > 1.f)
            {
                return false;
            }
        }
        return true;
    }

    bool validHeroProbe(const ALMetalWorldHeroProbe* hero)
    {
        if (!hero || !hero->radiance_texture ||
            !finiteValues(hero->constants.hero_box, 16) ||
            !finiteValues(hero->constants.hero_sphere, 4) ||
            !finiteValues(hero->constants.clip_plane, 4) ||
            hero->constants.config[0] > 1 ||
            hero->constants.config[1] != 1)
        {
            return false;
        }

        const ALMetalTextureDescriptor& descriptor =
            hero->radiance_texture->descriptor();
        const std::uint32_t hero_max_lod = hero->constants.config[2];
        const std::uint32_t required_levels =
            static_cast<std::uint32_t>(
                std::ceil(static_cast<float>(hero_max_lod) * 0.25f)) + 1;
        return descriptor.type == ALMetalTextureType::texture_cube &&
               (descriptor.format == ALMetalPixelFormat::rg11b10_float ||
                descriptor.format == ALMetalPixelFormat::rgba8_unorm) &&
               descriptor.width != 0 &&
               descriptor.width == descriptor.height &&
               descriptor.array_length == 1 &&
               descriptor.mipmap_level_count == required_levels &&
               descriptor.sample_count == 1 &&
               alMetalHasUsage(
                   descriptor.usage, ALMetalTextureUsage::shader_read);
    }

    bool validScreenSpaceReflections(
        const ALMetalWorldScreenSpaceReflections* reflections)
    {
        if (!reflections || !reflections->scene_color ||
            !reflections->scene_depth ||
            !finiteValues(reflections->constants.projection, 16) ||
            !finiteValues(reflections->constants.inverse_projection, 16) ||
            !finiteValues(
                reflections->constants.inverse_modelview_delta, 16) ||
            !finiteValues(reflections->constants.screen, 4) ||
            !finiteValues(reflections->constants.trace, 4) ||
            !finiteValues(reflections->constants.sampling, 4) ||
            reflections->constants.screen[0] <= 0.f ||
            reflections->constants.screen[1] <= 0.f ||
            reflections->constants.screen[2] <= 0.f ||
            reflections->constants.trace[0] < 0.f ||
            reflections->constants.sampling[0] < 1.f ||
            reflections->constants.sampling[0] > 128.f ||
            reflections->constants.config[0] > 1 ||
            reflections->constants.config[1] != 1)
        {
            return false;
        }

        const ALMetalTextureDescriptor& color =
            reflections->scene_color->descriptor();
        const ALMetalTextureDescriptor& depth =
            reflections->scene_depth->descriptor();
        const bool color_format =
            color.format == ALMetalPixelFormat::rg11b10_float ||
            color.format == ALMetalPixelFormat::rgba8_unorm;
        return color.type == ALMetalTextureType::texture_2d &&
               depth.type == ALMetalTextureType::texture_2d &&
               color_format &&
               depth.format == ALMetalPixelFormat::r32_float &&
               color.width != 0 && color.height != 0 &&
               color.width == depth.width &&
               color.height == depth.height &&
               color.array_length == 1 && depth.array_length == 1 &&
               color.mipmap_level_count == 1 &&
               depth.mipmap_level_count == 1 &&
               color.sample_count == 1 && depth.sample_count == 1 &&
               alMetalHasUsage(
                   color.usage, ALMetalTextureUsage::shader_read) &&
               alMetalHasUsage(
                   depth.usage, ALMetalTextureUsage::shader_read);
    }

    bool validDirectionalShadows(
        const ALMetalWorldDirectionalShadows* shadows)
    {
        if (!shadows || !shadows->depth_texture)
        {
            return false;
        }
        const ALMetalTextureDescriptor& descriptor =
            shadows->depth_texture->descriptor();
        const ALMetalWorldDirectionalShadowConstants& constants =
            shadows->constants;
        return descriptor.type == ALMetalTextureType::texture_2d_array &&
               descriptor.format == ALMetalPixelFormat::r32_float &&
               descriptor.array_length == 4 &&
               descriptor.mipmap_level_count == 1 &&
               alMetalHasUsage(
                   descriptor.usage, ALMetalTextureUsage::shader_read) &&
               finiteValues(&constants.shadow_matrix[0][0], 4 * 16) &&
               finiteValues(constants.shadow_clip, 4) &&
               finiteValues(constants.resolution, 4) &&
               finiteValues(constants.parameters, 4) &&
               constants.resolution[0] == descriptor.width &&
               constants.resolution[1] == descriptor.height &&
               constants.resolution[2] > 0.f &&
               constants.resolution[3] > 0.f &&
               constants.parameters[3] > 0.f &&
               constants.config[0] >= 1 && constants.config[0] <= 4 &&
               (constants.config[1] & ~0xfu) == 0 &&
               constants.config[2] <= 1 && constants.config[3] <= 3 &&
               (constants.config[3] < 3 || constants.parameters[2] > 0.f);
    }

    bool validProjectorShadows(
        const ALMetalWorldDirectionalShadows* shadows)
    {
        if (!shadows || !shadows->depth_texture)
        {
            return false;
        }
        const ALMetalTextureDescriptor& descriptor =
            shadows->depth_texture->descriptor();
        const ALMetalWorldDirectionalShadowConstants& constants =
            shadows->constants;
        return descriptor.type == ALMetalTextureType::texture_2d_array &&
               descriptor.format == ALMetalPixelFormat::r32_float &&
               descriptor.width != 0 && descriptor.height != 0 &&
               descriptor.array_length == 2 &&
               descriptor.mipmap_level_count == 1 &&
               descriptor.sample_count == 1 &&
               alMetalHasUsage(
                   descriptor.usage, ALMetalTextureUsage::shader_read) &&
               finiteValues(&constants.shadow_matrix[0][0], 2 * 16) &&
               finiteValues(constants.shadow_clip, 4) &&
               finiteValues(constants.resolution, 4) &&
               finiteValues(constants.parameters, 4) &&
               constants.resolution[0] == descriptor.width &&
               constants.resolution[1] == descriptor.height &&
               constants.resolution[2] > 0.f &&
               constants.resolution[3] > 0.f &&
               constants.parameters[3] > 0.f &&
               constants.config[0] == 2 &&
               (constants.config[1] & ~0x3u) == 0 &&
               constants.config[2] <= 1 && constants.config[3] <= 3 &&
               (constants.config[3] < 3 || constants.parameters[2] > 0.f);
    }

}

struct ALMetalWorldGBufferRenderer::Impl
{
    using NormalPipelines =
        std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>;
    using RiggedPipelines = std::array<NormalPipelines, 2>;
    using MaskPipelines = std::array<RiggedPipelines, 2>;
    using BumpPipelines = std::array<MaskPipelines, 2>;
    using MaterialRiggedPipelines = std::array<NormalPipelines, 2>;
    using AvatarVariantPipelines = std::array<NormalPipelines, 3>;
    using TerrainPartitionPipelines = std::array<NormalPipelines, 2>;
    using OpaqueForwardRiggedPipelines =
        std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>;
    using OpaqueForwardMaskPipelines =
        std::array<OpaqueForwardRiggedPipelines, 2>;

    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::array<BumpPipelines, 2> pipelines;
    std::array<MaterialRiggedPipelines, 2> material_pipelines;
    std::array<MaterialRiggedPipelines, 2> pbr_pipelines;
    std::array<MaskPipelines, 2> fullbright_pipelines;
    std::array<RiggedPipelines, 2> shiny_pipelines;
    std::array<RiggedPipelines, 2> emboss_pipelines;
    std::array<MaterialRiggedPipelines, 2> flat_alpha_pipelines;
    std::array<MaterialRiggedPipelines, 2> pbr_alpha_pipelines;
    std::array<MaterialRiggedPipelines, 2> legacy_alpha_pipelines;
    std::array<AvatarVariantPipelines, 2> avatar_pipelines;
    std::array<NormalPipelines, 2> impostor_pipelines;
    std::array<NormalPipelines, 2> coverage_pipelines;
    std::array<TerrainPartitionPipelines, 2> terrain_pipelines;
    std::array<TerrainPartitionPipelines, 2> pbr_terrain_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        local_light_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        projector_light_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        lighting_composition_pipelines;
    std::array<OpaqueForwardMaskPipelines, 2>
        opaque_forward_fullbright_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        opaque_forward_shiny_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        opaque_forward_emboss_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        water_exclusion_pipelines;
    std::array<std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>, 2>
        water_haze_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        pre_water_fullbright_pipelines;
    std::array<std::array<OpaqueForwardRiggedPipelines, 2>, 2>
        pre_water_simple_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        pre_water_material_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        legacy_glow_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        pbr_glow_pipelines;
    std::unique_ptr<ALMetalRenderPipeline> light_map_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> light_map_blur_pipeline;
    std::array<NormalPipelines, 2> hdri_sky_pipelines;
    std::array<NormalPipelines, 2> windlight_sky_pipelines;
    std::array<NormalPipelines, 2> windlight_star_pipelines;
    std::array<NormalPipelines, 2> windlight_sun_pipelines;
    std::array<NormalPipelines, 2> windlight_moon_pipelines;
    std::array<NormalPipelines, 2> windlight_meteor_pipelines;
    std::array<NormalPipelines, 2> windlight_aurora_pipelines;
    std::array<NormalPipelines, 2> windlight_cloud_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        base_light_pipelines;
    std::unique_ptr<ALMetalComputePipeline> brdf_lut_pipeline;
    std::unique_ptr<ALMetalDepthStencilState> forward_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_depth;
    std::unique_ptr<ALMetalDepthStencilState> forward_readonly_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_readonly_depth;
    std::unique_ptr<ALMetalDepthStencilState> forward_coverage_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_coverage_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        forward_water_exclusion_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        reverse_water_exclusion_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        forward_water_haze_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        reverse_water_haze_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        underwater_water_haze_depth;
    std::unique_ptr<ALMetalSamplerState> diffuse_sampler;
    std::unique_ptr<ALMetalSamplerState> probe_sampler;
    std::unique_ptr<ALMetalSamplerState> point_clamp_sampler;
    std::unique_ptr<ALMetalSamplerState> point_repeat_sampler;
    std::unique_ptr<ALMetalSamplerState> linear_clamp_sampler;
    std::unique_ptr<ALMetalSamplerState> ramp_sampler;
    std::unique_ptr<ALMetalSamplerState> hdri_sampler;
    std::unique_ptr<ALMetalTexture> brdf_lut;
    bool brdf_lut_ready = false;
    std::unique_ptr<ALMetalTexture> unshadowed_depth;
    ALMetalWorldDirectionalShadowConstants unshadowed_constants{};
    std::unique_ptr<ALMetalTexture> inactive_hero_probe;
    ALMetalWorldHeroProbeConstants inactive_hero_constants{};
    std::unique_ptr<ALMetalTexture> inactive_ssr_color;
    std::unique_ptr<ALMetalTexture> inactive_ssr_depth;
    ALMetalWorldScreenSpaceReflectionConstants inactive_ssr_constants{};
};

ALMetalWorldGBufferRenderer::ALMetalWorldGBufferRenderer(
    std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalWorldGBufferRenderer::~ALMetalWorldGBufferRenderer() = default;

std::unique_ptr<ALMetalWorldGBufferRenderer>
ALMetalWorldGBufferRenderer::create(const ALMetalDevice& device,
                                    std::string* error)
{
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->library = ALMetalShaderLibrary::loadBuiltin(device, error);
    if (!impl->library ||
        !impl->library->hasFunction("almetal_world_simple_vertex") ||
        !impl->library->hasFunction("almetal_world_simple_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_bump_vertex") ||
        !impl->library->hasFunction("almetal_world_bump_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_material_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_material_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_pbr_vertex") ||
        !impl->library->hasFunction("almetal_world_pbr_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_avatar_rigid_vertex") ||
        !impl->library->hasFunction("almetal_world_avatar_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_avatar_cloth_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_impostor_vertex") ||
        !impl->library->hasFunction("almetal_world_coverage_vertex") ||
        !impl->library->hasFunction("almetal_world_terrain_vertex") ||
        !impl->library->hasFunction("almetal_world_simple_fragment") ||
        !impl->library->hasFunction("almetal_world_alpha_mask_fragment") ||
        !impl->library->hasFunction("almetal_world_bump_fragment") ||
        !impl->library->hasFunction("almetal_world_material_fragment") ||
        !impl->library->hasFunction("almetal_world_pbr_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_fullbright_fragment") ||
        !impl->library->hasFunction("almetal_world_shiny_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_shiny_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_shiny_fragment") ||
        !impl->library->hasFunction("almetal_world_emboss_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_emboss_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_emboss_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_water_exclusion_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_water_exclusion_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_water_haze_field_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_water_haze_fullscreen_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_water_haze_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pre_water_fullbright_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pre_water_simple_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pre_water_simple_shadow_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pre_water_material_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_glow_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_glow_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_glow_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pbr_glow_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_pbr_glow_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_pbr_glow_fragment") ||
        !impl->library->hasFunction("almetal_world_flat_alpha_fragment") ||
        !impl->library->hasFunction("almetal_world_pbr_alpha_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_alpha_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_alpha_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_alpha_fragment") ||
        !impl->library->hasFunction("almetal_world_impostor_fragment") ||
        !impl->library->hasFunction("almetal_world_coverage_fragment") ||
        !impl->library->hasFunction("almetal_world_terrain_fragment") ||
        !impl->library->hasFunction("almetal_world_pbr_terrain_fragment") ||
        !impl->library->hasFunction("almetal_world_local_light_vertex") ||
        !impl->library->hasFunction("almetal_world_local_light_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_projector_light_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_lighting_composition_fragment") ||
        !impl->library->hasFunction("almetal_world_light_map_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_light_map_blur_fragment") ||
        !impl->library->hasFunction("almetal_world_hdri_sky_vertex") ||
        !impl->library->hasFunction("almetal_world_hdri_sky_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_windlight_sky_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_windlight_sky_fragment") ||
        !impl->library->hasFunction("almetal_world_star_vertex") ||
        !impl->library->hasFunction("almetal_world_star_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_heavenly_body_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_heavenly_body_fragment") ||
        !impl->library->hasFunction("almetal_world_meteor_vertex") ||
        !impl->library->hasFunction("almetal_world_meteor_fragment") ||
        !impl->library->hasFunction("almetal_world_aurora_vertex") ||
        !impl->library->hasFunction("almetal_world_aurora_fragment") ||
        !impl->library->hasFunction("almetal_world_cloud_vertex") ||
        !impl->library->hasFunction("almetal_world_cloud_fragment") ||
        !impl->library->hasFunction("almetal_world_base_light_fragment") ||
        !impl->library->hasFunction("almetal_pbr_preview_brdf_lut"))
    {
        if (impl->library)
        {
            setError(error,
                     "Built-in Metal library is missing world G-buffer functions");
        }
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor light_map_descriptor;
    light_map_descriptor.library = impl->library.get();
    light_map_descriptor.vertex_function =
        "almetal_world_local_light_vertex";
    light_map_descriptor.fragment_function =
        "almetal_world_light_map_fragment";
    light_map_descriptor.colors[0].format = ALMetalPixelFormat::rg8_unorm;
    light_map_descriptor.label = "Alchemy world shadow/SSAO light-map pipeline";
    impl->light_map_pipeline = ALMetalRenderPipeline::create(
        device, light_map_descriptor, error);
    if (!impl->light_map_pipeline)
    {
        return nullptr;
    }
    light_map_descriptor.fragment_function =
        "almetal_world_light_map_blur_fragment";
    light_map_descriptor.label = "Alchemy world SSAO bilateral-blur pipeline";
    impl->light_map_blur_pipeline = ALMetalRenderPipeline::create(
        device, light_map_descriptor, error);
    if (!impl->light_map_blur_pipeline)
    {
        return nullptr;
    }

    constexpr std::array<ALMetalPixelFormat, 2> emissive_formats{{
        ALMetalPixelFormat::rgba8_unorm,
        ALMetalPixelFormat::rg11b10_float,
    }};
    constexpr std::array<ALMetalPixelFormat, 2> screen_formats{{
        ALMetalPixelFormat::rgba8_unorm,
        ALMetalPixelFormat::rgba16_float,
    }};
    for (std::size_t hdr = 0; hdr < screen_formats.size(); ++hdr)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_world_local_light_vertex";
        descriptor.fragment_function = "almetal_world_local_light_fragment";
        descriptor.colors[0].format = screen_formats[hdr];
        descriptor.label = hdr
            ? "Alchemy HDR world local-point-light pipeline"
            : "Alchemy world local-point-light pipeline";
        impl->local_light_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->local_light_pipelines[hdr])
        {
            return nullptr;
        }

        descriptor.fragment_function =
            "almetal_world_projector_light_fragment";
        descriptor.colors[0].blending_enabled = true;
        descriptor.colors[0].source_rgb = ALMetalBlendFactor::one;
        descriptor.colors[0].destination_rgb = ALMetalBlendFactor::one;
        descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
        descriptor.colors[0].destination_alpha = ALMetalBlendFactor::one;
        descriptor.label = hdr
            ? "Alchemy HDR world projector-light pipeline"
            : "Alchemy world projector-light pipeline";
        impl->projector_light_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->projector_light_pipelines[hdr])
        {
            return nullptr;
        }

        // The following owners replace the fragment function and do not
        // accumulate multiple draws into one target.
        descriptor.colors[0].blending_enabled = false;
        descriptor.colors[0].destination_rgb = ALMetalBlendFactor::zero;
        descriptor.colors[0].destination_alpha = ALMetalBlendFactor::zero;

        descriptor.fragment_function =
            "almetal_world_lighting_composition_fragment";
        descriptor.label = hdr
            ? "Alchemy HDR world lighting-composition pipeline"
            : "Alchemy world lighting-composition pipeline";
        impl->lighting_composition_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->lighting_composition_pipelines[hdr])
        {
            return nullptr;
        }

        descriptor.fragment_function = "almetal_world_base_light_fragment";
        descriptor.label = hdr
            ? "Alchemy HDR world base-light pipeline"
            : "Alchemy world base-light pipeline";
        impl->base_light_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->base_light_pipelines[hdr])
        {
            return nullptr;
        }

        for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
        {
            ALMetalRenderPipelineDescriptor sky_descriptor;
            sky_descriptor.library = impl->library.get();
            sky_descriptor.vertex_function =
                "almetal_world_hdri_sky_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_hdri_sky_fragment";
            sky_descriptor.color_attachment_count = 4;
            sky_descriptor.colors[0].format =
                ALMetalPixelFormat::rgba8_unorm_srgb;
            sky_descriptor.colors[1].format =
                ALMetalPixelFormat::rgba8_unorm;
            sky_descriptor.colors[2].format = normal_hdr
                ? ALMetalPixelFormat::rgba16_unorm
                : ALMetalPixelFormat::rgb10a2_unorm;
            sky_descriptor.colors[3].format = emissive_formats[hdr];
            sky_descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            sky_descriptor.label = hdr
                ? "Alchemy HDRI sky G-buffer pipeline"
                : "Alchemy vintage HDRI sky G-buffer pipeline";
            impl->hdri_sky_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->hdri_sky_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.fragment_function =
                "almetal_world_windlight_sky_fragment";
            sky_descriptor.vertex_function =
                "almetal_world_windlight_sky_vertex";
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight sky G-buffer pipeline"
                : "Alchemy vintage WindLight sky G-buffer pipeline";
            impl->windlight_sky_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_sky_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_star_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_star_fragment";
            // The vintage GL owner disables framebuffer sRGB and adds star
            // values directly to the encoded albedo bytes. Its Metal pass
            // therefore targets the compatible linear UNORM view. HDR writes
            // stars to emissive and keeps the ordinary sRGB albedo view.
            if (hdr == 0)
            {
                sky_descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm;
            }
            for (std::size_t attachment = 0; attachment < 4;
                 ++attachment)
            {
                sky_descriptor.colors[attachment].blending_enabled = true;
                sky_descriptor.colors[attachment].source_rgb =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_rgb =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].source_alpha =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_alpha =
                    ALMetalBlendFactor::one;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight star G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage star G-buffer pipeline";
            impl->windlight_star_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_star_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_heavenly_body_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_heavenly_body_fragment";
            for (std::size_t attachment = 0; attachment < 4;
                 ++attachment)
            {
                sky_descriptor.colors[attachment].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                sky_descriptor.colors[attachment].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                sky_descriptor.colors[attachment].source_alpha =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight sun G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage sun G-buffer pipeline";
            impl->windlight_sun_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_sun_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight moon G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage moon G-buffer pipeline";
            impl->windlight_moon_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_moon_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_meteor_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_meteor_fragment";
            for (std::size_t attachment = 0; attachment < 4;
                 ++attachment)
            {
                sky_descriptor.colors[attachment].source_rgb =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_rgb =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].source_alpha =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_alpha =
                    ALMetalBlendFactor::one;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight meteor G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage meteor G-buffer pipeline";
            impl->windlight_meteor_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_meteor_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_aurora_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_aurora_fragment";
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight aurora G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage aurora G-buffer pipeline";
            impl->windlight_aurora_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_aurora_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_cloud_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_cloud_fragment";
            for (std::size_t attachment = 0; attachment < 4;
                 ++attachment)
            {
                sky_descriptor.colors[attachment].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                sky_descriptor.colors[attachment].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                sky_descriptor.colors[attachment].source_alpha =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight cloud G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage cloud G-buffer pipeline";
            impl->windlight_cloud_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_cloud_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }
        }
    }

    ALMetalComputePipelineDescriptor brdf_descriptor;
    brdf_descriptor.library = impl->library.get();
    brdf_descriptor.function = "almetal_pbr_preview_brdf_lut";
    brdf_descriptor.label = "Alchemy world PBR BRDF LUT pipeline";
    impl->brdf_lut_pipeline = ALMetalComputePipeline::create(
        device, brdf_descriptor, error);
    if (!impl->brdf_lut_pipeline)
    {
        return nullptr;
    }

    constexpr std::uint32_t brdf_lut_size = 512;
    ALMetalTextureDescriptor brdf_texture;
    brdf_texture.format = ALMetalPixelFormat::rg16_float;
    brdf_texture.width = brdf_lut_size;
    brdf_texture.height = brdf_lut_size;
    brdf_texture.storage = ALMetalStorageMode::private_gpu;
    brdf_texture.usage = ALMetalTextureUsage::shader_read |
                         ALMetalTextureUsage::shader_write;
    brdf_texture.label = "Alchemy world PBR BRDF LUT";
    impl->brdf_lut = ALMetalTexture::create(device, brdf_texture, error);
    if (!impl->brdf_lut)
    {
        return nullptr;
    }
    constexpr std::array<const char*, 3> avatar_vertex_functions{{
        "almetal_world_avatar_rigid_vertex",
        "almetal_world_avatar_skinned_vertex",
        "almetal_world_avatar_cloth_skinned_vertex",
    }};
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t alpha_mask = 0; alpha_mask < 2; ++alpha_mask)
        {
            for (std::size_t rigged = 0; rigged < 2; ++rigged)
            {
                for (std::size_t normal_hdr = 0; normal_hdr < 2;
                     ++normal_hdr)
                {
                    ALMetalRenderPipelineDescriptor descriptor;
                    descriptor.library = impl->library.get();
                    descriptor.vertex_function = rigged
                        ? "almetal_world_simple_skinned_vertex"
                        : "almetal_world_simple_vertex";
                    descriptor.fragment_function =
                        "almetal_world_fullbright_fragment";
                    descriptor.color_attachment_count = 4;
                    descriptor.colors[0].format =
                        ALMetalPixelFormat::rgba8_unorm_srgb;
                    descriptor.colors[0].blending_enabled = !alpha_mask;
                    descriptor.colors[0].source_rgb =
                        ALMetalBlendFactor::source_alpha;
                    descriptor.colors[0].destination_rgb =
                        ALMetalBlendFactor::one_minus_source_alpha;
                    descriptor.colors[0].source_alpha =
                        ALMetalBlendFactor::one;
                    descriptor.colors[0].destination_alpha =
                        ALMetalBlendFactor::one_minus_source_alpha;
                    descriptor.colors[1].format =
                        ALMetalPixelFormat::rgba8_unorm;
                    descriptor.colors[1].write_mask =
                        ALMetalColorWriteMask::none;
                    descriptor.colors[2].format = normal_hdr
                        ? ALMetalPixelFormat::rgba16_unorm
                        : ALMetalPixelFormat::rgb10a2_unorm;
                    descriptor.colors[2].write_mask =
                        ALMetalColorWriteMask::none;
                    descriptor.colors[3].format = emissive_formats[hdr];
                    descriptor.colors[3].write_mask =
                        ALMetalColorWriteMask::none;
                    descriptor.depth_stencil_format =
                        ALMetalPixelFormat::depth32_float_stencil8;
                    descriptor.label = alpha_mask
                        ? "Alchemy world fullbright-mask impostor pipeline"
                        : "Alchemy world fullbright impostor pipeline";
                    auto& pipeline =
                        impl->fullbright_pipelines[hdr][alpha_mask]
                                                   [rigged][normal_hdr];
                    pipeline = ALMetalRenderPipeline::create(
                        device, descriptor, error);
                    if (!pipeline)
                    {
                        return nullptr;
                    }

                    if (normal_hdr == 0)
                    {
                        descriptor.color_attachment_count = 1;
                        descriptor.colors[0].format = screen_formats[hdr];
                        descriptor.label = alpha_mask
                            ? "Alchemy world final-color fullbright-mask pipeline"
                            : "Alchemy world final-color fullbright pipeline";
                        auto& opaque_pipeline =
                            impl->opaque_forward_fullbright_pipelines[hdr]
                                                                      [alpha_mask]
                                                                      [rigged];
                        opaque_pipeline = ALMetalRenderPipeline::create(
                            device, descriptor, error);
                        if (!opaque_pipeline)
                        {
                            return nullptr;
                        }
                    }
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < screen_formats.size(); ++hdr)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function =
            "almetal_world_water_exclusion_vertex";
        descriptor.fragment_function =
            "almetal_world_water_exclusion_fragment";
        descriptor.color_attachment_count = 1;
        descriptor.colors[0].format = screen_formats[hdr];
        descriptor.colors[0].write_mask = ALMetalColorWriteMask::none;
        descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.label = "Alchemy world water-exclusion stencil pipeline";
        impl->water_exclusion_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->water_exclusion_pipelines[hdr])
        {
            return nullptr;
        }

        for (std::size_t above_water = 0; above_water < 2;
             ++above_water)
        {
            ALMetalRenderPipelineDescriptor haze_descriptor;
            haze_descriptor.library = impl->library.get();
            haze_descriptor.vertex_function = above_water
                ? "almetal_world_water_haze_field_vertex"
                : "almetal_world_water_haze_fullscreen_vertex";
            haze_descriptor.fragment_function =
                "almetal_world_water_haze_fragment";
            haze_descriptor.color_attachment_count = 1;
            haze_descriptor.colors[0].format = screen_formats[hdr];
            haze_descriptor.colors[0].blending_enabled = true;
            haze_descriptor.colors[0].source_rgb =
                ALMetalBlendFactor::one;
            haze_descriptor.colors[0].destination_rgb =
                ALMetalBlendFactor::source_alpha;
            haze_descriptor.colors[0].source_alpha =
                ALMetalBlendFactor::zero;
            haze_descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::source_alpha;
            haze_descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            haze_descriptor.label = above_water
                ? "Alchemy world above-water haze pipeline"
                : "Alchemy world underwater haze pipeline";
            impl->water_haze_pipelines[hdr][above_water] =
                ALMetalRenderPipeline::create(
                    device, haze_descriptor, error);
            if (!impl->water_haze_pipelines[hdr][above_water])
            {
                return nullptr;
            }
        }

        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            ALMetalRenderPipelineDescriptor alpha_descriptor;
            alpha_descriptor.library = impl->library.get();
            alpha_descriptor.vertex_function = rigged
                ? "almetal_world_shiny_skinned_vertex"
                : "almetal_world_shiny_vertex";
            alpha_descriptor.fragment_function =
                "almetal_world_pre_water_fullbright_fragment";
            alpha_descriptor.color_attachment_count = 1;
            alpha_descriptor.colors[0].format = screen_formats[hdr];
            alpha_descriptor.colors[0].blending_enabled = true;
            alpha_descriptor.colors[0].source_rgb =
                ALMetalBlendFactor::source_alpha;
            alpha_descriptor.colors[0].destination_rgb =
                ALMetalBlendFactor::one_minus_source_alpha;
            alpha_descriptor.colors[0].source_alpha =
                ALMetalBlendFactor::zero;
            alpha_descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::one_minus_source_alpha;
            alpha_descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            alpha_descriptor.label =
                "Alchemy world pre-water fullbright-alpha pipeline";
            impl->pre_water_fullbright_pipelines[hdr][rigged] =
                ALMetalRenderPipeline::create(
                    device, alpha_descriptor, error);
            if (!impl->pre_water_fullbright_pipelines[hdr][rigged])
            {
                return nullptr;
            }
            for (std::size_t shadows = 0; shadows < 2; ++shadows)
            {
                alpha_descriptor.fragment_function = shadows
                    ? "almetal_world_pre_water_simple_shadow_fragment"
                    : "almetal_world_pre_water_simple_fragment";
                alpha_descriptor.label = shadows
                    ? "Alchemy world pre-water simple-alpha shadow pipeline"
                    : "Alchemy world pre-water simple-alpha pipeline";
                impl->pre_water_simple_pipelines[hdr][shadows][rigged] =
                    ALMetalRenderPipeline::create(
                        device, alpha_descriptor, error);
                if (!impl->pre_water_simple_pipelines
                        [hdr][shadows][rigged])
                {
                    return nullptr;
                }
            }

            alpha_descriptor.vertex_function = rigged
                ? "almetal_world_legacy_alpha_skinned_vertex"
                : "almetal_world_legacy_alpha_vertex";
            alpha_descriptor.fragment_function =
                "almetal_world_pre_water_material_fragment";
            alpha_descriptor.label =
                "Alchemy world pre-water legacy-material alpha pipeline";
            impl->pre_water_material_pipelines[hdr][rigged] =
                ALMetalRenderPipeline::create(
                    device, alpha_descriptor, error);
            if (!impl->pre_water_material_pipelines[hdr][rigged])
            {
                return nullptr;
            }
        }

        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            ALMetalRenderPipelineDescriptor glow_descriptor;
            glow_descriptor.library = impl->library.get();
            glow_descriptor.vertex_function = rigged
                ? "almetal_world_legacy_glow_skinned_vertex"
                : "almetal_world_legacy_glow_vertex";
            glow_descriptor.fragment_function =
                "almetal_world_legacy_glow_fragment";
            glow_descriptor.color_attachment_count = 1;
            glow_descriptor.colors[0].format = screen_formats[hdr];
            glow_descriptor.colors[0].blending_enabled = true;
            glow_descriptor.colors[0].source_alpha =
                ALMetalBlendFactor::one;
            glow_descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::one;
            glow_descriptor.colors[0].write_mask =
                ALMetalColorWriteMask::alpha;
            glow_descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            glow_descriptor.label =
                "Alchemy world legacy additive-glow pipeline";
            impl->legacy_glow_pipelines[hdr][rigged] =
                ALMetalRenderPipeline::create(
                    device, glow_descriptor, error);
            if (!impl->legacy_glow_pipelines[hdr][rigged])
            {
                return nullptr;
            }

            glow_descriptor.vertex_function = rigged
                ? "almetal_world_pbr_glow_skinned_vertex"
                : "almetal_world_pbr_glow_vertex";
            glow_descriptor.fragment_function =
                "almetal_world_pbr_glow_fragment";
            glow_descriptor.colors[0].blending_enabled = false;
            glow_descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::zero;
            glow_descriptor.label =
                "Alchemy world PBR glow-replacement pipeline";
            impl->pbr_glow_pipelines[hdr][rigged] =
                ALMetalRenderPipeline::create(
                    device, glow_descriptor, error);
            if (!impl->pbr_glow_pipelines[hdr][rigged])
            {
                return nullptr;
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_shiny_skinned_vertex"
                    : "almetal_world_shiny_vertex";
                descriptor.fragment_function = "almetal_world_shiny_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world shiny avatar-impostor pipeline";
                auto& pipeline =
                    impl->shiny_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }

                if (normal_hdr == 0)
                {
                    descriptor.color_attachment_count = 1;
                    descriptor.colors[0].format = screen_formats[hdr];
                    descriptor.label =
                        "Alchemy world final-color shiny pipeline";
                    auto& opaque_pipeline =
                        impl->opaque_forward_shiny_pipelines[hdr][rigged];
                    opaque_pipeline = ALMetalRenderPipeline::create(
                        device, descriptor, error);
                    if (!opaque_pipeline)
                    {
                        return nullptr;
                    }
                }
            }
        }
    }

    const auto rgb_write_mask = static_cast<ALMetalColorWriteMask>(
        static_cast<std::uint8_t>(ALMetalColorWriteMask::red) |
        static_cast<std::uint8_t>(ALMetalColorWriteMask::green) |
        static_cast<std::uint8_t>(ALMetalColorWriteMask::blue));
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_emboss_skinned_vertex"
                    : "almetal_world_emboss_vertex";
                descriptor.fragment_function =
                    "almetal_world_emboss_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[0].blending_enabled = true;
                descriptor.colors[0].source_rgb =
                    ALMetalBlendFactor::destination_color;
                descriptor.colors[0].destination_rgb =
                    ALMetalBlendFactor::source_color;
                descriptor.colors[0].write_mask = rgb_write_mask;
                descriptor.colors[1].format =
                    ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world emboss impostor pipeline";
                auto& pipeline =
                    impl->emboss_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }

                if (normal_hdr == 0)
                {
                    descriptor.color_attachment_count = 1;
                    descriptor.colors[0].format = screen_formats[hdr];
                    descriptor.label =
                        "Alchemy world final-color emboss pipeline";
                    auto& opaque_pipeline =
                        impl->opaque_forward_emboss_pipelines[hdr][rigged];
                    opaque_pipeline = ALMetalRenderPipeline::create(
                        device, descriptor, error);
                    if (!opaque_pipeline)
                    {
                        return nullptr;
                    }
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_legacy_alpha_skinned_vertex"
                    : "almetal_world_legacy_alpha_vertex";
                descriptor.fragment_function =
                    "almetal_world_legacy_alpha_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[0].blending_enabled = true;
                descriptor.colors[0].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                descriptor.colors[0].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
                descriptor.colors[0].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask = ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world legacy-material impostor-alpha pipeline";
                auto& pipeline =
                    impl->legacy_alpha_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_simple_skinned_vertex"
                    : "almetal_world_simple_vertex";
                descriptor.fragment_function =
                    "almetal_world_flat_alpha_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[0].blending_enabled = true;
                descriptor.colors[0].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                descriptor.colors[0].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
                descriptor.colors[0].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask = ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world flat impostor-alpha pipeline";
                auto& pipeline =
                    impl->flat_alpha_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
        {
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = impl->library.get();
            descriptor.vertex_function = "almetal_world_impostor_vertex";
            descriptor.fragment_function = "almetal_world_impostor_fragment";
            descriptor.color_attachment_count = 4;
            descriptor.colors[0].format =
                ALMetalPixelFormat::rgba8_unorm_srgb;
            descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
            descriptor.colors[2].format = normal_hdr
                ? ALMetalPixelFormat::rgba16_unorm
                : ALMetalPixelFormat::rgb10a2_unorm;
            descriptor.colors[3].format = emissive_formats[hdr];
            descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            descriptor.label = "Alchemy world impostor G-buffer pipeline";
            impl->impostor_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(device, descriptor, error);
            if (!impl->impostor_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }
        }
    }
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
        {
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = impl->library.get();
            descriptor.vertex_function = "almetal_world_coverage_vertex";
            descriptor.fragment_function = "almetal_world_coverage_fragment";
            descriptor.color_attachment_count = 4;
            descriptor.colors[0].format =
                ALMetalPixelFormat::rgba8_unorm_srgb;
            descriptor.colors[0].write_mask = ALMetalColorWriteMask::alpha;
            descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
            descriptor.colors[1].write_mask = ALMetalColorWriteMask::none;
            descriptor.colors[2].format = normal_hdr
                ? ALMetalPixelFormat::rgba16_unorm
                : ALMetalPixelFormat::rgb10a2_unorm;
            descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
            descriptor.colors[3].format = emissive_formats[hdr];
            descriptor.colors[3].write_mask = ALMetalColorWriteMask::none;
            descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            descriptor.label = "Alchemy world opaque-coverage pipeline";
            impl->coverage_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(device, descriptor, error);
            if (!impl->coverage_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }
        }
    }
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t pinned = 0; pinned < 2; ++pinned)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = "almetal_world_terrain_vertex";
                descriptor.fragment_function = "almetal_world_terrain_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.tessellation.enabled = true;
                descriptor.tessellation.partition_mode = pinned
                    ? ALMetalTessellationPartitionMode::integer
                    : ALMetalTessellationPartitionMode::fractional_odd;
                descriptor.tessellation.maximum_factor = 64;
                descriptor.tessellation.factor_step_function =
                    ALMetalTessellationFactorStepFunction::per_patch;
                descriptor.tessellation.output_winding =
                    ALMetalTessellationOutputWinding::counter_clockwise;
                descriptor.label = "Alchemy world terrain G-buffer pipeline";
                auto& pipeline = impl->terrain_pipelines[hdr][pinned][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }

                descriptor.fragment_function =
                    "almetal_world_pbr_terrain_fragment";
                descriptor.label =
                    "Alchemy world PBR terrain G-buffer pipeline";
                auto& pbr_pipeline =
                    impl->pbr_terrain_pipelines[hdr][pinned][normal_hdr];
                pbr_pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pbr_pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t variant = 0;
             variant < avatar_vertex_functions.size(); ++variant)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = avatar_vertex_functions[variant];
                descriptor.fragment_function =
                    "almetal_world_alpha_mask_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format =
                    ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label = "Alchemy world avatar G-buffer pipeline";
                auto& pipeline =
                    impl->avatar_pipelines[hdr][variant][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_pbr_skinned_vertex"
                    : "almetal_world_pbr_vertex";
                descriptor.fragment_function = "almetal_world_pbr_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format =
                    ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label = "Alchemy world PBR G-buffer pipeline";
                auto& pipeline = impl->pbr_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_pbr_skinned_vertex"
                    : "almetal_world_pbr_vertex";
                descriptor.fragment_function =
                    "almetal_world_pbr_alpha_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[0].blending_enabled = true;
                descriptor.colors[0].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                descriptor.colors[0].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
                descriptor.colors[0].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask = ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world PBR impostor-alpha pipeline";
                auto& pipeline =
                    impl->pbr_alpha_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_material_skinned_vertex"
                    : "almetal_world_material_vertex";
                descriptor.fragment_function =
                    "almetal_world_material_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format =
                    ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world legacy material G-buffer pipeline";
                auto& pipeline = impl->material_pipelines[hdr][rigged]
                                                         [normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t bump = 0; bump < 2; ++bump)
        {
            for (std::size_t masked = 0; masked < 2; ++masked)
            {
                for (std::size_t rigged = 0; rigged < 2; ++rigged)
                {
                    for (std::size_t normal_hdr = 0; normal_hdr < 2;
                         ++normal_hdr)
                    {
                        ALMetalRenderPipelineDescriptor descriptor;
                        descriptor.library = impl->library.get();
                        descriptor.vertex_function = bump
                            ? (rigged ? "almetal_world_bump_skinned_vertex"
                                      : "almetal_world_bump_vertex")
                            : (rigged ? "almetal_world_simple_skinned_vertex"
                                      : "almetal_world_simple_vertex");
                        descriptor.fragment_function = bump
                            ? "almetal_world_bump_fragment"
                            : (masked ? "almetal_world_alpha_mask_fragment"
                                      : "almetal_world_simple_fragment");
                        descriptor.color_attachment_count = 4;
                        descriptor.colors[0].format =
                            ALMetalPixelFormat::rgba8_unorm_srgb;
                        descriptor.colors[1].format =
                            ALMetalPixelFormat::rgba8_unorm;
                        descriptor.colors[2].format = normal_hdr
                            ? ALMetalPixelFormat::rgba16_unorm
                            : ALMetalPixelFormat::rgb10a2_unorm;
                        descriptor.colors[3].format =
                            emissive_formats[hdr];
                        descriptor.depth_stencil_format =
                            ALMetalPixelFormat::depth32_float_stencil8;
                        descriptor.label =
                            "Alchemy world G-buffer pipeline";
                        auto& pipeline = impl->pipelines[hdr][bump][masked]
                                                         [rigged][normal_hdr];
                        pipeline = ALMetalRenderPipeline::create(
                            device, descriptor, error);
                        if (!pipeline)
                        {
                            return nullptr;
                        }
                    }
                }
            }
        }
    }

    ALMetalDepthStencilDescriptor depth;
    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.depth_write_enabled = true;
    depth.label = "Alchemy forward-Z world G-buffer depth";
    impl->forward_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->forward_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z world G-buffer depth";
    impl->reverse_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->reverse_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.depth_write_enabled = false;
    depth.label = "Alchemy forward-Z world read-only depth";
    impl->forward_readonly_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->forward_readonly_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z world read-only depth";
    impl->reverse_readonly_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->reverse_readonly_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater;
    depth.depth_write_enabled = false;
    depth.label = "Alchemy forward-Z world coverage depth";
    impl->forward_coverage_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->forward_coverage_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::less;
    depth.label = "Alchemy reverse-Z world coverage depth";
    impl->reverse_coverage_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->reverse_coverage_depth)
    {
        return nullptr;
    }

    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.depth_write_enabled = false;
    depth.stencil_enabled = true;
    depth.front_stencil.compare = ALMetalCompareFunction::always;
    depth.front_stencil.stencil_failure =
        ALMetalStencilOperation::keep;
    depth.front_stencil.depth_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_stencil_pass =
        ALMetalStencilOperation::replace;
    depth.front_stencil.read_mask = 0u;
    depth.front_stencil.write_mask = 1u;
    depth.back_stencil = depth.front_stencil;
    depth.label = "Alchemy forward-Z water-exclusion stencil";
    impl->forward_water_exclusion_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->forward_water_exclusion_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z water-exclusion stencil";
    impl->reverse_water_exclusion_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->reverse_water_exclusion_depth)
    {
        return nullptr;
    }

    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.depth_write_enabled = false;
    depth.stencil_enabled = true;
    depth.front_stencil.compare = ALMetalCompareFunction::equal;
    depth.front_stencil.stencil_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_stencil_pass = ALMetalStencilOperation::keep;
    depth.front_stencil.read_mask = 1u;
    depth.front_stencil.write_mask = 0u;
    depth.back_stencil = depth.front_stencil;
    depth.label = "Alchemy forward-Z above-water haze depth and stencil";
    impl->forward_water_haze_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->forward_water_haze_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z above-water haze depth and stencil";
    impl->reverse_water_haze_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->reverse_water_haze_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::always;
    depth.stencil_enabled = false;
    depth.label = "Alchemy underwater water-haze depth";
    impl->underwater_water_haze_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->underwater_water_haze_depth)
    {
        return nullptr;
    }

    ALMetalSamplerDescriptor sampler;
    sampler.min_filter = ALMetalSamplerFilter::linear;
    sampler.mag_filter = ALMetalSamplerFilter::linear;
    sampler.mip_filter = ALMetalSamplerMipFilter::linear;
    sampler.address_s = ALMetalSamplerAddressMode::repeat;
    sampler.address_t = ALMetalSamplerAddressMode::repeat;
    sampler.address_r = ALMetalSamplerAddressMode::repeat;
    sampler.max_anisotropy = 16;
    sampler.label = "Alchemy world diffuse anisotropic wrap sampler";
    impl->diffuse_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->diffuse_sampler)
    {
        return nullptr;
    }

    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.label = "Alchemy world reflection-probe anisotropic clamp sampler";
    impl->probe_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->probe_sampler)
    {
        return nullptr;
    }

    sampler.min_filter = ALMetalSamplerFilter::nearest;
    sampler.mag_filter = ALMetalSamplerFilter::nearest;
    sampler.mip_filter = ALMetalSamplerMipFilter::none;
    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.max_anisotropy = 1;
    sampler.label = "Alchemy world terrain point clamp sampler";
    impl->point_clamp_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->point_clamp_sampler)
    {
        return nullptr;
    }

    sampler.address_s = ALMetalSamplerAddressMode::repeat;
    sampler.address_t = ALMetalSamplerAddressMode::repeat;
    sampler.address_r = ALMetalSamplerAddressMode::repeat;
    sampler.label = "Alchemy world point repeat sampler";
    impl->point_repeat_sampler = ALMetalSamplerState::create(
        device, sampler, error);
    if (!impl->point_repeat_sampler)
    {
        return nullptr;
    }

    sampler.min_filter = ALMetalSamplerFilter::linear;
    sampler.mag_filter = ALMetalSamplerFilter::linear;
    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.label = "Alchemy world terrain linear clamp sampler";
    impl->linear_clamp_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->linear_clamp_sampler)
    {
        return nullptr;
    }

    sampler.mip_filter = ALMetalSamplerMipFilter::linear;
    sampler.max_anisotropy = 16;
    sampler.label = "Alchemy world terrain ramp anisotropic clamp sampler";
    impl->ramp_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->ramp_sampler)
    {
        return nullptr;
    }

    sampler.min_filter = ALMetalSamplerFilter::linear;
    sampler.mag_filter = ALMetalSamplerFilter::linear;
    sampler.mip_filter = ALMetalSamplerMipFilter::linear;
    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.max_anisotropy = 1;
    sampler.label = "Alchemy world HDRI trilinear clamp sampler";
    impl->hdri_sampler = ALMetalSamplerState::create(
        device, sampler, error);
    if (!impl->hdri_sampler)
    {
        return nullptr;
    }

    ALMetalTextureDescriptor unshadowed_descriptor;
    unshadowed_descriptor.type = ALMetalTextureType::texture_2d_array;
    unshadowed_descriptor.format = ALMetalPixelFormat::r32_float;
    unshadowed_descriptor.width = 1;
    unshadowed_descriptor.height = 1;
    unshadowed_descriptor.array_length = 4;
    unshadowed_descriptor.storage = ALMetalStorageMode::private_gpu;
    unshadowed_descriptor.usage = ALMetalTextureUsage::shader_read;
    unshadowed_descriptor.label =
        "Alchemy world unshadowed directional fallback";
    impl->unshadowed_depth = ALMetalTexture::create(
        device, unshadowed_descriptor, error);
    auto unshadowed_upload = impl->unshadowed_depth
        ? device.makeCommandBuffer(
              "Alchemy world unshadowed directional fallback upload",
              error)
        : nullptr;
    const float fully_lit_depth = 1.f;
    bool unshadowed_ready = unshadowed_upload != nullptr;
    for (std::uint32_t cascade = 0;
         unshadowed_ready && cascade < 4; ++cascade)
    {
        unshadowed_ready = impl->unshadowed_depth->upload(
            *unshadowed_upload, 0, cascade, {}, &fully_lit_depth,
            sizeof(fully_lit_depth), error);
    }
    unshadowed_ready = unshadowed_ready &&
        unshadowed_upload->commitAndWait(error);
    if (!unshadowed_ready)
    {
        return nullptr;
    }
    impl->unshadowed_constants.resolution[0] = 1.f;
    impl->unshadowed_constants.resolution[1] = 1.f;
    impl->unshadowed_constants.resolution[2] = 1.f;
    impl->unshadowed_constants.resolution[3] = 1.f;
    impl->unshadowed_constants.parameters[2] = 1.f;
    impl->unshadowed_constants.config[0] = 4;
    impl->unshadowed_constants.config[1] = 0xfu;

    ALMetalTextureDescriptor hero_descriptor;
    hero_descriptor.type = ALMetalTextureType::texture_cube;
    hero_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
    hero_descriptor.width = 1;
    hero_descriptor.height = 1;
    hero_descriptor.storage = ALMetalStorageMode::private_gpu;
    hero_descriptor.usage = ALMetalTextureUsage::shader_read;
    hero_descriptor.label = "Alchemy world inactive hero-probe fallback";
    impl->inactive_hero_probe = ALMetalTexture::create(
        device, hero_descriptor, error);
    auto hero_upload = impl->inactive_hero_probe
        ? device.makeCommandBuffer(
              "Alchemy world inactive hero-probe fallback upload", error)
        : nullptr;
    const std::uint32_t black = 0;
    bool hero_ready = hero_upload != nullptr;
    for (std::uint32_t face = 0; hero_ready && face < 6; ++face)
    {
        hero_ready = impl->inactive_hero_probe->upload(
            *hero_upload, 0, face, {}, &black, sizeof(black), error);
    }
    hero_ready = hero_ready && hero_upload->commitAndWait(error);
    if (!hero_ready)
    {
        return nullptr;
    }

    ALMetalTextureDescriptor ssr_descriptor;
    ssr_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
    ssr_descriptor.width = 1;
    ssr_descriptor.height = 1;
    ssr_descriptor.storage = ALMetalStorageMode::private_gpu;
    ssr_descriptor.usage = ALMetalTextureUsage::shader_read;
    ssr_descriptor.label = "Alchemy world inactive SSR color fallback";
    impl->inactive_ssr_color = ALMetalTexture::create(
        device, ssr_descriptor, error);
    ssr_descriptor.format = ALMetalPixelFormat::r32_float;
    ssr_descriptor.label = "Alchemy world inactive SSR depth fallback";
    impl->inactive_ssr_depth = ALMetalTexture::create(
        device, ssr_descriptor, error);
    auto ssr_upload = impl->inactive_ssr_color && impl->inactive_ssr_depth
        ? device.makeCommandBuffer(
              "Alchemy world inactive SSR fallback upload", error)
        : nullptr;
    const float far_depth = 1.f;
    const bool ssr_ready = ssr_upload &&
        impl->inactive_ssr_color->upload(
            *ssr_upload, 0, 0, {}, &black, sizeof(black), error) &&
        impl->inactive_ssr_depth->upload(
            *ssr_upload, 0, 0, {}, &far_depth, sizeof(far_depth), error) &&
        ssr_upload->commitAndWait(error);
    if (!ssr_ready)
    {
        return nullptr;
    }
    impl->inactive_ssr_constants.screen[0] = 1.f;
    impl->inactive_ssr_constants.screen[1] = 1.f;
    impl->inactive_ssr_constants.screen[2] = 1.f;
    impl->inactive_ssr_constants.sampling[0] = 1.f;

    return std::unique_ptr<ALMetalWorldGBufferRenderer>(
        new ALMetalWorldGBufferRenderer(std::move(impl)));
}

std::unique_ptr<ALMetalRenderTarget> ALMetalWorldGBufferRenderer::render(
    const ALMetalWorldGBufferScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches || scene.batch_count == 0 ||
        scene.width == 0 || scene.height == 0)
    {
        setError(error, "Metal world G-buffer scene is incomplete");
        return nullptr;
    }
    if (!finiteValues(scene.clear_color, 4))
    {
        setError(error, "Metal world G-buffer clear color is non-finite");
        return nullptr;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    constexpr std::uint32_t max_skin_joints = 110;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        if (batch.impostor)
        {
            const ALMetalPixelFormat expected_normal = scene.normal_hdr
                ? ALMetalPixelFormat::rgba16_unorm
                : ALMetalPixelFormat::rgb10a2_unorm;
            const ALMetalPixelFormat expected_emissive = scene.hdr
                ? ALMetalPixelFormat::rg11b10_float
                : ALMetalPixelFormat::rgba8_unorm;
            if (batch.vertices || batch.indices || batch.vertex_count != 0 ||
                batch.index_count != 0 || batch.texture_count != 0 ||
                batch.normal_texture || batch.material_slot_count != 0 ||
                batch.pbr_slot_count != 0 || batch.skin_palette ||
                batch.avatar_rigid || batch.avatar_skinning ||
                batch.avatar_cloth || batch.impostor_fullbright ||
                batch.impostor_shiny ||
                batch.impostor_emboss ||
                batch.terrain ||
                !finiteValues(batch.modelview_projection, 16) ||
                !finiteValues(batch.impostor_center, 4) ||
                !finiteValues(batch.impostor_left, 4) ||
                !finiteValues(batch.impostor_up, 4) ||
                !finiteValues(&batch.impostor_normal_rotation[0][0], 12) ||
                !sampleableTexture(batch.impostor_textures[0]) ||
                !sampleableTexture(batch.impostor_textures[1]) ||
                !sampleableTexture(batch.impostor_textures[2]) ||
                !sampleableTexture(batch.impostor_textures[3]) ||
                batch.impostor_textures[0]->descriptor().format !=
                    ALMetalPixelFormat::rgba8_unorm_srgb ||
                batch.impostor_textures[1]->descriptor().format !=
                    ALMetalPixelFormat::rgba8_unorm ||
                batch.impostor_textures[2]->descriptor().format !=
                    expected_normal ||
                batch.impostor_textures[3]->descriptor().format !=
                    expected_emissive)
            {
                setError(error, "Metal world impostor batch is invalid");
                return nullptr;
            }
            continue;
        }
        if (batch.terrain)
        {
            const bool pbr_terrain = batch.terrain_pbr;
            const bool height_paint = batch.terrain_paint_type == 0;
            const std::size_t hex_value_count = pbr_terrain ? 32 : 8;
            bool valid_factors = true;
            for (std::uint16_t factor : batch.terrain_tessellation_factors)
            {
                valid_factors = valid_factors &&
                    (factor & 0x8000u) == 0 &&
                    (factor & 0x7fffu) != 0 &&
                    (factor & 0x7c00u) != 0x7c00u;
            }
            if (!batch.vertices || batch.indices || batch.vertex_count != 4 ||
                batch.index_count != 0 || batch.texture_count != 4 ||
                batch.normal_texture || batch.material_slot_count != 0 ||
                batch.pbr_slot_count != (pbr_terrain ? 4u : 0u) ||
                batch.skin_palette ||
                batch.avatar_rigid || batch.avatar_skinning ||
                batch.avatar_cloth || !valid_factors ||
                (!pbr_terrain &&
                 (batch.terrain_paint_texture || batch.alpha_mask ||
                  batch.terrain_flat_normals)) ||
                (pbr_terrain &&
                 (batch.terrain_paint_type > 1 ||
                  batch.terrain_pbr_detail < -4 ||
                  batch.terrain_pbr_detail > 0 ||
                  !finiteValues(batch.terrain_modelview, 16) ||
                  !finiteValues(&batch.terrain_uv_transform[0][0], 16) ||
                  !finiteValues(&batch.terrain_uv_offset[0][0], 8) ||
                  !finiteValues(&batch.terrain_normal_axes[0][0], 16))) ||
                batch.position_offset % float4_stride != 0 ||
                !rangeFits(batch.position_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !sampleableTexture(batch.terrain_height_texture) ||
                !sampleableTexture(batch.terrain_composition_texture) ||
                (!pbr_terrain &&
                 !sampleableTexture(batch.terrain_alpha_ramp_texture)) ||
                (pbr_terrain &&
                 (!sampleableTexture(
                      height_paint ? batch.terrain_alpha_ramp_texture
                                   : batch.terrain_paint_texture) ||
                  (height_paint && batch.terrain_paint_texture) ||
                  (!height_paint && batch.terrain_alpha_ramp_texture))) ||
                (batch.terrain_parcel_overlay_texture &&
                 !sampleableTexture(
                     batch.terrain_parcel_overlay_texture)) ||
                batch.terrain_height_texture->descriptor().format !=
                    ALMetalPixelFormat::r32_float ||
                batch.terrain_composition_texture->descriptor().format !=
                    ALMetalPixelFormat::rg16_float ||
                (batch.terrain_parcel_overlay_texture &&
                 batch.terrain_parcel_overlay_texture->descriptor().format !=
                     ALMetalPixelFormat::rgba8_unorm_srgb) ||
                batch.terrain_height_texture->descriptor().width !=
                    batch.terrain_composition_texture->descriptor().width ||
                batch.terrain_height_texture->descriptor().height !=
                    batch.terrain_composition_texture->descriptor().height ||
                !std::isfinite(batch.terrain_grid_scale) ||
                batch.terrain_grid_scale <= 0.f ||
                batch.impostor_fullbright || batch.impostor_shiny ||
                batch.impostor_emboss ||
                !std::isfinite(batch.terrain_region_scale) ||
                batch.terrain_region_scale <= 0.f ||
                !std::isfinite(batch.terrain_detail_scale) ||
                batch.terrain_detail_scale <= 0.f ||
                !finiteValues(batch.terrain_detail_phase, 2) ||
                !std::isfinite(batch.terrain_triplanar_blend) ||
                batch.terrain_triplanar_blend <= 0.f ||
                (batch.terrain_hex &&
                 (!finiteValues(&batch.terrain_hex_fraction[0][0],
                                hex_value_count) ||
                  std::any_of(
                      &batch.terrain_hex_fraction[0][0],
                      &batch.terrain_hex_fraction[0][0] + hex_value_count,
                      [](float value)
                      {
                          return value < 0.f || value >= 1.f;
                      }))) ||
                !finiteValues(batch.modelview_projection, 16) ||
                !finiteValues(&batch.normal_matrix[0][0], 12))
            {
                setError(error, "Metal world terrain batch is invalid");
                return nullptr;
            }
            for (std::uint32_t texture_index = 0;
                 texture_index < batch.texture_count; ++texture_index)
            {
                if (!sampleableTexture(
                        batch.diffuse_textures[texture_index]))
                {
                    setError(error,
                             "Metal world terrain detail texture is invalid");
                    return nullptr;
                }
            }
            if (pbr_terrain)
            {
                bool has_masked_material = false;
                for (std::uint32_t slot_index = 0;
                     slot_index < batch.pbr_slot_count; ++slot_index)
                {
                    const ALMetalWorldPBRSlot& slot =
                        batch.pbr_slots[slot_index];
                    has_masked_material = has_masked_material ||
                                          slot.alpha_cutoff >= 0.f;
                    if (!sampleableTexture(slot.normal_texture) ||
                        !sampleableTexture(slot.orm_texture) ||
                        !sampleableTexture(slot.emissive_texture) ||
                        !finiteValues(slot.base_color_factor, 4) ||
                        !finiteValues(slot.emissive_color, 4) ||
                        !std::isfinite(slot.roughness_factor) ||
                        !std::isfinite(slot.metallic_factor) ||
                        !std::isfinite(slot.alpha_cutoff) ||
                        slot.roughness_factor < 0.f ||
                        slot.roughness_factor > 1.f ||
                        slot.metallic_factor < 0.f ||
                        slot.metallic_factor > 1.f ||
                        slot.alpha_cutoff < -1.f ||
                        slot.alpha_cutoff > 1.f)
                    {
                        setError(error,
                                 "Metal world PBR terrain slot is invalid");
                        return nullptr;
                    }
                }
                if (has_masked_material != batch.alpha_mask)
                {
                    setError(error,
                             "Metal world PBR terrain mask state is invalid");
                    return nullptr;
                }
            }
            continue;
        }
        const bool bump = batch.normal_texture != nullptr;
        const bool material = batch.material_slot_count != 0;
        const bool pbr = isPBRBatch(batch);
        const bool avatar = batch.avatar_rigid || batch.avatar_skinning;
        const bool material_normal_map = materialHasNormalMap(batch);
        const bool material_specular_map = materialHasSpecularMap(batch);
        const bool tangent_required = bump || material_normal_map || pbr;
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.texture_count == 0 ||
            batch.impostor_fullbright || batch.impostor_shiny ||
            batch.impostor_emboss ||
            batch.texture_count >
                ALMetalWorldSimpleBatch::max_diffuse_textures ||
            (material &&
             (batch.material_slot_count >
                  ALMetalWorldSimpleBatch::max_material_slots ||
              batch.material_slot_count != batch.texture_count ||
              bump || pbr || batch.alpha_mask ||
              !validMaterialAlphaMode(batch.material_alpha_mode))) ||
            (pbr &&
             (batch.pbr_slot_count >
                  ALMetalWorldSimpleBatch::max_material_slots ||
              batch.pbr_slot_count != batch.texture_count ||
              material || bump)) ||
            (batch.avatar_cloth && !batch.avatar_skinning) ||
            (!batch.avatar_skinning && batch.cloth_offset != 0) ||
            (avatar &&
             (batch.avatar_rigid == batch.avatar_skinning ||
              (batch.avatar_rigid && batch.skin_palette != nullptr) ||
              batch.texture_count != 1 || !batch.alpha_mask ||
              batch.use_vertex_color || batch.alpha_cutoff_bias ||
              material || pbr || bump)) ||
            (!material && batch.material_alpha_mode != 0) ||
            batch.vertex_count == 0 || batch.index_count == 0 ||
            batch.index_count % 3 != 0 ||
            batch.position_offset % float4_stride != 0 ||
            batch.normal_offset % float4_stride != 0 ||
            batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
            (!avatar &&
             batch.color_offset % (sizeof(std::uint8_t) * 4) != 0) ||
            (tangent_required &&
             batch.tangent_offset % float4_stride != 0) ||
            (material_normal_map &&
             batch.texcoord1_offset % (sizeof(float) * 2) != 0) ||
            (material_specular_map &&
             batch.texcoord2_offset % (sizeof(float) * 2) != 0) ||
            batch.index_offset % index_stride != 0 ||
            !std::isfinite(batch.alpha_cutoff) ||
            !finiteValues(batch.modelview_projection, 16) ||
            !finiteValues(&batch.normal_matrix[0][0], 12) ||
            !finiteValues(&batch.rigged_normal_matrix[0][0], 12) ||
            !finiteValues(batch.texture_matrix, 16) ||
            (batch.avatar_cloth &&
             (!finiteValues(batch.avatar_wind, 4) ||
              !finiteValues(batch.avatar_sin_wave, 4) ||
              !finiteValues(batch.avatar_gravity, 4) ||
              !finiteValues(batch.avatar_cloth_depth, 4))))
        {
            setError(error, "Metal world simple batch is invalid");
            return nullptr;
        }
        for (std::uint32_t slot = 0;
             slot < batch.material_slot_count; ++slot)
        {
            const ALMetalWorldMaterialSlot& material_slot =
                batch.material_slots[slot];
            if (!finiteValues(material_slot.specular_color, 4) ||
                !std::isfinite(material_slot.env_intensity) ||
                !std::isfinite(material_slot.alpha_cutoff) ||
                !std::isfinite(material_slot.fullbright) ||
                (material_normal_map !=
                 (material_slot.normal_texture != nullptr)) ||
                (material_specular_map !=
                 (material_slot.specular_texture != nullptr)) ||
                (material_normal_map &&
                 !sampleableTexture(material_slot.normal_texture)) ||
                (material_specular_map &&
                 !sampleableTexture(material_slot.specular_texture)))
            {
                setError(error, "Metal world material slot is invalid");
                return nullptr;
            }
        }
        for (std::uint32_t slot = 0; slot < batch.pbr_slot_count; ++slot)
        {
            const ALMetalWorldPBRSlot& pbr_slot = batch.pbr_slots[slot];
            if (!sampleableTexture(pbr_slot.normal_texture) ||
                !sampleableTexture(pbr_slot.orm_texture) ||
                !sampleableTexture(pbr_slot.emissive_texture) ||
                !finiteValues(&pbr_slot.texture_transform[0][0], 32) ||
                !finiteValues(pbr_slot.emissive_color, 4) ||
                !std::isfinite(pbr_slot.roughness_factor) ||
                !std::isfinite(pbr_slot.metallic_factor) ||
                !std::isfinite(pbr_slot.alpha_cutoff) ||
                pbr_slot.roughness_factor < 0.f ||
                pbr_slot.roughness_factor > 1.f ||
                pbr_slot.metallic_factor < 0.f ||
                pbr_slot.metallic_factor > 1.f ||
                pbr_slot.alpha_cutoff < -1.f ||
                pbr_slot.alpha_cutoff > 1.f)
            {
                setError(error, "Metal world PBR slot is invalid");
                return nullptr;
            }
        }
        if (!rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.normal_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       sizeof(float) * 2, batch.vertices->length()) ||
            (!avatar &&
             !rangeFits(batch.color_offset, batch.vertex_count,
                        sizeof(std::uint8_t) * 4,
                        batch.vertices->length())) ||
            (tangent_required &&
             !rangeFits(batch.tangent_offset, batch.vertex_count,
                        float4_stride, batch.vertices->length())) ||
            (material_normal_map &&
             !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                        sizeof(float) * 2, batch.vertices->length())) ||
            (material_specular_map &&
             !rangeFits(batch.texcoord2_offset, batch.vertex_count,
                        sizeof(float) * 2, batch.vertices->length())) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error,
                     "Metal world simple geometry range exceeds its buffers");
            return nullptr;
        }
        if (batch.avatar_skinning)
        {
            constexpr std::size_t avatar_palette_floats = 45 * 4;
            if (!batch.skin_palette || batch.skin_joint_count != 15 ||
                batch.skin_palette_float_count != avatar_palette_floats ||
                batch.weight_offset % sizeof(float) != 0 ||
                !rangeFits(batch.weight_offset, batch.vertex_count,
                           sizeof(float), batch.vertices->length()) ||
                !finiteValues(batch.skin_palette,
                              avatar_palette_floats) ||
                (batch.avatar_cloth &&
                 (batch.cloth_offset % float4_stride != 0 ||
                  !rangeFits(batch.cloth_offset, batch.vertex_count,
                             float4_stride,
                             batch.vertices->length()))))
            {
                setError(error,
                         "Metal world avatar skinning data is invalid");
                return nullptr;
            }
        }
        else if (batch.skin_palette)
        {
            const std::size_t expected_palette_floats =
                4 + static_cast<std::size_t>(batch.skin_joint_count) * 12;
            if (batch.skin_joint_count == 0 ||
                batch.skin_joint_count > max_skin_joints ||
                batch.skin_palette_float_count != expected_palette_floats ||
                batch.weight_offset % float4_stride != 0 ||
                !rangeFits(batch.weight_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !finiteValues(batch.skin_palette,
                              expected_palette_floats))
            {
                setError(error,
                         "Metal world simple skinning data is invalid");
                return nullptr;
            }
        }
        else if (batch.weight_offset != 0 || batch.cloth_offset != 0 ||
                 batch.skin_palette_float_count != 0 ||
                 batch.skin_joint_count != 0)
        {
            setError(error,
                     "Metal world simple skinning data is incomplete");
            return nullptr;
        }
        for (std::uint32_t texture_index = 0;
             texture_index < batch.texture_count; ++texture_index)
        {
            const ALMetalTexture* texture =
                batch.diffuse_textures[texture_index];
            if (!texture || texture->descriptor().type !=
                    ALMetalTextureType::texture_2d ||
                !alMetalHasUsage(texture->descriptor().usage,
                                 ALMetalTextureUsage::shader_read))
            {
                setError(error,
                         "Metal world diffuse texture is not sampleable 2D");
                return nullptr;
            }
        }
        if (bump &&
            (batch.normal_texture->descriptor().type !=
                 ALMetalTextureType::texture_2d ||
             !alMetalHasUsage(batch.normal_texture->descriptor().usage,
                              ALMetalTextureUsage::shader_read)))
        {
            setError(error,
                     "Metal world normal texture is not sampleable 2D");
            return nullptr;
        }
    }

    std::unique_ptr<ALMetalBuffer> terrain_factor_buffer;
    std::vector<std::uint16_t> terrain_factors(scene.batch_count * 6, 0);
    bool has_terrain = false;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        if (!batch.terrain)
        {
            continue;
        }
        has_terrain = true;
        std::memcpy(terrain_factors.data() + index * 6,
                    batch.terrain_tessellation_factors,
                    sizeof(batch.terrain_tessellation_factors));
    }
    if (has_terrain)
    {
        ALMetalBufferDescriptor factor_descriptor;
        factor_descriptor.length = terrain_factors.size() *
                                   sizeof(terrain_factors.front());
        factor_descriptor.storage = ALMetalStorageMode::shared;
        factor_descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        factor_descriptor.label =
            "Alchemy world terrain tessellation factors";
        terrain_factor_buffer = ALMetalBuffer::create(
            *mImpl->device, factor_descriptor, error);
        if (!terrain_factor_buffer || !terrain_factor_buffer->write(
                0, terrain_factors.data(), factor_descriptor.length, error))
        {
            return nullptr;
        }
    }

    std::vector<std::unique_ptr<ALMetalBuffer>> palette_storage;
    std::vector<const ALMetalBuffer*> palette_buffers(
        scene.batch_count, nullptr);
    for (std::size_t batch_index = 0;
         batch_index < scene.batch_count; ++batch_index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[batch_index];
        if (batch.terrain || !batch.skin_palette)
        {
            continue;
        }
        for (std::size_t previous = 0; previous < batch_index; ++previous)
        {
            const ALMetalWorldSimpleBatch& candidate = scene.batches[previous];
            if (candidate.skin_palette == batch.skin_palette &&
                candidate.skin_palette_float_count ==
                    batch.skin_palette_float_count &&
                candidate.skin_joint_count == batch.skin_joint_count)
            {
                palette_buffers[batch_index] = palette_buffers[previous];
                break;
            }
        }
        if (palette_buffers[batch_index])
        {
            continue;
        }
        ALMetalBufferDescriptor palette_descriptor;
        palette_descriptor.length =
            batch.skin_palette_float_count * sizeof(float);
        palette_descriptor.storage = ALMetalStorageMode::shared;
        palette_descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        palette_descriptor.label = "Alchemy world skin palette";
        auto palette = ALMetalBuffer::create(
            *mImpl->device, palette_descriptor, error);
        if (!palette || !palette->write(
                0, batch.skin_palette, palette_descriptor.length, error))
        {
            return nullptr;
        }
        palette_buffers[batch_index] = palette.get();
        palette_storage.push_back(std::move(palette));
    }

    ALMetalRenderTargetDescriptor target_descriptor;
    target_descriptor.width = scene.width;
    target_descriptor.height = scene.height;
    target_descriptor.color_attachment_count = 4;
    target_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
    target_descriptor.colors[0].label = "Alchemy world G-buffer albedo";
    target_descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
    target_descriptor.colors[1].label = "Alchemy world G-buffer specular ORM";
    target_descriptor.colors[2].format = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    target_descriptor.colors[2].label = "Alchemy world G-buffer normal";
    target_descriptor.colors[3].format = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    target_descriptor.colors[3].label = "Alchemy world G-buffer emissive";
    target_descriptor.depth_stencil_format =
        ALMetalPixelFormat::depth32_float_stencil8;
    target_descriptor.depth_stencil_shader_read = true;
    target_descriptor.label = "Alchemy native world G-buffer bootstrap";
    auto target = ALMetalRenderTarget::create(
        *mImpl->device, target_descriptor, error);
    if (!target)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::clear;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
        pass_config.colors[attachment].clear = {
            scene.clear_color[0], scene.clear_color[1],
            scene.clear_color[2], scene.clear_color[3],
        };
    }
    pass_config.depth_load = ALMetalLoadAction::clear;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.clear_depth = scene.reverse_z ? 0.0 : 1.0;
    pass_config.stencil_load = ALMetalLoadAction::clear;
    pass_config.stencil_store = ALMetalStoreAction::store;
    pass_config.clear_stencil = 0;
    auto pass = target->makeRenderPass(pass_config, error);
    if (!pass)
    {
        return nullptr;
    }
    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native world G-buffer bootstrap", error);
    if (!command)
    {
        return nullptr;
    }
    auto encoder = ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world simple G-buffer encoder", error);
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_depth.get() : mImpl->forward_depth.get();
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    bool encoded =
        encoder->setDepthStencilState(depth, error) &&
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::back, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise,
                                       error);

    for (std::size_t index = 0; encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        if (batch.impostor)
        {
            WorldImpostorConstants constants{};
            std::memcpy(constants.modelview_projection,
                        batch.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.center, batch.impostor_center,
                        sizeof(constants.center));
            std::memcpy(constants.left, batch.impostor_left,
                        sizeof(constants.left));
            std::memcpy(constants.up, batch.impostor_up,
                        sizeof(constants.up));
            std::memcpy(constants.normal_rotation,
                        batch.impostor_normal_rotation,
                        sizeof(constants.normal_rotation));
            constants.depth_convention[0] = scene.reverse_z ? 1u : 0u;
            const ALMetalRenderPipeline* pipeline =
                mImpl->impostor_pipelines[scene.hdr ? 1 : 0]
                                         [scene.normal_hdr ? 1 : 0].get();
            encoded =
                // A camera-facing impostor is intrinsically two-sided. The
                // GL path inherits culling state, but its generated winding
                // changes sign with the viewer basis; accepting both avoids
                // dropping the billboard at those basis transitions.
                encoder->setCullMode(ALMetalCullMode::none, error) &&
                encoder->setPipeline(*pipeline, error) &&
                encoder->setVertexBytes(
                    0, &constants, sizeof(constants), error) &&
                encoder->setFragmentBytes(
                    0, &constants, sizeof(constants), error) &&
                encoder->setFragmentTextureLinear(
                    0, batch.impostor_textures[0], error) &&
                encoder->setFragmentTexture(
                    1, batch.impostor_textures[1], error) &&
                encoder->setFragmentTexture(
                    2, batch.impostor_textures[2], error) &&
                encoder->setFragmentTexture(
                    3, batch.impostor_textures[3], error) &&
                encoder->setFragmentSampler(
                    0, mImpl->point_clamp_sampler.get(), error) &&
                encoder->draw(
                    ALMetalPrimitiveType::triangle, 0, 6, 1, error);
            continue;
        }
        if (batch.terrain)
        {
            WorldTerrainConstants constants{};
            std::memcpy(constants.modelview_projection,
                        batch.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.modelview, batch.terrain_modelview,
                        sizeof(constants.modelview));
            std::memcpy(constants.normal_matrix,
                        batch.normal_matrix,
                        sizeof(constants.normal_matrix));
            constants.surface[0] = batch.terrain_grid_scale;
            constants.surface[1] = batch.terrain_region_scale;
            constants.surface[2] = batch.terrain_detail_scale;
            constants.surface[3] = batch.terrain_triplanar_blend;
            constants.detail[0] = batch.terrain_detail_phase[0];
            constants.detail[1] = batch.terrain_detail_phase[1];
            constants.detail[2] = batch.terrain_smoothing ? 1.f : 0.f;
            constants.detail[3] = batch.terrain_triplanar ? 1.f : 0.f;
            constants.config[0] = scene.reverse_z ? 1u : 0u;
            constants.config[1] =
                batch.terrain_parcel_overlay_texture ? 1u : 0u;
            constants.config[2] = batch.terrain_hex ? 1u : 0u;
            constants.config[3] = scene.normal_hdr ? 1u : 0u;
            constants.pbr_config[0] = static_cast<std::uint32_t>(
                batch.terrain_pbr_detail + 4);
            constants.pbr_config[1] = batch.terrain_paint_type;
            constants.pbr_config[2] = batch.alpha_mask ? 1u : 0u;
            constants.pbr_config[3] =
                batch.terrain_flat_normals ? 1u : 0u;
            std::memcpy(constants.hex_fraction,
                        batch.terrain_hex_fraction,
                        sizeof(constants.hex_fraction));
            std::memcpy(constants.hex_base,
                        batch.terrain_hex_base,
                        sizeof(constants.hex_base));
            for (std::size_t slot = 0;
                 slot < batch.pbr_slot_count; ++slot)
            {
                const ALMetalWorldPBRSlot& source = batch.pbr_slots[slot];
                WorldTerrainConstants::PBRSlot& destination =
                    constants.pbr_slots[slot];
                std::memcpy(destination.uv_transform,
                            batch.terrain_uv_transform[slot],
                            sizeof(destination.uv_transform));
                std::memcpy(destination.uv_offset,
                            batch.terrain_uv_offset[slot],
                            sizeof(batch.terrain_uv_offset[slot]));
                std::memcpy(destination.normal_axes,
                            batch.terrain_normal_axes[slot],
                            sizeof(destination.normal_axes));
                std::memcpy(destination.base_color_factor,
                            source.base_color_factor,
                            sizeof(destination.base_color_factor));
                std::memcpy(destination.emissive_color,
                            source.emissive_color,
                            sizeof(destination.emissive_color));
                destination.properties[0] = source.roughness_factor;
                destination.properties[1] = source.metallic_factor;
                destination.properties[2] = source.alpha_cutoff;
                destination.properties[3] = source.base_color_factor[3];
            }
            const auto& terrain_pipelines = batch.terrain_pbr
                ? mImpl->pbr_terrain_pipelines
                : mImpl->terrain_pipelines;
            const ALMetalRenderPipeline* pipeline =
                terrain_pipelines[scene.hdr ? 1 : 0]
                    [batch.terrain_tessellation_pinned ? 1 : 0]
                    [scene.normal_hdr ? 1 : 0].get();

            encoded =
                encoder->setCullMode(ALMetalCullMode::back, error) &&
                encoder->setPipeline(*pipeline, error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBytes(
                    4, &constants, sizeof(constants), error) &&
                encoder->setFragmentBytes(
                    0, &constants, sizeof(constants), error) &&
                encoder->setVertexTexture(
                    0, batch.terrain_height_texture, error) &&
                encoder->setVertexTexture(
                    1, batch.terrain_composition_texture, error) &&
                encoder->setVertexSampler(
                    2, mImpl->linear_clamp_sampler.get(), error) &&
                encoder->setFragmentSampler(
                    0, mImpl->diffuse_sampler.get(), error) &&
                encoder->setFragmentSampler(
                    1, mImpl->point_clamp_sampler.get(), error) &&
                encoder->setFragmentSampler(
                    3, mImpl->ramp_sampler.get(), error);
            for (std::uint32_t texture = 0;
                 encoded && texture < batch.texture_count; ++texture)
            {
                encoded = encoder->setFragmentTexture(
                    texture, batch.diffuse_textures[texture], error);
            }
            if (encoded && batch.terrain_pbr)
            {
                const ALMetalTexture* blend = batch.terrain_paint_type == 0
                    ? batch.terrain_alpha_ramp_texture
                    : batch.terrain_paint_texture;
                for (std::uint32_t slot = 0;
                     encoded && slot < batch.pbr_slot_count; ++slot)
                {
                    const ALMetalWorldPBRSlot& pbr_slot =
                        batch.pbr_slots[slot];
                    encoded =
                        encoder->setFragmentTextureLinear(
                            4 + slot, pbr_slot.normal_texture, error) &&
                        encoder->setFragmentTextureLinear(
                            8 + slot, pbr_slot.orm_texture, error) &&
                        encoder->setFragmentTexture(
                            12 + slot, pbr_slot.emissive_texture, error);
                }
                encoded = encoded &&
                    encoder->setFragmentTexture(
                        16, batch.terrain_height_texture, error) &&
                    encoder->setFragmentTexture(17, blend, error) &&
                    encoder->setFragmentTexture(
                        18, batch.terrain_parcel_overlay_texture
                            ? batch.terrain_parcel_overlay_texture : blend,
                        error);
            }
            else if (encoded)
            {
                encoded =
                    encoder->setFragmentTexture(
                        4, batch.terrain_height_texture, error) &&
                    encoder->setFragmentTexture(
                        5, batch.terrain_alpha_ramp_texture, error) &&
                    encoder->setFragmentTexture(
                        6, batch.terrain_parcel_overlay_texture
                            ? batch.terrain_parcel_overlay_texture
                            : batch.terrain_alpha_ramp_texture,
                        error);
            }
            encoded = encoded &&
                encoder->setTessellationFactorBuffer(
                    *terrain_factor_buffer,
                    index * sizeof(batch.terrain_tessellation_factors),
                    0, error) &&
                encoder->drawPatches(4, 0, 1, 1, 0, error);
            continue;
        }
        WorldSimpleConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.normal_matrix, batch.normal_matrix,
                    sizeof(constants.normal_matrix));
        std::memcpy(constants.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(constants.rigged_normal_matrix));
        std::memcpy(constants.texture_matrix, batch.texture_matrix,
                    sizeof(constants.texture_matrix));
        std::memcpy(constants.avatar_wind, batch.avatar_wind,
                    sizeof(constants.avatar_wind));
        std::memcpy(constants.avatar_sin_wave, batch.avatar_sin_wave,
                    sizeof(constants.avatar_sin_wave));
        std::memcpy(constants.avatar_gravity, batch.avatar_gravity,
                    sizeof(constants.avatar_gravity));
        std::memcpy(constants.avatar_cloth_depth,
                    batch.avatar_cloth_depth,
                    sizeof(constants.avatar_cloth_depth));
        constants.material[0] = batch.alpha_cutoff;
        constants.material[1] = batch.use_vertex_color ? 1.f : 0.f;
        constants.material[2] = batch.alpha_cutoff_bias ? 1.f : 0.f;
        constants.material[3] = batch.alpha_mask ? 1.f : 0.f;
        constants.depth_convention[0] = scene.reverse_z ? 1u : 0u;
        constants.depth_convention[1] = batch.texture_count;
        const bool avatar = batch.avatar_rigid || batch.avatar_skinning;
        const bool rigged = batch.skin_palette != nullptr && !avatar;
        const bool bump = batch.normal_texture != nullptr;
        const bool material = batch.material_slot_count != 0;
        const bool pbr = isPBRBatch(batch);
        const bool material_normal_map = materialHasNormalMap(batch);
        const bool material_specular_map = materialHasSpecularMap(batch);
        const std::size_t avatar_variant = batch.avatar_rigid
            ? 0 : (batch.avatar_cloth ? 2 : 1);
        const ALMetalRenderPipeline* pipeline = avatar
            ? mImpl->avatar_pipelines[scene.hdr ? 1 : 0]
                                     [avatar_variant]
                                     [scene.normal_hdr ? 1 : 0].get()
            : (pbr
                ? mImpl->pbr_pipelines[scene.hdr ? 1 : 0]
                                      [rigged ? 1 : 0]
                                      [scene.normal_hdr ? 1 : 0].get()
                : (material
                    ? mImpl->material_pipelines[scene.hdr ? 1 : 0]
                                               [rigged ? 1 : 0]
                                               [scene.normal_hdr ? 1 : 0].get()
                    : mImpl->pipelines[scene.hdr ? 1 : 0]
                                      [bump ? 1 : 0]
                                      [batch.alpha_mask ? 1 : 0]
                                      [rigged ? 1 : 0]
                                      [scene.normal_hdr ? 1 : 0].get()));

        encoded =
            encoder->setCullMode(
                pbr && batch.double_sided
                    ? ALMetalCullMode::none : ALMetalCullMode::back,
                error) &&
            encoder->setPipeline(*pipeline, error) &&
            encoder->setVertexBuffer(0, *batch.vertices,
                                     batch.position_offset, error) &&
            encoder->setVertexBuffer(1, *batch.vertices,
                                     batch.normal_offset, error) &&
            encoder->setVertexBuffer(2, *batch.vertices,
                                     batch.texcoord_offset, error) &&
            encoder->setVertexBytes(4, &constants, sizeof(constants), error) &&
            encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error);
        if (encoded && !avatar)
        {
            encoded = encoder->setVertexBuffer(
                3, *batch.vertices, batch.color_offset, error);
        }
        if (encoded && batch.skin_palette)
        {
            encoded =
                encoder->setVertexBuffer(
                    5, *palette_buffers[index], 0, error) &&
                encoder->setVertexBuffer(
                    6, *batch.vertices, batch.weight_offset, error) &&
                encoder->setVertexBytes(
                    7, &batch.skin_joint_count,
                    sizeof(batch.skin_joint_count), error);
        }
        if (encoded && batch.avatar_cloth)
        {
            encoded = encoder->setVertexBuffer(
                9, *batch.vertices, batch.cloth_offset, error);
        }
        if (encoded && bump)
        {
            encoded =
                encoder->setVertexBuffer(8, *batch.vertices,
                                         batch.tangent_offset, error) &&
                encoder->setFragmentTexture(
                    32, batch.normal_texture, error);
        }
        if (encoded && material)
        {
            encoded =
                encoder->setVertexBuffer(
                    8, *batch.vertices,
                    material_normal_map
                        ? batch.tangent_offset : batch.normal_offset,
                    error) &&
                encoder->setVertexBuffer(
                    9, *batch.vertices,
                    material_normal_map
                        ? batch.texcoord1_offset : batch.texcoord_offset,
                    error) &&
                encoder->setVertexBuffer(
                    10, *batch.vertices,
                    material_specular_map
                        ? batch.texcoord2_offset : batch.texcoord_offset,
                    error);
            WorldMaterialConstants material_constants{};
            for (std::size_t slot = 0;
                 slot < batch.material_slot_count; ++slot)
            {
                const ALMetalWorldMaterialSlot& source =
                    batch.material_slots[slot];
                WorldMaterialSlotConstants& destination =
                    material_constants.slots[slot];
                std::memcpy(destination.specular_color,
                            source.specular_color,
                            sizeof(destination.specular_color));
                destination.properties[0] = source.env_intensity;
                destination.properties[1] = source.alpha_cutoff;
                destination.properties[2] = source.fullbright;
            }
            material_constants.config[0] = batch.material_slot_count;
            material_constants.config[1] = batch.material_alpha_mode;
            material_constants.config[2] = material_normal_map ? 1u : 0u;
            material_constants.config[3] = material_specular_map ? 1u : 0u;
            encoded = encoded && encoder->setFragmentBytes(
                0, &material_constants, sizeof(material_constants), error);
        }
        if (encoded && pbr)
        {
            encoded = encoder->setVertexBuffer(
                8, *batch.vertices, batch.tangent_offset, error);
            WorldPBRConstants pbr_constants{};
            for (std::size_t slot = 0; slot < batch.pbr_slot_count; ++slot)
            {
                const ALMetalWorldPBRSlot& source = batch.pbr_slots[slot];
                WorldPBRSlotConstants& destination =
                    pbr_constants.slots[slot];
                std::memcpy(destination.texture_transform,
                            source.texture_transform,
                            sizeof(destination.texture_transform));
                std::memcpy(destination.emissive_color,
                            source.emissive_color,
                            sizeof(destination.emissive_color));
                destination.properties[0] = source.roughness_factor;
                destination.properties[1] = source.metallic_factor;
                destination.properties[2] = source.alpha_cutoff;
            }
            pbr_constants.config[0] = batch.pbr_slot_count;
            pbr_constants.config[1] = batch.alpha_mask ? 1u : 0u;
            pbr_constants.config[2] = scene.normal_hdr ? 1u : 0u;
            encoded = encoded &&
                encoder->setVertexBytes(
                    11, &pbr_constants, sizeof(pbr_constants), error) &&
                encoder->setFragmentBytes(
                    0, &pbr_constants, sizeof(pbr_constants), error);
        }
        for (std::size_t texture_index = 0;
             encoded && texture_index <
                 ALMetalWorldSimpleBatch::max_diffuse_textures;
             ++texture_index)
        {
            const ALMetalTexture* texture = texture_index < batch.texture_count
                ? batch.diffuse_textures[texture_index]
                : batch.diffuse_textures[0];
            encoded = encoder->setFragmentTexture(
                static_cast<std::uint32_t>(texture_index), texture, error);
        }
        if (encoded && material)
        {
            for (std::size_t slot = 0;
                 encoded && slot < ALMetalWorldSimpleBatch::max_material_slots;
                 ++slot)
            {
                const std::size_t source = std::min<std::size_t>(
                    slot, batch.material_slot_count - 1);
                const ALMetalTexture* normal_texture = material_normal_map
                    ? batch.material_slots[source].normal_texture
                    : batch.diffuse_textures[0];
                const ALMetalTexture* specular_texture = material_specular_map
                    ? batch.material_slots[source].specular_texture
                    : batch.diffuse_textures[0];
                encoded =
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(8 + slot),
                        normal_texture, error) &&
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(16 + slot),
                        specular_texture, error);
            }
        }
        if (encoded && pbr)
        {
            for (std::size_t slot = 0;
                 encoded && slot < ALMetalWorldSimpleBatch::max_material_slots;
                 ++slot)
            {
                const std::size_t source = std::min<std::size_t>(
                    slot, batch.pbr_slot_count - 1);
                const ALMetalWorldPBRSlot& pbr_slot =
                    batch.pbr_slots[source];
                encoded =
                    encoder->setFragmentTextureLinear(
                        static_cast<std::uint32_t>(8 + slot),
                        pbr_slot.normal_texture, error) &&
                    encoder->setFragmentTextureLinear(
                        static_cast<std::uint32_t>(16 + slot),
                        pbr_slot.orm_texture, error) &&
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(24 + slot),
                        pbr_slot.emissive_texture, error);
            }
        }
        encoded = encoded &&
            encoder->drawIndexed(ALMetalPrimitiveType::triangle,
                                 batch.index_count, batch.index_type,
                                 *batch.indices, batch.index_offset, 1, error);
    }

    if (!encoded || !encoder->end(error))
    {
        return nullptr;
    }
    if (scene.stamp_opaque_coverage)
    {
        ALMetalRenderPassConfig coverage_config;
        for (std::size_t attachment = 0; attachment < 4; ++attachment)
        {
            coverage_config.colors[attachment].load =
                ALMetalLoadAction::load;
            coverage_config.colors[attachment].store =
                ALMetalStoreAction::store;
        }
        coverage_config.depth_load = ALMetalLoadAction::load;
        coverage_config.depth_store = ALMetalStoreAction::store;
        coverage_config.stencil_load = ALMetalLoadAction::load;
        coverage_config.stencil_store = ALMetalStoreAction::store;
        auto coverage_pass = target->makeRenderPass(coverage_config, error);
        if (!coverage_pass)
        {
            return nullptr;
        }
        auto coverage_encoder = ALMetalRenderEncoder::begin(
            *command, *coverage_pass,
            "Alchemy world opaque-coverage encoder", error);
        if (!coverage_encoder)
        {
            return nullptr;
        }
        const std::uint32_t reverse_z = scene.reverse_z ? 1u : 0u;
        const ALMetalDepthStencilState* coverage_depth = scene.reverse_z
            ? mImpl->reverse_coverage_depth.get()
            : mImpl->forward_coverage_depth.get();
        const ALMetalRenderPipeline* coverage_pipeline =
            mImpl->coverage_pipelines[scene.hdr ? 1 : 0]
                                     [scene.normal_hdr ? 1 : 0].get();
        encoded =
            coverage_encoder->setDepthStencilState(coverage_depth, error) &&
            coverage_encoder->setViewport(viewport, error) &&
            coverage_encoder->setScissor(scissor, error) &&
            coverage_encoder->setCullMode(ALMetalCullMode::none, error) &&
            coverage_encoder->setPipeline(*coverage_pipeline, error) &&
            coverage_encoder->setVertexBytes(
                0, &reverse_z, sizeof(reverse_z), error) &&
            coverage_encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
            coverage_encoder->end(error);
        if (!encoded)
        {
            return nullptr;
        }
    }
    if (!command->commit(error))
    {
        return nullptr;
    }
    return target;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderLocalLights(
    const ALMetalWorldLocalLightingScene& scene,
    std::string* error)
{
    constexpr std::size_t max_light_count = 65535;
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        (scene.light_count != 0 && !scene.lights) ||
        scene.light_count > max_light_count ||
        !finiteValues(scene.inverse_projection, 16) ||
        !std::isfinite(scene.specular_exponent) ||
        scene.specular_exponent <= 0.f)
    {
        setError(error, "Metal world local-light scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.gbuffer->descriptor();
    const ALMetalTexture* albedo = scene.gbuffer->colorTexture(0);
    const ALMetalTexture* specular = scene.gbuffer->colorTexture(1);
    const ALMetalTexture* normal = scene.gbuffer->colorTexture(2);
    const ALMetalTexture* emissive = scene.gbuffer->colorTexture(3);
    const ALMetalTexture* depth = scene.gbuffer->depthStencilTexture();
    const bool normal_format =
        input.colors[2].format == ALMetalPixelFormat::rgb10a2_unorm ||
        input.colors[2].format == ALMetalPixelFormat::rgba16_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 4 ||
        input.colors[0].format != ALMetalPixelFormat::rgba8_unorm_srgb ||
        input.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        !normal_format || input.colors[3].format != expected_emissive ||
        input.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !input.depth_stencil_shader_read ||
        !sampleableTexture(albedo) || !sampleableTexture(specular) ||
        !sampleableTexture(normal) || !sampleableTexture(emissive) ||
        !sampleableTexture(depth))
    {
        setError(error, "Metal world local-light G-buffer is incompatible");
        return nullptr;
    }

    for (std::size_t index = 0; index < scene.light_count; ++index)
    {
        const ALMetalWorldLocalLight& light = scene.lights[index];
        if (!finiteValues(light.position_radius, 4) ||
            !finiteValues(light.color_falloff, 4) ||
            light.position_radius[3] <= 0.f ||
            light.color_falloff[3] <= -1.f)
        {
            setError(error, "Metal world local point light is invalid");
            return nullptr;
        }
    }

    std::unique_ptr<ALMetalBuffer> light_buffer;
    if (scene.light_count != 0)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = scene.light_count * sizeof(ALMetalWorldLocalLight);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy world local-point-light list";
        light_buffer = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!light_buffer || !light_buffer->write(
                0, scene.lights, descriptor.length, error))
        {
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world local-point-light accumulation";
    output_descriptor.label =
        "Alchemy native world local-point-light target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native world local-point-light accumulation", error);
    if (!command)
    {
        return nullptr;
    }

    constexpr std::uint32_t brdf_lut_size = 512;
    const bool generate_brdf_lut =
        scene.light_count != 0 && !mImpl->brdf_lut_ready;
    if (generate_brdf_lut)
    {
        auto compute = ALMetalComputeEncoder::begin(
            *command, "Alchemy world PBR BRDF LUT encoder", error);
        if (!compute ||
            !compute->setPipeline(*mImpl->brdf_lut_pipeline, error) ||
            !compute->setTexture(0, mImpl->brdf_lut.get(), error) ||
            !compute->dispatch(
                brdf_lut_size, brdf_lut_size, 1, 8, 8, 1, error) ||
            !compute->end(error))
        {
            return nullptr;
        }
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world local-point-light encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    bool encoded = true;
    if (scene.light_count != 0)
    {
        WorldLocalLightingConstants constants;
        std::memcpy(constants.inverse_projection,
                    scene.inverse_projection,
                    sizeof(constants.inverse_projection));
        constants.surface[0] = scene.specular_exponent;
        constants.surface[1] = scene.classic_mode ? 0.9f : 1.f;
        constants.config[0] =
            static_cast<std::uint32_t>(scene.light_count);
        constants.config[1] = scene.reverse_z ? 1u : 0u;
        const ALMetalViewport viewport{
            0.0, 0.0,
            static_cast<double>(input.width),
            static_cast<double>(input.height), 0.0, 1.0,
        };
        const ALMetalScissorRect scissor{
            0, 0, input.width, input.height,
        };
        encoded =
            encoder->setViewport(viewport, error) &&
            encoder->setScissor(scissor, error) &&
            encoder->setCullMode(ALMetalCullMode::none, error) &&
            encoder->setPipeline(
                *mImpl->local_light_pipelines[scene.hdr ? 1 : 0], error) &&
            encoder->setFragmentBuffer(0, *light_buffer, 0, error) &&
            encoder->setFragmentBytes(
                1, &constants, sizeof(constants), error) &&
            encoder->setFragmentTexture(0, albedo, error) &&
            encoder->setFragmentTexture(1, specular, error) &&
            encoder->setFragmentTexture(2, normal, error) &&
            encoder->setFragmentTexture(3, depth, error) &&
            encoder->setFragmentTexture(4, mImpl->brdf_lut.get(), error) &&
            encoder->setFragmentSampler(
                0, mImpl->point_clamp_sampler.get(), error) &&
            encoder->setFragmentSampler(
                1, mImpl->linear_clamp_sampler.get(), error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    }
    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return nullptr;
    }
    if (generate_brdf_lut)
    {
        // The renderer uses one queue. A later command therefore sees this
        // generation without a CPU wait, while this command's tracked hazard
        // orders the fragment read after the compute write.
        mImpl->brdf_lut_ready = true;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderProjectorLights(
    const ALMetalWorldProjectorLightingScene& scene,
    std::string* error)
{
    constexpr std::size_t max_light_count = 65535;
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        (scene.light_count != 0 && !scene.lights) ||
        scene.light_count > max_light_count ||
        !finiteValues(scene.inverse_projection, 16) ||
        !std::isfinite(scene.specular_exponent) ||
        scene.specular_exponent <= 0.f)
    {
        setError(error, "Metal world projector-light scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.gbuffer->descriptor();
    const ALMetalTexture* albedo = scene.gbuffer->colorTexture(0);
    const ALMetalTexture* specular = scene.gbuffer->colorTexture(1);
    const ALMetalTexture* normal = scene.gbuffer->colorTexture(2);
    const ALMetalTexture* emissive = scene.gbuffer->colorTexture(3);
    const ALMetalTexture* depth = scene.gbuffer->depthStencilTexture();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 4 ||
        input.colors[0].format != ALMetalPixelFormat::rgba8_unorm_srgb ||
        input.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        input.colors[2].format != expected_normal ||
        input.colors[3].format != expected_emissive ||
        input.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !input.depth_stencil_shader_read ||
        !sampleableTexture(albedo) || !sampleableTexture(specular) ||
        !sampleableTexture(normal) || !sampleableTexture(emissive) ||
        !sampleableTexture(depth))
    {
        setError(error, "Metal world projector-light G-buffer is incompatible");
        return nullptr;
    }

    bool needs_shadows = false;
    for (std::size_t index = 0; index < scene.light_count; ++index)
    {
        const ALMetalWorldProjectorLight& light = scene.lights[index];
        const ALMetalTextureDescriptor* cookie = light.cookie_texture
            ? &light.cookie_texture->descriptor() : nullptr;
        if (!sampleableTexture(light.cookie_texture) || !cookie ||
            cookie->type != ALMetalTextureType::texture_2d ||
            cookie->sample_count != 1 ||
            !finiteValues(light.position_radius, 4) ||
            !finiteValues(light.color_falloff, 4) ||
            !finiteValues(light.projector_matrix, 16) ||
            !finiteValues(light.projector_plane, 4) ||
            !finiteValues(light.projector_normal, 4) ||
            !finiteValues(light.projector_origin, 4) ||
            !finiteValues(light.sampling, 4) ||
            light.position_radius[3] <= 0.f ||
            light.color_falloff[3] <= -1.f ||
            light.projector_normal[3] <= 0.f ||
            light.projector_origin[3] < 0.f ||
            light.sampling[0] < 1.f || light.sampling[1] < 0.f ||
            light.sampling[1] > 1.f || light.shadow_channel < -1 ||
            light.shadow_channel > 1)
        {
            setError(error, "Metal world projector light is invalid");
            return nullptr;
        }
        needs_shadows = needs_shadows || light.shadow_channel >= 0;
    }
    if (needs_shadows)
    {
        if (!validProjectorShadows(scene.projector_shadows))
        {
            setError(error,
                     "Metal world projector shadows are incompatible");
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world projector-light accumulation";
    output_descriptor.label =
        "Alchemy native world projector-light target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native world projector-light accumulation", error);
    if (!command)
    {
        return nullptr;
    }

    constexpr std::uint32_t brdf_lut_size = 512;
    const bool generate_brdf_lut =
        scene.light_count != 0 && !mImpl->brdf_lut_ready;
    if (generate_brdf_lut)
    {
        auto compute = ALMetalComputeEncoder::begin(
            *command, "Alchemy world projector BRDF LUT encoder", error);
        if (!compute ||
            !compute->setPipeline(*mImpl->brdf_lut_pipeline, error) ||
            !compute->setTexture(0, mImpl->brdf_lut.get(), error) ||
            !compute->dispatch(
                brdf_lut_size, brdf_lut_size, 1, 8, 8, 1, error) ||
            !compute->end(error))
        {
            return nullptr;
        }
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world projector-light encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0,
        static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    const ALMetalTexture* projector_shadow_depth = needs_shadows
        ? scene.projector_shadows->depth_texture
        : mImpl->unshadowed_depth.get();
    const ALMetalWorldDirectionalShadowConstants& projector_shadows =
        needs_shadows
            ? scene.projector_shadows->constants
            : mImpl->unshadowed_constants;
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->projector_light_pipelines[scene.hdr ? 1 : 0], error) &&
        encoder->setFragmentTexture(0, albedo, error) &&
        encoder->setFragmentTexture(1, specular, error) &&
        encoder->setFragmentTexture(2, normal, error) &&
        encoder->setFragmentTexture(3, depth, error) &&
        encoder->setFragmentTexture(4, mImpl->brdf_lut.get(), error) &&
        encoder->setFragmentTexture(6, projector_shadow_depth, error) &&
        encoder->setFragmentBytes(
            1, &projector_shadows, sizeof(projector_shadows), error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->setFragmentSampler(
            1, mImpl->linear_clamp_sampler.get(), error) &&
        encoder->setFragmentSampler(
            2, mImpl->diffuse_sampler.get(), error);

    for (std::size_t index = 0; encoded && index < scene.light_count;
         ++index)
    {
        const ALMetalWorldProjectorLight& light = scene.lights[index];
        WorldProjectorLightingConstants constants;
        std::memcpy(constants.inverse_projection,
                    scene.inverse_projection,
                    sizeof(constants.inverse_projection));
        std::memcpy(constants.projector_matrix,
                    light.projector_matrix,
                    sizeof(constants.projector_matrix));
        std::memcpy(constants.position_radius, light.position_radius,
                    sizeof(constants.position_radius));
        std::memcpy(constants.color_falloff, light.color_falloff,
                    sizeof(constants.color_falloff));
        std::memcpy(constants.projector_plane, light.projector_plane,
                    sizeof(constants.projector_plane));
        std::memcpy(constants.projector_normal, light.projector_normal,
                    sizeof(constants.projector_normal));
        std::memcpy(constants.projector_origin, light.projector_origin,
                    sizeof(constants.projector_origin));
        constants.sampling[0] = light.sampling[0];
        constants.sampling[1] = light.sampling[1];
        constants.sampling[2] = scene.specular_exponent;
        constants.sampling[3] = scene.classic_mode ? 0.9f : 1.f;
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = static_cast<std::uint32_t>(
            light.shadow_channel + 1);
        constants.config[2] = scene.normal_hdr ? 1u : 0u;
        encoded =
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->setFragmentTexture(
                5, light.cookie_texture, error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    }

    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return nullptr;
    }
    if (generate_brdf_lut)
    {
        mImpl->brdf_lut_ready = true;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::composeLighting(
    const ALMetalWorldLightingCompositionScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.base_lighting ||
        !scene.local_lighting || !scene.projector_lighting)
    {
        setError(error, "Metal world lighting composition is incomplete");
        return nullptr;
    }

    const ALMetalPixelFormat expected_format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    const ALMetalRenderTargetDescriptor& base =
        scene.base_lighting->descriptor();
    const auto compatible = [&](const ALMetalRenderTarget& target)
    {
        const ALMetalRenderTargetDescriptor& descriptor =
            target.descriptor();
        return descriptor.width == base.width &&
            descriptor.height == base.height &&
            descriptor.sample_count == 1 &&
            descriptor.color_attachment_count == 1 &&
            descriptor.colors[0].format == expected_format &&
            sampleableTexture(target.colorTexture(0));
    };
    if (base.width == 0 || base.height == 0 ||
        !compatible(*scene.base_lighting) ||
        !compatible(*scene.local_lighting) ||
        !compatible(*scene.projector_lighting))
    {
        setError(error,
                 "Metal world lighting composition targets are incompatible");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = base.width;
    output_descriptor.height = base.height;
    output_descriptor.colors[0].format = expected_format;
    output_descriptor.colors[0].label =
        "Alchemy world composed deferred lighting";
    output_descriptor.label =
        "Alchemy native world deferred-light composition target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native world deferred-light composition", error);
    if (!command)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass,
        "Alchemy world deferred-light composition encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(base.width),
        static_cast<double>(base.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{
        0, 0, base.width, base.height,
    };
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->lighting_composition_pipelines[scene.hdr ? 1 : 0],
            error) &&
        encoder->setFragmentTexture(
            0, scene.base_lighting->colorTexture(0), error) &&
        encoder->setFragmentTexture(
            1, scene.local_lighting->colorTexture(0), error) &&
        encoder->setFragmentTexture(
            2, scene.projector_lighting->colorTexture(0), error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderOpaquePostDeferred(
    const ALMetalWorldOpaquePostDeferredScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        !scene.composed_lighting || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0)
    {
        setError(error, "Metal world opaque post-deferred scene is incomplete");
        return nullptr;
    }

    const ALMetalPixelFormat color_format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    const ALMetalRenderTargetDescriptor& gbuffer =
        scene.gbuffer->descriptor();
    const ALMetalRenderTargetDescriptor& lighting =
        scene.composed_lighting->descriptor();
    if (gbuffer.width != scene.width || gbuffer.height != scene.height ||
        gbuffer.sample_count != 1 || gbuffer.color_attachment_count != 4 ||
        gbuffer.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !scene.gbuffer->depthStencilTexture() ||
        lighting.width != scene.width || lighting.height != scene.height ||
        lighting.sample_count != 1 || lighting.color_attachment_count != 1 ||
        lighting.colors[0].format != color_format ||
        !sampleableTexture(scene.composed_lighting->colorTexture(0)))
    {
        setError(error, "Metal world opaque post-deferred inputs are incompatible");
        return nullptr;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    constexpr std::size_t float2_stride = sizeof(float) * 2;
    constexpr std::size_t color_stride = sizeof(std::uint32_t);
    constexpr std::uint32_t max_skin_joints = 110;
    bool has_shiny = false;
    bool has_water_exclusion = false;
    bool has_water_haze = false;
    bool has_pre_water_alpha = false;
    bool has_pre_water_simple = false;
    bool has_pre_water_material = false;
    bool has_pre_water_shadows = false;
    bool saw_pre_water_alpha = false;
    bool saw_post_exclusion = false;
    const auto valid_skinning = [=](
        const ALMetalWorldSimpleBatch& batch, bool rigged)
    {
        return rigged
            ? batch.skin_joint_count != 0 &&
                  batch.skin_joint_count <= max_skin_joints &&
                  batch.skin_palette_float_count ==
                      4 + static_cast<std::size_t>(
                              batch.skin_joint_count) * 12 &&
                  batch.weight_offset % float4_stride == 0 &&
                  rangeFits(batch.weight_offset, batch.vertex_count,
                            float4_stride, batch.vertices->length()) &&
                  finiteValues(batch.skin_palette,
                               batch.skin_palette_float_count)
            : batch.skin_palette_float_count == 0 &&
                  batch.skin_joint_count == 0;
    };
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const std::uint32_t family_count =
            (batch.impostor_fullbright ? 1u : 0u) +
            (batch.impostor_shiny ? 1u : 0u) +
            (batch.impostor_emboss ? 1u : 0u) +
            (batch.water_exclusion ? 1u : 0u) +
            (batch.water_haze ? 1u : 0u) +
            (batch.pre_water_alpha ? 1u : 0u) +
            (batch.legacy_glow ? 1u : 0u) +
            (batch.pbr_glow ? 1u : 0u);
        const bool rigged = batch.skin_palette != nullptr;
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (family_count != 1 ||
            ((batch.pre_water_fullbright || batch.pre_water_simple ||
              batch.pre_water_material || batch.pre_water_shadows) &&
             !batch.pre_water_alpha))
        {
            setError(error, "Metal world opaque post-deferred batch is invalid");
            return nullptr;
        }
        if ((has_water_haze && !batch.pre_water_alpha) ||
            (saw_pre_water_alpha && !batch.pre_water_alpha))
        {
            setError(error,
                     "Metal world water-haze ordering is invalid");
            return nullptr;
        }
        if (batch.water_haze)
        {
            if (has_water_haze || saw_pre_water_alpha ||
                batch.vertices || batch.indices ||
                batch.vertex_count != 0 || batch.index_count != 0 ||
                batch.texture_count != 0 || batch.normal_texture ||
                batch.material_slot_count != 0 ||
                batch.pbr_slot_count != 0 || rigged ||
                batch.skin_palette_float_count != 0 ||
                batch.skin_joint_count != 0)
            {
                setError(error, "Metal world water-haze marker is invalid");
                return nullptr;
            }
            has_water_haze = true;
            saw_post_exclusion = true;
            continue;
        }
        if (!batch.vertices || !batch.indices ||
            batch.vertex_count == 0 || batch.index_count == 0 ||
            batch.normal_texture || batch.impostor || batch.terrain ||
            batch.avatar_rigid || batch.avatar_skinning ||
            batch.avatar_cloth ||
            batch.index_offset % index_stride != 0 ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()) ||
            batch.position_offset % float4_stride != 0 ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !finiteValues(batch.modelview_projection, 16))
        {
            setError(error, "Metal world opaque post-deferred batch is invalid");
            return nullptr;
        }
        if (batch.water_exclusion)
        {
            if (saw_post_exclusion || batch.texture_count != 0 || rigged ||
                batch.material_slot_count != 0 ||
                batch.pbr_slot_count != 0 ||
                batch.skin_palette_float_count != 0 ||
                batch.skin_joint_count != 0 ||
                !finiteValues(batch.modelview, 16))
            {
                setError(
                    error,
                    "Metal world water-exclusion batch is invalid");
                return nullptr;
            }
            has_water_exclusion = true;
            continue;
        }
        saw_post_exclusion = true;
        if (batch.pre_water_alpha)
        {
            has_pre_water_alpha = true;
            has_pre_water_simple =
                has_pre_water_simple || batch.pre_water_simple;
            has_pre_water_material =
                has_pre_water_material || batch.pre_water_material;
            has_pre_water_shadows =
                has_pre_water_shadows || batch.pre_water_shadows;
            saw_pre_water_alpha = true;
        }
        const bool glow = batch.legacy_glow || batch.pbr_glow;
        const bool alpha = batch.pre_water_alpha;
        if (batch.texture_count == 0 ||
            batch.texture_count >
                ALMetalWorldSimpleBatch::max_diffuse_textures ||
            batch.texcoord_offset % float2_stride != 0 ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       float2_stride, batch.vertices->length()) ||
            !finiteValues(batch.texture_matrix, 16) ||
            !valid_skinning(batch, rigged) ||
            (glow &&
             (batch.material_slot_count != 0 || batch.alpha_mask ||
              batch.material_alpha_mode != 0 ||
              batch.emissive_offset % color_stride != 0 ||
              !rangeFits(batch.emissive_offset, batch.vertex_count,
                         color_stride, batch.vertices->length()))) ||
            (batch.legacy_glow &&
             (batch.pbr_slot_count != 0 || batch.double_sided)) ||
            (batch.pbr_glow &&
             (batch.pbr_slot_count == 0 ||
              batch.pbr_slot_count != batch.texture_count ||
              batch.pbr_slot_count >
                  ALMetalWorldSimpleBatch::max_material_slots)) ||
            (alpha &&
             ((batch.pre_water_fullbright ? 1u : 0u) +
                      (batch.pre_water_simple ? 1u : 0u) +
                      (batch.pre_water_material ? 1u : 0u) != 1u ||
              (batch.pre_water_shadows &&
               !batch.pre_water_simple &&
               !batch.pre_water_material) ||
              batch.alpha_mask ||
              batch.double_sided ||
              batch.pbr_slot_count != 0 ||
              (batch.pre_water_material
                   ? (batch.material_alpha_mode != 1 ||
                      batch.material_slot_count == 0 ||
                      batch.material_slot_count != batch.texture_count ||
                      batch.material_slot_count >
                          ALMetalWorldSimpleBatch::max_material_slots)
                   : (batch.material_alpha_mode != 0 ||
                      batch.material_slot_count != 0)) ||
              !finiteValues(batch.modelview, 16) ||
              !finiteValues(&batch.normal_matrix[0][0], 12) ||
              !finiteValues(&batch.rigged_normal_matrix[0][0], 12))) ||
            (!glow && !alpha &&
             (batch.material_slot_count != 0 ||
              batch.pbr_slot_count != 0)) ||
            (batch.impostor_fullbright &&
             (!std::isfinite(batch.alpha_cutoff) ||
              batch.alpha_cutoff < 0.f || batch.alpha_cutoff > 1.f)) ||
            (!glow && !batch.impostor_emboss &&
             (batch.normal_offset % float4_stride != 0 ||
              batch.color_offset % color_stride != 0 ||
              !rangeFits(batch.normal_offset, batch.vertex_count,
                         float4_stride, batch.vertices->length()) ||
              !rangeFits(batch.color_offset, batch.vertex_count,
                         color_stride, batch.vertices->length()))) ||
            (batch.impostor_emboss &&
             (batch.texcoord1_offset % float2_stride != 0 ||
              !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                         float2_stride, batch.vertices->length()))) ||
            (batch.impostor_shiny &&
             (!finiteValues(batch.modelview, 16) ||
              !finiteValues(&batch.normal_matrix[0][0], 12) ||
              !finiteValues(&batch.rigged_normal_matrix[0][0], 12))) ||
            (batch.pre_water_material &&
             ((materialHasNormalMap(batch) &&
               (batch.tangent_offset % float4_stride != 0 ||
                batch.texcoord1_offset % float2_stride != 0 ||
                !rangeFits(batch.tangent_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                           float2_stride, batch.vertices->length()))) ||
              (materialHasSpecularMap(batch) &&
               (batch.texcoord2_offset % float2_stride != 0 ||
                !rangeFits(batch.texcoord2_offset, batch.vertex_count,
                           float2_stride, batch.vertices->length()))))))
        {
            setError(error, "Metal world opaque post-deferred batch is invalid");
            return nullptr;
        }
        for (std::uint32_t texture = 0;
             texture < batch.texture_count; ++texture)
        {
            if (!sampleableTexture(batch.diffuse_textures[texture]))
            {
                setError(error,
                         "Metal world opaque post-deferred texture is invalid");
                return nullptr;
            }
        }
        if (batch.pre_water_material)
        {
            const bool normal_map = materialHasNormalMap(batch);
            const bool specular_map = materialHasSpecularMap(batch);
            for (std::uint32_t slot = 0;
                 slot < batch.material_slot_count; ++slot)
            {
                const ALMetalWorldMaterialSlot& material =
                    batch.material_slots[slot];
                if (!finiteValues(material.specular_color, 4) ||
                    !std::isfinite(material.env_intensity) ||
                    !std::isfinite(material.fullbright) ||
                    material.env_intensity < 0.f ||
                    material.env_intensity > 1.f ||
                    material.fullbright < 0.f ||
                    material.fullbright > 1.f ||
                    (normal_map &&
                     !sampleableTexture(material.normal_texture)) ||
                    (specular_map &&
                     !sampleableTexture(material.specular_texture)))
                {
                    setError(
                        error,
                        "Metal world pre-water legacy material is invalid");
                    return nullptr;
                }
            }
        }
        if (batch.pbr_glow)
        {
            for (std::uint32_t slot = 0;
                 slot < batch.pbr_slot_count; ++slot)
            {
                const ALMetalWorldPBRSlot& material =
                    batch.pbr_slots[slot];
                if (!sampleableTexture(material.emissive_texture) ||
                    !finiteValues(
                        &material.texture_transform[0][0], 32) ||
                    !finiteValues(material.emissive_color, 4) ||
                    !finiteValues(material.base_color_factor, 4) ||
                    material.base_color_factor[3] < 0.f ||
                    material.base_color_factor[3] > 1.f ||
                    !std::isfinite(material.alpha_cutoff) ||
                    material.alpha_cutoff < -1.f ||
                    material.alpha_cutoff > 1.f)
                {
                    setError(error,
                             "Metal world PBR glow slot is invalid");
                    return nullptr;
                }
            }
        }
        has_shiny = has_shiny || batch.impostor_shiny;
    }
    if (has_water_exclusion)
    {
        const ALMetalWorldWaterExclusionSurface* surface =
            scene.water_exclusion_surface;
        const ALMetalTextureDescriptor* cell_map = surface &&
                surface->cell_map
            ? &surface->cell_map->descriptor() : nullptr;
        if (!surface || !cell_map ||
            cell_map->type != ALMetalTextureType::texture_2d ||
            cell_map->format != ALMetalPixelFormat::rg32_float ||
            cell_map->width == 0 || cell_map->height == 0 ||
            cell_map->sample_count != 1 ||
            !sampleableTexture(surface->cell_map) ||
            !finiteValues(surface->eye_to_agent, 16) ||
            !finiteValues(surface->cell_square, 4) ||
            !finiteValues(surface->camera_default_height, 4) ||
            surface->cell_square[2] <= 0.f ||
            surface->cell_square[3] <= 0.f ||
            surface->cell_square[3] >
                static_cast<float>(std::min(
                    cell_map->width, cell_map->height)))
        {
            setError(error,
                     "Metal world water-exclusion surface is invalid");
            return nullptr;
        }
    }
    if (has_water_haze)
    {
        const ALMetalWorldWaterHaze* haze = scene.water_haze;
        if (!haze || !gbuffer.depth_stencil_shader_read ||
            !sampleableTexture(scene.gbuffer->depthStencilTexture()) ||
            !finiteValues(haze->modelview_projection, 16) ||
            !finiteValues(haze->inverse_projection, 16) ||
            !finiteValues(haze->cell_square, 4) ||
            !finiteValues(haze->default_height, 4) ||
            !finiteValues(haze->rings, 4) ||
            !finiteValues(haze->water_plane, 4) ||
            !finiteValues(haze->fog_color, 4) ||
            !finiteValues(haze->fog, 4) ||
            haze->fog[0] <= 0.f || haze->fog[1] < 0.f)
        {
            setError(error, "Metal world water-haze inputs are invalid");
            return nullptr;
        }
        if (haze->above_water)
        {
            const ALMetalTextureDescriptor* cell_map = haze->cell_map
                ? &haze->cell_map->descriptor() : nullptr;
            if (!cell_map ||
                cell_map->type != ALMetalTextureType::texture_2d ||
                cell_map->format != ALMetalPixelFormat::rg32_float ||
                cell_map->width == 0 || cell_map->height == 0 ||
                cell_map->sample_count != 1 ||
                !sampleableTexture(haze->cell_map) ||
                haze->cell_square[2] <= 0.f ||
                haze->cell_square[3] <= 0.f ||
                haze->cell_square[3] > static_cast<float>(std::min(
                    cell_map->width, cell_map->height)) ||
                haze->rings[0] <= 0.f || haze->rings[1] <= 0.f ||
                haze->rings[2] <= 0.f || haze->draw_count != 3)
            {
                setError(error, "Metal world water-haze field is invalid");
                return nullptr;
            }
            for (std::uint32_t draw = 0; draw < haze->draw_count;
                 ++draw)
            {
                const ALMetalWorldWaterFieldDraw& field =
                    haze->draws[draw];
                if (field.part != draw || field.vertex_count == 0 ||
                    field.instance_count == 0 ||
                    field.triangle_strip != (draw == 0))
                {
                    setError(error,
                             "Metal world water-haze field draw is invalid");
                    return nullptr;
                }
            }
        }
        else if (haze->cell_map || haze->draw_count != 0)
        {
            setError(error, "Metal world underwater haze is invalid");
            return nullptr;
        }
    }
    if (has_pre_water_alpha)
    {
        const ALMetalWorldAlphaEnvironment* environment =
            scene.alpha_environment;
        const float water_normal_length = environment
            ? environment->water_plane[0] * environment->water_plane[0] +
                  environment->water_plane[1] * environment->water_plane[1] +
                  environment->water_plane[2] * environment->water_plane[2]
            : 0.f;
        const float light_length = environment
            ? environment->light_direction[0] *
                  environment->light_direction[0] +
                  environment->light_direction[1] *
                  environment->light_direction[1] +
                  environment->light_direction[2] *
                  environment->light_direction[2]
            : 0.f;
        if (!environment ||
            !finiteValues(environment->water_plane, 4) ||
            !finiteValues(&environment->eye_to_sky[0][0], 12) ||
            !finiteValues(environment->haze_extinction, 4) ||
            !finiteValues(environment->glow_max_y, 4) ||
            !finiteValues(environment->haze_additive, 4) ||
            !finiteValues(environment->haze_additive_glow, 4) ||
            !finiteValues(environment->water_fog_color, 4) ||
            !finiteValues(environment->light_direction, 4) ||
            !finiteValues(environment->atmos_sunlit, 4) ||
            !finiteValues(environment->atmos_sunlit_linear, 4) ||
            !finiteValues(environment->atmos_amblit, 4) ||
            !finiteValues(environment->atmospheric, 4) ||
            !finiteValues(environment->water_fog, 4) ||
            water_normal_length <= 0.f || light_length <= 0.f ||
            environment->atmospheric[0] < 0.f ||
            environment->atmospheric[1] < 0.f ||
            environment->atmospheric[3] < 0.f ||
            environment->atmos_sunlit[3] < 0.f ||
            environment->atmos_sunlit_linear[3] < 0.f ||
            environment->water_fog[0] <= 0.f ||
            environment->water_fog[1] < 0.f ||
            std::abs(environment->water_fog[2]) != 1.f)
        {
            setError(error,
                     "Metal world pre-water alpha environment is invalid");
            return nullptr;
        }
    }
    if ((has_pre_water_simple || has_pre_water_material) &&
        (!validLegacyAlphaLighting(scene.alpha_lighting) ||
         !validPBRReflectionProbes(scene.reflection_probes)))
    {
        setError(error,
                 "Metal world pre-water alpha lighting is invalid");
        return nullptr;
    }
    if (has_pre_water_shadows !=
            (scene.alpha_directional_shadows != nullptr) ||
        (has_pre_water_shadows &&
         !validDirectionalShadows(scene.alpha_directional_shadows)))
    {
        setError(error,
                 "Metal world pre-water alpha shadows are invalid");
        return nullptr;
    }
    if (has_shiny && !has_pre_water_simple && !has_pre_water_material &&
        !validReflectionProbes(scene.reflection_probes))
    {
        setError(error,
                 "Metal world opaque post-deferred reflection probes are invalid");
        return nullptr;
    }
    if (scene.hero_probe && !validHeroProbe(scene.hero_probe))
    {
        setError(error,
                 "Metal world opaque post-deferred hero probe is invalid");
        return nullptr;
    }
    if (scene.screen_space_reflections &&
        !validScreenSpaceReflections(scene.screen_space_reflections))
    {
        setError(
            error,
            "Metal world opaque post-deferred screen-space reflections are invalid");
        return nullptr;
    }

    std::vector<std::unique_ptr<ALMetalBuffer>> palette_storage;
    std::vector<const ALMetalBuffer*> palette_buffers(
        scene.batch_count, nullptr);
    for (std::size_t batch_index = 0;
         batch_index < scene.batch_count; ++batch_index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[batch_index];
        if (!batch.skin_palette)
        {
            continue;
        }
        for (std::size_t previous = 0; previous < batch_index; ++previous)
        {
            const ALMetalWorldSimpleBatch& candidate = scene.batches[previous];
            if (candidate.skin_palette == batch.skin_palette &&
                candidate.skin_palette_float_count ==
                    batch.skin_palette_float_count &&
                candidate.skin_joint_count == batch.skin_joint_count)
            {
                palette_buffers[batch_index] = palette_buffers[previous];
                break;
            }
        }
        if (palette_buffers[batch_index])
        {
            continue;
        }
        ALMetalBufferDescriptor descriptor;
        descriptor.length = batch.skin_palette_float_count * sizeof(float);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy world opaque post-deferred skin palette";
        auto palette = ALMetalBuffer::create(*mImpl->device, descriptor, error);
        if (!palette || !palette->write(
                0, batch.skin_palette, descriptor.length, error))
        {
            return nullptr;
        }
        palette_buffers[batch_index] = palette.get();
        palette_storage.push_back(std::move(palette));
    }

    std::unique_ptr<ALMetalBuffer> probe_constants_buffer;
    if (has_shiny || has_pre_water_simple || has_pre_water_material)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = sizeof(ALMetalWorldReflectionProbeConstants);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label =
            "Alchemy world opaque post-deferred reflection constants";
        probe_constants_buffer = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!probe_constants_buffer || !probe_constants_buffer->write(
                0, &scene.reflection_probes->constants,
                descriptor.length, error))
        {
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = scene.width;
    output_descriptor.height = scene.height;
    output_descriptor.colors[0].format = color_format;
    output_descriptor.colors[0].label =
        "Alchemy world post-deferred final color";
    output_descriptor.depth_stencil_format =
        ALMetalPixelFormat::depth32_float_stencil8;
    output_descriptor.label =
        "Alchemy native world opaque post-deferred target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }
    std::unique_ptr<ALMetalRenderTarget> haze_depth_snapshot;
    if (has_water_haze)
    {
        ALMetalRenderTargetDescriptor haze_depth_descriptor;
        haze_depth_descriptor.width = scene.width;
        haze_depth_descriptor.height = scene.height;
        haze_depth_descriptor.color_attachment_count = 0;
        haze_depth_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        haze_depth_descriptor.depth_stencil_shader_read = true;
        haze_depth_descriptor.label =
            "Alchemy native world water-haze depth snapshot";
        haze_depth_snapshot = ALMetalRenderTarget::create(
            *mImpl->device, haze_depth_descriptor, error);
        if (!haze_depth_snapshot)
        {
            return nullptr;
        }
    }
    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native world opaque post-deferred", error);
    if (!command ||
        !output->copyColorFrom(
            *command, 0, *scene.composed_lighting->colorTexture(0), {},
            error) ||
        !output->copyDepthStencilFrom(
            *command, *scene.gbuffer->depthStencilTexture(), {}, error))
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::load;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::clear;
    pass_config.stencil_store = ALMetalStoreAction::store;
    pass_config.clear_stencil = 0;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass,
        "Alchemy world opaque post-deferred encoder", error) : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error) &&
        encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error);
    const auto bind_forward_resources = [&](ALMetalRenderEncoder& active)
    {
        bool bound = true;
        if (has_shiny || has_pre_water_simple || has_pre_water_material)
        {
            bound =
            active.setFragmentSampler(
                1, mImpl->probe_sampler.get(), error) &&
            active.setFragmentTexture(
                32, scene.reflection_probes->radiance_texture, error) &&
            active.setFragmentBuffer(
                1, *probe_constants_buffer, 0, error);
        }
        if (bound && (has_pre_water_simple || has_pre_water_material))
        {
            bound = active.setFragmentTexture(
                37, scene.reflection_probes->irradiance_texture, error);
        }
        if (bound && (has_shiny || has_pre_water_material))
        {
            const ALMetalTexture* hero_texture = scene.hero_probe
                ? scene.hero_probe->radiance_texture
                : mImpl->inactive_hero_probe.get();
            const ALMetalWorldHeroProbeConstants& hero_constants =
                scene.hero_probe
                    ? scene.hero_probe->constants
                    : mImpl->inactive_hero_constants;
            bound =
                active.setFragmentTexture(
                    34, hero_texture, error) &&
                active.setFragmentBytes(
                    4, &hero_constants, sizeof(hero_constants), error);
        }
        if (bound && has_pre_water_material)
        {
            const ALMetalTexture* ssr_color =
                scene.screen_space_reflections
                    ? scene.screen_space_reflections->scene_color
                    : mImpl->inactive_ssr_color.get();
            const ALMetalTexture* ssr_depth =
                scene.screen_space_reflections
                    ? scene.screen_space_reflections->scene_depth
                    : mImpl->inactive_ssr_depth.get();
            const ALMetalWorldScreenSpaceReflectionConstants&
                ssr_constants = scene.screen_space_reflections
                    ? scene.screen_space_reflections->constants
                    : mImpl->inactive_ssr_constants;
            bound =
                active.setFragmentSampler(
                    2, mImpl->linear_clamp_sampler.get(), error) &&
                active.setFragmentSampler(
                    3, mImpl->point_clamp_sampler.get(), error) &&
                active.setFragmentTexture(35, ssr_color, error) &&
                active.setFragmentTexture(36, ssr_depth, error) &&
                active.setFragmentBytes(
                    5, &ssr_constants, sizeof(ssr_constants), error);
        }
        return bound;
    };
    encoded = encoded && bind_forward_resources(*encoder);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const bool rigged = batch.skin_palette != nullptr;
        if (batch.water_haze)
        {
            const ALMetalWorldWaterHaze& haze = *scene.water_haze;
            if (!encoder->end(error) || !haze_depth_snapshot ||
                !haze_depth_snapshot->copyDepthStencilFrom(
                    *command, *output->depthStencilTexture(), {}, error))
            {
                return nullptr;
            }
            ALMetalRenderPassConfig haze_pass_config;
            haze_pass_config.colors[0].load = ALMetalLoadAction::load;
            haze_pass_config.colors[0].store = ALMetalStoreAction::store;
            haze_pass_config.depth_load = ALMetalLoadAction::load;
            haze_pass_config.depth_store = ALMetalStoreAction::store;
            haze_pass_config.stencil_load = ALMetalLoadAction::load;
            haze_pass_config.stencil_store = ALMetalStoreAction::store;
            auto haze_pass = output->makeRenderPass(
                haze_pass_config, error);
            encoder = haze_pass ? ALMetalRenderEncoder::begin(
                *command, *haze_pass,
                "Alchemy world water-haze encoder", error) : nullptr;
            if (!encoder)
            {
                return nullptr;
            }
            encoded =
                encoder->setViewport(viewport, error) &&
                encoder->setScissor(scissor, error) &&
                encoder->setFrontFacingWinding(
                    ALMetalWinding::clockwise, error) &&
                encoder->setFragmentSampler(
                    0, mImpl->diffuse_sampler.get(), error);
            encoded = encoded && bind_forward_resources(*encoder);
            WorldWaterHazeConstants constants{};
            std::memcpy(constants.modelview_projection,
                        haze.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.inverse_projection,
                        haze.inverse_projection,
                        sizeof(constants.inverse_projection));
            std::memcpy(constants.cell_square, haze.cell_square,
                        sizeof(constants.cell_square));
            std::memcpy(constants.default_height, haze.default_height,
                        sizeof(constants.default_height));
            std::memcpy(constants.rings, haze.rings,
                        sizeof(constants.rings));
            std::memcpy(constants.water_plane, haze.water_plane,
                        sizeof(constants.water_plane));
            std::memcpy(constants.fog_color, haze.fog_color,
                        sizeof(constants.fog_color));
            std::memcpy(constants.fog, haze.fog,
                        sizeof(constants.fog));
            constants.config[0] = scene.reverse_z ? 1u : 0u;
            constants.config[1] = haze.above_water ? 1u : 0u;

            const ALMetalDepthStencilState* haze_depth =
                haze.above_water
                    ? (scene.reverse_z
                           ? mImpl->reverse_water_haze_depth.get()
                           : mImpl->forward_water_haze_depth.get())
                    : mImpl->underwater_water_haze_depth.get();
            encoded =
                encoder->setDepthStencilState(haze_depth, error) &&
                encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
                encoder->setStencilReferenceValue(0u, error) &&
                encoder->setCullMode(ALMetalCullMode::none, error) &&
                encoder->setPipeline(
                    *mImpl->water_haze_pipelines
                        [scene.hdr ? 1 : 0][haze.above_water ? 1 : 0],
                    error) &&
                encoder->setFragmentTexture(
                    33, haze_depth_snapshot->depthStencilTexture(),
                    error) &&
                encoder->setFragmentSampler(
                    1, mImpl->point_clamp_sampler.get(), error) &&
                encoder->setFragmentBytes(
                    0, &constants, sizeof(constants), error);
            if (encoded && haze.above_water)
            {
                encoded = encoder->setVertexTexture(
                    0, haze.cell_map, error);
                for (std::uint32_t draw = 0;
                     encoded && draw < haze.draw_count; ++draw)
                {
                    const ALMetalWorldWaterFieldDraw& field =
                        haze.draws[draw];
                    constants.config[2] = field.part;
                    encoded =
                        encoder->setVertexBytes(
                            0, &constants, sizeof(constants), error) &&
                        encoder->draw(
                            field.triangle_strip
                                ? ALMetalPrimitiveType::triangle_strip
                                : ALMetalPrimitiveType::triangle,
                            0, field.vertex_count,
                            field.instance_count, error);
                }
            }
            else if (encoded)
            {
                encoded =
                    encoder->setVertexBytes(
                        0, &constants, sizeof(constants), error) &&
                    encoder->draw(
                        ALMetalPrimitiveType::triangle, 0, 3, 1, error);
            }
            continue;
        }
        if (batch.pre_water_alpha)
        {
            WorldShinyConstants world{};
            std::memcpy(world.modelview_projection,
                        batch.modelview_projection,
                        sizeof(world.modelview_projection));
            std::memcpy(world.modelview, batch.modelview,
                        sizeof(world.modelview));
            std::memcpy(world.normal_matrix, batch.normal_matrix,
                        sizeof(world.normal_matrix));
            std::memcpy(world.rigged_normal_matrix,
                        batch.rigged_normal_matrix,
                        sizeof(world.rigged_normal_matrix));
            std::memcpy(world.texture_matrix, batch.texture_matrix,
                        sizeof(world.texture_matrix));
            world.config[0] = scene.reverse_z ? 1u : 0u;
            world.config[1] = batch.texture_count;

            const ALMetalDepthStencilState* depth = scene.reverse_z
                ? mImpl->reverse_depth.get() : mImpl->forward_depth.get();
            encoded =
                encoder->setDepthStencilState(depth, error) &&
                encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
                encoder->setCullMode(ALMetalCullMode::back, error) &&
                encoder->setPipeline(
                    batch.pre_water_material
                        ? *mImpl->pre_water_material_pipelines
                              [scene.hdr ? 1 : 0][rigged ? 1 : 0]
                        : batch.pre_water_simple
                        ? *mImpl->pre_water_simple_pipelines
                              [scene.hdr ? 1 : 0]
                              [batch.pre_water_shadows ? 1 : 0]
                              [rigged ? 1 : 0]
                        : *mImpl->pre_water_fullbright_pipelines
                              [scene.hdr ? 1 : 0][rigged ? 1 : 0],
                    error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBuffer(
                    1, *batch.vertices, batch.normal_offset, error) &&
                encoder->setVertexBuffer(
                    2, *batch.vertices, batch.texcoord_offset, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.color_offset, error) &&
                encoder->setVertexBytes(
                    4, &world, sizeof(world), error);
            if (encoded && batch.pre_water_material)
            {
                const bool normal_map = materialHasNormalMap(batch);
                const bool specular_map = materialHasSpecularMap(batch);
                WorldMaterialConstants material{};
                for (std::size_t slot = 0;
                     slot < batch.material_slot_count; ++slot)
                {
                    const ALMetalWorldMaterialSlot& source =
                        batch.material_slots[slot];
                    std::memcpy(
                        material.slots[slot].specular_color,
                        source.specular_color,
                        sizeof(material.slots[slot].specular_color));
                    material.slots[slot].properties[0] =
                        source.env_intensity;
                    material.slots[slot].properties[1] =
                        source.alpha_cutoff;
                    material.slots[slot].properties[2] =
                        source.fullbright;
                }
                material.config[0] = batch.material_slot_count;
                material.config[1] = batch.material_alpha_mode;
                material.config[2] = normal_map ? 1u : 0u;
                material.config[3] = specular_map ? 1u : 0u;
                const ALMetalTexture* directional_depth =
                    batch.pre_water_shadows
                        ? scene.alpha_directional_shadows->depth_texture
                        : mImpl->unshadowed_depth.get();
                const ALMetalWorldDirectionalShadowConstants&
                    directional_constants = batch.pre_water_shadows
                        ? scene.alpha_directional_shadows->constants
                        : mImpl->unshadowed_constants;
                encoded =
                    encoder->setVertexBuffer(
                        8, *batch.vertices,
                        normal_map
                            ? batch.tangent_offset : batch.normal_offset,
                        error) &&
                    encoder->setVertexBuffer(
                        9, *batch.vertices,
                        normal_map
                            ? batch.texcoord1_offset
                            : batch.texcoord_offset,
                        error) &&
                    encoder->setVertexBuffer(
                        10, *batch.vertices,
                        specular_map
                            ? batch.texcoord2_offset
                            : batch.texcoord_offset,
                        error) &&
                    encoder->setFragmentBytes(
                        0, &material, sizeof(material), error) &&
                    encoder->setFragmentBytes(
                        2, scene.alpha_lighting,
                        sizeof(*scene.alpha_lighting), error) &&
                    encoder->setFragmentTexture(
                        33, directional_depth, error) &&
                    encoder->setFragmentBytes(
                        3, &directional_constants,
                        sizeof(directional_constants), error) &&
                    encoder->setFragmentBytes(
                        6, scene.alpha_environment,
                        sizeof(*scene.alpha_environment), error);
                for (std::size_t slot = 0;
                     encoded && slot <
                         ALMetalWorldSimpleBatch::max_material_slots;
                     ++slot)
                {
                    const std::size_t source = std::min<std::size_t>(
                        slot, batch.material_slot_count - 1);
                    const ALMetalTexture* normal_texture = normal_map
                        ? batch.material_slots[source].normal_texture
                        : batch.diffuse_textures[0];
                    const ALMetalTexture* specular_texture = specular_map
                        ? batch.material_slots[source].specular_texture
                        : batch.diffuse_textures[0];
                    encoded =
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(slot),
                            batch.diffuse_textures[source], error) &&
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(8 + slot),
                            normal_texture, error) &&
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(16 + slot),
                            specular_texture, error);
                }
            }
            else if (encoded)
            {
                encoded = encoder->setFragmentBytes(
                    0, scene.alpha_environment,
                    sizeof(*scene.alpha_environment), error);
                if (encoded && batch.pre_water_simple)
                {
                    encoded = encoder->setFragmentBytes(
                        2, scene.alpha_lighting,
                        sizeof(*scene.alpha_lighting), error);
                }
                if (encoded && batch.pre_water_shadows)
                {
                    encoded =
                        encoder->setFragmentTexture(
                            33,
                            scene.alpha_directional_shadows->depth_texture,
                            error) &&
                        encoder->setFragmentBytes(
                            3,
                            &scene.alpha_directional_shadows->constants,
                            sizeof(scene.alpha_directional_shadows
                                       ->constants),
                            error);
                }
            }
            if (encoded && rigged)
            {
                encoded =
                    encoder->setVertexBuffer(
                        5, *palette_buffers[index], 0, error) &&
                    encoder->setVertexBuffer(
                        6, *batch.vertices, batch.weight_offset, error) &&
                    encoder->setVertexBytes(
                        7, &batch.skin_joint_count,
                        sizeof(batch.skin_joint_count), error);
            }
            for (std::size_t texture = 0;
                 encoded && !batch.pre_water_material &&
                 texture < batch.texture_count; ++texture)
            {
                encoded = encoder->setFragmentTexture(
                    static_cast<std::uint32_t>(texture),
                    batch.diffuse_textures[texture], error);
            }
            encoded = encoded && encoder->drawIndexed(
                ALMetalPrimitiveType::triangle, batch.index_count,
                batch.index_type, *batch.indices,
                batch.index_offset, 1, error);
            continue;
        }
        if (batch.water_exclusion)
        {
            WorldWaterExclusionConstants constants{};
            std::memcpy(constants.modelview_projection,
                        batch.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.modelview, batch.modelview,
                        sizeof(constants.modelview));
            std::memcpy(constants.eye_to_agent,
                        scene.water_exclusion_surface->eye_to_agent,
                        sizeof(constants.eye_to_agent));
            std::memcpy(constants.cell_square,
                        scene.water_exclusion_surface->cell_square,
                        sizeof(constants.cell_square));
            std::memcpy(constants.camera_default_height,
                        scene.water_exclusion_surface
                            ->camera_default_height,
                        sizeof(constants.camera_default_height));
            constants.config[0] = scene.reverse_z ? 1u : 0u;
            const ALMetalDepthStencilState* water_depth = scene.reverse_z
                ? mImpl->reverse_water_exclusion_depth.get()
                : mImpl->forward_water_exclusion_depth.get();
            encoded =
                encoder->setDepthStencilState(water_depth, error) &&
                encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
                encoder->setStencilReferenceValue(1u, error) &&
                encoder->setCullMode(ALMetalCullMode::back, error) &&
                encoder->setPipeline(
                    *mImpl->water_exclusion_pipelines
                        [scene.hdr ? 1 : 0],
                    error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBytes(
                    1, &constants, sizeof(constants), error) &&
                encoder->setFragmentBytes(
                    0, &constants, sizeof(constants), error) &&
                encoder->setFragmentTexture(
                    0, scene.water_exclusion_surface->cell_map, error) &&
                encoder->drawIndexed(
                    ALMetalPrimitiveType::triangle, batch.index_count,
                    batch.index_type, *batch.indices,
                    batch.index_offset, 1, error);
            continue;
        }
        if (batch.legacy_glow || batch.pbr_glow)
        {
            WorldSimpleConstants world{};
            std::memcpy(world.modelview_projection,
                        batch.modelview_projection,
                        sizeof(world.modelview_projection));
            std::memcpy(world.texture_matrix, batch.texture_matrix,
                        sizeof(world.texture_matrix));
            world.depth_convention[0] = scene.reverse_z ? 1u : 0u;
            world.depth_convention[1] = batch.texture_count;

            const ALMetalDepthStencilState* depth = scene.reverse_z
                ? mImpl->reverse_readonly_depth.get()
                : mImpl->forward_readonly_depth.get();
            const ALMetalRenderPipeline* glow_pipeline = batch.pbr_glow
                ? mImpl->pbr_glow_pipelines[scene.hdr ? 1 : 0]
                                                 [rigged ? 1 : 0].get()
                : mImpl->legacy_glow_pipelines[scene.hdr ? 1 : 0]
                                                    [rigged ? 1 : 0].get();
            encoded =
                encoder->setDepthStencilState(depth, error) &&
                encoder->setDepthBias(
                    batch.legacy_glow
                        ? (scene.reverse_z ? 1.f : -1.f) : 0.f,
                    batch.legacy_glow
                        ? (scene.reverse_z ? 1.f : -1.f) : 0.f,
                    0.f, error) &&
                encoder->setCullMode(
                    batch.pbr_glow && batch.double_sided
                        ? ALMetalCullMode::none : ALMetalCullMode::back,
                    error) &&
                encoder->setPipeline(*glow_pipeline, error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBuffer(
                    2, *batch.vertices, batch.texcoord_offset, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.emissive_offset, error) &&
                encoder->setVertexBytes(
                    4, &world, sizeof(world), error);
            if (encoded && rigged)
            {
                encoded =
                    encoder->setVertexBuffer(
                        5, *palette_buffers[index], 0, error) &&
                    encoder->setVertexBuffer(
                        6, *batch.vertices, batch.weight_offset, error) &&
                    encoder->setVertexBytes(
                        7, &batch.skin_joint_count,
                        sizeof(batch.skin_joint_count), error);
            }
            if (encoded && batch.pbr_glow)
            {
                WorldPBRConstants pbr{};
                for (std::size_t slot = 0;
                     slot < batch.pbr_slot_count; ++slot)
                {
                    const ALMetalWorldPBRSlot& source =
                        batch.pbr_slots[slot];
                    WorldPBRSlotConstants& destination = pbr.slots[slot];
                    std::memcpy(destination.texture_transform,
                                source.texture_transform,
                                sizeof(destination.texture_transform));
                    std::memcpy(destination.emissive_color,
                                source.emissive_color,
                                sizeof(destination.emissive_color));
                    destination.properties[0] = source.roughness_factor;
                    destination.properties[1] = source.metallic_factor;
                    destination.properties[2] = source.alpha_cutoff;
                    destination.properties[3] =
                        source.base_color_factor[3];
                }
                pbr.config[0] = batch.pbr_slot_count;
                encoded =
                    encoder->setVertexBytes(
                        11, &pbr, sizeof(pbr), error) &&
                    encoder->setFragmentBytes(
                        0, &pbr, sizeof(pbr), error);
                for (std::size_t slot = 0;
                     encoded && slot <
                         ALMetalWorldSimpleBatch::max_material_slots; ++slot)
                {
                    const std::size_t source = std::min<std::size_t>(
                        slot, batch.pbr_slot_count - 1);
                    encoded =
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(slot),
                            batch.diffuse_textures[source], error) &&
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(8 + slot),
                            batch.pbr_slots[source].emissive_texture, error);
                }
            }
            else
            {
                for (std::size_t texture = 0;
                     encoded && texture <
                         ALMetalWorldSimpleBatch::max_diffuse_textures;
                     ++texture)
                {
                    const std::size_t source = std::min<std::size_t>(
                        texture, batch.texture_count - 1);
                    encoded = encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(texture),
                        batch.diffuse_textures[source], error);
                }
            }
            encoded = encoded && encoder->drawIndexed(
                ALMetalPrimitiveType::triangle, batch.index_count,
                batch.index_type, *batch.indices,
                batch.index_offset, 1, error);
            continue;
        }
        const ALMetalRenderPipeline* pipeline = nullptr;
        if (batch.impostor_emboss)
        {
            pipeline = mImpl->opaque_forward_emboss_pipelines
                [scene.hdr ? 1 : 0][rigged ? 1 : 0].get();
        }
        else if (batch.impostor_shiny)
        {
            pipeline = mImpl->opaque_forward_shiny_pipelines
                [scene.hdr ? 1 : 0][rigged ? 1 : 0].get();
        }
        else
        {
            pipeline = mImpl->opaque_forward_fullbright_pipelines
                [scene.hdr ? 1 : 0][batch.alpha_mask ? 1 : 0]
                [rigged ? 1 : 0].get();
        }

        WorldSimpleConstants world{};
        std::memcpy(world.modelview_projection,
                    batch.modelview_projection,
                    sizeof(world.modelview_projection));
        std::memcpy(world.normal_matrix, batch.normal_matrix,
                    sizeof(world.normal_matrix));
        std::memcpy(world.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(world.rigged_normal_matrix));
        std::memcpy(world.texture_matrix, batch.texture_matrix,
                    sizeof(world.texture_matrix));
        world.material[0] = batch.alpha_cutoff;
        world.material[3] = batch.alpha_mask ? 1.f : 0.f;
        world.depth_convention[0] = scene.reverse_z ? 1u : 0u;
        world.depth_convention[1] = batch.texture_count;

        WorldShinyConstants shiny_world{};
        std::memcpy(shiny_world.modelview_projection,
                    batch.modelview_projection,
                    sizeof(shiny_world.modelview_projection));
        std::memcpy(shiny_world.modelview, batch.modelview,
                    sizeof(shiny_world.modelview));
        std::memcpy(shiny_world.normal_matrix, batch.normal_matrix,
                    sizeof(shiny_world.normal_matrix));
        std::memcpy(shiny_world.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(shiny_world.rigged_normal_matrix));
        std::memcpy(shiny_world.texture_matrix, batch.texture_matrix,
                    sizeof(shiny_world.texture_matrix));
        shiny_world.config[0] = scene.reverse_z ? 1u : 0u;
        shiny_world.config[1] = batch.texture_count;

        const ALMetalDepthStencilState* depth = batch.impostor_emboss
            ? (scene.reverse_z ? mImpl->reverse_readonly_depth.get()
                               : mImpl->forward_readonly_depth.get())
            : (scene.reverse_z ? mImpl->reverse_depth.get()
                               : mImpl->forward_depth.get());
        encoded =
            encoder->setDepthStencilState(depth, error) &&
            encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
            encoder->setCullMode(ALMetalCullMode::back, error) &&
            encoder->setPipeline(*pipeline, error) &&
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                4,
                batch.impostor_shiny
                    ? static_cast<const void*>(&shiny_world)
                    : static_cast<const void*>(&world),
                batch.impostor_shiny
                    ? sizeof(shiny_world) : sizeof(world),
                error);
        if (encoded && batch.impostor_emboss)
        {
            encoded = encoder->setVertexBuffer(
                9, *batch.vertices, batch.texcoord1_offset, error);
        }
        else if (encoded)
        {
            encoded =
                encoder->setVertexBuffer(
                    1, *batch.vertices, batch.normal_offset, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.color_offset, error);
        }
        if (encoded && rigged)
        {
            encoded =
                encoder->setVertexBuffer(
                    5, *palette_buffers[index], 0, error) &&
                encoder->setVertexBuffer(
                    6, *batch.vertices, batch.weight_offset, error) &&
                encoder->setVertexBytes(
                    7, &batch.skin_joint_count,
                    sizeof(batch.skin_joint_count), error);
        }
        for (std::size_t texture = 0;
             encoded && texture < batch.texture_count; ++texture)
        {
            encoded = encoder->setFragmentTexture(
                static_cast<std::uint32_t>(texture),
                batch.diffuse_textures[texture], error);
        }
        encoded = encoded && encoder->drawIndexed(
            ALMetalPrimitiveType::triangle, batch.index_count,
            batch.index_type, *batch.indices, batch.index_offset, 1, error);
    }

    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderLightMap(
    const ALMetalWorldLightMapScene& scene,
    std::string* error)
{
    const ALMetalWorldLightMapConstants& constants = scene.constants;
    const bool directional_shadows = constants.config[1] != 0;
    const bool ssao = constants.config[2] != 0;
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        !finiteValues(constants.inverse_projection, 16) ||
        !finiteValues(constants.light_direction, 4) ||
        !finiteValues(constants.ssao, 4) ||
        !finiteValues(&constants.blur_taps[0][0], 16) ||
        !finiteValues(constants.blur, 4) ||
        !finiteValues(constants.screen, 4) ||
        constants.config[0] > 1 || constants.config[1] > 1 ||
        constants.config[2] > 1 || constants.config[3] > 1 ||
        (scene.directional_shadows &&
         !validDirectionalShadows(scene.directional_shadows)) ||
        (ssao && (!sampleableTexture(scene.noise_texture) ||
                  constants.ssao[0] < 0.f || constants.ssao[1] < 0.f ||
                  constants.ssao[2] < 0.f || constants.ssao[3] <= 0.f ||
                  constants.blur_taps[0][1] <= 0.f ||
                  constants.blur[0] < 0.f || constants.blur[1] < 0.f)))
    {
        setError(error, "Metal world light-map scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.gbuffer->descriptor();
    const ALMetalTexture* normal = scene.gbuffer->colorTexture(2);
    const ALMetalTexture* depth = scene.gbuffer->depthStencilTexture();
    const bool normal_format =
        input.colors[2].format == ALMetalPixelFormat::rgb10a2_unorm ||
        input.colors[2].format == ALMetalPixelFormat::rgba16_unorm;
    const ALMetalTextureDescriptor* noise = ssao
        ? &scene.noise_texture->descriptor() : nullptr;
    const bool valid_noise = !noise ||
        (noise->format == ALMetalPixelFormat::rg32_float &&
         noise->width == 128 && noise->height == 128 &&
         noise->sample_count == 1 && noise->mipmap_level_count == 1);
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 4 || !normal_format ||
        input.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !input.depth_stencil_shader_read ||
        constants.screen[0] != static_cast<float>(input.width) ||
        constants.screen[1] != static_cast<float>(input.height) ||
        !valid_noise || !sampleableTexture(normal) ||
        !sampleableTexture(depth))
    {
        setError(error, "Metal world light-map G-buffer is incompatible");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format = ALMetalPixelFormat::rg8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world directional-shadow and SSAO light map";
    output_descriptor.label = "Alchemy native world light-map target";
    auto raw = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!raw)
    {
        return nullptr;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native world light-map production", error);
    if (!command)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {1.0, 1.0, 0.0, 0.0};
    auto pass = raw->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world light-map encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalWorldDirectionalShadowConstants& shadow_constants =
        directional_shadows && scene.directional_shadows
            ? scene.directional_shadows->constants
            : mImpl->unshadowed_constants;
    const ALMetalTexture* shadow_texture =
        directional_shadows && scene.directional_shadows
            ? scene.directional_shadows->depth_texture
            : mImpl->unshadowed_depth.get();
    const ALMetalTexture* noise_texture = ssao
        ? scene.noise_texture : mImpl->inactive_ssr_color.get();
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->light_map_pipeline, error) &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->setFragmentBytes(
            1, &shadow_constants, sizeof(shadow_constants), error) &&
        encoder->setFragmentTexture(0, normal, error) &&
        encoder->setFragmentTexture(1, depth, error) &&
        encoder->setFragmentTexture(2, shadow_texture, error) &&
        encoder->setFragmentTexture(3, noise_texture, error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->setFragmentSampler(
            1, mImpl->point_repeat_sampler.get(), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
        encoder->end(error);
    if (!encoded)
    {
        return nullptr;
    }

    if (!ssao)
    {
        return command->commit(error) ? std::move(raw) : nullptr;
    }

    auto scratch = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!scratch || !output)
    {
        return nullptr;
    }
    const float horizontal[4] = {1.f, 0.f, 0.f, 0.f};
    const float vertical[4] = {0.f, 1.f, 0.f, 0.f};
    ALMetalRenderTarget* destinations[2] = {scratch.get(), output.get()};
    const ALMetalTexture* sources[2] = {
        raw->colorTexture(0), scratch->colorTexture(0),
    };
    const float* deltas[2] = {horizontal, vertical};
    for (std::size_t axis = 0; axis < 2; ++axis)
    {
        auto blur_pass = destinations[axis]->makeRenderPass(
            pass_config, error);
        auto blur_encoder = blur_pass ? ALMetalRenderEncoder::begin(
            *command, *blur_pass,
            axis == 0 ? "Alchemy world horizontal SSAO blur encoder"
                      : "Alchemy world vertical SSAO blur encoder",
            error) : nullptr;
        encoded = blur_encoder &&
            blur_encoder->setViewport(viewport, error) &&
            blur_encoder->setScissor(scissor, error) &&
            blur_encoder->setCullMode(ALMetalCullMode::none, error) &&
            blur_encoder->setPipeline(
                *mImpl->light_map_blur_pipeline, error) &&
            blur_encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            blur_encoder->setFragmentBytes(
                1, deltas[axis], sizeof(horizontal), error) &&
            blur_encoder->setFragmentTexture(0, sources[axis], error) &&
            blur_encoder->setFragmentTexture(1, normal, error) &&
            blur_encoder->setFragmentTexture(2, depth, error) &&
            blur_encoder->setFragmentSampler(
                0, mImpl->point_clamp_sampler.get(), error) &&
            blur_encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
            blur_encoder->end(error);
        if (!encoded)
        {
            return nullptr;
        }
    }

    return command->commit(error) ? std::move(output) : nullptr;
}

bool ALMetalWorldGBufferRenderer::compositeHDRISky(
    ALMetalRenderTarget& target,
    const ALMetalWorldHDRISkyScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || !sampleableTexture(
            scene.environment_texture) ||
        scene.width == 0 || scene.height == 0 ||
        !finiteValues(scene.camera_local, 4) ||
        !finiteValues(&scene.environment_rotation[0][0], 12) ||
        !std::isfinite(scene.exposure_scale) || scene.exposure_scale < 0.f ||
        !std::isfinite(scene.horizontal_fraction) ||
        scene.horizontal_fraction < 0.f ||
        scene.horizontal_fraction > 1.f)
    {
        setError(error, "Metal world HDRI sky scene is incomplete");
        return false;
    }

    const ALMetalTextureDescriptor& environment =
        scene.environment_texture->descriptor();
    if (environment.type != ALMetalTextureType::texture_2d ||
        environment.format != ALMetalPixelFormat::rgba16_float ||
        environment.width == 0 || environment.height == 0 ||
        environment.array_length != 1 ||
        environment.mipmap_level_count == 0 ||
        environment.sample_count != 1)
    {
        setError(error, "Metal world HDRI sky environment is incompatible");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world HDRI sky target is incompatible");
        return false;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count < 3 ||
            batch.position_offset % float4_stride != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal world HDRI sky batch is invalid");
            return false;
        }
    }

    const double fractional_width =
        static_cast<double>(scene.horizontal_fraction) * scene.width;
    const std::uint32_t scissor_width = static_cast<std::uint32_t>(
        std::clamp(std::ceil(fractional_width - 0.5),
                   0.0, static_cast<double>(scene.width)));
    if (scissor_width == 0)
    {
        return true;
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    auto command = pass ? mImpl->device->makeCommandBuffer(
        "Alchemy native world HDRI sky", error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world HDRI sky encoder", error) : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{
        0, 0, scissor_width, scene.height,
    };
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->hdri_sky_pipelines[scene.hdr ? 1 : 0]
                                 [scene.normal_hdr ? 1 : 0].get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error) &&
        encoder->setFragmentTexture(
            0, scene.environment_texture, error) &&
        encoder->setFragmentSampler(0, mImpl->hdri_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        WorldHDRISkyConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.camera_exposure, scene.camera_local,
                    sizeof(scene.camera_local));
        constants.camera_exposure[3] = scene.exposure_scale;
        std::memcpy(constants.environment_rotation,
                    scene.environment_rotation,
                    sizeof(constants.environment_rotation));
        constants.screen[0] = static_cast<float>(scene.width);
        constants.screen[1] = static_cast<float>(scene.height);
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBytes(
                1, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle_strip,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    return encoded && encoder->end(error) && command->commit(error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightSky(
    ALMetalRenderTarget& target,
    const ALMetalWorldWindLightSkyScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 ||
        !sampleableTexture(scene.rainbow_texture) ||
        !sampleableTexture(scene.halo_texture) ||
        scene.width == 0 || scene.height == 0 ||
        !finiteValues(scene.constants.camera_local, 4) ||
        !finiteValues(scene.constants.light_density, 4) ||
        !finiteValues(scene.constants.blue_horizon, 4) ||
        !finiteValues(scene.constants.glow, 4) ||
        !finiteValues(scene.constants.haze_extinction, 4) ||
        !finiteValues(scene.constants.blue_weight, 4) ||
        !finiteValues(scene.constants.haze_weight, 4) ||
        !finiteValues(scene.constants.sky_light, 4) ||
        !finiteValues(scene.constants.sky_ambient, 4) ||
        !finiteValues(scene.constants.sky_light_atten, 4) ||
        !finiteValues(scene.constants.sky_ambient_below, 4) ||
        !finiteValues(scene.constants.optics, 4) ||
        scene.constants.light_density[3] < 0.f ||
        scene.constants.glow[3] < 0.f ||
        scene.constants.optics[0] < 0.f ||
        scene.constants.optics[1] < 0.f ||
        scene.constants.optics[2] < 0.f ||
        !std::isfinite(scene.sky_hdr_scale) ||
        scene.sky_hdr_scale < 0.f)
    {
        setError(error, "Metal world WindLight sky scene is incomplete");
        return false;
    }

    const ALMetalTextureDescriptor& rainbow =
        scene.rainbow_texture->descriptor();
    const ALMetalTextureDescriptor& halo = scene.halo_texture->descriptor();
    if (rainbow.type != ALMetalTextureType::texture_2d ||
        halo.type != ALMetalTextureType::texture_2d ||
        rainbow.width == 0 || rainbow.height == 0 ||
        halo.width == 0 || halo.height == 0 ||
        rainbow.sample_count != 1 || halo.sample_count != 1)
    {
        setError(error, "Metal world WindLight sky optics are incompatible");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight sky target is incompatible");
        return false;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count < 3 ||
            batch.position_offset % float4_stride != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal world WindLight sky batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    auto command = pass ? mImpl->device->makeCommandBuffer(
        "Alchemy native world WindLight sky", error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight sky encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_sky_pipelines[scene.hdr ? 1 : 0]
                                      [scene.normal_hdr ? 1 : 0].get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error) &&
        encoder->setFragmentTexture(0, scene.rainbow_texture, error) &&
        encoder->setFragmentTexture(1, scene.halo_texture, error) &&
        encoder->setFragmentSampler(
            0, mImpl->diffuse_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        WorldWindLightSkyConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        constants.sky = scene.constants;
        constants.transfer[0] = static_cast<float>(scene.width);
        constants.transfer[1] = static_cast<float>(scene.height);
        constants.transfer[2] = scene.sky_hdr_scale;
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBytes(
                1, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle_strip,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    return encoded && encoder->end(error) && command->commit(error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightStars(
    ALMetalRenderTarget& target,
    const ALMetalWorldStarScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0 ||
        !std::isfinite(scene.star_alpha) || scene.star_alpha < 0.001f ||
        !std::isfinite(scene.time) ||
        !finiteValues(scene.moon_direction, 4) ||
        !finiteValues(scene.moon_quad_center, 4) ||
        !finiteValues(scene.moon_quad_u, 4) ||
        !finiteValues(scene.moon_quad_v, 4) ||
        !std::isfinite(scene.moon_brightness) ||
        scene.moon_brightness < 0.f ||
        (scene.moon_mask && !sampleableTexture(scene.moon_texture)))
    {
        setError(error, "Metal world WindLight star scene is incomplete");
        return false;
    }

    if (scene.moon_mask)
    {
        const ALMetalTextureDescriptor& moon =
            scene.moon_texture->descriptor();
        if (moon.type != ALMetalTextureType::texture_2d ||
            moon.width == 0 || moon.height == 0 || moon.sample_count != 1)
        {
            setError(error,
                     "Metal world WindLight star moon mask is incompatible");
            return false;
        }
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight star target is incompatible");
        return false;
    }

    constexpr std::size_t position_stride = sizeof(float) * 4;
    constexpr std::size_t color_stride = sizeof(std::uint8_t) * 4;
    constexpr std::size_t texcoord_stride = sizeof(float) * 2;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldStarBatch& batch = scene.batches[index];
        if (!batch.vertices || batch.vertex_count < 3 ||
            batch.vertex_count % 3 != 0 ||
            batch.position_offset % position_stride != 0 ||
            batch.color_offset % color_stride != 0 ||
            batch.texcoord_offset % alignof(float) != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       position_stride, batch.vertices->length()) ||
            !rangeFits(batch.color_offset, batch.vertex_count,
                       color_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       texcoord_stride, batch.vertices->length()))
        {
            setError(error, "Metal world WindLight star batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    auto command = pass ? mImpl->device->makeCommandBuffer(
        "Alchemy native world WindLight stars", error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight star encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_star_pipelines[scene.hdr ? 1 : 0]
                                       [scene.normal_hdr ? 1 : 0].get();
    const ALMetalTexture* moon = scene.moon_mask
        ? scene.moon_texture : mImpl->inactive_ssr_color.get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error) &&
        encoder->setFragmentTexture(0, moon, error) &&
        encoder->setFragmentSampler(0, mImpl->ramp_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldStarBatch& batch = scene.batches[index];
        WorldStarConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        constants.screen_alpha_time[0] = static_cast<float>(scene.width);
        constants.screen_alpha_time[1] = static_cast<float>(scene.height);
        constants.screen_alpha_time[2] = scene.star_alpha;
        constants.screen_alpha_time[3] = scene.time;
        std::memcpy(constants.moon_direction_brightness,
                    scene.moon_direction,
                    sizeof(scene.moon_direction));
        constants.moon_direction_brightness[3] = scene.moon_brightness;
        std::memcpy(constants.moon_quad_center, scene.moon_quad_center,
                    sizeof(scene.moon_quad_center));
        std::memcpy(constants.moon_quad_u, scene.moon_quad_u,
                    sizeof(scene.moon_quad_u));
        std::memcpy(constants.moon_quad_v, scene.moon_quad_v,
                    sizeof(scene.moon_quad_v));
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                1, *batch.vertices, batch.color_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                3, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0,
                batch.vertex_count, 1, error);
    }

    return encoded && encoder->end(error) && command->commit(error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightHeavenlyBodies(
    ALMetalRenderTarget& target,
    const ALMetalWorldHeavenlyBodyScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0)
    {
        setError(error,
                 "Metal world WindLight heavenly-body scene is incomplete");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error,
                 "Metal world WindLight heavenly-body target is incompatible");
        return false;
    }

    constexpr std::size_t position_stride = sizeof(float) * 4;
    constexpr std::size_t texcoord_stride = sizeof(float) * 2;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldHeavenlyBodyBatch& batch = scene.batches[index];
        const std::size_t index_stride = batch.index_type ==
            ALMetalIndexType::uint32 ? sizeof(std::uint32_t)
                                     : sizeof(std::uint16_t);
        if (!batch.vertices || !batch.indices || !batch.texture_a ||
            !batch.texture_b || batch.vertex_count < 3 ||
            batch.index_count < 3 ||
            batch.position_offset % position_stride != 0 ||
            batch.texcoord_offset % alignof(float) != 0 ||
            batch.index_offset % index_stride != 0 ||
            !sampleableTexture(batch.texture_a) ||
            !sampleableTexture(batch.texture_b) ||
            !finiteValues(batch.modelview_projection, 16) ||
            !finiteValues(batch.parameters, 4) ||
            batch.parameters[0] < 0.f ||
            (!batch.moon && batch.parameters[0] > 1.f) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       position_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       texcoord_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error,
                     "Metal world WindLight heavenly-body batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    auto command = pass ? mImpl->device->makeCommandBuffer(
        "Alchemy native world WindLight heavenly bodies", error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass,
        "Alchemy world WindLight heavenly-body encoder", error) : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setFragmentSampler(
            0, mImpl->ramp_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldHeavenlyBodyBatch& batch = scene.batches[index];
        WorldHeavenlyBodyConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.parameters, batch.parameters,
                    sizeof(constants.parameters));
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        constants.config[2] = batch.moon ? 1u : 0u;
        const ALMetalRenderPipeline* pipeline = batch.moon
            ? mImpl->windlight_moon_pipelines[scene.hdr ? 1 : 0]
                                                [scene.normal_hdr ? 1 : 0]
                  .get()
            : mImpl->windlight_sun_pipelines[scene.hdr ? 1 : 0]
                                               [scene.normal_hdr ? 1 : 0]
                  .get();
        encoded =
            encoder->setPipeline(*pipeline, error) &&
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                1, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                2, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->setFragmentTexture(
                0, batch.texture_a, error) &&
            encoder->setFragmentTexture(
                1, batch.texture_b, error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    return encoded && encoder->end(error) && command->commit(error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightMeteors(
    ALMetalRenderTarget& target,
    const ALMetalWorldMeteorScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0 ||
        !std::isfinite(scene.width_pixels) || scene.width_pixels <= 0.f)
    {
        setError(error, "Metal world WindLight meteor scene is incomplete");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight meteor target is incompatible");
        return false;
    }

    constexpr std::size_t vector_stride = sizeof(float) * 4;
    constexpr std::size_t color_stride = sizeof(std::uint8_t) * 4;
    constexpr std::size_t texcoord_stride = sizeof(float) * 2;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldMeteorBatch& batch = scene.batches[index];
        if (!batch.vertices || batch.vertex_count < 3 ||
            batch.vertex_count % 3 != 0 ||
            batch.position_offset % vector_stride != 0 ||
            batch.normal_offset % vector_stride != 0 ||
            batch.color_offset % color_stride != 0 ||
            batch.texcoord_offset % alignof(float) != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       vector_stride, batch.vertices->length()) ||
            !rangeFits(batch.normal_offset, batch.vertex_count,
                       vector_stride, batch.vertices->length()) ||
            !rangeFits(batch.color_offset, batch.vertex_count,
                       color_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       texcoord_stride, batch.vertices->length()))
        {
            setError(error, "Metal world WindLight meteor batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    auto command = pass ? mImpl->device->makeCommandBuffer(
        "Alchemy native world WindLight meteors", error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight meteor encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_meteor_pipelines[scene.hdr ? 1 : 0]
                                           [scene.normal_hdr ? 1 : 0]
            .get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldMeteorBatch& batch = scene.batches[index];
        WorldMeteorConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        constants.screen_width[0] = static_cast<float>(scene.width);
        constants.screen_width[1] = static_cast<float>(scene.height);
        constants.screen_width[2] = scene.width_pixels;
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                1, *batch.vertices, batch.normal_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.color_offset, error) &&
            encoder->setVertexBuffer(
                3, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                4, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0,
                batch.vertex_count, 1, error);
    }

    return encoded && encoder->end(error) && command->commit(error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightAurora(
    ALMetalRenderTarget& target,
    const ALMetalWorldAuroraScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0 ||
        !finiteValues(scene.camera_local, 4) ||
        !std::isfinite(scene.intensity) || scene.intensity <= 0.001f ||
        !std::isfinite(scene.time))
    {
        setError(error, "Metal world WindLight aurora scene is incomplete");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight aurora target is incompatible");
        return false;
    }

    constexpr std::size_t position_stride = sizeof(float) * 4;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        const std::size_t index_stride = batch.index_type ==
            ALMetalIndexType::uint32 ? sizeof(std::uint32_t)
                                     : sizeof(std::uint16_t);
        if (!batch.vertices || !batch.indices || batch.vertex_count < 3 ||
            batch.index_count < 3 ||
            batch.position_offset % position_stride != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       position_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal world WindLight aurora batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    auto command = pass ? mImpl->device->makeCommandBuffer(
        "Alchemy native world WindLight aurora", error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight aurora encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_aurora_pipelines[scene.hdr ? 1 : 0]
                                           [scene.normal_hdr ? 1 : 0]
            .get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        WorldAuroraConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.camera_intensity, scene.camera_local,
                    sizeof(scene.camera_local));
        constants.camera_intensity[3] = scene.intensity;
        constants.time_screen[0] = scene.time;
        constants.time_screen[1] = static_cast<float>(scene.width);
        constants.time_screen[2] = static_cast<float>(scene.height);
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBytes(
                1, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle_strip,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    return encoded && encoder->end(error) && command->commit(error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightClouds(
    ALMetalRenderTarget& target,
    const ALMetalWorldCloudScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 ||
        !sampleableTexture(scene.noise_texture) ||
        !sampleableTexture(scene.next_noise_texture) ||
        scene.width == 0 || scene.height == 0 ||
        !finiteValues(scene.constants.camera_local, 4) ||
        !finiteValues(scene.constants.light_density, 4) ||
        !finiteValues(scene.constants.blue_horizon, 4) ||
        !finiteValues(scene.constants.glow, 4) ||
        !finiteValues(scene.constants.haze_extinction, 4) ||
        !finiteValues(scene.constants.blue_weight, 4) ||
        !finiteValues(scene.constants.haze_weight, 4) ||
        !finiteValues(scene.constants.sky_sunlight, 4) ||
        !finiteValues(scene.constants.atmos_ambient, 4) ||
        !finiteValues(scene.constants.sky_light_atten, 4) ||
        !finiteValues(scene.constants.cloud_sunlight, 4) ||
        !finiteValues(scene.constants.cloud_color_shadow, 4) ||
        !finiteValues(scene.constants.cloud_pos_density1, 4) ||
        !finiteValues(scene.constants.cloud_pos_density2, 4) ||
        !finiteValues(scene.constants.parameters, 4) ||
        scene.constants.light_density[3] < 0.f ||
        scene.constants.glow[3] < 0.f ||
        scene.constants.parameters[0] < 0.f ||
        scene.constants.parameters[1] < 0.f ||
        scene.constants.parameters[2] < 0.f ||
        scene.constants.parameters[2] > 1.f)
    {
        setError(error, "Metal world WindLight cloud scene is incomplete");
        return false;
    }

    const ALMetalTextureDescriptor& noise =
        scene.noise_texture->descriptor();
    const ALMetalTextureDescriptor& next_noise =
        scene.next_noise_texture->descriptor();
    if (noise.type != ALMetalTextureType::texture_2d ||
        next_noise.type != ALMetalTextureType::texture_2d ||
        noise.width == 0 || noise.height == 0 ||
        next_noise.width == 0 || next_noise.height == 0 ||
        noise.sample_count != 1 || next_noise.sample_count != 1)
    {
        setError(error, "Metal world WindLight cloud noise is incompatible");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight cloud target is incompatible");
        return false;
    }

    constexpr std::size_t position_stride = sizeof(float) * 4;
    constexpr std::size_t texcoord_stride = sizeof(float) * 2;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldCloudBatch& batch = scene.batches[index];
        const std::size_t index_stride = batch.index_type ==
            ALMetalIndexType::uint32 ? sizeof(std::uint32_t)
                                     : sizeof(std::uint16_t);
        if (!batch.vertices || !batch.indices || batch.vertex_count < 3 ||
            batch.index_count < 3 ||
            batch.position_offset % position_stride != 0 ||
            batch.texcoord_offset % alignof(float) != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       position_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       texcoord_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal world WindLight cloud batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    auto command = pass ? mImpl->device->makeCommandBuffer(
        "Alchemy native world WindLight clouds", error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight cloud encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_cloud_pipelines[scene.hdr ? 1 : 0]
                                          [scene.normal_hdr ? 1 : 0]
            .get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error) &&
        encoder->setFragmentTexture(0, scene.noise_texture, error) &&
        encoder->setFragmentTexture(1, scene.next_noise_texture, error) &&
        encoder->setFragmentSampler(
            0, mImpl->diffuse_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldCloudBatch& batch = scene.batches[index];
        WorldCloudConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        constants.cloud = scene.constants;
        constants.screen[0] = static_cast<float>(scene.width);
        constants.screen[1] = static_cast<float>(scene.height);
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                1, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                2, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle_strip,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    return encoded && encoder->end(error) && command->commit(error);
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderBaseLighting(
    const ALMetalWorldBaseLightingScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        !sampleableTexture(scene.light_map) ||
        !validPBRReflectionProbes(scene.reflection_probes) ||
        !finiteValues(scene.constants.inverse_projection, 16) ||
        !finiteValues(scene.constants.light_direction, 4) ||
        !finiteValues(scene.constants.atmos_sunlit, 4) ||
        !finiteValues(scene.constants.atmos_sunlit_linear, 4) ||
        !finiteValues(scene.constants.atmos_amblit, 4) ||
        !finiteValues(&scene.constants.ssao_effect[0][0], 12) ||
        !finiteValues(scene.constants.lighting, 4) ||
        !finiteValues(scene.constants.surface, 4) ||
        !finiteValues(scene.constants.sky_transfer, 4) ||
        scene.constants.surface[0] <= 0.f ||
        scene.constants.sky_transfer[0] < 0.f ||
        scene.constants.sky[0] > 1 ||
        scene.constants.sky[1] > 1 ||
        scene.constants.sky[2] > 2 ||
        scene.constants.sky[3] != 0 ||
        (scene.constants.sky[0] != 0 &&
         scene.constants.sky[1] != (scene.hdr ? 1u : 0u)) ||
        (scene.constants.sky[0] == 0 && scene.constants.sky[2] != 0) ||
        scene.constants.config[0] > 1 ||
        scene.constants.config[1] > 1 ||
        scene.constants.config[2] > 1 ||
        scene.constants.config[3] > 1)
    {
        setError(error, "Metal world base-light scene is incomplete");
        return nullptr;
    }
    if (scene.hero_probe && !validHeroProbe(scene.hero_probe))
    {
        setError(error, "Metal world base-light hero probe is invalid");
        return nullptr;
    }
    if (scene.screen_space_reflections &&
        !validScreenSpaceReflections(scene.screen_space_reflections))
    {
        setError(
            error,
            "Metal world base-light screen-space reflections are invalid");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.gbuffer->descriptor();
    const ALMetalTexture* albedo = scene.gbuffer->colorTexture(0);
    const ALMetalTexture* specular = scene.gbuffer->colorTexture(1);
    const ALMetalTexture* normal = scene.gbuffer->colorTexture(2);
    const ALMetalTexture* emissive = scene.gbuffer->colorTexture(3);
    const ALMetalTexture* depth = scene.gbuffer->depthStencilTexture();
    const bool normal_format =
        input.colors[2].format == ALMetalPixelFormat::rgb10a2_unorm ||
        input.colors[2].format == ALMetalPixelFormat::rgba16_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    const ALMetalTextureDescriptor& light_map =
        scene.light_map->descriptor();
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 4 ||
        input.colors[0].format != ALMetalPixelFormat::rgba8_unorm_srgb ||
        input.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        !normal_format || input.colors[3].format != expected_emissive ||
        input.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !input.depth_stencil_shader_read ||
        light_map.format != ALMetalPixelFormat::rg8_unorm ||
        light_map.width != input.width || light_map.height != input.height ||
        !sampleableTexture(albedo) || !sampleableTexture(specular) ||
        !sampleableTexture(normal) || !sampleableTexture(emissive) ||
        !sampleableTexture(depth))
    {
        setError(error, "Metal world base-light G-buffer is incompatible");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world base-light accumulation";
    output_descriptor.label = "Alchemy native world base-light target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    ALMetalBufferDescriptor probe_buffer_descriptor;
    probe_buffer_descriptor.length =
        sizeof(ALMetalWorldReflectionProbeConstants);
    probe_buffer_descriptor.storage = ALMetalStorageMode::shared;
    probe_buffer_descriptor.cpu_cache =
        ALMetalCpuCacheMode::write_combined;
    probe_buffer_descriptor.label =
        "Alchemy world base-light reflection-probe constants";
    auto probe_constants_buffer = ALMetalBuffer::create(
        *mImpl->device, probe_buffer_descriptor, error);
    if (!probe_constants_buffer || !probe_constants_buffer->write(
            0, &scene.reflection_probes->constants,
            probe_buffer_descriptor.length, error))
    {
        return nullptr;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native world base lighting", error);
    if (!command)
    {
        return nullptr;
    }

    constexpr std::uint32_t brdf_lut_size = 512;
    const bool generate_brdf_lut = !mImpl->brdf_lut_ready;
    if (generate_brdf_lut)
    {
        auto compute = ALMetalComputeEncoder::begin(
            *command, "Alchemy world PBR BRDF LUT encoder", error);
        if (!compute ||
            !compute->setPipeline(*mImpl->brdf_lut_pipeline, error) ||
            !compute->setTexture(0, mImpl->brdf_lut.get(), error) ||
            !compute->dispatch(
                brdf_lut_size, brdf_lut_size, 1, 8, 8, 1, error) ||
            !compute->end(error))
        {
            return nullptr;
        }
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world base-light encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    const ALMetalWorldReflectionProbes& probes =
        *scene.reflection_probes;
    const ALMetalTexture* hero_texture = scene.hero_probe
        ? scene.hero_probe->radiance_texture
        : mImpl->inactive_hero_probe.get();
    const ALMetalWorldHeroProbeConstants& hero_constants =
        scene.hero_probe
            ? scene.hero_probe->constants
            : mImpl->inactive_hero_constants;
    const ALMetalTexture* ssr_color = scene.screen_space_reflections
        ? scene.screen_space_reflections->scene_color
        : mImpl->inactive_ssr_color.get();
    const ALMetalTexture* ssr_depth = scene.screen_space_reflections
        ? scene.screen_space_reflections->scene_depth
        : mImpl->inactive_ssr_depth.get();
    const ALMetalWorldScreenSpaceReflectionConstants& ssr_constants =
        scene.screen_space_reflections
            ? scene.screen_space_reflections->constants
            : mImpl->inactive_ssr_constants;
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->base_light_pipelines[scene.hdr ? 1 : 0], error) &&
        encoder->setFragmentBytes(
            0, &scene.constants, sizeof(scene.constants), error) &&
        encoder->setFragmentBuffer(
            1, *probe_constants_buffer, 0, error) &&
        encoder->setFragmentBytes(
            2, &hero_constants, sizeof(hero_constants), error) &&
        encoder->setFragmentBytes(
            3, &ssr_constants, sizeof(ssr_constants), error) &&
        encoder->setFragmentTexture(0, albedo, error) &&
        encoder->setFragmentTexture(1, specular, error) &&
        encoder->setFragmentTexture(2, normal, error) &&
        encoder->setFragmentTexture(3, emissive, error) &&
        encoder->setFragmentTexture(4, depth, error) &&
        encoder->setFragmentTexture(5, scene.light_map, error) &&
        encoder->setFragmentTexture(6, probes.radiance_texture, error) &&
        encoder->setFragmentTexture(7, probes.irradiance_texture, error) &&
        encoder->setFragmentTexture(8, mImpl->brdf_lut.get(), error) &&
        encoder->setFragmentTexture(9, hero_texture, error) &&
        encoder->setFragmentTexture(10, ssr_color, error) &&
        encoder->setFragmentTexture(11, ssr_depth, error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->setFragmentSampler(
            1, mImpl->probe_sampler.get(), error) &&
        encoder->setFragmentSampler(
            2, mImpl->linear_clamp_sampler.get(), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return nullptr;
    }
    if (generate_brdf_lut)
    {
        mImpl->brdf_lut_ready = true;
    }
    return output;
}

bool ALMetalWorldGBufferRenderer::compositeImpostorPostDeferred(
    ALMetalRenderTarget& target,
    const ALMetalWorldGBufferScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0)
    {
        setError(error,
                 "Metal world impostor post-deferred scene is incomplete");
        return false;
    }

    const ALMetalRenderTargetDescriptor& target_descriptor =
        target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (target_descriptor.width != scene.width ||
        target_descriptor.height != scene.height ||
        target_descriptor.sample_count != 1 ||
        target_descriptor.color_attachment_count != 4 ||
        target_descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        target_descriptor.colors[1].format !=
            ALMetalPixelFormat::rgba8_unorm ||
        target_descriptor.colors[2].format != expected_normal ||
        target_descriptor.colors[3].format != expected_emissive ||
        target_descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error,
                 "Metal world impostor post-deferred target is incompatible");
        return false;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    constexpr std::uint32_t max_skin_joints = 110;
    const bool has_shiny = std::any_of(
        scene.batches, scene.batches + scene.batch_count,
        [](const ALMetalWorldSimpleBatch& batch)
        {
            return batch.impostor_shiny;
        });
    const bool has_legacy_alpha = std::any_of(
        scene.batches, scene.batches + scene.batch_count,
        [](const ALMetalWorldSimpleBatch& batch)
        {
            return batch.material_slot_count != 0 &&
                   batch.material_alpha_mode == 1;
        });
    // The material fragment has one fixed resource contract. Bind a coherent
    // probe generation even for an env-zero slot; Metal validates declared
    // texture/buffer arguments independently of uniform control flow.
    const bool legacy_alpha_uses_probes = has_legacy_alpha;
    if ((has_shiny || legacy_alpha_uses_probes) &&
        !validReflectionProbes(scene.reflection_probes))
    {
        setError(error,
                 has_shiny
                     ? "Metal world shiny impostor reflection probes are invalid"
                     : "Metal world legacy alpha reflection probes are invalid");
        return false;
    }
    if (scene.hero_probe && !validHeroProbe(scene.hero_probe))
    {
        setError(error,
                 "Metal world impostor hero probe is invalid");
        return false;
    }
    if (scene.screen_space_reflections &&
        !validScreenSpaceReflections(scene.screen_space_reflections))
    {
        setError(error,
                 "Metal world impostor screen-space reflections are invalid");
        return false;
    }
    if (has_legacy_alpha &&
        !validLegacyAlphaLighting(scene.legacy_alpha_lighting))
    {
        setError(error,
                 "Metal world legacy alpha lighting is invalid");
        return false;
    }
    if (has_legacy_alpha && scene.directional_shadows &&
        !validDirectionalShadows(scene.directional_shadows))
    {
        setError(error,
                 "Metal world legacy alpha directional shadows are invalid");
        return false;
    }
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const bool pbr = isPBRBatch(batch);
        const bool fullbright = batch.impostor_fullbright;
        const bool shiny = batch.impostor_shiny;
        const bool emboss = batch.impostor_emboss;
        const bool material = batch.material_slot_count != 0;
        const bool material_normal_map = materialHasNormalMap(batch);
        const bool material_specular_map = materialHasSpecularMap(batch);
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices ||
            batch.vertex_count == 0 || batch.index_count == 0 ||
            batch.index_count % 3 != 0 ||
            batch.texture_count == 0 ||
            batch.texture_count >
                ALMetalWorldSimpleBatch::max_diffuse_textures ||
            (emboss && batch.texture_count != 1) ||
            (pbr &&
             (batch.texture_count >
                  ALMetalWorldSimpleBatch::max_material_slots ||
              batch.pbr_slot_count != batch.texture_count)) ||
            (material &&
             (batch.material_alpha_mode != 1 ||
              batch.material_slot_count >
                  ALMetalWorldSimpleBatch::max_material_slots ||
              batch.material_slot_count != batch.texture_count ||
              pbr || fullbright || shiny || emboss || batch.alpha_mask)) ||
            (!material && batch.material_alpha_mode != 0) ||
            batch.normal_texture ||
            (!fullbright && batch.alpha_mask) ||
            (fullbright && (pbr || material || shiny || emboss)) ||
            (shiny && (pbr || material || emboss)) ||
            (emboss && (pbr || material)) ||
            batch.avatar_rigid ||
            batch.avatar_skinning || batch.avatar_cloth ||
            batch.impostor || batch.terrain ||
            (!emboss && !batch.use_vertex_color) ||
            (!pbr && batch.double_sided) ||
            batch.position_offset % float4_stride != 0 ||
            (!emboss && batch.normal_offset % float4_stride != 0) ||
            batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
            (emboss &&
             batch.texcoord1_offset % (sizeof(float) * 2) != 0) ||
            (!emboss &&
             batch.color_offset % (sizeof(std::uint8_t) * 4) != 0) ||
            ((pbr || material_normal_map) &&
             batch.tangent_offset % float4_stride != 0) ||
            (material_normal_map &&
             batch.texcoord1_offset % (sizeof(float) * 2) != 0) ||
            (material_specular_map &&
             batch.texcoord2_offset % (sizeof(float) * 2) != 0) ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            ((shiny || material) && !finiteValues(batch.modelview, 16)) ||
            !finiteValues(&batch.normal_matrix[0][0], 12) ||
            !finiteValues(&batch.rigged_normal_matrix[0][0], 12) ||
            !finiteValues(batch.texture_matrix, 16) ||
            (fullbright &&
             (!std::isfinite(batch.alpha_cutoff) ||
              batch.alpha_cutoff < 0.f || batch.alpha_cutoff > 1.f)) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            (!emboss &&
             !rangeFits(batch.normal_offset, batch.vertex_count,
                        float4_stride, batch.vertices->length())) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       sizeof(float) * 2, batch.vertices->length()) ||
            (emboss &&
             !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                        sizeof(float) * 2,
                        batch.vertices->length())) ||
            (!emboss &&
             !rangeFits(batch.color_offset, batch.vertex_count,
                        sizeof(std::uint8_t) * 4,
                        batch.vertices->length())) ||
            (pbr && !rangeFits(batch.tangent_offset, batch.vertex_count,
                               float4_stride,
                               batch.vertices->length())) ||
            (material_normal_map &&
             (!rangeFits(batch.tangent_offset, batch.vertex_count,
                         float4_stride, batch.vertices->length()) ||
              !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                         sizeof(float) * 2, batch.vertices->length()))) ||
            (material_specular_map &&
             !rangeFits(batch.texcoord2_offset, batch.vertex_count,
                        sizeof(float) * 2, batch.vertices->length())) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error,
                     "Metal world impostor post-deferred batch is invalid");
            return false;
        }
        if (batch.skin_palette)
        {
            const std::size_t expected_palette_floats =
                4 + static_cast<std::size_t>(batch.skin_joint_count) * 12;
            if (batch.skin_joint_count == 0 ||
                batch.skin_joint_count > max_skin_joints ||
                batch.skin_palette_float_count != expected_palette_floats ||
                batch.weight_offset % float4_stride != 0 ||
                !rangeFits(batch.weight_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !finiteValues(batch.skin_palette,
                              expected_palette_floats))
            {
                setError(error,
                         "Metal world impostor post-deferred skinning data is invalid");
                return false;
            }
        }
        else if (batch.weight_offset != 0 ||
                 batch.skin_palette_float_count != 0 ||
                 batch.skin_joint_count != 0)
        {
            setError(error,
                     "Metal world impostor post-deferred skinning data is incomplete");
            return false;
        }
        for (std::uint32_t slot = 0; slot < batch.texture_count; ++slot)
        {
            if (!sampleableTexture(batch.diffuse_textures[slot]))
            {
                setError(error,
                         "Metal world impostor post-deferred texture is invalid");
                return false;
            }
            if (pbr &&
                (!finiteValues(
                     &batch.pbr_slots[slot].texture_transform[0][0], 32) ||
                 batch.pbr_slots[slot].alpha_cutoff != -1.f))
            {
                setError(error,
                         "Metal world PBR alpha material is invalid");
                return false;
            }
            if (material)
            {
                const ALMetalWorldMaterialSlot& material_slot =
                    batch.material_slots[slot];
                if (!finiteValues(material_slot.specular_color, 4) ||
                    !std::isfinite(material_slot.env_intensity) ||
                    !std::isfinite(material_slot.fullbright) ||
                    material_slot.env_intensity < 0.f ||
                    material_slot.env_intensity > 1.f ||
                    material_slot.fullbright < 0.f ||
                    material_slot.fullbright > 1.f ||
                    (material_normal_map &&
                     !sampleableTexture(material_slot.normal_texture)) ||
                    (material_specular_map &&
                     !sampleableTexture(material_slot.specular_texture)))
                {
                    setError(error,
                             "Metal world legacy alpha material is invalid");
                    return false;
                }
            }
        }
    }

    std::vector<std::unique_ptr<ALMetalBuffer>> palette_storage;
    std::vector<const ALMetalBuffer*> palette_buffers(
        scene.batch_count, nullptr);
    for (std::size_t batch_index = 0;
         batch_index < scene.batch_count; ++batch_index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[batch_index];
        if (!batch.skin_palette)
        {
            continue;
        }
        for (std::size_t previous = 0; previous < batch_index; ++previous)
        {
            const ALMetalWorldSimpleBatch& candidate = scene.batches[previous];
            if (candidate.skin_palette == batch.skin_palette &&
                candidate.skin_palette_float_count ==
                    batch.skin_palette_float_count &&
                candidate.skin_joint_count == batch.skin_joint_count)
            {
                palette_buffers[batch_index] = palette_buffers[previous];
                break;
            }
        }
        if (palette_buffers[batch_index])
        {
            continue;
        }
        ALMetalBufferDescriptor descriptor;
        descriptor.length = batch.skin_palette_float_count * sizeof(float);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy world impostor post-deferred skin palette";
        auto palette = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!palette || !palette->write(
                0, batch.skin_palette, descriptor.length, error))
        {
            return false;
        }
        palette_buffers[batch_index] = palette.get();
        palette_storage.push_back(std::move(palette));
    }

    std::unique_ptr<ALMetalBuffer> probe_constants_buffer;
    if (has_shiny || legacy_alpha_uses_probes)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = sizeof(ALMetalWorldReflectionProbeConstants);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy world reflection-probe constants";
        probe_constants_buffer = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!probe_constants_buffer || !probe_constants_buffer->write(
                0, &scene.reflection_probes->constants,
                descriptor.length, error))
        {
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    if (!pass)
    {
        return false;
    }
    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native avatar-impostor post-deferred", error);
    if (!command)
    {
        return false;
    }
    auto encoder = ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world impostor post-deferred encoder",
        error);
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error) &&
        encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error);
    if (encoded && (has_shiny || legacy_alpha_uses_probes))
    {
        const ALMetalTexture* hero_texture = scene.hero_probe
            ? scene.hero_probe->radiance_texture
            : mImpl->inactive_hero_probe.get();
        const ALMetalWorldHeroProbeConstants& hero_constants =
            scene.hero_probe
                ? scene.hero_probe->constants
                : mImpl->inactive_hero_constants;
        encoded =
            encoder->setFragmentSampler(
                1, mImpl->probe_sampler.get(), error) &&
            encoder->setFragmentTexture(
                32, scene.reflection_probes->radiance_texture, error) &&
            encoder->setFragmentBuffer(
                1, *probe_constants_buffer, 0, error) &&
            encoder->setFragmentTexture(
                34, hero_texture, error) &&
            encoder->setFragmentBytes(
                4, &hero_constants, sizeof(hero_constants), error);
    }
    if (encoded && legacy_alpha_uses_probes)
    {
        const ALMetalTexture* ssr_color = scene.screen_space_reflections
            ? scene.screen_space_reflections->scene_color
            : mImpl->inactive_ssr_color.get();
        const ALMetalTexture* ssr_depth = scene.screen_space_reflections
            ? scene.screen_space_reflections->scene_depth
            : mImpl->inactive_ssr_depth.get();
        const ALMetalWorldScreenSpaceReflectionConstants& ssr_constants =
            scene.screen_space_reflections
                ? scene.screen_space_reflections->constants
                : mImpl->inactive_ssr_constants;
        encoded =
            encoder->setFragmentSampler(
                2, mImpl->linear_clamp_sampler.get(), error) &&
            encoder->setFragmentSampler(
                3, mImpl->point_clamp_sampler.get(), error) &&
            encoder->setFragmentTexture(35, ssr_color, error) &&
            encoder->setFragmentTexture(36, ssr_depth, error) &&
            encoder->setFragmentBytes(
                5, &ssr_constants, sizeof(ssr_constants), error);
    }

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const bool pbr_batch = isPBRBatch(batch);
        const bool fullbright_batch = batch.impostor_fullbright;
        const bool shiny_batch = batch.impostor_shiny;
        const bool emboss_batch = batch.impostor_emboss;
        const bool material_batch = batch.material_slot_count != 0;
        const bool material_normal_map = materialHasNormalMap(batch);
        const bool material_specular_map = materialHasSpecularMap(batch);
        const bool rigged = batch.skin_palette != nullptr;
        const bool depth_writable =
            fullbright_batch || shiny_batch || (rigged && !emboss_batch);
        const ALMetalDepthStencilState* depth = depth_writable
            ? (scene.reverse_z ? mImpl->reverse_depth.get()
                               : mImpl->forward_depth.get())
            : (scene.reverse_z ? mImpl->reverse_readonly_depth.get()
                               : mImpl->forward_readonly_depth.get());
        const ALMetalRenderPipeline* pipeline = nullptr;
        if (emboss_batch)
        {
            pipeline = mImpl->emboss_pipelines[scene.hdr ? 1 : 0]
                                                [rigged ? 1 : 0]
                                                [scene.normal_hdr ? 1 : 0].get();
        }
        else if (shiny_batch)
        {
            pipeline = mImpl->shiny_pipelines[scene.hdr ? 1 : 0]
                                              [rigged ? 1 : 0]
                                              [scene.normal_hdr ? 1 : 0].get();
        }
        else if (fullbright_batch)
        {
            pipeline = mImpl->fullbright_pipelines[scene.hdr ? 1 : 0]
                                                   [batch.alpha_mask ? 1 : 0]
                                                   [rigged ? 1 : 0]
                                                   [scene.normal_hdr ? 1 : 0].get();
        }
        else if (pbr_batch)
        {
            pipeline = mImpl->pbr_alpha_pipelines[scene.hdr ? 1 : 0]
                                                  [rigged ? 1 : 0]
                                                  [scene.normal_hdr ? 1 : 0].get();
        }
        else if (material_batch)
        {
            pipeline = mImpl->legacy_alpha_pipelines[scene.hdr ? 1 : 0]
                                                   [rigged ? 1 : 0]
                                                   [scene.normal_hdr ? 1 : 0].get();
        }
        else
        {
            pipeline = mImpl->flat_alpha_pipelines[scene.hdr ? 1 : 0]
                                                   [rigged ? 1 : 0]
                                                   [scene.normal_hdr ? 1 : 0].get();
        }

        WorldSimpleConstants world{};
        std::memcpy(world.modelview_projection,
                    batch.modelview_projection,
                    sizeof(world.modelview_projection));
        std::memcpy(world.normal_matrix, batch.normal_matrix,
                    sizeof(world.normal_matrix));
        std::memcpy(world.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(world.rigged_normal_matrix));
        std::memcpy(world.texture_matrix, batch.texture_matrix,
                    sizeof(world.texture_matrix));
        world.material[0] = batch.alpha_cutoff;
        world.material[3] = batch.alpha_mask ? 1.f : 0.f;
        world.depth_convention[0] = scene.reverse_z ? 1u : 0u;
        world.depth_convention[1] = batch.texture_count;

        WorldShinyConstants shiny_world{};
        std::memcpy(shiny_world.modelview_projection,
                    batch.modelview_projection,
                    sizeof(shiny_world.modelview_projection));
        std::memcpy(shiny_world.modelview, batch.modelview,
                    sizeof(shiny_world.modelview));
        std::memcpy(shiny_world.normal_matrix, batch.normal_matrix,
                    sizeof(shiny_world.normal_matrix));
        std::memcpy(shiny_world.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(shiny_world.rigged_normal_matrix));
        std::memcpy(shiny_world.texture_matrix, batch.texture_matrix,
                    sizeof(shiny_world.texture_matrix));
        shiny_world.config[0] = scene.reverse_z ? 1u : 0u;
        shiny_world.config[1] = batch.texture_count;

        encoded =
            encoder->setDepthStencilState(depth, error) &&
            encoder->setCullMode(
                pbr_batch && batch.double_sided
                    ? ALMetalCullMode::none : ALMetalCullMode::back,
                error) &&
            encoder->setPipeline(*pipeline, error) &&
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.texcoord_offset, error);
        if (encoded)
        {
            encoded = (shiny_batch || material_batch)
                ? encoder->setVertexBytes(
                      4, &shiny_world, sizeof(shiny_world), error)
                : encoder->setVertexBytes(
                      4, &world, sizeof(world), error);
        }
        if (encoded && emboss_batch)
        {
            encoded = encoder->setVertexBuffer(
                9, *batch.vertices, batch.texcoord1_offset, error);
        }
        else if (encoded)
        {
            encoded =
                encoder->setVertexBuffer(
                    1, *batch.vertices, batch.normal_offset, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.color_offset, error);
        }
        if (encoded && rigged)
        {
            encoded =
                encoder->setVertexBuffer(
                    5, *palette_buffers[index], 0, error) &&
                encoder->setVertexBuffer(
                    6, *batch.vertices, batch.weight_offset, error) &&
                encoder->setVertexBytes(
                    7, &batch.skin_joint_count,
                    sizeof(batch.skin_joint_count), error);
        }
        if (encoded && pbr_batch)
        {
            WorldPBRConstants pbr{};
            for (std::size_t slot = 0;
                 slot < batch.pbr_slot_count; ++slot)
            {
                const ALMetalWorldPBRSlot& source = batch.pbr_slots[slot];
                std::memcpy(pbr.slots[slot].texture_transform,
                            source.texture_transform,
                            sizeof(pbr.slots[slot].texture_transform));
            }
            pbr.config[0] = batch.pbr_slot_count;
            encoded =
                encoder->setVertexBuffer(
                    8, *batch.vertices, batch.tangent_offset, error) &&
                encoder->setVertexBytes(11, &pbr, sizeof(pbr), error) &&
                encoder->setFragmentBytes(0, &pbr, sizeof(pbr), error);
            for (std::size_t slot = 0;
                 encoded && slot <
                     ALMetalWorldSimpleBatch::max_material_slots; ++slot)
            {
                const std::size_t source = std::min<std::size_t>(
                    slot, batch.pbr_slot_count - 1);
                encoded = encoder->setFragmentTexture(
                    static_cast<std::uint32_t>(slot),
                    batch.diffuse_textures[source], error);
            }
        }
        else if (encoded && material_batch)
        {
            const ALMetalTexture* directional_depth =
                scene.directional_shadows
                    ? scene.directional_shadows->depth_texture
                    : mImpl->unshadowed_depth.get();
            const ALMetalWorldDirectionalShadowConstants&
                directional_constants = scene.directional_shadows
                    ? scene.directional_shadows->constants
                    : mImpl->unshadowed_constants;
            WorldMaterialConstants material{};
            for (std::size_t slot = 0;
                 slot < batch.material_slot_count; ++slot)
            {
                const ALMetalWorldMaterialSlot& source =
                    batch.material_slots[slot];
                std::memcpy(material.slots[slot].specular_color,
                            source.specular_color,
                            sizeof(material.slots[slot].specular_color));
                material.slots[slot].properties[0] = source.env_intensity;
                material.slots[slot].properties[1] = source.alpha_cutoff;
                material.slots[slot].properties[2] = source.fullbright;
            }
            material.config[0] = batch.material_slot_count;
            material.config[1] = batch.material_alpha_mode;
            material.config[2] = material_normal_map ? 1u : 0u;
            material.config[3] = material_specular_map ? 1u : 0u;
            encoded =
                encoder->setVertexBuffer(
                    8, *batch.vertices,
                    material_normal_map
                        ? batch.tangent_offset : batch.normal_offset,
                    error) &&
                encoder->setVertexBuffer(
                    9, *batch.vertices,
                    material_normal_map
                        ? batch.texcoord1_offset : batch.texcoord_offset,
                    error) &&
                encoder->setVertexBuffer(
                    10, *batch.vertices,
                    material_specular_map
                        ? batch.texcoord2_offset : batch.texcoord_offset,
                    error) &&
                encoder->setFragmentBytes(
                    0, &material, sizeof(material), error) &&
                encoder->setFragmentBytes(
                    2, scene.legacy_alpha_lighting,
                    sizeof(*scene.legacy_alpha_lighting), error) &&
                encoder->setFragmentTexture(
                    33, directional_depth, error) &&
                encoder->setFragmentBytes(
                    3, &directional_constants,
                    sizeof(directional_constants), error);
            for (std::size_t slot = 0;
                 encoded && slot <
                     ALMetalWorldSimpleBatch::max_material_slots; ++slot)
            {
                const std::size_t source = std::min<std::size_t>(
                    slot, batch.material_slot_count - 1);
                const ALMetalTexture* normal_texture = material_normal_map
                    ? batch.material_slots[source].normal_texture
                    : batch.diffuse_textures[0];
                const ALMetalTexture* specular_texture = material_specular_map
                    ? batch.material_slots[source].specular_texture
                    : batch.diffuse_textures[0];
                encoded =
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(slot),
                        batch.diffuse_textures[source], error) &&
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(8 + slot),
                        normal_texture, error) &&
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(16 + slot),
                        specular_texture, error);
            }
        }
        else
        {
            for (std::size_t slot = 0;
                 encoded && slot < batch.texture_count; ++slot)
            {
                encoded = encoder->setFragmentTexture(
                    static_cast<std::uint32_t>(slot),
                    batch.diffuse_textures[slot], error);
            }
        }
        encoded = encoded && encoder->drawIndexed(
            ALMetalPrimitiveType::triangle, batch.index_count,
            batch.index_type, *batch.indices, batch.index_offset, 1, error);
    }

    if (!encoded || !encoder->end(error))
    {
        return false;
    }
    if (scene.stamp_opaque_coverage)
    {
        auto coverage_pass = target.makeRenderPass(pass_config, error);
        if (!coverage_pass)
        {
            return false;
        }
        auto coverage_encoder = ALMetalRenderEncoder::begin(
            *command, *coverage_pass,
            "Alchemy world post-deferred coverage encoder", error);
        if (!coverage_encoder)
        {
            return false;
        }
        const std::uint32_t reverse_z = scene.reverse_z ? 1u : 0u;
        const ALMetalDepthStencilState* coverage_depth = scene.reverse_z
            ? mImpl->reverse_coverage_depth.get()
            : mImpl->forward_coverage_depth.get();
        const ALMetalRenderPipeline* coverage_pipeline =
            mImpl->coverage_pipelines[scene.hdr ? 1 : 0]
                                     [scene.normal_hdr ? 1 : 0].get();
        encoded =
            coverage_encoder->setDepthStencilState(coverage_depth, error) &&
            coverage_encoder->setViewport(viewport, error) &&
            coverage_encoder->setScissor(scissor, error) &&
            coverage_encoder->setCullMode(ALMetalCullMode::none, error) &&
            coverage_encoder->setPipeline(*coverage_pipeline, error) &&
            coverage_encoder->setVertexBytes(
                0, &reverse_z, sizeof(reverse_z), error) &&
            coverage_encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
            coverage_encoder->end(error);
        if (!encoded)
        {
            return false;
        }
    }
    return command->commit(error);
}
