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
        // x = roughness factor, y = metallic factor, z = alpha cutoff.
        float properties[4];
    };

    struct alignas(16) WorldPBRConstants
    {
        WorldPBRSlotConstants
            slots[ALMetalWorldSimpleBatch::max_material_slots]{};
        // x = populated slot count, y = alpha mask, z = HDR normal target.
        std::uint32_t config[4]{};
    };

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
    std::array<AvatarVariantPipelines, 2> avatar_pipelines;
    std::array<NormalPipelines, 2> impostor_pipelines;
    std::array<NormalPipelines, 2> coverage_pipelines;
    std::array<TerrainPartitionPipelines, 2> terrain_pipelines;
    std::array<TerrainPartitionPipelines, 2> pbr_terrain_pipelines;
    std::unique_ptr<ALMetalDepthStencilState> forward_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_depth;
    std::unique_ptr<ALMetalDepthStencilState> forward_readonly_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_readonly_depth;
    std::unique_ptr<ALMetalDepthStencilState> forward_coverage_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_coverage_depth;
    std::unique_ptr<ALMetalSamplerState> diffuse_sampler;
    std::unique_ptr<ALMetalSamplerState> probe_sampler;
    std::unique_ptr<ALMetalSamplerState> point_clamp_sampler;
    std::unique_ptr<ALMetalSamplerState> linear_clamp_sampler;
    std::unique_ptr<ALMetalSamplerState> ramp_sampler;
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
        !impl->library->hasFunction("almetal_world_flat_alpha_fragment") ||
        !impl->library->hasFunction("almetal_world_pbr_alpha_fragment") ||
        !impl->library->hasFunction("almetal_world_impostor_fragment") ||
        !impl->library->hasFunction("almetal_world_coverage_fragment") ||
        !impl->library->hasFunction("almetal_world_terrain_fragment") ||
        !impl->library->hasFunction("almetal_world_pbr_terrain_fragment"))
    {
        if (impl->library)
        {
            setError(error,
                     "Built-in Metal library is missing world G-buffer functions");
        }
        return nullptr;
    }

    constexpr std::array<ALMetalPixelFormat, 2> emissive_formats{{
        ALMetalPixelFormat::rgba8_unorm,
        ALMetalPixelFormat::rg11b10_float,
    }};
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
                        ALMetalPixelFormat::depth32_float;
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
                    ALMetalPixelFormat::depth32_float;
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
                    ALMetalPixelFormat::depth32_float;
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
                    ALMetalPixelFormat::depth32_float;
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
                ALMetalPixelFormat::depth32_float;
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
                ALMetalPixelFormat::depth32_float;
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
                descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
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
                    ALMetalPixelFormat::depth32_float;
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
                    ALMetalPixelFormat::depth32_float;
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
                    ALMetalPixelFormat::depth32_float;
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
                    ALMetalPixelFormat::depth32_float;
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
                            ALMetalPixelFormat::depth32_float;
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

    sampler.min_filter = ALMetalSamplerFilter::linear;
    sampler.mag_filter = ALMetalSamplerFilter::linear;
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
    target_descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
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
            ALMetalPixelFormat::depth32_float)
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
    if (has_shiny &&
        !validReflectionProbes(scene.reflection_probes))
    {
        setError(error,
                 "Metal world shiny impostor reflection probes are invalid");
        return false;
    }
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const bool pbr = isPBRBatch(batch);
        const bool fullbright = batch.impostor_fullbright;
        const bool shiny = batch.impostor_shiny;
        const bool emboss = batch.impostor_emboss;
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
            batch.material_slot_count != 0 || batch.normal_texture ||
            (!fullbright && batch.alpha_mask) ||
            (fullbright && (pbr || shiny || emboss)) ||
            (shiny && (pbr || emboss)) || (emboss && pbr) ||
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
            (pbr && batch.tangent_offset % float4_stride != 0) ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            (shiny && !finiteValues(batch.modelview, 16)) ||
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
    if (has_shiny)
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
    if (encoded && has_shiny)
    {
        encoded =
            encoder->setFragmentSampler(
                1, mImpl->probe_sampler.get(), error) &&
            encoder->setFragmentTexture(
                32, scene.reflection_probes->radiance_texture, error) &&
            encoder->setFragmentBuffer(
                1, *probe_constants_buffer, 0, error);
    }

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const bool pbr_batch = isPBRBatch(batch);
        const bool fullbright_batch = batch.impostor_fullbright;
        const bool shiny_batch = batch.impostor_shiny;
        const bool emboss_batch = batch.impostor_emboss;
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
            encoded = shiny_batch
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
