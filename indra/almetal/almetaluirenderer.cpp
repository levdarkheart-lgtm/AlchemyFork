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
        std::unique_ptr<ALMetalRenderPipeline> state;
    };

    struct SamplerEntry
    {
        ALMetalSamplerDescriptor descriptor{};
        std::unique_ptr<ALMetalSamplerState> state;
    };

    const ALMetalDevice* device = nullptr;
    ALMetalFrameScheduler* scheduler = nullptr;
    ALMetalSurface* surface = nullptr;
    ALMetalPixelFormat output_format = ALMetalPixelFormat::bgra8_unorm;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderTarget> target;
    std::unique_ptr<ALMetalDrawable> drawable;
    std::unique_ptr<ALMetalRenderPass> pass;
    std::unique_ptr<ALMetalFrame> frame;
    std::unique_ptr<ALMetalRenderEncoder> encoder;
    std::vector<PipelineEntry> pipelines;
    std::vector<SamplerEntry> samplers;
    ALMetalUIRendererStats totals{};
    std::uint64_t frame_batches = 0;
    std::uint64_t frame_vertices = 0;
    std::uint32_t frame_width = 0;
    std::uint32_t frame_height = 0;

    ALMetalRenderPipeline* pipeline(ALMetalUIShader shader,
                                    const ALMetalUIBlendState& blend,
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
            if (entry.shader == shader && sameBlend(entry.blend, target_blend))
            {
                return entry.state.get();
            }
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = shader == ALMetalUIShader::textured
            ? "almetal_ui_vertex" : "almetal_solid_vertex";
        descriptor.fragment_function = shader == ALMetalUIShader::textured
            ? "almetal_ui_fragment" : "almetal_solid_fragment";
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
        descriptor.label = shader == ALMetalUIShader::textured
            ? "Alchemy production UI pipeline" : "Alchemy production solid UI pipeline";

        auto state = ALMetalRenderPipeline::create(*device, descriptor, error);
        if (!state)
        {
            return nullptr;
        }
        pipelines.push_back({shader, target_blend, std::move(state)});
        totals.pipeline_states = pipelines.size();
        return pipelines.back().state.get();
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
                 target->descriptor().height != height)
        {
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = width;
            descriptor.height = height;
            descriptor.color_attachment_count = 1;
            descriptor.colors[0].format = output_format;
            descriptor.colors[0].shader_read = true;
            descriptor.colors[0].label = "Alchemy production UI color";
            descriptor.label = "Alchemy production UI target";
            target = ALMetalRenderTarget::create(*device, descriptor, error);
            if (!target)
            {
                return false;
            }
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
            pass = target->makeRenderPass(config, error);
        }
        if (!pass)
        {
            drawable.reset();
            return false;
        }

        frame = scheduler->beginFrame("Alchemy production UI frame", UINT32_MAX, error);
        if (!frame)
        {
            pass.reset();
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
        !library->hasFunction("almetal_solid_vertex") ||
        !library->hasFunction("almetal_solid_fragment"))
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

bool ALMetalUIRenderer::draw(const ALMetalUIBatch& batch, std::string* error)
{
    if (!mImpl || !batch.vertices || !batch.texture || batch.vertex_count == 0 ||
        batch.target_width == 0 || batch.target_height == 0)
    {
        setError(error, "Metal UI batch is missing geometry, texture or target dimensions");
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

    ALMetalRenderPipeline* pipeline = mImpl->pipeline(batch.shader, batch.blend, error);
    ALMetalSamplerState* sampler = mImpl->sampler(batch.sampler, error);
    if (!pipeline || !sampler)
    {
        return false;
    }

    ALMetalRenderEncoder& encoder = *mImpl->encoder;
    if (!encoder.setPipeline(*pipeline, error) ||
        !encoder.setVertexBuffer(0, *batch.vertices, batch.position_offset, error) ||
        !encoder.setVertexBuffer(1, *batch.vertices, batch.texcoord_offset, error))
    {
        return false;
    }
    if (batch.shader == ALMetalUIShader::textured &&
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
    } remap{draw.remap_mode, batch.vertex_count};
    if (!encoder.setVertexBytes(4, &remap, sizeof(remap), error) ||
        !encoder.setFragmentTexture(0, batch.texture, error) ||
        !encoder.setFragmentSampler(0, sampler, error))
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
    else if (!encoder.setFragmentBytes(0, batch.solid_color,
                                       sizeof(batch.solid_color), error))
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

bool ALMetalUIRenderer::endFrame(std::string* error)
{
    if (!mImpl)
    {
        return true;
    }
    if (!mImpl->frame && mImpl->surface)
    {
        const ALMetalSurfaceInfo& info = mImpl->surface->info();
        if (!mImpl->beginFrame(info.width, info.height, error))
        {
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
        return false;
    }
    if (!mImpl->scheduler->submit(*mImpl->frame, error))
    {
        mImpl->frame.reset();
        mImpl->pass.reset();
        mImpl->drawable.reset();
        return false;
    }
    mImpl->frame.reset();
    mImpl->pass.reset();
    mImpl->drawable.reset();
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

