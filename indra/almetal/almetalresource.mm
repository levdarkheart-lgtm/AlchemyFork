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
#import <IOSurface/IOSurface.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>
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
            case ALMetalPixelFormat::r8_sint:                return MTLPixelFormatR8Sint;
            case ALMetalPixelFormat::r8_uint:                return MTLPixelFormatR8Uint;
            case ALMetalPixelFormat::rg8_unorm:              return MTLPixelFormatRG8Unorm;
            case ALMetalPixelFormat::rg8_sint:               return MTLPixelFormatRG8Sint;
            case ALMetalPixelFormat::rg8_uint:               return MTLPixelFormatRG8Uint;
            case ALMetalPixelFormat::rgba8_unorm:            return MTLPixelFormatRGBA8Unorm;
            case ALMetalPixelFormat::rgba8_unorm_srgb:       return MTLPixelFormatRGBA8Unorm_sRGB;
            case ALMetalPixelFormat::rgba8_sint:             return MTLPixelFormatRGBA8Sint;
            case ALMetalPixelFormat::rgba8_uint:             return MTLPixelFormatRGBA8Uint;
            case ALMetalPixelFormat::bgra8_unorm:            return MTLPixelFormatBGRA8Unorm;
            case ALMetalPixelFormat::bgra8_unorm_srgb:       return MTLPixelFormatBGRA8Unorm_sRGB;
            case ALMetalPixelFormat::r16_unorm:              return MTLPixelFormatR16Unorm;
            case ALMetalPixelFormat::r16_float:              return MTLPixelFormatR16Float;
            case ALMetalPixelFormat::r16_sint:               return MTLPixelFormatR16Sint;
            case ALMetalPixelFormat::r16_uint:               return MTLPixelFormatR16Uint;
            case ALMetalPixelFormat::rg16_float:             return MTLPixelFormatRG16Float;
            case ALMetalPixelFormat::rg16_sint:              return MTLPixelFormatRG16Sint;
            case ALMetalPixelFormat::rg16_uint:              return MTLPixelFormatRG16Uint;
            case ALMetalPixelFormat::rgba16_unorm:           return MTLPixelFormatRGBA16Unorm;
            case ALMetalPixelFormat::rgba16_float:           return MTLPixelFormatRGBA16Float;
            case ALMetalPixelFormat::rgba16_sint:            return MTLPixelFormatRGBA16Sint;
            case ALMetalPixelFormat::rgba16_uint:            return MTLPixelFormatRGBA16Uint;
            case ALMetalPixelFormat::r32_float:              return MTLPixelFormatR32Float;
            case ALMetalPixelFormat::r32_sint:               return MTLPixelFormatR32Sint;
            case ALMetalPixelFormat::r32_uint:               return MTLPixelFormatR32Uint;
            case ALMetalPixelFormat::rg32_float:             return MTLPixelFormatRG32Float;
            case ALMetalPixelFormat::rg32_sint:              return MTLPixelFormatRG32Sint;
            case ALMetalPixelFormat::rg32_uint:              return MTLPixelFormatRG32Uint;
            case ALMetalPixelFormat::rgba32_float:           return MTLPixelFormatRGBA32Float;
            case ALMetalPixelFormat::rgba32_sint:            return MTLPixelFormatRGBA32Sint;
            case ALMetalPixelFormat::rgba32_uint:            return MTLPixelFormatRGBA32Uint;
            case ALMetalPixelFormat::rgb10a2_unorm:          return MTLPixelFormatRGB10A2Unorm;
            case ALMetalPixelFormat::rgb10a2_uint:           return MTLPixelFormatRGB10A2Uint;
            case ALMetalPixelFormat::rg11b10_float:          return MTLPixelFormatRG11B10Float;
            case ALMetalPixelFormat::bc1_rgba:               return MTLPixelFormatBC1_RGBA;
            case ALMetalPixelFormat::bc1_rgba_srgb:          return MTLPixelFormatBC1_RGBA_sRGB;
            case ALMetalPixelFormat::bc2_rgba:               return MTLPixelFormatBC2_RGBA;
            case ALMetalPixelFormat::bc2_rgba_srgb:          return MTLPixelFormatBC2_RGBA_sRGB;
            case ALMetalPixelFormat::bc3_rgba:               return MTLPixelFormatBC3_RGBA;
            case ALMetalPixelFormat::bc3_rgba_srgb:          return MTLPixelFormatBC3_RGBA_sRGB;
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
            case ALMetalPixelFormat::r8_sint:
            case ALMetalPixelFormat::r8_uint:                return 1;
            case ALMetalPixelFormat::rg8_unorm:              return 2;
            case ALMetalPixelFormat::rg8_sint:
            case ALMetalPixelFormat::rg8_uint:               return 2;
            case ALMetalPixelFormat::rgba8_unorm:
            case ALMetalPixelFormat::rgba8_unorm_srgb:
            case ALMetalPixelFormat::rgba8_sint:
            case ALMetalPixelFormat::rgba8_uint:
            case ALMetalPixelFormat::bgra8_unorm:
            case ALMetalPixelFormat::bgra8_unorm_srgb:
            case ALMetalPixelFormat::r32_float:
            case ALMetalPixelFormat::r32_sint:
            case ALMetalPixelFormat::r32_uint:
            case ALMetalPixelFormat::rgb10a2_unorm:
            case ALMetalPixelFormat::rgb10a2_uint:
            case ALMetalPixelFormat::rg11b10_float:
            case ALMetalPixelFormat::depth32_float:          return 4;
            case ALMetalPixelFormat::r16_unorm:
            case ALMetalPixelFormat::r16_float:              return 2;
            case ALMetalPixelFormat::r16_sint:
            case ALMetalPixelFormat::r16_uint:               return 2;
            case ALMetalPixelFormat::rg16_float:
            case ALMetalPixelFormat::rg16_sint:
            case ALMetalPixelFormat::rg16_uint:              return 4;
            case ALMetalPixelFormat::rgba16_unorm:
            case ALMetalPixelFormat::rgba16_float:
            case ALMetalPixelFormat::rgba16_sint:
            case ALMetalPixelFormat::rgba16_uint:            return 8;
            case ALMetalPixelFormat::rg32_float:
            case ALMetalPixelFormat::rg32_sint:
            case ALMetalPixelFormat::rg32_uint:              return 8;
            case ALMetalPixelFormat::rgba32_float:
            case ALMetalPixelFormat::rgba32_sint:
            case ALMetalPixelFormat::rgba32_uint:            return 16;
            case ALMetalPixelFormat::depth32_float_stencil8: return 8;
            case ALMetalPixelFormat::bc1_rgba:
            case ALMetalPixelFormat::bc1_rgba_srgb:
            case ALMetalPixelFormat::bc2_rgba:
            case ALMetalPixelFormat::bc2_rgba_srgb:
            case ALMetalPixelFormat::bc3_rgba:
            case ALMetalPixelFormat::bc3_rgba_srgb:          return 0;
            case ALMetalPixelFormat::invalid:                return 0;
        }
        return 0;
    }

    std::uint32_t formatBlockWidth(ALMetalPixelFormat format)
    {
        switch (format)
        {
            case ALMetalPixelFormat::bc1_rgba:
            case ALMetalPixelFormat::bc1_rgba_srgb:
            case ALMetalPixelFormat::bc2_rgba:
            case ALMetalPixelFormat::bc2_rgba_srgb:
            case ALMetalPixelFormat::bc3_rgba:
            case ALMetalPixelFormat::bc3_rgba_srgb: return 4;
            default:                                return 1;
        }
    }

    std::uint32_t formatBlockHeight(ALMetalPixelFormat format)
    {
        return formatBlockWidth(format);
    }

    std::size_t formatBytesPerBlock(ALMetalPixelFormat format)
    {
        switch (format)
        {
            case ALMetalPixelFormat::bc1_rgba:
            case ALMetalPixelFormat::bc1_rgba_srgb: return 8;
            case ALMetalPixelFormat::bc2_rgba:
            case ALMetalPixelFormat::bc2_rgba_srgb:
            case ALMetalPixelFormat::bc3_rgba:
            case ALMetalPixelFormat::bc3_rgba_srgb: return 16;
            default:                                return formatBytesPerPixel(format);
        }
    }

    MTLTextureSwizzle textureSwizzle(ALMetalTextureSwizzle swizzle)
    {
        switch (swizzle)
        {
            case ALMetalTextureSwizzle::zero:  return MTLTextureSwizzleZero;
            case ALMetalTextureSwizzle::one:   return MTLTextureSwizzleOne;
            case ALMetalTextureSwizzle::red:   return MTLTextureSwizzleRed;
            case ALMetalTextureSwizzle::green: return MTLTextureSwizzleGreen;
            case ALMetalTextureSwizzle::blue:  return MTLTextureSwizzleBlue;
            case ALMetalTextureSwizzle::alpha: return MTLTextureSwizzleAlpha;
        }
        return MTLTextureSwizzleRed;
    }

    bool hasIdentitySwizzle(const ALMetalTextureDescriptor& descriptor)
    {
        const std::array<ALMetalTextureSwizzle, 4> identity = {
            ALMetalTextureSwizzle::red,
            ALMetalTextureSwizzle::green,
            ALMetalTextureSwizzle::blue,
            ALMetalTextureSwizzle::alpha,
        };
        return descriptor.swizzle == identity;
    }

    MTLTextureType textureType(ALMetalTextureType type)
    {
        switch (type)
        {
            case ALMetalTextureType::texture_2d:             return MTLTextureType2D;
            case ALMetalTextureType::texture_2d_array:       return MTLTextureType2DArray;
            case ALMetalTextureType::texture_cube:           return MTLTextureTypeCube;
            case ALMetalTextureType::texture_cube_array:     return MTLTextureTypeCubeArray;
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
            case ALMetalTextureType::texture_cube_array:
                return descriptor.array_length * 6;
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

    struct TextureTransferLayout
    {
        std::size_t tight_row = 0;
        std::uint32_t rows = 0;
    };

    bool textureTransferLayout(const ALMetalTextureDescriptor& descriptor,
                               std::uint32_t mip_level,
                               const ALMetalRegion2D& region,
                               TextureTransferLayout* layout,
                               std::string* error)
    {
        const std::uint32_t block_width = formatBlockWidth(descriptor.format);
        const std::uint32_t block_height = formatBlockHeight(descriptor.format);
        const std::size_t bytes_per_block = formatBytesPerBlock(descriptor.format);
        if (bytes_per_block == 0)
        {
            setError(error, "Metal texture format has no transferable pixel/block size");
            return false;
        }

        const std::uint32_t mip_width = mipDimension(descriptor.width, mip_level);
        const std::uint32_t mip_height = mipDimension(descriptor.height, mip_level);
        if (block_width > 1 || block_height > 1)
        {
            if ((region.x % block_width) != 0 || (region.y % block_height) != 0 ||
                ((region.width % block_width) != 0 && region.x + region.width != mip_width) ||
                ((region.height % block_height) != 0 && region.y + region.height != mip_height))
            {
                setError(error, "Compressed Metal texture transfer is not block aligned");
                return false;
            }
        }

        const std::size_t blocks_per_row =
            (static_cast<std::size_t>(region.width) + block_width - 1) / block_width;
        if (blocks_per_row > std::numeric_limits<std::size_t>::max() / bytes_per_block)
        {
            setError(error, "Metal texture transfer row size overflowed");
            return false;
        }
        layout->tight_row = blocks_per_row * bytes_per_block;
        layout->rows = (region.height + block_height - 1) / block_height;
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
    id<MTLTexture> shader_view = nil;
    id<MTLTexture> linear_view = nil;
    std::mutex linear_view_mutex;
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
        if (descriptor.type != ALMetalTextureType::texture_2d_array &&
            descriptor.type != ALMetalTextureType::texture_cube_array &&
            descriptor.array_length != 1)
        {
            setError(error, "Only an array Metal texture accepts array_length greater than one");
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
        if (formatBlockWidth(descriptor.format) > 1 && !native_device.supportsBCTextureCompression)
        {
            setError(error, "Metal device does not support BC-compressed textures");
            return nullptr;
        }
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
        const MTLPixelFormat linear_format = pixelFormat(
            alMetalLinearFormat(descriptor.format));
        const bool needs_linear_view = linear_format != native_descriptor.pixelFormat;
        if (!hasIdentitySwizzle(descriptor) || needs_linear_view)
        {
            native_descriptor.usage |= MTLTextureUsagePixelFormatView;
        }

        id<MTLTexture> texture = [native_device newTextureWithDescriptor:native_descriptor];
        if (!texture)
        {
            setError(error, "Metal device could not allocate the texture");
            return nullptr;
        }
        texture.label = toNSString(descriptor.label);

        id<MTLTexture> shader_view = nil;
        if (!hasIdentitySwizzle(descriptor))
        {
            const MTLTextureSwizzleChannels channels = MTLTextureSwizzleChannelsMake(
                textureSwizzle(descriptor.swizzle[0]),
                textureSwizzle(descriptor.swizzle[1]),
                textureSwizzle(descriptor.swizzle[2]),
                textureSwizzle(descriptor.swizzle[3]));
            shader_view = [texture
                newTextureViewWithPixelFormat:native_descriptor.pixelFormat
                                  textureType:native_descriptor.textureType
                                       levels:NSMakeRange(0, descriptor.mipmap_level_count)
                                       slices:NSMakeRange(0, sliceCount(descriptor))
                                      swizzle:channels];
            if (!shader_view)
            {
                setError(error, "Metal could not create the requested texture swizzle view");
                return nullptr;
            }
            shader_view.label = toNSString(descriptor.label + " swizzle");
        }

        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->texture = texture;
        impl->shader_view = shader_view;
        return std::unique_ptr<ALMetalTexture>(new ALMetalTexture(std::move(impl), descriptor));
    }
}

std::unique_ptr<ALMetalTexture> ALMetalTexture::createFromIOSurface(
    const ALMetalDevice& device,
    void* iosurface,
    std::uint32_t plane,
    const ALMetalTextureDescriptor& descriptor,
    std::string* error)
{
    @autoreleasepool
    {
        IOSurfaceRef surface = static_cast<IOSurfaceRef>(iosurface);
        if (!device.nativeHandle() || !surface)
        {
            setError(error, "Cannot import a Metal texture without a device and IOSurface");
            return nullptr;
        }
        if (descriptor.type != ALMetalTextureType::texture_2d ||
            descriptor.storage != ALMetalStorageMode::shared ||
            descriptor.format != ALMetalPixelFormat::bgra8_unorm ||
            descriptor.width == 0 || descriptor.height == 0 ||
            descriptor.array_length != 1 ||
            descriptor.mipmap_level_count != 1 ||
            descriptor.sample_count != 1 ||
            pixelFormat(descriptor.format) == MTLPixelFormatInvalid ||
            !hasIdentitySwizzle(descriptor))
        {
            setError(error, "IOSurface import requires one shared BGRA8, identity-swizzled 2D Metal level");
            return nullptr;
        }
        const std::size_t plane_count = IOSurfaceGetPlaneCount(surface);
        if ((plane_count == 0 && plane != 0) ||
            (plane_count != 0 && plane >= plane_count))
        {
            setError(error, "IOSurface import plane is out of range");
            return nullptr;
        }
        const std::size_t surface_width = plane_count == 0
            ? IOSurfaceGetWidth(surface)
            : IOSurfaceGetWidthOfPlane(surface, plane);
        const std::size_t surface_height = plane_count == 0
            ? IOSurfaceGetHeight(surface)
            : IOSurfaceGetHeightOfPlane(surface, plane);
        if (descriptor.width != surface_width ||
            descriptor.height != surface_height)
        {
            setError(error, "IOSurface dimensions do not match the Metal texture descriptor");
            return nullptr;
        }
        const std::size_t surface_element_bytes = plane_count == 0
            ? IOSurfaceGetBytesPerElement(surface)
            : IOSurfaceGetBytesPerElementOfPlane(surface, plane);
        const std::size_t surface_row_bytes = plane_count == 0
            ? IOSurfaceGetBytesPerRow(surface)
            : IOSurfaceGetBytesPerRowOfPlane(surface, plane);
        if (surface_element_bytes != 4 ||
            surface_row_bytes < surface_width * surface_element_bytes ||
            surface_row_bytes % 16 != 0)
        {
            setError(error, "IOSurface BGRA rows are incompatible with Metal texture alignment");
            return nullptr;
        }

        id<MTLDevice> native_device =
            (__bridge id<MTLDevice>)device.nativeHandle();
        MTLTextureDescriptor* native_descriptor =
            [[MTLTextureDescriptor alloc] init];
        native_descriptor.textureType = MTLTextureType2D;
        native_descriptor.pixelFormat = pixelFormat(descriptor.format);
        native_descriptor.width = descriptor.width;
        native_descriptor.height = descriptor.height;
        native_descriptor.depth = 1;
        native_descriptor.arrayLength = 1;
        native_descriptor.mipmapLevelCount = 1;
        native_descriptor.sampleCount = 1;
        native_descriptor.storageMode = MTLStorageModeShared;
        native_descriptor.hazardTrackingMode =
            descriptor.hazard_tracking == ALMetalHazardTrackingMode::tracked
                ? MTLHazardTrackingModeTracked
                : MTLHazardTrackingModeUntracked;
        native_descriptor.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        native_descriptor.usage = textureUsage(descriptor.usage);

        id<MTLTexture> texture = [native_device
            newTextureWithDescriptor:native_descriptor
                           iosurface:surface
                               plane:plane];
        if (!texture)
        {
            setError(error, "Metal device could not import the IOSurface texture");
            return nullptr;
        }
        texture.label = toNSString(descriptor.label);

        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->texture = texture;
        return std::unique_ptr<ALMetalTexture>(
            new ALMetalTexture(std::move(impl), descriptor));
    }
}

void* ALMetalTexture::nativeHandle() const noexcept
{
    if (!mImpl)
    {
        return nullptr;
    }
    return (__bridge void*)(mImpl->shader_view ? mImpl->shader_view : mImpl->texture);
}

void* ALMetalTexture::nativeLinearHandle() const noexcept
{
    if (!mImpl)
    {
        return nullptr;
    }
    const MTLPixelFormat linear_format = pixelFormat(
        alMetalLinearFormat(mDescriptor.format));
    if (linear_format == pixelFormat(mDescriptor.format))
    {
        return nativeHandle();
    }

    // A viewer can own thousands of sRGB mirrors but only a small fraction
    // are resized. Defer the compatible view object until a raw-storage
    // operation actually needs it.
    std::lock_guard<std::mutex> lock(mImpl->linear_view_mutex);
    if (!mImpl->linear_view)
    {
        @autoreleasepool
        {
            const MTLTextureSwizzleChannels channels = MTLTextureSwizzleChannelsMake(
                textureSwizzle(mDescriptor.swizzle[0]),
                textureSwizzle(mDescriptor.swizzle[1]),
                textureSwizzle(mDescriptor.swizzle[2]),
                textureSwizzle(mDescriptor.swizzle[3]));
            mImpl->linear_view = [mImpl->texture
                newTextureViewWithPixelFormat:linear_format
                                  textureType:mImpl->texture.textureType
                                       levels:NSMakeRange(0, mDescriptor.mipmap_level_count)
                                       slices:NSMakeRange(0, sliceCount(mDescriptor))
                                      swizzle:channels];
            if (mImpl->linear_view)
            {
                mImpl->linear_view.label = toNSString(
                    mDescriptor.label + " linear storage view");
            }
        }
    }
    return (__bridge void*)mImpl->linear_view;
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

std::uint32_t ALMetalTexture::blockWidth() const noexcept
{
    return formatBlockWidth(mDescriptor.format);
}

std::uint32_t ALMetalTexture::blockHeight() const noexcept
{
    return formatBlockHeight(mDescriptor.format);
}

std::size_t ALMetalTexture::bytesPerBlock() const noexcept
{
    return formatBytesPerBlock(mDescriptor.format);
}

bool ALMetalTexture::isCompressed() const noexcept
{
    return blockWidth() > 1 || blockHeight() > 1;
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
    if (mDescriptor.format ==
        ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(
            error,
            "Combined Metal depth-stencil textures cannot receive an interleaved CPU upload");
        return false;
    }
    if (!normalizedRegion(mDescriptor, mip_level, slice, &region, error))
    {
        return false;
    }
    TextureTransferLayout layout;
    if (!textureTransferLayout(mDescriptor, mip_level, region, &layout, error))
    {
        return false;
    }
    if (bytes_per_row < layout.tight_row)
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
                            bytesPerImage:bytes_per_row * layout.rows];
            return true;
        }

        const std::size_t aligned_row = alignUp(layout.tight_row, 256);
        if (aligned_row > std::numeric_limits<std::size_t>::max() / layout.rows)
        {
            setError(error, "Metal texture upload staging size overflowed");
            return false;
        }
        const std::size_t staging_size = aligned_row * layout.rows;
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
        for (std::uint32_t row = 0; row < layout.rows; ++row)
        {
            std::memcpy(destination + row * aligned_row,
                        source + row * bytes_per_row,
                        layout.tight_row);
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

bool ALMetalTexture::copyFrom(ALMetalCommandBuffer& command_buffer,
                              const ALMetalTexture& source,
                              ALMetalTextureCopy copy,
                              std::string* error)
{
    if (!mImpl || !source.mImpl)
    {
        setError(error, "Metal texture copy requires live source and destination textures");
        return false;
    }
    if (mDescriptor.type == ALMetalTextureType::texture_2d_multisample ||
        source.mDescriptor.type == ALMetalTextureType::texture_2d_multisample ||
        mDescriptor.storage == ALMetalStorageMode::memoryless ||
        source.mDescriptor.storage == ALMetalStorageMode::memoryless)
    {
        setError(error, "Multisample and memoryless Metal textures cannot use a direct blit copy");
        return false;
    }
    if (mDescriptor.format != source.mDescriptor.format)
    {
        setError(error, "Metal texture copy requires matching pixel formats");
        return false;
    }
    if (mImpl->device != source.mImpl->device)
    {
        setError(error, "Metal texture copy source and destination use different devices");
        return false;
    }
    if (!normalizedRegion(source.mDescriptor, copy.source_mip_level,
                          copy.source_slice, &copy.source_region, error))
    {
        return false;
    }
    TextureTransferLayout source_layout;
    if (!textureTransferLayout(source.mDescriptor, copy.source_mip_level,
                               copy.source_region, &source_layout, error))
    {
        return false;
    }

    ALMetalRegion2D destination_region{
        copy.destination_x,
        copy.destination_y,
        copy.source_region.width,
        copy.source_region.height,
    };
    if (!normalizedRegion(mDescriptor, copy.destination_mip_level,
                          copy.destination_slice, &destination_region, error))
    {
        return false;
    }
    TextureTransferLayout destination_layout;
    if (!textureTransferLayout(mDescriptor, copy.destination_mip_level,
                               destination_region, &destination_layout, error))
    {
        return false;
    }
    if (command_buffer.isCommitted() ||
        command_buffer.nativeDevice() != (__bridge void*)mImpl->device)
    {
        setError(error, "Metal texture copy requires an uncommitted command buffer from the same device");
        return false;
    }

    @autoreleasepool
    {
        id<MTLCommandBuffer> command = (__bridge id<MTLCommandBuffer>)command_buffer.nativeHandle();
        id<MTLBlitCommandEncoder> encoder = [command blitCommandEncoder];
        if (!encoder)
        {
            setError(error, "Metal could not create a texture copy blit encoder");
            return false;
        }
        [encoder copyFromTexture:source.mImpl->texture
                     sourceSlice:copy.source_slice
                     sourceLevel:copy.source_mip_level
                    sourceOrigin:MTLOriginMake(copy.source_region.x, copy.source_region.y, 0)
                      sourceSize:MTLSizeMake(copy.source_region.width, copy.source_region.height, 1)
                       toTexture:mImpl->texture
                destinationSlice:copy.destination_slice
                destinationLevel:copy.destination_mip_level
               destinationOrigin:MTLOriginMake(copy.destination_x, copy.destination_y, 0)];
        [encoder endEncoding];
        return true;
    }
}

bool ALMetalTexture::generateMipmaps(ALMetalCommandBuffer& command_buffer,
                                     std::string* error)
{
    if (mDescriptor.mipmap_level_count <= 1)
    {
        setError(error, "Metal texture has no mip levels to generate");
        return false;
    }
    if (isCompressed())
    {
        setError(error, "Metal cannot generate mipmaps for a block-compressed texture");
        return false;
    }
    if (mDescriptor.type == ALMetalTextureType::texture_2d_multisample ||
        mDescriptor.storage == ALMetalStorageMode::memoryless)
    {
        setError(error, "Multisample and memoryless Metal textures cannot generate mipmaps");
        return false;
    }
    if (command_buffer.isCommitted() ||
        command_buffer.nativeDevice() != (__bridge void*)mImpl->device)
    {
        setError(error, "Metal mip generation requires an uncommitted command buffer from the same device");
        return false;
    }

    @autoreleasepool
    {
        id<MTLCommandBuffer> command = (__bridge id<MTLCommandBuffer>)command_buffer.nativeHandle();
        id<MTLBlitCommandEncoder> encoder = [command blitCommandEncoder];
        if (!encoder)
        {
            setError(error, "Metal could not create a mip-generation blit encoder");
            return false;
        }
        [encoder generateMipmapsForTexture:mImpl->texture];
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
    TextureTransferLayout layout;
    const bool depth_from_combined =
        mDescriptor.format == ALMetalPixelFormat::depth32_float_stencil8;
    if (depth_from_combined)
    {
        // The generic readback contract exposes the depth plane of a
        // combined attachment. Stencil remains render-state-owned; callers
        // validate it through a draw because Metal copies the two planes
        // independently rather than as an interleaved eight-byte texel.
        layout.tight_row = static_cast<std::size_t>(region.width) *
                           sizeof(float);
        layout.rows = region.height;
    }
    else if (!textureTransferLayout(
                 mDescriptor, mip_level, region, &layout, error))
    {
        return false;
    }
    if (bytes_per_row < layout.tight_row)
    {
        setError(error, "Metal texture readback row pitch is too small");
        return false;
    }

    @autoreleasepool
    {
        if (mDescriptor.storage == ALMetalStorageMode::shared &&
            !depth_from_combined)
        {
            [mImpl->texture getBytes:bytes
                         bytesPerRow:bytes_per_row
                       bytesPerImage:bytes_per_row * layout.rows
                          fromRegion:MTLRegionMake2D(region.x, region.y, region.width, region.height)
                         mipmapLevel:mip_level
                               slice:slice];
            return true;
        }

        const std::size_t aligned_row = alignUp(layout.tight_row, 256);
        if (aligned_row > std::numeric_limits<std::size_t>::max() / layout.rows)
        {
            setError(error, "Metal texture readback staging size overflowed");
            return false;
        }
        const std::size_t staging_size = aligned_row * layout.rows;
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
        destinationBytesPerImage:staging_size
                         options:depth_from_combined
                             ? MTLBlitOptionDepthFromDepthStencil
                             : MTLBlitOptionNone];
        [encoder endEncoding];
        if (!command_buffer->commitAndWait(error))
        {
            return false;
        }

        const auto* source = static_cast<const std::uint8_t*>(staging.contents);
        auto* destination = static_cast<std::uint8_t*>(bytes);
        for (std::uint32_t row = 0; row < layout.rows; ++row)
        {
            std::memcpy(destination + row * bytes_per_row,
                        source + row * aligned_row,
                        layout.tight_row);
        }
        return true;
    }
}

bool ALMetalTexture::readbackRGB8(const ALMetalDevice& device,
                                  std::uint32_t mip_level,
                                  std::uint32_t slice,
                                  ALMetalRegion2D region,
                                  void* bytes,
                                  std::size_t bytes_per_row,
                                  std::string* error) const
{
    if (!bytes)
    {
        setError(error, "Cannot read Metal RGB pixels into a null destination");
        return false;
    }
    const bool bgra =
        mDescriptor.format == ALMetalPixelFormat::bgra8_unorm ||
        mDescriptor.format == ALMetalPixelFormat::bgra8_unorm_srgb;
    const bool rgba =
        mDescriptor.format == ALMetalPixelFormat::rgba8_unorm ||
        mDescriptor.format == ALMetalPixelFormat::rgba8_unorm_srgb;
    if (!bgra && !rgba)
    {
        setError(error,
                 "Metal RGB readback requires an RGBA8 or BGRA8 texture");
        return false;
    }
    if (!normalizedRegion(mDescriptor, mip_level, slice, &region, error))
    {
        return false;
    }
    const std::size_t rgb_row =
        static_cast<std::size_t>(region.width) * 3u;
    if (bytes_per_row < rgb_row)
    {
        setError(error, "Metal RGB readback row pitch is too small");
        return false;
    }
    const std::size_t rgba_row =
        static_cast<std::size_t>(region.width) * 4u;
    if (region.height != 0 &&
        rgba_row > std::numeric_limits<std::size_t>::max() / region.height)
    {
        setError(error, "Metal RGB readback staging size overflowed");
        return false;
    }

    std::vector<std::uint8_t> storage(
        rgba_row * static_cast<std::size_t>(region.height));
    if (!readback(device, mip_level, slice, region, storage.data(),
                  rgba_row, error))
    {
        return false;
    }

    auto* destination = static_cast<std::uint8_t*>(bytes);
    for (std::uint32_t row = 0; row < region.height; ++row)
    {
        const std::uint8_t* source = storage.data() + row * rgba_row;
        std::uint8_t* output = destination + row * bytes_per_row;
        for (std::uint32_t column = 0; column < region.width; ++column)
        {
            const std::size_t source_pixel =
                static_cast<std::size_t>(column) * 4u;
            const std::size_t output_pixel =
                static_cast<std::size_t>(column) * 3u;
            output[output_pixel + 0] =
                source[source_pixel + (bgra ? 2u : 0u)];
            output[output_pixel + 1] = source[source_pixel + 1u];
            output[output_pixel + 2] =
                source[source_pixel + (bgra ? 0u : 2u)];
        }
    }
    return true;
}

struct ALMetalTextureReadbackRing::Impl
{
    struct Slot
    {
        std::unique_ptr<ALMetalBuffer> staging;
        std::unique_ptr<ALMetalCommandBuffer> command;
        std::size_t tight_row = 0;
        std::size_t aligned_row = 0;
        std::uint32_t rows = 0;
    };

    const ALMetalDevice* device = nullptr;
    std::vector<Slot> slots;
    std::string label;
    std::size_t next = 0;
    std::uint32_t pending = 0;
};

ALMetalTextureReadbackRing::ALMetalTextureReadbackRing() = default;

ALMetalTextureReadbackRing::~ALMetalTextureReadbackRing()
{
    release();
}

bool ALMetalTextureReadbackRing::allocate(const ALMetalDevice& device,
                                          std::uint32_t slots,
                                          std::string label,
                                          std::string* error)
{
    release();
    if (!device.nativeHandle())
    {
        setError(error, "Cannot allocate a Metal texture readback ring without a device");
        return false;
    }
    if (slots == 0)
    {
        setError(error, "Metal texture readback ring requires at least one slot");
        return false;
    }

    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->slots.resize(slots);
    impl->label = label.empty() ? "Alchemy texture readback ring" : std::move(label);
    mImpl = std::move(impl);
    return true;
}

void ALMetalTextureReadbackRing::release()
{
    if (!mImpl)
    {
        return;
    }

    for (auto& slot : mImpl->slots)
    {
        if (slot.command &&
            slot.command->status() != ALMetalCommandStatus::completed &&
            slot.command->status() != ALMetalCommandStatus::error)
        {
            slot.command->waitUntilCompleted();
        }
        slot.command.reset();
    }
    mImpl.reset();
}

bool ALMetalTextureReadbackRing::isAllocated() const noexcept
{
    return mImpl && mImpl->device && !mImpl->slots.empty();
}

std::uint32_t ALMetalTextureReadbackRing::pending() const noexcept
{
    return mImpl ? mImpl->pending : 0;
}

bool ALMetalTextureReadbackRing::request(const ALMetalTexture& source,
                                         std::uint32_t mip_level,
                                         std::uint32_t slice,
                                         ALMetalRegion2D region,
                                         std::string* error)
{
    if (!isAllocated())
    {
        setError(error, "Metal texture readback ring is not allocated");
        return false;
    }
    if (!source.nativeHandle())
    {
        setError(error, "Cannot queue readback from an invalid Metal texture");
        return false;
    }
    if (mImpl->pending == mImpl->slots.size())
    {
        return false;
    }

    const auto& descriptor = source.descriptor();
    if (descriptor.type == ALMetalTextureType::texture_2d_multisample ||
        descriptor.storage == ALMetalStorageMode::memoryless)
    {
        setError(error, "Multisample and memoryless Metal textures require a resolve before readback");
        return false;
    }
    if (!normalizedRegion(descriptor, mip_level, slice, &region, error))
    {
        return false;
    }

    id<MTLTexture> texture = (__bridge id<MTLTexture>)source.nativeHandle();
    if ((__bridge void*)texture.device != mImpl->device->nativeHandle())
    {
        setError(error, "Metal texture and readback ring device do not match");
        return false;
    }

    TextureTransferLayout layout;
    const bool depth_from_combined =
        descriptor.format == ALMetalPixelFormat::depth32_float_stencil8;
    if (depth_from_combined)
    {
        layout.tight_row = static_cast<std::size_t>(region.width) * sizeof(float);
        layout.rows = region.height;
    }
    else if (!textureTransferLayout(descriptor, mip_level, region, &layout, error))
    {
        return false;
    }

    const std::size_t aligned_row = alignUp(layout.tight_row, 256);
    if (aligned_row > std::numeric_limits<std::size_t>::max() / layout.rows)
    {
        setError(error, "Metal texture readback staging size overflowed");
        return false;
    }
    const std::size_t staging_size = aligned_row * layout.rows;
    auto& slot = mImpl->slots[mImpl->next];
    if (!slot.staging || slot.staging->length() < staging_size)
    {
        ALMetalBufferDescriptor staging_descriptor;
        staging_descriptor.length = staging_size;
        staging_descriptor.storage = ALMetalStorageMode::shared;
        staging_descriptor.label = mImpl->label + " slot " +
                                   std::to_string(mImpl->next);
        slot.staging = ALMetalBuffer::create(
            *mImpl->device, staging_descriptor, error);
        if (!slot.staging)
        {
            return false;
        }
    }

    auto command = mImpl->device->makeCommandBuffer(mImpl->label, error);
    if (!command)
    {
        return false;
    }

    @autoreleasepool
    {
        id<MTLCommandBuffer> native_command =
            (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        id<MTLBuffer> staging = (__bridge id<MTLBuffer>)slot.staging->nativeHandle();
        id<MTLBlitCommandEncoder> encoder = [native_command blitCommandEncoder];
        if (!encoder)
        {
            setError(error, "Metal could not create a texture readback ring blit encoder");
            return false;
        }
        [encoder copyFromTexture:texture
                     sourceSlice:slice
                     sourceLevel:mip_level
                    sourceOrigin:MTLOriginMake(region.x, region.y, 0)
                      sourceSize:MTLSizeMake(region.width, region.height, 1)
                        toBuffer:staging
               destinationOffset:0
          destinationBytesPerRow:aligned_row
        destinationBytesPerImage:staging_size
                         options:depth_from_combined
                             ? MTLBlitOptionDepthFromDepthStencil
                             : MTLBlitOptionNone];
        [encoder endEncoding];
    }
    if (!command->commit(error))
    {
        return false;
    }

    slot.command = std::move(command);
    slot.tight_row = layout.tight_row;
    slot.aligned_row = aligned_row;
    slot.rows = layout.rows;
    mImpl->next = (mImpl->next + 1) % mImpl->slots.size();
    ++mImpl->pending;
    return true;
}

bool ALMetalTextureReadbackRing::collect(void* bytes,
                                         std::size_t bytes_per_row,
                                         bool wait,
                                         std::string* error)
{
    if (!isAllocated())
    {
        setError(error, "Metal texture readback ring is not allocated");
        return false;
    }
    if (mImpl->pending == 0)
    {
        return false;
    }
    if (!bytes)
    {
        setError(error, "Cannot collect a Metal texture readback into a null destination");
        return false;
    }

    const std::size_t oldest =
        (mImpl->next + mImpl->slots.size() - mImpl->pending) %
        mImpl->slots.size();
    auto& slot = mImpl->slots[oldest];
    if (bytes_per_row < slot.tight_row)
    {
        setError(error, "Metal texture readback ring row pitch is too small");
        return false;
    }

    const auto retire = [&slot, this]()
    {
        slot.command.reset();
        --mImpl->pending;
    };

    if (!slot.command)
    {
        setError(error, "Metal texture readback ring slot has no command buffer");
        retire();
        return false;
    }
    if (slot.command->status() == ALMetalCommandStatus::error)
    {
        const std::string description = slot.command->errorDescription();
        setError(error, description.empty()
                            ? "Metal texture readback ring command failed"
                            : description);
        retire();
        return false;
    }
    if (slot.command->status() != ALMetalCommandStatus::completed)
    {
        if (!wait)
        {
            return false;
        }
        if (!slot.command->waitUntilCompleted(error))
        {
            retire();
            return false;
        }
    }

    const auto* source = static_cast<const std::uint8_t*>(
        slot.staging->mappedBytes());
    if (!source)
    {
        setError(error, "Metal texture readback ring staging buffer is not mapped");
        retire();
        return false;
    }
    auto* destination = static_cast<std::uint8_t*>(bytes);
    for (std::uint32_t row = 0; row < slot.rows; ++row)
    {
        std::memcpy(destination + row * bytes_per_row,
                    source + row * slot.aligned_row,
                    slot.tight_row);
    }
    retire();
    return true;
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

std::unique_ptr<ALMetalRenderPass> ALMetalRenderPass::createForColorAttachment(
    const ALMetalDevice& device,
    void* native_texture,
    ALMetalPixelFormat format,
    const ALMetalRenderPassColorConfig& config,
    std::string* error)
{
    @autoreleasepool
    {
        id<MTLTexture> texture = (__bridge id<MTLTexture>)native_texture;
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        const MTLPixelFormat expected_format = pixelFormat(format);
        if (!texture || !native_device || expected_format == MTLPixelFormatInvalid)
        {
            setError(error, "Metal drawable pass requires a texture, device and valid color format");
            return nullptr;
        }
        if (texture.device != native_device)
        {
            setError(error, "Metal drawable texture belongs to a different device");
            return nullptr;
        }
        if (texture.textureType != MTLTextureType2D || texture.sampleCount != 1 ||
            (texture.usage & MTLTextureUsageRenderTarget) == 0)
        {
            setError(error, "Metal drawable pass requires a single-sample renderable 2D texture");
            return nullptr;
        }
        if (texture.pixelFormat != expected_format)
        {
            setError(error, "Metal drawable texture format does not match the render pass");
            return nullptr;
        }
        if (isResolveAction(config.store))
        {
            setError(error, "A single-sample Metal drawable cannot use a resolve store action");
            return nullptr;
        }

        MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        MTLRenderPassColorAttachmentDescriptor* attachment = descriptor.colorAttachments[0];
        attachment.texture = texture;
        attachment.loadAction = loadAction(config.load);
        attachment.storeAction = storeAction(config.store);
        attachment.clearColor = MTLClearColorMake(config.clear.red,
                                                   config.clear.green,
                                                   config.clear.blue,
                                                   config.clear.alpha);

        auto impl = std::make_unique<ALMetalRenderPass::Impl>();
        impl->descriptor = descriptor;
        return std::unique_ptr<ALMetalRenderPass>(new ALMetalRenderPass(std::move(impl)));
    }
}

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

std::unique_ptr<ALMetalTexture> ALMetalRenderTarget::takeColorTexture(
    std::uint32_t index) noexcept
{
    return mImpl && index < mDescriptor.color_attachment_count
        ? std::move(mImpl->colors[index]) : nullptr;
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

bool ALMetalRenderTarget::copyColorFrom(
    ALMetalCommandBuffer& command_buffer,
    std::uint32_t destination_index,
    const ALMetalTexture& source,
    ALMetalTextureCopy copy,
    std::string* error)
{
    if (!mImpl || destination_index >= mDescriptor.color_attachment_count ||
        !mImpl->colors[destination_index])
    {
        setError(error, "Metal render-target color copy destination is unavailable");
        return false;
    }
    return mImpl->colors[destination_index]->copyFrom(
        command_buffer, source, copy, error);
}

bool ALMetalRenderTarget::copyDepthStencilFrom(
    ALMetalCommandBuffer& command_buffer,
    const ALMetalTexture& source,
    ALMetalTextureCopy copy,
    std::string* error)
{
    if (!mImpl || !mImpl->depth_stencil)
    {
        setError(error, "Metal render-target depth copy destination is unavailable");
        return false;
    }
    return mImpl->depth_stencil->copyFrom(
        command_buffer, source, copy, error);
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
            const ALMetalPixelFormat attachment_format =
                mDescriptor.colors[index].format;
            if (pass_config.linear_storage_view &&
                (multisample ||
                 alMetalLinearFormat(attachment_format) == attachment_format))
            {
                setError(error,
                         "A linear-storage Metal render-pass view requires a single-sample sRGB color attachment");
                return nullptr;
            }
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
            void* color_handle = multisample
                ? mImpl->multisample_colors[index]->nativeHandle()
                : pass_config.linear_storage_view
                    ? mImpl->colors[index]->nativeLinearHandle()
                    : mImpl->colors[index]->nativeHandle();
            if (!color_handle)
            {
                setError(error,
                         "Metal could not create the requested linear-storage render-pass view");
                return nullptr;
            }
            attachment.texture = (__bridge id<MTLTexture>)color_handle;
            attachment.resolveTexture = multisample
                ? (__bridge id<MTLTexture>)mImpl->colors[index]->nativeHandle() : nil;
            attachment.loadAction = loadAction(pass_config.load);
            attachment.storeAction = storeAction(pass_config.store);
            attachment.clearColor = MTLClearColorMake(pass_config.clear.red,
                                                       pass_config.clear.green,
                                                       pass_config.clear.blue,
                                                       pass_config.clear.alpha);
        }

        if (mImpl->depth_stencil && config.bind_depth_stencil)
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
