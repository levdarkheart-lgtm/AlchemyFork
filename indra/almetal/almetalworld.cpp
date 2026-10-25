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

    struct alignas(16) WorldMaterialSlotConstants
    {
        float specular_color[4];
        float properties[4];
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
        float normal_matrix[3][4];
        float surface[4]{};
        float detail[4]{};
        std::uint32_t config[4]{};
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
    std::array<AvatarVariantPipelines, 2> avatar_pipelines;
    std::array<TerrainPartitionPipelines, 2> terrain_pipelines;
    std::unique_ptr<ALMetalDepthStencilState> forward_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_depth;
    std::unique_ptr<ALMetalSamplerState> diffuse_sampler;
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
        !impl->library->hasFunction("almetal_world_terrain_vertex") ||
        !impl->library->hasFunction("almetal_world_simple_fragment") ||
        !impl->library->hasFunction("almetal_world_alpha_mask_fragment") ||
        !impl->library->hasFunction("almetal_world_bump_fragment") ||
        !impl->library->hasFunction("almetal_world_material_fragment") ||
        !impl->library->hasFunction("almetal_world_pbr_fragment") ||
        !impl->library->hasFunction("almetal_world_terrain_fragment"))
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
        if (batch.terrain)
        {
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
                batch.pbr_slot_count != 0 || batch.skin_palette ||
                batch.avatar_rigid || batch.avatar_skinning ||
                batch.avatar_cloth || !valid_factors ||
                batch.position_offset % float4_stride != 0 ||
                !rangeFits(batch.position_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !sampleableTexture(batch.terrain_height_texture) ||
                !sampleableTexture(batch.terrain_composition_texture) ||
                !sampleableTexture(batch.terrain_alpha_ramp_texture) ||
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
                !std::isfinite(batch.terrain_region_scale) ||
                batch.terrain_region_scale <= 0.f ||
                !std::isfinite(batch.terrain_detail_scale) ||
                batch.terrain_detail_scale <= 0.f ||
                !finiteValues(batch.terrain_detail_phase, 2) ||
                !std::isfinite(batch.terrain_triplanar_blend) ||
                batch.terrain_triplanar_blend <= 0.f ||
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
        if (batch.terrain)
        {
            WorldTerrainConstants constants{};
            std::memcpy(constants.modelview_projection,
                        batch.modelview_projection,
                        sizeof(constants.modelview_projection));
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
            const ALMetalRenderPipeline* pipeline =
                mImpl->terrain_pipelines[scene.hdr ? 1 : 0]
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
                    3, mImpl->ramp_sampler.get(), error) &&
                encoder->setFragmentTexture(
                    4, batch.terrain_height_texture, error) &&
                encoder->setFragmentTexture(
                    5, batch.terrain_alpha_ramp_texture, error) &&
                encoder->setFragmentTexture(
                    6, batch.terrain_parcel_overlay_texture
                        ? batch.terrain_parcel_overlay_texture
                        : batch.terrain_alpha_ramp_texture,
                    error);
            for (std::uint32_t texture = 0;
                 encoded && texture < batch.texture_count; ++texture)
            {
                encoded = encoder->setFragmentTexture(
                    texture, batch.diffuse_textures[texture], error);
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

    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return nullptr;
    }
    return target;
}
