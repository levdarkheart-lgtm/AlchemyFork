/**
 * @file almetalframe.mm
 * @brief Bounded Metal frames-in-flight and command completion ownership.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetalframe.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <atomic>
#include <mutex>
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

    std::string describeError(NSError* error)
    {
        if (!error)
        {
            return "Metal command buffer failed without an NSError";
        }
        NSString* value = [NSString stringWithFormat:@"%@ (%@:%ld)",
                            error.localizedDescription,
                            error.domain,
                            static_cast<long>(error.code)];
        const char* utf8 = value.UTF8String;
        return utf8 ? std::string(utf8) : std::string("Metal command buffer failed");
    }

    dispatch_time_t deadline(std::uint32_t timeout_milliseconds)
    {
        if (timeout_milliseconds == UINT32_MAX)
        {
            return DISPATCH_TIME_FOREVER;
        }
        return dispatch_time(DISPATCH_TIME_NOW,
                             static_cast<std::int64_t>(timeout_milliseconds) * NSEC_PER_MSEC);
    }

    struct FrameState
    {
        id<MTLDevice> device = nil;
        dispatch_semaphore_t available = nullptr;
        dispatch_group_t active = nullptr;
        std::uint32_t maximum_in_flight = 0;
        std::atomic<std::uint64_t> next_serial{1};
        std::atomic<std::uint64_t> begun{0};
        std::atomic<std::uint64_t> submitted{0};
        std::atomic<std::uint64_t> completed{0};
        std::atomic<std::uint64_t> failed{0};
        std::atomic<std::uint32_t> in_flight{0};
        std::mutex slot_mutex;
        std::uint32_t available_slots = 0;
        mutable std::mutex error_mutex;
        std::string last_error;
    };

    void releaseSlot(const std::shared_ptr<FrameState>& state, std::uint32_t slot)
    {
        {
            std::lock_guard<std::mutex> lock(state->slot_mutex);
            state->available_slots |= (1u << slot);
        }
        dispatch_semaphore_signal(state->available);
    }
}

struct ALMetalFrame::Impl
{
    std::shared_ptr<FrameState> state;
    std::unique_ptr<ALMetalCommandBuffer> command_buffer;
    std::uint64_t serial = 0;
    std::uint32_t slot = 0;
    bool submitted = false;
    bool owns_slot = true;
};

struct ALMetalFrameScheduler::Impl
{
    std::shared_ptr<FrameState> state;
    const ALMetalDevice* device = nullptr;
};

ALMetalFrame::ALMetalFrame(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalFrame::~ALMetalFrame()
{
    if (mImpl && mImpl->owns_slot)
    {
        // An abandoned frame never entered the dispatch group. Return its
        // reservation so a failed encoder cannot permanently stall rendering.
        releaseSlot(mImpl->state, mImpl->slot);
        mImpl->owns_slot = false;
    }
}

ALMetalCommandBuffer& ALMetalFrame::commandBuffer() noexcept
{
    return *mImpl->command_buffer;
}

const ALMetalCommandBuffer& ALMetalFrame::commandBuffer() const noexcept
{
    return *mImpl->command_buffer;
}

std::uint64_t ALMetalFrame::serial() const noexcept
{
    return mImpl ? mImpl->serial : 0;
}

std::uint32_t ALMetalFrame::slot() const noexcept
{
    return mImpl ? mImpl->slot : 0;
}

bool ALMetalFrame::wasSubmitted() const noexcept
{
    return mImpl && mImpl->submitted;
}

ALMetalFrameScheduler::ALMetalFrameScheduler(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalFrameScheduler::~ALMetalFrameScheduler()
{
    if (mImpl && mImpl->state)
    {
        dispatch_group_wait(mImpl->state->active, DISPATCH_TIME_FOREVER);
    }
}

std::unique_ptr<ALMetalFrameScheduler> ALMetalFrameScheduler::create(
    const ALMetalDevice& device,
    std::uint32_t maximum_in_flight,
    std::string* error)
{
    if (!device.nativeHandle())
    {
        setError(error, "Cannot create a Metal frame scheduler without a device");
        return nullptr;
    }
    if (maximum_in_flight < 1 || maximum_in_flight > 3)
    {
        setError(error, "Metal frame scheduler supports one to three frames in flight");
        return nullptr;
    }

    auto state = std::make_shared<FrameState>();
    state->device = (__bridge id<MTLDevice>)device.nativeHandle();
    state->available = dispatch_semaphore_create(maximum_in_flight);
    state->active = dispatch_group_create();
    state->maximum_in_flight = maximum_in_flight;
    state->available_slots = (1u << maximum_in_flight) - 1u;
    if (!state->available || !state->active)
    {
        setError(error, "Could not allocate Metal frame synchronization primitives");
        return nullptr;
    }

    auto impl = std::make_unique<Impl>();
    impl->state = std::move(state);
    impl->device = &device;
    return std::unique_ptr<ALMetalFrameScheduler>(new ALMetalFrameScheduler(std::move(impl)));
}

std::unique_ptr<ALMetalFrame> ALMetalFrameScheduler::beginFrame(
    const std::string& label,
    std::uint32_t timeout_milliseconds,
    std::string* error)
{
    if (!mImpl || !mImpl->state || !mImpl->device)
    {
        setError(error, "Metal frame scheduler is unavailable");
        return nullptr;
    }
    if (dispatch_semaphore_wait(mImpl->state->available, deadline(timeout_milliseconds)) != 0)
    {
        setError(error, "Timed out waiting for a Metal frame slot");
        return nullptr;
    }

    const std::uint64_t serial = mImpl->state->next_serial.fetch_add(1);
    std::string command_label = label.empty()
        ? "Alchemy frame " + std::to_string(serial)
        : label;
    auto command = mImpl->device->makeCommandBuffer(command_label, error);
    if (!command)
    {
        dispatch_semaphore_signal(mImpl->state->available);
        return nullptr;
    }

    auto frame_impl = std::make_unique<ALMetalFrame::Impl>();
    frame_impl->state = mImpl->state;
    frame_impl->command_buffer = std::move(command);
    frame_impl->serial = serial;
    {
        std::lock_guard<std::mutex> lock(mImpl->state->slot_mutex);
        for (std::uint32_t slot = 0; slot < mImpl->state->maximum_in_flight; ++slot)
        {
            if ((mImpl->state->available_slots & (1u << slot)) != 0)
            {
                frame_impl->slot = slot;
                mImpl->state->available_slots &= ~(1u << slot);
                break;
            }
        }
    }
    mImpl->state->begun.fetch_add(1);
    return std::unique_ptr<ALMetalFrame>(new ALMetalFrame(std::move(frame_impl)));
}

bool ALMetalFrameScheduler::submit(ALMetalFrame& frame, std::string* error)
{
    if (!mImpl || !frame.mImpl || frame.mImpl->state != mImpl->state)
    {
        setError(error, "Metal frame belongs to a different scheduler");
        return false;
    }
    if (frame.mImpl->submitted || !frame.mImpl->owns_slot ||
        frame.mImpl->command_buffer->isCommitted())
    {
        setError(error, "Metal frame was already submitted or committed outside its scheduler");
        return false;
    }

    std::shared_ptr<FrameState> state = mImpl->state;
    const std::uint32_t slot = frame.mImpl->slot;
    id<MTLCommandBuffer> native_command =
        (__bridge id<MTLCommandBuffer>)frame.mImpl->command_buffer->nativeHandle();
    if (!native_command)
    {
        setError(error, "Metal frame has no command buffer");
        return false;
    }

    dispatch_group_enter(state->active);
    state->in_flight.fetch_add(1);
    [native_command addCompletedHandler:^(id<MTLCommandBuffer> completed_command) {
        if (completed_command.status == MTLCommandBufferStatusCompleted)
        {
            state->completed.fetch_add(1);
        }
        else
        {
            state->failed.fetch_add(1);
            std::lock_guard<std::mutex> lock(state->error_mutex);
            state->last_error = describeError(completed_command.error);
        }
        state->in_flight.fetch_sub(1);
        releaseSlot(state, slot);
        dispatch_group_leave(state->active);
    }];

    if (!frame.mImpl->command_buffer->commit(error))
    {
        state->in_flight.fetch_sub(1);
        dispatch_group_leave(state->active);
        releaseSlot(state, slot);
        frame.mImpl->owns_slot = false;
        return false;
    }

    frame.mImpl->submitted = true;
    frame.mImpl->owns_slot = false;
    state->submitted.fetch_add(1);
    return true;
}

bool ALMetalFrameScheduler::waitUntilIdle(std::uint32_t timeout_milliseconds,
                                          std::string* error) const
{
    if (!mImpl || !mImpl->state)
    {
        setError(error, "Metal frame scheduler is unavailable");
        return false;
    }
    if (dispatch_group_wait(mImpl->state->active, deadline(timeout_milliseconds)) != 0)
    {
        setError(error, "Timed out waiting for Metal frames to complete");
        return false;
    }
    if (mImpl->state->failed.load() != 0)
    {
        setError(error, lastError());
        return false;
    }
    return true;
}

ALMetalFrameStats ALMetalFrameScheduler::stats() const noexcept
{
    ALMetalFrameStats result;
    if (mImpl && mImpl->state)
    {
        result.begun = mImpl->state->begun.load();
        result.submitted = mImpl->state->submitted.load();
        result.completed = mImpl->state->completed.load();
        result.failed = mImpl->state->failed.load();
        result.in_flight = mImpl->state->in_flight.load();
        result.maximum_in_flight = mImpl->state->maximum_in_flight;
    }
    return result;
}

std::string ALMetalFrameScheduler::lastError() const
{
    if (!mImpl || !mImpl->state)
    {
        return {};
    }
    std::lock_guard<std::mutex> lock(mImpl->state->error_mutex);
    return mImpl->state->last_error;
}
