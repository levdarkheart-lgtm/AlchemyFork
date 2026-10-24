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
    };

    static_assert(sizeof(LitPreviewConstants) == 224);
}

struct ALMetalLitPreviewRenderer::Impl
{
    struct PipelineSet
    {
        std::unique_ptr<ALMetalRenderPipeline> solid;
        std::unique_ptr<ALMetalRenderPipeline> textured;
        std::unique_ptr<ALMetalRenderPipeline> edge_u16;
        std::unique_ptr<ALMetalRenderPipeline> edge_u32;
        std::unique_ptr<ALMetalRenderPipeline> solid_blended;
        std::unique_ptr<ALMetalRenderPipeline> textured_blended;
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
    };

    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::array<PipelineSet, 2> pipelines;
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
        !impl->library->hasFunction("almetal_lit_preview_textured_fragment") ||
        !impl->library->hasFunction("almetal_lit_preview_edge_u16_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_edge_u32_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_skinned_vertex") ||
        !impl->library->hasFunction("almetal_lit_preview_skinned_textured_vertex") ||
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

        if (!create_pipeline(pipelines.solid,
                             "almetal_lit_preview_vertex",
                             "almetal_lit_preview_fragment",
                             "Alchemy lit preview" + suffix + " pipeline") ||
            !create_pipeline(pipelines.textured,
                             "almetal_lit_preview_textured_vertex",
                             "almetal_lit_preview_textured_fragment",
                             "Alchemy textured lit preview" + suffix + " pipeline") ||
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
                             "Alchemy skinned U32 lit preview edge" + suffix + " pipeline"))
        {
            return nullptr;
        }

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
    const Impl::PipelineSet& pipelines = mImpl->pipelines[
        template_descriptor.format == ALMetalPixelFormat::rgba8_unorm_srgb ? 1 : 0];

    constexpr std::size_t vector_stride = sizeof(float) * 4;
    const auto range_fits = [](std::size_t offset, std::size_t count,
                               std::size_t stride, std::size_t length)
    {
        return count <= (std::numeric_limits<std::size_t>::max() - offset) / stride &&
               offset + count * stride <= length;
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
            const std::size_t expected_palette_floats =
                4 + static_cast<std::size_t>(batch.skin_joint_count) * 12;
            if (batch.skin_joint_count == 0 ||
                batch.skin_joint_count > max_skin_joints ||
                batch.skin_palette_float_count != expected_palette_floats ||
                !range_fits(batch.weight_offset, batch.vertex_count,
                            vector_stride, batch.vertices->length()))
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
                    candidate.skin_joint_count == batch.skin_joint_count)
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
                 batch.skin_joint_count != 0)
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

            const bool skinned = batch.skin_palette != nullptr;
            const ALMetalRenderPipeline* fill_pipeline = nullptr;
            if (batch.texture)
            {
                if (skinned)
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
                if (skinned)
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
                                         batch.position_offset, error) &&
                encoder->setVertexBuffer(1, *batch.vertices,
                                         batch.normal_offset, error) &&
                encoder->setVertexBytes(2, &constants, sizeof(constants), error);
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
            if (encoded && batch.texture)
            {
                encoded =
                    encoder->setVertexBuffer(3, *batch.vertices,
                                             batch.texcoord_offset, error) &&
                    // The legacy preview uses AnisoWrap without sRGB decode.
                    // Bind the linear-compatible view so Metal samples the
                    // same encoded bytes even when LLImageGL owns sRGB storage.
                    encoder->setFragmentTextureLinear(0, batch.texture, error) &&
                    encoder->setFragmentSampler(0, mImpl->texture_sampler.get(),
                                                error);
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
