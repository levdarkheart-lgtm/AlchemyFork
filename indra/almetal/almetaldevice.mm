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
#include "almetaldiagnostictelemetry.h"
#include "almetalpipelinecache.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

#if !defined(__arm64__)
#error "The native Metal backend must be compiled for arm64"
#endif

namespace
{
    struct QueueSubmission
    {
        std::mutex mutex;
        std::uint64_t next_serial = 1;
    };
    struct NativeBackingOwner
    {
        explicit NativeBackingOwner(id<MTLResource> value) : resource(value) {}
        id<MTLResource> resource;
    };
    thread_local std::shared_ptr<const void> submission_owner;
    std::atomic<std::uint64_t> committed_command_buffers{0};
    std::atomic<std::uint64_t> encoded_draw_calls{0};
    std::atomic<bool> command_timing_enabled{false};

    std::atomic<std::uint64_t> render_info_epoch{0};
    struct RenderInfoState
    {
        std::mutex mutex;
        std::uint64_t generation = 0;
        ALMetalRenderInfo counts;
    };

    RenderInfoState& renderInfoState()
    {
        // Publication is synchronous at CPU encoder end; no completion
        // callback owns this census or needs a process-lifetime heap sink.
        static RenderInfoState state;
        return state;
    }

    void addRenderInfoCount(std::uint64_t& destination, std::uint64_t value,
                            bool& known) noexcept
    {
        if (value > std::numeric_limits<std::uint64_t>::max() - destination)
        {
            destination = std::numeric_limits<std::uint64_t>::max();
            known = false;
        }
        else destination += value;
    }

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

struct ALMetalCommandCompletion::Impl
{
    id<MTLCommandBuffer> command_buffer = nil;
    std::atomic<bool> abandoned{false};
    std::atomic<bool> committed{false};
};

ALMetalCommandCompletion::ALMetalCommandCompletion(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalCommandCompletion::~ALMetalCommandCompletion() = default;

bool ALMetalCommandCompletion::isComplete() const noexcept
{
    if (mImpl && mImpl->abandoned.load(std::memory_order_acquire))
    {
        return true;
    }
    if (!mImpl || !mImpl->command_buffer)
    {
        return false;
    }
    const MTLCommandBufferStatus status = mImpl->command_buffer.status;
    return status == MTLCommandBufferStatusCompleted ||
           status == MTLCommandBufferStatusError;
}

ALMetalCommandStatus ALMetalCommandCompletion::status() const noexcept
{
    if (!mImpl || !mImpl->command_buffer ||
        mImpl->abandoned.load(std::memory_order_acquire))
    {
        return ALMetalCommandStatus::unknown;
    }
    return commandStatus(mImpl->command_buffer.status);
}

bool ALMetalCommandCompletion::waitUntilCompleted(std::string* error) const
{
    if (!mImpl || !mImpl->command_buffer ||
        mImpl->abandoned.load(std::memory_order_acquire))
    { setError(error, "Metal completion has no submitted live command"); return false; }
    if (!mImpl->committed.load(std::memory_order_acquire))
    { setError(error, "Metal completion command must be committed before waiting"); return false; }
    [mImpl->command_buffer waitUntilCompleted];
    if (mImpl->command_buffer.status != MTLCommandBufferStatusCompleted)
    {
        const auto description=describeNSError(mImpl->command_buffer.error);
        setError(error, description.empty()?"Metal completion command did not complete":description);
        return false;
    }
    return true;
}

ALMetalCommandTimingStatus ALMetalCommandCompletion::gpuTime(
    ALMetalCommandGPUTime& output) const noexcept
{
    output = {};
    if (!mImpl || !mImpl->command_buffer ||
        mImpl->abandoned.load(std::memory_order_acquire))
    {
        return ALMetalCommandTimingStatus::failed;
    }
    const MTLCommandBufferStatus status = mImpl->command_buffer.status;
    if (status == MTLCommandBufferStatusError)
    {
        return ALMetalCommandTimingStatus::failed;
    }
    if (status != MTLCommandBufferStatusCompleted)
    {
        return ALMetalCommandTimingStatus::pending;
    }

    // Metal exposes these values only after completion. Reading the retained
    // command directly avoids a completion-handler publication race and any
    // owner -> ticket -> command -> callback ownership cycle.
    const double start = mImpl->command_buffer.GPUStartTime;
    const double end = mImpl->command_buffer.GPUEndTime;
    if (!std::isfinite(start) || !std::isfinite(end) ||
        !(start > 0.0) || !(end > start))
    {
        return ALMetalCommandTimingStatus::failed;
    }
    const double nanoseconds = std::round((end - start) * 1'000'000'000.0);
    // The strict upper bound also avoids converting rounded 2^64 to uint64_t
    // on platforms where double cannot represent UINT64_MAX exactly.
    if (!std::isfinite(nanoseconds) || !(nanoseconds > 0.0) ||
        !(nanoseconds < static_cast<double>(
            std::numeric_limits<std::uint64_t>::max())))
    {
        return ALMetalCommandTimingStatus::failed;
    }
    output.start_seconds = start;
    output.end_seconds = end;
    output.elapsed_nanoseconds = static_cast<std::uint64_t>(nanoseconds);
    return ALMetalCommandTimingStatus::ready;
}

struct ALMetalCommandBuffer::Impl
{
    id<MTLDevice> device = nil;
    id<MTLCommandBuffer> command_buffer = nil;
    bool committed = false;
    ALMetalDiagnosticTelemetry::CommandToken diagnostic;
    std::shared_ptr<const ALMetalCommandCompletion> completion;
    // Empty commands allocate no owner list or completion callback.
    std::shared_ptr<std::vector<std::shared_ptr<const void>>> resource_owners;
    std::shared_ptr<ALMetalResourcePlanning::Command> resource_plan;
    std::shared_ptr<ALMetalResourcePlanning::Plan> resource_registry;
    std::shared_ptr<QueueSubmission> submission_order;
};

struct ALMetalDevice::Impl
{
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;
    ALMetalResourcePlanMode resource_plan_mode = ALMetalResourcePlanMode::disabled;
    std::shared_ptr<ALMetalResourcePlanning::Plan> resource_plan =
        std::make_shared<ALMetalResourcePlanning::Plan>();
    std::shared_ptr<QueueSubmission> submission_order = std::make_shared<QueueSubmission>();
};

ALMetalCommandBuffer::ALMetalCommandBuffer(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalCommandBuffer::~ALMetalCommandBuffer()
{
    if (mImpl && !mImpl->committed && mImpl->resource_plan)
        mImpl->resource_plan->retire(ALMetalResourcePlanning::Outcome::abandoned);
    if (mImpl && !mImpl->committed)
        ALMetalDiagnosticTelemetry::commandAbandoned(mImpl->diagnostic);
    if (mImpl && !mImpl->committed && mImpl->completion)
    {
        mImpl->completion->mImpl->abandoned.store(
            true, std::memory_order_release);
    }
}

void* ALMetalCommandBuffer::nativeHandle() const noexcept
{
    if (mImpl && mImpl->resource_plan) mImpl->resource_plan->markOpaque();
    return nativeHandleForEncoding();
}

void* ALMetalCommandBuffer::nativeHandleForEncoding() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->command_buffer : nullptr;
}

const void* ALMetalCommandBuffer::nativeIdentity() const noexcept
{ return nativeHandleForEncoding(); }

void* ALMetalCommandBuffer::nativeHandleForDeclaredResources() const noexcept
{
    if (mImpl && mImpl->resource_plan)
    {
        const auto state = mImpl->resource_plan->stats();
        if (!state.valid || state.retired || state.submitted || !state.active_pass) return nullptr;
    }
    return nativeHandleForEncoding();
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

std::shared_ptr<const ALMetalCommandCompletion>
ALMetalCommandBuffer::completionToken() const
{
    if (!mImpl || !mImpl->command_buffer)
    {
        return {};
    }
    if (!mImpl->completion)
    {
        auto impl = std::make_unique<ALMetalCommandCompletion::Impl>();
        impl->command_buffer = mImpl->command_buffer;
        impl->committed.store(mImpl->committed, std::memory_order_relaxed);
        mImpl->completion = std::shared_ptr<const ALMetalCommandCompletion>(
            new ALMetalCommandCompletion(std::move(impl)));
    }
    return mImpl->completion;
}

std::string ALMetalCommandBuffer::errorDescription() const
{
    return mImpl ? describeNSError(mImpl->command_buffer.error) : "No Metal command buffer";
}

bool ALMetalCommandBuffer::retainUntilCompletion(
    std::shared_ptr<const void> owner, std::string* error)
{
    if (!mImpl || !mImpl->command_buffer || mImpl->committed || !owner)
    {
        setError(error, "Metal resource retention requires an owner and uncommitted command");
        return false;
    }
    if (!mImpl->resource_owners)
        mImpl->resource_owners =
            std::make_shared<std::vector<std::shared_ptr<const void>>>();
    for (const auto& existing : *mImpl->resource_owners)
        if (!existing.owner_before(owner) && !owner.owner_before(existing))
            return true;
    mImpl->resource_owners->push_back(std::move(owner));
    return true;
}

std::shared_ptr<ALMetalResourcePlanning::Command>
ALMetalCommandBuffer::resourceUsePlan() const noexcept
{ return mImpl ? mImpl->resource_plan : nullptr; }

bool ALMetalCommandBuffer::registerExternalBacking(void* handle, ALMetalResourcePlanning::View& output,
    std::string* error) const
{
    output = {};
    id<MTLResource> resource = (__bridge id<MTLResource>)handle;
    if (!mImpl || !resource || resource.device != mImpl->device)
    { setError(error, "Metal foreign resource does not match its command device"); return false; }
    if (!mImpl->resource_registry) return true;
    try
    {
        ALMetalResourcePlanning::Description description;
        description.kind = ALMetalResourcePlanning::Kind::texture;
        description.origin = ALMetalResourcePlanning::Origin::external_unknown;
        output = mImpl->resource_registry->allocate(description, std::make_shared<NativeBackingOwner>(resource), (__bridge const void*)resource);
        if (output) return true;
    }
    catch (...) {}
    setError(error, "Metal foreign resource backing could not be retained"); return false;
}

bool ALMetalCommandBuffer::beginResourcePass(ALMetalResourcePlanning::Pass kind,
    std::uint64_t& pass, std::string* error)
{
    pass = 0;
    if (!mImpl || mImpl->committed)
    { setError(error, "Metal resource pass requires an uncommitted command"); return false; }
    if (!mImpl->resource_plan) return true;
    pass = mImpl->resource_plan->beginPass(kind);
    if (!pass)
    { setError(error, "Metal resource pass overlaps an active or invalid pass"); return false; }
    return true;
}

bool ALMetalCommandBuffer::endResourcePass(std::uint64_t pass, std::string* error)
{
    if (!mImpl) return false;
    if (!mImpl->resource_plan) return pass == 0;
    if (mImpl->resource_plan->endPass(pass)) return true;
    mImpl->resource_plan->invalidate();
    setError(error, "Metal resource pass does not match its command owner"); return false;
}

bool ALMetalCommandBuffer::useResource(const ALMetalResourcePlanning::View& view,
    ALMetalResourcePlanning::Access access, std::string* error)
{
    if (!mImpl || mImpl->committed) return false;
    if (!mImpl->resource_plan) return true;
    try
    {
        if (mImpl->resource_plan->use(view, access)) return true;
    }
    catch (...) {}
    mImpl->resource_plan->invalidate();
    setError(error, "Metal resource use could not retain the exact current backing"); return false;
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
    std::unique_lock<std::mutex> submission_lock;
    if (mImpl->resource_plan)
    {
        submission_lock = std::unique_lock(mImpl->submission_order->mutex);
        if (!mImpl->submission_order->next_serial)
        { setError(error, "Metal resource submission identity exhausted"); return false; }
    }
    if (mImpl->resource_plan && !mImpl->resource_plan->prepareSubmit())
    {
        setError(error, "Metal resource use plan is invalid or still has an active pass");
        return false;
    }

    if (const auto plan = mImpl->resource_plan)
    {
        [mImpl->command_buffer addCompletedHandler:^(id<MTLCommandBuffer> completed)
        {
            plan->retire(completed.status == MTLCommandBufferStatusCompleted
                ? ALMetalResourcePlanning::Outcome::completed
                : ALMetalResourcePlanning::Outcome::error);
        }];
    }

    if (mImpl->resource_owners)
    {
        const auto owners = std::move(mImpl->resource_owners);
        [mImpl->command_buffer addCompletedHandler:^(id<MTLCommandBuffer>)
        {
            // Explicitly break owner -> completion token -> native command
            // -> callback -> owner, even if Metal retains a finished callback.
            owners->clear();
        }];
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

    const auto diagnostic = mImpl->diagnostic;
    if (ALMetalDiagnosticTelemetry::commandTokenActive(diagnostic))
    {
        [mImpl->command_buffer addCompletedHandler:^(id<MTLCommandBuffer> completed_command)
        {
            // Diagnostic observation only. Apple exposes GPUStart/EndTime in
            // system mach seconds after completion; never read them early or
            // wait for them. No native object is retained by the JSONL sink.
            const auto status = completed_command.status;
            const bool completed = status == MTLCommandBufferStatusCompleted;
            ALMetalDiagnosticTelemetry::commandCompleted(diagnostic,
                static_cast<std::uint32_t>(status),
                completed ? completed_command.GPUStartTime : 0.0,
                completed ? completed_command.GPUEndTime : 0.0,
                status == MTLCommandBufferStatusError ? completed_command.error.code : 0);
        }];
        ALMetalDiagnosticTelemetry::commandCommitBegin(diagnostic);
    }
    // This serial describes the real queue commit, never creation order.
    // Publish committed uses only AFTER Metal has accepted that commit, so
    // another owner cannot reuse backing while its producer is unqueued.
    [mImpl->command_buffer commit];
    mImpl->committed = true;
    if (mImpl->completion) mImpl->completion->mImpl->committed.store(true, std::memory_order_release);
    if (mImpl->resource_plan)
        mImpl->resource_plan->submitted(mImpl->submission_order->next_serial++);
    committed_command_buffers.fetch_add(1, std::memory_order_relaxed);
    ALMetalDiagnosticTelemetry::commandCommitEnd(diagnostic);
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

    ALMetalDiagnosticTelemetry::commandWaitBegin(mImpl->diagnostic);
    [mImpl->command_buffer waitUntilCompleted];
    ALMetalDiagnosticTelemetry::commandWaitEnd(mImpl->diagnostic,
        static_cast<std::uint32_t>(mImpl->command_buffer.status));
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
  mInfo(std::move(info)),
  mPipelineCache(new ALMetalPipelineCache(*this))
{
}

ALMetalDevice::~ALMetalDevice() = default;

ALMetalPipelineCache& ALMetalDevice::pipelineCache() const noexcept
{
    return *mPipelineCache;
}

ALMetalResourcePlanMode ALMetalDevice::resourcePlanModeFromEnvironment(ALMetalResourcePlanMode fallback) noexcept
{
    if (fallback == ALMetalResourcePlanMode::automatic) fallback = ALMetalResourcePlanMode::disabled;
    const char* value = std::getenv("ALCHEMY_METAL_RESOURCE_PLAN");
    if (!value || !*value) return fallback;
    if (std::strcmp(value, "1") == 0 || std::strcmp(value, "enabled") == 0) return ALMetalResourcePlanMode::enabled;
    if (std::strcmp(value, "accounting_only") == 0) return ALMetalResourcePlanMode::accounting_only;
    if (std::strcmp(value, "0") == 0 || std::strcmp(value, "disabled") == 0) return ALMetalResourcePlanMode::disabled;
    return fallback;
}

std::unique_ptr<ALMetalDevice> ALMetalDevice::create(std::string* error,
                                                  ALMetalResourcePlanMode resource_plan_mode)
{
    if (resource_plan_mode == ALMetalResourcePlanMode::automatic)
        resource_plan_mode = resourcePlanModeFromEnvironment();
    if (resource_plan_mode != ALMetalResourcePlanMode::disabled &&
        resource_plan_mode != ALMetalResourcePlanMode::accounting_only &&
        resource_plan_mode != ALMetalResourcePlanMode::enabled)
    { setError(error, "Metal resource planning mode is invalid"); return nullptr; }
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
        impl->resource_plan_mode = resource_plan_mode;
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

ALMetalResourcePlanMode ALMetalDevice::resourcePlanningMode() const noexcept
{ return mImpl ? mImpl->resource_plan_mode : ALMetalResourcePlanMode::disabled; }

const std::shared_ptr<ALMetalResourcePlanning::Plan>& ALMetalDevice::resourcePlan() const noexcept
{
    static const std::shared_ptr<ALMetalResourcePlanning::Plan> unavailable;
    return mImpl ? mImpl->resource_plan : unavailable;
}

ALMetalResourcePlanning::Stats ALMetalDevice::uniqueResourceStats() const noexcept
{ return mImpl ? mImpl->resource_plan->stats() : ALMetalResourcePlanning::Stats{}; }

bool ALMetalDevice::registerNativeBacking(void* handle, ALMetalResourcePlanning::Description description,
    ALMetalResourcePlanning::View& output, std::string* error) const
{
    output = {};
    id<MTLResource> resource = (__bridge id<MTLResource>)handle;
    if (!mImpl || !resource || resource.device != mImpl->device)
    { setError(error, "Metal native backing registration requires this device's exact resource"); return false; }
    if (mImpl->resource_plan_mode == ALMetalResourcePlanMode::disabled) return true;
    try
    {
        if (description.origin == ALMetalResourcePlanning::Origin::viewer_owned && !description.memoryless)
            description.metal_reported_bytes = static_cast<std::uint64_t>(resource.allocatedSize);
        output = mImpl->resource_plan->allocate(description, std::make_shared<NativeBackingOwner>(resource), (__bridge const void*)resource);
        if (output) return true;
    }
    catch (...) {}
    setError(error, "Metal native backing registration could not retain its allocation"); return false;
}

[[noreturn]] void ALMetalDevice::forceDriverCrash() const
{
    // Match the original deliberate null client-array graphics API call.
    // API validation aborts here when enabled; the driver otherwise faults
    // copying the invalid CPU source. Do not submit invalid GPU commands.
    const void* invalid_client_data = nullptr;
    (void)[mImpl->device newBufferWithBytes:invalid_client_data
                                  length:16
                                 options:MTLResourceStorageModeShared];
    // A runtime may reject the call without crashing. The explicit crash
    // test must still terminate rather than silently resume the viewer.
    std::abort();
}

std::uint64_t ALMetalDevice::committedCommandBufferCount() noexcept
{
    return committed_command_buffers.load(std::memory_order_relaxed);
}

std::uint64_t ALMetalDevice::encodedDrawCallCount() noexcept
{
    return encoded_draw_calls.load(std::memory_order_relaxed);
}

void ALMetalDevice::addEncodedDrawCalls(std::uint64_t count) noexcept
{
    if (count != 0)
    {
        encoded_draw_calls.fetch_add(count, std::memory_order_relaxed);
    }
}

void ALMetalDevice::beginRenderInfoFrame(bool enabled) noexcept
{
    auto& state = renderInfoState();
    std::lock_guard lock(state.mutex);
    if (++state.generation == 0) ++state.generation;
    state.counts = {};
    state.counts.enabled = enabled;
    render_info_epoch.store(enabled ? state.generation : 0,
                            std::memory_order_release);
}

ALMetalRenderInfo ALMetalDevice::renderInfo() noexcept
{
    auto& state = renderInfoState();
    std::lock_guard lock(state.mutex);
    return state.counts;
}

std::uint64_t ALMetalDevice::renderInfoEpoch() noexcept
{
    return render_info_epoch.load(std::memory_order_acquire);
}

void ALMetalDevice::publishRenderInfo(std::uint64_t epoch,
                                     const ALMetalRenderInfo& counts) noexcept
{
    if (epoch == 0) return;
    auto& state = renderInfoState();
    std::lock_guard lock(state.mutex);
    if (!state.counts.enabled || epoch != state.generation) return;
    auto& total = state.counts;
    total.draw_calls_known &= counts.draw_calls_known;
    total.input_elements_known &= counts.input_elements_known;
    addRenderInfoCount(total.draw_calls, counts.draw_calls, total.draw_calls_known);
    addRenderInfoCount(total.input_elements, counts.input_elements,
                       total.input_elements_known);
    addRenderInfoCount(total.indirect_submissions, counts.indirect_submissions,
                       total.draw_calls_known);
    addRenderInfoCount(total.indirect_command_slots, counts.indirect_command_slots,
                       total.draw_calls_known);
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

ALMetalSubmissionScope::ALMetalSubmissionScope(std::shared_ptr<const void> owner) noexcept
    : mPrevious(std::move(submission_owner))
{
    submission_owner = std::move(owner);
}

ALMetalSubmissionScope::~ALMetalSubmissionScope()
{
    submission_owner = std::move(mPrevious);
}

const std::shared_ptr<const void>& ALMetalSubmissionScope::current() noexcept
{
    return submission_owner;
}

std::unique_ptr<ALMetalCommandBuffer> ALMetalDevice::makeCommandBuffer(
    const std::string& label,
    std::string* error) const
{
    @autoreleasepool
    {
        const auto diagnostic = ALMetalDiagnosticTelemetry::commandCreateBegin(label.c_str());
        if (!mImpl || !mImpl->command_queue)
        {
            ALMetalDiagnosticTelemetry::commandCreateEnd(diagnostic, false);
            setError(error, "Metal command queue is unavailable");
            return nullptr;
        }
        id<MTLCommandBuffer> command = [mImpl->command_queue commandBuffer];
        if (!command)
        {
            ALMetalDiagnosticTelemetry::commandCreateEnd(diagnostic, false);
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
        impl->diagnostic = diagnostic;
        impl->submission_order = mImpl->submission_order;
        if (mImpl->resource_plan_mode != ALMetalResourcePlanMode::disabled)
            impl->resource_registry = mImpl->resource_plan;
        if (mImpl->resource_plan_mode == ALMetalResourcePlanMode::enabled)
            impl->resource_plan = mImpl->resource_plan->command();
        ALMetalDiagnosticTelemetry::commandCreateEnd(diagnostic, true);
        auto result = std::unique_ptr<ALMetalCommandBuffer>(new ALMetalCommandBuffer(std::move(impl)));
        const auto& owner = ALMetalSubmissionScope::current();
        if (owner && !result->retainUntilCompletion(owner, error)) return nullptr;
        return result;
    }
}

bool ALMetalDevice::waitUntilIdle(std::string* error) const
{
    auto command = makeCommandBuffer("Alchemy idle barrier", error);
    return command && command->commitAndWait(error);
}
