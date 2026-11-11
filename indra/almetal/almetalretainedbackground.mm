/**
 * @file almetalretainedbackground.mm
 * @brief Completion-safe retained pre-UI native color and depth.
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
#include "almetalretainedbackground.h"
#include "almetaldevice.h"
#include "almetalpipeline.h"

#import <Metal/Metal.h>

#include <new>
#include <utility>

namespace
{
bool validContext(const ALMetalRetainedBackground::Context& c)
{
    return c.device && c.device->nativeHandle() && c.surface_identity &&
        c.surface_generation != 0 && c.width != 0 && c.height != 0;
}

bool validTexture(const ALMetalTexture* texture,
                  const ALMetalRetainedBackground::Context& c, bool depth,
                  std::uint32_t width, std::uint32_t height)
{
    if (!texture || !texture->nativeHandle()) return false;
    const auto& d = texture->descriptor();
    id<MTLTexture> native = (__bridge id<MTLTexture>)texture->nativeHandle();
    const bool format = depth
        ? d.format == ALMetalPixelFormat::depth32_float ||
          d.format == ALMetalPixelFormat::depth32_float_stencil8
        : d.format == ALMetalPixelFormat::bgra8_unorm;
    return format && d.type == ALMetalTextureType::texture_2d &&
        d.width == width && d.height == height &&
        d.sample_count == 1 && d.array_length == 1 &&
        d.storage != ALMetalStorageMode::memoryless &&
        alMetalHasUsage(d.usage, ALMetalTextureUsage::shader_read) &&
        (__bridge void*)native.device == c.device->nativeHandle();
}
void fail(std::string* error, const char* text) { if (error) *error = text; }
}

struct ALMetalRetainedBackground::ScaleState
{
    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    std::unique_ptr<ALMetalSamplerState> color_sampler, depth_sampler;
    std::unique_ptr<ALMetalDepthStencilState> depth_state;
    std::shared_ptr<ALMetalRenderTargetPool> pool;
    std::uint64_t shader_generation = 0;

    bool reloadShaders(std::string* error)
    {
        const auto generation = ALMetalShaderLibrary::builtinReloadGeneration();
        if (shader_generation == generation) return true;
        auto candidate_library = ALMetalShaderLibrary::loadBuiltin(*device, error);
        if (!candidate_library) return false;
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = candidate_library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function = "almetal_retained_background_scale_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.label = "Alchemy retained world normalized color/depth scale";
        auto candidate = ALMetalRenderPipeline::create(*device, descriptor, error);
        if (!candidate) return false;
        library = std::move(candidate_library);
        pipeline = std::move(candidate);
        shader_generation = generation;
        return true;
    }
};

ALMetalRetainedBackground::ALMetalRetainedBackground() = default;
ALMetalRetainedBackground::~ALMetalRetainedBackground() = default;

std::uint64_t ALMetalRetainedBackground::shaderReloadGeneration() const noexcept
{
    return mScale ? mScale->shader_generation : 0;
}

ALMetalRetainedBackground::Lease ALMetalRetainedBackground::retain(
    const Context& context, std::uint64_t source_frame,
    std::shared_ptr<const ALMetalRenderTarget> color_owner,
    std::shared_ptr<const ALMetalRenderTarget> depth_owner, std::string* error)
{
    if (!validContext(context) || !color_owner || !depth_owner ||
        color_owner->descriptor().color_attachment_count != 1 ||
        !validTexture(color_owner->colorTexture(0), context, false, context.width, context.height) ||
        !validTexture(depth_owner->depthStencilTexture(), context, true, context.width, context.height))
    {
        fail(error, "Retained UI background requires compatible native color and readable depth");
        return {};
    }
    try
    {
        auto record = std::make_shared<Record>();
        record->color = ALMetalTexture::retainSharedBacking(*color_owner->colorTexture(0), error);
        record->depth = ALMetalTexture::retainSharedBacking(*depth_owner->depthStencilTexture(), error);
        if (!record->color || !record->depth) return {};
        record->context = context;
        record->source_frame = source_frame;
        record->color_owner = std::move(color_owner);
        record->depth_owner = std::move(depth_owner);
        return record;
    }
    catch (const std::bad_alloc&)
    {
        fail(error, "Retained UI background allocation failed");
        return {};
    }
}

bool ALMetalRetainedBackground::publish(
    const Context& context, std::uint64_t source_frame,
    std::shared_ptr<const ALMetalRenderTarget> color_owner,
    std::shared_ptr<const ALMetalRenderTarget> depth_owner, std::string* error)
{
    if (source_frame == std::numeric_limits<std::uint64_t>::max())
    {
        fail(error, "Completed world background requires a real source frame");
        return false;
    }
    const ALMetalTexture* color = color_owner ? color_owner->colorTexture(0) : nullptr;
    const ALMetalTexture* depth = depth_owner ? depth_owner->depthStencilTexture() : nullptr;
    if (!validContext(context) || !color || !depth ||
        color_owner->descriptor().color_attachment_count != 1 ||
        color->descriptor().width == 0 || color->descriptor().height == 0 ||
        !validTexture(color, context, false, color->descriptor().width, color->descriptor().height) ||
        !validTexture(depth, context, true, color->descriptor().width, color->descriptor().height))
    {
        fail(error, "World UI background requires a coherent native color/depth pair");
        return false;
    }
    const bool scale = color->descriptor().width != context.width ||
        color->descriptor().height != context.height;
    auto candidate = scale
        ? resample(context, source_frame, std::move(color_owner), std::move(depth_owner), error)
        : retain(context, source_frame, std::move(color_owner), std::move(depth_owner), error);
    if (!candidate) return false;
    mRecord = std::move(candidate);
    return true;
}

ALMetalRetainedBackground::Lease ALMetalRetainedBackground::resample(
    const Context& context, std::uint64_t source_frame,
    std::shared_ptr<const ALMetalRenderTarget> color_owner,
    std::shared_ptr<const ALMetalRenderTarget> depth_owner, std::string* error)
{
    try
    {
        if (!mScale || mScale->device != context.device)
        {
            auto state = std::make_unique<ScaleState>();
            state->device = context.device;
            if (!state->reloadShaders(error)) return {};
            ALMetalSamplerDescriptor sampler;
            sampler.min_filter = sampler.mag_filter = ALMetalSamplerFilter::linear;
            sampler.mip_filter = ALMetalSamplerMipFilter::none;
            sampler.address_s = sampler.address_t = sampler.address_r =
                ALMetalSamplerAddressMode::mirror_repeat;
            sampler.label = "Alchemy original final BilinearMirror color";
            state->color_sampler = ALMetalSamplerState::create(*context.device, sampler, error);
            sampler.min_filter = sampler.mag_filter = ALMetalSamplerFilter::nearest;
            sampler.address_s = sampler.address_t = sampler.address_r =
                ALMetalSamplerAddressMode::clamp_to_edge;
            sampler.label = "Alchemy original final PointClamp depth";
            state->depth_sampler = ALMetalSamplerState::create(*context.device, sampler, error);
            ALMetalDepthStencilDescriptor depth;
            depth.depth_compare = ALMetalCompareFunction::always;
            depth.depth_write_enabled = true;
            depth.label = "Alchemy exact normalized retained depth write";
            state->depth_state = ALMetalDepthStencilState::create(*context.device, depth, error);
            state->pool = ALMetalRenderTargetPool::create(*context.device);
            if (!state->pipeline || !state->color_sampler || !state->depth_sampler ||
                !state->depth_state || !state->pool) return {};
            mScale = std::move(state);
        }
        // Keep the target pool and published leases across a shader-only reload.
        if (!mScale->reloadShaders(error)) return {};
        ALMetalRenderTargetDescriptor d;
        d.width = context.width; d.height = context.height;
        d.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        d.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        d.depth_stencil_shader_read = true;
        d.label = "Alchemy surface-size retained world pair";
        std::shared_ptr<ALMetalRenderTarget> target = mScale->pool->acquire(d, error);
        if (!target) return {};
        auto candidate = retain(context, source_frame, target, target, error);
        if (!candidate) return {};
        ALMetalRenderPassConfig pc;
        pc.depth_store = ALMetalStoreAction::store;
        pc.stencil_store = ALMetalStoreAction::store;
        pc.clear_depth = context.reverse_z ? 0.0 : 1.0;
        auto pass = target->makeRenderPass(pc, error);
        auto command = context.device->makeCommandBuffer("Alchemy retained world scale", error);
        if (!pass || !command ||
            !command->retainUntilCompletion(color_owner, error) ||
            !command->retainUntilCompletion(depth_owner, error) ||
            !command->retainUntilCompletion(target, error)) return {};
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "World color/depth scale", error);
        if (!encoder ||
            !encoder->setPipeline(*mScale->pipeline, error) ||
            !encoder->setDepthStencilState(mScale->depth_state.get(), error) ||
            !encoder->setFragmentTexture(0, color_owner->colorTexture(0), error) ||
            !encoder->setFragmentTexture(1, depth_owner->depthStencilTexture(), error) ||
            !encoder->setFragmentSampler(0, mScale->color_sampler.get(), error) ||
            !encoder->setFragmentSampler(1, mScale->depth_sampler.get(), error) ||
            !encoder->setViewport({0, 0, static_cast<double>(context.width),
                static_cast<double>(context.height), 0, 1}, error) ||
            !encoder->setScissor({0, 0, context.width, context.height}, error) ||
            !encoder->setCullMode(ALMetalCullMode::none, error) ||
            !encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error) ||
            !encoder->end(error) || !command->commit(error)) return {};
        return candidate;
    }
    catch (const std::bad_alloc&)
    {
        fail(error, "Retained world scale allocation failed");
        return {};
    }
}

ALMetalRetainedBackground::Lease ALMetalRetainedBackground::find(
    const Context& c) const noexcept
{
    if (!mRecord) return {};
    const auto& old = mRecord->context;
    return c.device == old.device && c.surface_identity == old.surface_identity &&
        c.surface_generation == old.surface_generation &&
        c.width == old.width && c.height == old.height && c.reverse_z == old.reverse_z
            ? mRecord : Lease{};
}

ALMetalRetainedBackground::Lease ALMetalRetainedBackground::makeCleared(
    const Context& context, std::string* error)
{
    if (!validContext(context))
    {
        fail(error, "Cleared UI background context is invalid");
        return {};
    }
    ALMetalRenderTargetDescriptor d;
    d.width = context.width; d.height = context.height;
    d.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
    d.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
    d.depth_stencil_shader_read = true;
    d.label = "Alchemy current-size cleared UI background";
    std::shared_ptr<const ALMetalRenderTarget> target =
        ALMetalRenderTarget::create(*context.device, d, error);
    if (!target) return {};
    ALMetalRenderPassConfig pc;
    pc.colors[0].load = ALMetalLoadAction::clear;
    pc.colors[0].store = ALMetalStoreAction::store;
    pc.colors[0].clear = {0.0, 0.0, 0.0, 1.0};
    pc.depth_load = ALMetalLoadAction::clear; pc.depth_store = ALMetalStoreAction::store;
    pc.stencil_load = ALMetalLoadAction::clear; pc.stencil_store = ALMetalStoreAction::store;
    pc.clear_depth = context.reverse_z ? 0.0 : 1.0;
    auto pass = target->makeRenderPass(pc, error);
    auto command = context.device->makeCommandBuffer("Alchemy cleared UI backing", error);
    if (!pass || !command) return {};
    id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
    id<MTLRenderCommandEncoder> encoder =
        [native renderCommandEncoderWithDescriptor:(__bridge MTLRenderPassDescriptor*)pass->nativeHandle()];
    if (!encoder)
    {
        fail(error, "Cleared UI backing render encoder is unavailable");
        return {};
    }
    [encoder endEncoding];
    if (!command->commit(error)) return {};
    return retain(context, std::numeric_limits<std::uint64_t>::max(), target, target, error);
}
