/**
 * @file almetaluirenderer.cpp
 * @brief Production immediate-mode UI batches encoded by native Metal.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "almetaluirenderer.h"

#include "almetalsurface.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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

    bool sameBlend(const ALMetalUIBlendState& lhs, const ALMetalUIBlendState& rhs)
    {
        return lhs.enabled == rhs.enabled &&
               lhs.source_rgb == rhs.source_rgb &&
               lhs.destination_rgb == rhs.destination_rgb &&
               lhs.rgb_operation == rhs.rgb_operation &&
               lhs.source_alpha == rhs.source_alpha &&
               lhs.destination_alpha == rhs.destination_alpha &&
               lhs.alpha_operation == rhs.alpha_operation &&
               lhs.write_mask == rhs.write_mask;
    }

    bool sameSampler(const ALMetalSamplerDescriptor& lhs,
                     const ALMetalSamplerDescriptor& rhs)
    {
        return lhs.min_filter == rhs.min_filter &&
               lhs.mag_filter == rhs.mag_filter &&
               lhs.mip_filter == rhs.mip_filter &&
               lhs.address_s == rhs.address_s &&
               lhs.address_t == rhs.address_t &&
               lhs.address_r == rhs.address_r &&
               lhs.max_anisotropy == rhs.max_anisotropy &&
               lhs.normalized_coordinates == rhs.normalized_coordinates;
    }

    bool sameDepth(const ALMetalUIDepthState& lhs,
                   const ALMetalUIDepthState& rhs)
    {
        return lhs.enabled == rhs.enabled &&
               lhs.write_enabled == rhs.write_enabled &&
               lhs.compare == rhs.compare;
    }

    bool shaderUsesTexture(ALMetalUIShader shader)
    {
        return shader == ALMetalUIShader::textured ||
               shader == ALMetalUIShader::solid_color ||
               shader == ALMetalUIShader::alpha_mask_textured;
    }

    bool shaderUsesVertexColor(ALMetalUIShader shader)
    {
        return shader == ALMetalUIShader::textured ||
               shader == ALMetalUIShader::alpha_mask_textured ||
               shader == ALMetalUIShader::alpha_mask_solid;
    }

    bool rangeFits(std::size_t offset, std::size_t count,
                   std::size_t stride, std::size_t length)
    {
        return stride != 0 && offset <= length &&
               count <= (length - offset) / stride;
    }

    bool finiteFloats(const float* values, std::size_t count)
    {
        return values && std::all_of(
            values, values + count,
            [](float value) { return std::isfinite(value); });
    }

    const char* vertexFunction(ALMetalUIShader shader)
    {
        return shader == ALMetalUIShader::solid_color
            ? "almetal_solid_vertex" : "almetal_ui_vertex";
    }

    const char* fragmentFunction(ALMetalUIShader shader)
    {
        switch (shader)
        {
            case ALMetalUIShader::textured:            return "almetal_ui_fragment";
            case ALMetalUIShader::solid_color:         return "almetal_solid_fragment";
            case ALMetalUIShader::alpha_mask_textured: return "almetal_alpha_mask_fragment";
            case ALMetalUIShader::alpha_mask_solid:    return "almetal_alpha_mask_solid_fragment";
        }
        return "";
    }

    struct PrimitiveDraw
    {
        ALMetalPrimitiveType primitive = ALMetalPrimitiveType::triangle;
        std::uint32_t vertex_count = 0;
        std::uint32_t remap_mode = 0;
    };

    bool resolvePrimitive(ALMetalUIPrimitive source, std::uint32_t count,
                          PrimitiveDraw& result)
    {
        result.vertex_count = count;
        switch (source)
        {
            case ALMetalUIPrimitive::points:
                result.primitive = ALMetalPrimitiveType::point;
                return count > 0;
            case ALMetalUIPrimitive::lines:
                result.primitive = ALMetalPrimitiveType::line;
                return count >= 2;
            case ALMetalUIPrimitive::line_strip:
                result.primitive = ALMetalPrimitiveType::line_strip;
                return count >= 2;
            case ALMetalUIPrimitive::line_loop:
                result.primitive = ALMetalPrimitiveType::line_strip;
                result.vertex_count = count + 1;
                result.remap_mode = 2;
                return count >= 2;
            case ALMetalUIPrimitive::triangles:
                result.primitive = ALMetalPrimitiveType::triangle;
                return count >= 3;
            case ALMetalUIPrimitive::triangle_strip:
                result.primitive = ALMetalPrimitiveType::triangle_strip;
                return count >= 3;
            case ALMetalUIPrimitive::triangle_fan:
                result.primitive = ALMetalPrimitiveType::triangle;
                result.vertex_count = (count - 2) * 3;
                result.remap_mode = 1;
                return count >= 3;
        }
        return false;
    }
}

struct ALMetalUIRenderer::Impl
{
    struct PipelineEntry
    {
        ALMetalUIShader shader = ALMetalUIShader::textured;
        ALMetalUIBlendState blend{};
        ALMetalPixelFormat depth_format = ALMetalPixelFormat::invalid;
        std::unique_ptr<ALMetalRenderPipeline> state;
    };

    struct SamplerEntry
    {
        ALMetalSamplerDescriptor descriptor{};
        std::unique_ptr<ALMetalSamplerState> state;
    };

    struct HUDPipelineEntry
    {
        ALMetalHUDShader shader = ALMetalHUDShader::fullbright;
        ALMetalUIBlendState blend{};
        ALMetalPixelFormat depth_format = ALMetalPixelFormat::invalid;
        std::unique_ptr<ALMetalRenderPipeline> state;
    };

    struct DepthEntry
    {
        ALMetalUIDepthState descriptor{};
        std::unique_ptr<ALMetalDepthStencilState> state;
    };

    const ALMetalDevice* device = nullptr;
    ALMetalFrameScheduler* scheduler = nullptr;
    ALMetalSurface* surface = nullptr;
    ALMetalPixelFormat output_format = ALMetalPixelFormat::bgra8_unorm;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> background_pipeline;
    ALMetalPixelFormat background_depth_format =
        ALMetalPixelFormat::invalid;
    std::unique_ptr<ALMetalSamplerState> background_sampler;
    std::unique_ptr<ALMetalRenderPipeline> depth_clear_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> depth_restore_pipeline;
    std::unique_ptr<ALMetalSamplerState> depth_restore_sampler;
    ALMetalPixelFormat depth_region_format = ALMetalPixelFormat::invalid;
    std::unique_ptr<ALMetalRenderTarget> target;
    std::unique_ptr<ALMetalDrawable> drawable;
    std::unique_ptr<ALMetalRenderPass> pass;
    std::unique_ptr<ALMetalFrame> frame;
    std::unique_ptr<ALMetalRenderEncoder> encoder;
    std::vector<PipelineEntry> pipelines;
    std::vector<HUDPipelineEntry> hud_pipelines;
    std::vector<SamplerEntry> samplers;
    std::vector<DepthEntry> depths;
    ALMetalUIRendererStats totals{};
    std::uint64_t frame_batches = 0;
    std::uint64_t frame_vertices = 0;
    std::uint32_t frame_width = 0;
    std::uint32_t frame_height = 0;
    const ALMetalTexture* frame_background = nullptr;
    const ALMetalTexture* frame_depth = nullptr;

    ALMetalPixelFormat frameDepthFormat() const noexcept
    {
        return frame_depth ? frame_depth->descriptor().format
                           : ALMetalPixelFormat::invalid;
    }

    bool prepareBackground(std::string* error)
    {
        const ALMetalPixelFormat depth_format = frameDepthFormat();
        if (background_pipeline && background_sampler &&
            background_depth_format == depth_format)
        {
            return true;
        }
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function = "almetal_resample_fragment";
        descriptor.colors[0].format = output_format;
        descriptor.depth_stencil_format = depth_format;
        descriptor.label = "Alchemy native world/UI backing pipeline";
        background_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!background_pipeline)
        {
            return false;
        }
        background_depth_format = depth_format;
        ALMetalSamplerDescriptor sampler_descriptor;
        sampler_descriptor.min_filter = ALMetalSamplerFilter::nearest;
        sampler_descriptor.mag_filter = ALMetalSamplerFilter::nearest;
        sampler_descriptor.mip_filter = ALMetalSamplerMipFilter::none;
        sampler_descriptor.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
        sampler_descriptor.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
        sampler_descriptor.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
        sampler_descriptor.label = "Alchemy native world/UI backing sampler";
        background_sampler = ALMetalSamplerState::create(
            *device, sampler_descriptor, error);
        return background_sampler != nullptr;
    }

    bool prepareDepthRegions(std::string* error)
    {
        const ALMetalPixelFormat depth_format = frameDepthFormat();
        if (depth_clear_pipeline && depth_restore_pipeline &&
            depth_restore_sampler && depth_region_format == depth_format)
        {
            return true;
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function = "almetal_ui_depth_clear_fragment";
        descriptor.colors[0].format = output_format;
        descriptor.colors[0].write_mask = ALMetalColorWriteMask::none;
        descriptor.depth_stencil_format = depth_format;
        descriptor.label = "Alchemy localized HUD depth clear pipeline";
        depth_clear_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!depth_clear_pipeline)
        {
            return false;
        }

        descriptor.fragment_function = "almetal_ui_depth_restore_fragment";
        descriptor.label = "Alchemy localized HUD depth restore pipeline";
        depth_restore_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!depth_restore_pipeline)
        {
            depth_clear_pipeline.reset();
            return false;
        }

        ALMetalSamplerDescriptor sampler_descriptor;
        sampler_descriptor.min_filter = ALMetalSamplerFilter::nearest;
        sampler_descriptor.mag_filter = ALMetalSamplerFilter::nearest;
        sampler_descriptor.mip_filter = ALMetalSamplerMipFilter::none;
        sampler_descriptor.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
        sampler_descriptor.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
        sampler_descriptor.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
        sampler_descriptor.label = "Alchemy localized HUD depth restore sampler";
        depth_restore_sampler = ALMetalSamplerState::create(
            *device, sampler_descriptor, error);
        if (!depth_restore_sampler)
        {
            depth_clear_pipeline.reset();
            depth_restore_pipeline.reset();
            return false;
        }
        depth_region_format = depth_format;
        return true;
    }

    bool validateDepthRects(const ALMetalScissorRect* rects,
                            std::size_t count,
                            std::string* error) const
    {
        if (!frame_depth)
        {
            setError(error,
                     "Metal UI depth regions require a frame depth backing");
            return false;
        }
        if (count > 0 && !rects)
        {
            setError(error, "Metal UI depth regions are missing rectangles");
            return false;
        }
        const ALMetalTextureDescriptor& descriptor =
            frame_depth->descriptor();
        for (std::size_t index = 0; index < count; ++index)
        {
            const ALMetalScissorRect& rect = rects[index];
            if (rect.width == 0 || rect.height == 0 ||
                rect.x >= descriptor.width || rect.y >= descriptor.height ||
                rect.width > descriptor.width - rect.x ||
                rect.height > descriptor.height - rect.y)
            {
                setError(error,
                         "Metal UI depth region is outside the retained depth target");
                return false;
            }
        }
        return true;
    }

    bool drawDepthRects(const ALMetalScissorRect* rects,
                        std::size_t count,
                        bool restore,
                        float clear_depth,
                        ALMetalCompareFunction compare,
                        std::string* error)
    {
        if (count == 0)
        {
            return true;
        }
        const ALMetalTextureDescriptor& descriptor =
            frame_depth->descriptor();
        if (!beginFrame(descriptor.width, descriptor.height, error) ||
            !prepareDepthRegions(error))
        {
            return false;
        }

        ALMetalUIDepthState write_depth;
        write_depth.enabled = true;
        write_depth.write_enabled = true;
        write_depth.compare = compare;
        ALMetalDepthStencilState* depth = depthState(write_depth, error);
        if (!depth)
        {
            return false;
        }

        ALMetalRenderEncoder& active_encoder = *encoder;
        ALMetalRenderPipeline* active_pipeline = restore
            ? depth_restore_pipeline.get() : depth_clear_pipeline.get();
        const ALMetalViewport viewport{
            0.0, 0.0, static_cast<double>(descriptor.width),
            static_cast<double>(descriptor.height), 0.0, 1.0};
        if (!active_encoder.setPipeline(*active_pipeline, error) ||
            !active_encoder.setDepthStencilState(depth, error) ||
            !active_encoder.setViewport(viewport, error) ||
            !active_encoder.setCullMode(ALMetalCullMode::none, error))
        {
            return false;
        }
        if (restore)
        {
            if (!active_encoder.setFragmentTexture(0, frame_depth, error) ||
                !active_encoder.setFragmentSampler(
                    0, depth_restore_sampler.get(), error))
            {
                return false;
            }
        }
        else if (!active_encoder.setFragmentBytes(
                     0, &clear_depth, sizeof(clear_depth), error))
        {
            return false;
        }

        for (std::size_t index = 0; index < count; ++index)
        {
            if (!active_encoder.setScissor(rects[index], error) ||
                !active_encoder.draw(
                    ALMetalPrimitiveType::triangle, 0, 3, 1, error))
            {
                return false;
            }
        }
        return true;
    }

    ALMetalRenderPipeline* pipeline(ALMetalUIShader shader,
                                    const ALMetalUIBlendState& blend,
                                    ALMetalPixelFormat depth_format,
                                    std::string* error)
    {
        ALMetalUIBlendState target_blend = blend;
        if (surface && target_blend.enabled &&
            target_blend.source_rgb == ALMetalBlendFactor::source_alpha &&
            target_blend.destination_rgb == ALMetalBlendFactor::one_minus_source_alpha &&
            target_blend.source_alpha == ALMetalBlendFactor::source_alpha &&
            target_blend.destination_alpha == ALMetalBlendFactor::one_minus_source_alpha)
        {
            // The transparent CAMetalLayer is composited as premultiplied
            // color. RGB already becomes premultiplied through SrcAlpha, but
            // applying SrcAlpha to alpha itself would square coverage. Preserve
            // the GL color equation while emitting the mathematically matching
            // source-over alpha for Core Animation.
            target_blend.source_alpha = ALMetalBlendFactor::one;
        }
        for (PipelineEntry& entry : pipelines)
        {
            if (entry.shader == shader && sameBlend(entry.blend, target_blend) &&
                entry.depth_format == depth_format)
            {
                return entry.state.get();
            }
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = vertexFunction(shader);
        descriptor.fragment_function = fragmentFunction(shader);
        descriptor.color_attachment_count = 1;
        descriptor.colors[0].format = output_format;
        descriptor.colors[0].blending_enabled = target_blend.enabled;
        descriptor.colors[0].source_rgb = target_blend.source_rgb;
        descriptor.colors[0].destination_rgb = target_blend.destination_rgb;
        descriptor.colors[0].rgb_operation = target_blend.rgb_operation;
        descriptor.colors[0].source_alpha = target_blend.source_alpha;
        descriptor.colors[0].destination_alpha = target_blend.destination_alpha;
        descriptor.colors[0].alpha_operation = target_blend.alpha_operation;
        descriptor.colors[0].write_mask = target_blend.write_mask;
        descriptor.depth_stencil_format = depth_format;
        descriptor.label = "Alchemy production UI pipeline";

        auto state = ALMetalRenderPipeline::create(*device, descriptor, error);
        if (!state)
        {
            return nullptr;
        }
        pipelines.push_back(
            {shader, target_blend, depth_format, std::move(state)});
        totals.pipeline_states = pipelines.size() + hud_pipelines.size();
        return pipelines.back().state.get();
    }

    ALMetalRenderPipeline* hudPipeline(
        ALMetalHUDShader shader,
        const ALMetalUIBlendState& blend,
        ALMetalPixelFormat depth_format,
        std::string* error)
    {
        ALMetalUIBlendState target_blend = blend;
        if (surface && target_blend.enabled &&
            target_blend.source_rgb == ALMetalBlendFactor::source_alpha &&
            target_blend.destination_rgb == ALMetalBlendFactor::one_minus_source_alpha &&
            target_blend.source_alpha == ALMetalBlendFactor::source_alpha &&
            target_blend.destination_alpha == ALMetalBlendFactor::one_minus_source_alpha)
        {
            target_blend.source_alpha = ALMetalBlendFactor::one;
        }
        for (HUDPipelineEntry& entry : hud_pipelines)
        {
            if (entry.shader == shader &&
                sameBlend(entry.blend, target_blend) &&
                entry.depth_format == depth_format)
            {
                return entry.state.get();
            }
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        const bool pbr = shader == ALMetalHUDShader::pbr_alpha;
        descriptor.vertex_function = pbr
            ? "almetal_hud_pbr_alpha_vertex"
            : "almetal_hud_fullbright_vertex";
        descriptor.fragment_function = pbr
            ? "almetal_hud_pbr_alpha_fragment"
            : "almetal_hud_fullbright_fragment";
        descriptor.color_attachment_count = 1;
        descriptor.colors[0].format = output_format;
        descriptor.colors[0].blending_enabled = target_blend.enabled;
        descriptor.colors[0].source_rgb = target_blend.source_rgb;
        descriptor.colors[0].destination_rgb = target_blend.destination_rgb;
        descriptor.colors[0].rgb_operation = target_blend.rgb_operation;
        descriptor.colors[0].source_alpha = target_blend.source_alpha;
        descriptor.colors[0].destination_alpha = target_blend.destination_alpha;
        descriptor.colors[0].alpha_operation = target_blend.alpha_operation;
        descriptor.colors[0].write_mask = target_blend.write_mask;
        descriptor.depth_stencil_format = depth_format;
        switch (shader)
        {
            case ALMetalHUDShader::fullbright_alpha_mask:
                descriptor.label =
                    "Alchemy HUD fullbright alpha-mask pipeline";
                break;
            case ALMetalHUDShader::fullbright_alpha:
                descriptor.label =
                    "Alchemy HUD fullbright alpha pipeline";
                break;
            case ALMetalHUDShader::fullbright:
                descriptor.label = "Alchemy HUD fullbright pipeline";
                break;
            case ALMetalHUDShader::pbr_alpha:
                descriptor.label = "Alchemy HUD PBR alpha pipeline";
                break;
        }

        auto state = ALMetalRenderPipeline::create(*device, descriptor, error);
        if (!state)
        {
            return nullptr;
        }
        hud_pipelines.push_back(
            {shader, target_blend, depth_format, std::move(state)});
        totals.pipeline_states = pipelines.size() + hud_pipelines.size();
        return hud_pipelines.back().state.get();
    }

    ALMetalSamplerState* sampler(const ALMetalSamplerDescriptor& descriptor,
                                 std::string* error)
    {
        for (SamplerEntry& entry : samplers)
        {
            if (sameSampler(entry.descriptor, descriptor))
            {
                return entry.state.get();
            }
        }

        auto state = ALMetalSamplerState::create(*device, descriptor, error);
        if (!state)
        {
            return nullptr;
        }
        samplers.push_back({descriptor, std::move(state)});
        totals.sampler_states = samplers.size();
        return samplers.back().state.get();
    }

    ALMetalDepthStencilState* depthState(
        const ALMetalUIDepthState& descriptor,
        std::string* error)
    {
        for (DepthEntry& entry : depths)
        {
            if (sameDepth(entry.descriptor, descriptor))
            {
                return entry.state.get();
            }
        }

        ALMetalDepthStencilDescriptor native;
        native.depth_compare = descriptor.enabled
            ? descriptor.compare : ALMetalCompareFunction::always;
        native.depth_write_enabled = descriptor.enabled &&
            descriptor.write_enabled;
        native.label = "Alchemy production UI depth";
        auto state = ALMetalDepthStencilState::create(
            *device, native, error);
        if (!state)
        {
            return nullptr;
        }
        depths.push_back({descriptor, std::move(state)});
        return depths.back().state.get();
    }

    bool beginFrame(std::uint32_t width, std::uint32_t height, std::string* error)
    {
        if (frame)
        {
            if (frame_width != width || frame_height != height)
            {
                setError(error, "Metal UI target dimensions changed inside one frame");
                return false;
            }
            return true;
        }

        if (frame_background)
        {
            const ALMetalTextureDescriptor& background =
                frame_background->descriptor();
            if (background.width != width || background.height != height)
            {
                setError(error,
                         "Metal UI frame background dimensions do not match the target");
                return false;
            }
            if (!prepareBackground(error))
            {
                return false;
            }
        }
        if (frame_depth)
        {
            const ALMetalTextureDescriptor& depth =
                frame_depth->descriptor();
            if (depth.width != width || depth.height != height)
            {
                setError(error,
                         "Metal UI frame depth dimensions do not match the target");
                return false;
            }
            if (surface)
            {
                setError(error,
                         "Metal UI frame depth requires an offscreen renderer");
                return false;
            }
        }

        if (surface)
        {
            drawable = surface->nextDrawable(error);
            if (!drawable)
            {
                return false;
            }
            if (drawable->width() != width || drawable->height() != height)
            {
                setError(error, "Metal UI batch dimensions do not match the window drawable");
                drawable.reset();
                return false;
            }
        }
        else if (!target || target->descriptor().width != width ||
                 target->descriptor().height != height ||
                 target->descriptor().depth_stencil_format !=
                     frameDepthFormat())
        {
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = width;
            descriptor.height = height;
            descriptor.color_attachment_count = 1;
            descriptor.colors[0].format = output_format;
            descriptor.colors[0].shader_read = true;
            descriptor.colors[0].label = "Alchemy production UI color";
            descriptor.depth_stencil_format = frameDepthFormat();
            descriptor.depth_stencil_shader_read = frame_depth != nullptr;
            descriptor.label = "Alchemy production UI target";
            target = ALMetalRenderTarget::create(*device, descriptor, error);
            if (!target)
            {
                return false;
            }
        }

        frame = scheduler->beginFrame("Alchemy production UI frame", UINT32_MAX, error);
        if (!frame)
        {
            drawable.reset();
            return false;
        }
        if (frame_depth && !target->copyDepthStencilFrom(
                frame->commandBuffer(), *frame_depth, {}, error))
        {
            frame.reset();
            drawable.reset();
            return false;
        }

        ALMetalRenderPassColorConfig color_config;
        color_config.load = ALMetalLoadAction::clear;
        color_config.store = ALMetalStoreAction::store;
        color_config.clear = {0.0, 0.0, 0.0, 0.0};
        if (surface)
        {
            pass = ALMetalRenderPass::createForColorAttachment(
                *device, drawable->nativeTexture(), ALMetalPixelFormat::bgra8_unorm,
                color_config, error);
        }
        else
        {
            ALMetalRenderPassConfig config;
            config.colors[0] = color_config;
            if (frame_depth)
            {
                config.depth_load = ALMetalLoadAction::load;
                config.depth_store = ALMetalStoreAction::store;
                if (frameDepthFormat() ==
                    ALMetalPixelFormat::depth32_float_stencil8)
                {
                    config.stencil_load = ALMetalLoadAction::load;
                    config.stencil_store = ALMetalStoreAction::store;
                }
            }
            pass = target->makeRenderPass(config, error);
        }
        if (!pass)
        {
            frame.reset();
            drawable.reset();
            return false;
        }
        encoder = ALMetalRenderEncoder::begin(
            frame->commandBuffer(), *pass, "Alchemy production UI encoder", error);
        if (!encoder)
        {
            frame.reset();
            pass.reset();
            drawable.reset();
            return false;
        }
        frame_batches = 0;
        frame_vertices = 0;
        frame_width = width;
        frame_height = height;
        if (frame_background)
        {
            const ALMetalViewport viewport{
                0.0, 0.0, static_cast<double>(width),
                static_cast<double>(height), 0.0, 1.0};
            const ALMetalScissorRect scissor{0, 0, width, height};
            if (!encoder->setPipeline(*background_pipeline, error) ||
                !encoder->setFragmentTexture(0, frame_background, error) ||
                !encoder->setFragmentSampler(
                    0, background_sampler.get(), error) ||
                !encoder->setViewport(viewport, error) ||
                !encoder->setScissor(scissor, error) ||
                !encoder->setCullMode(ALMetalCullMode::none, error) ||
                !encoder->draw(
                    ALMetalPrimitiveType::triangle, 0, 3, 1, error))
            {
                encoder.reset();
                frame.reset();
                pass.reset();
                drawable.reset();
                return false;
            }
        }
        return true;
    }
};

ALMetalUIRenderer::ALMetalUIRenderer(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalUIRenderer::~ALMetalUIRenderer() = default;

std::unique_ptr<ALMetalUIRenderer> ALMetalUIRenderer::create(
    const ALMetalDevice& device,
    ALMetalFrameScheduler& scheduler,
    std::string* error)
{
    return createForTexture(device, scheduler, ALMetalPixelFormat::bgra8_unorm, error);
}

std::unique_ptr<ALMetalUIRenderer> ALMetalUIRenderer::createForTexture(
    const ALMetalDevice& device,
    ALMetalFrameScheduler& scheduler,
    ALMetalPixelFormat format,
    std::string* error)
{
    if (format != ALMetalPixelFormat::rgba8_unorm &&
        format != ALMetalPixelFormat::rgba8_unorm_srgb &&
        format != ALMetalPixelFormat::bgra8_unorm &&
        format != ALMetalPixelFormat::bgra8_unorm_srgb)
    {
        setError(error, "Metal UI offscreen target requires an RGBA8 or BGRA8 format");
        return nullptr;
    }
    auto library = ALMetalShaderLibrary::loadBuiltin(device, error);
    if (!library)
    {
        return nullptr;
    }
    if (!library->hasFunction("almetal_ui_vertex") ||
        !library->hasFunction("almetal_ui_fragment") ||
        !library->hasFunction("almetal_alpha_mask_fragment") ||
        !library->hasFunction("almetal_alpha_mask_solid_fragment") ||
        !library->hasFunction("almetal_solid_vertex") ||
        !library->hasFunction("almetal_solid_fragment") ||
        !library->hasFunction("almetal_resample_vertex") ||
        !library->hasFunction("almetal_resample_fragment") ||
        !library->hasFunction("almetal_ui_depth_clear_fragment") ||
        !library->hasFunction("almetal_ui_depth_restore_fragment") ||
        !library->hasFunction("almetal_hud_fullbright_vertex") ||
        !library->hasFunction("almetal_hud_fullbright_fragment") ||
        !library->hasFunction("almetal_hud_pbr_alpha_vertex") ||
        !library->hasFunction("almetal_hud_pbr_alpha_fragment"))
    {
        setError(error, "Built-in Metal library is missing production UI functions");
        return nullptr;
    }

    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->scheduler = &scheduler;
    impl->output_format = format;
    impl->library = std::move(library);
    return std::unique_ptr<ALMetalUIRenderer>(new ALMetalUIRenderer(std::move(impl)));
}

std::unique_ptr<ALMetalUIRenderer> ALMetalUIRenderer::createForSurface(
    const ALMetalDevice& device,
    ALMetalFrameScheduler& scheduler,
    ALMetalSurface& surface,
    std::string* error)
{
    if (surface.info().format != ALMetalSurfaceFormat::bgra8_unorm)
    {
        setError(error, "Production UI renderer requires a BGRA8-unorm Metal surface");
        return nullptr;
    }
    auto renderer = create(device, scheduler, error);
    if (renderer)
    {
        renderer->mImpl->surface = &surface;
    }
    return renderer;
}

bool ALMetalUIRenderer::setFrameBackground(
    const ALMetalTexture* background,
    std::string* error)
{
    if (!mImpl)
    {
        setError(error, "Metal UI renderer is unavailable");
        return false;
    }
    if (mImpl->frame)
    {
        setError(error,
                 "Metal UI frame background must be selected before drawing");
        return false;
    }
    if (!background)
    {
        mImpl->frame_background = nullptr;
        return true;
    }
    const ALMetalTextureDescriptor& descriptor = background->descriptor();
    if (descriptor.type != ALMetalTextureType::texture_2d ||
        descriptor.format != mImpl->output_format ||
        descriptor.width == 0 || descriptor.height == 0 ||
        descriptor.array_length != 1 || descriptor.sample_count != 1 ||
        !alMetalHasUsage(
            descriptor.usage, ALMetalTextureUsage::shader_read))
    {
        setError(error, "Metal UI frame background is incompatible");
        return false;
    }
    if (mImpl->target && mImpl->target->colorTexture(0) &&
        mImpl->target->colorTexture(0)->nativeHandle() ==
            background->nativeHandle())
    {
        setError(error,
                 "Metal UI frame background cannot alias its destination");
        return false;
    }
    mImpl->frame_background = background;
    return true;
}

bool ALMetalUIRenderer::setFrameDepth(
    const ALMetalTexture* depth,
    std::string* error)
{
    if (!mImpl)
    {
        setError(error, "Metal UI renderer is unavailable");
        return false;
    }
    if (mImpl->frame)
    {
        setError(error,
                 "Metal UI frame depth must be selected before drawing");
        return false;
    }
    if (!depth)
    {
        mImpl->frame_depth = nullptr;
        return true;
    }
    if (mImpl->surface)
    {
        setError(error,
                 "Metal UI frame depth requires an offscreen renderer");
        return false;
    }
    const ALMetalTextureDescriptor& descriptor = depth->descriptor();
    if (descriptor.type != ALMetalTextureType::texture_2d ||
        (descriptor.format != ALMetalPixelFormat::depth32_float &&
         descriptor.format !=
             ALMetalPixelFormat::depth32_float_stencil8) ||
        descriptor.width == 0 || descriptor.height == 0 ||
        descriptor.array_length != 1 || descriptor.sample_count != 1)
    {
        setError(error, "Metal UI frame depth is incompatible");
        return false;
    }
    if (mImpl->target && mImpl->target->depthStencilTexture() &&
        mImpl->target->depthStencilTexture()->nativeHandle() ==
            depth->nativeHandle())
    {
        setError(error,
                 "Metal UI frame depth cannot alias its destination");
        return false;
    }
    mImpl->frame_depth = depth;
    return true;
}

bool ALMetalUIRenderer::clearFrameDepthRects(
    const ALMetalScissorRect* rects,
    std::size_t count,
    float depth,
    std::string* error)
{
    if (!mImpl)
    {
        setError(error, "Metal UI renderer is unavailable");
        return false;
    }
    if (!std::isfinite(depth) || depth < 0.f || depth > 1.f)
    {
        setError(error, "Metal UI clear depth must be finite and normalized");
        return false;
    }
    if (!mImpl->validateDepthRects(rects, count, error))
    {
        return false;
    }
    return mImpl->drawDepthRects(
        rects, count, false, depth, ALMetalCompareFunction::always, error);
}

bool ALMetalUIRenderer::restoreFrameDepthRects(
    const ALMetalScissorRect* rects,
    std::size_t count,
    ALMetalCompareFunction compare,
    std::string* error)
{
    if (!mImpl)
    {
        setError(error, "Metal UI renderer is unavailable");
        return false;
    }
    if (!mImpl->validateDepthRects(rects, count, error))
    {
        return false;
    }
    if (!alMetalHasUsage(
            mImpl->frame_depth->descriptor().usage,
            ALMetalTextureUsage::shader_read))
    {
        setError(error,
                 "Metal UI depth restore requires a shader-readable backing");
        return false;
    }
    return mImpl->drawDepthRects(rects, count, true, 0.f, compare, error);
}

bool ALMetalUIRenderer::draw(const ALMetalUIBatch& batch, std::string* error)
{
    if (!mImpl || !batch.vertices || batch.vertex_count == 0 ||
        batch.target_width == 0 || batch.target_height == 0)
    {
        setError(error, "Metal UI batch is missing geometry or target dimensions");
        return false;
    }
    if (batch.depth.write_enabled && !batch.depth.enabled)
    {
        setError(error, "Metal UI depth writes require depth testing");
        return false;
    }
    if (batch.depth.enabled && !mImpl->frame_depth)
    {
        setError(error,
                 "Metal UI depth test requires a frame depth backing");
        return false;
    }
    const bool uses_texture = shaderUsesTexture(batch.shader);
    if (uses_texture && !batch.texture)
    {
        setError(error, "Metal UI batch is missing its sampled texture");
        return false;
    }
    if (batch.text_shadow_mode > 2)
    {
        setError(error, "Metal UI text shadow mode must be 0, 1 or 2");
        return false;
    }

    PrimitiveDraw draw;
    if (!resolvePrimitive(batch.primitive, batch.vertex_count, draw))
    {
        setError(error, "Metal UI batch has too few vertices for its primitive");
        return false;
    }
    if (!mImpl->beginFrame(batch.target_width, batch.target_height, error))
    {
        return false;
    }

    ALMetalRenderPipeline* pipeline = mImpl->pipeline(
        batch.shader, batch.blend, mImpl->frameDepthFormat(), error);
    ALMetalSamplerState* sampler = uses_texture
        ? mImpl->sampler(batch.sampler, error) : nullptr;
    ALMetalDepthStencilState* depth = mImpl->depthState(
        batch.depth, error);
    if (!pipeline || (uses_texture && !sampler) || !depth)
    {
        return false;
    }

    ALMetalRenderEncoder& encoder = *mImpl->encoder;
    if (!encoder.setPipeline(*pipeline, error) ||
        !encoder.setDepthStencilState(depth, error) ||
        !encoder.setVertexBuffer(0, *batch.vertices, batch.position_offset, error) ||
        !encoder.setVertexBuffer(1, *batch.vertices, batch.texcoord_offset, error))
    {
        return false;
    }
    if (shaderUsesVertexColor(batch.shader) &&
        !encoder.setVertexBuffer(2, *batch.vertices, batch.color_offset, error))
    {
        return false;
    }
    if (!encoder.setVertexBytes(3, &batch.matrices, sizeof(batch.matrices), error))
    {
        return false;
    }

    struct VertexRemapConstants
    {
        std::uint32_t mode;
        std::uint32_t source_count;
        std::uint32_t flip_y;
        std::uint32_t reverse_z;
    } remap{draw.remap_mode, batch.vertex_count, mImpl->surface ? 0u : 1u,
            batch.depth.reverse_z ? 1u : 0u};
    if (!encoder.setVertexBytes(4, &remap, sizeof(remap), error))
    {
        return false;
    }
    if (uses_texture &&
        (!encoder.setFragmentTexture(0, batch.texture, error) ||
         !encoder.setFragmentSampler(0, sampler, error)))
    {
        return false;
    }

    if (batch.shader == ALMetalUIShader::textured)
    {
        if (!encoder.setFragmentBytes(0, &batch.text_shadow_mode,
                                      sizeof(batch.text_shadow_mode), error))
        {
            return false;
        }
    }
    else if (batch.shader == ALMetalUIShader::solid_color)
    {
        if (!encoder.setFragmentBytes(0, batch.solid_color,
                                      sizeof(batch.solid_color), error))
        {
            return false;
        }
    }
    else if (!encoder.setFragmentBytes(0, &batch.minimum_alpha,
                                       sizeof(batch.minimum_alpha), error))
    {
        return false;
    }

    if (!encoder.setViewport(batch.viewport, error) ||
        !encoder.setScissor(batch.scissor, error) ||
        !encoder.setCullMode(ALMetalCullMode::none, error) ||
        !encoder.setFrontFacingWinding(ALMetalWinding::counter_clockwise, error) ||
        !encoder.draw(draw.primitive, 0, draw.vertex_count, 1, error))
    {
        return false;
    }

    ++mImpl->frame_batches;
    mImpl->frame_vertices += draw.vertex_count;
    return true;
}

bool ALMetalUIRenderer::drawHUDIndexed(
    const ALMetalHUDIndexedBatch& batch,
    std::string* error)
{
    if (!mImpl || !batch.vertices || batch.vertex_count == 0 ||
        !batch.indices || batch.index_count == 0 ||
        batch.target_width == 0 || batch.target_height == 0)
    {
        setError(error,
                 "Metal HUD batch is missing geometry or target dimensions");
        return false;
    }
    if (batch.texture_count == 0 ||
        batch.texture_count > ALMetalHUDIndexedBatch::max_textures)
    {
        setError(error, "Metal HUD batch has an invalid texture count");
        return false;
    }
    const std::size_t index_stride =
        batch.index_type == ALMetalIndexType::uint32 ? 4u : 2u;
    if (batch.position_offset % (sizeof(float) * 4) != 0 ||
        batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
        batch.color_offset % (sizeof(std::uint8_t) * 4) != 0 ||
        batch.index_offset % index_stride != 0 ||
        !rangeFits(batch.position_offset, batch.vertex_count,
                   sizeof(float) * 4, batch.vertices->length()) ||
        !rangeFits(batch.texcoord_offset, batch.vertex_count,
                   sizeof(float) * 2, batch.vertices->length()) ||
        !rangeFits(batch.color_offset, batch.vertex_count,
                   sizeof(std::uint8_t) * 4, batch.vertices->length()) ||
        !rangeFits(batch.index_offset, batch.index_count,
                   index_stride, batch.indices->length()))
    {
        setError(error, "Metal HUD geometry range exceeds its buffers");
        return false;
    }
    if (!finiteFloats(batch.matrices.modelview_projection, 16) ||
        !finiteFloats(batch.matrices.texture0, 16))
    {
        setError(error, "Metal HUD matrices must be finite");
        return false;
    }
    for (std::size_t texture = 0; texture < batch.texture_count; ++texture)
    {
        if (!batch.textures[texture])
        {
            setError(error, "Metal HUD batch is missing a diffuse texture");
            return false;
        }
    }
    const bool pbr = batch.shader == ALMetalHUDShader::pbr_alpha;
    if (pbr &&
        (batch.texture_count != 1 || !batch.emissive_texture ||
         !finiteFloats(batch.pbr_base_color_transform, 8) ||
         !finiteFloats(batch.pbr_emissive_transform, 8) ||
         !finiteFloats(batch.pbr_emissive_color, 4)))
    {
        setError(error, "Metal HUD PBR material state is invalid");
        return false;
    }
    if (batch.depth.write_enabled && !batch.depth.enabled)
    {
        setError(error, "Metal HUD depth writes require depth testing");
        return false;
    }
    if (batch.depth.enabled && !mImpl->frame_depth)
    {
        setError(error,
                 "Metal HUD depth test requires a frame depth backing");
        return false;
    }
    if (!std::isfinite(batch.minimum_alpha) ||
        batch.minimum_alpha < 0.f || batch.minimum_alpha > 1.f)
    {
        setError(error, "Metal HUD alpha cutoff must be finite and normalized");
        return false;
    }
    if (!mImpl->beginFrame(batch.target_width, batch.target_height, error))
    {
        return false;
    }

    ALMetalRenderPipeline* pipeline = mImpl->hudPipeline(
        batch.shader, batch.blend, mImpl->frameDepthFormat(), error);
    ALMetalSamplerState* sampler = mImpl->sampler(batch.sampler, error);
    ALMetalDepthStencilState* depth = mImpl->depthState(batch.depth, error);
    if (!pipeline || !sampler || !depth)
    {
        return false;
    }

    struct alignas(16) HUDConstants
    {
        float modelview_projection[16];
        float texture_matrix[16];
        float material[4]{};
        std::uint32_t config[4]{};
        float pbr_base_color_transform[8]{};
        float pbr_emissive_transform[8]{};
        float pbr_emissive_color[4]{};
    } constants;
    static_assert(sizeof(HUDConstants) == 240);
    std::memcpy(constants.modelview_projection,
                batch.matrices.modelview_projection,
                sizeof(constants.modelview_projection));
    std::memcpy(constants.texture_matrix, batch.matrices.texture0,
                sizeof(constants.texture_matrix));
    constants.material[0] = batch.minimum_alpha;
    constants.config[0] = batch.depth.reverse_z ? 1u : 0u;
    constants.config[1] = batch.texture_count;
    constants.config[2] = mImpl->surface ? 0u : 1u;
    constants.config[3] =
        batch.shader == ALMetalHUDShader::fullbright_alpha_mask ? 1u :
        batch.shader == ALMetalHUDShader::fullbright_alpha ? 2u : 0u;
    std::memcpy(constants.pbr_base_color_transform,
                batch.pbr_base_color_transform,
                sizeof(constants.pbr_base_color_transform));
    std::memcpy(constants.pbr_emissive_transform,
                batch.pbr_emissive_transform,
                sizeof(constants.pbr_emissive_transform));
    std::memcpy(constants.pbr_emissive_color,
                batch.pbr_emissive_color,
                sizeof(constants.pbr_emissive_color));

    ALMetalRenderEncoder& encoder = *mImpl->encoder;
    bool encoded =
        encoder.setPipeline(*pipeline, error) &&
        encoder.setDepthStencilState(depth, error) &&
        encoder.setVertexBuffer(
            0, *batch.vertices, batch.position_offset, error) &&
        encoder.setVertexBuffer(
            1, *batch.vertices, batch.texcoord_offset, error) &&
        encoder.setVertexBuffer(
            2, *batch.vertices, batch.color_offset, error) &&
        encoder.setVertexBytes(
            3, &constants, sizeof(constants), error) &&
        encoder.setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder.setFragmentSampler(0, sampler, error);
    if (encoded && pbr)
    {
        // glTF base/emissive colour uses the texture's sRGB view so fixed
        // function filtering occurs in linear light, matching GLTF_COLOR_SAMPLER.
        encoded = encoder.setFragmentTexture(0, batch.textures[0], error) &&
                  encoder.setFragmentTexture(
                      1, batch.emissive_texture, error);
    }
    for (std::size_t texture = 0;
         encoded && !pbr && texture < batch.texture_count; ++texture)
    {
        // The matching HUD GLSL program binds these assets with sRGB decode
        // disabled. Use the compatible raw-storage view for both uncompressed
        // and BC-compressed sRGB mirrors so their display-encoded multiply is
        // not silently linearized by the Metal texture format.
        encoded = encoder.setFragmentTextureLinear(
            static_cast<std::uint32_t>(texture),
            batch.textures[texture], error);
    }
    if (!encoded ||
        !encoder.setViewport(batch.viewport, error) ||
        !encoder.setScissor(batch.scissor, error) ||
        !encoder.setCullMode(
            batch.double_sided ? ALMetalCullMode::none
                               : ALMetalCullMode::back,
            error) ||
        !encoder.setFrontFacingWinding(
            mImpl->surface ? ALMetalWinding::counter_clockwise
                           : ALMetalWinding::clockwise,
            error) ||
        !encoder.drawIndexed(
            ALMetalPrimitiveType::triangle, batch.index_count,
            batch.index_type, *batch.indices, batch.index_offset, 1, error))
    {
        return false;
    }

    ++mImpl->frame_batches;
    mImpl->frame_vertices += batch.index_count;
    return true;
}

bool ALMetalUIRenderer::endFrame(std::string* error)
{
    if (!mImpl)
    {
        return true;
    }
    if (!mImpl->frame && mImpl->frame_background)
    {
        const ALMetalTextureDescriptor& source =
            mImpl->frame_background->descriptor();
        if (!mImpl->beginFrame(
                source.width, source.height, error))
        {
            mImpl->frame_background = nullptr;
            mImpl->frame_depth = nullptr;
            return false;
        }
    }
    if (!mImpl->frame && mImpl->frame_depth)
    {
        mImpl->frame_depth = nullptr;
        return true;
    }
    else if (!mImpl->frame && mImpl->surface)
    {
        const ALMetalSurfaceInfo& info = mImpl->surface->info();
        if (!mImpl->beginFrame(info.width, info.height, error))
        {
            mImpl->frame_background = nullptr;
            mImpl->frame_depth = nullptr;
            return false;
        }
    }
    if (!mImpl->frame)
    {
        return true;
    }
    if (!mImpl->encoder || !mImpl->encoder->end(error))
    {
        mImpl->encoder.reset();
        mImpl->frame.reset();
        mImpl->pass.reset();
        mImpl->drawable.reset();
        mImpl->frame_background = nullptr;
        mImpl->frame_depth = nullptr;
        return false;
    }
    mImpl->encoder.reset();
    if (mImpl->surface &&
        (!mImpl->drawable ||
         !mImpl->surface->present(mImpl->frame->commandBuffer(), *mImpl->drawable, error)))
    {
        mImpl->frame.reset();
        mImpl->pass.reset();
        mImpl->drawable.reset();
        mImpl->frame_background = nullptr;
        mImpl->frame_depth = nullptr;
        return false;
    }
    if (!mImpl->scheduler->submit(*mImpl->frame, error))
    {
        mImpl->frame.reset();
        mImpl->pass.reset();
        mImpl->drawable.reset();
        mImpl->frame_background = nullptr;
        mImpl->frame_depth = nullptr;
        return false;
    }
    mImpl->frame.reset();
    mImpl->pass.reset();
    mImpl->drawable.reset();
    mImpl->frame_background = nullptr;
    mImpl->frame_depth = nullptr;
    ++mImpl->totals.frames;
    mImpl->totals.batches += mImpl->frame_batches;
    mImpl->totals.vertices += mImpl->frame_vertices;
    return true;
}

bool ALMetalUIRenderer::hasOpenFrame() const noexcept
{
    return mImpl && mImpl->frame != nullptr;
}

const ALMetalTexture* ALMetalUIRenderer::outputTexture() const noexcept
{
    return mImpl && mImpl->target ? mImpl->target->colorTexture(0) : nullptr;
}

const ALMetalTexture* ALMetalUIRenderer::outputDepthTexture() const noexcept
{
    return mImpl && mImpl->target
        ? mImpl->target->depthStencilTexture() : nullptr;
}

std::unique_ptr<ALMetalTexture> ALMetalUIRenderer::takeOutputTexture() noexcept
{
    if (!mImpl || mImpl->surface || mImpl->frame || !mImpl->target)
    {
        return nullptr;
    }
    auto output = mImpl->target->takeColorTexture(0);
    mImpl->target.reset();
    return output;
}

ALMetalUIRendererStats ALMetalUIRenderer::stats() const noexcept
{
    return mImpl ? mImpl->totals : ALMetalUIRendererStats{};
}

