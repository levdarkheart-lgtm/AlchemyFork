/**
 * @file almetalterrainpaint.cpp
 * @brief Native Metal terrain paintmap bake.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetalterrainpaint.h"

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

    struct alignas(16) TerrainPaintConstants
    {
        float modelview_projection[16];
        float precision;
        float padding[3]{};
    };
}

struct ALMetalTerrainPaintRenderer::Impl
{
    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    std::unique_ptr<ALMetalSamplerState> sampler;
    std::uint64_t shader_generation = 0;
};

ALMetalTerrainPaintRenderer::ALMetalTerrainPaintRenderer(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalTerrainPaintRenderer::~ALMetalTerrainPaintRenderer() = default;

std::uint64_t ALMetalTerrainPaintRenderer::shaderReloadGeneration() const noexcept
{
    return mImpl ? mImpl->shader_generation : 0;
}

std::unique_ptr<ALMetalTerrainPaintRenderer> ALMetalTerrainPaintRenderer::create(
    const ALMetalDevice& device,
    std::string* error)
{
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->shader_generation = ALMetalShaderLibrary::builtinReloadGeneration();
    impl->library = ALMetalShaderLibrary::loadBuiltin(device, error);
    if (!impl->library ||
        !impl->library->hasFunction("almetal_terrain_paint_vertex") ||
        !impl->library->hasFunction("almetal_terrain_paint_fragment"))
    {
        if (impl->library)
        {
            setError(error, "Built-in Metal library is missing terrain paint functions");
        }
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor pipeline_descriptor;
    pipeline_descriptor.library = impl->library.get();
    pipeline_descriptor.vertex_function = "almetal_terrain_paint_vertex";
    pipeline_descriptor.fragment_function = "almetal_terrain_paint_fragment";
    pipeline_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
    pipeline_descriptor.label = "Alchemy terrain paintmap bake pipeline";
    impl->pipeline = ALMetalRenderPipeline::create(device, pipeline_descriptor, error);
    if (!impl->pipeline)
    {
        return nullptr;
    }

    ALMetalSamplerDescriptor sampler_descriptor;
    sampler_descriptor.min_filter = ALMetalSamplerFilter::linear;
    sampler_descriptor.mag_filter = ALMetalSamplerFilter::linear;
    sampler_descriptor.mip_filter = ALMetalSamplerMipFilter::linear;
    sampler_descriptor.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler_descriptor.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler_descriptor.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler_descriptor.max_anisotropy = 16;
    sampler_descriptor.label = "Alchemy terrain alpha-ramp sampler";
    impl->sampler = ALMetalSamplerState::create(device, sampler_descriptor, error);
    if (!impl->sampler)
    {
        return nullptr;
    }

    return std::unique_ptr<ALMetalTerrainPaintRenderer>(
        new ALMetalTerrainPaintRenderer(std::move(impl)));
}

std::unique_ptr<ALMetalTexture> ALMetalTerrainPaintRenderer::render(
    const ALMetalTerrainPaintBatch& batch,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !batch.vertices || !batch.indices ||
        !batch.alpha_ramp || !batch.destination_template || batch.index_count == 0)
    {
        setError(error, "Metal terrain paint batch is incomplete");
        return nullptr;
    }
    if (!std::isfinite(batch.precision) || batch.precision < 1.f)
    {
        setError(error, "Metal terrain paint precision must be positive and finite");
        return nullptr;
    }

    const ALMetalTextureDescriptor& source_descriptor =
        batch.destination_template->descriptor();
    if (source_descriptor.type != ALMetalTextureType::texture_2d ||
        source_descriptor.format != ALMetalPixelFormat::rgba8_unorm ||
        source_descriptor.sample_count != 1 ||
        !alMetalHasUsage(source_descriptor.usage, ALMetalTextureUsage::shader_read))
    {
        setError(error, "Metal terrain paint destination must be a shader-readable RGBA8 texture");
        return nullptr;
    }
    if (!alMetalHasUsage(batch.alpha_ramp->descriptor().usage,
                         ALMetalTextureUsage::shader_read))
    {
        setError(error, "Metal terrain alpha ramp is not shader-readable");
        return nullptr;
    }

    const std::size_t index_stride = batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
    if (batch.position_offset >= batch.vertices->length() ||
        batch.texcoord_offset >= batch.vertices->length() ||
        batch.index_count > (std::numeric_limits<std::size_t>::max() - batch.index_offset) /
                                index_stride ||
        batch.index_offset + static_cast<std::size_t>(batch.index_count) * index_stride >
            batch.indices->length())
    {
        setError(error, "Metal terrain paint geometry range exceeds its buffers");
        return nullptr;
    }

    if (mImpl->shader_generation != ALMetalShaderLibrary::builtinReloadGeneration())
    {
        auto candidate = create(*mImpl->device, error);
        if (!candidate) return nullptr;
        mImpl = std::move(candidate->mImpl);
    }

    ALMetalTextureDescriptor destination_descriptor = source_descriptor;
    destination_descriptor.usage = destination_descriptor.usage |
                                   ALMetalTextureUsage::render_target;
    destination_descriptor.label = "Alchemy terrain paintmap bake";
    auto destination = ALMetalTexture::create(
        *mImpl->device, destination_descriptor, error);
    if (!destination)
    {
        return nullptr;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy terrain paintmap bake", error);
    if (!command)
    {
        return nullptr;
    }

    ALMetalRenderPassColorConfig color;
    color.load = ALMetalLoadAction::clear;
    color.store = ALMetalStoreAction::store;
    color.clear = {0.0, 0.0, 0.0, 0.0};
    auto pass = ALMetalRenderPass::createForColorAttachment(
        *mImpl->device, destination->nativeHandle(), ALMetalPixelFormat::rgba8_unorm,
        color, error);
    if (!pass)
    {
        return nullptr;
    }
    auto encoder = ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy terrain paintmap bake encoder", error);
    if (!encoder)
    {
        return nullptr;
    }

    TerrainPaintConstants constants{};
    std::memcpy(constants.modelview_projection, batch.modelview_projection,
                sizeof(constants.modelview_projection));
    constants.precision = batch.precision;
    const ALMetalViewport viewport{
        0.0, 0.0,
        static_cast<double>(destination_descriptor.width),
        static_cast<double>(destination_descriptor.height),
        0.0, 1.0,
    };
    const ALMetalScissorRect scissor{
        0, 0, destination_descriptor.width, destination_descriptor.height,
    };

    const bool encoded =
        encoder->setPipeline(*mImpl->pipeline, error) &&
        encoder->setVertexBuffer(0, *batch.vertices, batch.position_offset, error) &&
        encoder->setVertexBuffer(1, *batch.vertices, batch.texcoord_offset, error) &&
        encoder->setVertexBytes(2, &constants, sizeof(constants), error) &&
        encoder->setFragmentTexture(0, batch.alpha_ramp, error) &&
        encoder->setFragmentSampler(0, mImpl->sampler.get(), error) &&
        encoder->setFragmentBytes(0, &constants, sizeof(constants), error) &&
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::counter_clockwise, error) &&
        encoder->drawIndexed(ALMetalPrimitiveType::triangle, batch.index_count,
                             batch.index_type, *batch.indices, batch.index_offset,
                             1, error) &&
        encoder->end(error);
    if (!encoded ||
        (destination_descriptor.mipmap_level_count > 1 &&
         !destination->generateMipmaps(*command, error)) ||
        !command->commit(error))
    {
        return nullptr;
    }
    return destination;
}
