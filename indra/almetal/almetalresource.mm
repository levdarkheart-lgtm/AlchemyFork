/**
 * @file almetalresource.mm
 * @brief Typed Metal buffers, textures, render targets and transfer paths.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetalresource.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
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

    NSString* toNSString(const std::string& value)
    {
        return value.empty() ? nil : [NSString stringWithUTF8String:value.c_str()];
    }

    MTLStorageMode storageMode(ALMetalStorageMode mode)
    {
        switch (mode)
        {
            case ALMetalStorageMode::shared:      return MTLStorageModeShared;
            case ALMetalStorageMode::private_gpu: return MTLStorageModePrivate;
            case ALMetalStorageMode::memoryless:  return MTLStorageModeMemoryless;
        }
        return MTLStorageModePrivate;
    }

    MTLResourceOptions resourceOptions(ALMetalStorageMode storage,
                                       ALMetalHazardTrackingMode hazards,
                                       ALMetalCpuCacheMode cache)
    {
        MTLResourceOptions options = static_cast<MTLResourceOptions>(storageMode(storage) << MTLResourceStorageModeShift);
        if (hazards == ALMetalHazardTrackingMode::untracked)
        {
            options |= MTLResourceHazardTrackingModeUntracked;
        }
        else
        {
            options |= MTLResourceHazardTrackingModeTracked;
        }
        if (cache == ALMetalCpuCacheMode::write_combined)
        {
            options |= MTLResourceCPUCacheModeWriteCombined;
        }
        return options;
    }

    bool checkedRange(std::size_t total, std::size_t offset, std::size_t length,
                      std::string* error)
    {
        if (length == 0)
        {
            setError(error, "Metal transfer length must be non-zero");
            return false;
        }
        if (offset > total || length > total - offset)
        {
            setError(error, "Metal transfer range exceeds the resource");
            return false;
        }
        return true;
    }

    MTLPixelFormat pixelFormat(ALMetalPixelFormat format)
    {
        switch (format)
        {
            case ALMetalPixelFormat::r8_unorm:               return MTLPixelFormatR8Unorm;
            case ALMetalPixelFormat::rg8_unorm:              return MTLPixelFormatRG8Unorm;
            case ALMetalPixelFormat::rgba8_unorm:            return MTLPixelFormatRGBA8Unorm;
            case ALMetalPixelFormat::rgba8_unorm_srgb:       return MTLPixelFormatRGBA8Unorm_sRGB;
            case ALMetalPixelFormat::bgra8_unorm:            return MTLPixelFormatBGRA8Unorm;
            case ALMetalPixelFormat::bgra8_unorm_srgb:       return MTLPixelFormatBGRA8Unorm_sRGB;
            case ALMetalPixelFormat::r16_float:              return MTLPixelFormatR16Float;
            case ALMetalPixelFormat::rg16_float:             return MTLPixelFormatRG16Float;
            case ALMetalPixelFormat::rgba16_float:           return MTLPixelFormatRGBA16Float;
            case ALMetalPixelFormat::r32_float:              return MTLPixelFormatR32Float;
            case ALMetalPixelFormat::depth32_float:          return MTLPixelFormatDepth32Float;
            case ALMetalPixelFormat::depth32_float_stencil8: return MTLPixelFormatDepth32Float_Stencil8;
            case ALMetalPixelFormat::invalid:                return MTLPixelFormatInvalid;
        }
        return MTLPixelFormatInvalid;
    }

    std::size_t formatBytesPerPixel(ALMetalPixelFormat format)
    {
        switch (format)
        {
            case ALMetalPixelFormat::r8_unorm:               return 1;
            case ALMetalPixelFormat::rg8_unorm:              return 2;
            case ALMetalPixelFormat::rgba8_unorm:
            case ALMetalPixelFormat::rgba8_unorm_srgb:
            case ALMetalPixelFormat::bgra8_unorm:
            case ALMetalPixelFormat::bgra8_unorm_srgb:
            case ALMetalPixelFormat::r32_float:
            case ALMetalPixelFormat::depth32_float:          return 4;
            case ALMetalPixelFormat::r16_float:              return 2;
            case ALMetalPixelFormat::rg16_float:             return 4;
            case ALMetalPixelFormat::rgba16_float:           return 8;
            case ALMetalPixelFormat::depth32_float_stencil8: return 8;
            case ALMetalPixelFormat::invalid:                return 0;
        }
        return 0;
    }

    MTLTextureType textureType(ALMetalTextureType type)
    {
        switch (type)
        {
            case ALMetalTextureType::texture_2d:             return MTLTextureType2D;
            case ALMetalTextureType::texture_2d_array:       return MTLTextureType2DArray;
            case ALMetalTextureType::texture_cube:           return MTLTextureTypeCube;
            case ALMetalTextureType::texture_2d_multisample: return MTLTextureType2DMultisample;
        }
        return MTLTextureType2D;
    }

    MTLTextureUsage textureUsage(ALMetalTextureUsage usage)
    {
        MTLTextureUsage native_usage = MTLTextureUsageUnknown;
        if (alMetalHasUsage(usage, ALMetalTextureUsage::shader_read))
        {
            native_usage |= MTLTextureUsageShaderRead;
        }
        if (alMetalHasUsage(usage, ALMetalTextureUsage::shader_write))
        {
            native_usage |= MTLTextureUsageShaderWrite;
        }
        if (alMetalHasUsage(usage, ALMetalTextureUsage::render_target))
        {
            native_usage |= MTLTextureUsageRenderTarget;
        }
        return native_usage;
    }

    std::uint32_t mipDimension(std::uint32_t dimension, std::uint32_t level)
    {
        return std::max(1u, dimension >> level);
    }

    std::uint32_t maxMipLevels(std::uint32_t width, std::uint32_t height)
    {
        std::uint32_t levels = 1;
        for (std::uint32_t dimension = std::max(width, height); dimension > 1; dimension >>= 1)
        {
            ++levels;
        }
        return levels;
    }

    std::uint32_t sliceCount(const ALMetalTextureDescriptor& descriptor)
    {
        switch (descriptor.type)
        {
            case ALMetalTextureType::texture_2d_array: return descriptor.array_length;
            case ALMetalTextureType::texture_cube:     return 6;
            default:                                   return 1;
        }
    }

    bool normalizedRegion(const ALMetalTextureDescriptor& descriptor,
                          std::uint32_t mip_level,
                          std::uint32_t slice,
                          ALMetalRegion2D* region,
                          std::string* error)
    {
        if (mip_level >= descriptor.mipmap_level_count)
        {
            setError(error, "Metal texture mip level is out of range");
            return false;
        }
        if (slice >= sliceCount(descriptor))
        {
            setError(error, "Metal texture slice is out of range");
            return false;
        }
        const std::uint32_t mip_width = mipDimension(descriptor.width, mip_level);
        const std::uint32_t mip_height = mipDimension(descriptor.height, mip_level);
        if (region->width == 0)
        {
            region->width = mip_width;
        }
        if (region->height == 0)
        {
            region->height = mip_height;
        }
        if (region->x >= mip_width || region->y >= mip_height ||
            region->width > mip_width - region->x || region->height > mip_height - region->y)
        {
            setError(error, "Metal texture region exceeds the selected mip level");
            return false;
        }
        return true;
    }

    std::size_t alignUp(std::size_t value, std::size_t alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    MTLLoadAction loadAction(ALMetalLoadAction action)
    {
        switch (action)
        {
            case ALMetalLoadAction::dont_care: return MTLLoadActionDontCare;
            case ALMetalLoadAction::load:      return MTLLoadActionLoad;
            case ALMetalLoadAction::clear:     return MTLLoadActionClear;
        }
        return MTLLoadActionDontCare;
    }

    MTLStoreAction storeAction(ALMetalStoreAction action)
    {
        switch (action)
        {
            case ALMetalStoreAction::dont_care:                    return MTLStoreActionDontCare;
            case ALMetalStoreAction::store:                        return MTLStoreActionStore;
            case ALMetalStoreAction::multisample_resolve:          return MTLStoreActionMultisampleResolve;
            case ALMetalStoreAction::store_and_multisample_resolve: return MTLStoreActionStoreAndMultisampleResolve;
        }
        return MTLStoreActionDontCare;
    }

    bool isResolveAction(ALMetalStoreAction action)
    {
        return action == ALMetalStoreAction::multisample_resolve ||
               action == ALMetalStoreAction::store_and_multisample_resolve;
    }
}

struct ALMetalBuffer::Impl
{
    id<MTLDevice> device = nil;
    id<MTLBuffer> buffer = nil;
};

ALMetalBuffer::ALMetalBuffer(std::unique_ptr<Impl> impl, ALMetalBufferDescriptor descriptor)
: mImpl(std::move(impl)),
  mDescriptor(std::move(descriptor))
{
}

ALMetalBuffer::~ALMetalBuffer() = default;

std::unique_ptr<ALMetalBuffer> ALMetalBuffer::create(
    const ALMetalDevice& device,
    const ALMetalBufferDescriptor& descriptor,
    std::string* error)
{
    @autoreleasepool
    {
        if (!device.nativeHandle())
        {
            setError(error, "Cannot create a Metal buffer without a device");
            return nullptr;
        }
        if (descriptor.length == 0 || descriptor.length > device.info().max_buffer_length)
        {
            setError(error, "Metal buffer length is zero or exceeds the device limit");
            return nullptr;
        }
        if (descriptor.storage == ALMetalStorageMode::memoryless)
        {
            setError(error, "Metal buffers cannot use memoryless storage");
            return nullptr;
        }

        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        id<MTLBuffer> buffer = [native_device
            newBufferWithLength:descriptor.length
                       options:resourceOptions(descriptor.storage,
                                               descriptor.hazard_tracking,
                                               descriptor.cpu_cache)];
        if (!buffer)
        {
            setError(error, "Metal device could not allocate the buffer");
            return nullptr;
        }
        buffer.label = toNSString(descriptor.label);

        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->buffer = buffer;
        return std::unique_ptr<ALMetalBuffer>(new ALMetalBuffer(std::move(impl), descriptor));
    }
}

void* ALMetalBuffer::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->buffer : nullptr;
}

std::size_t ALMetalBuffer::length() const noexcept
{
    return mDescriptor.length;
}

ALMetalStorageMode ALMetalBuffer::storageMode() const noexcept
{
    return mDescriptor.storage;
}

void* ALMetalBuffer::mappedBytes() const noexcept
{
    return mImpl && mDescriptor.storage == ALMetalStorageMode::shared ? mImpl->buffer.contents : nullptr;
}

bool ALMetalBuffer::write(std::size_t offset, const void* bytes, std::size_t length,
                          std::string* error)
{
    if (!bytes)
    {
        setError(error, "Cannot write a Metal buffer from a null source");
        return false;
    }
    if (mDescriptor.storage != ALMetalStorageMode::shared)
    {
        setError(error, "Direct Metal buffer writes require shared storage");
        return false;
    }
    if (!checkedRange(mDescriptor.length, offset, length, error))
    {
        return false;
    }
    std::memcpy(static_cast<std::uint8_t*>(mImpl->buffer.contents) + offset, bytes, length);
    return true;
}

bool ALMetalBuffer::read(std::size_t offset, void* bytes, std::size_t length,
                         std::string* error) const
{
    if (!bytes)
    {
        setError(error, "Cannot read a Metal buffer into a null destination");
        return false;
    }
    if (mDescriptor.storage != ALMetalStorageMode::shared)
    {
        setError(error, "Direct Metal buffer reads require shared storage");
        return false;
    }
    if (!checkedRange(mDescriptor.length, offset, length, error))
    {
        return false;
    }
    std::memcpy(bytes, static_cast<const std::uint8_t*>(mImpl->buffer.contents) + offset, length);
    return true;
}

bool ALMetalBuffer::upload(ALMetalCommandBuffer& command_buffer,
                           std::size_t offset,
                           const void* bytes,
                           std::size_t length,
                           std::string* error)
{
    if (!bytes)
    {
        setError(error, "Cannot upload a Metal buffer from a null source");
        return false;
    }
    if (!checkedRange(mDescriptor.length, offset, length, error))
    {
        return false;
    }
    if (command_buffer.isCommitted())
    {
        setError(error, "Cannot upload with an already committed Metal command buffer");
        return false;
    }
    if (command_buffer.nativeDevice() != (__bridge void*)mImpl->device)
    {
        setError(error, "Metal buffer and command buffer use different devices");
        return false;
    }
    if (mDescriptor.storage == ALMetalStorageMode::shared)
    {
        return write(offset, bytes, length, error);
    }

    @autoreleasepool
    {
        id<MTLBuffer> staging = [mImpl->device
            newBufferWithBytes:bytes
                      length:length
                     options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
        if (!staging)
        {
            setError(error, "Metal could not allocate the buffer upload staging resource");
            return false;
        }
        id<MTLCommandBuffer> command = (__bridge id<MTLCommandBuffer>)command_buffer.nativeHandle();
        id<MTLBlitCommandEncoder> encoder = [command blitCommandEncoder];
        if (!encoder)
        {
            setError(error, "Metal could not create a buffer upload blit encoder");
            return false;
        }
        [encoder copyFromBuffer:staging
                   sourceOffset:0
                       toBuffer:mImpl->buffer
              destinationOffset:offset
                           size:length];
        [encoder endEncoding];
        return true;
    }
}

bool ALMetalBuffer::readback(const ALMetalDevice& device,
                             std::size_t offset,
                             void* bytes,
                             std::size_t length,
                             std::string* error) const
{
    if (!bytes)
    {
        setError(error, "Cannot read a Metal buffer into a null destination");
        return false;
    }
    if (!checkedRange(mDescriptor.length, offset, length, error))
    {
        return false;
    }
    if (device.nativeHandle() != (__bridge void*)mImpl->device)
    {
        setError(error, "Metal buffer and readback device do not match");
        return false;
    }
    if (mDescriptor.storage == ALMetalStorageMode::shared)
    {
        return read(offset, bytes, length, error);
    }

    @autoreleasepool
    {
        id<MTLBuffer> staging = [mImpl->device newBufferWithLength:length
                                                          options:MTLResourceStorageModeShared];
        if (!staging)
        {
            setError(error, "Metal could not allocate the buffer readback staging resource");
            return false;
        }
        auto command_buffer = device.makeCommandBuffer("Alchemy buffer readback", error);
        if (!command_buffer)
        {
            return false;
        }
        id<MTLCommandBuffer> command = (__bridge id<MTLCommandBuffer>)command_buffer->nativeHandle();
        id<MTLBlitCommandEncoder> encoder = [command blitCommandEncoder];
        if (!encoder)
        {
            setError(error, "Metal could not create a buffer readback blit encoder");
            return false;
        }
        [encoder copyFromBuffer:mImpl->buffer
                   sourceOffset:offset
                       toBuffer:staging
              destinationOffset:0
                           size:length];
        [encoder endEncoding];
        if (!command_buffer->commitAndWait(error))
        {
            return false;
        }
        std::memcpy(bytes, staging.contents, length);
        return true;
    }
}

struct ALMetalTexture::Impl
{
    id<MTLDevice> device = nil;
    id<MTLTexture> texture = nil;
};

ALMetalTexture::ALMetalTexture(std::unique_ptr<Impl> impl, ALMetalTextureDescriptor descriptor)
: mImpl(std::move(impl)),
  mDescriptor(std::move(descriptor))
{
}

ALMetalTexture::~ALMetalTexture() = default;

std::unique_ptr<ALMetalTexture> ALMetalTexture::create(
    const ALMetalDevice& device,
    const ALMetalTextureDescriptor& descriptor,
    std::string* error)
{
    @autoreleasepool
    {
        if (!device.nativeHandle())
        {
            setError(error, "Cannot create a Metal texture without a device");
            return nullptr;
        }
        if (pixelFormat(descriptor.format) == MTLPixelFormatInvalid ||
            descriptor.width == 0 || descriptor.height == 0 ||
            descriptor.array_length == 0 || descriptor.mipmap_level_count == 0)
        {
            setError(error, "Metal texture descriptor has an invalid format or dimension");
            return nullptr;
        }
        if (descriptor.mipmap_level_count > maxMipLevels(descriptor.width, descriptor.height))
        {
            setError(error, "Metal texture mip count exceeds its dimensions");
            return nullptr;
        }
        const bool multisample = descriptor.type == ALMetalTextureType::texture_2d_multisample;
        if ((multisample && (descriptor.sample_count <= 1 || descriptor.mipmap_level_count != 1 ||
                             descriptor.array_length != 1)) ||
            (!multisample && descriptor.sample_count != 1))
        {
            setError(error, "Metal texture sample count does not match its texture type");
            return nullptr;
        }
        if (descriptor.type != ALMetalTextureType::texture_2d_array && descriptor.array_length != 1)
        {
            setError(error, "Only a 2D-array Metal texture accepts array_length greater than one");
            return nullptr;
        }
        if (descriptor.storage == ALMetalStorageMode::memoryless &&
            (!alMetalHasUsage(descriptor.usage, ALMetalTextureUsage::render_target) ||
             alMetalHasUsage(descriptor.usage, ALMetalTextureUsage::shader_read) ||
             alMetalHasUsage(descriptor.usage, ALMetalTextureUsage::shader_write)))
        {
            setError(error, "Memoryless Metal textures must be render-target-only");
            return nullptr;
        }

        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        if (multisample && ![native_device supportsTextureSampleCount:descriptor.sample_count])
        {
            setError(error, "Metal device does not support the requested sample count");
            return nullptr;
        }

        MTLTextureDescriptor* native_descriptor = [[MTLTextureDescriptor alloc] init];
        native_descriptor.textureType = textureType(descriptor.type);
        native_descriptor.pixelFormat = pixelFormat(descriptor.format);
        native_descriptor.width = descriptor.width;
        native_descriptor.height = descriptor.height;
        native_descriptor.depth = 1;
        native_descriptor.arrayLength = descriptor.array_length;
        native_descriptor.mipmapLevelCount = descriptor.mipmap_level_count;
        native_descriptor.sampleCount = descriptor.sample_count;
        native_descriptor.storageMode = storageMode(descriptor.storage);
        native_descriptor.hazardTrackingMode = descriptor.hazard_tracking == ALMetalHazardTrackingMode::tracked
            ? MTLHazardTrackingModeTracked : MTLHazardTrackingModeUntracked;
        native_descriptor.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        native_descriptor.usage = textureUsage(descriptor.usage);

        id<MTLTexture> texture = [native_device newTextureWithDescriptor:native_descriptor];
        if (!texture)
        {
            setError(error, "Metal device could not allocate the texture");
            return nullptr;
        }
        texture.label = toNSString(descriptor.label);

        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->texture = texture;
        return std::unique_ptr<ALMetalTexture>(new ALMetalTexture(std::move(impl), descriptor));
    }
}

void* ALMetalTexture::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->texture : nullptr;
}

const ALMetalTextureDescriptor& ALMetalTexture::descriptor() const noexcept
{
    return mDescriptor;
}

std::uint32_t ALMetalTexture::width(std::uint32_t mip_level) const noexcept
{
    return mip_level < mDescriptor.mipmap_level_count ? mipDimension(mDescriptor.width, mip_level) : 0;
}

std::uint32_t ALMetalTexture::height(std::uint32_t mip_level) const noexcept
{
    return mip_level < mDescriptor.mipmap_level_count ? mipDimension(mDescriptor.height, mip_level) : 0;
}

std::size_t ALMetalTexture::bytesPerPixel() const noexcept
{
    return formatBytesPerPixel(mDescriptor.format);
}

bool ALMetalTexture::upload(ALMetalCommandBuffer& command_buffer,
                            std::uint32_t mip_level,
                            std::uint32_t slice,
                            ALMetalRegion2D region,
                            const void* bytes,
                            std::size_t bytes_per_row,
                            std::string* error)
{
    if (!bytes)
    {
        setError(error, "Cannot upload a Metal texture from a null source");
        return false;
    }
    if (mDescriptor.type == ALMetalTextureType::texture_2d_multisample ||
        mDescriptor.storage == ALMetalStorageMode::memoryless)
    {
        setError(error, "Multisample and memoryless Metal textures cannot receive CPU uploads");
        return false;
    }
    if (!normalizedRegion(mDescriptor, mip_level, slice, &region, error))
    {
        return false;
    }
    const std::size_t tight_row = static_cast<std::size_t>(region.width) * bytesPerPixel();
    if (tight_row == 0 || bytes_per_row < tight_row)
    {
        setError(error, "Metal texture upload row pitch is too small");
        return false;
    }
    if (command_buffer.isCommitted() ||
        command_buffer.nativeDevice() != (__bridge void*)mImpl->device)
    {
        setError(error, "Metal texture upload requires an uncommitted command buffer from the same device");
        return false;
    }

    @autoreleasepool
    {
        const MTLRegion native_region = MTLRegionMake2D(region.x, region.y, region.width, region.height);
        if (mDescriptor.storage == ALMetalStorageMode::shared)
        {
            [mImpl->texture replaceRegion:native_region
                              mipmapLevel:mip_level
                                    slice:slice
                                withBytes:bytes
                              bytesPerRow:bytes_per_row
                            bytesPerImage:bytes_per_row * region.height];
            return true;
        }

        const std::size_t aligned_row = alignUp(tight_row, 256);
        if (aligned_row > std::numeric_limits<std::size_t>::max() / region.height)
        {
            setError(error, "Metal texture upload staging size overflowed");
            return false;
        }
        const std::size_t staging_size = aligned_row * region.height;
        id<MTLBuffer> staging = [mImpl->device
            newBufferWithLength:staging_size
                         options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
        if (!staging)
        {
            setError(error, "Metal could not allocate the texture upload staging resource");
            return false;
        }
        auto* destination = static_cast<std::uint8_t*>(staging.contents);
        const auto* source = static_cast<const std::uint8_t*>(bytes);
        for (std::uint32_t row = 0; row < region.height; ++row)
        {
            std::memcpy(destination + row * aligned_row, source + row * bytes_per_row, tight_row);
        }

        id<MTLCommandBuffer> command = (__bridge id<MTLCommandBuffer>)command_buffer.nativeHandle();
        id<MTLBlitCommandEncoder> encoder = [command blitCommandEncoder];
        if (!encoder)
        {
            setError(error, "Metal could not create a texture upload blit encoder");
            return false;
        }
        [encoder copyFromBuffer:staging
                   sourceOffset:0
              sourceBytesPerRow:aligned_row
            sourceBytesPerImage:staging_size
                     sourceSize:MTLSizeMake(region.width, region.height, 1)
                      toTexture:mImpl->texture
               destinationSlice:slice
               destinationLevel:mip_level
              destinationOrigin:MTLOriginMake(region.x, region.y, 0)];
        [encoder endEncoding];
        return true;
    }
}

bool ALMetalTexture::readback(const ALMetalDevice& device,
                              std::uint32_t mip_level,
                              std::uint32_t slice,
                              ALMetalRegion2D region,
                              void* bytes,
                              std::size_t bytes_per_row,
                              std::string* error) const
{
    if (!bytes)
    {
        setError(error, "Cannot read a Metal texture into a null destination");
        return false;
    }
    if (mDescriptor.type == ALMetalTextureType::texture_2d_multisample ||
        mDescriptor.storage == ALMetalStorageMode::memoryless)
    {
        setError(error, "Multisample and memoryless Metal textures require a resolve before readback");
        return false;
    }
    if (!normalizedRegion(mDescriptor, mip_level, slice, &region, error))
    {
        return false;
    }
    if (device.nativeHandle() != (__bridge void*)mImpl->device)
    {
        setError(error, "Metal texture and readback device do not match");
        return false;
    }
    const std::size_t tight_row = static_cast<std::size_t>(region.width) * bytesPerPixel();
    if (tight_row == 0 || bytes_per_row < tight_row)
    {
        setError(error, "Metal texture readback row pitch is too small");
        return false;
    }

    @autoreleasepool
    {
        if (mDescriptor.storage == ALMetalStorageMode::shared)
        {
            [mImpl->texture getBytes:bytes
                         bytesPerRow:bytes_per_row
                       bytesPerImage:bytes_per_row * region.height
                          fromRegion:MTLRegionMake2D(region.x, region.y, region.width, region.height)
                         mipmapLevel:mip_level
                               slice:slice];
            return true;
        }

        const std::size_t aligned_row = alignUp(tight_row, 256);
        if (aligned_row > std::numeric_limits<std::size_t>::max() / region.height)
        {
            setError(error, "Metal texture readback staging size overflowed");
            return false;
        }
        const std::size_t staging_size = aligned_row * region.height;
        id<MTLBuffer> staging = [mImpl->device newBufferWithLength:staging_size
                                                          options:MTLResourceStorageModeShared];
        if (!staging)
        {
            setError(error, "Metal could not allocate the texture readback staging resource");
            return false;
        }
        auto command_buffer = device.makeCommandBuffer("Alchemy texture readback", error);
        if (!command_buffer)
        {
            return false;
        }
        id<MTLCommandBuffer> command = (__bridge id<MTLCommandBuffer>)command_buffer->nativeHandle();
        id<MTLBlitCommandEncoder> encoder = [command blitCommandEncoder];
        if (!encoder)
        {
            setError(error, "Metal could not create a texture readback blit encoder");
            return false;
        }
        [encoder copyFromTexture:mImpl->texture
                     sourceSlice:slice
                     sourceLevel:mip_level
                    sourceOrigin:MTLOriginMake(region.x, region.y, 0)
                      sourceSize:MTLSizeMake(region.width, region.height, 1)
                        toBuffer:staging
               destinationOffset:0
          destinationBytesPerRow:aligned_row
        destinationBytesPerImage:staging_size];
        [encoder endEncoding];
        if (!command_buffer->commitAndWait(error))
        {
            return false;
        }

        const auto* source = static_cast<const std::uint8_t*>(staging.contents);
        auto* destination = static_cast<std::uint8_t*>(bytes);
        for (std::uint32_t row = 0; row < region.height; ++row)
        {
            std::memcpy(destination + row * bytes_per_row, source + row * aligned_row, tight_row);
        }
        return true;
    }
}

struct ALMetalRenderPass::Impl
{
    MTLRenderPassDescriptor* descriptor = nil;
};

ALMetalRenderPass::ALMetalRenderPass(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalRenderPass::~ALMetalRenderPass() = default;

void* ALMetalRenderPass::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->descriptor : nullptr;
}

struct ALMetalRenderTarget::Impl
{
    std::array<std::unique_ptr<ALMetalTexture>, AL_METAL_MAX_COLOR_ATTACHMENTS> colors;
    std::array<std::unique_ptr<ALMetalTexture>, AL_METAL_MAX_COLOR_ATTACHMENTS> multisample_colors;
    std::unique_ptr<ALMetalTexture> depth_stencil;
};

ALMetalRenderTarget::ALMetalRenderTarget(
    std::unique_ptr<Impl> impl,
    ALMetalRenderTargetDescriptor descriptor)
: mImpl(std::move(impl)),
  mDescriptor(std::move(descriptor))
{
}

ALMetalRenderTarget::~ALMetalRenderTarget() = default;

std::unique_ptr<ALMetalRenderTarget> ALMetalRenderTarget::create(
    const ALMetalDevice& device,
    const ALMetalRenderTargetDescriptor& descriptor,
    std::string* error)
{
    if (descriptor.width == 0 || descriptor.height == 0 ||
        descriptor.color_attachment_count > AL_METAL_MAX_COLOR_ATTACHMENTS ||
        (descriptor.color_attachment_count == 0 && descriptor.depth_stencil_format == ALMetalPixelFormat::invalid))
    {
        setError(error, "Metal render target descriptor has invalid dimensions or attachment count");
        return nullptr;
    }
    if (descriptor.sample_count == 0 ||
        (descriptor.sample_count != 1 && descriptor.sample_count != 2 &&
         descriptor.sample_count != 4 && descriptor.sample_count != 8))
    {
        setError(error, "Metal render target sample count must be 1, 2, 4 or 8");
        return nullptr;
    }
    if (descriptor.depth_stencil_format != ALMetalPixelFormat::invalid &&
        descriptor.depth_stencil_format != ALMetalPixelFormat::depth32_float &&
        descriptor.depth_stencil_format != ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal render target depth attachment requires a depth format");
        return nullptr;
    }
    if (descriptor.memoryless_depth_stencil && descriptor.depth_stencil_shader_read)
    {
        setError(error, "A memoryless Metal depth attachment cannot be shader-readable");
        return nullptr;
    }

    auto impl = std::make_unique<Impl>();
    for (std::uint32_t index = 0; index < descriptor.color_attachment_count; ++index)
    {
        const auto& color = descriptor.colors[index];
        if (pixelFormat(color.format) == MTLPixelFormatInvalid ||
            color.format == ALMetalPixelFormat::depth32_float ||
            color.format == ALMetalPixelFormat::depth32_float_stencil8)
        {
            setError(error, "Metal render target color attachment has a non-color format");
            return nullptr;
        }

        ALMetalTextureDescriptor resolved;
        resolved.format = color.format;
        resolved.width = descriptor.width;
        resolved.height = descriptor.height;
        resolved.storage = ALMetalStorageMode::private_gpu;
        resolved.usage = ALMetalTextureUsage::render_target;
        if (color.shader_read)
        {
            resolved.usage = resolved.usage | ALMetalTextureUsage::shader_read;
        }
        if (color.shader_write)
        {
            resolved.usage = resolved.usage | ALMetalTextureUsage::shader_write;
        }
        resolved.label = color.label.empty()
            ? descriptor.label + " color " + std::to_string(index)
            : color.label;
        impl->colors[index] = ALMetalTexture::create(device, resolved, error);
        if (!impl->colors[index])
        {
            return nullptr;
        }

        if (descriptor.sample_count > 1)
        {
            ALMetalTextureDescriptor multisample = resolved;
            multisample.type = ALMetalTextureType::texture_2d_multisample;
            multisample.sample_count = descriptor.sample_count;
            multisample.storage = descriptor.memoryless_multisample
                ? ALMetalStorageMode::memoryless : ALMetalStorageMode::private_gpu;
            multisample.usage = ALMetalTextureUsage::render_target;
            multisample.label = resolved.label + " MSAA";
            impl->multisample_colors[index] = ALMetalTexture::create(device, multisample, error);
            if (!impl->multisample_colors[index])
            {
                return nullptr;
            }
        }
    }

    if (descriptor.depth_stencil_format != ALMetalPixelFormat::invalid)
    {
        ALMetalTextureDescriptor depth;
        depth.type = descriptor.sample_count > 1
            ? ALMetalTextureType::texture_2d_multisample : ALMetalTextureType::texture_2d;
        depth.format = descriptor.depth_stencil_format;
        depth.width = descriptor.width;
        depth.height = descriptor.height;
        depth.sample_count = descriptor.sample_count;
        depth.storage = descriptor.memoryless_depth_stencil
            ? ALMetalStorageMode::memoryless : ALMetalStorageMode::private_gpu;
        depth.usage = ALMetalTextureUsage::render_target;
        if (descriptor.depth_stencil_shader_read)
        {
            depth.usage = depth.usage | ALMetalTextureUsage::shader_read;
        }
        depth.label = descriptor.label + " depth-stencil";
        impl->depth_stencil = ALMetalTexture::create(device, depth, error);
        if (!impl->depth_stencil)
        {
            return nullptr;
        }
    }

    return std::unique_ptr<ALMetalRenderTarget>(
        new ALMetalRenderTarget(std::move(impl), descriptor));
}

const ALMetalRenderTargetDescriptor& ALMetalRenderTarget::descriptor() const noexcept
{
    return mDescriptor;
}

const ALMetalTexture* ALMetalRenderTarget::colorTexture(std::uint32_t index) const noexcept
{
    return mImpl && index < mDescriptor.color_attachment_count ? mImpl->colors[index].get() : nullptr;
}

const ALMetalTexture* ALMetalRenderTarget::multisampleColorTexture(std::uint32_t index) const noexcept
{
    return mImpl && index < mDescriptor.color_attachment_count
        ? mImpl->multisample_colors[index].get() : nullptr;
}

const ALMetalTexture* ALMetalRenderTarget::depthStencilTexture() const noexcept
{
    return mImpl ? mImpl->depth_stencil.get() : nullptr;
}

std::unique_ptr<ALMetalRenderPass> ALMetalRenderTarget::makeRenderPass(
    const ALMetalRenderPassConfig& config,
    std::string* error) const
{
    @autoreleasepool
    {
        if (!mImpl)
        {
            setError(error, "Metal render target is unavailable");
            return nullptr;
        }
        MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        for (std::uint32_t index = 0; index < mDescriptor.color_attachment_count; ++index)
        {
            const auto& pass_config = config.colors[index];
            const bool multisample = mDescriptor.sample_count > 1;
            if (!multisample && isResolveAction(pass_config.store))
            {
                setError(error, "A single-sample Metal color attachment cannot use a resolve store action");
                return nullptr;
            }
            if (multisample && mDescriptor.memoryless_multisample &&
                (pass_config.load == ALMetalLoadAction::load ||
                 pass_config.store == ALMetalStoreAction::store ||
                 pass_config.store == ALMetalStoreAction::store_and_multisample_resolve))
            {
                setError(error, "A memoryless Metal multisample attachment cannot load or preserve its samples");
                return nullptr;
            }

            MTLRenderPassColorAttachmentDescriptor* attachment = descriptor.colorAttachments[index];
            attachment.texture = (__bridge id<MTLTexture>)(multisample
                ? mImpl->multisample_colors[index]->nativeHandle()
                : mImpl->colors[index]->nativeHandle());
            attachment.resolveTexture = multisample
                ? (__bridge id<MTLTexture>)mImpl->colors[index]->nativeHandle() : nil;
            attachment.loadAction = loadAction(pass_config.load);
            attachment.storeAction = storeAction(pass_config.store);
            attachment.clearColor = MTLClearColorMake(pass_config.clear.red,
                                                       pass_config.clear.green,
                                                       pass_config.clear.blue,
                                                       pass_config.clear.alpha);
        }

        if (mImpl->depth_stencil)
        {
            const bool memoryless = mDescriptor.memoryless_depth_stencil;
            if ((memoryless && (config.depth_load == ALMetalLoadAction::load ||
                                config.depth_store != ALMetalStoreAction::dont_care)) ||
                isResolveAction(config.depth_store))
            {
                setError(error, "Metal depth load/store actions are incompatible with this attachment");
                return nullptr;
            }
            id<MTLTexture> depth_texture = (__bridge id<MTLTexture>)mImpl->depth_stencil->nativeHandle();
            descriptor.depthAttachment.texture = depth_texture;
            descriptor.depthAttachment.loadAction = loadAction(config.depth_load);
            descriptor.depthAttachment.storeAction = storeAction(config.depth_store);
            descriptor.depthAttachment.clearDepth = config.clear_depth;

            if (mDescriptor.depth_stencil_format == ALMetalPixelFormat::depth32_float_stencil8)
            {
                if ((memoryless && (config.stencil_load == ALMetalLoadAction::load ||
                                    config.stencil_store != ALMetalStoreAction::dont_care)) ||
                    isResolveAction(config.stencil_store))
                {
                    setError(error, "Metal stencil load/store actions are incompatible with this attachment");
                    return nullptr;
                }
                descriptor.stencilAttachment.texture = depth_texture;
                descriptor.stencilAttachment.loadAction = loadAction(config.stencil_load);
                descriptor.stencilAttachment.storeAction = storeAction(config.stencil_store);
                descriptor.stencilAttachment.clearStencil = config.clear_stencil;
            }
        }

        auto impl = std::make_unique<ALMetalRenderPass::Impl>();
        impl->descriptor = descriptor;
        return std::unique_ptr<ALMetalRenderPass>(new ALMetalRenderPass(std::move(impl)));
    }
}
