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

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

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
        float post[4];
    };

    static_assert(sizeof(PBRPreviewConstants) == 384);

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
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2> pipelines;
    std::unique_ptr<ALMetalSamplerState> color_sampler;
    std::unique_ptr<ALMetalSamplerState> data_sampler;
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
        !impl->library->hasFunction("almetal_pbr_preview_fragment"))
    {
        if (impl->library)
        {
            setError(error,
                     "Built-in Metal library is missing PBR preview functions");
        }
        return nullptr;
    }

    constexpr std::array<ALMetalPixelFormat, 2> formats{{
        ALMetalPixelFormat::rgba8_unorm,
        ALMetalPixelFormat::rgba8_unorm_srgb,
    }};
    for (std::size_t index = 0; index < formats.size(); ++index)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_pbr_preview_vertex";
        descriptor.fragment_function = "almetal_pbr_preview_fragment";
        descriptor.colors[0].format = formats[index];
        descriptor.colors[0].blending_enabled = true;
        descriptor.colors[0].source_rgb = ALMetalBlendFactor::source_alpha;
        descriptor.colors[0].destination_rgb =
            ALMetalBlendFactor::one_minus_source_alpha;
        descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
        descriptor.colors[0].destination_alpha =
            ALMetalBlendFactor::one_minus_source_alpha;
        descriptor.label = index == 0
            ? "Alchemy PBR material preview pipeline"
            : "Alchemy PBR material preview sRGB pipeline";
        impl->pipelines[index] = ALMetalRenderPipeline::create(
            device, descriptor, error);
        if (!impl->pipelines[index])
        {
            return nullptr;
        }
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
            !std::isfinite(batch.exposure) ||
            !std::isfinite(batch.tonemap_mix) ||
            batch.metallic_factor < 0.f || batch.metallic_factor > 1.f ||
            batch.roughness_factor < 0.f || batch.roughness_factor > 1.f ||
            batch.alpha_cutoff < -1.f || batch.alpha_cutoff > 1.f ||
            batch.exposure < 0.f || batch.tonemap_mix < 0.f ||
            batch.tonemap_mix > 1.f || batch.tonemap_type > 3)
        {
            setError(error, "Metal PBR preview batch is invalid");
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor target_descriptor;
    target_descriptor.width = output_template.width;
    target_descriptor.height = output_template.height;
    target_descriptor.colors[0].format = output_template.format;
    target_descriptor.colors[0].shader_read = true;
    target_descriptor.colors[0].label = "Alchemy PBR preview color";
    target_descriptor.label = "Alchemy PBR preview target";
    auto target = ALMetalRenderTarget::create(
        *mImpl->device, target_descriptor, error);
    if (!target)
    {
        return nullptr;
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
    auto pass = target->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy PBR preview encoder", error) : nullptr;
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
    bool encoded = encoder &&
        encoder->setPipeline(*mImpl->pipelines[pipeline_index], error) &&
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error);

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
        constants.material[3] = batch.exposure;
        constants.post[0] = batch.tonemap_mix;
        constants.post[1] = static_cast<float>(batch.tonemap_type);
        constants.post[2] = pipeline_index == 1 ? 1.f : 0.f;

        encoded =
            encoder->setCullMode(batch.double_sided
                ? ALMetalCullMode::none : ALMetalCullMode::back, error) &&
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                1, *batch.vertices, batch.normal_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBuffer(
                3, *batch.vertices, batch.color_offset, error) &&
            encoder->setVertexBuffer(
                4, *batch.vertices, batch.tangent_offset, error) &&
            encoder->setVertexBytes(
                5, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->setFragmentTexture(
                0, batch.base_color_texture, error) &&
            encoder->setFragmentTextureLinear(
                1, batch.normal_texture, error) &&
            encoder->setFragmentTextureLinear(
                2, batch.orm_texture, error) &&
            encoder->setFragmentTexture(
                3, batch.emissive_texture, error) &&
            encoder->setFragmentSampler(
                0, mImpl->color_sampler.get(), error) &&
            encoder->setFragmentSampler(
                1, mImpl->data_sampler.get(), error) &&
            encoder->setFragmentSampler(
                2, mImpl->data_sampler.get(), error) &&
            encoder->setFragmentSampler(
                3, mImpl->color_sampler.get(), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle, batch.index_count,
                batch.index_type, *batch.indices,
                batch.index_offset, 1, error);
    }

    const ALMetalTexture* color = target->colorTexture(0);
    if (!encoded || !encoder->end(error) || !color ||
        !output->copyFrom(*command, *color, {}, error) ||
        (output_descriptor.mipmap_level_count > 1 &&
         !output->generateMipmaps(*command, error)) ||
        !command->commit(error))
    {
        return nullptr;
    }
    return output;
}
