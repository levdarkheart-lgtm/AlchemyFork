/**
 * @file almetalspatialscaler.mm
 * @brief Cached classic MetalFX spatial scaling with explicit caller ownership.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "almetalspatialscaler.h"

#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

#include <array>
#include <new>
#include <utility>

namespace
{
namespace RP = ALMetalResourcePlanning;
using Failure = ALMetalSpatialScalerFailure;

MTLPixelFormat nativeFormat(ALMetalPixelFormat format) noexcept
{
    switch (format)
    {
    case ALMetalPixelFormat::rgba8_unorm: return MTLPixelFormatRGBA8Unorm;
    case ALMetalPixelFormat::bgra8_unorm: return MTLPixelFormatBGRA8Unorm;
    case ALMetalPixelFormat::rgb10a2_unorm: return MTLPixelFormatRGB10A2Unorm;
    case ALMetalPixelFormat::rgba16_float: return MTLPixelFormatRGBA16Float;
    default: return MTLPixelFormatInvalid;
    }
}

ALMetalTextureUsage typedUsage(MTLTextureUsage value) noexcept
{
    auto result = ALMetalTextureUsage::none;
    if (value & MTLTextureUsageShaderRead)
        result = result | ALMetalTextureUsage::shader_read;
    if (value & MTLTextureUsageShaderWrite)
        result = result | ALMetalTextureUsage::shader_write;
    if (value & MTLTextureUsageRenderTarget)
        result = result | ALMetalTextureUsage::render_target;
    return result;
}

constexpr std::array identitySwizzle{
    ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::green,
    ALMetalTextureSwizzle::blue, ALMetalTextureSwizzle::alpha};

bool compatibleTexture(const ALMetalTexture& texture, id<MTLTexture> native,
    id<MTLDevice> device, std::uint32_t width, std::uint32_t height,
    ALMetalPixelFormat format, MTLTextureUsage usage, bool output) noexcept
{
    const auto& desc = texture.descriptor();
    return native && native.device == device &&
        desc.type == ALMetalTextureType::texture_2d &&
        native.textureType == MTLTextureType2D &&
        desc.width == width && desc.height == height &&
        native.width == width && native.height == height &&
        desc.format == format && native.pixelFormat == nativeFormat(format) &&
        desc.sample_count == 1 && native.sampleCount == 1 &&
        desc.array_length == 1 && native.arrayLength == 1 &&
        desc.mipmap_level_count == 1 && native.mipmapLevelCount == 1 &&
        desc.swizzle == identitySwizzle &&
        desc.hazard_tracking == ALMetalHazardTrackingMode::tracked &&
        native.hazardTrackingMode == MTLHazardTrackingModeTracked &&
        desc.storage != ALMetalStorageMode::memoryless &&
        native.storageMode != MTLStorageModeMemoryless &&
        (!output || (desc.storage == ALMetalStorageMode::private_gpu &&
                     native.storageMode == MTLStorageModePrivate)) &&
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

struct ALMetalSpatialScaler::Impl
{
    struct Entry
    {
        ALMetalSpatialScalerDescriptor descriptor;
        ALMetalSpatialScalerRequirements requirements;
        id<MTLFXSpatialScaler> scaler = nil;
        Failure failure = Failure::none;
        std::string error;
    };
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    std::uint32_t max_dimension = 0;
    std::array<std::shared_ptr<Entry>, 4> entries;
    ALMetalSpatialScalerStats counters;
    Failure last_failure = Failure::none;

    bool fail(Failure reason, const char* text, std::string* error)
    {
        last_failure = reason;
        if (error) *error = text;
        return false;
    }

    std::shared_ptr<Entry> entry(const ALMetalSpatialScalerDescriptor& desc)
    {
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            if (entries[index] && entries[index]->descriptor == desc)
            {
                ++counters.cache_hits;
                auto result = entries[index];
                for (; index; --index) entries[index] = std::move(entries[index - 1]);
                entries[0] = result;
                return result;
            }
        }
        auto result = std::make_shared<Entry>();
        result->descriptor = desc;
        const auto reject = [&](Failure failure, const char* text)
        { result->failure = failure; result->error = text; };
        const bool dimensions = desc.input_width && desc.input_height &&
            desc.output_width >= desc.input_width &&
            desc.output_height >= desc.input_height &&
            (desc.output_width > desc.input_width || desc.output_height > desc.input_height) &&
            (!max_dimension || (desc.output_width <= max_dimension &&
                                desc.output_height <= max_dimension));
        if (!dimensions || desc.color_mode != ALMetalSpatialScalerColorMode::perceptual)
            reject(Failure::invalid_descriptor,
                "MetalFX spatial scaling requires valid upscaled dimensions and explicit perceptual color");
        else if (nativeFormat(desc.input_format) == MTLPixelFormatInvalid ||
                 nativeFormat(desc.output_format) == MTLPixelFormatInvalid)
            reject(Failure::unsupported_format,
                "MetalFX spatial scaling requires a supported non-sRGB color representation");
        else
        {
            @autoreleasepool
            {
                auto native = [MTLFXSpatialScalerDescriptor new];
                native.inputWidth = desc.input_width;
                native.inputHeight = desc.input_height;
                native.outputWidth = desc.output_width;
                native.outputHeight = desc.output_height;
                native.colorTextureFormat = nativeFormat(desc.input_format);
                native.outputTextureFormat = nativeFormat(desc.output_format);
                native.colorProcessingMode = MTLFXSpatialScalerColorProcessingModePerceptual;
                ++counters.creation_attempts;
                @try
                {
                    result->scaler = [native newSpatialScalerWithDevice:device];
                }
                @catch (NSException*)
                {
                    result->scaler = nil;
                }
                if (!result->scaler)
                    reject(Failure::creation_failed,
                        "MetalFX rejected the spatial scaler format/dimension descriptor");
                else
                {
                    ++counters.creation_successes;
                    const auto input = result->scaler.colorTextureUsage;
                    const auto output = result->scaler.outputTextureUsage;
                    result->requirements.input_usage = typedUsage(input);
                    result->requirements.output_usage = typedUsage(output);
                    result->requirements.native_input_usage = input;
                    result->requirements.native_output_usage = output;
                    constexpr auto known = MTLTextureUsageShaderRead |
                        MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
                    if (((input | output) & ~known) != 0)
                        reject(Failure::unsupported_usage,
                            "MetalFX requires texture usage bits unavailable in the typed texture factory");
                    else if (result->scaler.inputWidth != desc.input_width ||
                        result->scaler.inputHeight != desc.input_height ||
                        result->scaler.outputWidth != desc.output_width ||
                        result->scaler.outputHeight != desc.output_height ||
                        result->scaler.colorTextureFormat != nativeFormat(desc.input_format) ||
                        result->scaler.outputTextureFormat != nativeFormat(desc.output_format) ||
                        result->scaler.colorProcessingMode != MTLFXSpatialScalerColorProcessingModePerceptual)
                        reject(Failure::creation_failed,
                            "MetalFX spatial scaler did not preserve the requested descriptor");
                }
            }
        }
        for (std::size_t index = entries.size() - 1; index; --index)
            entries[index] = std::move(entries[index - 1]);
        entries[0] = result;
        return result;
    }
};

bool ALMetalSpatialScaler::supported(const ALMetalDevice& device) noexcept
{
    @try
    {
        if (@available(macOS 13.0, *))
        {
            auto native = (__bridge id<MTLDevice>)device.nativeHandle();
            return native && [MTLFXSpatialScalerDescriptor supportsDevice:native];
        }
    }
    @catch (NSException*) {}
    return false;
}

std::unique_ptr<ALMetalSpatialScaler> ALMetalSpatialScaler::create(
    const ALMetalDevice& device, std::string* error)
{
    if (!supported(device))
    {
        if (error) *error = "Classic MetalFX spatial scaling is unavailable on this device";
        return nullptr;
    }
    try
    {
        auto impl = std::make_unique<Impl>();
        impl->device = (__bridge id<MTLDevice>)device.nativeHandle();
        impl->queue = (__bridge id<MTLCommandQueue>)device.nativeCommandQueue();
        impl->max_dimension = device.info().max_texture_2d_dimension;
        return std::unique_ptr<ALMetalSpatialScaler>(new ALMetalSpatialScaler(std::move(impl)));
    }
    catch (const std::bad_alloc&)
    {
        if (error) *error = "Could not allocate the MetalFX spatial scaler owner";
        return nullptr;
    }
}

ALMetalSpatialScaler::ALMetalSpatialScaler(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
ALMetalSpatialScaler::~ALMetalSpatialScaler() = default;

bool ALMetalSpatialScaler::prepare(const ALMetalSpatialScalerDescriptor& desc,
    ALMetalSpatialScalerRequirements& requirements, std::string* error)
{
    requirements = {};
    try
    {
        const auto entry = mImpl->entry(desc);
        requirements = entry->requirements;
        if (entry->failure != Failure::none)
            return mImpl->fail(entry->failure, entry->error.c_str(), error);
        mImpl->last_failure = Failure::none;
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return mImpl->fail(Failure::creation_failed,
            "Could not allocate a cached MetalFX spatial scaler entry", error);
    }
}

bool ALMetalSpatialScaler::encode(ALMetalCommandBuffer& command,
    const ALMetalSpatialScalerDescriptor& desc, const ALMetalTexture& input,
    const ALMetalTexture& output, std::string* error)
{
    ALMetalSpatialScalerRequirements requirements;
    if (!prepare(desc, requirements, error)) return false;
    const auto entry = mImpl->entries[0];
    if (command.isCommitted() || command.nativeDevice() != (__bridge void*)mImpl->device)
        return mImpl->fail(Failure::incompatible_command,
            "MetalFX spatial scaling requires this device's uncommitted command", error);
    // The effect can reuse private scratch between encodes. Preserve its
    // existing ordered queue; device equality alone does not establish that.
    // This also marks the hidden MetalFX closure opaque before any encoding.
    auto native_command = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
    if (!native_command || native_command.commandQueue != mImpl->queue ||
        !native_command.retainedReferences)
        return mImpl->fail(Failure::incompatible_command,
            "MetalFX spatial scaling requires the owner's retained-resource command queue", error);
    auto source = (__bridge id<MTLTexture>)input.nativeHandle();
    auto destination = (__bridge id<MTLTexture>)output.nativeHandle();
    if (source == destination || !compatibleTexture(input, source, mImpl->device,
            desc.input_width, desc.input_height, desc.input_format,
            static_cast<MTLTextureUsage>(requirements.native_input_usage), false) ||
        !compatibleTexture(output, destination, mImpl->device,
            desc.output_width, desc.output_height, desc.output_format,
            static_cast<MTLTextureUsage>(requirements.native_output_usage), true))
        return mImpl->fail(Failure::incompatible_texture,
            "MetalFX input/output backing, dimensions, formats, tracking, storage or usages are incompatible", error);
    try
    {
        if (!command.retainUntilCompletion(entry, error))
            return mImpl->fail(Failure::retention_failed,
                "Could not retain the exact MetalFX scaler through command completion", error);
    }
    catch (const std::bad_alloc&)
    {
        return mImpl->fail(Failure::retention_failed,
            "Could not allocate MetalFX command lifetime retention", error);
    }
    ResourcePass pass(command, error);
    if (!pass.active || !command.useResource(input.resourceView(), RP::Access::read, error) ||
        !command.useResource(output.resourceView(), RP::Access::write, error))
        return mImpl->fail(Failure::resource_tracking_failed,
            "Could not declare the MetalFX external backing accesses", error);
    @try
    {
        entry->scaler.colorTexture = source;
        entry->scaler.inputContentWidth = desc.input_width;
        entry->scaler.inputContentHeight = desc.input_height;
        entry->scaler.outputTexture = destination;
        [entry->scaler encodeToCommandBuffer:native_command];
    }
    @catch (NSException*)
    {
        entry->scaler.colorTexture = nil;
        entry->scaler.outputTexture = nil;
        return mImpl->fail(Failure::encoding_failed,
            "MetalFX spatial encoding raised a native exception; abandon this command", error);
    }
    // Encoded native commands retain the referenced resources. Cached effects
    // need not keep the latest image alive while idle. Caller pool leases still
    // govern generation reuse and must live through the enclosing transaction.
    entry->scaler.colorTexture = nil;
    entry->scaler.outputTexture = nil;
    if (!pass.end(error))
        return mImpl->fail(Failure::resource_tracking_failed,
            "MetalFX external resource pass could not finish; abandon this command", error);
    ++mImpl->counters.encoded_calls;
    mImpl->last_failure = Failure::none;
    return true;
}

ALMetalSpatialScalerFailure ALMetalSpatialScaler::lastFailure() const noexcept
{ return mImpl->last_failure; }

ALMetalSpatialScalerStats ALMetalSpatialScaler::stats() const noexcept
{
    auto result = mImpl->counters;
    for (const auto& entry : mImpl->entries)
        if (entry)
        {
            ++result.cached_entries;
            result.cached_failures += entry->failure != Failure::none;
        }
    return result;
}
