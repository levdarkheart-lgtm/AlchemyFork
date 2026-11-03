/**
 * @file almetalsculptpreview.cpp
 * @brief Native Metal solid-lit upload preview render pass.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetalsculptpreview.h"

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

    struct alignas(16) LitPreviewConstants
    {
        float modelview_projection[16];
        float normal_matrix[3][4];
        float light_direction[3][4];
        float light_diffuse[3][4];
        float diffuse_color[4];
        float avatar_wind[4];
        float avatar_sin_wave[4];
        float avatar_gravity[4];
        float avatar_cloth_depth[4];
        float pbr_base_color_transform[8];
        float pbr_emissive_transform[8];
        float pbr_emissive_color[4];
        float texture_matrix[16];
    };

    static_assert(sizeof(LitPreviewConstants) == 432);
}

struct ALMetalLitPreviewRenderer::Impl
{
    struct PipelineSet
    {
        std::unique_ptr<ALMetalRenderPipeline> background;
        std::unique_ptr<ALMetalRenderPipeline> solid;
        std::unique_ptr<ALMetalRenderPipeline> textured;
        std::unique_ptr<ALMetalRenderPipeline> flat;
        std::unique_ptr<ALMetalRenderPipeline> edge_u16;
        std::unique_ptr<ALMetalRenderPipeline> edge_u32;
        std::unique_ptr<ALMetalRenderPipeline> solid_blended;
        std::unique_ptr<ALMetalRenderPipeline> textured_blended;
        std::unique_ptr<ALMetalRenderPipeline> flat_blended;
        std::unique_ptr<ALMetalRenderPipeline> flat_depth_only;
        std::unique_ptr<ALMetalRenderPipeline> edge_u16_blended;
        std::unique_ptr<ALMetalRenderPipeline> edge_u32_blended;
        std::unique_ptr<ALMetalRenderPipeline> skinned_solid;
        std::unique_ptr<ALMetalRenderPipeline> skinned_textured;
        std::unique_ptr<ALMetalRenderPipeline> skinned_edge_u16;
        std::unique_ptr<ALMetalRenderPipeline> skinned_edge_u32;
        std::unique_ptr<ALMetalRenderPipeline> skinned_solid_blended;
        std::unique_ptr<ALMetalRenderPipeline> skinned_textured_blended;
        std::unique_ptr<ALMetalRenderPipeline> skinned_edge_u16_blended;
        std::unique_ptr<ALMetalRenderPipeline> skinned_edge_u32_blended;
        std::unique_ptr<ALMetalRenderPipeline> avatar_solid;
        std::unique_ptr<ALMetalRenderPipeline> avatar_textured;
        std::unique_ptr<ALMetalRenderPipeline> avatar_unlit_textured;
        std::unique_ptr<ALMetalRenderPipeline> avatar_skinned_solid;
        std::unique_ptr<ALMetalRenderPipeline> avatar_skinned_textured;
        std::unique_ptr<ALMetalRenderPipeline> avatar_skinned_unlit_textured;
        std::unique_ptr<ALMetalRenderPipeline>
            avatar_skinned_unlit_cloth_textured;
        std::unique_ptr<ALMetalRenderPipeline> deferred_rigged_textured;
        std::unique_ptr<ALMetalRenderPipeline>
            legacy_material_rigged_textured;
        std::unique_ptr<ALMetalRenderPipeline> pbr_rigged_textured;
        std::unique_ptr<ALMetalRenderPipeline> pbr_blend_rigged_textured;
        std::unique_ptr<ALMetalRenderPipeline> pbr_glow_rigged_textured;
        std::unique_ptr<ALMetalRenderPipeline> fullbright_rigged_textured;
        std::unique_ptr<ALMetalRenderPipeline>
            fullbright_mask_rigged_textured;
    };

    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::array<PipelineSet, 2> pipelines;
    std::unique_ptr<ALMetalDepthStencilState> depth_state;
    std::unique_ptr<ALMetalDepthStencilState> occluded_depth_state;
    std::unique_ptr<ALMetalDepthStencilState> no_depth_state;
    std::unique_ptr<ALMetalSamplerState> texture_sampler;
    std::unique_ptr<ALMetalSamplerState> background_sampler;
    std::unique_ptr<ALMetalBuffer> unit_sphere_vertices;
    std::unique_ptr<ALMetalBuffer> unit_sphere_indices;
    std::uint32_t unit_sphere_vertex_count = 0;
    std::uint32_t unit_sphere_index_count = 0;
};

ALMetalLitPreviewRenderer::ALMetalLitPreviewRenderer(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalLitPreviewRenderer::~ALMetalLitPreviewRenderer() = default;

std::unique_ptr<ALMetalLitPreviewRenderer> ALMetalLitPreviewRenderer::create(
    const ALMetalDevice& device,
    std::string* error)
{
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->library = ALMetalShaderLibrary::loadBuiltin(device, error);
    if (!impl->library ||
        !impl->library->hasFunction("almetal_resample_vertex") ||
        !impl->library->hasFunction("almetal_resample_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_flat_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_textured_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_edge_u16_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_edge_u32_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_skinned_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_skinned_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_avatar_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_avatar_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_avatar_unlit_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_avatar_skinned_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_avatar_skinned_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_avatar_skinned_unlit_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_avatar_skinned_unlit_cloth_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_deferred_rigged_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_deferred_rigged_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_legacy_material_rigged_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_pbr_rigged_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_pbr_blend_rigged_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_pbr_glow_rigged_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_pbr_glow_rigged_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_fullbright_mask_rigged_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_fullbright_rigged_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_fullbright_mask_rigged_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_avatar_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_avatar_unlit_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_skinned_edge_u16_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_skinned_edge_u32_vertex"))
    {
        if (impl->library)
        {
            setError(error, "Built-in Metal library is missing lit preview functions");
        }
        return nullptr;
    }

    constexpr std::array<ALMetalPixelFormat, 2> color_formats{{
        ALMetalPixelFormat::rgba8_unorm,
        ALMetalPixelFormat::rgba8_unorm_srgb,
    }};
    for (std::size_t format_index = 0; format_index < color_formats.size();
         ++format_index)
    {
        Impl::PipelineSet& pipelines = impl->pipelines[format_index];
        const std::string suffix = format_index == 0 ? "" : " sRGB";
        ALMetalRenderPipelineDescriptor pipeline_descriptor;
        pipeline_descriptor.library = impl->library.get();
        pipeline_descriptor.colors[0].format = color_formats[format_index];
        pipeline_descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;

        const auto create_pipeline = [&](std::unique_ptr<ALMetalRenderPipeline>& destination,
                                         const char* vertex_function,
                                         const char* fragment_function,
                                         const std::string& label)
        {
            pipeline_descriptor.vertex_function = vertex_function;
            pipeline_descriptor.fragment_function = fragment_function;
            pipeline_descriptor.label = label;
            destination = ALMetalRenderPipeline::create(
                device, pipeline_descriptor, error);
            return destination != nullptr;
        };

        if (!create_pipeline(pipelines.background,
                             "almetal_resample_vertex",
                             "almetal_resample_fragment",
                             "Alchemy preview background" + suffix + " pipeline") ||
            !create_pipeline(pipelines.solid,
                             "almetal_lit_preview_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.textured,
                             "almetal_lit_preview_textured_vertex",
                             "almetal_lit_preview_textured_fragment",
                             "Alchemy textured lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.flat,
                             "almetal_lit_preview_flat_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy flat preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.edge_u16,
                             "almetal_lit_preview_edge_u16_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy U16 lit preview edge" + suffix + " pipeline") ||
            !create_pipeline(pipelines.edge_u32,
                             "almetal_lit_preview_edge_u32_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy U32 lit preview edge" + suffix + " pipeline") ||
            !create_pipeline(pipelines.skinned_solid,
                             "almetal_lit_preview_skinned_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy skinned lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.skinned_textured,
                             "almetal_lit_preview_skinned_textured_vertex",
                             "almetal_lit_preview_textured_fragment",
                             "Alchemy skinned textured lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.skinned_edge_u16,
                             "almetal_lit_preview_skinned_edge_u16_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy skinned U16 lit preview edge" + suffix + " pipeline") ||
            !create_pipeline(pipelines.skinned_edge_u32,
                             "almetal_lit_preview_skinned_edge_u32_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy skinned U32 lit preview edge" + suffix + " pipeline") ||
            !create_pipeline(pipelines.avatar_solid,
                             "almetal_lit_preview_avatar_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy rigid-avatar lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.avatar_textured,
                             "almetal_lit_preview_avatar_textured_vertex",
                             "almetal_lit_preview_avatar_fragment",
                             "Alchemy textured rigid-avatar lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.avatar_unlit_textured,
                             "almetal_lit_preview_avatar_unlit_textured_vertex",
                             "almetal_lit_preview_avatar_unlit_fragment",
                             "Alchemy textured rigid-avatar unlit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.avatar_skinned_solid,
                             "almetal_lit_preview_avatar_skinned_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy avatar-skinned lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.avatar_skinned_textured,
                             "almetal_lit_preview_avatar_skinned_textured_vertex",
                             "almetal_lit_preview_avatar_fragment",
                             "Alchemy textured avatar-skinned lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.avatar_skinned_unlit_textured,
                             "almetal_lit_preview_avatar_skinned_unlit_textured_vertex",
                             "almetal_lit_preview_avatar_unlit_fragment",
                             "Alchemy textured avatar-skinned unlit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.avatar_skinned_unlit_cloth_textured,
                             "almetal_lit_preview_avatar_skinned_unlit_cloth_textured_vertex",
                             "almetal_lit_preview_avatar_unlit_fragment",
                             "Alchemy textured avatar-skinned unlit cloth preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.deferred_rigged_textured,
                             "almetal_lit_preview_deferred_rigged_textured_vertex",
                             "almetal_lit_preview_deferred_rigged_fragment",
                             "Alchemy deferred rigged attachment preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.legacy_material_rigged_textured,
                             "almetal_lit_preview_deferred_rigged_textured_vertex",
                             "almetal_lit_preview_legacy_material_rigged_fragment",
                             "Alchemy legacy-material rigged attachment preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.pbr_rigged_textured,
                             "almetal_lit_preview_pbr_rigged_textured_vertex",
                             "almetal_lit_preview_deferred_rigged_fragment",
                             "Alchemy PBR rigged attachment preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.pbr_blend_rigged_textured,
                             "almetal_lit_preview_pbr_rigged_textured_vertex",
                             "almetal_lit_preview_pbr_blend_rigged_fragment",
                             "Alchemy blended PBR rigged attachment preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.pbr_glow_rigged_textured,
                             "almetal_lit_preview_pbr_glow_rigged_textured_vertex",
                             "almetal_lit_preview_pbr_glow_rigged_fragment",
                             "Alchemy glowing PBR rigged attachment preview" + suffix + " pipeline"))
        {
            return nullptr;
        }

        // renderGeomPostDeferred keeps alpha writes disabled for opaque and
        // alpha-masked fullbright pools. Preserve the background/deferred
        // coverage already present in the visual-hint target.
        pipeline_descriptor.colors[0].write_mask =
            static_cast<ALMetalColorWriteMask>(
                static_cast<std::uint8_t>(ALMetalColorWriteMask::red) |
                static_cast<std::uint8_t>(ALMetalColorWriteMask::green) |
                static_cast<std::uint8_t>(ALMetalColorWriteMask::blue));
        if (!create_pipeline(pipelines.fullbright_rigged_textured,
                             "almetal_lit_preview_fullbright_mask_rigged_textured_vertex",
                             "almetal_lit_preview_fullbright_rigged_fragment",
                             "Alchemy blended fullbright rigged attachment preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.fullbright_mask_rigged_textured,
                             "almetal_lit_preview_fullbright_mask_rigged_textured_vertex",
                             "almetal_lit_preview_fullbright_mask_rigged_fragment",
                             "Alchemy fullbright mask rigged attachment preview" + suffix + " pipeline"))
        {
            return nullptr;
        }

        pipeline_descriptor.colors[0].write_mask = ALMetalColorWriteMask::all;
        pipeline_descriptor.colors[0].blending_enabled = true;
        pipeline_descriptor.colors[0].source_rgb = ALMetalBlendFactor::source_alpha;
        pipeline_descriptor.colors[0].destination_rgb =
            ALMetalBlendFactor::one_minus_source_alpha;
        pipeline_descriptor.colors[0].source_alpha = ALMetalBlendFactor::source_alpha;
        pipeline_descriptor.colors[0].destination_alpha =
            ALMetalBlendFactor::one_minus_source_alpha;

        if (!create_pipeline(pipelines.solid_blended,
                             "almetal_lit_preview_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy blended lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.textured_blended,
                             "almetal_lit_preview_textured_vertex",
                             "almetal_lit_preview_textured_fragment",
                             "Alchemy blended textured lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.flat_blended,
                             "almetal_lit_preview_flat_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy blended flat preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.edge_u16_blended,
                             "almetal_lit_preview_edge_u16_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy blended U16 lit preview edge" + suffix + " pipeline") ||
            !create_pipeline(pipelines.edge_u32_blended,
                             "almetal_lit_preview_edge_u32_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy blended U32 lit preview edge" + suffix + " pipeline") ||
            !create_pipeline(pipelines.skinned_solid_blended,
                             "almetal_lit_preview_skinned_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy blended skinned lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.skinned_textured_blended,
                             "almetal_lit_preview_skinned_textured_vertex",
                             "almetal_lit_preview_textured_fragment",
                             "Alchemy blended skinned textured lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.skinned_edge_u16_blended,
                             "almetal_lit_preview_skinned_edge_u16_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy blended skinned U16 lit preview edge" + suffix + " pipeline") ||
            !create_pipeline(pipelines.skinned_edge_u32_blended,
                             "almetal_lit_preview_skinned_edge_u32_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy blended skinned U32 lit preview edge" + suffix + " pipeline"))
        {
            return nullptr;
        }

        pipeline_descriptor.colors[0].blending_enabled = false;
        pipeline_descriptor.colors[0].write_mask = ALMetalColorWriteMask::none;
        if (!create_pipeline(pipelines.flat_depth_only,
                             "almetal_lit_preview_flat_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy flat preview depth-only" + suffix + " pipeline"))
        {
            return nullptr;
        }
    }

    ALMetalDepthStencilDescriptor depth_descriptor;
    depth_descriptor.depth_compare = ALMetalCompareFunction::less_equal;
    depth_descriptor.depth_write_enabled = true;
    depth_descriptor.label = "Alchemy lit preview depth";
    impl->depth_state = ALMetalDepthStencilState::create(device, depth_descriptor, error);
    if (!impl->depth_state)
    {
        return nullptr;
    }

    depth_descriptor.depth_compare = ALMetalCompareFunction::greater;
    depth_descriptor.depth_write_enabled = false;
    depth_descriptor.label = "Alchemy lit preview occluded depth";
    impl->occluded_depth_state = ALMetalDepthStencilState::create(
        device, depth_descriptor, error);
    if (!impl->occluded_depth_state)
    {
        return nullptr;
    }

    depth_descriptor.depth_compare = ALMetalCompareFunction::always;
    depth_descriptor.depth_write_enabled = false;
    depth_descriptor.label = "Alchemy preview background no depth";
    impl->no_depth_state = ALMetalDepthStencilState::create(
        device, depth_descriptor, error);
    if (!impl->no_depth_state)
    {
        return nullptr;
    }

    ALMetalSamplerDescriptor sampler_descriptor;
    sampler_descriptor.max_anisotropy = 16;
    sampler_descriptor.label = "Alchemy lit preview anisotropic wrap sampler";
    impl->texture_sampler = ALMetalSamplerState::create(
        device, sampler_descriptor, error);
    if (!impl->texture_sampler)
    {
        return nullptr;
    }

    sampler_descriptor.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler_descriptor.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler_descriptor.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler_descriptor.label = "Alchemy preview background anisotropic clamp sampler";
    impl->background_sampler = ALMetalSamplerState::create(
        device, sampler_descriptor, error);
    if (!impl->background_sampler)
    {
        return nullptr;
    }

    // Match LLRenderSphere exactly, but keep the immutable diagnostic mesh in
    // the pass that consumes it. This avoids rebuilding or flattening 3,600
    // vertices for every animated upload-preview frame.
    constexpr std::uint32_t latitude_slices = 20;
    constexpr std::uint32_t longitude_slices = 30;
    constexpr float two_pi = 6.28318530717958647692f;
    std::array<std::array<float, 3>,
               (latitude_slices + 1) * (longitude_slices + 1)>
        sphere_positions{};
    std::array<std::uint16_t,
               latitude_slices * longitude_slices * 6>
        sphere_indices{};
    for (std::uint32_t latitude = 0; latitude <= latitude_slices; ++latitude)
    {
        const float lat = static_cast<float>(latitude) /
                          static_cast<float>(latitude_slices);
        for (std::uint32_t longitude = 0;
             longitude <= longitude_slices; ++longitude)
        {
            const float lon = static_cast<float>(longitude) /
                              static_cast<float>(longitude_slices);
            sphere_positions[latitude * (longitude_slices + 1) + longitude] = {
                std::sin(two_pi * lat) * std::cos(two_pi * lon),
                std::sin(two_pi * lat) * std::sin(two_pi * lon),
                std::cos(two_pi * lat),
            };
        }
    }
    for (std::uint32_t latitude = 0; latitude < latitude_slices; ++latitude)
    {
        for (std::uint32_t longitude = 0;
             longitude < longitude_slices; ++longitude)
        {
            const std::size_t destination =
                (latitude * longitude_slices + longitude) * 6;
            const std::uint16_t current = static_cast<std::uint16_t>(
                latitude * (longitude_slices + 1) + longitude);
            const std::uint16_t next = static_cast<std::uint16_t>(
                (latitude + 1) * (longitude_slices + 1) + longitude);
            sphere_indices[destination + 0] = current;
            sphere_indices[destination + 1] = current + 1;
            sphere_indices[destination + 2] = next;
            sphere_indices[destination + 3] = next;
            sphere_indices[destination + 4] = current + 1;
            sphere_indices[destination + 5] = next + 1;
        }
    }

    ALMetalBufferDescriptor sphere_vertex_descriptor;
    sphere_vertex_descriptor.length = sizeof(sphere_positions);
    sphere_vertex_descriptor.storage = ALMetalStorageMode::shared;
    sphere_vertex_descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
    sphere_vertex_descriptor.label = "Alchemy preview unit sphere vertices";
    impl->unit_sphere_vertices = ALMetalBuffer::create(
        device, sphere_vertex_descriptor, error);
    ALMetalBufferDescriptor sphere_index_descriptor;
    sphere_index_descriptor.length = sizeof(sphere_indices);
    sphere_index_descriptor.storage = ALMetalStorageMode::shared;
    sphere_index_descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
    sphere_index_descriptor.label = "Alchemy preview unit sphere indices";
    impl->unit_sphere_indices = ALMetalBuffer::create(
        device, sphere_index_descriptor, error);
    if (!impl->unit_sphere_vertices || !impl->unit_sphere_indices ||
        !impl->unit_sphere_vertices->write(
            0, sphere_positions.data(), sizeof(sphere_positions), error) ||
        !impl->unit_sphere_indices->write(
            0, sphere_indices.data(), sizeof(sphere_indices), error))
    {
        return nullptr;
    }
    impl->unit_sphere_vertex_count = sphere_positions.size();
    impl->unit_sphere_index_count = sphere_indices.size();

    return std::unique_ptr<ALMetalLitPreviewRenderer>(
        new ALMetalLitPreviewRenderer(std::move(impl)));
}

std::unique_ptr<ALMetalTexture> ALMetalLitPreviewRenderer::render(
    const ALMetalLitPreviewBatch& batch,
    std::string* error)
{
    ALMetalLitPreviewScene scene;
    scene.batches = &batch;
    scene.batch_count = 1;
    scene.destination_template = batch.destination_template;
    scene.destination_descriptor = batch.destination_descriptor;
    return render(scene, error);
}

std::unique_ptr<ALMetalTexture> ALMetalLitPreviewRenderer::render(
    const ALMetalLitPreviewScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 ||
        (!scene.destination_template && !scene.destination_descriptor))
    {
        setError(error, "Metal lit preview scene is incomplete");
        return nullptr;
    }

    const ALMetalTextureDescriptor& template_descriptor = scene.destination_template
        ? scene.destination_template->descriptor() : *scene.destination_descriptor;
    if (template_descriptor.type != ALMetalTextureType::texture_2d ||
        (template_descriptor.format != ALMetalPixelFormat::rgba8_unorm &&
         template_descriptor.format != ALMetalPixelFormat::rgba8_unorm_srgb) ||
        template_descriptor.sample_count != 1 ||
        template_descriptor.storage != ALMetalStorageMode::private_gpu ||
        !alMetalHasUsage(template_descriptor.usage,
                         ALMetalTextureUsage::shader_read))
    {
        setError(error, "Metal lit preview destination must be a private shader-readable RGBA8 or sRGB RGBA8 texture");
        return nullptr;
    }
    if (scene.linear_storage_view &&
        template_descriptor.format != ALMetalPixelFormat::rgba8_unorm_srgb)
    {
        setError(error,
                 "Metal lit preview linear storage requires an sRGB RGBA8 destination");
        return nullptr;
    }
    const ALMetalPixelFormat render_format = scene.linear_storage_view
        ? alMetalLinearFormat(template_descriptor.format)
        : template_descriptor.format;
    const Impl::PipelineSet& pipelines = mImpl->pipelines[
        render_format == ALMetalPixelFormat::rgba8_unorm_srgb ? 1 : 0];

    constexpr std::size_t vector_stride = sizeof(float) * 4;
    const auto range_fits = [](std::size_t offset, std::size_t count,
                               std::size_t stride, std::size_t length)
    {
        return count <= (std::numeric_limits<std::size_t>::max() - offset) / stride &&
               offset + count * stride <= length;
    };
    const auto finite_values = [](const float* values, std::size_t count)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            if (!std::isfinite(values[index]))
            {
                return false;
            }
        }
        return true;
    };
    constexpr std::uint32_t max_skin_joints = 110;
    std::vector<std::unique_ptr<ALMetalBuffer>> skin_palette_storage;
    std::vector<const ALMetalBuffer*> skin_palette_buffers(
        scene.batch_count, nullptr);
    for (std::size_t batch_index = 0; batch_index < scene.batch_count; ++batch_index)
    {
        const ALMetalLitPreviewBatch& batch = scene.batches[batch_index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (batch.geometry == ALMetalLitPreviewGeometry::unit_sphere)
        {
            if (batch.vertices || batch.vertex_count != 0 || batch.indices ||
                batch.index_count != 0 || batch.texture || batch.show_edges ||
                batch.skin_palette || batch.skin_palette_float_count != 0 ||
                batch.skin_joint_count != 0 || batch.avatar_lighting ||
                batch.avatar_unlit || batch.avatar_skinning ||
                batch.avatar_cloth || batch.deferred_rigged ||
                batch.fullbright_mask_rigged || batch.fullbright_rigged ||
                batch.legacy_material_rigged || batch.pbr_rigged ||
                batch.pbr_glow || batch.pbr_blend ||
                batch.pbr_double_sided ||
                batch.depth_prepass)
            {
                setError(error, "Metal unit-sphere preview batch has external geometry");
                return nullptr;
            }
            continue;
        }
        if (batch.geometry ==
                ALMetalLitPreviewGeometry::unindexed_flat_triangles ||
            batch.geometry ==
                ALMetalLitPreviewGeometry::unindexed_flat_lines)
        {
            const bool triangles = batch.geometry ==
                ALMetalLitPreviewGeometry::unindexed_flat_triangles;
            if (!batch.vertices || batch.vertex_count == 0 ||
                batch.indices || batch.index_count != 0 || batch.texture ||
                batch.show_edges || batch.skin_palette ||
                batch.skin_palette_float_count != 0 ||
                batch.skin_joint_count != 0 || batch.avatar_lighting ||
                batch.avatar_unlit || batch.avatar_skinning ||
                batch.avatar_cloth || batch.deferred_rigged ||
                batch.fullbright_mask_rigged || batch.fullbright_rigged ||
                batch.legacy_material_rigged || batch.pbr_rigged ||
                batch.pbr_glow || batch.pbr_blend ||
                batch.pbr_double_sided ||
                batch.vertex_count % (triangles ? 3 : 2) != 0)
            {
                setError(error, triangles
                    ? "Metal flat preview requires an unindexed triangle list"
                    : "Metal flat-line preview requires complete line segments");
                return nullptr;
            }
            if (batch.depth_prepass &&
                (!triangles || !batch.alpha_blending ||
                 batch.depth_mode !=
                    ALMetalLitPreviewDepthMode::less_equal_write))
            {
                setError(error, "Metal flat preview depth prepass requires blending");
                return nullptr;
            }
            if (!range_fits(batch.position_offset, batch.vertex_count,
                            sizeof(float) * 3, batch.vertices->length()))
            {
                setError(error, "Metal lit preview geometry range exceeds its buffers");
                return nullptr;
            }
            continue;
        }
        if (!batch.vertices || batch.vertex_count == 0 ||
            !batch.indices || batch.index_count == 0)
        {
            setError(error, "Metal lit preview batch is incomplete");
            return nullptr;
        }
        if (batch.depth_prepass)
        {
            setError(error, "Metal lit preview depth prepass requires flat geometry");
            return nullptr;
        }
        if (batch.pbr_double_sided && !batch.pbr_rigged)
        {
            setError(error,
                     "Metal PBR double-sided state requires a PBR batch");
            return nullptr;
        }
        if ((batch.pbr_glow && !batch.pbr_rigged) ||
            (batch.pbr_blend && !batch.pbr_rigged) ||
            (batch.pbr_emissive_texture && !batch.pbr_glow))
        {
            setError(error,
                     "Metal PBR glow state requires a glowing PBR batch");
            return nullptr;
        }
        if ((batch.avatar_lighting &&
             (batch.avatar_unlit || batch.skin_palette || batch.show_edges ||
              batch.alpha_blending)) ||
            (batch.avatar_unlit &&
             (!batch.texture || batch.avatar_lighting || batch.show_edges ||
              batch.alpha_blending ||
              ((batch.skin_palette != nullptr) != batch.avatar_skinning))))
        {
            setError(error, "Metal avatar preview mode is invalid");
            return nullptr;
        }
        if (batch.avatar_cloth &&
            (!batch.avatar_unlit || !batch.avatar_skinning ||
             !batch.skin_palette ||
             batch.cloth_offset % vector_stride != 0 ||
             !range_fits(batch.cloth_offset, batch.vertex_count,
                         vector_stride, batch.vertices->length())))
        {
            setError(error, "Metal avatar cloth preview data is invalid");
            return nullptr;
        }
        if (batch.deferred_rigged &&
            (!batch.texture || !batch.skin_palette || batch.avatar_skinning ||
             batch.avatar_lighting || batch.avatar_unlit ||
             batch.avatar_cloth || batch.fullbright_mask_rigged ||
             batch.fullbright_rigged || batch.legacy_material_rigged ||
             batch.pbr_rigged ||
             batch.show_edges || batch.alpha_blending ||
             render_format !=
                 ALMetalPixelFormat::rgba8_unorm ||
             !std::isfinite(batch.alpha_cutoff) || batch.alpha_cutoff < 0.f ||
             batch.alpha_cutoff > 1.f ||
             batch.color_offset % (sizeof(std::uint8_t) * 4) != 0 ||
             !range_fits(batch.color_offset, batch.vertex_count,
                         sizeof(std::uint8_t) * 4,
                         batch.vertices->length())))
        {
            setError(error, "Metal deferred rigged preview data is invalid");
            return nullptr;
        }
        if (batch.fullbright_mask_rigged &&
            (!batch.texture || !batch.skin_palette || batch.avatar_skinning ||
             batch.avatar_lighting || batch.avatar_unlit ||
             batch.avatar_cloth || batch.deferred_rigged ||
             batch.fullbright_rigged || batch.legacy_material_rigged ||
             batch.pbr_rigged ||
             batch.show_edges || batch.alpha_blending ||
             render_format !=
                 ALMetalPixelFormat::rgba8_unorm ||
             !std::isfinite(batch.alpha_cutoff) || batch.alpha_cutoff < 0.f ||
             batch.alpha_cutoff > 1.f ||
             batch.color_offset % (sizeof(std::uint8_t) * 4) != 0 ||
             !range_fits(batch.color_offset, batch.vertex_count,
                         sizeof(std::uint8_t) * 4,
                         batch.vertices->length())))
        {
            setError(error,
                     "Metal fullbright mask rigged preview data is invalid");
            return nullptr;
        }
        if (batch.fullbright_rigged &&
            (!batch.texture || !batch.skin_palette || batch.avatar_skinning ||
             batch.avatar_lighting || batch.avatar_unlit ||
             batch.avatar_cloth || batch.deferred_rigged ||
             batch.fullbright_mask_rigged || batch.legacy_material_rigged ||
             batch.pbr_rigged ||
             batch.show_edges ||
             batch.alpha_blending ||
             render_format !=
                 ALMetalPixelFormat::rgba8_unorm ||
             batch.color_offset % (sizeof(std::uint8_t) * 4) != 0 ||
             !range_fits(batch.color_offset, batch.vertex_count,
                         sizeof(std::uint8_t) * 4,
                         batch.vertices->length())))
        {
            setError(error,
                     "Metal fullbright rigged preview data is invalid");
            return nullptr;
        }
        if (batch.legacy_material_rigged &&
            (!batch.texture || !batch.skin_palette || batch.avatar_skinning ||
             batch.avatar_lighting || batch.avatar_unlit ||
             batch.avatar_cloth || batch.deferred_rigged ||
             batch.fullbright_mask_rigged || batch.fullbright_rigged ||
             batch.pbr_rigged ||
             batch.show_edges || batch.alpha_blending ||
             render_format !=
                 ALMetalPixelFormat::rgba8_unorm ||
             !std::isfinite(batch.alpha_cutoff) || batch.alpha_cutoff < 0.f ||
             batch.alpha_cutoff > 1.f ||
             batch.color_offset % (sizeof(std::uint8_t) * 4) != 0 ||
             !range_fits(batch.color_offset, batch.vertex_count,
                         sizeof(std::uint8_t) * 4,
                         batch.vertices->length())))
        {
            setError(error,
                     "Metal legacy-material rigged preview data is invalid");
            return nullptr;
        }
        if (batch.pbr_rigged &&
            (!batch.texture || !batch.skin_palette || batch.avatar_skinning ||
             batch.avatar_lighting || batch.avatar_unlit ||
             batch.avatar_cloth || batch.deferred_rigged ||
             batch.fullbright_mask_rigged || batch.fullbright_rigged ||
             batch.legacy_material_rigged || batch.show_edges ||
             batch.alpha_blending ||
             render_format !=
                 ALMetalPixelFormat::rgba8_unorm ||
             !std::isfinite(batch.alpha_cutoff) || batch.alpha_cutoff < 0.f ||
             batch.alpha_cutoff > 1.f ||
             !finite_values(batch.pbr_base_color_transform, 8) ||
             (batch.pbr_blend && batch.pbr_glow) ||
             (batch.pbr_glow &&
              (!batch.pbr_emissive_texture || batch.alpha_cutoff != 0.f ||
               !finite_values(batch.pbr_emissive_transform, 8) ||
               !finite_values(batch.pbr_emissive_color, 4) ||
               batch.emissive_offset % (sizeof(std::uint8_t) * 4) != 0 ||
               !range_fits(batch.emissive_offset, batch.vertex_count,
                           sizeof(std::uint8_t) * 4,
                           batch.vertices->length()))) ||
             !finite_values(batch.texture_matrix, 16) ||
             batch.color_offset % (sizeof(std::uint8_t) * 4) != 0 ||
             !range_fits(batch.color_offset, batch.vertex_count,
                         sizeof(std::uint8_t) * 4,
                         batch.vertices->length())))
        {
            setError(error, "Metal PBR rigged preview data is invalid");
            return nullptr;
        }
        if (!range_fits(batch.position_offset, batch.vertex_count, vector_stride,
                        batch.vertices->length()) ||
            (!batch.fullbright_mask_rigged && !batch.fullbright_rigged &&
             !range_fits(batch.normal_offset, batch.vertex_count,
                         vector_stride, batch.vertices->length())) ||
            (batch.texture &&
             !range_fits(batch.texcoord_offset, batch.vertex_count,
                         sizeof(float) * 2, batch.vertices->length())) ||
            !range_fits(batch.index_offset, batch.index_count, index_stride,
                        batch.indices->length()))
        {
            setError(error, "Metal lit preview geometry range exceeds its buffers");
            return nullptr;
        }
        if (batch.index_offset % index_stride != 0)
        {
            setError(error, "Metal lit preview index offset is not element aligned");
            return nullptr;
        }
        if (batch.show_edges &&
            (batch.index_count % 3 != 0 ||
             batch.index_count > std::numeric_limits<std::uint32_t>::max() / 2))
        {
            setError(error, "Metal lit preview edges require complete triangle indices");
            return nullptr;
        }
        if (batch.skin_palette)
        {
            constexpr std::uint32_t avatar_skin_joints = 15;
            constexpr std::size_t avatar_palette_floats = 45 * 4;
            const std::size_t expected_palette_floats = batch.avatar_skinning
                ? avatar_palette_floats
                : 4 + static_cast<std::size_t>(batch.skin_joint_count) * 12;
            const std::size_t weight_stride = batch.avatar_skinning
                ? sizeof(float) : vector_stride;
            if (batch.skin_joint_count == 0 ||
                batch.skin_joint_count > max_skin_joints ||
                (batch.avatar_skinning &&
                 batch.skin_joint_count != avatar_skin_joints) ||
                (batch.avatar_skinning &&
                 (batch.show_edges || batch.alpha_blending)) ||
                batch.skin_palette_float_count != expected_palette_floats ||
                !range_fits(batch.weight_offset, batch.vertex_count,
                            weight_stride, batch.vertices->length()))
            {
                setError(error, "Metal lit preview skinning data is invalid");
                return nullptr;
            }

            // A model normally contributes several material faces which all
            // share one palette. Upload it once per scene rather than once per
            // face; the command buffer retains the shared Metal resource.
            for (std::size_t previous = 0; previous < batch_index; ++previous)
            {
                const ALMetalLitPreviewBatch& candidate = scene.batches[previous];
                if (candidate.skin_palette == batch.skin_palette &&
                    candidate.skin_palette_float_count ==
                        batch.skin_palette_float_count &&
                    candidate.skin_joint_count == batch.skin_joint_count &&
                    candidate.avatar_skinning == batch.avatar_skinning)
                {
                    skin_palette_buffers[batch_index] =
                        skin_palette_buffers[previous];
                    break;
                }
            }
            if (!skin_palette_buffers[batch_index])
            {
                ALMetalBufferDescriptor palette_descriptor;
                palette_descriptor.length = expected_palette_floats * sizeof(float);
                palette_descriptor.storage = ALMetalStorageMode::shared;
                palette_descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
                palette_descriptor.label = "Alchemy lit preview skin palette";
                auto palette_buffer = ALMetalBuffer::create(
                    *mImpl->device, palette_descriptor, error);
                if (!palette_buffer || !palette_buffer->write(
                        0, batch.skin_palette, palette_descriptor.length, error))
                {
                    return nullptr;
                }
                skin_palette_buffers[batch_index] = palette_buffer.get();
                skin_palette_storage.push_back(std::move(palette_buffer));
            }
        }
        else if (batch.skin_palette_float_count != 0 ||
                 batch.skin_joint_count != 0 || batch.avatar_skinning ||
                 batch.avatar_cloth || batch.deferred_rigged ||
                 batch.fullbright_mask_rigged || batch.fullbright_rigged ||
                 batch.legacy_material_rigged || batch.pbr_rigged)
        {
            setError(error, "Metal lit preview skinning data is incomplete");
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor target_descriptor;
    target_descriptor.width = template_descriptor.width;
    target_descriptor.height = template_descriptor.height;
    target_descriptor.colors[0].format = template_descriptor.format;
    target_descriptor.colors[0].shader_read = true;
    target_descriptor.colors[0].label = "Alchemy lit preview color";
    target_descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
    target_descriptor.memoryless_depth_stencil = true;
    target_descriptor.label = "Alchemy lit preview target";
    auto target = ALMetalRenderTarget::create(*mImpl->device, target_descriptor, error);
    if (!target)
    {
        return nullptr;
    }

    ALMetalTextureDescriptor output_descriptor = template_descriptor;
    output_descriptor.usage = output_descriptor.usage |
                              ALMetalTextureUsage::render_target;
    output_descriptor.label = "Alchemy lit preview mirror";
    auto output = ALMetalTexture::create(*mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy lit preview render", error);
    if (!command)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0,
        static_cast<double>(template_descriptor.width),
        static_cast<double>(template_descriptor.height),
        0.0, 1.0,
    };
    const ALMetalScissorRect scissor{
        0, 0, template_descriptor.width, template_descriptor.height,
    };
    const ALMetalTexture* color = target->colorTexture(0);
    bool encoded = color != nullptr;
    std::size_t segment_begin = 0;
    while (encoded && segment_begin < scene.batch_count)
    {
        std::size_t segment_end = segment_begin + 1;
        while (segment_end < scene.batch_count &&
               !scene.batches[segment_end].clear_depth_before)
        {
            ++segment_end;
        }

        ALMetalRenderPassConfig pass_config;
        pass_config.colors[0].load = segment_begin == 0
            ? ALMetalLoadAction::clear : ALMetalLoadAction::load;
        pass_config.colors[0].store = ALMetalStoreAction::store;
        pass_config.colors[0].clear = {
            scene.clear_color[0], scene.clear_color[1],
            scene.clear_color[2], scene.clear_color[3],
        };
        pass_config.colors[0].linear_storage_view =
            scene.linear_storage_view;
        // Each segment begins at the initial scene or an explicit legacy
        // depth-clear boundary. The target intentionally uses memoryless
        // depth, so retaining color and clearing depth is tile-local.
        pass_config.depth_load = ALMetalLoadAction::clear;
        pass_config.depth_store = ALMetalStoreAction::dont_care;
        pass_config.clear_depth = 1.0;
        auto pass = target->makeRenderPass(pass_config, error);
        auto encoder = pass ? ALMetalRenderEncoder::begin(
            *command, *pass, "Alchemy lit preview encoder", error) : nullptr;
        encoded =
            encoder &&
            encoder->setDepthStencilState(mImpl->depth_state.get(), error) &&
            encoder->setViewport(viewport, error) &&
            encoder->setScissor(scissor, error) &&
            encoder->setCullMode(ALMetalCullMode::back, error) &&
            // Mirroring GL framebuffer row orientation flips clip Y and
            // therefore changes the projected front-face winding.
            encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error);

        if (encoded && segment_begin == 0 && scene.background_texture)
        {
            encoded =
                encoder->setDepthStencilState(mImpl->no_depth_state.get(), error) &&
                encoder->setCullMode(ALMetalCullMode::none, error) &&
                encoder->setPipeline(*pipelines.background, error) &&
                encoder->setFragmentTextureLinear(
                    0, scene.background_texture, error) &&
                encoder->setFragmentSampler(
                    0, mImpl->background_sampler.get(), error) &&
                encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
        }

        // Convex hulls alone use the legacy two-pass contract: establish the
        // nearest surface across every hull without touching color, then
        // blend their colors against that completed depth. Keep both phases
        // in one encoder so the memoryless depth attachment remains live.
        for (std::size_t batch_index = segment_begin;
             encoded && batch_index < segment_end; ++batch_index)
        {
            const ALMetalLitPreviewBatch& batch = scene.batches[batch_index];
            if (!batch.depth_prepass)
            {
                continue;
            }
            LitPreviewConstants constants{};
            std::memcpy(constants.modelview_projection,
                        batch.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.diffuse_color, batch.diffuse_color,
                        sizeof(constants.diffuse_color));
            encoded =
                encoder->setDepthStencilState(mImpl->depth_state.get(), error) &&
                encoder->setCullMode(ALMetalCullMode::back, error) &&
                encoder->setPipeline(*pipelines.flat_depth_only, error) &&
                encoder->setVertexBuffer(0, *batch.vertices,
                                         batch.position_offset, error) &&
                encoder->setVertexBytes(2, &constants, sizeof(constants), error) &&
                encoder->draw(ALMetalPrimitiveType::triangle, 0,
                              batch.vertex_count, 1, error);
        }

        for (std::size_t batch_index = segment_begin;
             encoded && batch_index < segment_end; ++batch_index)
        {
            const ALMetalLitPreviewBatch& batch = scene.batches[batch_index];
            LitPreviewConstants constants{};
            std::memcpy(constants.modelview_projection, batch.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.normal_matrix, batch.normal_matrix,
                        sizeof(constants.normal_matrix));
            std::memcpy(constants.light_direction, batch.light_direction,
                        sizeof(constants.light_direction));
            std::memcpy(constants.light_diffuse, batch.light_diffuse,
                        sizeof(constants.light_diffuse));
            std::memcpy(constants.diffuse_color, batch.diffuse_color,
                        sizeof(constants.diffuse_color));
            std::memcpy(constants.avatar_wind, batch.avatar_wind,
                        sizeof(constants.avatar_wind));
            std::memcpy(constants.avatar_sin_wave, batch.avatar_sin_wave,
                        sizeof(constants.avatar_sin_wave));
            std::memcpy(constants.avatar_gravity, batch.avatar_gravity,
                        sizeof(constants.avatar_gravity));
            std::memcpy(constants.avatar_cloth_depth,
                        batch.avatar_cloth_depth,
                        sizeof(constants.avatar_cloth_depth));
            std::memcpy(constants.pbr_base_color_transform,
                        batch.pbr_base_color_transform,
                        sizeof(constants.pbr_base_color_transform));
            std::memcpy(constants.pbr_emissive_transform,
                        batch.pbr_emissive_transform,
                        sizeof(constants.pbr_emissive_transform));
            std::memcpy(constants.pbr_emissive_color,
                        batch.pbr_emissive_color,
                        sizeof(constants.pbr_emissive_color));
            std::memcpy(constants.texture_matrix, batch.texture_matrix,
                        sizeof(constants.texture_matrix));

            const ALMetalDepthStencilState* batch_depth_state =
                batch.depth_mode ==
                    ALMetalLitPreviewDepthMode::greater_read_only
                ? mImpl->occluded_depth_state.get()
                : mImpl->depth_state.get();
            const bool flat_lines = batch.geometry ==
                ALMetalLitPreviewGeometry::unindexed_flat_lines;
            encoded =
                encoder->setDepthStencilState(batch_depth_state, error) &&
                encoder->setCullMode(flat_lines || batch.pbr_double_sided
                    ? ALMetalCullMode::none : ALMetalCullMode::back, error);

            if (encoded &&
                batch.geometry != ALMetalLitPreviewGeometry::indexed_lit)
            {
                const ALMetalRenderPipeline* flat_pipeline =
                    batch.alpha_blending
                        ? pipelines.flat_blended.get() : pipelines.flat.get();
                const bool unit_sphere = batch.geometry ==
                    ALMetalLitPreviewGeometry::unit_sphere;
                const ALMetalBuffer* flat_vertices = unit_sphere
                    ? mImpl->unit_sphere_vertices.get() : batch.vertices;
                encoded = encoder->setPipeline(*flat_pipeline, error) &&
                    encoder->setVertexBuffer(
                        0, *flat_vertices,
                        unit_sphere ? 0 : batch.position_offset, error) &&
                    encoder->setVertexBytes(2, &constants,
                                            sizeof(constants), error);
                if (encoded && unit_sphere)
                {
                    encoded = encoder->drawIndexed(
                        ALMetalPrimitiveType::triangle,
                        mImpl->unit_sphere_index_count,
                        ALMetalIndexType::uint16,
                        *mImpl->unit_sphere_indices, 0, 1, error);
                }
                else if (encoded)
                {
                    encoded = encoder->draw(
                        flat_lines ? ALMetalPrimitiveType::line
                                   : ALMetalPrimitiveType::triangle,
                        0, batch.vertex_count, 1, error);
                }
                continue;
            }

            const bool skinned = batch.skin_palette != nullptr;
            const bool avatar_skinned = skinned && batch.avatar_skinning;
            const bool avatar_lit = batch.avatar_lighting || avatar_skinned;
            const ALMetalRenderPipeline* fill_pipeline = nullptr;
            if (batch.texture)
            {
                if (batch.fullbright_rigged)
                {
                    fill_pipeline = pipelines.fullbright_rigged_textured.get();
                }
                else if (batch.pbr_rigged)
                {
                    fill_pipeline = batch.pbr_blend
                        ? pipelines.pbr_blend_rigged_textured.get()
                        : batch.pbr_glow
                            ? pipelines.pbr_glow_rigged_textured.get()
                            : pipelines.pbr_rigged_textured.get();
                }
                else if (batch.legacy_material_rigged)
                {
                    fill_pipeline =
                        pipelines.legacy_material_rigged_textured.get();
                }
                else if (batch.fullbright_mask_rigged)
                {
                    fill_pipeline =
                        pipelines.fullbright_mask_rigged_textured.get();
                }
                else if (batch.deferred_rigged)
                {
                    fill_pipeline = pipelines.deferred_rigged_textured.get();
                }
                else if (batch.avatar_cloth)
                {
                    fill_pipeline =
                        pipelines.avatar_skinned_unlit_cloth_textured.get();
                }
                else if (batch.avatar_unlit && avatar_skinned)
                {
                    fill_pipeline =
                        pipelines.avatar_skinned_unlit_textured.get();
                }
                else if (batch.avatar_unlit)
                {
                    fill_pipeline = pipelines.avatar_unlit_textured.get();
                }
                else if (avatar_skinned)
                {
                    fill_pipeline = pipelines.avatar_skinned_textured.get();
                }
                else if (avatar_lit)
                {
                    fill_pipeline = pipelines.avatar_textured.get();
                }
                else if (skinned)
                {
                    fill_pipeline = batch.alpha_blending
                        ? pipelines.skinned_textured_blended.get()
                        : pipelines.skinned_textured.get();
                }
                else
                {
                    fill_pipeline = batch.alpha_blending
                        ? pipelines.textured_blended.get()
                        : pipelines.textured.get();
                }
            }
            else
            {
                if (avatar_skinned)
                {
                    fill_pipeline = pipelines.avatar_skinned_solid.get();
                }
                else if (avatar_lit)
                {
                    fill_pipeline = pipelines.avatar_solid.get();
                }
                else if (skinned)
                {
                    fill_pipeline = batch.alpha_blending
                        ? pipelines.skinned_solid_blended.get()
                        : pipelines.skinned_solid.get();
                }
                else
                {
                    fill_pipeline = batch.alpha_blending
                        ? pipelines.solid_blended.get()
                        : pipelines.solid.get();
                }
            }
            encoded =
                encoder->setPipeline(*fill_pipeline, error) &&
                encoder->setVertexBuffer(0, *batch.vertices,
                                         batch.position_offset, error);
            if (encoded && !batch.fullbright_mask_rigged &&
                !batch.fullbright_rigged)
            {
                encoded = encoder->setVertexBuffer(
                    1, *batch.vertices, batch.normal_offset, error);
            }
            encoded = encoded &&
                encoder->setVertexBytes(2, &constants, sizeof(constants), error);
            if (encoded && skinned)
            {
                encoded =
                    encoder->setVertexBuffer(6, *batch.vertices,
                                             batch.weight_offset, error) &&
                    encoder->setVertexBuffer(
                        7, *skin_palette_buffers[batch_index], 0, error);
                if (encoded && !avatar_skinned)
                {
                    encoded = encoder->setVertexBytes(
                        8, &batch.skin_joint_count,
                        sizeof(batch.skin_joint_count), error);
                }
            }
            if (encoded && batch.avatar_cloth)
            {
                encoded = encoder->setVertexBuffer(
                    9, *batch.vertices, batch.cloth_offset, error);
            }
            if (encoded && (batch.deferred_rigged ||
                            batch.fullbright_mask_rigged ||
                            batch.fullbright_rigged ||
                            batch.legacy_material_rigged ||
                            batch.pbr_rigged))
            {
                encoded = encoder->setVertexBuffer(
                    4, *batch.vertices, batch.color_offset, error);
            }
            if (encoded && batch.pbr_glow)
            {
                encoded = encoder->setVertexBuffer(
                    5, *batch.vertices, batch.emissive_offset, error);
            }
            if (encoded &&
                (batch.deferred_rigged || batch.fullbright_mask_rigged ||
                 batch.legacy_material_rigged || batch.pbr_rigged))
            {
                encoded = encoder->setFragmentBytes(
                    1, &batch.alpha_cutoff, sizeof(batch.alpha_cutoff), error);
            }
            if (encoded && batch.texture)
            {
                encoded =
                    encoder->setVertexBuffer(3, *batch.vertices,
                                             batch.texcoord_offset, error);
                if (encoded)
                {
                    // Deferred and fullbright rigged geometry follow their GL
                    // shaders' SRGBDecode sampler. Other legacy preview
                    // families keep encoded-space sampling through the linear
                    // view.
                    encoded = (batch.deferred_rigged ||
                               batch.fullbright_mask_rigged ||
                               batch.fullbright_rigged ||
                               batch.legacy_material_rigged ||
                               batch.pbr_rigged)
                        ? encoder->setFragmentTexture(
                              0, batch.texture, error)
                        : encoder->setFragmentTextureLinear(
                              0, batch.texture, error);
                }
                encoded = encoded &&
                    encoder->setFragmentSampler(0, mImpl->texture_sampler.get(),
                                                error);
            }
            if (encoded && batch.pbr_glow)
            {
                encoded =
                    encoder->setFragmentTexture(
                        1, batch.pbr_emissive_texture, error) &&
                    encoder->setFragmentSampler(
                        1, mImpl->texture_sampler.get(), error);
            }
            encoded = encoded &&
                encoder->drawIndexed(ALMetalPrimitiveType::triangle,
                                     batch.index_count, batch.index_type,
                                     *batch.indices, batch.index_offset, 1, error);
            if (encoded && batch.show_edges)
            {
                LitPreviewConstants edge_constants = constants;
                std::memcpy(edge_constants.diffuse_color, batch.edge_color,
                            sizeof(edge_constants.diffuse_color));
                const std::uint32_t index_element_offset =
                    static_cast<std::uint32_t>(batch.index_offset /
                        (batch.index_type == ALMetalIndexType::uint32 ? 4 : 2));
                const ALMetalRenderPipeline* edge_pipeline = nullptr;
                if (batch.index_type == ALMetalIndexType::uint32)
                {
                    if (skinned)
                    {
                        edge_pipeline = batch.alpha_blending
                            ? pipelines.skinned_edge_u32_blended.get()
                            : pipelines.skinned_edge_u32.get();
                    }
                    else
                    {
                        edge_pipeline = batch.alpha_blending
                            ? pipelines.edge_u32_blended.get()
                            : pipelines.edge_u32.get();
                    }
                }
                else
                {
                    if (skinned)
                    {
                        edge_pipeline = batch.alpha_blending
                            ? pipelines.skinned_edge_u16_blended.get()
                            : pipelines.skinned_edge_u16.get();
                    }
                    else
                    {
                        edge_pipeline = batch.alpha_blending
                            ? pipelines.edge_u16_blended.get()
                            : pipelines.edge_u16.get();
                    }
                }
                encoded =
                    encoder->setPipeline(*edge_pipeline, error) &&
                    encoder->setVertexBuffer(0, *batch.vertices,
                                             batch.position_offset, error) &&
                    encoder->setVertexBuffer(1, *batch.vertices,
                                             batch.normal_offset, error) &&
                    encoder->setVertexBytes(2, &edge_constants,
                                            sizeof(edge_constants), error) &&
                    encoder->setVertexBuffer(4, *batch.indices, 0, error) &&
                    encoder->setVertexBytes(5, &index_element_offset,
                                            sizeof(index_element_offset), error);
                if (encoded && skinned)
                {
                    encoded =
                        encoder->setVertexBuffer(6, *batch.vertices,
                                                 batch.weight_offset, error) &&
                        encoder->setVertexBuffer(
                            7, *skin_palette_buffers[batch_index], 0, error) &&
                        encoder->setVertexBytes(
                            8, &batch.skin_joint_count,
                            sizeof(batch.skin_joint_count), error);
                }
                encoded = encoded &&
                    encoder->setCullMode(ALMetalCullMode::none, error) &&
                    encoder->draw(ALMetalPrimitiveType::line, 0,
                                  batch.index_count * 2, 1, error) &&
                    encoder->setCullMode(ALMetalCullMode::back, error);
            }
        }
        encoded = encoded && encoder->end(error);
        segment_begin = segment_end;
    }
    if (!encoded || !output->copyFrom(*command, *color, {}, error) ||
        (output_descriptor.mipmap_level_count > 1 &&
         !output->generateMipmaps(*command, error)) ||
        !command->commit(error))
    {
        return nullptr;
    }
    return output;
}
