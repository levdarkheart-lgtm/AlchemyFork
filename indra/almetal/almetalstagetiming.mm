/**
 * @file almetalstagetiming.mm
 * @brief Explicit bounded native stage-timestamp diagnostics.
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
#include "almetalstagetiming.h"
#include "almetalresource.h"
#import <Metal/Metal.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace
{
thread_local const ALMetalStageTimingScope* current_scope = nullptr;
void setError(std::string* error, const char* message) noexcept
{
    if (error) try { *error = message; } catch (...) {}
}

// This is the ONLY object registered as a command-completion owner. It must
// never acquire a command, completion token, window, record or prepared pass.
struct SampleOwner { id<MTLCounterSampleBuffer> buffer = nil; };

struct ClockPair { std::uint64_t cpu_ns = 0, gpu_ticks = 0; };
ClockPair clocks(id<MTLDevice> device)
{
    MTLTimestamp cpu = 0, gpu = 0;
    [device sampleTimestamps:&cpu gpuTimestamp:&gpu];
    return {cpu, gpu}; // Apple documents the CPU values as nanoseconds.
}

struct CommandRecord
{
    // Outside SampleOwner: no command -> owner -> command ownership cycle.
    id<MTLCommandBuffer> native = nil;
    std::shared_ptr<SampleOwner> owner;
    std::optional<ClockPair> after;
    std::size_t prepared_passes = 0;
};

struct PassRecord
{
    std::shared_ptr<CommandRecord> command;
    ALMetalStageTimingResult result;
    bool profile = false;
    bool consumed = false, closed = false, terminal = false, reported = false;
};

void unavailable(PassRecord& record, ALMetalStageTimingStatus status) noexcept
{
    record.result.status = status;
    record.result.elapsed.reset();
    record.terminal = true;
}
}

ALMetalStageTimingScope::ALMetalStageTimingScope(ALMetalStageTimingWindow* window,
    std::uint64_t frame, std::uint32_t view_kind, std::int32_t face) noexcept
    : mWindow(window), mPrevious(current_scope), mFrame(frame), mViewKind(view_kind), mFace(face)
{ current_scope = this; }
ALMetalStageTimingScope::~ALMetalStageTimingScope() { current_scope = mPrevious; }
const ALMetalStageTimingScope* ALMetalStageTimingScope::current() noexcept { return current_scope; }

struct ALMetalStageTimingWindow::PreparedPass::Impl
{
    MTLRenderPassDescriptor* descriptor = nil;
    MTLComputePassDescriptor* compute_descriptor = nil;
    std::shared_ptr<PassRecord> record;
};

struct ALMetalStageTimingWindow::Impl
{
    id<MTLDevice> device = nil;
    id<MTLCounterSet> set = nil;
    ClockPair before;
    std::size_t limit = 0, rejected = 0;
    bool sealed = false, profile = false;
    std::vector<std::shared_ptr<CommandRecord>> commands;
    std::vector<std::shared_ptr<PassRecord>> records;
};

ALMetalStageTimingWindow::PreparedPass::PreparedPass(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
ALMetalStageTimingWindow::PreparedPass::~PreparedPass() = default;

void* ALMetalStageTimingWindow::PreparedPass::consume(
    ALMetalCommandBuffer& command, std::string* error) noexcept
{
    @try
    {
        const auto& record = *mImpl->record;
        if (record.closed || record.consumed || !record.command->native ||
            command.isCommitted() || record.command->native.status >= MTLCommandBufferStatusCommitted ||
            command.nativeHandle() != (__bridge void*)record.command->native)
        {
            setError(error, "Stage-counter pass requires its original active command and one consumption");
            return nullptr;
        }
        mImpl->record->consumed = true;
        return mImpl->compute_descriptor ? (__bridge void*)mImpl->compute_descriptor
                                        : (__bridge void*)mImpl->descriptor;
    }
    @catch (NSException*) { setError(error, "Native stage-counter pass consumption failed"); return nullptr; }
}

std::size_t ALMetalStageTimingWindow::PreparedPass::firstSampleIndex() const noexcept
{
    return mImpl->record->result.first_sample;
}

void ALMetalStageTimingWindow::PreparedPass::recordPipeline(
    const std::string& vertex, const std::string& fragment, const std::string& compute,
    std::uintptr_t native_pipeline) noexcept
{
    auto& record = *mImpl->record;
    if (!record.profile || record.closed) return;
    try
    {
        ALMetalStageTimingPipeline identity{vertex, fragment, compute, native_pipeline};
        auto& result = record.result;
        if (std::find(result.pipelines.begin(), result.pipelines.end(), identity) != result.pipelines.end()) return;
        if (result.pipelines.size() == 32 || vertex.size() > 256 || fragment.size() > 256 || compute.size() > 256)
        { ++result.omitted_pipelines; return; }
        result.pipelines.push_back(std::move(identity));
    }
    catch (...) { ++record.result.omitted_pipelines; }
}

void ALMetalStageTimingWindow::PreparedPass::abandon() noexcept
{
    auto& record = *mImpl->record;
    if (!record.terminal)
    {
        record.consumed = false;
        record.closed = true;
    }
}

ALMetalStageTimingWindow::ALMetalStageTimingWindow(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
ALMetalStageTimingWindow::~ALMetalStageTimingWindow()
{
    for (const auto& record : mImpl->records) record->closed = true;
}

std::unique_ptr<ALMetalStageTimingWindow> ALMetalStageTimingWindow::create(
    const ALMetalDevice& device, std::size_t max_passes, std::string* error, bool frame_profile) noexcept
{
    @autoreleasepool
    {
        try
        {
            @try
            {
                if (!device.nativeHandle() || max_passes == 0 || max_passes > maximum_passes)
                {
                    setError(error, "Stage-counter window needs a device and1...256 physical passes");
                    return nullptr;
                }
                if (frame_profile && [MTLCaptureManager sharedCaptureManager].isCapturing)
                {
                    setError(error, "Manual frame counters are unavailable during an existing Metal capture");
                    return nullptr;
                }
                auto impl = std::make_unique<Impl>();
                impl->profile = frame_profile;
                impl->device = (__bridge id<MTLDevice>)device.nativeHandle();
                if (![impl->device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])
                {
                    setError(error, "Stage-boundary counters are unavailable"); return nullptr;
                }
                for (id<MTLCounterSet> set in impl->device.counterSets)
                    if ([set.name isEqualToString:MTLCommonCounterSetTimestamp]) impl->set = set;
                bool timestamp = false;
                for (id<MTLCounter> counter in impl->set.counters)
                    timestamp |= [counter.name isEqualToString:MTLCommonCounterTimestamp];
                if (!impl->set || !timestamp)
                {
                    setError(error, "Common GPU timestamp counter is unavailable"); return nullptr;
                }
                impl->limit = max_passes;
                impl->commands.reserve(max_passes);
                impl->records.reserve(max_passes);
                impl->before = clocks(impl->device);
                if (!impl->before.cpu_ns || !impl->before.gpu_ticks)
                {
                    setError(error, "Initial stage-counter clock calibration is unavailable"); return nullptr;
                }
                return std::unique_ptr<ALMetalStageTimingWindow>(
                    new ALMetalStageTimingWindow(std::move(impl)));
            }
            @catch (NSException*) { setError(error, "Native stage-counter window setup failed"); }
        }
        catch (...) { setError(error, "Stage-counter window setup allocation failed"); }
        return nullptr;
    }
}

ALMetalStageTimingCapabilities ALMetalStageTimingWindow::capabilities(const ALMetalDevice& device) noexcept
{
    ALMetalStageTimingCapabilities result;
    @try
    {
        id<MTLDevice> native = (__bridge id<MTLDevice>)device.nativeHandle();
        result.stage_boundary = [native supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary];
        result.draw_boundary = [native supportsCounterSampling:MTLCounterSamplingPointAtDrawBoundary];
        result.dispatch_boundary = [native supportsCounterSampling:MTLCounterSamplingPointAtDispatchBoundary];
        for (id<MTLCounterSet> set in native.counterSets)
        {
            result.statistics |= [set.name isEqualToString:MTLCommonCounterSetStatistic];
            if ([set.name isEqualToString:MTLCommonCounterSetTimestamp])
                for (id<MTLCounter> counter in set.counters)
                    result.timestamps |= [counter.name isEqualToString:MTLCommonCounterTimestamp];
        }
    }
    @catch (NSException*) {}
    return result;
}

std::unique_ptr<ALMetalStageTimingWindow::PreparedPass> ALMetalStageTimingWindow::prepare(
    ALMetalCommandBuffer& command, const ALMetalRenderPass& pass,
    const ALMetalStageTimingTag& tag, std::string* error) noexcept
{
    return prepareImpl(command, &pass, tag, false, error);
}

std::unique_ptr<ALMetalStageTimingWindow::PreparedPass> ALMetalStageTimingWindow::prepareCompute(
    ALMetalCommandBuffer& command, const ALMetalStageTimingTag& tag, std::string* error) noexcept
{
    if (!mImpl->profile) return nullptr;
    return prepareImpl(command, nullptr, tag, true, error);
}

std::unique_ptr<ALMetalStageTimingWindow::PreparedPass> ALMetalStageTimingWindow::prepareImpl(
    ALMetalCommandBuffer& command, const ALMetalRenderPass* pass,
    const ALMetalStageTimingTag& tag, bool compute, std::string* error) noexcept
{
    @autoreleasepool
    {
        try
        {
            @try
            {
                id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
                MTLRenderPassDescriptor* original = pass
                    ? (__bridge MTLRenderPassDescriptor*)pass->nativeHandle() : nil;
                if (mImpl->sealed || mImpl->records.size() == mImpl->limit || !native || (!compute && !original) ||
                    command.isCommitted() || native.status >= MTLCommandBufferStatusCommitted ||
                    native.device != mImpl->device || tag.label.empty() || tag.label.size() > 128 ||
                    (!compute && original.sampleBufferAttachments[0].sampleBuffer))
                {
                    ++mImpl->rejected;
                    setError(error, "Stage-counter pass admission is unavailable; keep the original pass");
                    return nullptr;
                }
                std::shared_ptr<CommandRecord> entry;
                for (const auto& candidate : mImpl->commands)
                    if (candidate->native == native) { entry = candidate; break; }
                const bool fresh_command = !entry;
                if (fresh_command)
                {
                    entry = std::make_shared<CommandRecord>();
                    entry->native = native;
                    entry->owner = std::make_shared<SampleOwner>();
                    auto descriptor = [MTLCounterSampleBufferDescriptor new];
                    descriptor.counterSet = mImpl->set;
                    descriptor.storageMode = MTLStorageModeShared;
                    descriptor.sampleCount = mImpl->limit * 4;
                    descriptor.label = @"Alchemy bounded physical-pass stage timestamps";
                    NSError* native_error = nil;
                    entry->owner->buffer = [mImpl->device newCounterSampleBufferWithDescriptor:
                        descriptor error:&native_error];
                    if (!entry->owner->buffer)
                    {
                        ++mImpl->rejected;
                        setError(error, "Shared timestamp sample storage is unavailable; keep the original pass");
                        return nullptr;
                    }
                }
                auto record = std::make_shared<PassRecord>();
                record->command = entry;
                record->profile = mImpl->profile;
                record->result.compute = compute;
                record->result.tag = tag;
                record->result.pass_index = mImpl->records.size();
                record->result.first_sample = entry->prepared_passes * 4;
                auto prepared = std::make_unique<PreparedPass::Impl>();
                prepared->record = record;
                if (compute) prepared->compute_descriptor = [MTLComputePassDescriptor computePassDescriptor];
                else prepared->descriptor = [original copy];
                if (!prepared->descriptor && !prepared->compute_descriptor)
                {
                    ++mImpl->rejected; setError(error, "Cannot copy stage-counter pass descriptor"); return nullptr;
                }
                if (compute)
                {
                    auto attachment = prepared->compute_descriptor.sampleBufferAttachments[0];
                    attachment.sampleBuffer = entry->owner->buffer;
                    attachment.startOfEncoderSampleIndex = record->result.first_sample;
                    attachment.endOfEncoderSampleIndex = record->result.first_sample + 1;
                }
                else
                {
                    auto attachment = prepared->descriptor.sampleBufferAttachments[0];
                    attachment.sampleBuffer = entry->owner->buffer;
                    attachment.startOfVertexSampleIndex = record->result.first_sample;
                    attachment.endOfVertexSampleIndex = record->result.first_sample + 1;
                    attachment.startOfFragmentSampleIndex = record->result.first_sample + 2;
                    attachment.endOfFragmentSampleIndex = record->result.first_sample + 3;
                }
                auto result = std::unique_ptr<PreparedPass>(new PreparedPass(std::move(prepared)));
                // All allocating work finishes before registering the buffer-only
                // owner and appending into the pre-reserved bounded vectors.
                if (fresh_command && !command.retainUntilCompletion(entry->owner, error))
                { ++mImpl->rejected; return nullptr; }
                if (fresh_command) mImpl->commands.push_back(entry);
                mImpl->records.push_back(std::move(record));
                ++entry->prepared_passes;
                return result;
            }
            @catch (NSException*) { setError(error, "Native stage-counter pass preparation failed"); }
        }
        catch (...) { setError(error, "Stage-counter pass preparation allocation failed"); }
        ++mImpl->rejected;
        return nullptr;
    }
}

bool ALMetalStageTimingWindow::seal(std::string* error) noexcept
{
    if (mImpl->sealed) return true;
    mImpl->sealed = true;
    @try
    {
        for (const auto& entry : mImpl->commands)
        {
            const auto status = entry->native.status;
            for (const auto& record : mImpl->records)
            {
                if (record->command != entry) continue;
                record->closed = true;
                if (status < MTLCommandBufferStatusCommitted)
                    unavailable(*record, ALMetalStageTimingStatus::unsubmitted_at_seal);
                else if (!record->consumed && status == MTLCommandBufferStatusCompleted)
                    unavailable(*record, ALMetalStageTimingStatus::not_encoded);
            }
            // This command can be committed later by its caller. Its own
            // registered SampleOwner still protects its independent storage.
            if (status < MTLCommandBufferStatusCommitted)
            { entry->native = nil; entry->owner.reset(); }
        }
        return true;
    }
    @catch (NSException*)
    {
        for (const auto& record : mImpl->records)
        { record->closed = true; unavailable(*record, ALMetalStageTimingStatus::counter_unavailable); }
        setError(error, "Native stage-counter sealing failed");
        return false;
    }
}

bool ALMetalStageTimingWindow::poll(std::vector<ALMetalStageTimingResult>& ready,
                                  std::string* error) noexcept
{
    if (!mImpl->sealed) { setError(error, "Seal the stage-counter window before polling"); return false; }
    @autoreleasepool
    {
        try
        {
            @try
            {
                for (const auto& entry : mImpl->commands)
                {
                    if (!entry->native) continue;
                    const auto status = entry->native.status;
                    if (status != MTLCommandBufferStatusCompleted && status != MTLCommandBufferStatusError)
                        continue;
                    if (status == MTLCommandBufferStatusCompleted && !entry->after)
                        entry->after = clocks(mImpl->device);
                    for (const auto& record : mImpl->records)
                    {
                        if (record->terminal || record->command != entry) continue;
                        if (status == MTLCommandBufferStatusError)
                        {
                            record->result.gpu_error_code = entry->native.error.code;
                            unavailable(*record, ALMetalStageTimingStatus::gpu_error); continue;
                        }
                        if (!record->consumed)
                        { unavailable(*record, ALMetalStageTimingStatus::not_encoded); continue; }
                        const auto after = *entry->after;
                        auto& result = record->result;
                        result.calibration = ALMetalStageTimingCalibration{
                            mImpl->before.cpu_ns, after.cpu_ns,
                            mImpl->before.gpu_ticks, after.gpu_ticks};
                        if (after.cpu_ns <= mImpl->before.cpu_ns || after.gpu_ticks <= mImpl->before.gpu_ticks)
                        { unavailable(*record, ALMetalStageTimingStatus::calibration_unavailable); continue; }
                        const std::size_t sample_count = result.compute ? 2 : 4;
                        NSData* data = [entry->owner->buffer resolveCounterRange:
                            NSMakeRange(result.first_sample, sample_count)];
                        if (!data || data.length != sample_count * sizeof(MTLCounterResultTimestamp))
                        { unavailable(*record, ALMetalStageTimingStatus::counter_unavailable); continue; }
                        std::array<MTLCounterResultTimestamp, 4> samples{};
                        std::memcpy(samples.data(), data.bytes, data.length);
                        std::array<std::uint64_t, 4> raw{};
                        bool valid = true;
                        for (std::size_t i = 0; i < sample_count; ++i)
                        {
                            raw[i] = samples[i].timestamp;
                            valid &= raw[i] != 0 && raw[i] != MTLCounterErrorValue &&
                                raw[i] >= mImpl->before.gpu_ticks && raw[i] <= after.gpu_ticks;
                        }
                        result.raw_samples = raw;
                        if (!valid || raw[1] <= raw[0] || (!result.compute && raw[3] <= raw[2]))
                        { unavailable(*record, ALMetalStageTimingStatus::counter_unavailable); continue; }
                        const double scale = double(after.cpu_ns - mImpl->before.cpu_ns) /
                            double(after.gpu_ticks - mImpl->before.gpu_ticks);
                        const double vertex = double(raw[1] - raw[0]) * scale;
                        const double fragment = result.compute ? 0 : double(raw[3] - raw[2]) * scale;
                        if (!std::isfinite(vertex) || !std::isfinite(fragment) || vertex <= 0 || (!result.compute && fragment <= 0))
                        { unavailable(*record, ALMetalStageTimingStatus::calibration_unavailable); continue; }
                        result.status = ALMetalStageTimingStatus::complete;
                        result.elapsed = result.compute ? ALMetalStageTimingElapsed{0, 0, vertex}
                                                       : ALMetalStageTimingElapsed{vertex, fragment, 0};
                        record->terminal = true;
                    }
                    // Every record for a finished command is now terminal.
                    entry->native = nil;
                    entry->owner.reset();
                }
                std::vector<ALMetalStageTimingResult> output;
                output.reserve(mImpl->records.size());
                for (const auto& record : mImpl->records)
                    if (record->terminal && !record->reported) output.push_back(record->result);
                ready = std::move(output);
                for (const auto& record : mImpl->records)
                    if (record->terminal) record->reported = true;
                return true;
            }
            @catch (NSException*) { setError(error, "Native stage-counter resolution failed"); }
        }
        catch (...) { setError(error, "Stage-counter result allocation failed"); }
        // Keep existing reports unconsumed so a caller may retry; no waits or
        // rendering failure is introduced by diagnostic allocation pressure.
        return false;
    }
}

bool ALMetalStageTimingWindow::isSealed() const noexcept { return mImpl->sealed; }
bool ALMetalStageTimingWindow::isFinished() const noexcept
{
    return mImpl->sealed && std::all_of(mImpl->records.begin(), mImpl->records.end(),
        [](const auto& record) { return record->reported; });
}
std::size_t ALMetalStageTimingWindow::preparedPassCount() const noexcept { return mImpl->records.size(); }
std::size_t ALMetalStageTimingWindow::pendingPassCount() const noexcept
{
    return std::count_if(mImpl->records.begin(), mImpl->records.end(),
        [](const auto& record) { return !record->terminal; });
}
std::size_t ALMetalStageTimingWindow::rejectedPrepareCount() const noexcept { return mImpl->rejected; }
