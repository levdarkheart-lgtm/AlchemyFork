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
               shader == ALMetalUIShader::vertex_color ||
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
        return shader == ALMetalUIShader::solid_color ||
                       shader == ALMetalUIShader::uniform_color
            ? "almetal_solid_vertex" : "almetal_ui_vertex";
    }

    const char* fragmentFunction(ALMetalUIShader shader)
    {
        switch (shader)
        {
            case ALMetalUIShader::textured:            return "almetal_ui_fragment";
            case ALMetalUIShader::vertex_color:        return "almetal_ui_vertex_color_fragment";
            case ALMetalUIShader::uniform_color:       return "almetal_uniform_color_fragment";
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
    std::unique_ptr<ALMetalRenderPipeline> selection_id_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> selection_id_skinned_pipeline;
    std::unique_ptr<ALMetalSamplerState> selection_depth_sampler;
    std::unique_ptr<ALMetalDepthStencilState> selection_forward_depth;
    std::unique_ptr<ALMetalDepthStencilState> selection_reverse_depth;
    std::unique_ptr<ALMetalRenderTarget> selection_target;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 4>
        selection_wire_pipelines;
    ALMetalPixelFormat selection_wire_depth_format =
        ALMetalPixelFormat::invalid;
    std::unique_ptr<ALMetalRenderPipeline> selection_jump_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> selection_tile_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> selection_glow_reach_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> selection_glow_row_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> selection_backdrop_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> selection_contour_pipeline;
    ALMetalPixelFormat selection_contour_depth_format =
        ALMetalPixelFormat::invalid;
    std::unique_ptr<ALMetalRenderTarget> selection_jump_target;
    std::unique_ptr<ALMetalRenderTarget> selection_tile_target;
    std::unique_ptr<ALMetalRenderTarget> selection_glow_reach_target;
    std::unique_ptr<ALMetalRenderTarget> selection_glow_row_target;
    std::unique_ptr<ALMetalRenderTarget> selection_backdrop_target;
    std::unique_ptr<ALMetalTexture> selection_palette;
    std::vector<std::unique_ptr<ALMetalBuffer>> selection_frame_palettes;
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

    void refreshPipelineCount() noexcept
    {
        totals.pipeline_states = pipelines.size() + hud_pipelines.size() +
            (selection_id_pipeline ? 1u : 0u) +
            (selection_id_skinned_pipeline ? 1u : 0u) +
            std::count_if(
                selection_wire_pipelines.begin(),
                selection_wire_pipelines.end(),
                [](const auto& pipeline) { return pipeline != nullptr; }) +
            (selection_jump_pipeline ? 1u : 0u) +
            (selection_tile_pipeline ? 1u : 0u) +
            (selection_glow_reach_pipeline ? 1u : 0u) +
            (selection_glow_row_pipeline ? 1u : 0u) +
            (selection_backdrop_pipeline ? 1u : 0u) +
            (selection_contour_pipeline ? 1u : 0u);
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
        refreshPipelineCount();
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
        refreshPipelineCount();
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

    bool prepareSelectionIDs(std::string* error)
    {
        if (selection_id_pipeline && selection_id_skinned_pipeline &&
            selection_depth_sampler && selection_forward_depth &&
            selection_reverse_depth)
        {
            return true;
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "almetal_selection_id_vertex";
        descriptor.fragment_function = "almetal_selection_id_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        descriptor.label = "Alchemy selection id pipeline";
        selection_id_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!selection_id_pipeline)
        {
            return false;
        }
        descriptor.vertex_function = "almetal_selection_id_skinned_vertex";
        descriptor.label = "Alchemy skinned selection id pipeline";
        selection_id_skinned_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!selection_id_skinned_pipeline)
        {
            selection_id_pipeline.reset();
            return false;
        }

        ALMetalSamplerDescriptor sampler_descriptor;
        sampler_descriptor.min_filter = ALMetalSamplerFilter::nearest;
        sampler_descriptor.mag_filter = ALMetalSamplerFilter::nearest;
        sampler_descriptor.mip_filter = ALMetalSamplerMipFilter::none;
        sampler_descriptor.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
        sampler_descriptor.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
        sampler_descriptor.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
        sampler_descriptor.label = "Alchemy selection scene-depth sampler";
        selection_depth_sampler = ALMetalSamplerState::create(
            *device, sampler_descriptor, error);
        if (!selection_depth_sampler)
        {
            selection_id_pipeline.reset();
            selection_id_skinned_pipeline.reset();
            return false;
        }

        ALMetalDepthStencilDescriptor depth_descriptor;
        depth_descriptor.depth_compare = ALMetalCompareFunction::less_equal;
        depth_descriptor.depth_write_enabled = true;
        depth_descriptor.label = "Alchemy forward selection id depth";
        selection_forward_depth = ALMetalDepthStencilState::create(
            *device, depth_descriptor, error);
        if (!selection_forward_depth)
        {
            return false;
        }
        depth_descriptor.depth_compare = ALMetalCompareFunction::greater_equal;
        depth_descriptor.label = "Alchemy reverse selection id depth";
        selection_reverse_depth = ALMetalDepthStencilState::create(
            *device, depth_descriptor, error);
        if (!selection_reverse_depth)
        {
            return false;
        }
        refreshPipelineCount();
        return true;
    }

    bool prepareSelectionTarget(std::uint32_t width, std::uint32_t height,
                                std::string* error)
    {
        if (selection_target &&
            selection_target->descriptor().width == width &&
            selection_target->descriptor().height == height)
        {
            return true;
        }
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = width;
        descriptor.height = height;
        descriptor.color_attachment_count = 1;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.colors[0].shader_read = true;
        descriptor.colors[0].label = "Alchemy selection id bytes";
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        descriptor.depth_stencil_shader_read = true;
        descriptor.label = "Alchemy selection id/depth target";
        selection_target = ALMetalRenderTarget::create(
            *device, descriptor, error);
        return selection_target != nullptr;
    }

    bool prepareSelectionWireframe(std::string* error)
    {
        const ALMetalPixelFormat depth_format = frameDepthFormat();
        if (std::all_of(
                selection_wire_pipelines.begin(),
                selection_wire_pipelines.end(),
                [](const auto& pipeline) { return pipeline != nullptr; }) &&
            selection_wire_depth_format == depth_format)
        {
            return true;
        }

        for (auto& pipeline : selection_wire_pipelines)
        {
            pipeline.reset();
        }
        static constexpr const char* vertex_functions[4] = {
            "almetal_selection_wire_u16_vertex",
            "almetal_selection_wire_u32_vertex",
            "almetal_selection_wire_skinned_u16_vertex",
            "almetal_selection_wire_skinned_u32_vertex",
        };
        static constexpr const char* labels[4] = {
            "Alchemy selection wire U16 pipeline",
            "Alchemy selection wire U32 pipeline",
            "Alchemy skinned selection wire U16 pipeline",
            "Alchemy skinned selection wire U32 pipeline",
        };
        for (std::size_t index = 0;
             index < selection_wire_pipelines.size(); ++index)
        {
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = library.get();
            descriptor.vertex_function = vertex_functions[index];
            descriptor.fragment_function =
                "almetal_selection_wire_fragment";
            descriptor.colors[0].format = output_format;
            descriptor.colors[0].blending_enabled = true;
            descriptor.colors[0].source_rgb =
                ALMetalBlendFactor::source_alpha;
            descriptor.colors[0].destination_rgb =
                ALMetalBlendFactor::one_minus_source_alpha;
            descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
            descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::one_minus_source_alpha;
            descriptor.colors[0].write_mask =
                static_cast<ALMetalColorWriteMask>(
                    static_cast<std::uint8_t>(
                        ALMetalColorWriteMask::red) |
                    static_cast<std::uint8_t>(
                        ALMetalColorWriteMask::green) |
                    static_cast<std::uint8_t>(
                        ALMetalColorWriteMask::blue));
            descriptor.depth_stencil_format = depth_format;
            descriptor.label = labels[index];
            selection_wire_pipelines[index] =
                ALMetalRenderPipeline::create(
                    *device, descriptor, error);
            if (!selection_wire_pipelines[index])
            {
                for (auto& pipeline : selection_wire_pipelines)
                {
                    pipeline.reset();
                }
                selection_wire_depth_format =
                    ALMetalPixelFormat::invalid;
                return false;
            }
        }
        selection_wire_depth_format = depth_format;
        refreshPipelineCount();
        return true;
    }

    bool prepareSelectionContour(std::string* error)
    {
        const ALMetalPixelFormat depth_format = frameDepthFormat();
        if (selection_jump_pipeline && selection_tile_pipeline &&
            selection_contour_pipeline &&
            selection_contour_depth_format == depth_format)
        {
            return true;
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function = "almetal_selection_jump_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::r8_unorm;
        descriptor.label = "Alchemy selection jump pipeline";
        selection_jump_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!selection_jump_pipeline)
        {
            return false;
        }

        descriptor.fragment_function = "almetal_selection_tile_fragment";
        descriptor.color_attachment_count = 2;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba16_unorm;
        descriptor.colors[1].format = ALMetalPixelFormat::r32_float;
        descriptor.label = "Alchemy selection tile pipeline";
        selection_tile_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!selection_tile_pipeline)
        {
            selection_jump_pipeline.reset();
            return false;
        }

        descriptor.fragment_function = "almetal_selection_contour_fragment";
        descriptor.color_attachment_count = 1;
        descriptor.colors[0] = {};
        descriptor.colors[0].format = output_format;
        descriptor.colors[0].blending_enabled = true;
        descriptor.colors[0].source_rgb =
            ALMetalBlendFactor::source_alpha;
        descriptor.colors[0].destination_rgb =
            ALMetalBlendFactor::one_minus_source_alpha;
        descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
        descriptor.colors[0].destination_alpha =
            ALMetalBlendFactor::one_minus_source_alpha;
        descriptor.colors[0].write_mask = static_cast<ALMetalColorWriteMask>(
            static_cast<std::uint8_t>(ALMetalColorWriteMask::red) |
            static_cast<std::uint8_t>(ALMetalColorWriteMask::green) |
            static_cast<std::uint8_t>(ALMetalColorWriteMask::blue));
        descriptor.depth_stencil_format = depth_format;
        descriptor.label = "Alchemy selection contour pipeline";
        selection_contour_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!selection_contour_pipeline)
        {
            selection_jump_pipeline.reset();
            selection_tile_pipeline.reset();
            return false;
        }
        selection_contour_depth_format = depth_format;
        refreshPipelineCount();
        return true;
    }

    bool prepareSelectionGlow(std::string* error)
    {
        if (selection_glow_reach_pipeline && selection_glow_row_pipeline)
        {
            return true;
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function =
            "almetal_selection_glow_reach_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::rgba16_unorm;
        descriptor.label = "Alchemy selection glow reach pipeline";
        selection_glow_reach_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!selection_glow_reach_pipeline)
        {
            return false;
        }

        descriptor.fragment_function =
            "almetal_selection_glow_row_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::r8_unorm;
        descriptor.label = "Alchemy selection glow row pipeline";
        selection_glow_row_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!selection_glow_row_pipeline)
        {
            selection_glow_reach_pipeline.reset();
            return false;
        }
        refreshPipelineCount();
        return true;
    }

    bool prepareSelectionBackdrop(std::string* error)
    {
        if (selection_backdrop_pipeline)
        {
            return true;
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function =
            "almetal_selection_backdrop_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        descriptor.label = "Alchemy selection contrast backdrop pipeline";
        selection_backdrop_pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!selection_backdrop_pipeline)
        {
            return false;
        }
        refreshPipelineCount();
        return true;
    }

    bool prepareSelectionContourTargets(
        std::uint32_t width, std::uint32_t height,
        std::uint32_t palette_width, std::uint32_t palette_height,
        bool edges, bool glow, bool contrast,
        std::string* error)
    {
        if (edges && (!selection_jump_target ||
            selection_jump_target->descriptor().width != width ||
            selection_jump_target->descriptor().height != height))
        {
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = width;
            descriptor.height = height;
            descriptor.color_attachment_count = 1;
            descriptor.colors[0].format = ALMetalPixelFormat::r8_unorm;
            descriptor.colors[0].shader_read = true;
            descriptor.colors[0].label = "Alchemy selection jump marks";
            descriptor.label = "Alchemy selection jump target";
            selection_jump_target = ALMetalRenderTarget::create(
                *device, descriptor, error);
            if (!selection_jump_target)
            {
                return false;
            }
        }

        const std::uint32_t tile_width = (width + 7u) / 8u;
        const std::uint32_t tile_height = (height + 7u) / 8u;
        if (edges && (!selection_tile_target ||
            selection_tile_target->descriptor().width != tile_width ||
            selection_tile_target->descriptor().height != tile_height))
        {
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = tile_width;
            descriptor.height = tile_height;
            descriptor.color_attachment_count = 2;
            descriptor.colors[0].format = ALMetalPixelFormat::rgba16_unorm;
            descriptor.colors[0].shader_read = true;
            descriptor.colors[0].label = "Alchemy selection tile ids";
            descriptor.colors[1].format = ALMetalPixelFormat::r32_float;
            descriptor.colors[1].shader_read = true;
            descriptor.colors[1].label = "Alchemy selection tile depth";
            descriptor.label = "Alchemy selection tile target";
            selection_tile_target = ALMetalRenderTarget::create(
                *device, descriptor, error);
            if (!selection_tile_target)
            {
                return false;
            }
        }

        if (!selection_palette ||
            selection_palette->descriptor().width != palette_width ||
            selection_palette->descriptor().height != palette_height)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.format = ALMetalPixelFormat::rgba8_unorm;
            descriptor.width = palette_width;
            descriptor.height = palette_height;
            descriptor.storage = ALMetalStorageMode::private_gpu;
            descriptor.usage = ALMetalTextureUsage::shader_read;
            descriptor.label = "Alchemy selection palette";
            selection_palette = ALMetalTexture::create(
                *device, descriptor, error);
            if (!selection_palette)
            {
                return false;
            }
        }

        if (glow &&
            (!selection_glow_reach_target ||
             selection_glow_reach_target->descriptor().width != tile_width ||
             selection_glow_reach_target->descriptor().height != tile_height))
        {
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = tile_width;
            descriptor.height = tile_height;
            descriptor.color_attachment_count = 1;
            descriptor.colors[0].format =
                ALMetalPixelFormat::rgba16_unorm;
            descriptor.colors[0].shader_read = true;
            descriptor.colors[0].label =
                "Alchemy selection glow tile reach";
            descriptor.label = "Alchemy selection glow reach target";
            selection_glow_reach_target = ALMetalRenderTarget::create(
                *device, descriptor, error);
            if (!selection_glow_reach_target)
            {
                return false;
            }
        }
        if (glow &&
            (!selection_glow_row_target ||
             selection_glow_row_target->descriptor().width != width ||
             selection_glow_row_target->descriptor().height != height))
        {
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = width;
            descriptor.height = height;
            descriptor.color_attachment_count = 1;
            descriptor.colors[0].format = ALMetalPixelFormat::r8_unorm;
            descriptor.colors[0].shader_read = true;
            descriptor.colors[0].label =
                "Alchemy selection glow row distance";
            descriptor.label = "Alchemy selection glow row target";
            selection_glow_row_target = ALMetalRenderTarget::create(
                *device, descriptor, error);
            if (!selection_glow_row_target)
            {
                return false;
            }
        }
        const std::uint32_t backdrop_width = std::max(width / 2u, 1u);
        const std::uint32_t backdrop_height = std::max(height / 2u, 1u);
        if (contrast &&
            (!selection_backdrop_target ||
             selection_backdrop_target->descriptor().width != backdrop_width ||
             selection_backdrop_target->descriptor().height != backdrop_height))
        {
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = backdrop_width;
            descriptor.height = backdrop_height;
            descriptor.color_attachment_count = 1;
            descriptor.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
            descriptor.colors[0].shader_read = true;
            descriptor.colors[0].label =
                "Alchemy selection contrast backdrop";
            descriptor.label = "Alchemy selection contrast backdrop target";
            selection_backdrop_target = ALMetalRenderTarget::create(
                *device, descriptor, error);
            if (!selection_backdrop_target)
            {
                return false;
            }
        }
        return true;
    }

    bool resumeMainPass(std::string* error)
    {
        if (!frame || !target || surface)
        {
            setError(error,
                     "Metal selection pass cannot resume a non-offscreen UI frame");
            return false;
        }
        ALMetalRenderPassConfig config;
        config.colors[0].load = ALMetalLoadAction::load;
        config.colors[0].store = ALMetalStoreAction::store;
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
        if (!pass)
        {
            return false;
        }
        encoder = ALMetalRenderEncoder::begin(
            frame->commandBuffer(), *pass,
            "Alchemy resumed production UI encoder", error);
        return encoder != nullptr;
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
        !library->hasFunction("almetal_ui_vertex_color_fragment") ||
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
        !library->hasFunction("almetal_hud_pbr_alpha_fragment") ||
        !library->hasFunction("almetal_selection_id_vertex") ||
        !library->hasFunction("almetal_selection_id_skinned_vertex") ||
        !library->hasFunction("almetal_selection_id_fragment") ||
        !library->hasFunction("almetal_selection_jump_fragment") ||
        !library->hasFunction("almetal_selection_tile_fragment") ||
        !library->hasFunction("almetal_selection_glow_reach_fragment") ||
        !library->hasFunction("almetal_selection_glow_row_fragment") ||
        !library->hasFunction("almetal_selection_backdrop_fragment") ||
        !library->hasFunction("almetal_selection_contour_fragment"))
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
    else if (batch.shader == ALMetalUIShader::vertex_color)
    {
        // The fragment consumes only the interpolated vertex colour.
    }
    else if (batch.shader == ALMetalUIShader::solid_color ||
             batch.shader == ALMetalUIShader::uniform_color)
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

bool ALMetalUIRenderer::drawSelectionIDs(
    const ALMetalSelectionIDScene& scene,
    std::string* error)
{
    if (!mImpl || mImpl->surface || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0)
    {
        setError(error,
                 "Metal selection ids require an offscreen scene with batches");
        return false;
    }
    if (!mImpl->frame_background || !mImpl->frame_depth)
    {
        setError(error,
                 "Metal selection ids require retained world color and depth");
        return false;
    }
    const ALMetalTextureDescriptor& background =
        mImpl->frame_background->descriptor();
    const ALMetalTextureDescriptor& retained_depth =
        mImpl->frame_depth->descriptor();
    if (background.width != scene.width ||
        background.height != scene.height ||
        retained_depth.width != scene.width ||
        retained_depth.height != scene.height ||
        !alMetalHasUsage(retained_depth.usage,
                         ALMetalTextureUsage::shader_read))
    {
        setError(error,
                 "Metal selection backings do not match the id target");
        return false;
    }
    if (!finiteFloats(scene.inverse_projection, 16))
    {
        setError(error,
                 "Metal selection inverse projection must be finite");
        return false;
    }

    std::vector<std::unique_ptr<ALMetalBuffer>> palettes;
    std::vector<const ALMetalBuffer*> palette_by_batch(
        scene.batch_count, nullptr);
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalSelectionIDBatch& batch = scene.batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4u : 2u;
        const bool skinned = batch.skin_palette != nullptr ||
                             batch.skin_palette_float_count != 0 ||
                             batch.skin_joint_count != 0;
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count < 3 || batch.index_count % 3 != 0 ||
            batch.id == 0 || batch.id > 65535 || batch.priority > 255 ||
            batch.position_offset % (sizeof(float) * 4) != 0 ||
            batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
            batch.index_offset % index_stride != 0 ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       sizeof(float) * 4, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       sizeof(float) * 2, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()) ||
            !finiteFloats(batch.modelview_projection, 16) ||
            !finiteFloats(batch.texture_matrix, 16) ||
            !finiteFloats(batch.base_color_transform, 8) ||
            !std::isfinite(batch.alpha_cutoff) ||
            batch.alpha_cutoff < -1.f || batch.alpha_cutoff > 2.f)
        {
            setError(error, "Metal selection id batch is invalid");
            return false;
        }
        if (batch.alpha_cutoff > 0.f && batch.alpha_cutoff <= 1.f &&
            (!batch.texture ||
             !alMetalHasUsage(batch.texture->descriptor().usage,
                              ALMetalTextureUsage::shader_read)))
        {
            setError(error,
                     "Metal selection alpha test is missing its texture");
            return false;
        }
        if (!skinned)
        {
            continue;
        }
        const std::size_t expected_palette_floats =
            4u + static_cast<std::size_t>(batch.skin_joint_count) * 12u;
        if (!batch.skin_palette || batch.skin_joint_count == 0 ||
            batch.skin_palette_float_count != expected_palette_floats ||
            batch.weight_offset % (sizeof(float) * 4) != 0 ||
            !rangeFits(batch.weight_offset, batch.vertex_count,
                       sizeof(float) * 4, batch.vertices->length()) ||
            !finiteFloats(batch.skin_palette,
                          batch.skin_palette_float_count))
        {
            setError(error,
                     "Metal selection skinning data is invalid");
            return false;
        }
        ALMetalBufferDescriptor descriptor;
        descriptor.length = batch.skin_palette_float_count * sizeof(float);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy selection skin palette";
        auto palette = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!palette || !palette->write(
                0, batch.skin_palette, descriptor.length, error))
        {
            return false;
        }
        palette_by_batch[index] = palette.get();
        palettes.push_back(std::move(palette));
    }

    if (!mImpl->prepareSelectionIDs(error) ||
        !mImpl->prepareSelectionTarget(scene.width, scene.height, error) ||
        !mImpl->beginFrame(scene.width, scene.height, error))
    {
        return false;
    }
    for (auto& palette : palettes)
    {
        mImpl->selection_frame_palettes.push_back(std::move(palette));
    }
    if (!mImpl->encoder || !mImpl->encoder->end(error))
    {
        return false;
    }
    mImpl->encoder.reset();
    mImpl->pass.reset();

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
    pass_config.depth_load = ALMetalLoadAction::clear;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.clear_depth = scene.reverse_z ? 0.0 : 1.0;
    auto selection_pass = mImpl->selection_target->makeRenderPass(
        pass_config, error);
    auto selection_encoder = selection_pass
        ? ALMetalRenderEncoder::begin(
              mImpl->frame->commandBuffer(), *selection_pass,
              "Alchemy selection id encoder", error)
        : nullptr;
    bool encoded = selection_encoder != nullptr;
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0};
    const ALMetalScissorRect scissor{
        0, 0, scene.width, scene.height};

    struct alignas(16) SelectionConstants
    {
        float modelview_projection[16];
        float texture_matrix[16];
        float inverse_projection[16];
        float base_color_transform[8];
        float screen_material[4];
        std::uint32_t config[4];
    } constants;
    static_assert(sizeof(SelectionConstants) == 256);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalSelectionIDBatch& batch = scene.batches[index];
        const bool skinned = palette_by_batch[index] != nullptr;
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.texture_matrix, batch.texture_matrix,
                    sizeof(constants.texture_matrix));
        std::memcpy(constants.inverse_projection,
                    scene.inverse_projection,
                    sizeof(constants.inverse_projection));
        std::memcpy(constants.base_color_transform,
                    batch.base_color_transform,
                    sizeof(constants.base_color_transform));
        constants.screen_material[0] = static_cast<float>(scene.width);
        constants.screen_material[1] = static_cast<float>(scene.height);
        constants.screen_material[2] = batch.alpha_cutoff;
        constants.screen_material[3] = 0.001f;
        constants.config[0] = batch.id;
        constants.config[1] = batch.priority;
        constants.config[2] =
            (scene.reverse_z ? 1u : 0u) |
            (scene.use_scene_depth ? 2u : 0u) |
            (batch.show_hidden ? 4u : 0u) |
            (batch.outlined ? 8u : 0u) |
            (batch.keep_cut ? 16u : 0u);
        constants.config[3] = batch.skin_joint_count;

        ALMetalSamplerState* diffuse_sampler =
            mImpl->sampler(batch.sampler, error);
        encoded = diffuse_sampler != nullptr &&
            selection_encoder->setPipeline(
                skinned ? *mImpl->selection_id_skinned_pipeline
                        : *mImpl->selection_id_pipeline,
                error) &&
            selection_encoder->setDepthStencilState(
                scene.reverse_z ? mImpl->selection_reverse_depth.get()
                                : mImpl->selection_forward_depth.get(),
                error) &&
            selection_encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            selection_encoder->setVertexBuffer(
                1, *batch.vertices, batch.texcoord_offset, error) &&
            selection_encoder->setVertexBytes(
                3, &constants, sizeof(constants), error) &&
            selection_encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            selection_encoder->setFragmentTexture(
                0, batch.texture ? batch.texture
                                 : mImpl->frame_background,
                error) &&
            selection_encoder->setFragmentTexture(
                1, mImpl->frame_depth, error) &&
            selection_encoder->setFragmentSampler(
                0, diffuse_sampler, error) &&
            selection_encoder->setFragmentSampler(
                1, mImpl->selection_depth_sampler.get(), error);
        if (encoded && skinned)
        {
            encoded = selection_encoder->setVertexBuffer(
                          2, *batch.vertices, batch.weight_offset, error) &&
                      selection_encoder->setVertexBuffer(
                          4, *palette_by_batch[index], 0, error);
        }
        encoded = encoded &&
            selection_encoder->setViewport(viewport, error) &&
            selection_encoder->setScissor(scissor, error) &&
            selection_encoder->setCullMode(
                batch.double_sided ? ALMetalCullMode::none
                                   : ALMetalCullMode::back,
                error) &&
            selection_encoder->setFrontFacingWinding(
                ALMetalWinding::clockwise, error) &&
            selection_encoder->drawIndexed(
                ALMetalPrimitiveType::triangle, batch.index_count,
                batch.index_type, *batch.indices,
                batch.index_offset, 1, error);
    }
    if (selection_encoder)
    {
        encoded = selection_encoder->end(error) && encoded;
    }
    selection_encoder.reset();
    selection_pass.reset();

    std::string resume_error;
    const bool resumed = mImpl->resumeMainPass(&resume_error);
    if (!resumed)
    {
        setError(error, resume_error);
        return false;
    }
    if (!encoded)
    {
        return false;
    }
    mImpl->frame_batches += scene.batch_count;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        mImpl->frame_vertices += scene.batches[index].index_count;
    }
    return true;
}

bool ALMetalUIRenderer::drawSelectionContour(
    const ALMetalSelectionContourScene& scene,
    std::string* error)
{
    const bool glow = scene.glow_radius != 0u;
    const bool edges = scene.contour_enabled || glow;
    const bool wire = scene.wire_batch_count != 0u;
    const bool contrast = scene.contrast_shift &&
        (scene.contour_enabled || wire);
    const bool palette_size_valid =
        scene.palette_width == 256u && scene.palette_height >= 2u &&
        scene.palette_height % 2u == 0u &&
        scene.palette_height <= 512u &&
        scene.palette_bytes ==
            static_cast<std::size_t>(scene.palette_width) *
            static_cast<std::size_t>(scene.palette_height) * 4u;
    if (!mImpl || mImpl->surface || !scene.palette_rgba8 ||
        !palette_size_valid || scene.width == 0 || scene.height == 0 ||
        (!edges && !wire) || (wire && !scene.wire_batches) ||
        (edges && (scene.outline_width == 0 ||
                   scene.outline_width > 253u ||
                   scene.inner_width == 0 ||
                   scene.inner_width > 253u)) ||
        scene.rim_width > 253u || scene.glow_radius > 253u ||
        (wire && (scene.wire_width == 0 || scene.wire_width > 253u ||
                  !std::isfinite(scene.wire_depth_slope) ||
                  !std::isfinite(scene.wire_depth_units) ||
                  scene.wire_depth_slope < 0.f ||
                  scene.wire_depth_units < 0.f)) ||
        !std::isfinite(scene.glow_brightness) ||
        scene.glow_brightness < 0.f || scene.glow_brightness > 64.f)
    {
        setError(error, "Metal selection contour inputs are invalid");
        return false;
    }
    if (!finiteFloats(scene.inverse_projection, 16))
    {
        setError(error,
                 "Metal selection contour inverse projection must be finite");
        return false;
    }
    if (!mImpl->frame || !mImpl->encoder || !mImpl->selection_target ||
        mImpl->frame_width != scene.width ||
        mImpl->frame_height != scene.height ||
        mImpl->selection_target->descriptor().width != scene.width ||
        mImpl->selection_target->descriptor().height != scene.height)
    {
        setError(error,
                 "Metal selection contour requires current same-size ids");
        return false;
    }

    std::vector<std::unique_ptr<ALMetalBuffer>> wire_palettes;
    std::vector<const ALMetalBuffer*> wire_palette_by_batch(
        scene.wire_batch_count, nullptr);
    for (std::size_t index = 0;
         index < scene.wire_batch_count; ++index)
    {
        const ALMetalSelectionIDBatch& batch = scene.wire_batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4u : 2u;
        const bool skinned = batch.skin_palette != nullptr ||
                             batch.skin_palette_float_count != 0 ||
                             batch.skin_joint_count != 0;
        const std::uint32_t palette_row =
            (batch.id / scene.palette_width) * 2u + 1u;
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count < 3 || batch.index_count % 3 != 0 ||
            batch.index_count >
                std::numeric_limits<std::uint32_t>::max() / 2u ||
            batch.id == 0 || batch.id > 65535 ||
            palette_row >= scene.palette_height ||
            batch.position_offset % (sizeof(float) * 4) != 0 ||
            batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
            batch.index_offset % index_stride != 0 ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       sizeof(float) * 4, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       sizeof(float) * 2, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()) ||
            !finiteFloats(batch.modelview_projection, 16) ||
            !finiteFloats(batch.texture_matrix, 16) ||
            !finiteFloats(batch.base_color_transform, 8) ||
            !std::isfinite(batch.alpha_cutoff) ||
            batch.alpha_cutoff < -1.f || batch.alpha_cutoff > 2.f)
        {
            setError(error, "Metal selection wire batch is invalid");
            return false;
        }
        if (batch.alpha_cutoff > 0.f && batch.alpha_cutoff <= 1.f &&
            (!batch.texture ||
             !alMetalHasUsage(batch.texture->descriptor().usage,
                              ALMetalTextureUsage::shader_read)))
        {
            setError(error,
                     "Metal selection wire alpha test is missing its texture");
            return false;
        }
        if (!skinned)
        {
            continue;
        }
        const std::size_t expected_palette_floats =
            4u + static_cast<std::size_t>(batch.skin_joint_count) * 12u;
        if (!batch.skin_palette || batch.skin_joint_count == 0 ||
            batch.skin_palette_float_count != expected_palette_floats ||
            batch.weight_offset % (sizeof(float) * 4) != 0 ||
            !rangeFits(batch.weight_offset, batch.vertex_count,
                       sizeof(float) * 4, batch.vertices->length()) ||
            !finiteFloats(batch.skin_palette,
                          batch.skin_palette_float_count))
        {
            setError(error,
                     "Metal selection wire skinning data is invalid");
            return false;
        }
        ALMetalBufferDescriptor descriptor;
        descriptor.length =
            batch.skin_palette_float_count * sizeof(float);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy selection wire skin palette";
        auto palette = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!palette || !palette->write(
                0, batch.skin_palette, descriptor.length, error))
        {
            return false;
        }
        wire_palette_by_batch[index] = palette.get();
        wire_palettes.push_back(std::move(palette));
    }
    if ((edges && !mImpl->prepareSelectionContour(error)) ||
        (wire && !mImpl->prepareSelectionWireframe(error)) ||
        (glow && !mImpl->prepareSelectionGlow(error)) ||
        (contrast && !mImpl->prepareSelectionBackdrop(error)) ||
        !mImpl->prepareSelectionContourTargets(
            scene.width, scene.height,
            scene.palette_width, scene.palette_height,
            edges, glow, contrast, error))
    {
        return false;
    }
    for (auto& palette : wire_palettes)
    {
        mImpl->selection_frame_palettes.push_back(std::move(palette));
    }

    if (!mImpl->encoder->end(error))
    {
        return false;
    }
    mImpl->encoder.reset();
    mImpl->pass.reset();

    struct alignas(16) SelectionContourConstants
    {
        float inverse_projection[16];
        std::uint32_t target[4];
        std::uint32_t widths[4];
        std::uint32_t glow[4];
        float glow_values[4];
    } constants;
    static_assert(sizeof(SelectionContourConstants) == 128);
    std::memcpy(constants.inverse_projection,
                scene.inverse_projection,
                sizeof(constants.inverse_projection));
    constants.target[0] = scene.width;
    constants.target[1] = scene.height;
    constants.target[2] = (scene.width + 7u) / 8u;
    constants.target[3] = (scene.height + 7u) / 8u;
    constants.widths[0] = scene.outline_width;
    constants.widths[1] = scene.inner_width;
    constants.widths[2] = scene.rim_width;
    constants.widths[3] = scene.reverse_z ? 1u : 0u;
    constants.glow[0] = scene.glow_radius;
    constants.glow[1] = scene.contour_enabled ? 1u : 0u;
    constants.glow[2] = contrast ? 1u : 0u;
    constants.glow[3] = 0u;
    constants.glow_values[0] = scene.glow_brightness;
    constants.glow_values[1] = 0.f;
    constants.glow_values[2] = 0.f;
    constants.glow_values[3] = 0.f;

    bool encoded = mImpl->selection_palette->upload(
        mImpl->frame->commandBuffer(), 0, 0,
        {0, 0, scene.palette_width, scene.palette_height},
        scene.palette_rgba8,
        static_cast<std::size_t>(scene.palette_width) * 4u,
        error);

    const ALMetalTexture* id_map =
        mImpl->selection_target->colorTexture(0);
    const ALMetalTexture* depth_map =
        mImpl->selection_target->depthStencilTexture();
    const ALMetalViewport full_viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0};
    const ALMetalScissorRect full_scissor{
        0, 0, scene.width, scene.height};
    const ALMetalViewport    tile_viewport{ 0.0, 0.0, static_cast<double>(constants.target[2]), static_cast<double>(constants.target[3]),
                                         0.0, 1.0 };
    const ALMetalScissorRect tile_scissor{ 0, 0, constants.target[2], constants.target[3] };

    if (encoded && edges)
    {
        ALMetalRenderPassConfig jump_config;
        jump_config.colors[0].load  = ALMetalLoadAction::clear;
        jump_config.colors[0].store = ALMetalStoreAction::store;
        jump_config.colors[0].clear = { 0.0, 0.0, 0.0, 0.0 };
        auto jump_pass              = encoded ? mImpl->selection_jump_target->makeRenderPass(jump_config, error) : nullptr;
        auto jump_encoder =
            jump_pass ? ALMetalRenderEncoder::begin(mImpl->frame->commandBuffer(), *jump_pass, "Alchemy selection jump encoder", error)
                      : nullptr;
        encoded = encoded && jump_encoder && jump_encoder->setPipeline(*mImpl->selection_jump_pipeline, error) &&
                  jump_encoder->setFragmentTexture(0, id_map, error) && jump_encoder->setFragmentTexture(1, depth_map, error) &&
                  jump_encoder->setFragmentBytes(0, &constants, sizeof(constants), error) &&
                  jump_encoder->setViewport(full_viewport, error) && jump_encoder->setScissor(full_scissor, error) &&
                  jump_encoder->setCullMode(ALMetalCullMode::none, error) &&
                  jump_encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
        if (jump_encoder)
        {
            encoded = jump_encoder->end(error) && encoded;
        }
        jump_encoder.reset();
        jump_pass.reset();

        ALMetalRenderPassConfig tile_config;
        tile_config.colors[0].load  = ALMetalLoadAction::clear;
        tile_config.colors[0].store = ALMetalStoreAction::store;
        tile_config.colors[0].clear = { 0.0, 0.0, 0.0, 0.0 };
        tile_config.colors[1].load  = ALMetalLoadAction::clear;
        tile_config.colors[1].store = ALMetalStoreAction::store;
        tile_config.colors[1].clear = { 1.0e30, 0.0, 0.0, 0.0 };
        auto tile_pass              = encoded ? mImpl->selection_tile_target->makeRenderPass(tile_config, error) : nullptr;
        auto tile_encoder =
            tile_pass ? ALMetalRenderEncoder::begin(mImpl->frame->commandBuffer(), *tile_pass, "Alchemy selection tile encoder", error)
                      : nullptr;
        encoded = encoded && tile_encoder && tile_encoder->setPipeline(*mImpl->selection_tile_pipeline, error) &&
                  tile_encoder->setFragmentTexture(0, id_map, error) && tile_encoder->setFragmentTexture(1, depth_map, error) &&
                  tile_encoder->setFragmentTexture(2, mImpl->selection_jump_target->colorTexture(0), error) &&
                  tile_encoder->setFragmentBytes(0, &constants, sizeof(constants), error) &&
                  tile_encoder->setViewport(tile_viewport, error) && tile_encoder->setScissor(tile_scissor, error) &&
                  tile_encoder->setCullMode(ALMetalCullMode::none, error) &&
                  tile_encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
        if (tile_encoder)
        {
            encoded = tile_encoder->end(error) && encoded;
        }
        tile_encoder.reset();
        tile_pass.reset();
    }

    if (encoded && glow)
    {
        ALMetalRenderPassConfig reach_config;
        reach_config.colors[0].load  = ALMetalLoadAction::clear;
        reach_config.colors[0].store = ALMetalStoreAction::store;
        reach_config.colors[0].clear = { 0.0, 0.0, 0.0, 0.0 };
        auto reach_pass              = mImpl->selection_glow_reach_target->makeRenderPass(reach_config, error);
        auto reach_encoder = reach_pass
            ? ALMetalRenderEncoder::begin(
                  mImpl->frame->commandBuffer(), *reach_pass,
                  "Alchemy selection glow reach encoder", error)
            : nullptr;
        encoded = reach_encoder &&
            reach_encoder->setPipeline(
                *mImpl->selection_glow_reach_pipeline, error) &&
            reach_encoder->setFragmentTexture(
                0, mImpl->selection_tile_target->colorTexture(0), error) &&
            reach_encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            reach_encoder->setViewport(tile_viewport, error) &&
            reach_encoder->setScissor(tile_scissor, error) &&
            reach_encoder->setCullMode(ALMetalCullMode::none, error) &&
            reach_encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error);
        if (reach_encoder)
        {
            encoded = reach_encoder->end(error) && encoded;
        }

        ALMetalRenderPassConfig row_config;
        row_config.colors[0].load = ALMetalLoadAction::clear;
        row_config.colors[0].store = ALMetalStoreAction::store;
        row_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
        auto row_pass = encoded
            ? mImpl->selection_glow_row_target->makeRenderPass(
                  row_config, error)
            : nullptr;
        auto row_encoder = row_pass
            ? ALMetalRenderEncoder::begin(
                  mImpl->frame->commandBuffer(), *row_pass,
                  "Alchemy selection glow row encoder", error)
            : nullptr;
        encoded = encoded && row_encoder &&
            row_encoder->setPipeline(
                *mImpl->selection_glow_row_pipeline, error) &&
            row_encoder->setFragmentTexture(0, id_map, error) &&
            row_encoder->setFragmentTexture(
                1, mImpl->selection_glow_reach_target->colorTexture(0),
                error) &&
            row_encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            row_encoder->setViewport(full_viewport, error) &&
            row_encoder->setScissor(full_scissor, error) &&
            row_encoder->setCullMode(ALMetalCullMode::none, error) &&
            row_encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error);
        if (row_encoder)
        {
            encoded = row_encoder->end(error) && encoded;
        }
    }

    if (encoded && contrast)
    {
        ALMetalRenderPassConfig backdrop_config;
        backdrop_config.colors[0].load = ALMetalLoadAction::clear;
        backdrop_config.colors[0].store = ALMetalStoreAction::store;
        backdrop_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
        auto backdrop_pass =
            mImpl->selection_backdrop_target->makeRenderPass(
                backdrop_config, error);
        auto backdrop_encoder = backdrop_pass
            ? ALMetalRenderEncoder::begin(
                  mImpl->frame->commandBuffer(), *backdrop_pass,
                  "Alchemy selection contrast backdrop encoder", error)
            : nullptr;
        const ALMetalViewport backdrop_viewport{
            0.0, 0.0,
            static_cast<double>(
                mImpl->selection_backdrop_target->descriptor().width),
            static_cast<double>(
                mImpl->selection_backdrop_target->descriptor().height),
            0.0, 1.0};
        const ALMetalScissorRect backdrop_scissor{
            0, 0,
            mImpl->selection_backdrop_target->descriptor().width,
            mImpl->selection_backdrop_target->descriptor().height};
        encoded = backdrop_encoder &&
            backdrop_encoder->setPipeline(
                *mImpl->selection_backdrop_pipeline, error) &&
            backdrop_encoder->setFragmentTexture(
                0, mImpl->target->colorTexture(0), error) &&
            backdrop_encoder->setFragmentTexture(1, id_map, error) &&
            backdrop_encoder->setFragmentTexture(2, depth_map, error) &&
            backdrop_encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            backdrop_encoder->setViewport(backdrop_viewport, error) &&
            backdrop_encoder->setScissor(backdrop_scissor, error) &&
            backdrop_encoder->setCullMode(ALMetalCullMode::none, error) &&
            backdrop_encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error);
        if (backdrop_encoder)
        {
            encoded = backdrop_encoder->end(error) && encoded;
        }
    }

    std::string resume_error;
    const bool resumed = mImpl->resumeMainPass(&resume_error);
    if (!resumed)
    {
        setError(error, resume_error);
        return false;
    }
    if (!encoded)
    {
        return false;
    }

    std::uint64_t wire_draws = 0;
    std::uint64_t wire_vertices = 0;
    if (wire)
    {
        struct alignas(16) SelectionWireConstants
        {
            float modelview_projection[16];
            float texture_matrix[16];
            float inverse_projection[16];
            float base_color_transform[8];
            float screen_material[4];
            std::uint32_t config[4];
            std::uint32_t draw[4];
        } wire_constants;
        static_assert(sizeof(SelectionWireConstants) == 272);

        const ALMetalTexture* wire_backdrop = contrast
            ? mImpl->selection_backdrop_target->colorTexture(0)
            : mImpl->selection_palette.get();
        const std::uint32_t pass_count = scene.wire_hud ? 1u : 2u;
        for (std::uint32_t pass_index = 0;
             encoded && pass_index < pass_count; ++pass_index)
        {
            const std::uint32_t wire_pass = scene.wire_hud
                ? 2u : pass_index;
            ALMetalUIDepthState wire_depth_descriptor;
            wire_depth_descriptor.enabled = !scene.wire_hud;
            wire_depth_descriptor.write_enabled = false;
            if (wire_pass == 0u)
            {
                wire_depth_descriptor.compare = scene.reverse_z
                    ? ALMetalCompareFunction::greater_equal
                    : ALMetalCompareFunction::less_equal;
            }
            else if (wire_pass == 1u)
            {
                wire_depth_descriptor.compare = scene.reverse_z
                    ? ALMetalCompareFunction::less
                    : ALMetalCompareFunction::greater;
            }
            ALMetalDepthStencilState* wire_depth =
                mImpl->depthState(wire_depth_descriptor, error);
            if (!wire_depth)
            {
                return false;
            }

            const float bias_sign = scene.reverse_z ? 1.f : -1.f;
            encoded = mImpl->encoder->setDepthStencilState(
                          wire_depth, error) &&
                      mImpl->encoder->setDepthBias(
                          scene.wire_hud
                              ? 0.f
                              : bias_sign * scene.wire_depth_units,
                          scene.wire_hud
                              ? 0.f
                              : bias_sign * scene.wire_depth_slope,
                          0.f, error);
            for (std::size_t index = 0;
                 encoded && index < scene.wire_batch_count; ++index)
            {
                const ALMetalSelectionIDBatch& batch =
                    scene.wire_batches[index];
                if (wire_pass == 1u && !batch.show_hidden)
                {
                    continue;
                }
                const bool skinned = wire_palette_by_batch[index] != nullptr;
                const std::size_t pipeline_index =
                    (skinned ? 2u : 0u) +
                    (batch.index_type == ALMetalIndexType::uint32 ? 1u : 0u);
                std::memcpy(
                    wire_constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(wire_constants.modelview_projection));
                std::memcpy(
                    wire_constants.texture_matrix,
                    batch.texture_matrix,
                    sizeof(wire_constants.texture_matrix));
                std::memcpy(
                    wire_constants.inverse_projection,
                    scene.inverse_projection,
                    sizeof(wire_constants.inverse_projection));
                std::memcpy(
                    wire_constants.base_color_transform,
                    batch.base_color_transform,
                    sizeof(wire_constants.base_color_transform));
                wire_constants.screen_material[0] =
                    static_cast<float>(scene.width);
                wire_constants.screen_material[1] =
                    static_cast<float>(scene.height);
                wire_constants.screen_material[2] = batch.alpha_cutoff;
                wire_constants.screen_material[3] = 0.001f;
                wire_constants.config[0] = batch.id;
                wire_constants.config[1] = scene.wire_width;
                wire_constants.config[2] =
                    (scene.reverse_z ? 1u : 0u) |
                    (batch.keep_cut ? 2u : 0u);
                wire_constants.config[3] = batch.skin_joint_count;
                wire_constants.draw[0] = wire_pass;
                wire_constants.draw[1] = contrast ? 1u : 0u;
                wire_constants.draw[2] = 0u;
                wire_constants.draw[3] = 0u;

                ALMetalSamplerState* diffuse_sampler =
                    mImpl->sampler(batch.sampler, error);
                encoded = diffuse_sampler != nullptr &&
                    mImpl->encoder->setPipeline(
                        *mImpl->selection_wire_pipelines[pipeline_index],
                        error) &&
                    mImpl->encoder->setVertexBuffer(
                        0, *batch.vertices, batch.position_offset, error) &&
                    mImpl->encoder->setVertexBuffer(
                        1, *batch.vertices, batch.texcoord_offset, error) &&
                    mImpl->encoder->setVertexBytes(
                        3, &wire_constants,
                        sizeof(wire_constants), error) &&
                    mImpl->encoder->setVertexBuffer(
                        5, *batch.indices, batch.index_offset, error) &&
                    mImpl->encoder->setFragmentBytes(
                        0, &wire_constants,
                        sizeof(wire_constants), error) &&
                    mImpl->encoder->setFragmentTexture(
                        0, batch.texture ? batch.texture
                                         : mImpl->frame_background,
                        error) &&
                    mImpl->encoder->setFragmentTexture(
                        1, depth_map, error) &&
                    mImpl->encoder->setFragmentTexture(
                        2, mImpl->selection_palette.get(), error) &&
                    mImpl->encoder->setFragmentTexture(
                        3, wire_backdrop, error) &&
                    mImpl->encoder->setFragmentSampler(
                        0, diffuse_sampler, error);
                if (encoded && skinned)
                {
                    encoded = mImpl->encoder->setVertexBuffer(
                                  2, *batch.vertices,
                                  batch.weight_offset, error) &&
                              mImpl->encoder->setVertexBuffer(
                                  4, *wire_palette_by_batch[index],
                                  0, error);
                }
                encoded = encoded &&
                    mImpl->encoder->setViewport(full_viewport, error) &&
                    mImpl->encoder->setScissor(full_scissor, error) &&
                    mImpl->encoder->setCullMode(
                        batch.double_sided
                            ? ALMetalCullMode::none
                            : ALMetalCullMode::back,
                        error) &&
                    mImpl->encoder->setFrontFacingWinding(
                        ALMetalWinding::clockwise, error) &&
                    mImpl->encoder->draw(
                        ALMetalPrimitiveType::triangle, 0,
                        batch.index_count * 2u, 1, error);
                if (encoded)
                {
                    ++wire_draws;
                    wire_vertices += batch.index_count * 2u;
                }
            }
        }
        encoded = mImpl->encoder->setDepthBias(
            0.f, 0.f, 0.f, error) && encoded;
        if (!encoded)
        {
            return false;
        }
    }

    if (edges)
    {
        ALMetalUIDepthState       disabled_depth;
        ALMetalDepthStencilState* contour_depth = mImpl->depthState(disabled_depth, error);
        if (!contour_depth)
        {
            return false;
        }
        ALMetalRenderEncoder& contour_encoder = *mImpl->encoder;
        encoded =
            contour_encoder.setPipeline(*mImpl->selection_contour_pipeline, error) &&
            contour_encoder.setDepthStencilState(contour_depth, error) && contour_encoder.setFragmentTexture(0, id_map, error) &&
            contour_encoder.setFragmentTexture(1, depth_map, error) &&
            contour_encoder.setFragmentTexture(2, mImpl->selection_jump_target->colorTexture(0), error) &&
            contour_encoder.setFragmentTexture(3, mImpl->selection_tile_target->colorTexture(0), error) &&
            contour_encoder.setFragmentTexture(4, mImpl->selection_tile_target->colorTexture(1), error) &&
            contour_encoder.setFragmentTexture(5, mImpl->selection_palette.get(), error) &&
            contour_encoder.setFragmentTexture(
                6, glow ? mImpl->selection_glow_reach_target->colorTexture(0) : mImpl->selection_tile_target->colorTexture(0), error) &&
            contour_encoder.setFragmentTexture(
                7, glow ? mImpl->selection_glow_row_target->colorTexture(0) : mImpl->selection_jump_target->colorTexture(0), error) &&
            contour_encoder.setFragmentTexture(
                8, contrast ? mImpl->selection_backdrop_target->colorTexture(0) : mImpl->selection_tile_target->colorTexture(0), error) &&
            contour_encoder.setFragmentBytes(0, &constants, sizeof(constants), error) &&
            contour_encoder.setViewport(full_viewport, error) && contour_encoder.setScissor(full_scissor, error) &&
            contour_encoder.setCullMode(ALMetalCullMode::none, error) &&
            contour_encoder.draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
        if (!encoded)
        {
            return false;
        }
    }
    mImpl->frame_batches += (edges ? 3u : 0u) +
        wire_draws + (glow ? 2u : 0u) +
        (contrast ? 1u : 0u);
    mImpl->frame_vertices += (edges ? 9u : 0u) +
        wire_vertices + (glow ? 6u : 0u) +
        (contrast ? 3u : 0u);
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
            mImpl->selection_frame_palettes.clear();
            return false;
        }
    }
    if (!mImpl->frame && mImpl->frame_depth)
    {
        mImpl->frame_depth = nullptr;
        mImpl->selection_frame_palettes.clear();
        return true;
    }
    else if (!mImpl->frame && mImpl->surface)
    {
        const ALMetalSurfaceInfo& info = mImpl->surface->info();
        if (!mImpl->beginFrame(info.width, info.height, error))
        {
            mImpl->frame_background = nullptr;
            mImpl->frame_depth = nullptr;
            mImpl->selection_frame_palettes.clear();
            return false;
        }
    }
    if (!mImpl->frame)
    {
        mImpl->selection_frame_palettes.clear();
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
        mImpl->selection_frame_palettes.clear();
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
        mImpl->selection_frame_palettes.clear();
        return false;
    }
    if (!mImpl->scheduler->submit(*mImpl->frame, error))
    {
        mImpl->frame.reset();
        mImpl->pass.reset();
        mImpl->drawable.reset();
        mImpl->frame_background = nullptr;
        mImpl->frame_depth = nullptr;
        mImpl->selection_frame_palettes.clear();
        return false;
    }
    mImpl->frame.reset();
    mImpl->pass.reset();
    mImpl->drawable.reset();
    mImpl->frame_background = nullptr;
    mImpl->frame_depth = nullptr;
    mImpl->selection_frame_palettes.clear();
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

const ALMetalTexture* ALMetalUIRenderer::selectionIDTexture() const noexcept
{
    return mImpl && mImpl->selection_target
        ? mImpl->selection_target->colorTexture(0) : nullptr;
}

const ALMetalTexture* ALMetalUIRenderer::selectionDepthTexture() const noexcept
{
    return mImpl && mImpl->selection_target
        ? mImpl->selection_target->depthStencilTexture() : nullptr;
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

