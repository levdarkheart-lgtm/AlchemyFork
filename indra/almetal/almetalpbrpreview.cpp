/**
 * @file almetalpbrpreview.cpp
 * @brief Native Metal glTF material swatch renderer.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetalpbrpreview.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    void setError(std::string* error, const std::string& message)
    {
        if (error)
        {
            *error = message;
        }
    }

    struct alignas(16) PBRPreviewConstants
    {
        float modelview_projection[16];
        float modelview[16];
        float normal_matrix[3][4];
        float texture_transform[4][8];
        float emissive_color[4];
        float sun_direction[4];
        float sun_color[4];
        float material[4];
    };

    static_assert(sizeof(PBRPreviewConstants) == 368);

    struct alignas(16) PBRPreviewExposureConstants
    {
        float compensation;
        float min_ev;
        float max_ev;
        float low_fraction;
        float high_fraction;
        std::uint32_t metering_mode;
        std::uint32_t enabled;
        std::uint32_t reserved;
    };

    static_assert(sizeof(PBRPreviewExposureConstants) == 32);

    struct alignas(16) PBRPreviewPostConstants
    {
        // static exposure, tonemap mix, tonemap type, target is sRGB
        float post[4];
        float tonemap_params[4];
        // bloom strength, halation strength, enabled, cross-filter strength
        float bloom[4];
        float halation_tint[4];
    };

    static_assert(sizeof(PBRPreviewPostConstants) == 64);

    struct alignas(16) PBRPreviewBloomExtractConstants
    {
        // threshold, knee, firefly clamp, alpha glow boost
        float params[4];
        float halation_weights[4];
    };

    static_assert(sizeof(PBRPreviewBloomExtractConstants) == 32);

    struct alignas(16) PBRPreviewBloomFilterConstants
    {
        // inverse source width/height, scatter or first-downsample flag
        float params[4];
    };

    static_assert(sizeof(PBRPreviewBloomFilterConstants) == 16);

    struct alignas(16) PBRPreviewCrossFilterConstants
    {
        // target texel x/y, authored length, chromatic dispersion
        float sampling[4];
        // arm direction x/y, pass scale, exponential falloff base
        float filtering[4];
    };

    static_assert(sizeof(PBRPreviewCrossFilterConstants) == 32);

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
}

struct ALMetalPBRPreviewRenderer::Impl
{
    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> shade_pipeline;
    std::unique_ptr<ALMetalComputePipeline> exposure_pipeline;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2> post_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        bloom_extract_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        bloom_downsample_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        bloom_upsample_pipelines;
    std::unique_ptr<ALMetalRenderPipeline> cross_filter_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> cross_filter_add_pipeline;
    std::unique_ptr<ALMetalSamplerState> color_sampler;
    std::unique_ptr<ALMetalSamplerState> data_sampler;
    std::unique_ptr<ALMetalSamplerState> meter_sampler;
    std::unique_ptr<ALMetalSamplerState> post_sampler;
    std::unique_ptr<ALMetalSamplerState> bloom_sampler;
    std::unique_ptr<ALMetalSamplerState> cross_filter_sampler;
};

ALMetalPBRPreviewRenderer::ALMetalPBRPreviewRenderer(
    std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalPBRPreviewRenderer::~ALMetalPBRPreviewRenderer() = default;

std::unique_ptr<ALMetalPBRPreviewRenderer>
ALMetalPBRPreviewRenderer::create(const ALMetalDevice& device,
                                  std::string* error)
{
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->library = ALMetalShaderLibrary::loadBuiltin(device, error);
    if (!impl->library ||
        !impl->library->hasFunction("almetal_pbr_preview_vertex") ||
        !impl->library->hasFunction("almetal_pbr_preview_fragment") ||
        !impl->library->hasFunction("almetal_pbr_preview_exposure") ||
        !impl->library->hasFunction("almetal_pbr_preview_post_fragment") ||
        !impl->library->hasFunction(
            "almetal_pbr_preview_bloom_extract_fragment") ||
        !impl->library->hasFunction(
            "almetal_pbr_preview_bloom_downsample_fragment") ||
        !impl->library->hasFunction(
            "almetal_pbr_preview_bloom_upsample_fragment") ||
        !impl->library->hasFunction(
            "almetal_pbr_preview_cross_filter_fragment"))
    {
        if (impl->library)
        {
            setError(error,
                     "Built-in Metal library is missing PBR preview functions");
        }
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor shade_descriptor;
    shade_descriptor.library = impl->library.get();
    shade_descriptor.vertex_function = "almetal_pbr_preview_vertex";
    shade_descriptor.fragment_function = "almetal_pbr_preview_fragment";
    shade_descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    shade_descriptor.colors[0].blending_enabled = true;
    shade_descriptor.colors[0].source_rgb = ALMetalBlendFactor::source_alpha;
    shade_descriptor.colors[0].destination_rgb =
        ALMetalBlendFactor::one_minus_source_alpha;
    shade_descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
    shade_descriptor.colors[0].destination_alpha =
        ALMetalBlendFactor::one_minus_source_alpha;
    shade_descriptor.label = "Alchemy PBR material preview HDR pipeline";
    impl->shade_pipeline = ALMetalRenderPipeline::create(
        device, shade_descriptor, error);
    if (!impl->shade_pipeline)
    {
        return nullptr;
    }

    ALMetalComputePipelineDescriptor exposure_descriptor;
    exposure_descriptor.library = impl->library.get();
    exposure_descriptor.function = "almetal_pbr_preview_exposure";
    exposure_descriptor.label = "Alchemy PBR material preview exposure pipeline";
    impl->exposure_pipeline = ALMetalComputePipeline::create(
        device, exposure_descriptor, error);
    if (!impl->exposure_pipeline)
    {
        return nullptr;
    }

    constexpr std::array<ALMetalPixelFormat, 2> post_formats{{
        ALMetalPixelFormat::rgba8_unorm,
        ALMetalPixelFormat::rgba8_unorm_srgb,
    }};
    for (std::size_t index = 0; index < post_formats.size(); ++index)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function = "almetal_pbr_preview_post_fragment";
        descriptor.colors[0].format = post_formats[index];
        descriptor.label = index == 0
            ? "Alchemy PBR material preview post pipeline"
            : "Alchemy PBR material preview sRGB post pipeline";
        impl->post_pipelines[index] = ALMetalRenderPipeline::create(
            device, descriptor, error);
        if (!impl->post_pipelines[index])
        {
            return nullptr;
        }
    }

    constexpr std::array<ALMetalPixelFormat, 2> bloom_formats{{
        ALMetalPixelFormat::rg11b10_float,
        ALMetalPixelFormat::rgba16_float,
    }};
    for (std::size_t index = 0; index < bloom_formats.size(); ++index)
    {
        const auto make_bloom_pipeline = [&](const char* fragment,
                                             const char* label,
                                             bool additive)
        {
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = impl->library.get();
            descriptor.vertex_function = "almetal_resample_vertex";
            descriptor.fragment_function = fragment;
            descriptor.colors[0].format = bloom_formats[index];
            descriptor.colors[0].blending_enabled = additive;
            if (additive)
            {
                descriptor.colors[0].source_rgb = ALMetalBlendFactor::one;
                descriptor.colors[0].destination_rgb = ALMetalBlendFactor::one;
                descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
                descriptor.colors[0].destination_alpha = ALMetalBlendFactor::one;
            }
            descriptor.label = label;
            return ALMetalRenderPipeline::create(device, descriptor, error);
        };
        impl->bloom_extract_pipelines[index] = make_bloom_pipeline(
            "almetal_pbr_preview_bloom_extract_fragment",
            index == 0 ? "Alchemy PBR preview bloom extract pipeline"
                       : "Alchemy PBR preview halation extract pipeline",
            false);
        impl->bloom_downsample_pipelines[index] = make_bloom_pipeline(
            "almetal_pbr_preview_bloom_downsample_fragment",
            index == 0 ? "Alchemy PBR preview bloom downsample pipeline"
                       : "Alchemy PBR preview halation downsample pipeline",
            false);
        impl->bloom_upsample_pipelines[index] = make_bloom_pipeline(
            "almetal_pbr_preview_bloom_upsample_fragment",
            index == 0 ? "Alchemy PBR preview bloom upsample pipeline"
                       : "Alchemy PBR preview halation upsample pipeline",
            true);
        if (!impl->bloom_extract_pipelines[index] ||
            !impl->bloom_downsample_pipelines[index] ||
            !impl->bloom_upsample_pipelines[index])
        {
            return nullptr;
        }
    }

    const auto make_cross_filter_pipeline = [&](bool additive,
                                                const char* label)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function =
            "almetal_pbr_preview_cross_filter_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
        descriptor.colors[0].blending_enabled = additive;
        if (additive)
        {
            descriptor.colors[0].source_rgb = ALMetalBlendFactor::one;
            descriptor.colors[0].destination_rgb = ALMetalBlendFactor::one;
            descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
            descriptor.colors[0].destination_alpha = ALMetalBlendFactor::one;
        }
        descriptor.label = label;
        return ALMetalRenderPipeline::create(device, descriptor, error);
    };
    impl->cross_filter_pipeline = make_cross_filter_pipeline(
        false, "Alchemy PBR preview cross-filter pipeline");
    impl->cross_filter_add_pipeline = make_cross_filter_pipeline(
        true, "Alchemy PBR preview additive cross-filter pipeline");
    if (!impl->cross_filter_pipeline || !impl->cross_filter_add_pipeline)
    {
        return nullptr;
    }

    ALMetalSamplerDescriptor sampler;
    sampler.max_anisotropy = 16;
    sampler.label = "Alchemy PBR preview color sampler";
    impl->color_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->color_sampler)
    {
        return nullptr;
    }
    sampler.label = "Alchemy PBR preview data sampler";
    impl->data_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->data_sampler)
    {
        return nullptr;
    }
    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.max_anisotropy = 1;
    sampler.label = "Alchemy PBR preview exposure meter sampler";
    impl->meter_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->meter_sampler)
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
    sampler.label = "Alchemy PBR preview post sampler";
    impl->post_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->post_sampler)
    {
        return nullptr;
    }
    sampler.min_filter = ALMetalSamplerFilter::linear;
    sampler.mag_filter = ALMetalSamplerFilter::linear;
    sampler.mip_filter = ALMetalSamplerMipFilter::none;
    sampler.address_s = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.address_t = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.address_r = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.label = "Alchemy PBR preview bloom sampler";
    impl->bloom_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->bloom_sampler)
    {
        return nullptr;
    }
    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.label = "Alchemy PBR preview cross-filter sampler";
    impl->cross_filter_sampler = ALMetalSamplerState::create(
        device, sampler, error);
    if (!impl->cross_filter_sampler)
    {
        return nullptr;
    }

    return std::unique_ptr<ALMetalPBRPreviewRenderer>(
        new ALMetalPBRPreviewRenderer(std::move(impl)));
}

std::unique_ptr<ALMetalTexture> ALMetalPBRPreviewRenderer::render(
    const ALMetalPBRPreviewScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || !scene.destination_descriptor)
    {
        setError(error, "Metal PBR preview scene is incomplete");
        return nullptr;
    }

    const ALMetalTextureDescriptor& output_template =
        *scene.destination_descriptor;
    if (output_template.type != ALMetalTextureType::texture_2d ||
        (output_template.format != ALMetalPixelFormat::rgba8_unorm &&
         output_template.format != ALMetalPixelFormat::rgba8_unorm_srgb) ||
        output_template.sample_count != 1 ||
        output_template.storage != ALMetalStorageMode::private_gpu ||
        !alMetalHasUsage(output_template.usage,
                         ALMetalTextureUsage::shader_read))
    {
        setError(error,
                 "Metal PBR preview destination must be a private shader-readable RGBA8 texture");
        return nullptr;
    }

    if (!std::isfinite(scene.exposure) || scene.exposure < 0.f ||
        !std::isfinite(scene.tonemap_mix) || scene.tonemap_mix < 0.f ||
        scene.tonemap_mix > 1.f || scene.tonemap_type > 6 ||
        !finiteValues(scene.tonemap_params, 4) ||
        !std::isfinite(scene.exposure_compensation) ||
        !std::isfinite(scene.exposure_min_ev) ||
        !std::isfinite(scene.exposure_max_ev) ||
        !std::isfinite(scene.exposure_low_fraction) ||
        !std::isfinite(scene.exposure_high_fraction) ||
        scene.exposure_metering_mode > 2 ||
        scene.exposure_low_fraction < 0.f ||
        scene.exposure_low_fraction > 0.99f ||
        scene.exposure_high_fraction <
            scene.exposure_low_fraction + 0.01f ||
        scene.exposure_high_fraction > 1.f ||
        scene.bloom_mip_count < 3 || scene.bloom_mip_count > 7 ||
        scene.bloom_resolution_scale > 4 ||
        !std::isfinite(scene.bloom_threshold) ||
        !std::isfinite(scene.bloom_knee) ||
        !std::isfinite(scene.bloom_firefly_clamp) ||
        !std::isfinite(scene.bloom_scatter) ||
        !std::isfinite(scene.bloom_alpha_glow_boost) ||
        !std::isfinite(scene.bloom_strength) ||
        !std::isfinite(scene.bloom_halation_strength) ||
        !finiteValues(scene.bloom_halation_tint, 3) ||
        !finiteValues(scene.bloom_halation_weights, 3) ||
        !std::isfinite(scene.cross_filter_strength) ||
        !std::isfinite(scene.cross_filter_angle) ||
        !std::isfinite(scene.cross_filter_length) ||
        !std::isfinite(scene.cross_filter_falloff) ||
        !std::isfinite(scene.cross_filter_chromatic) ||
        scene.bloom_knee < 0.f || scene.bloom_firefly_clamp < 0.f ||
        scene.bloom_scatter < 0.f ||
        scene.bloom_alpha_glow_boost < 0.f ||
        scene.bloom_strength < 0.f ||
        scene.bloom_halation_strength < 0.f ||
        scene.cross_filter_strength < 0.f ||
        scene.cross_filter_strength > 32.f ||
        scene.cross_filter_points < 2 || scene.cross_filter_points > 12 ||
        scene.cross_filter_angle < 0.f ||
        scene.cross_filter_angle > 360.f ||
        scene.cross_filter_length < 0.25f ||
        scene.cross_filter_length > 2.f ||
        scene.cross_filter_falloff < 0.1f ||
        scene.cross_filter_falloff > 3.f ||
        scene.cross_filter_chromatic < 0.f ||
        scene.cross_filter_chromatic > 1.f ||
        ((scene.tonemap_type == 2 || scene.tonemap_type == 3 ||
          scene.tonemap_type == 4) && scene.tonemap_params[0] <= 0.f) ||
        (scene.tonemap_type == 6 &&
         (scene.tonemap_params[0] <= 0.f ||
          scene.tonemap_params[1] <= 0.f ||
          scene.tonemap_params[2] <= 0.f ||
          scene.tonemap_params[3] <= 0.f)))
    {
        setError(error, "Metal PBR preview post state is invalid");
        return nullptr;
    }

    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalPBRPreviewBatch& batch = scene.batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count == 0 || !batch.base_color_texture ||
            !batch.normal_texture || !batch.orm_texture ||
            !batch.emissive_texture || batch.index_offset % index_stride != 0 ||
            batch.position_offset % (sizeof(float) * 4) != 0 ||
            batch.normal_offset % (sizeof(float) * 4) != 0 ||
            batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
            batch.color_offset % (sizeof(std::uint8_t) * 4) != 0 ||
            batch.tangent_offset % (sizeof(float) * 4) != 0 ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       sizeof(float) * 4, batch.vertices->length()) ||
            !rangeFits(batch.normal_offset, batch.vertex_count,
                       sizeof(float) * 4, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       sizeof(float) * 2, batch.vertices->length()) ||
            !rangeFits(batch.color_offset, batch.vertex_count,
                       sizeof(std::uint8_t) * 4, batch.vertices->length()) ||
            !rangeFits(batch.tangent_offset, batch.vertex_count,
                       sizeof(float) * 4, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()) ||
            !finiteValues(batch.modelview_projection, 16) ||
            !finiteValues(batch.modelview, 16) ||
            !finiteValues(&batch.normal_matrix[0][0], 12) ||
            !finiteValues(&batch.texture_transform[0][0], 32) ||
            !finiteValues(batch.emissive_color, 4) ||
            !finiteValues(batch.sun_direction, 4) ||
            !finiteValues(batch.sun_color, 4) ||
            !std::isfinite(batch.metallic_factor) ||
            !std::isfinite(batch.roughness_factor) ||
            !std::isfinite(batch.alpha_cutoff) ||
            batch.metallic_factor < 0.f || batch.metallic_factor > 1.f ||
            batch.roughness_factor < 0.f || batch.roughness_factor > 1.f ||
            batch.alpha_cutoff < -1.f || batch.alpha_cutoff > 1.f)
        {
            setError(error, "Metal PBR preview batch is invalid");
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor hdr_descriptor;
    hdr_descriptor.width = output_template.width;
    hdr_descriptor.height = output_template.height;
    hdr_descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    hdr_descriptor.colors[0].shader_read = true;
    hdr_descriptor.colors[0].label = "Alchemy PBR preview linear HDR color";
    hdr_descriptor.label = "Alchemy PBR preview linear HDR target";
    auto hdr_target = ALMetalRenderTarget::create(
        *mImpl->device, hdr_descriptor, error);
    if (!hdr_target)
    {
        return nullptr;
    }

    ALMetalRenderTargetDescriptor post_descriptor;
    post_descriptor.width = output_template.width;
    post_descriptor.height = output_template.height;
    post_descriptor.colors[0].format = output_template.format;
    post_descriptor.colors[0].shader_read = true;
    post_descriptor.colors[0].label = "Alchemy PBR preview display color";
    post_descriptor.label = "Alchemy PBR preview display target";
    auto post_target = ALMetalRenderTarget::create(
        *mImpl->device, post_descriptor, error);
    if (!post_target)
    {
        return nullptr;
    }

    ALMetalTextureDescriptor exposure_descriptor;
    exposure_descriptor.format = ALMetalPixelFormat::r32_float;
    exposure_descriptor.width = 1;
    exposure_descriptor.height = 1;
    exposure_descriptor.storage = ALMetalStorageMode::private_gpu;
    exposure_descriptor.usage = ALMetalTextureUsage::shader_read |
                                ALMetalTextureUsage::shader_write;
    exposure_descriptor.label = "Alchemy PBR preview exposure";
    auto exposure = ALMetalTexture::create(
        *mImpl->device, exposure_descriptor, error);
    if (!exposure)
    {
        return nullptr;
    }

    const bool bloom_requested = scene.bloom_enabled &&
        (scene.bloom_strength > 0.f ||
         (scene.bloom_halation &&
          scene.bloom_halation_strength > 0.f));
    const std::size_t bloom_pipeline_index =
        scene.bloom_halation ? 1 : 0;
    const ALMetalPixelFormat bloom_format = scene.bloom_halation
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rg11b10_float;
    constexpr std::array<std::uint32_t, 5> bloom_scale_numerator{{
        1, 3, 1, 1, 1,
    }};
    constexpr std::array<std::uint32_t, 5> bloom_scale_denominator{{
        1, 4, 2, 4, 8,
    }};
    const std::uint32_t bloom_base_width = std::max(
        1u,
        output_template.width *
            bloom_scale_numerator[scene.bloom_resolution_scale] /
            bloom_scale_denominator[scene.bloom_resolution_scale]);
    const std::uint32_t bloom_base_height = std::max(
        1u,
        output_template.height *
            bloom_scale_numerator[scene.bloom_resolution_scale] /
            bloom_scale_denominator[scene.bloom_resolution_scale]);
    std::vector<std::unique_ptr<ALMetalRenderTarget>> bloom_targets;
    if (bloom_requested)
    {
        for (std::uint32_t mip = 0; mip < scene.bloom_mip_count; ++mip)
        {
            const std::uint32_t width = std::max(
                1u, bloom_base_width >> mip);
            const std::uint32_t height = std::max(
                1u, bloom_base_height >> mip);
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = width;
            descriptor.height = height;
            descriptor.colors[0].format = bloom_format;
            descriptor.colors[0].shader_read = true;
            descriptor.colors[0].label = "Alchemy PBR preview bloom color";
            descriptor.label = "Alchemy PBR preview bloom target";
            auto target = ALMetalRenderTarget::create(
                *mImpl->device, descriptor, error);
            if (!target)
            {
                return nullptr;
            }
            bloom_targets.push_back(std::move(target));
            if (width == 1 && height == 1)
            {
                break;
            }
        }
    }
    const bool bloom_active = bloom_targets.size() >= 3;
    if (!bloom_active)
    {
        bloom_targets.clear();
    }

    const bool cross_filter_active = bloom_active &&
        scene.cross_filter_enabled && scene.bloom_strength > 0.f &&
        scene.cross_filter_strength > 0.f;
    std::array<std::unique_ptr<ALMetalRenderTarget>, 3>
        cross_filter_targets;
    if (cross_filter_active)
    {
        const std::uint32_t width = std::max(1u, bloom_base_width / 2u);
        const std::uint32_t height = std::max(1u, bloom_base_height / 2u);
        for (std::size_t index = 0;
             index < cross_filter_targets.size(); ++index)
        {
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = width;
            descriptor.height = height;
            descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
            descriptor.colors[0].shader_read = true;
            descriptor.colors[0].label =
                "Alchemy PBR preview cross-filter color";
            descriptor.label = "Alchemy PBR preview cross-filter target";
            cross_filter_targets[index] = ALMetalRenderTarget::create(
                *mImpl->device, descriptor, error);
            if (!cross_filter_targets[index])
            {
                return nullptr;
            }
        }
    }

    ALMetalTextureDescriptor output_descriptor = output_template;
    output_descriptor.usage = output_descriptor.usage |
                              ALMetalTextureUsage::render_target;
    output_descriptor.label = "Alchemy PBR preview mirror";
    auto output = ALMetalTexture::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy PBR preview render", error);
    if (!command)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
    auto shade_pass = hdr_target->makeRenderPass(pass_config, error);
    auto shade_encoder = shade_pass ? ALMetalRenderEncoder::begin(
        *command, *shade_pass, "Alchemy PBR preview HDR encoder", error)
        : nullptr;
    const ALMetalViewport viewport{
        0.0, 0.0,
        static_cast<double>(output_template.width),
        static_cast<double>(output_template.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{
        0, 0, output_template.width, output_template.height,
    };
    const std::size_t pipeline_index =
        output_template.format == ALMetalPixelFormat::rgba8_unorm_srgb ? 1 : 0;
    bool encoded = shade_encoder &&
        shade_encoder->setPipeline(*mImpl->shade_pipeline, error) &&
        shade_encoder->setViewport(viewport, error) &&
        shade_encoder->setScissor(scissor, error) &&
        shade_encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error);

    for (std::size_t index = 0; encoded && index < scene.batch_count; ++index)
    {
        const ALMetalPBRPreviewBatch& batch = scene.batches[index];
        PBRPreviewConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.modelview, batch.modelview,
                    sizeof(constants.modelview));
        std::memcpy(constants.normal_matrix, batch.normal_matrix,
                    sizeof(constants.normal_matrix));
        std::memcpy(constants.texture_transform, batch.texture_transform,
                    sizeof(constants.texture_transform));
        std::memcpy(constants.emissive_color, batch.emissive_color,
                    sizeof(constants.emissive_color));
        std::memcpy(constants.sun_direction, batch.sun_direction,
                    sizeof(constants.sun_direction));
        std::memcpy(constants.sun_color, batch.sun_color,
                    sizeof(constants.sun_color));
        constants.material[0] = batch.metallic_factor;
        constants.material[1] = batch.roughness_factor;
        constants.material[2] = batch.alpha_cutoff;

        encoded =
            shade_encoder->setCullMode(batch.double_sided
                ? ALMetalCullMode::none : ALMetalCullMode::back, error) &&
            shade_encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            shade_encoder->setVertexBuffer(
                1, *batch.vertices, batch.normal_offset, error) &&
            shade_encoder->setVertexBuffer(
                2, *batch.vertices, batch.texcoord_offset, error) &&
            shade_encoder->setVertexBuffer(
                3, *batch.vertices, batch.color_offset, error) &&
            shade_encoder->setVertexBuffer(
                4, *batch.vertices, batch.tangent_offset, error) &&
            shade_encoder->setVertexBytes(
                5, &constants, sizeof(constants), error) &&
            shade_encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            shade_encoder->setFragmentTexture(
                0, batch.base_color_texture, error) &&
            shade_encoder->setFragmentTextureLinear(
                1, batch.normal_texture, error) &&
            shade_encoder->setFragmentTextureLinear(
                2, batch.orm_texture, error) &&
            shade_encoder->setFragmentTexture(
                3, batch.emissive_texture, error) &&
            shade_encoder->setFragmentSampler(
                0, mImpl->color_sampler.get(), error) &&
            shade_encoder->setFragmentSampler(
                1, mImpl->data_sampler.get(), error) &&
            shade_encoder->setFragmentSampler(
                2, mImpl->data_sampler.get(), error) &&
            shade_encoder->setFragmentSampler(
                3, mImpl->color_sampler.get(), error) &&
            shade_encoder->drawIndexed(
                ALMetalPrimitiveType::triangle, batch.index_count,
                batch.index_type, *batch.indices,
                batch.index_offset, 1, error);
    }

    const ALMetalTexture* hdr_color = hdr_target->colorTexture(0);
    if (!encoded || !shade_encoder->end(error) || !hdr_color)
    {
        return nullptr;
    }

    PBRPreviewExposureConstants exposure_constants{};
    exposure_constants.compensation = scene.exposure_compensation;
    exposure_constants.min_ev = std::min(
        scene.exposure_min_ev, scene.exposure_max_ev);
    exposure_constants.max_ev = std::max(
        scene.exposure_min_ev, scene.exposure_max_ev);
    exposure_constants.low_fraction = scene.exposure_low_fraction;
    exposure_constants.high_fraction = scene.exposure_high_fraction;
    exposure_constants.metering_mode = scene.exposure_metering_mode;
    exposure_constants.enabled = scene.meter_exposure ? 1u : 0u;
    auto exposure_encoder = ALMetalComputeEncoder::begin(
        *command, "Alchemy PBR preview exposure encoder", error);
    encoded = exposure_encoder &&
        exposure_encoder->setPipeline(*mImpl->exposure_pipeline, error) &&
        exposure_encoder->setTexture(0, hdr_color, error) &&
        exposure_encoder->setTexture(1, exposure.get(), error) &&
        exposure_encoder->setSampler(0, mImpl->meter_sampler.get(), error) &&
        exposure_encoder->setBytes(
            0, &exposure_constants, sizeof(exposure_constants), error) &&
        exposure_encoder->dispatch(1, 1, 1, 1, 1, 1, error) &&
        exposure_encoder->end(error);
    if (!encoded)
    {
        return nullptr;
    }

    const auto encode_bloom_pass = [&](ALMetalRenderTarget& target,
                                       const ALMetalRenderPipeline& pipeline,
                                       const ALMetalTexture& source,
                                       const ALMetalSamplerState& sampler,
                                       const void* constants,
                                       std::size_t constants_size,
                                       ALMetalLoadAction load,
                                       const char* label)
    {
        ALMetalRenderPassConfig config;
        config.colors[0].load = load;
        config.colors[0].store = ALMetalStoreAction::store;
        config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
        auto pass = target.makeRenderPass(config, error);
        auto encoder = pass ? ALMetalRenderEncoder::begin(
            *command, *pass, label, error)
            : nullptr;
        const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
        return encoder &&
            encoder->setPipeline(pipeline, error) &&
            encoder->setViewport(
                {0.0, 0.0,
                 static_cast<double>(descriptor.width),
                 static_cast<double>(descriptor.height), 0.0, 1.0},
                error) &&
            encoder->setScissor(
                {0, 0, descriptor.width, descriptor.height}, error) &&
            encoder->setFragmentTexture(0, &source, error) &&
            encoder->setFragmentSampler(
                0, &sampler, error) &&
            encoder->setFragmentBytes(
                0, constants, constants_size, error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
            encoder->end(error);
    };

    const ALMetalTexture* bloom_color = hdr_color;
    const ALMetalTexture* cross_filter_color = hdr_color;
    if (bloom_active)
    {
        PBRPreviewBloomExtractConstants extract_constants{};
        extract_constants.params[0] = scene.bloom_threshold;
        extract_constants.params[1] = scene.bloom_knee;
        extract_constants.params[2] = scene.bloom_firefly_clamp;
        extract_constants.params[3] = scene.bloom_alpha_glow_boost;
        std::memcpy(extract_constants.halation_weights,
                    scene.bloom_halation_weights,
                    sizeof(scene.bloom_halation_weights));
        encoded = encode_bloom_pass(
            *bloom_targets[0],
            *mImpl->bloom_extract_pipelines[bloom_pipeline_index],
            *hdr_color, *mImpl->bloom_sampler,
            &extract_constants, sizeof(extract_constants),
            ALMetalLoadAction::clear,
            "Alchemy PBR preview bloom extract encoder");

        for (std::size_t mip = 1;
             encoded && mip < bloom_targets.size(); ++mip)
        {
            const ALMetalTexture* source =
                bloom_targets[mip - 1]->colorTexture(0);
            if (!source)
            {
                encoded = false;
                break;
            }
            PBRPreviewBloomFilterConstants filter_constants{};
            filter_constants.params[0] = 1.f /
                static_cast<float>(source->width());
            filter_constants.params[1] = 1.f /
                static_cast<float>(source->height());
            filter_constants.params[2] = mip == 1 ? 1.f : 0.f;
            encoded = encode_bloom_pass(
                *bloom_targets[mip],
                *mImpl->bloom_downsample_pipelines[bloom_pipeline_index],
                *source, *mImpl->bloom_sampler,
                &filter_constants, sizeof(filter_constants),
                ALMetalLoadAction::clear,
                "Alchemy PBR preview bloom downsample encoder");
        }

        if (encoded && cross_filter_active)
        {
            const ALMetalRenderTargetDescriptor& descriptor =
                cross_filter_targets[0]->descriptor();
            const float texel_x = 1.f /
                static_cast<float>(descriptor.width);
            const float texel_y = 1.f /
                static_cast<float>(descriptor.height);
            constexpr float pi = 3.14159265358979323846f;
            constexpr float pass_scales[3] = {1.f, 4.f, 16.f};
            constexpr float maximum_step = 63.f;
            const float angle = scene.cross_filter_angle * (pi / 180.f);
            const float falloff = std::exp(
                scene.cross_filter_falloff * 6.f / maximum_step);

            for (std::uint32_t arm = 0;
                 encoded && arm < scene.cross_filter_points; ++arm)
            {
                const float theta = angle + 2.f * pi *
                    static_cast<float>(arm) /
                    static_cast<float>(scene.cross_filter_points);
                const ALMetalTexture* sources[3] = {
                    bloom_targets[0]->colorTexture(0),
                    cross_filter_targets[0]->colorTexture(0),
                    cross_filter_targets[1]->colorTexture(0),
                };
                ALMetalRenderTarget* destinations[3] = {
                    cross_filter_targets[0].get(),
                    cross_filter_targets[1].get(),
                    cross_filter_targets[2].get(),
                };
                for (std::size_t pass = 0; encoded && pass < 3; ++pass)
                {
                    if (!sources[pass])
                    {
                        encoded = false;
                        break;
                    }
                    PBRPreviewCrossFilterConstants constants{};
                    constants.sampling[0] = texel_x;
                    constants.sampling[1] = texel_y;
                    constants.sampling[2] = scene.cross_filter_length;
                    constants.sampling[3] = scene.cross_filter_chromatic;
                    constants.filtering[0] = std::cos(theta);
                    constants.filtering[1] = std::sin(theta);
                    constants.filtering[2] = pass_scales[pass];
                    constants.filtering[3] = falloff;
                    const bool additive = pass == 2 && arm > 0;
                    encoded = encode_bloom_pass(
                        *destinations[pass],
                        additive ? *mImpl->cross_filter_add_pipeline
                                 : *mImpl->cross_filter_pipeline,
                        *sources[pass], *mImpl->cross_filter_sampler,
                        &constants, sizeof(constants),
                        additive ? ALMetalLoadAction::load
                                 : ALMetalLoadAction::clear,
                        "Alchemy PBR preview cross-filter encoder");
                }
            }
            cross_filter_color = encoded
                ? cross_filter_targets[2]->colorTexture(0) : nullptr;
        }

        for (std::size_t mip = bloom_targets.size() - 1;
             encoded && mip > 0; --mip)
        {
            const ALMetalTexture* source =
                bloom_targets[mip]->colorTexture(0);
            if (!source)
            {
                encoded = false;
                break;
            }
            PBRPreviewBloomFilterConstants filter_constants{};
            filter_constants.params[0] = 1.f /
                static_cast<float>(source->width());
            filter_constants.params[1] = 1.f /
                static_cast<float>(source->height());
            filter_constants.params[2] = scene.bloom_scatter;
            encoded = encode_bloom_pass(
                *bloom_targets[mip - 1],
                *mImpl->bloom_upsample_pipelines[bloom_pipeline_index],
                *source, *mImpl->bloom_sampler,
                &filter_constants, sizeof(filter_constants),
                ALMetalLoadAction::load,
                "Alchemy PBR preview bloom upsample encoder");
        }
        bloom_color = encoded ? bloom_targets[0]->colorTexture(0) : nullptr;
    }
    if (!encoded || !bloom_color || !cross_filter_color)
    {
        return nullptr;
    }

    auto post_pass = post_target->makeRenderPass(pass_config, error);
    auto post_encoder = post_pass ? ALMetalRenderEncoder::begin(
        *command, *post_pass, "Alchemy PBR preview post encoder", error)
        : nullptr;
    PBRPreviewPostConstants post_constants{};
    post_constants.post[0] = scene.exposure;
    post_constants.post[1] = scene.tonemap_mix;
    post_constants.post[2] = static_cast<float>(scene.tonemap_type);
    post_constants.post[3] = pipeline_index == 1 ? 1.f : 0.f;
    std::memcpy(post_constants.tonemap_params, scene.tonemap_params,
                sizeof(post_constants.tonemap_params));
    post_constants.bloom[0] = bloom_active ? scene.bloom_strength : 0.f;
    post_constants.bloom[1] = bloom_active && scene.bloom_halation
        ? scene.bloom_halation_strength : 0.f;
    post_constants.bloom[2] = bloom_active ? 1.f : 0.f;
    post_constants.bloom[3] = cross_filter_active
        ? scene.cross_filter_strength : 0.f;
    std::memcpy(post_constants.halation_tint,
                scene.bloom_halation_tint,
                sizeof(scene.bloom_halation_tint));
    encoded = post_encoder &&
        post_encoder->setPipeline(
            *mImpl->post_pipelines[pipeline_index], error) &&
        post_encoder->setViewport(viewport, error) &&
        post_encoder->setScissor(scissor, error) &&
        post_encoder->setFragmentTexture(0, hdr_color, error) &&
        post_encoder->setFragmentTexture(1, exposure.get(), error) &&
        post_encoder->setFragmentTexture(2, bloom_color, error) &&
        post_encoder->setFragmentTexture(3, cross_filter_color, error) &&
        post_encoder->setFragmentSampler(
            0, mImpl->post_sampler.get(), error) &&
        post_encoder->setFragmentSampler(
            1, mImpl->bloom_sampler.get(), error) &&
        post_encoder->setFragmentSampler(
            2, mImpl->cross_filter_sampler.get(), error) &&
        post_encoder->setFragmentBytes(
            0, &post_constants, sizeof(post_constants), error) &&
        post_encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
        post_encoder->end(error);

    const ALMetalTexture* post_color = post_target->colorTexture(0);
    if (!encoded || !post_color ||
        !output->copyFrom(*command, *post_color, {}, error) ||
        (output_descriptor.mipmap_level_count > 1 &&
         !output->generateMipmaps(*command, error)) ||
        !command->commit(error))
    {
        return nullptr;
    }
    return output;
}
