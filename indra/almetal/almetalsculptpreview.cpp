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

    struct alignas(16) LitPreviewConstants
    {
        float modelview_projection[16];
        float normal_matrix[3][4];
        float light_direction[3][4];
        float light_diffuse[3][4];
        float diffuse_color[4];
    };

    static_assert(sizeof(LitPreviewConstants) == 224);
}

struct ALMetalLitPreviewRenderer::Impl
{
    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> solid_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> textured_pipeline;
    std::unique_ptr<ALMetalDepthStencilState> depth_state;
    std::unique_ptr<ALMetalSamplerState> texture_sampler;
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
        !impl->library->hasFunction("almetal_lit_preview_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_textured_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_textured_fragment"))
    {
        if (impl->library)
        {
            setError(error, "Built-in Metal library is missing lit preview functions");
        }
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor pipeline_descriptor;
    pipeline_descriptor.library = impl->library.get();
    pipeline_descriptor.vertex_function = "almetal_lit_preview_vertex";
    pipeline_descriptor.fragment_function = "almetal_lit_preview_fragment";
    pipeline_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
    pipeline_descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
    pipeline_descriptor.label = "Alchemy lit preview pipeline";
    impl->solid_pipeline = ALMetalRenderPipeline::create(
        device, pipeline_descriptor, error);
    if (!impl->solid_pipeline)
    {
        return nullptr;
    }

    pipeline_descriptor.vertex_function = "almetal_lit_preview_textured_vertex";
    pipeline_descriptor.fragment_function = "almetal_lit_preview_textured_fragment";
    pipeline_descriptor.label = "Alchemy textured lit preview pipeline";
    impl->textured_pipeline = ALMetalRenderPipeline::create(
        device, pipeline_descriptor, error);
    if (!impl->textured_pipeline)
    {
        return nullptr;
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

    ALMetalSamplerDescriptor sampler_descriptor;
    sampler_descriptor.max_anisotropy = 16;
    sampler_descriptor.label = "Alchemy lit preview anisotropic wrap sampler";
    impl->texture_sampler = ALMetalSamplerState::create(
        device, sampler_descriptor, error);
    if (!impl->texture_sampler)
    {
        return nullptr;
    }

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
    return render(scene, error);
}

std::unique_ptr<ALMetalTexture> ALMetalLitPreviewRenderer::render(
    const ALMetalLitPreviewScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || !scene.destination_template)
    {
        setError(error, "Metal lit preview scene is incomplete");
        return nullptr;
    }

    const ALMetalTextureDescriptor& template_descriptor =
        scene.destination_template->descriptor();
    if (template_descriptor.type != ALMetalTextureType::texture_2d ||
        template_descriptor.format != ALMetalPixelFormat::rgba8_unorm ||
        template_descriptor.sample_count != 1 ||
        template_descriptor.storage != ALMetalStorageMode::private_gpu ||
        !alMetalHasUsage(template_descriptor.usage,
                         ALMetalTextureUsage::shader_read))
    {
        setError(error, "Metal lit preview destination must be a private shader-readable RGBA8 texture");
        return nullptr;
    }

    constexpr std::size_t vector_stride = sizeof(float) * 4;
    const auto range_fits = [](std::size_t offset, std::size_t count,
                               std::size_t stride, std::size_t length)
    {
        return count <= (std::numeric_limits<std::size_t>::max() - offset) / stride &&
               offset + count * stride <= length;
    };
    for (std::size_t batch_index = 0; batch_index < scene.batch_count; ++batch_index)
    {
        const ALMetalLitPreviewBatch& batch = scene.batches[batch_index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count == 0)
        {
            setError(error, "Metal lit preview batch is incomplete");
            return nullptr;
        }
        if (!range_fits(batch.position_offset, batch.vertex_count, vector_stride,
                        batch.vertices->length()) ||
            !range_fits(batch.normal_offset, batch.vertex_count, vector_stride,
                        batch.vertices->length()) ||
            (batch.texture &&
             !range_fits(batch.texcoord_offset, batch.vertex_count,
                         sizeof(float) * 2, batch.vertices->length())) ||
            !range_fits(batch.index_offset, batch.index_count, index_stride,
                        batch.indices->length()))
        {
            setError(error, "Metal lit preview geometry range exceeds its buffers");
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

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {
        scene.clear_color[0], scene.clear_color[1],
        scene.clear_color[2], scene.clear_color[3],
    };
    pass_config.depth_load = ALMetalLoadAction::clear;
    pass_config.depth_store = ALMetalStoreAction::dont_care;
    pass_config.clear_depth = 1.0;
    auto pass = target->makeRenderPass(pass_config, error);
    if (!pass)
    {
        return nullptr;
    }
    auto encoder = ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy lit preview encoder", error);
    if (!encoder)
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
    bool encoded =
        color &&
        encoder->setDepthStencilState(mImpl->depth_state.get(), error) &&
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::back, error) &&
        // Mirroring GL framebuffer row orientation flips clip Y and therefore
        // changes the projected front-face winding.
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error);
    for (std::size_t batch_index = 0;
         encoded && batch_index < scene.batch_count; ++batch_index)
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

        encoded =
            encoder->setPipeline(
                batch.texture ? *mImpl->textured_pipeline
                              : *mImpl->solid_pipeline,
                error) &&
            encoder->setVertexBuffer(0, *batch.vertices,
                                     batch.position_offset, error) &&
            encoder->setVertexBuffer(1, *batch.vertices,
                                     batch.normal_offset, error) &&
            encoder->setVertexBytes(2, &constants, sizeof(constants), error);
        if (encoded && batch.texture)
        {
            encoded =
                encoder->setVertexBuffer(3, *batch.vertices,
                                         batch.texcoord_offset, error) &&
                // The legacy preview uses AnisoWrap without sRGB decode. Bind
                // the linear-compatible view so Metal samples the same encoded
                // bytes even when LLImageGL owns sRGB storage.
                encoder->setFragmentTextureLinear(0, batch.texture, error) &&
                encoder->setFragmentSampler(0, mImpl->texture_sampler.get(),
                                            error);
        }
        encoded = encoded &&
            encoder->drawIndexed(ALMetalPrimitiveType::triangle,
                                 batch.index_count, batch.index_type,
                                 *batch.indices, batch.index_offset, 1, error);
    }
    encoded = encoded && encoder->end(error);
    if (!encoded || !output->copyFrom(*command, *color, {}, error) ||
        (output_descriptor.mipmap_level_count > 1 &&
         !output->generateMipmaps(*command, error)) ||
        !command->commit(error))
    {
        return nullptr;
    }
    return output;
}
