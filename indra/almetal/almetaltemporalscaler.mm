/**
 * @file almetaltemporalscaler.mm
 * @brief Stateful classic MetalFX temporal reconstruction on the main queue.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetaltemporalscaler.h"

#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

#include <array>
#include <cmath>
#include <new>
#include <utility>
#include <vector>

namespace
{
namespace RP = ALMetalResourcePlanning;
using Failure = ALMetalTemporalScalerFailure;

MTLPixelFormat nativeFormat(ALMetalPixelFormat format) noexcept
{
    switch (format)
    {
    case ALMetalPixelFormat::r8_unorm: return MTLPixelFormatR8Unorm;
    case ALMetalPixelFormat::r16_float: return MTLPixelFormatR16Float;
    case ALMetalPixelFormat::r32_float: return MTLPixelFormatR32Float;
    case ALMetalPixelFormat::depth32_float: return MTLPixelFormatDepth32Float;
    case ALMetalPixelFormat::rg16_float: return MTLPixelFormatRG16Float;
    case ALMetalPixelFormat::rgba16_float: return MTLPixelFormatRGBA16Float;
    default: return MTLPixelFormatInvalid;
    }
}

ALMetalTextureUsage typedUsage(MTLTextureUsage value) noexcept
{
    auto result = ALMetalTextureUsage::none;
    if (value & MTLTextureUsageShaderRead) result = result | ALMetalTextureUsage::shader_read;
    if (value & MTLTextureUsageShaderWrite) result = result | ALMetalTextureUsage::shader_write;
    if (value & MTLTextureUsageRenderTarget) result = result | ALMetalTextureUsage::render_target;
    return result;
}

constexpr std::array identitySwizzle{
    ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::green,
    ALMetalTextureSwizzle::blue, ALMetalTextureSwizzle::alpha};

bool compatible(const ALMetalTexture* texture, id<MTLDevice> device,
    std::uint32_t width, std::uint32_t height, ALMetalPixelFormat format,
    std::uint64_t usage, bool output = false) noexcept
{
    if (!texture) return false;
    auto native = (__bridge id<MTLTexture>)texture->nativeHandle();
    const auto& desc = texture->descriptor();
    return native && native.device == device && desc.type == ALMetalTextureType::texture_2d &&
        native.textureType == MTLTextureType2D && desc.width == width && desc.height == height &&
        native.width == width && native.height == height && desc.format == format &&
        native.pixelFormat == nativeFormat(format) && desc.sample_count == 1 && native.sampleCount == 1 &&
        desc.array_length == 1 && native.arrayLength == 1 && desc.mipmap_level_count == 1 &&
        native.mipmapLevelCount == 1 && desc.swizzle == identitySwizzle &&
        desc.hazard_tracking == ALMetalHazardTrackingMode::tracked &&
        native.hazardTrackingMode == MTLHazardTrackingModeTracked &&
        desc.storage != ALMetalStorageMode::memoryless && native.storageMode != MTLStorageModeMemoryless &&
        (!output || (desc.storage == ALMetalStorageMode::private_gpu && native.storageMode == MTLStorageModePrivate)) &&
        (native.usage & usage) == usage;
}

struct ResourcePass
{
    ALMetalCommandBuffer& command;
    std::uint64_t identity = 0;
    bool active = false;
    ResourcePass(ALMetalCommandBuffer& value, std::string* error) : command(value)
    { active = command.beginResourcePass(RP::Pass::render, identity, error); }
    ~ResourcePass() { if (active) command.endResourcePass(identity); }
    bool end(std::string* error)
    {
        if (!active) return false;
        active = false;
        return command.endResourcePass(identity, error);
    }
};
}

struct ALMetalTemporalScaler::Impl
{
    struct Entry
    {
        ALMetalTemporalScalerDescriptor descriptor;
        ALMetalTemporalScalerRequirements requirements;
        id<MTLFXTemporalScaler> scaler = nil;
        // Scene is linear HDR, not pre-exposed by the renderer. An explicit
        // immutable one avoids depending on nil-exposure vendor behavior.
        std::unique_ptr<ALMetalTexture> unit_exposure;
    };
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    const ALMetalDevice* typed_device = nullptr;
    ALMetalTemporalScalerSupport support;
    std::uint32_t max_dimension = 0;
    std::shared_ptr<Entry> current;
    ALMetalTemporalScalerStats counters;
    Failure last_failure = Failure::none;
    bool needs_reset = true;
    Ticket next_ticket = 1, encoded_ticket = 0;
    std::shared_ptr<const ALMetalCommandCompletion> encoded_completion;
    // Existing main-group admission bounds outstanding commands. Tokens are
    // retired on the CPU owner without imposing another frame-admission limit.
    // Capacity is reserved BEFORE encoding, so submitted() cannot allocate.
    std::vector<std::shared_ptr<const ALMetalCommandCompletion>> submitted;

    bool fail(Failure reason, const char* text, std::string* error)
    {
        last_failure = reason;
        if (error) *error = text;
        return false;
    }
    void invalidate() noexcept
    {
        needs_reset = true;
        counters.history_valid = false;
    }
    void poll() noexcept
    {
        std::size_t keep = 0;
        for (std::size_t index = 0; index < submitted.size(); ++index)
        {
            const auto& token = submitted[index];
            const auto status = token->status();
            if (status == ALMetalCommandStatus::error || status == ALMetalCommandStatus::unknown)
            {
                ++counters.gpu_failures;
                invalidate();
            }
            else if (!token->isComplete()) submitted[keep++] = token;
        }
        submitted.resize(keep);
    }
};

ALMetalTemporalScalerSupport ALMetalTemporalScaler::support(const ALMetalDevice& device) noexcept
{
    ALMetalTemporalScalerSupport result;
    @try
    {
        if (@available(macOS 13.0, *))
        {
            auto native = (__bridge id<MTLDevice>)device.nativeHandle();
            result.supported = native && [MTLFXTemporalScalerDescriptor supportsDevice:native];
            if (result.supported)
            {
                result.minimum_upscale_factor = 1.f;
                result.maximum_upscale_factor = 2.f;
                if (@available(macOS 14.0, *))
                {
                    result.minimum_upscale_factor = [MTLFXTemporalScalerDescriptor supportedInputContentMinScaleForDevice:native];
                    result.maximum_upscale_factor = [MTLFXTemporalScalerDescriptor supportedInputContentMaxScaleForDevice:native];
                }
                if (@available(macOS 14.4, *)) result.reactive_mask = true;
            }
        }
    }
    @catch (NSException*) { return {}; }
    return result;
}

std::unique_ptr<ALMetalTemporalScaler> ALMetalTemporalScaler::create(
    const ALMetalDevice& device, std::string* error)
{
    const auto capabilities = support(device);
    if (!capabilities.supported)
    {
        if (error) *error = "Classic MetalFX temporal reconstruction is unavailable on this device";
        return nullptr;
    }
    try
    {
        auto impl = std::make_unique<Impl>();
        impl->device = (__bridge id<MTLDevice>)device.nativeHandle();
        impl->queue = (__bridge id<MTLCommandQueue>)device.nativeCommandQueue();
        impl->typed_device = &device;
        impl->support = capabilities;
        impl->max_dimension = device.info().max_texture_2d_dimension;
        impl->submitted.reserve(8);
        return std::unique_ptr<ALMetalTemporalScaler>(new ALMetalTemporalScaler(std::move(impl)));
    }
    catch (const std::bad_alloc&)
    {
        if (error) *error = "Could not allocate the Temporal history owner";
        return nullptr;
    }
}

ALMetalTemporalScaler::ALMetalTemporalScaler(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
ALMetalTemporalScaler::~ALMetalTemporalScaler() = default;

bool ALMetalTemporalScaler::prepare(const ALMetalTemporalScalerDescriptor& desc,
    ALMetalTemporalScalerRequirements& requirements, std::string* error)
{
    requirements = {};
    auto& impl = *mImpl;
    impl.poll();
    if (impl.encoded_ticket)
        return impl.fail(Failure::transaction_pending, "Resolve the encoded Temporal transaction before preparing another world", error);
    if (impl.current && impl.current->descriptor == desc)
    {
        requirements = impl.current->requirements;
        ++impl.counters.prepare_reuses;
        impl.last_failure = Failure::none;
        return true;
    }
    const float sx = desc.input_width ? float(desc.output_width) / float(desc.input_width) : 0.f;
    const float sy = desc.input_height ? float(desc.output_height) / float(desc.input_height) : 0.f;
    const bool dimensions = desc.input_width && desc.input_height && desc.output_width && desc.output_height &&
        desc.input_width <= desc.output_width && desc.input_height <= desc.output_height &&
        desc.output_width <= impl.max_dimension && desc.output_height <= impl.max_dimension &&
        sx + 1.e-6f >= impl.support.minimum_upscale_factor && sy + 1.e-6f >= impl.support.minimum_upscale_factor &&
        sx <= impl.support.maximum_upscale_factor + 1.e-6f && sy <= impl.support.maximum_upscale_factor + 1.e-6f;
    if (!dimensions)
        return impl.fail(Failure::invalid_descriptor, "Temporal dimensions exceed this device's supported scale/texture range", error);
    if (desc.color_format != ALMetalPixelFormat::rgba16_float || desc.output_format != ALMetalPixelFormat::rgba16_float ||
        desc.motion_format != ALMetalPixelFormat::rg16_float ||
        (desc.depth_format != ALMetalPixelFormat::r32_float && desc.depth_format != ALMetalPixelFormat::depth32_float))
        return impl.fail(Failure::unsupported_format, "Temporal requires linear HDR RGBA16Float, RG16Float motion and R32Float/Depth32Float depth", error);
    if (desc.reactive_mask && !impl.support.reactive_mask)
        return impl.fail(Failure::unsupported_device, "This OS does not support the required Temporal reactive mask", error);
    try
    {
        auto entry = std::make_shared<Impl::Entry>();
        entry->descriptor = desc;
        @try
        {
            auto native = [MTLFXTemporalScalerDescriptor new];
            native.inputWidth = desc.input_width; native.inputHeight = desc.input_height;
            native.outputWidth = desc.output_width; native.outputHeight = desc.output_height;
            native.colorTextureFormat = nativeFormat(desc.color_format);
            native.depthTextureFormat = nativeFormat(desc.depth_format);
            native.motionTextureFormat = nativeFormat(desc.motion_format);
            native.outputTextureFormat = nativeFormat(desc.output_format);
            native.autoExposureEnabled = NO;
            native.requiresSynchronousInitialization = YES;
            native.inputContentPropertiesEnabled = NO;
            if (@available(macOS 14.4, *))
            {
                native.reactiveMaskTextureEnabled = desc.reactive_mask;
                native.reactiveMaskTextureFormat = desc.reactive_mask ? MTLPixelFormatR8Unorm : MTLPixelFormatInvalid;
            }
            ++impl.counters.creation_attempts;
            entry->scaler = [native newTemporalScalerWithDevice:impl.device];
        }
        @catch (NSException*) { entry->scaler = nil; }
        if (!entry->scaler)
            return impl.fail(Failure::creation_failed, "MetalFX rejected the Temporal scaler descriptor", error);
        auto& req = entry->requirements;
        req.native_color_usage = entry->scaler.colorTextureUsage;
        req.native_depth_usage = entry->scaler.depthTextureUsage;
        req.native_motion_usage = entry->scaler.motionTextureUsage;
        req.native_output_usage = entry->scaler.outputTextureUsage;
        if (desc.reactive_mask) req.native_reactive_usage = entry->scaler.reactiveTextureUsage;
        constexpr auto known = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
        if ((req.native_color_usage | req.native_depth_usage | req.native_motion_usage |
             req.native_output_usage | req.native_reactive_usage) & ~known)
            return impl.fail(Failure::unsupported_usage, "Temporal requires texture usages unavailable in the typed resource factory", error);
        req.color_usage = typedUsage(static_cast<MTLTextureUsage>(req.native_color_usage));
        req.depth_usage = typedUsage(static_cast<MTLTextureUsage>(req.native_depth_usage));
        req.motion_usage = typedUsage(static_cast<MTLTextureUsage>(req.native_motion_usage));
        req.reactive_usage = typedUsage(static_cast<MTLTextureUsage>(req.native_reactive_usage));
        req.output_usage = typedUsage(static_cast<MTLTextureUsage>(req.native_output_usage));
        ALMetalTextureDescriptor exposure;
        exposure.width = exposure.height = 1;
        exposure.format = ALMetalPixelFormat::r16_float;
        exposure.usage = ALMetalTextureUsage::shader_read;
        exposure.storage = ALMetalStorageMode::shared;
        exposure.label = "Temporal immutable unit exposure";
        entry->unit_exposure = ALMetalTexture::create(*impl.typed_device, exposure, error);
        if (!entry->unit_exposure)
            return impl.fail(Failure::creation_failed, "Could not allocate Temporal unit exposure", error);
        // CPU initializes immutable shared storage exactly once, before any
        // command can reference it. No GPU upload command or wait is required.
        const __fp16 one = 1.f;
        [(__bridge id<MTLTexture>)entry->unit_exposure->nativeHandle()
            replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&one bytesPerRow:sizeof(one)];
        impl.current = std::move(entry);
        impl.invalidate();
        ++impl.counters.creation_successes;
        requirements = impl.current->requirements;
        impl.last_failure = Failure::none;
        return true;
    }
    catch (const std::bad_alloc&)
    { return impl.fail(Failure::creation_failed, "Could not allocate Temporal scaler storage", error); }
}

bool ALMetalTemporalScaler::encode(ALMetalCommandBuffer& command,
    const ALMetalTemporalScalerDescriptor& desc, const ALMetalTemporalScalerInputs& input,
    const ALMetalTemporalScalerFrame& frame, Ticket& ticket, std::string* error)
{
    ticket = 0;
    ALMetalTemporalScalerRequirements requirements;
    if (!prepare(desc, requirements, error)) return false;
    auto& impl = *mImpl;
    const auto entry = impl.current;
    if (!std::isfinite(frame.jitter_x) || !std::isfinite(frame.jitter_y) ||
        std::abs(frame.jitter_x) > .5f || std::abs(frame.jitter_y) > .5f ||
        !std::isfinite(frame.pre_exposure) || frame.pre_exposure <= 0.f)
        return impl.fail(Failure::invalid_descriptor, "Temporal jitter/exposure inputs must be finite and within their documented range", error);
    auto native_command = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
    if (command.isCommitted() || command.nativeDevice() != (__bridge void*)impl.device ||
        !native_command || native_command.commandQueue != impl.queue || !native_command.retainedReferences)
        return impl.fail(Failure::incompatible_command, "Temporal requires the owner's uncommitted retained-resource queue command", error);
    const auto& req = requirements;
    if (!compatible(input.color, impl.device, desc.input_width, desc.input_height, desc.color_format, req.native_color_usage) ||
        !compatible(input.depth, impl.device, desc.input_width, desc.input_height, desc.depth_format, req.native_depth_usage) ||
        !compatible(input.motion, impl.device, desc.input_width, desc.input_height, desc.motion_format, req.native_motion_usage) ||
        !compatible(input.output, impl.device, desc.output_width, desc.output_height, desc.output_format, req.native_output_usage, true) ||
        (desc.reactive_mask && !compatible(input.reactive, impl.device, desc.input_width, desc.input_height,
             ALMetalPixelFormat::r8_unorm, req.native_reactive_usage)))
        return impl.fail(Failure::incompatible_texture, "Temporal color/depth/motion/reactive/output backing is missing or incompatible", error);
    const auto output_handle = input.output->nativeHandle();
    if (output_handle == input.color->nativeHandle() || output_handle == input.depth->nativeHandle() ||
        output_handle == input.motion->nativeHandle() || (input.reactive && output_handle == input.reactive->nativeHandle()))
        return impl.fail(Failure::incompatible_texture, "Temporal input/output textures must not alias", error);
    const auto completion = command.completionToken();
    if (!completion || !command.retainUntilCompletion(entry, error))
        return impl.fail(Failure::retention_failed, "Could not retain the Temporal scaler and completion identity", error);
    try
    { impl.submitted.reserve(impl.submitted.size() + 1); }
    catch (const std::bad_alloc&)
    { return impl.fail(Failure::retention_failed, "Could not reserve Temporal completion observation before encoding", error); }
    ResourcePass pass(command, error);
    if (!pass.active ||
        !command.useResource(input.color->resourceView(), RP::Access::read, error) ||
        !command.useResource(input.depth->resourceView(), RP::Access::read, error) ||
        !command.useResource(input.motion->resourceView(), RP::Access::read, error) ||
        !command.useResource(entry->unit_exposure->resourceView(), RP::Access::read, error) ||
        (desc.reactive_mask && !command.useResource(input.reactive->resourceView(), RP::Access::read, error)) ||
        !command.useResource(input.output->resourceView(), RP::Access::write, error))
        return impl.fail(Failure::resource_tracking_failed, "Could not declare Temporal external resource accesses", error);
    const bool reset = frame.reset || impl.needs_reset || !impl.counters.history_valid;
    @try
    {
        entry->scaler.colorTexture = (__bridge id<MTLTexture>)input.color->nativeHandle();
        entry->scaler.depthTexture = (__bridge id<MTLTexture>)input.depth->nativeHandle();
        entry->scaler.motionTexture = (__bridge id<MTLTexture>)input.motion->nativeHandle();
        entry->scaler.outputTexture = (__bridge id<MTLTexture>)input.output->nativeHandle();
        entry->scaler.exposureTexture = (__bridge id<MTLTexture>)entry->unit_exposure->nativeHandle();
        if (desc.reactive_mask) entry->scaler.reactiveMaskTexture = (__bridge id<MTLTexture>)input.reactive->nativeHandle();
        entry->scaler.inputContentWidth = desc.input_width; entry->scaler.inputContentHeight = desc.input_height;
        entry->scaler.preExposure = frame.pre_exposure;
        entry->scaler.jitterOffsetX = frame.jitter_x; entry->scaler.jitterOffsetY = frame.jitter_y;
        entry->scaler.motionVectorScaleX = 1.f; entry->scaler.motionVectorScaleY = 1.f;
        entry->scaler.depthReversed = frame.depth_reversed;
        entry->scaler.reset = reset;
        [entry->scaler encodeToCommandBuffer:native_command];
    }
    @catch (NSException*)
    {
        impl.invalidate();
        return impl.fail(Failure::encoding_failed, "Temporal encoding raised a native exception; abandon this world", error);
    }
    @finally
    {
        entry->scaler.colorTexture = nil; entry->scaler.depthTexture = nil;
        entry->scaler.motionTexture = nil; entry->scaler.outputTexture = nil;
        entry->scaler.exposureTexture = nil; entry->scaler.reactiveMaskTexture = nil;
    }
    if (!pass.end(error))
    {
        impl.invalidate();
        return impl.fail(Failure::resource_tracking_failed, "Temporal resource pass failed; abandon this world", error);
    }
    impl.encoded_ticket = impl.next_ticket++;
    if (!impl.next_ticket) ++impl.next_ticket;
    impl.encoded_completion = completion;
    ticket = impl.encoded_ticket;
    ++impl.counters.encoded_calls;
    if (reset) ++impl.counters.reset_calls;
    impl.last_failure = Failure::none;
    return true;
}

bool ALMetalTemporalScaler::submitted(Ticket ticket, std::string* error)
{
    auto& impl = *mImpl;
    if (!ticket || ticket != impl.encoded_ticket || !impl.encoded_completion)
        return impl.fail(Failure::invalid_transaction, "Temporal submission ticket does not match the encoded world", error);
    const auto status = impl.encoded_completion->status();
    if (status == ALMetalCommandStatus::not_enqueued || status == ALMetalCommandStatus::enqueued ||
        status == ALMetalCommandStatus::unknown)
        return impl.fail(Failure::invalid_transaction, "Temporal history cannot advance before actual command submission", error);
    impl.submitted.push_back(std::move(impl.encoded_completion));
    impl.encoded_ticket = 0;
    impl.needs_reset = false;
    impl.counters.history_valid = true;
    ++impl.counters.submitted_calls;
    impl.poll();
    impl.last_failure = Failure::none;
    return true;
}

void ALMetalTemporalScaler::abandon(Ticket ticket) noexcept
{
    auto& impl = *mImpl;
    if (ticket && ticket == impl.encoded_ticket)
    {
        impl.encoded_ticket = 0;
        impl.encoded_completion.reset();
        ++impl.counters.abandoned_calls;
        impl.invalidate();
    }
}

void ALMetalTemporalScaler::invalidate() noexcept { mImpl->invalidate(); }
ALMetalTemporalScalerFailure ALMetalTemporalScaler::lastFailure() const noexcept { return mImpl->last_failure; }
ALMetalTemporalScalerStats ALMetalTemporalScaler::stats() const noexcept
{
    auto result = mImpl->counters;
    result.pending_submissions = static_cast<std::uint32_t>(mImpl->submitted.size());
    return result;
}
