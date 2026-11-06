/**
 * @file algpubuffer.cpp
 * @brief Native Metal buffer storage and rings for GPU/CPU data exchange.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "algpubuffer.h"

#include "alframefences.h"
#include "almetaldevice.h"
#include "almetalresource.h"
#include "llrender.h"

#include <cstring>
#include <string>
#include <utility>

U64 ALGPUBuffer::sBytesAllocated = 0;

namespace
{
    ALMetalDevice* sMetalDevice = nullptr;
    constexpr size_t METAL_BUFFER_OFFSET_ALIGNMENT = 256;

    // Whether [offset, offset + bytes) lies in a store of `size` bytes, without the sum wrapping.
    bool in_range(size_t offset, size_t bytes, size_t size)
    {
        return offset <= size && bytes <= size - offset;
    }

    size_t round_up(size_t value, size_t multiple)
    {
        return (value + multiple - 1) / multiple * multiple;
    }

    void warn_failure(const char* operation, const char* label,
                      const std::string& error)
    {
        LL_WARNS("GPUBuffer") << operation << " failed for "
                              << (label ? label : "a native GPU buffer")
                              << (error.empty() ? "" : ": ") << error
                              << LL_ENDL;
    }
}

ALGPUBuffer::ALGPUBuffer() = default;

ALGPUBuffer::~ALGPUBuffer()
{
    release();
}

ALGPUBuffer::ALGPUBuffer(ALGPUBuffer&& other) noexcept
    : mBuffer(std::move(other.mBuffer))
    , mDevice(other.mDevice)
    , mSize(other.mSize)
    , mFlags(other.mFlags)
    , mMapped(other.mMapped)
{
    other.mDevice = nullptr;
    other.mSize   = 0;
    other.mFlags  = 0;
    other.mMapped = nullptr;
}

ALGPUBuffer& ALGPUBuffer::operator=(ALGPUBuffer&& other) noexcept
{
    if (this != &other)
    {
        release();
        mBuffer = std::move(other.mBuffer);
        mDevice = other.mDevice;
        mSize   = other.mSize;
        mFlags  = other.mFlags;
        mMapped = other.mMapped;
        other.mDevice = nullptr;
        other.mSize   = 0;
        other.mFlags  = 0;
        other.mMapped = nullptr;
    }
    return *this;
}

void ALGPUBuffer::setMetalDevice(ALMetalDevice* device) noexcept
{
    sMetalDevice = device;
}

ALMetalDevice* ALGPUBuffer::getMetalDevice() noexcept
{
    return sMetalDevice;
}

bool ALGPUBuffer::allocate(size_t bytes, U32 flags, const void* data, const char* label)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    release();

    if (bytes == 0)
    {
        LL_WARNS("GPUBuffer") << "Refusing to allocate an empty buffer" << (label ? ": " : "") << (label ? label : "")
                              << LL_ENDL;
        llassert(false);
        return false;
    }

    ALMetalDevice* device = sMetalDevice;
    if (!device)
    {
        LL_WARNS("GPUBuffer") << "Cannot allocate "
                              << (label ? label : "a native GPU buffer")
                              << " without the Metal device" << LL_ENDL;
        return false;
    }

    // A flushed write map needs a write map, and a map that reads what the GPU wrote stays coherent.
    llassert(!(flags & FLUSH_EXPLICIT) || ((flags & STREAM) && !(flags & READBACK)));
    if (!(flags & STREAM) || (flags & READBACK))
    {
        flags &= ~FLUSH_EXPLICIT;
    }

    const bool cpu_visible = (flags & (READBACK | STREAM)) != 0;
    ALMetalBufferDescriptor descriptor;
    descriptor.length = bytes;
    descriptor.storage = cpu_visible
        ? ALMetalStorageMode::shared
        : ALMetalStorageMode::private_gpu;
    descriptor.cpu_cache = ((flags & STREAM) && !(flags & READBACK))
        ? ALMetalCpuCacheMode::write_combined
        : ALMetalCpuCacheMode::default_cache;
    descriptor.label = label ? label : "Alchemy GPU buffer";

    std::string error;
    auto buffer = ALMetalBuffer::create(*device, descriptor, &error);
    if (!buffer)
    {
        warn_failure("Allocation", label, error);
        return false;
    }

    if (data)
    {
        if (cpu_visible)
        {
            if (!buffer->write(0, data, bytes, &error))
            {
                warn_failure("Initial upload", label, error);
                return false;
            }
        }
        else
        {
            auto command = device->makeCommandBuffer(
                "Alchemy GPU buffer initial upload", &error);
            if (!command || !buffer->upload(*command, 0, data, bytes, &error) ||
                !command->commit(&error))
            {
                warn_failure("Initial upload", label, error);
                return false;
            }
        }
    }

    mMapped = static_cast<U8*>(buffer->mappedBytes());
    mBuffer = std::move(buffer);
    mDevice = device;
    mSize   = bytes;
    mFlags  = flags;
    sBytesAllocated += bytes;
    LL_PROFILE_PLOT("GPU buffer bytes", (int64_t)sBytesAllocated);
    return true;
}

void ALGPUBuffer::release()
{
    if (mBuffer)
    {
        sBytesAllocated -= mSize;
        LL_PROFILE_PLOT("GPU buffer bytes", (int64_t)sBytesAllocated);
        mBuffer.reset();
    }
    mDevice = nullptr;
    mSize   = 0;
    mFlags  = 0;
    mMapped = nullptr;
}

void ALGPUBuffer::upload(size_t offset, const void* data, size_t bytes)
{
    llassert(mFlags & (DYNAMIC | STREAM));
    if (!mBuffer || !mDevice || mDevice != sMetalDevice || !data ||
        !in_range(offset, bytes, mSize))
    {
        llassert(false);
        return;
    }
    if (bytes == 0)
    {
        return;
    }

    std::string error;
    if (mMapped)
    {
        std::memcpy(mMapped + offset, data, bytes);
        flushMapped(offset, bytes);
    }
    else
    {
        auto command = mDevice->makeCommandBuffer(
            "Alchemy GPU buffer upload", &error);
        if (!command || !mBuffer->upload(*command, offset, data, bytes, &error) ||
            !command->commit(&error))
        {
            warn_failure("Upload", nullptr, error);
            llassert(false);
        }
    }
}

void ALGPUBuffer::flushMapped(size_t offset, size_t bytes)
{
    if (!mMapped || !(mFlags & FLUSH_EXPLICIT) || bytes == 0)
    {
        return;
    }
    if (!in_range(offset, bytes, mSize))
    {
        llassert(false);
        return;
    }

    // Shared storage is coherent on supported Apple Silicon. Submission order
    // publishes CPU writes; no API cache-flush operation is required.
}

void ALGPUBuffer::clear(U32 value)
{
    clearRange(0, mSize, value);
}

void ALGPUBuffer::clearRange(size_t offset, size_t bytes, U32 value)
{
    llassert(offset % sizeof(U32) == 0 && bytes % sizeof(U32) == 0);
    if (!mBuffer || !mDevice || mDevice != sMetalDevice ||
        !in_range(offset, bytes, mSize))
    {
        llassert(false);
        return;
    }
    if (bytes == 0)
    {
        return;
    }

    std::string error;
    auto command = mDevice->makeCommandBuffer("Alchemy GPU buffer fill", &error);
    if (!command || !mBuffer->fillWords(*command, offset, bytes, value, &error) ||
        !command->commit(&error))
    {
        warn_failure("Fill", nullptr, error);
        llassert(false);
    }
}

void ALGPUBuffer::copyFrom(const ALGPUBuffer& src, size_t src_offset, size_t dst_offset, size_t bytes)
{
    if (!mBuffer || !mDevice || mDevice != sMetalDevice || !src.mBuffer ||
        src.mDevice != mDevice ||
        !in_range(src_offset, bytes, src.mSize) ||
        !in_range(dst_offset, bytes, mSize))
    {
        llassert(false);
        return;
    }
    llassert(&src != this || src_offset + bytes <= dst_offset || dst_offset + bytes <= src_offset);
    if (bytes == 0)
    {
        return;
    }

    std::string error;
    auto command = mDevice->makeCommandBuffer("Alchemy GPU buffer copy", &error);
    if (!command ||
        !mBuffer->copyFrom(*command, *src.mBuffer, src_offset, dst_offset,
                           bytes, &error) ||
        !command->commit(&error))
    {
        warn_failure("Copy", nullptr, error);
        llassert(false);
    }
}

bool ALGPUBuffer::read(size_t offset, void* out, size_t bytes) const
{
    if (!mBuffer || !mDevice || mDevice != sMetalDevice || !out ||
        !in_range(offset, bytes, mSize))
    {
        llassert(false);
        return false;
    }
    if (bytes == 0)
    {
        return true;
    }

    std::string error;
    if (!mBuffer->readback(*mDevice, offset, out, bytes, &error))
    {
        warn_failure("Readback", nullptr, error);
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
// ALGPUStream
//-----------------------------------------------------------------------------

ALGPUStream::~ALGPUStream()
{
    release();
}

bool ALGPUStream::allocate(size_t bytes, U32 regions, const char* label)
{
    release();
    if (bytes == 0 || regions == 0)
    {
        llassert(false);
        return false;
    }

    // Metal buffer offsets used for constant/storage slices are kept at the
    // conservative 256-byte alignment shared by supported Apple GPUs.
    const size_t stride = round_up(round_up(bytes, sizeof(U32)),
                                   METAL_BUFFER_OFFSET_ALIGNMENT);
    if (!mStore.allocate(stride * regions, ALGPUBuffer::STREAM, nullptr, label))
    {
        return false;
    }
    mReadIn.assign(regions, 0);
    if (!mStore.getMapped())
    {
        mStaging.resize(bytes);
    }
    mRegionBytes  = bytes;
    mRegionStride = stride;
    return true;
}

ALFrameFences& ALGPUStream::frames() const
{
    return mFrameFences ? *mFrameFences : gGL.frameFences();
}

void ALGPUStream::release()
{
    mReadIn.clear();
    mStaging.clear();
    mStore.release();
    mRegionBytes  = 0;
    mRegionStride = 0;
    mPublished    = -1;
    mWriting      = -1;
    mNext         = 0;
    mStalls       = 0;
}

U8* ALGPUStream::map()
{
    if (!mStore.isAllocated())
    {
        llassert(false);
        return nullptr;
    }

    // A region mapped and not yet published is the one to write still.
    if (mWriting < 0)
    {
        // Every read of the region published last was issued before this call, in this frame or an earlier one.
        ALFrameFences& fences = frames();
        if (mPublished >= 0)
        {
            mReadIn[mPublished] = fences.frame();
            mPublished = -1;
        }

        const U32 region = mNext;
        // Finished already, unless the GPU is as many frames behind as there are regions, or the ring came round
        // within the frame. An ended frame is safe to wait out however long the
        // GPU takes. A still-open frame cannot be waited on because its command
        // buffer is not committed; fail closed and let the caller retry after
        // the frame boundary rather than overwrite storage the GPU will read.
        if (!fences.isComplete(mReadIn[region]))
        {
            if (mReadIn[region] >= fences.frame())
            {
                LL_WARNS_ONCE("GPUBuffer")
                    << "A stream ring exhausted its regions within one Metal frame; refusing to overwrite an in-use region"
                    << LL_ENDL;
                return nullptr;
            }
            bool stalled = false;
            fences.waitFinished(mReadIn[region], &stalled);
            if (stalled)
            {
                ++mStalls;
            }
        }
        mReadIn[region] = 0;
        mWriting = (S32)region;
        mNext = (mNext + 1) % (U32)mReadIn.size();
    }

    return mStore.getMapped() ? mStore.getMapped() + (size_t)mWriting * mRegionStride : mStaging.data();
}

size_t ALGPUStream::publish(size_t bytes)
{
    llassert(mWriting >= 0 && bytes <= mRegionBytes);
    if (mWriting < 0)
    {
        return 0;
    }
    bytes = llmin(bytes, mRegionBytes);

    const size_t offset = (size_t)mWriting * mRegionStride;
    if (!mStore.getMapped() && bytes > 0)
    {
        mStore.upload(offset, mStaging.data(), bytes);
    }
    mPublished = mWriting;
    mWriting   = -1;
    return offset;
}

void ALGPUStream::noteRead(size_t offset)
{
    if (mRegionStride == 0)
    {
        return;
    }
    const size_t region = offset / mRegionStride;
    // The region published last is noted by the next map(), in its frame or a later one; one being written is not
    // published.
    if (region >= mReadIn.size() || (S32)region == mPublished || (S32)region == mWriting)
    {
        return;
    }
    mReadIn[region] = frames().frame();
}
