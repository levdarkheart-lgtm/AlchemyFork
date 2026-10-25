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

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

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
        float texture_matrix[16];
        std::uint32_t depth_convention[4]{};
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
}

struct ALMetalWorldGBufferRenderer::Impl
{
    struct PipelineSet
    {
        std::unique_ptr<ALMetalRenderPipeline> normal_10;
        std::unique_ptr<ALMetalRenderPipeline> normal_16;
    };

    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::array<PipelineSet, 2> pipelines;
    std::unique_ptr<ALMetalDepthStencilState> forward_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_depth;
    std::unique_ptr<ALMetalSamplerState> diffuse_sampler;
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
        !impl->library->hasFunction("almetal_world_simple_fragment"))
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
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_world_simple_vertex";
        descriptor.fragment_function = "almetal_world_simple_fragment";
        descriptor.color_attachment_count = 4;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
        descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.colors[3].format = emissive_formats[hdr];
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;

        descriptor.colors[2].format = ALMetalPixelFormat::rgb10a2_unorm;
        descriptor.label = hdr
            ? "Alchemy HDR world simple G-buffer RGB10A2 pipeline"
            : "Alchemy world simple G-buffer RGB10A2 pipeline";
        impl->pipelines[hdr].normal_10 = ALMetalRenderPipeline::create(
            device, descriptor, error);
        if (!impl->pipelines[hdr].normal_10)
        {
            return nullptr;
        }

        descriptor.colors[2].format = ALMetalPixelFormat::rgba16_unorm;
        descriptor.label = hdr
            ? "Alchemy HDR world simple G-buffer RGBA16 pipeline"
            : "Alchemy world simple G-buffer RGBA16 pipeline";
        impl->pipelines[hdr].normal_16 = ALMetalRenderPipeline::create(
            device, descriptor, error);
        if (!impl->pipelines[hdr].normal_16)
        {
            return nullptr;
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
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.texture_count == 0 ||
            batch.texture_count >
                ALMetalWorldSimpleBatch::max_diffuse_textures ||
            batch.vertex_count == 0 || batch.index_count == 0 ||
            batch.index_count % 3 != 0 ||
            batch.position_offset % float4_stride != 0 ||
            batch.normal_offset % float4_stride != 0 ||
            batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
            batch.color_offset % (sizeof(std::uint8_t) * 4) != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !finiteValues(&batch.normal_matrix[0][0], 12) ||
            !finiteValues(batch.texture_matrix, 16))
        {
            setError(error, "Metal world simple batch is invalid");
            return nullptr;
        }
        if (!rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.normal_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       sizeof(float) * 2, batch.vertices->length()) ||
            !rangeFits(batch.color_offset, batch.vertex_count,
                       sizeof(std::uint8_t) * 4, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error,
                     "Metal world simple geometry range exceeds its buffers");
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

    const Impl::PipelineSet& pipelines = mImpl->pipelines[scene.hdr ? 1 : 0];
    const ALMetalRenderPipeline* pipeline = scene.normal_hdr
        ? pipelines.normal_16.get() : pipelines.normal_10.get();
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_depth.get() : mImpl->forward_depth.get();
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    bool encoded =
        encoder->setPipeline(*pipeline, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::back, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise,
                                       error);

    for (std::size_t index = 0; encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        WorldSimpleConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.normal_matrix, batch.normal_matrix,
                    sizeof(constants.normal_matrix));
        std::memcpy(constants.texture_matrix, batch.texture_matrix,
                    sizeof(constants.texture_matrix));
        constants.depth_convention[0] = scene.reverse_z ? 1u : 0u;
        constants.depth_convention[1] = batch.texture_count;

        encoded =
            encoder->setVertexBuffer(0, *batch.vertices,
                                     batch.position_offset, error) &&
            encoder->setVertexBuffer(1, *batch.vertices,
                                     batch.normal_offset, error) &&
            encoder->setVertexBuffer(2, *batch.vertices,
                                     batch.texcoord_offset, error) &&
            encoder->setVertexBuffer(3, *batch.vertices,
                                     batch.color_offset, error) &&
            encoder->setVertexBytes(4, &constants, sizeof(constants), error) &&
            encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error);
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
