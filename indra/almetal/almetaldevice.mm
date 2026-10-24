/**
 * @file almetaldevice.mm
 * @brief Apple Silicon Metal device, queue and command-buffer ownership.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetaldevice.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <utility>

#if !defined(__arm64__)
#error "The native Metal backend must be compiled for arm64"
#endif

namespace
{
    void setError(std::string* destination, const std::string& message)
    {
        if (destination)
        {
            *destination = message;
        }
    }

    std::string fromNSString(NSString* value)
    {
        if (!value)
        {
            return {};
        }
        const char* utf8 = [value UTF8String];
        return utf8 ? std::string(utf8) : std::string();
    }

    std::string describeNSError(NSError* error)
    {
        if (!error)
        {
            return {};
        }
        NSString* description = [NSString stringWithFormat:@"%@ (%@:%ld)",
                                  error.localizedDescription,
                                  error.domain,
                                  static_cast<long>(error.code)];
        return fromNSString(description);
    }

    ALMetalCommandStatus commandStatus(MTLCommandBufferStatus status)
    {
        switch (status)
        {
            case MTLCommandBufferStatusNotEnqueued: return ALMetalCommandStatus::not_enqueued;
            case MTLCommandBufferStatusEnqueued:    return ALMetalCommandStatus::enqueued;
            case MTLCommandBufferStatusCommitted:   return ALMetalCommandStatus::committed;
            case MTLCommandBufferStatusScheduled:   return ALMetalCommandStatus::scheduled;
            case MTLCommandBufferStatusCompleted:   return ALMetalCommandStatus::completed;
            case MTLCommandBufferStatusError:       return ALMetalCommandStatus::error;
        }
        return ALMetalCommandStatus::unknown;
    }
}

struct ALMetalCommandBuffer::Impl
{
    id<MTLDevice> device = nil;
    id<MTLCommandBuffer> command_buffer = nil;
    bool committed = false;
};

struct ALMetalDevice::Impl
{
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;
};

ALMetalCommandBuffer::ALMetalCommandBuffer(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalCommandBuffer::~ALMetalCommandBuffer() = default;

void* ALMetalCommandBuffer::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->command_buffer : nullptr;
}

void* ALMetalCommandBuffer::nativeDevice() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->device : nullptr;
}

ALMetalCommandStatus ALMetalCommandBuffer::status() const noexcept
{
    return mImpl ? commandStatus(mImpl->command_buffer.status) : ALMetalCommandStatus::unknown;
}

bool ALMetalCommandBuffer::isCommitted() const noexcept
{
    return mImpl && mImpl->committed;
}

std::string ALMetalCommandBuffer::errorDescription() const
{
    return mImpl ? describeNSError(mImpl->command_buffer.error) : "No Metal command buffer";
}

bool ALMetalCommandBuffer::commit(std::string* error)
{
    if (!mImpl || !mImpl->command_buffer)
    {
        setError(error, "No Metal command buffer");
        return false;
    }
    if (mImpl->committed)
    {
        setError(error, "Metal command buffer was already committed");
        return false;
    }

    [mImpl->command_buffer commit];
    mImpl->committed = true;
    return true;
}

bool ALMetalCommandBuffer::waitUntilCompleted(std::string* error)
{
    if (!mImpl || !mImpl->command_buffer)
    {
        setError(error, "No Metal command buffer");
        return false;
    }
    if (!mImpl->committed)
    {
        setError(error, "Metal command buffer must be committed before waiting");
        return false;
    }

    [mImpl->command_buffer waitUntilCompleted];
    if (mImpl->command_buffer.status != MTLCommandBufferStatusCompleted)
    {
        std::string description = describeNSError(mImpl->command_buffer.error);
        setError(error, description.empty() ? "Metal command buffer did not complete" : description);
        return false;
    }
    return true;
}

bool ALMetalCommandBuffer::commitAndWait(std::string* error)
{
    return commit(error) && waitUntilCompleted(error);
}

ALMetalDevice::ALMetalDevice(std::unique_ptr<Impl> impl, ALMetalDeviceInfo info)
: mImpl(std::move(impl)),
  mInfo(std::move(info))
{
}

ALMetalDevice::~ALMetalDevice() = default;

std::unique_ptr<ALMetalDevice> ALMetalDevice::create(std::string* error)
{
    @autoreleasepool
    {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device)
        {
            setError(error, "MTLCreateSystemDefaultDevice returned no device");
            return nullptr;
        }

        // The fork intentionally targets built-in Apple Silicon only. A
        // discrete/eGPU device has different storage and synchronization
        // semantics and must never be selected as a quiet fallback.
        if (!device.hasUnifiedMemory || device.removable || device.location != MTLDeviceLocationBuiltIn)
        {
            setError(error, "The selected Metal device is not a built-in unified-memory Apple GPU");
            return nullptr;
        }

        id<MTLCommandQueue> queue = [device newCommandQueueWithMaxCommandBufferCount:64];
        if (!queue)
        {
            setError(error, "Metal could not create the viewer command queue");
            return nullptr;
        }
        queue.label = @"Alchemy viewer command queue";

        ALMetalDeviceInfo info;
        info.name = fromNSString(device.name);
        info.registry_id = device.registryID;
        info.recommended_working_set_size = device.recommendedMaxWorkingSetSize;
        info.max_buffer_length = device.maxBufferLength;
        info.current_allocated_size = device.currentAllocatedSize;
        info.unified_memory = device.hasUnifiedMemory;
        info.low_power = device.lowPower;
        info.removable = device.removable;
        info.headless = device.headless;
        info.max_threads_per_threadgroup_width = static_cast<std::uint32_t>(device.maxThreadsPerThreadgroup.width);
        info.max_threads_per_threadgroup_height = static_cast<std::uint32_t>(device.maxThreadsPerThreadgroup.height);
        info.max_threads_per_threadgroup_depth = static_cast<std::uint32_t>(device.maxThreadsPerThreadgroup.depth);

        for (std::uint32_t family = 1; family <= 9; ++family)
        {
            if ([device supportsFamily:static_cast<MTLGPUFamily>(1000 + family)])
            {
                info.highest_apple_family = family;
            }
        }
        for (std::uint32_t family = 1; family <= 2; ++family)
        {
            if ([device supportsFamily:static_cast<MTLGPUFamily>(2000 + family)])
            {
                info.highest_mac_family = family;
            }
        }

        auto impl = std::make_unique<Impl>();
        impl->device = device;
        impl->command_queue = queue;
        return std::unique_ptr<ALMetalDevice>(new ALMetalDevice(std::move(impl), std::move(info)));
    }
}

const ALMetalDeviceInfo& ALMetalDevice::info() const noexcept
{
    return mInfo;
}

void* ALMetalDevice::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->device : nullptr;
}

void* ALMetalDevice::nativeCommandQueue() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->command_queue : nullptr;
}

std::unique_ptr<ALMetalCommandBuffer> ALMetalDevice::makeCommandBuffer(
    const std::string& label,
    std::string* error) const
{
    @autoreleasepool
    {
        if (!mImpl || !mImpl->command_queue)
        {
            setError(error, "Metal command queue is unavailable");
            return nullptr;
        }
        id<MTLCommandBuffer> command = [mImpl->command_queue commandBuffer];
        if (!command)
        {
            setError(error, "Metal command queue returned no command buffer");
            return nullptr;
        }
        if (!label.empty())
        {
            command.label = [NSString stringWithUTF8String:label.c_str()];
        }

        auto impl = std::make_unique<ALMetalCommandBuffer::Impl>();
        impl->device = mImpl->device;
        impl->command_buffer = command;
        return std::unique_ptr<ALMetalCommandBuffer>(new ALMetalCommandBuffer(std::move(impl)));
    }
}

bool ALMetalDevice::waitUntilIdle(std::string* error) const
{
    auto command = makeCommandBuffer("Alchemy idle barrier", error);
    return command && command->commitAndWait(error);
}
