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

#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <utility>

#if !defined(__arm64__)
#error "The native Metal backend must be compiled for arm64"
#endif

namespace
{
    std::atomic<std::uint64_t> committed_command_buffers{0};
    std::atomic<bool> command_timing_enabled{false};

    struct CommandTimingState
    {
        std::mutex mutex;
        std::uint64_t submitted = 0;
        std::uint64_t completed = 0;
        std::uint64_t timed = 0;
        std::uint64_t gpu_time_nanoseconds = 0;
    };

    CommandTimingState& commandTimingState()
    {
        // Completion handlers can outlive their C++ wrapper during orderly
        // process teardown. Keep this tiny process-lifetime sink valid until
        // the OS has retired the command queue rather than racing static
        // destruction.
        static CommandTimingState* state = new CommandTimingState();
        return *state;
    }

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

    if (command_timing_enabled.load(std::memory_order_relaxed))
    {
        CommandTimingState& timing = commandTimingState();
        {
            std::lock_guard<std::mutex> lock(timing.mutex);
            ++timing.submitted;
        }
        CommandTimingState* timing_state = &timing;
        [mImpl->command_buffer
            addCompletedHandler:^(id<MTLCommandBuffer> completed_command)
            {
                std::lock_guard<std::mutex> lock(timing_state->mutex);
                ++timing_state->completed;
                if (completed_command.status !=
                    MTLCommandBufferStatusCompleted)
                {
                    return;
                }
                const double start = completed_command.GPUStartTime;
                const double end = completed_command.GPUEndTime;
                const double duration_nanoseconds =
                    (end - start) * 1'000'000'000.0;
                if (!(start > 0.0) || end < start ||
                    !std::isfinite(duration_nanoseconds) ||
                    duration_nanoseconds < 0.0 ||
                    duration_nanoseconds >
                        static_cast<double>(
                            std::numeric_limits<std::uint64_t>::max()))
                {
                    return;
                }
                const auto duration = static_cast<std::uint64_t>(
                    duration_nanoseconds + 0.5);
                timing_state->gpu_time_nanoseconds += duration;
                ++timing_state->timed;
            }];
    }

    [mImpl->command_buffer commit];
    mImpl->committed = true;
    committed_command_buffers.fetch_add(1, std::memory_order_relaxed);
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
        // Metal exposes this limit through the GPU-family capability table,
        // not a per-device property. Every supported Apple Silicon device is
        // Apple family 7 or later, whose 1D/2D texture limit is 16,384. Keep
        // the value on the selected runtime device so resource validation
        // remains independent of OpenGL's unrelated limit.
        info.max_texture_2d_dimension =
            [device supportsFamily:MTLGPUFamilyApple7] ? 16'384u : 8'192u;
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

std::uint64_t ALMetalDevice::currentAllocatedSize() const noexcept
{
    return mImpl && mImpl->device
        ? static_cast<std::uint64_t>(mImpl->device.currentAllocatedSize) : 0;
}

void* ALMetalDevice::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->device : nullptr;
}

void* ALMetalDevice::nativeCommandQueue() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->command_queue : nullptr;
}

std::uint64_t ALMetalDevice::committedCommandBufferCount() noexcept
{
    return committed_command_buffers.load(std::memory_order_relaxed);
}

void ALMetalDevice::setCommandTimingEnabled(bool enabled) noexcept
{
    if (enabled)
    {
        (void)commandTimingState();
    }
    command_timing_enabled.store(enabled, std::memory_order_relaxed);
}

bool ALMetalDevice::commandTimingEnabled() noexcept
{
    return command_timing_enabled.load(std::memory_order_relaxed);
}

ALMetalCommandTiming ALMetalDevice::commandTiming() noexcept
{
    CommandTimingState& timing = commandTimingState();
    std::lock_guard<std::mutex> lock(timing.mutex);
    ALMetalCommandTiming result;
    result.submitted = timing.submitted;
    result.completed = timing.completed;
    result.timed = timing.timed;
    result.gpu_time_nanoseconds = timing.gpu_time_nanoseconds;
    return result;
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
