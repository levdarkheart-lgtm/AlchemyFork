/**
 * @file almetaltextureops.cpp
 * @brief Cached GPU texture transforms used by production resource owners.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetaltextureops.h"

#include "almetalpipeline.h"

#include <cstdint>
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
}

struct ALMetalTextureResampler::Impl
{
    struct Pipeline
    {
        ALMetalPixelFormat format = ALMetalPixelFormat::invalid;
        std::unique_ptr<ALMetalRenderPipeline> state;
    };

    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalSamplerState> sampler;
    std::vector<Pipeline> pipelines;

    ALMetalRenderPipeline* pipeline(ALMetalPixelFormat format, std::string* error)
    {
        for (Pipeline& entry : pipelines)
        {
            if (entry.format == format)
            {
                return entry.state.get();
            }
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function = "almetal_resample_fragment";
        descriptor.colors[0].format = format;
        descriptor.label = "Alchemy bilinear texture-resample pipeline";
        auto state = ALMetalRenderPipeline::create(*device, descriptor, error);
        if (!state)
        {
            return nullptr;
        }
        pipelines.push_back({format, std::move(state)});
        return pipelines.back().state.get();
    }
};

ALMetalTextureResampler::ALMetalTextureResampler(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalTextureResampler::~ALMetalTextureResampler() = default;

std::unique_ptr<ALMetalTextureResampler> ALMetalTextureResampler::create(
    const ALMetalDevice& device,
    std::string* error)
{
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->library = ALMetalShaderLibrary::loadBuiltin(device, error);
    if (!impl->library)
    {
        return nullptr;
    }

    ALMetalSamplerDescriptor sampler_descriptor;
    sampler_descriptor.min_filter = ALMetalSamplerFilter::linear;
    sampler_descriptor.mag_filter = ALMetalSamplerFilter::linear;
    sampler_descriptor.mip_filter = ALMetalSamplerMipFilter::none;
    sampler_descriptor.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler_descriptor.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler_descriptor.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler_descriptor.label = "Alchemy bilinear texture-resample sampler";
    impl->sampler = ALMetalSamplerState::create(device, sampler_descriptor, error);
    if (!impl->sampler)
    {
        return nullptr;
    }
    return std::unique_ptr<ALMetalTextureResampler>(
        new ALMetalTextureResampler(std::move(impl)));
}

bool ALMetalTextureResampler::supports(ALMetalPixelFormat format) noexcept
{
    switch (format)
    {
        case ALMetalPixelFormat::r8_unorm:
        case ALMetalPixelFormat::rg8_unorm:
        case ALMetalPixelFormat::rgba8_unorm:
        case ALMetalPixelFormat::rgba8_unorm_srgb:
        case ALMetalPixelFormat::bgra8_unorm:
        case ALMetalPixelFormat::bgra8_unorm_srgb:
        case ALMetalPixelFormat::r16_unorm:
        case ALMetalPixelFormat::r16_float:
        case ALMetalPixelFormat::rg16_float:
        case ALMetalPixelFormat::rgba16_unorm:
        case ALMetalPixelFormat::rgba16_float:
        case ALMetalPixelFormat::r32_float:
        case ALMetalPixelFormat::rg32_float:
        case ALMetalPixelFormat::rgba32_float:
        case ALMetalPixelFormat::rgb10a2_unorm:
        case ALMetalPixelFormat::rg11b10_float:
            return true;
        default:
            // Integer and compressed textures cannot be linearly filtered.
            return false;
    }
}

bool ALMetalTextureResampler::encode(ALMetalCommandBuffer& command_buffer,
                                     const ALMetalTexture& source,
                                     const ALMetalTexture& destination,
                                     std::string* error)
{
    if (!mImpl || !mImpl->device)
    {
        setError(error, "Metal texture resampler is unavailable");
        return false;
    }
    const ALMetalTextureDescriptor& source_descriptor = source.descriptor();
    const ALMetalTextureDescriptor& destination_descriptor = destination.descriptor();
    if (source_descriptor.type != ALMetalTextureType::texture_2d ||
        destination_descriptor.type != ALMetalTextureType::texture_2d ||
        source_descriptor.format != destination_descriptor.format ||
        !supports(source_descriptor.format))
    {
        setError(error, "Metal texture resample requires matching filterable 2D formats");
        return false;
    }
    if (!alMetalHasUsage(source_descriptor.usage, ALMetalTextureUsage::shader_read) ||
        !alMetalHasUsage(destination_descriptor.usage, ALMetalTextureUsage::render_target))
    {
        setError(error, "Metal texture resample usage must permit source sampling and destination rendering");
        return false;
    }

    ALMetalRenderPassColorConfig color;
    color.load = ALMetalLoadAction::dont_care;
    color.store = ALMetalStoreAction::store;
    const ALMetalPixelFormat render_format = alMetalLinearFormat(
        destination_descriptor.format);
    auto pass = ALMetalRenderPass::createForColorAttachment(
        *mImpl->device, destination.nativeLinearHandle(), render_format,
        color, error);
    if (!pass)
    {
        return false;
    }
    ALMetalRenderPipeline* pipeline = mImpl->pipeline(render_format, error);
    if (!pipeline)
    {
        return false;
    }
    auto encoder = ALMetalRenderEncoder::begin(
        command_buffer, *pass, "Alchemy bilinear texture resample", error);
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0,
        0.0,
        static_cast<double>(destination_descriptor.width),
        static_cast<double>(destination_descriptor.height),
        0.0,
        1.0,
    };
    const ALMetalScissorRect scissor{
        0,
        0,
        destination_descriptor.width,
        destination_descriptor.height,
    };
    return encoder->setPipeline(*pipeline, error) &&
           encoder->setFragmentTextureLinear(0, &source, error) &&
           encoder->setFragmentSampler(0, mImpl->sampler.get(), error) &&
           encoder->setViewport(viewport, error) &&
           encoder->setScissor(scissor, error) &&
           encoder->setCullMode(ALMetalCullMode::none, error) &&
           encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
           encoder->end(error);
}

struct ALMetalNormalMapGenerator::Impl
{
    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalSamplerState> sampler;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
};

ALMetalNormalMapGenerator::ALMetalNormalMapGenerator(
    std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalNormalMapGenerator::~ALMetalNormalMapGenerator() = default;

std::unique_ptr<ALMetalNormalMapGenerator>
ALMetalNormalMapGenerator::create(const ALMetalDevice& device,
                                  std::string* error)
{
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->library = ALMetalShaderLibrary::loadBuiltin(device, error);
    if (!impl->library)
    {
        return nullptr;
    }

    ALMetalSamplerDescriptor sampler_descriptor;
    sampler_descriptor.min_filter = ALMetalSamplerFilter::linear;
    sampler_descriptor.mag_filter = ALMetalSamplerFilter::linear;
    sampler_descriptor.mip_filter = ALMetalSamplerMipFilter::none;
    sampler_descriptor.address_s = ALMetalSamplerAddressMode::repeat;
    sampler_descriptor.address_t = ALMetalSamplerAddressMode::repeat;
    sampler_descriptor.address_r = ALMetalSamplerAddressMode::repeat;
    sampler_descriptor.label = "Alchemy legacy bump-map generator sampler";
    impl->sampler = ALMetalSamplerState::create(
        device, sampler_descriptor, error);
    if (!impl->sampler)
    {
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor pipeline_descriptor;
    pipeline_descriptor.library = impl->library.get();
    pipeline_descriptor.vertex_function = "almetal_resample_vertex";
    pipeline_descriptor.fragment_function =
        "almetal_normal_map_generate_fragment";
    pipeline_descriptor.colors[0].format =
        ALMetalPixelFormat::rgba8_unorm;
    pipeline_descriptor.label = "Alchemy legacy bump-map generator pipeline";
    impl->pipeline = ALMetalRenderPipeline::create(
        device, pipeline_descriptor, error);
    if (!impl->pipeline)
    {
        return nullptr;
    }
    return std::unique_ptr<ALMetalNormalMapGenerator>(
        new ALMetalNormalMapGenerator(std::move(impl)));
}

bool ALMetalNormalMapGenerator::encode(
    ALMetalCommandBuffer& command_buffer,
    const ALMetalTexture& source,
    const ALMetalTexture& destination,
    float normal_scale,
    bool darkness,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !mImpl->pipeline || !mImpl->sampler)
    {
        setError(error, "Metal normal-map generator is unavailable");
        return false;
    }
    const ALMetalTextureDescriptor& source_descriptor = source.descriptor();
    const ALMetalTextureDescriptor& destination_descriptor =
        destination.descriptor();
    if (source_descriptor.type != ALMetalTextureType::texture_2d ||
        destination_descriptor.type != ALMetalTextureType::texture_2d ||
        destination_descriptor.format != ALMetalPixelFormat::rgba8_unorm ||
        source_descriptor.width == 0 || source_descriptor.height == 0 ||
        destination_descriptor.width == 0 ||
        destination_descriptor.height == 0 || normal_scale <= 0.f)
    {
        setError(error,
                 "Metal normal-map generation requires non-empty 2D "
                 "textures, RGBA8 output, and a positive scale");
        return false;
    }
    if (!alMetalHasUsage(source_descriptor.usage,
                         ALMetalTextureUsage::shader_read) ||
        !alMetalHasUsage(destination_descriptor.usage,
                         ALMetalTextureUsage::render_target))
    {
        setError(error,
                 "Metal normal-map usage must permit source sampling and "
                 "destination rendering");
        return false;
    }

    struct alignas(16) Constants
    {
        float step_x;
        float step_y;
        float scale;
        std::uint32_t darkness;
    };
    const Constants constants{
        1.f / static_cast<float>(destination_descriptor.width),
        1.f / static_cast<float>(destination_descriptor.height),
        normal_scale,
        darkness ? 1u : 0u,
    };

    ALMetalRenderPassColorConfig color;
    color.load = ALMetalLoadAction::dont_care;
    color.store = ALMetalStoreAction::store;
    auto pass = ALMetalRenderPass::createForColorAttachment(
        *mImpl->device, destination.nativeHandle(),
        ALMetalPixelFormat::rgba8_unorm, color, error);
    if (!pass)
    {
        return false;
    }
    auto encoder = ALMetalRenderEncoder::begin(
        command_buffer, *pass, "Alchemy legacy bump-map generation", error);
    if (!encoder)
    {
        return false;
    }
    const ALMetalViewport viewport{
        0.0,
        0.0,
        static_cast<double>(destination_descriptor.width),
        static_cast<double>(destination_descriptor.height),
        0.0,
        1.0,
    };
    const ALMetalScissorRect scissor{
        0,
        0,
        destination_descriptor.width,
        destination_descriptor.height,
    };
    return encoder->setPipeline(*mImpl->pipeline, error) &&
           encoder->setFragmentTexture(0, &source, error) &&
           encoder->setFragmentSampler(0, mImpl->sampler.get(), error) &&
           encoder->setFragmentBytes(
               0, &constants, sizeof(constants), error) &&
           encoder->setViewport(viewport, error) &&
           encoder->setScissor(scissor, error) &&
           encoder->setCullMode(ALMetalCullMode::none, error) &&
           encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
           encoder->end(error);
}
